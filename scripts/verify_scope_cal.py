#!/usr/bin/env python3
"""
Verify the compiled scope vertical calibration against the bench.

WHAT THIS CHECKS, AND WHY IT IS A DIFFERENT QUESTION FROM THE ONE THAT
PRODUCED THE TABLE
------------------------------------------------------------------------
The gains in firmware/src/ui/scope_cal.c came from a per-range slope fit
(EXP-08): drive amplitude varied, range held fixed. That design never compares
one range against another, so a table can pass it on every row and still be
internally inconsistent.

This script asks the complementary question: hold the signal fixed, change the
range, and see whether the instrument reports the same voltage each time. It is
the check a user performs implicitly every time they turn the volts/div knob.

It also reports the ratio of measured to commanded amplitude. Read the caveat
printed at the end before treating that as accuracy — while the calibration
source is the same generator the table was built from, the ratio is close to
circular. It becomes a real accuracy figure the moment a trusted source is
connected, and at that point it is exactly what SCOPE_CAL_SOURCE_SCALE should
be set from.

WHY THE FLOOR IS SUBTRACTED
---------------------------
Peak-to-peak span includes the noise floor additively, so `span * mV_per_count`
over-reads by (floor * mV_per_count) — and mV_per_count varies 4x across the
ladder, so the bias is range-dependent and hits the coarse ranges hardest. The
first version of this measurement did not subtract it and reported a 7-8%
inflation that looked like a source-scale error and was not. Two estimators are
computed here: floor-subtracted, and a two-amplitude difference that cancels
the floor without measuring it. They should agree within quantisation; if they
do not, the floor is not additive and neither number should be trusted.

USAGE
-----
    python3 scripts/verify_scope_cal.py                 # ranges 5 6 7
    python3 scripts/verify_scope_cal.py --ranges 4 5 6 7 8 9
    python3 scripts/verify_scope_cal.py --amp 1000 2000

The device must be running a build with `fpga scope center ch1|ch2` (any build
since 7ea472f) and a live capture. The generator is the ESP32 on /dev/ttyUSB0.
"""
import argparse
import os
import re
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, Siggen  # noqa: E402

CAL_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "firmware", "src", "ui", "scope_cal.c")


def load_table(path=CAL_SRC):
    """Parse mV-per-count out of scope_cal.c.

    Read from the firmware source rather than duplicated here on purpose: a
    copy in this file would silently go stale the first time the table is
    re-measured, and a verification tool checking against the wrong table is
    worse than no tool.
    """
    with open(path) as f:
        src = f.read()

    m = re.search(r"mv_per_count\[2\]\[SCOPE_CAL_RANGE_COUNT\]\s*=\s*\{(.*?)\n\};",
                  src, re.S)
    if not m:
        raise SystemExit(f"could not find mv_per_count[][] in {path}")

    rows = re.findall(r"\{([^{}]*)\}", m.group(1))
    if len(rows) != 2:
        raise SystemExit(f"expected 2 channel rows in mv_per_count, got {len(rows)}")

    table = {}
    for ch, row in zip((1, 2), rows):
        vals = [float(x) for x in re.findall(r"([0-9]+\.[0-9]+)f", row)]
        if len(vals) != 10:
            raise SystemExit(f"CH{ch}: expected 10 gains, parsed {len(vals)}")
        table[ch] = {i: v for i, v in enumerate(vals)}
    return table


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ranges", type=int, nargs="+", default=[5, 6, 7])
    ap.add_argument("--amp", type=int, nargs=2, default=[1000, 2000],
                    metavar=("LO", "HI"), help="drive amplitudes in mVpp")
    ap.add_argument("--reps", type=int, default=5)
    ap.add_argument("--settle", type=float, default=1.4,
                    help="seconds; must exceed one capture buffer (~0.96 s)")
    ap.add_argument("--scope-port", default="/dev/ttyACM0")
    ap.add_argument("--siggen-port", default="/dev/ttyUSB0")
    args = ap.parse_args()

    lo_mv, hi_mv = args.amp
    if hi_mv <= lo_mv:
        raise SystemExit("--amp needs LO < HI; equal amplitudes make the "
                         "two-point estimator a division by zero")

    table = load_table()
    sc = Scope(args.scope_port)
    sg = Siggen(args.siggen_port)

    print("build:", sc.version().strip().replace("\r\n", " | "))
    print(f"table: {os.path.relpath(CAL_SRC)}")

    def span(op):
        out = []
        for _ in range(args.reps):
            v = sc.opread(op)
            out.append(float(v.max() - v.min()))
        return statistics.median(out)

    def setup(r):
        sc.scope_range(r, 1)
        sc.scope_range(r, 2)
        sc.cmd(f"fpga scope center ch1 {r}", timeout=20)
        sc.cmd(f"fpga scope center ch2 {r}", timeout=20)
        time.sleep(args.settle)

    def drive(mvpp):
        if mvpp == 0:
            sg.off(1)
            sg.off(2)
        else:
            # Two different SHAPES, not an anti-phase pair: a display that
            # inverts or rescales can make anti-phase look like one source
            # drawn twice.
            sg.tri(250, ch=1)
            sg.amp(mvpp, ch=1)
            sg.square(400, ch=2)
            sg.amp(mvpp, ch=2)
        time.sleep(args.settle)

    # ── Control first. A negative here voids everything below. ──────────
    print("\n=== CONTROL (run first) ===")
    r_ctrl = args.ranges[len(args.ranges) // 2]
    setup(r_ctrl)
    drive(0)
    f1, f2 = span(0x04), span(0x05)
    drive(hi_mv)
    d1, d2 = span(0x04), span(0x05)
    print(f"  range {r_ctrl} quiet:  op04 {f1:6.2f}   op05 {f2:6.2f}")
    print(f"  range {r_ctrl} driven: op04 {d1:6.2f}   op05 {d2:6.2f}")
    ok = d1 > 4 * max(f1, 1.0) and d2 > 4 * max(f2, 1.0)
    print(f"  control {'PASSED' if ok else 'FAILED'} "
          "(drive must lift span >=4x over the quiet floor)")
    if not ok:
        print("\n  *** VOID: span is not tracking the drive. Check the probe, "
              "the generator and the channel mask before reading anything "
              "below. ***")

    # ── Sweep ───────────────────────────────────────────────────────────
    print(f"\n=== per-range, {lo_mv} and {hi_mv} mVpp ===")
    print("rng ch |  floor     lo     hi | floor-corr | two-point | tier-gain")
    rows = {1: {}, 2: {}}
    for r in args.ranges:
        setup(r)
        drive(0)
        fl = {1: span(0x04), 2: span(0x05)}
        drive(lo_mv)
        lo = {1: span(0x04), 2: span(0x05)}
        drive(hi_mv)
        hi = {1: span(0x04), 2: span(0x05)}

        for ch in (1, 2):
            k = table[ch][r]
            if k <= 0.0:
                print(f" {r}  {ch} | {fl[ch]:6.1f} {lo[ch]:6.1f} {hi[ch]:6.1f} |"
                      f"     (no cal — range marked unusable)")
                continue
            corrected = (hi[ch] - fl[ch]) * k
            twopoint = (hi[ch] - lo[ch]) * k * (hi_mv / (hi_mv - lo_mv))
            rows[ch][r] = (corrected, twopoint)
            print(f" {r}  {ch} | {fl[ch]:6.1f} {lo[ch]:6.1f} {hi[ch]:6.1f} |"
                  f" {corrected:8.0f}mV | {twopoint:8.0f}mV | {k:6.2f} mV/ct")

    # ── The actual test ─────────────────────────────────────────────────
    print(f"\n=== agreement across ranges (commanded {hi_mv} mVpp) ===")
    for ch in (1, 2):
        if len(rows[ch]) < 2:
            print(f"  CH{ch}: fewer than two calibrated ranges — nothing to compare")
            continue
        for label, idx in (("floor-corrected", 0), ("two-point     ", 1)):
            vals = [rows[ch][r][idx] for r in sorted(rows[ch])]
            mean = statistics.mean(vals)
            spread = (max(vals) - min(vals)) / mean
            verdict = "consistent" if spread < 0.15 else "INCONSISTENT"
            print(f"  CH{ch} {label}: "
                  f"{' / '.join(f'{v:.0f}' for v in vals)} mVpp"
                  f"   spread {spread * 100:4.1f}%  {verdict}"
                  f"   mean/commanded {mean / hi_mv:.3f}")

    sg.off(1)
    sg.off(2)

    print("""
NOTE ON THE mean/commanded COLUMN
  While the drive is the ESP32 generator that these gains were derived from,
  this ratio is close to circular and is NOT an accuracy figure. Against a
  CALIBRATED source it becomes one, and 1/ratio is then what
  SCOPE_CAL_SOURCE_SCALE should be set to — one range is enough, because a
  source scale error is uniform across the whole table by construction.
  Do not adjust individual rows; tests/test_scope_cal.c will fail if you do.""")


if __name__ == "__main__":
    main()
