#!/usr/bin/env python3
"""EXP-19 — badge validation: do the numbers on the screen survive the bench?

Drives a commanded sine across a grid of (measured range x measured timebase
code) and compares what `fpga scope measure` reports — which is computed by
the SAME functions the badges call — against the command.

WHAT THIS DOES AND DOES NOT VALIDATE
------------------------------------
The vertical cal table traces to this same ESP32 source, so Vpp/Vrms
agreement here validates the PIPELINE (right channel's k, right range's k,
the multiply, the refusals) and cross-range consistency — NOT absolute
volts. Absolute waits on the calibrated source and SCOPE_CAL_SOURCE_SCALE
(scope_cal.h). Frequency IS absolute (source loop-rate measured against the
ESP32 crystal, EXP-14). The Vrms/Vpp ratio check is source-independent: a
clean sine must show 1/(2*sqrt(2)) = 0.3536 regardless of scale.

Usage:
    python3 scripts/exp19_badge_validation.py
    python3 scripts/exp19_badge_validation.py --ranges 5 6 7 --codes 0x10 0x12
"""
import argparse
import re
import statistics
import sys
import time

sys.path.insert(0, "scripts")
from bench import Scope, Siggen  # noqa: E402

SINE_RATIO = 0.35355  # Vrms/Vpp for a pure sine

# Amplitudes per range, chosen to use a healthy slice of each range's span
# without clipping (gains ~21.8 / 43.0 / 88.4 mV/count on r5/6/7).
AMPS = {5: [800, 2000, 3000], 6: [1500, 3000], 7: [2000, 3300]}
# Tone per code: lands in a comfortable bin (>>MIN_BIN, <<Nyquist/2).
TONE = {0x10: 400.0, 0x12: 100.0, 0x0F: 800.0, 0x0E: 1600.0}

M_RE = re.compile(
    r"M\s+\d+\s+pp1=(\d+)\s+ppr1=(\d+)\s+Vpp1_uV=(\S+)\s+Vrms1_uV=(\S+)\s+"
    r"duty1_pm=(\d+)\s+per1_smp100=(\S+)\s+f1_mHz=(\S+)\s+pp2=(\d+)\s+Vpp2_uV=(\S+)")


def read_measure(sc, reps=10):
    out = sc.cmd(f"fpga scope measure {reps}", timeout=reps * 0.3 + 5)
    rows = []
    for m in M_RE.finditer(out):
        rows.append({
            "pp1": int(m.group(1)),
            "ppr1": int(m.group(2)),
            "vpp_uV": None if m.group(3) == "-" else int(m.group(3)),
            "vrms_uV": None if m.group(4) == "-" else int(m.group(4)),
            "f_mHz": None if m.group(7) == "-" else int(m.group(7)),
        })
    if not rows:
        raise RuntimeError("no M lines parsed from:\n" + out)
    return rows


def med(rows, key):
    vals = [r[key] for r in rows if r[key] is not None]
    return statistics.median(vals) if vals else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ranges", type=int, nargs="+", default=[5, 6, 7])
    ap.add_argument("--codes", type=lambda x: int(x, 0), nargs="+",
                    default=[0x10, 0x12])
    ap.add_argument("--reps", type=int, default=10)
    args = ap.parse_args()

    sc = Scope()
    sg = Siggen()
    # EXP-14's documented trap, walked into by this harness's first run: the
    # source's loop rate depends on its CHANNEL CONFIGURATION (~300 Hz per
    # waveform channel), so it must be measured in the exact config the sweep
    # runs in. Run 1 measured it with a stale config and every frequency came
    # back a constant -0.93%. Configure first, measure second.
    sg.sine(400.0, ch=1)
    sg.off(2)
    true_per_commanded = sg.use_measured_fs(True)  # frequency truth
    print(f"# source correction (ch1 sine + ch2 off): {true_per_commanded}")

    results = []
    fails = 0

    for code in args.codes:
        tone = TONE[code]
        print(f"\n== timebase 0x{code:02X}, tone {tone} Hz ==")
        print(sc.timebase(code).strip())
        for r in args.ranges:
            sc.vdiv(1, r)
            sc.cmd(f"fpga scope center ch1 {r}", timeout=25)
            for mvpp in AMPS[r]:
                sg.sine(tone, ch=1)
                sg.amp(mvpp, ch=1)
                time.sleep(0.6)
                rows = read_measure(sc, args.reps)
                vpp = med(rows, "vpp_uV")
                vrms = med(rows, "vrms_uV")
                f = med(rows, "f_mHz")
                vpp_mv = vpp / 1000.0 if vpp else None
                ratio = (vrms / vpp) if (vpp and vrms) else None
                f_hz = f / 1000.0 if f else None
                vpp_err = (vpp_mv / mvpp - 1) * 100 if vpp_mv else None
                f_err = ((f_hz / tone - 1) * 100) if f_hz else None
                ratio_err = ((ratio / SINE_RATIO - 1) * 100) if ratio else None
                # Vrms vs commanded*0.3536 is the primary vertical criterion:
                # pp is an extreme-value statistic and noise inflates it (run
                # 2 measured +3..+10%, shrinking with amplitude — the additive
                # signature), while rms barely moves. Judging the ratio would
                # double-count pp's noise.
                vrms_mv = vrms / 1000.0 if vrms else None
                vrms_err = ((vrms_mv / (mvpp * SINE_RATIO) - 1) * 100
                            if vrms_mv else None)
                row = dict(code=code, rng=r, mvpp=mvpp, vpp_mv=vpp_mv,
                           vpp_err=vpp_err, vrms_err=vrms_err,
                           ratio=ratio, ratio_err=ratio_err,
                           f_hz=f_hz, f_err=f_err,
                           answered=sum(1 for x in rows if x["f_mHz"]))
                results.append(row)
                bad = (vpp_err is None or abs(vpp_err) > 12.0 or
                       vrms_err is None or abs(vrms_err) > 6.0 or
                       (f_err is not None and abs(f_err) > 2.0))
                fails += bad
                print(f"  r{r} {mvpp:4d}mVpp -> Vpp {vpp_mv and round(vpp_mv,1)}mV "
                      f"({vpp_err and round(vpp_err,1)}%)  "
                      f"Vrms err {vrms_err and round(vrms_err,1)}%  "
                      f"rms/pp {ratio and round(ratio,4)}  "
                      f"f {f_hz and round(f_hz,2)}Hz ({f_err and round(f_err,2)}%) "
                      f"[{row['answered']}/{args.reps} freq]"
                      + ("  <-- FAIL" if bad else ""))

    # -- refusal controls (must refuse, not report) -------------------------
    print("\n== refusal controls ==")
    sg.sine(TONE[0x10], ch=1)
    sg.amp(2000, ch=1)

    sc.timebase(0x10)
    sc.vdiv(1, 2)                 # range 2: rails, cal tier NONE, k = 0
    time.sleep(0.6)
    rows = read_measure(sc, 5)
    c1 = all(r["vpp_uV"] is None for r in rows)
    print(f"  range 2 (no cal): Vpp refuses on all reads: {c1}")

    sc.vdiv(1, 5)
    sc.cmd("fpga scope center ch1 5", timeout=25)
    sc.timebase(0x0C)             # 0x0C: no measured rate
    time.sleep(0.6)
    rows = read_measure(sc, 5)
    c2 = all(r["f_mHz"] is None for r in rows)
    vpp_still = med(rows, "vpp_uV") is not None
    print(f"  code 0x0C (no rate): freq refuses on all reads: {c2}; "
          f"Vpp still reports (independent axes): {vpp_still}")

    sc.timebase(0x10)             # leave the device in a sane state
    sg.off(1)

    ok = fails == 0 and c1 and c2 and vpp_still
    verdict = ("PASS" if ok else
               f"FAIL ({fails} grid misses, controls {c1}/{c2}/{vpp_still})")
    print(f"\nRESULT: {verdict}")
    print("# grid rows (markdown):")
    print("| code | rng | commanded mVpp | badge Vpp | err% | Vrms err% "
          "| rms/pp | badge f | err% | freq answered |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for w in results:
        print(f"| 0x{w['code']:02X} | {w['rng']} | {w['mvpp']} "
              f"| {w['vpp_mv'] and round(w['vpp_mv'],1)} "
              f"| {w['vpp_err'] and round(w['vpp_err'],1)} "
              f"| {w['vrms_err'] and round(w['vrms_err'],1)} "
              f"| {w['ratio'] and round(w['ratio'],4)} "
              f"| {w['f_hz'] and round(w['f_hz'],2)} "
              f"| {w['f_err'] and round(w['f_err'],2)} "
              f"| {w['answered']}/{args.reps} |")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
