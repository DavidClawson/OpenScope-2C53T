#!/usr/bin/env python3
"""EXP-38 -- does CH2 come up live from a cold boot with no shell command?

QUESTION
    CH2 was solved in two halves in August (PC1/PC2 channel mask, EXP-01/07;
    TMR13/PA6 offset arm, EXP-07/21) but every plain `guest-coldtrace` build
    since has read op05 as all zeros, because the boot-time arm sat behind
    FPGA_CH2_TRIGGER=0. As of 2026-09-14 it defaults to 1. Does that single
    default make CH2 live at boot, with nothing typed into the shell?

HYPOTHESIS AND FALSIFIER
    ON image (FPGA_CH2_TRIGGER=1): after an MCU reset, TMR13 CTRL1 bit0 = 1,
    C1DT = 2544, PA6 CFGLR nibble = AF-PP, and op05 carries the CH2 drive
    (peak at the 2 kHz bin) before any `trig2` is sent.
    OFF image (FPGA_CH2_TRIGGER=0): TMR13 CTRL1 = 0, C1DT = 0, PA6 = GPIO
    output, op05 span 0 -- the state every "CH2 dead" note was taken on.
    If the ON image reads op05 span 0, the default did not reach the boot
    path (or PA6 is being reclaimed after the arm) and the change is refuted.

DRIVE
    JDS6600 (crystal-accurate in frequency): CH1 = 1 kHz, CH2 = 2 kHz, 3 Vpp,
    both sine -- EXP-21's rig. Timebase 0x10 (12,490 S/s, non-tearing for
    opread), so the tones land at bins ~82 and ~164 of a 1024-point record.
    Range 5 on both channels (21-23 mV/count: 3 Vpp ~ 135 counts pp), which is
    also the range the 2544 boot code was centred for.

CONTROL (run FIRST, same session, same path)
    (a) DAC1 sweep on CH1 (`trig raw 1024/2048/3072`): op04 mean must move
        monotonically -- proves the offset-reference path is visible through
        this instrument.
    (b) Runtime arm (`trig2 raw 2544`) on a build where op05 was zeros: op05
        must wake with a peak at the 2 kHz bin -- proves this instrument can
        see CH2 waking, so a later "op05 is zeros" is a real negative.
    If either control fails the boot phases are VOID, not negative.

WHAT THE `trig2` SHELL COMMAND DOES
    cmd_scope_trig2() calls scope_trigger_ch2_init() UNCONDITIONALLY before
    parsing its arguments, so even a malformed `trig2` arms TMR13. The boot
    phase therefore reads TMR13/PA6 with `mem read` (pure register readback,
    no side effect) and NEVER sends `trig2` before the op05 record.

USAGE
    python3 scripts/exp38_ch2_boot_arm.py control            # on any image
    python3 scripts/exp38_ch2_boot_arm.py boot --expect on   # fresh ON boot
    python3 scripts/exp38_ch2_boot_arm.py boot --expect off  # fresh OFF boot
"""
import argparse
import os
import re
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bench  # noqa: E402

TIMEBASE = 0x10
FS = 12490.0
F1, F2 = 1000.0, 2000.0
VPP = 3.0
RANGE = 5
N_REC = 5

TMR13_CTRL1 = 0x40001C00
TMR13_C1DT = 0x40001C34
GPIOA_CFGLR = 0x40010800
ON_CODE = 2544


def mem32(sc, addr):
    txt = sc.cmd("mem read 0x%08X 1" % addr)
    m = re.search(r"0x%08X:\s+([0-9A-Fa-f]{8})" % addr, txt)
    if not m:
        raise bench.BenchError("mem read %#x: no value in reply:\n%s" % (addr, txt))
    return int(m.group(1), 16)


def readback(sc, label):
    """Side-effect-free: TMR13 and PA6 state as the hardware reports it."""
    ctrl1 = mem32(sc, TMR13_CTRL1)
    c1dt = mem32(sc, TMR13_C1DT)
    cfglr = mem32(sc, GPIOA_CFGLR)
    pa6 = (cfglr >> 24) & 0xF
    mode = {0x4: "floating in", 0x3: "GPIO out 50M", 0x1: "GPIO out 10M",
            0x2: "GPIO out 2M", 0xB: "AF-PP 50M", 0x9: "AF-PP 10M",
            0xA: "AF-PP 2M", 0x8: "in pull"}.get(pa6, "?")
    print("[%s] TMR13 CTRL1=%08X (CEN=%d)  C1DT=%d  PA6 nibble=%X (%s)"
          % (label, ctrl1, ctrl1 & 1, c1dt, pa6, mode))
    return ctrl1, c1dt, pa6


def record(sc, op):
    v = sc.opread(op)
    pk = bench.peaks(v, k=1)[0]
    return float(v.mean()), float(v.max() - v.min()), pk


def records(sc, label):
    out = {}
    for op, f in ((0x04, F1), (0x05, F2)):
        exp_bin = bench.bin_of(f, FS)
        rows = [record(sc, op) for _ in range(N_REC)]
        means = [r[0] for r in rows]
        spans = [r[1] for r in rows]
        bins = [r[2][0] for r in rows]
        print("[%s] op%02X  mean %5.1f..%5.1f  span %3.0f..%3.0f  peak bin %s (expect ~%.1f)"
              % (label, op, min(means), max(means), min(spans), max(spans),
                 bins, exp_bin))
        out[op] = dict(mean=float(np.median(means)), span=float(np.median(spans)),
                       bins=bins, exp_bin=exp_bin)
    return out


def tone_present(r, tol=3.0):
    """A signal with its peak within tol bins of the expected tone, all records."""
    return r["span"] > 20 and all(abs(b - r["exp_bin"]) <= tol for b in r["bins"])


def setup_drive(jds):
    snap = jds.state()
    jds.output(False, False)
    for ch, f in ((1, F1), (2, F2)):
        jds.waveform("sine", ch)
        jds.freq(f, ch)
        jds.amp(VPP, ch)
        jds.offset(0.0, ch)
    jds.output(True, True)
    print("[drive] JDS6600: CH1 %.0f Hz  CH2 %.0f Hz  %.1f Vpp sine, both on" % (F1, F2, VPP))
    return snap


def setup_scope(sc):
    sc.vdiv(1, RANGE)
    sc.vdiv(2, RANGE)
    sc.timebase(TIMEBASE)
    print("[scope] vdiv both = range %d, timebase 0x%02X" % (RANGE, TIMEBASE))


def phase_control(sc):
    print("=== CONTROL phase (any image) ===")
    readback(sc, "pre")
    base = records(sc, "baseline")

    # (a) DAC1 sweep on CH1 -- the offset path is visible.
    means = []
    for code in (1024, 2048, 3072):
        sc.cmd("trig raw %d" % code)
        time.sleep(0.3)
        m = np.median([record(sc, 0x04)[0] for _ in range(3)])
        means.append(m)
        print("[ctrl-a] trig raw %4d -> op04 mean %5.1f" % (code, m))
    sc.cmd("trig raw 2048")
    mono = (means[0] < means[1] < means[2]) or (means[0] > means[1] > means[2])
    print("[ctrl-a] DAC1 moves CH1 mean monotonically: %s" % ("PASS" if mono else "FAIL"))

    # (b) runtime CH2 arm -- the instrument can see CH2 wake.
    sc.cmd("trig2 raw %d" % ON_CODE)
    time.sleep(0.3)
    readback(sc, "post-trig2")
    armed = records(sc, "armed")
    ok_b = tone_present(armed[0x05])
    print("[ctrl-b] op05 carries the 2 kHz tone after trig2 raw %d: %s"
          % (ON_CODE, "PASS" if ok_b else "FAIL"))
    ok_a1 = tone_present(armed[0x04])
    print("[ctrl-b] op04 still carries the 1 kHz tone (channels independent): %s"
          % ("PASS" if ok_a1 else "FAIL"))
    print("[ctrl] baseline op05 span was %.0f (%s)" %
          (base[0x05]["span"], "zeros -> this image had no boot arm" if base[0x05]["span"] == 0
           else "already live -> this image arms at boot, or was armed earlier"))
    verdict = mono and ok_b and ok_a1
    print("=== CONTROL %s ===" % ("PASSED" if verdict else "FAILED -> boot phases VOID"))
    return verdict


def phase_boot(sc, expect):
    print("=== BOOT phase, expecting arm %s (no trig2 sent before the record) ===" % expect.upper())
    ctrl1, c1dt, pa6 = readback(sc, "boot")   # FIRST, before anything else
    r = records(sc, "boot")
    ch2 = tone_present(r[0x05])
    ch1 = tone_present(r[0x04])
    print("[boot] op04 1 kHz tone: %s   op05 2 kHz tone: %s" % (ch1, ch2))
    if expect == "on":
        regs_ok = (ctrl1 & 1) == 1 and c1dt == ON_CODE and pa6 in (0xB, 0x9, 0xA)
        verdict = regs_ok and ch1 and ch2
        print("[boot] ON: regs %s (CEN=%d C1DT=%d PA6=%X), CH2 live %s -> %s"
              % (regs_ok, ctrl1 & 1, c1dt, pa6, ch2, "CONFIRMED" if verdict else "REFUTED"))
    else:
        regs_ok = (ctrl1 & 1) == 0 and c1dt == 0
        verdict = regs_ok and ch1 and (r[0x05]["span"] == 0)
        print("[boot] OFF: regs %s (CEN=%d C1DT=%d PA6=%X), op05 span %.0f -> %s"
              % (regs_ok, ctrl1 & 1, c1dt, pa6, r[0x05]["span"],
                 "as predicted" if verdict else "UNEXPECTED"))
    return verdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", choices=("control", "boot"))
    ap.add_argument("--expect", choices=("on", "off"), default="on")
    ap.add_argument("--scope", default="/dev/ttyACM0")
    ap.add_argument("--jds", default="/dev/ttyUSB0")
    a = ap.parse_args()

    sc = bench.Scope(a.scope)
    jds = bench.JDS6600(a.jds)
    try:
        setup_drive(jds)
        if a.phase == "boot":
            # Register readback must precede vdiv/timebase? They do not touch
            # TMR13/PA6, but read first anyway so the order is unarguable.
            readback(sc, "pre-setup")
        setup_scope(sc)
        ok = phase_control(sc) if a.phase == "control" else phase_boot(sc, a.expect)
    finally:
        jds.close()
        sc.close()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
