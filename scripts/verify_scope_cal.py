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

SOURCES (--source, see scripts/README-bench.md)
-----------------------------------------------
esp32 (default)  The historical rig, unchanged: CH1 triangle 250 Hz, CH2 square
                 400 Hz, amplitudes COMMANDED in mVpp — the circular case above.
kodedot          A Kode Dot running sigsrc, ONE pin into both probes. The pin
                 swings 0 V to the 3V3 rail, so there is exactly one amplitude:
                 the rail as measured on a DMM (--v3v3). That reference is an
                 independent measurement, so mean/reference IS an accuracy
                 figure. One amplitude means no two-point estimator; in its place
                 the difference of the MEANS of two static captures (`dc 1`
                 minus `dc 0`), which carries neither the floor bias of a span
                 nor any overshoot a square's edges add to one.
                 Centring wants a quiet input, and the Dot's quiet is its LOW
                 rail, so 0 V lands at code 128 and only the upper half of the
                 ADC is usable — which ranges that leaves is printed (and
                 enforced). --center-mid centres once on each rail and sets the
                 offset DAC to the midpoint, which should recover range 5 (not
                 yet run on hardware; rows that clip are flagged and excluded).
manual           Any generator; the operator types what a DMM reads. Vrms is
                 converted per --waveform: a sine is 2*sqrt(2)*Vrms, a 50 %
                 square is 2*Vrms, a DC reading is a square's high level.

The expected span in counts for a drive is Vpp / (mV_per_count *
SCOPE_CAL_SOURCE_SCALE) — Vpp, not Vrms*2*sqrt(2), which is right only for a
sine. Each row prints it next to what was measured.

USAGE
-----
    python3 scripts/verify_scope_cal.py                 # ranges 5 6 7
    python3 scripts/verify_scope_cal.py --ranges 4 5 6 7 8 9
    python3 scripts/verify_scope_cal.py --amp 1000 2000
    python3 scripts/verify_scope_cal.py --source kodedot --v3v3 3.292
    python3 scripts/verify_scope_cal.py --source kodedot --dry-run   # no hardware

The device must be running a build with `fpga scope center ch1|ch2` (any build
since 7ea472f) and a live capture. Ports are discovered by USB id: the scope by
2e3c:5740, a Kode Dot by Espressif's 0x303A; the ESP32 siggen as before.
"""
import argparse
import os
import re
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bench  # noqa: E402
from bench import (BenchError, WAVEFORMS, add_source_args,  # noqa: E402
                   expected_span_counts, open_bench)

CAL_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "firmware", "src", "ui", "scope_cal.c")
CAL_HDR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "firmware", "src", "ui", "scope_cal.h")

#: 8-bit capture: codes 0..255, and centring puts the quiet input at 128.
FULL_SCALE_COUNTS = 256
CENTRE_CODE = 128
#: Counts kept clear of each rail: centring lands within a few counts of 128,
#: and a span that touches a rail is a lower bound, not a measurement.
CLIP_MARGIN = 8
#: Below this many counts the +/-2-count quantisation of a span exceeds 10 %,
#: the agreement limit used below. Such a range is usable but coarse.
MIN_RESOLVED_COUNTS = 20
OPCODE = {1: 0x04, 2: 0x05}
SOURCE_CLASS = {"esp32": bench.Esp32Source, "kodedot": bench.KodeDotSource,
                "manual": bench.ManualSource}
_CENTER_RE = re.compile(
    r"CH(\d) range (\d+): center (\S+)=(\d+) \(median=(\d+)\)(.*)")


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


def load_source_scale(path=CAL_HDR):
    """SCOPE_CAL_SOURCE_SCALE from scope_cal.h — the firmware multiplies every
    table entry by it at lookup, so the TRUE gain is table x scale. Read from
    the header for the same reason the table is read from the source."""
    with open(path) as f:
        m = re.search(r"#define\s+SCOPE_CAL_SOURCE_SCALE\s+([0-9]*\.[0-9]+)f?\b", f.read())
    if not m:
        raise SystemExit(f"could not find SCOPE_CAL_SOURCE_SCALE in {path}")
    return float(m.group(1))


def range_coverage(table, scale, vpp_mv, midpoint, channels=(1, 2)):
    """Which ranges a ``vpp_mv`` drive fits, with expected span counts.

    Returns ``({range: (status, {ch: counts})}, headroom)``; status is ``ok``,
    ``coarse`` (fits, under MIN_RESOLVED_COUNTS), ``clips`` or ``nocal``.
    Full scale per range is 256 counts x the true gain (table x scale). With
    the waveform's MIDPOINT centred at 128 a drive may use the whole span less
    a margin each side; with its LOW level at 128 (a logic source centred on
    its quiet low rail) only the upper half."""
    headroom = (FULL_SCALE_COUNTS - 2 * CLIP_MARGIN if midpoint
                else FULL_SCALE_COUNTS - 1 - CENTRE_CODE - CLIP_MARGIN)
    out = {}
    for r in sorted(table[channels[0]]):
        gains = {ch: table[ch][r] * scale for ch in channels}
        if any(k <= 0 for k in gains.values()):
            out[r] = ("nocal", {})
            continue
        counts = {ch: expected_span_counts(vpp_mv, k) for ch, k in gains.items()}
        if max(counts.values()) > headroom:
            status = "clips"
        elif min(counts.values()) < MIN_RESOLVED_COUNTS:
            status = "coarse"
        else:
            status = "ok"
        out[r] = (status, counts)
    return out, headroom


def coverage_report(cov, headroom, vpp_mv, scale, midpoint, channels=(1, 2)):
    def entry(r, counts):
        return "r%d (%s)" % (r, " / ".join("CH%d %.0f" % (ch, counts[ch]) for ch in channels))
    by = {s: [(r, c) for r, (st, c) in sorted(cov.items()) if st == s]
          for s in ("ok", "coarse", "clips", "nocal")}
    where = ("its midpoint" if midpoint else "its LOW level") + " centred at code 128"
    lines = [f"coverage at {vpp_mv:.0f} mVpp (scope_cal.c gains x SCOPE_CAL_SOURCE_SCALE "
             f"{scale:.2f}; {where}, so a drive may use {headroom} counts):"]
    if by["ok"]:
        lines.append("  fits     " + ", ".join(entry(r, c) for r, c in by["ok"]))
    if by["coarse"]:
        lines.append("  coarse   " + ", ".join(entry(r, c) for r, c in by["coarse"])
                     + f"  -- under {MIN_RESOLVED_COUNTS} counts: +/-2 counts of span "
                       "quantisation is over 10%")
    if by["clips"]:
        lines.append("  clips    " + ", ".join(entry(r, c) for r, c in by["clips"])
                     + f"  -- over {headroom} counts"
                     + ("" if midpoint else
                        f" (--center-mid allows {FULL_SCALE_COUNTS - 2 * CLIP_MARGIN})"))
    if by["nocal"]:
        lines.append("  no cal   " + " ".join("r%d" % r for r, _ in by["nocal"])
                     + "  -- gain 0.0f in scope_cal.c")
    return "\n".join(lines)


def parse_center(text):
    """The `fpga scope center chN r` report line, or None."""
    m = _CENTER_RE.search(text or "")
    if not m:
        return None
    return {"ch": int(m.group(1)), "range": int(m.group(2)), "ref": m.group(3),
            "dac": int(m.group(4)), "median": int(m.group(5)),
            "stale": "STALE" in m.group(6)}


def build_parser():
    ap = argparse.ArgumentParser(
        description="Hold the signal, change the range: does the instrument report "
                    "the same voltage each time?",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ranges", type=int, nargs="+", default=None,
                    help="frontend ranges (default: 5 6 7; kodedot: every range "
                         "its one amplitude fits)")
    ap.add_argument("--amp", type=int, nargs="+", default=None, metavar="MVPP",
                    help="drive amplitudes in mVpp: LO HI, or a single value for a "
                         "fixed-amplitude source (default: 1000 2000; kodedot: its "
                         "rail, --v3v3)")
    ap.add_argument("--waveform", choices=WAVEFORMS, default=None,
                    help="stimulus shape (default per source: esp32 = the historical "
                         "tri/square pair, kodedot = square, manual = sine)")
    ap.add_argument("--freq", type=float, default=None, metavar="HZ",
                    help="drive frequency for kodedot/manual (default 330 Hz)")
    ap.add_argument("--channels", type=int, nargs="+", choices=(1, 2), default=[1, 2],
                    help="scope channels to measure (default: both)")
    ap.add_argument("--center-mid", action="store_true",
                    help="kodedot: centre on each rail and set the offset DAC to the "
                         "midpoint, so the square may use the whole ADC span")
    ap.add_argument("--timebase", type=lambda s: int(s, 0), default=None, metavar="CODE",
                    help="set reg 0x01 first, e.g. 0x10 (the table was taken there); "
                         "default: leave the live timebase alone")
    ap.add_argument("--reps", type=int, default=5)
    ap.add_argument("--settle", type=float, default=1.4,
                    help="seconds; must exceed one capture buffer (~0.96 s)")
    add_source_args(ap)
    return ap


def plan_run(args, table, scale):
    """Validate the request against what the source can produce, BEFORE any
    port is opened. Returns a dict; raises SystemExit with the reason."""
    cls = SOURCE_CLASS[args.source]
    chans = tuple(sorted(set(args.channels)))
    if args.waveform is not None and args.waveform not in cls.waveforms:
        raise SystemExit(f"--source {args.source} cannot produce a {args.waveform}: it "
                         f"makes {' / '.join(cls.waveforms)} only")
    waveform = args.waveform or cls.default_waveform
    if args.center_mid and not cls.can_hold:
        raise SystemExit("--center-mid needs a source that can hold both of its levels "
                         "(--source kodedot)")
    midpoint = cls.quiet_is_midpoint or args.center_mid

    fixed = args.v3v3 * 1000.0 if args.source == "kodedot" else None
    amps = list(args.amp) if args.amp else ([round(fixed)] if fixed else [1000, 2000])
    if len(amps) > 2:
        raise SystemExit("--amp takes LO HI, or one value")
    if len(amps) == 2 and amps[1] <= amps[0]:
        raise SystemExit("--amp needs LO < HI; equal amplitudes make the "
                         "two-point estimator a division by zero")

    if fixed is not None:
        bad = [a for a in amps if abs(a - fixed) > cls.AMP_TOLERANCE * fixed]
        cov, head = range_coverage(table, scale, fixed, midpoint, chans)
        if bad or len(amps) != 1:
            raise SystemExit(
                f"--source kodedot cannot produce {' and '.join('%d mVpp' % a for a in amps)}: "
                f"its only amplitude is its logic level, {fixed:.0f} mVpp — the 3V3 rail "
                f"as measured on a DMM (--v3v3 {args.v3v3:g}).\n"
                + coverage_report(cov, head, fixed, scale, midpoint, chans)
                + f"\nRe-run without --amp (it defaults to {fixed:.0f}), or with --amp "
                  f"{fixed:.0f}.")
        amps = [fixed]                    # the reference is the measurement
    elif args.source == "esp32":
        over = [a for a in amps if not 0 < a <= bench.Esp32Source.MAX_VPP_MV]
        if over:
            raise SystemExit(f"--source esp32 cannot produce {over} mVpp: its DAC "
                             f"swings 0..{bench.Esp32Source.MAX_VPP_MV:.0f} mVpp")

    hi = amps[-1]
    cov, head = range_coverage(table, scale, hi, midpoint, chans)
    if args.ranges is None:
        ranges = ([r for r, (st, _) in sorted(cov.items()) if st in ("ok", "coarse")]
                  if fixed is not None else [5, 6, 7])
    else:
        ranges = list(args.ranges)
    bad_r = [r for r in ranges if r not in cov]
    if bad_r:
        raise SystemExit(f"--ranges {bad_r}: ranges are 0..9")
    refused = [r for r in ranges if cov[r][0] in ("clips", "nocal")]
    if fixed is not None and refused:
        raise SystemExit(
            f"--source kodedot at {fixed:.0f} mVpp cannot measure range(s) "
            f"{', '.join(map(str, refused))}: a clipped span is a lower bound, and a "
            "range with no calibration has nothing to verify.\n"
            + coverage_report(cov, head, fixed, scale, midpoint, chans))
    if len(ranges) < 2:
        print("note: fewer than two ranges — the cross-range agreement test needs two")
    return {"waveform": waveform, "midpoint": midpoint, "amps": amps, "ranges": ranges,
            "channels": chans, "coverage": cov, "headroom": head,
            "warn_clips": [r for r in ranges if cov[r][0] == "clips"]}


def main(argv=None):
    args = build_parser().parse_args(argv)
    table = load_table()
    scale = load_source_scale()
    plan = plan_run(args, table, scale)
    chans, ranges, amps, wf = plan["channels"], plan["ranges"], plan["amps"], plan["waveform"]
    two_amps = len(amps) == 2
    lo_mv, hi_mv = (amps[0], amps[1]) if two_amps else (None, amps[0])
    sleep = (lambda _s: None) if args.dry_run else time.sleep

    sim_gains = {ch: [table[ch][r] * scale for r in range(10)] for ch in (1, 2)}
    sc, src, _sim = open_bench(args, waveform=wf, gains=sim_gains)
    try:
        run(args, sc, src, table, scale, plan, chans, ranges, wf, two_amps,
            lo_mv, hi_mv, sleep)
    finally:
        src.close()
        sc.close()


def run(args, sc, src, table, scale, plan, chans, ranges, wf, two_amps, lo_mv, hi_mv,
        sleep):
    print("build:", sc.version().strip().replace("\r\n", " | "))
    print(f"table: {os.path.relpath(CAL_SRC)}")
    print(f"source: {src.describe()}; waveform {wf}"
          + ("" if src.outputs > 1 else "; one output wired to both probes"))
    print(coverage_report(plan["coverage"], plan["headroom"], hi_mv, scale,
                          plan["midpoint"], chans))
    for r in plan["warn_clips"]:
        print(f"  WARNING: range {r} is expected to clip at {hi_mv:.0f} mVpp")
    if args.timebase is not None:
        sc.timebase(args.timebase)

    def capture(op):
        """Median span, median mean, and whether any read touched a rail."""
        spans, means, clipped = [], [], False
        for _ in range(args.reps):
            v = sc.opread(op)
            spans.append(float(v.max() - v.min()))
            means.append(float(v.mean()))
            clipped = clipped or bool(v.min() <= 0 or v.max() >= 255)
        return statistics.median(spans), statistics.median(means), clipped

    def span(op):
        return capture(op)[0]

    def center_mid(r):
        """Centre on each rail, then put the offset DAC at the midpoint.

        `fpga scope center` servos the offset DAC until a QUIET input sits at
        code 128. Run it with the pin held low and again held high; the offset
        DAC is linear (DAC1 on CH1, TMR13's RC-filtered PWM on CH2), so the
        midpoint code puts the square's midpoint at 128. All servo runs happen
        before either midpoint is written, so re-applying the range for one
        channel cannot undo the other. A failed servo (median off target, or a
        STALE warning) leaves the low rail at 128, and the clip flag on the
        rows says whether that mattered."""
        got = {}
        for ch in chans:
            src.hold(0)
            sleep(args.settle)
            lo = parse_center(sc.cmd(f"fpga scope center ch{ch} {r}", timeout=20))
            src.hold(1)
            sleep(args.settle)
            hi = parse_center(sc.cmd(f"fpga scope center ch{ch} {r}", timeout=20))
            got[ch] = (lo, hi)
        for ch, (lo, hi) in got.items():
            good = all(c is not None and not c["stale"]
                       and abs(c["median"] - CENTRE_CODE) <= 6 for c in (lo, hi))
            if good:
                dac = int(round((lo["dac"] + hi["dac"]) / 2))
                print(f"  r{r} CH{ch}: offset DAC low rail {lo['dac']}, high rail "
                      f"{hi['dac']} -> midpoint {dac}")
            else:
                dac = lo["dac"] if lo else None
                print(f"  r{r} CH{ch}: two-level centring FAILED (low {lo}, high {hi}); "
                      "the low rail stays at 128")
            if dac is not None:
                sc.cmd(f"{'trig' if ch == 1 else 'trig2'} raw {dac}")
        src.quiet()

    def setup(r):
        sc.scope_range(r, 1)
        sc.scope_range(r, 2)
        if src.center_on_quiet:
            src.quiet()
        if args.center_mid:
            center_mid(r)
        else:
            for ch in chans:
                sc.cmd(f"fpga scope center ch{ch} {r}", timeout=20)
        sleep(args.settle)

    def drive(mvpp):
        ref = src.drive(mvpp, waveform=wf, hz=args.freq)
        sleep(args.settle)
        return ref

    def ops(vals, fmt):
        return "   ".join(f"op{OPCODE[ch]:02x} {vals[ch]:{fmt}}" for ch in chans)

    # ── Control first. A negative here voids everything below. ──────────
    print("\n=== CONTROL (run first) ===")
    r_ctrl = ranges[len(ranges) // 2]
    # A control on a range the drive barely moves proves little; if the
    # middle range is coarse at this amplitude, use the best-resolved one.
    if plan["coverage"][r_ctrl][0] != "ok":
        r_ctrl = max(ranges, key=lambda r: min(plan["coverage"][r][1].values(), default=0))
    setup(r_ctrl)
    drive(0)
    f = {ch: span(OPCODE[ch]) for ch in chans}
    drive(hi_mv)
    d = {ch: span(OPCODE[ch]) for ch in chans}
    print(f"  range {r_ctrl} quiet:  {ops(f, '6.2f')}")
    print(f"  range {r_ctrl} driven: {ops(d, '6.2f')}")
    ok = all(d[ch] > 4 * max(f[ch], 1.0) for ch in chans)
    print(f"  control {'PASSED' if ok else 'FAILED'} "
          "(drive must lift span >=4x over the quiet floor)")
    if not ok:
        print("\n  *** VOID: span is not tracking the drive. Check the probe, "
              "the generator and the channel mask before reading anything "
              "below. ***")

    # ── Sweep ───────────────────────────────────────────────────────────
    static = src.can_hold
    amps_txt = f"{lo_mv:.0f} and {hi_mv:.0f}" if two_amps else f"{hi_mv:.0f}"
    print(f"\n=== per-range, {amps_txt} mVpp ===")
    print("rng ch |  floor     lo     hi | floor-corr | two-point | tier-gain"
          " | expect" + (" | static-lvl" if static else ""))
    rows = {ch: {} for ch in chans}
    for r in ranges:
        setup(r)
        drive(0)
        fl = {ch: capture(OPCODE[ch]) for ch in chans}
        hl = {}
        if static:
            src.hold(1)
            sleep(args.settle)
            hl = {ch: capture(OPCODE[ch]) for ch in chans}
        lo = {}
        lo_ref = None
        if two_amps:
            lo_ref = drive(lo_mv)
            lo = {ch: capture(OPCODE[ch]) for ch in chans}
        hi_ref = drive(hi_mv)
        hi = {ch: capture(OPCODE[ch]) for ch in chans}

        for ch in chans:
            k = table[ch][r]
            lo_txt = f"{lo[ch][0]:6.1f}" if two_amps else f"{'-':>6}"
            if k <= 0.0:
                print(f" {r}  {ch} | {fl[ch][0]:6.1f} {lo_txt} {hi[ch][0]:6.1f} |"
                      f"     (no cal — range marked unusable)")
                continue
            clipped = fl[ch][2] or hi[ch][2] or (two_amps and lo[ch][2]) or \
                (static and hl[ch][2])
            corrected = (hi[ch][0] - fl[ch][0]) * k
            est = {"floor-corrected": (corrected, max(hi[ch][0] - fl[ch][0], 1.0), 2.0)}
            tp_txt = f"{'n/a':>10}"
            if two_amps:
                twopoint = (hi[ch][0] - lo[ch][0]) * k * (hi_ref / (hi_ref - lo_ref))
                # Denominators in COUNTS, kept so the quantisation floor of each
                # estimator can be computed below. Both are a difference of two
                # integer-valued spans, so each carries about +/-2 counts.
                est["two-point"] = (twopoint, max(hi[ch][0] - lo[ch][0], 1.0), 2.0)
                tp_txt = f"{twopoint:8.0f}mV"
            st_txt = ""
            if static:
                # Difference of two MEANS of ~1024 samples: no floor term and no
                # edge overshoot, and resolved far below one count; 0.5 count is
                # a deliberately pessimistic quantisation floor for it.
                dm = hl[ch][1] - fl[ch][1]
                est["static-level"] = (dm * k, max(dm, 1.0), 0.5)
                st_txt = f" | {dm * k:8.0f}mV"
            expect = expected_span_counts(hi_ref, k * scale)
            flag = "   CLIPPED — excluded" if clipped else ""
            print(f" {r}  {ch} | {fl[ch][0]:6.1f} {lo_txt} {hi[ch][0]:6.1f} |"
                  f" {corrected:8.0f}mV | {tp_txt} | {k:6.2f} mV/ct"
                  f" | {expect:6.1f}{st_txt}{flag}")
            if not clipped:
                rows[ch][r] = (est, hi_ref)

    # ── The actual test ─────────────────────────────────────────────────
    ref_word = "reference" if src.trusted_amplitude else "commanded"
    print(f"\n=== agreement across ranges ({ref_word} {hi_mv:.0f} mVpp) ===")
    labels = (("floor-corrected", "floor-corrected"), ("two-point", "two-point     "),
              ("static-level", "static-level  "))
    for ch in chans:
        if len(rows[ch]) < 2:
            print(f"  CH{ch}: fewer than two calibrated ranges — nothing to compare")
            continue
        for key, label in labels:
            rs = [r for r in sorted(rows[ch]) if key in rows[ch][r][0]]
            if len(rs) < 2:
                continue
            vals = [rows[ch][r][0][key][0] for r in rs]
            ratios = [rows[ch][r][0][key][0] / rows[ch][r][1] for r in rs]
            mean = statistics.mean(vals)
            mean_ratio = statistics.mean(ratios)
            spread = (max(ratios) - min(ratios)) / mean_ratio

            # An estimator cannot resolve a disagreement smaller than its own
            # quantisation. Each span is a difference of integer sample values,
            # so the denominator carries about +/-2 counts; at range 7 the
            # two-point denominator is ~12 counts, i.e. +/-17%, and calling
            # that "INCONSISTENT" against a flat 15% threshold would be the
            # instrument failing to know what it can detect. Compare the
            # spread against the estimator's own floor instead.
            quant = max(rows[ch][r][0][key][2] / rows[ch][r][0][key][1] for r in rs)
            limit = max(0.10, 1.5 * quant)
            verdict = ("consistent" if spread <= limit
                       else "INCONSISTENT")
            note = "" if spread <= limit else "  <-- exceeds quantisation floor"
            implied = (f"   -> SCOPE_CAL_SOURCE_SCALE {1.0 / mean_ratio:.3f} (compiled "
                       f"{scale:.2f})" if src.trusted_amplitude else "")

            print(f"  CH{ch} {label}: "
                  f"{' / '.join(f'{v:.0f}' for v in vals)} mVpp"
                  f"   spread {spread * 100:4.1f}%"
                  f"  (quantisation floor {quant * 100:4.1f}%, limit "
                  f"{limit * 100:4.1f}%)  {verdict}{note}"
                  f"   mean/{ref_word} {mean / hi_mv if not src.trusted_amplitude else mean_ratio:.3f}"
                  f"{implied}")

    src.quiet()

    if src.outputs < 2:
        print("\nBLIND SPOT: one output into both channels. The two-SHAPES control "
              "(triangle on CH1, square on CH2) is not available, so a display that "
              "duplicated one channel onto the other would not be caught here.")
    if src.trusted_amplitude:
        print("""
NOTE ON THE mean/reference COLUMN
  The reference is a MEASURED amplitude (a DMM), not the generator these gains
  were derived from, so this ratio IS an accuracy figure for the raw table,
  and 1/ratio is the measured value of SCOPE_CAL_SOURCE_SCALE — printed as
  "->". Compare it with the compiled constant; if it moves, change ONLY that
  constant in scope_cal.h, one range being enough, because a source scale
  error is uniform across the whole table by construction. Do not adjust
  individual rows; tests/test_scope_cal.c will fail if you do. Its accuracy
  is the DMM's plus this script's quantisation floor.""")
    else:
        print("""
NOTE ON THE mean/commanded COLUMN
  While the drive is the ESP32 generator that these gains were derived from,
  this ratio is close to circular and is NOT an accuracy figure. Against a
  CALIBRATED source it becomes one, and 1/ratio is then what
  SCOPE_CAL_SOURCE_SCALE should be set to — one range is enough, because a
  source scale error is uniform across the whole table by construction.
  Do not adjust individual rows; tests/test_scope_cal.c will fail if you do.""")


if __name__ == "__main__":
    try:
        main()
    except BenchError as exc:
        raise SystemExit(f"bench: {exc}")
