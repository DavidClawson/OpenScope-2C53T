#!/usr/bin/env python3
"""EXP-49 -- what a read returns in each engine state, and whether regs 02/06/07 move the bracket.

WHY
    EXP-48: a triggered capture cannot repeat faster than ~190 ms + fill, at
    any timebase, and no reg-01/08 write clears the holdoff. Stock reads every
    29 ms regardless. What do reads INSIDE the bracket return -- the held
    triggered record (static, seam-free, safe to display) or the rolling
    buffer? And what does a read return between arming and the crossing?

PREDICTIONS (written before the run)
    Part A, 201.2 Hz, task parked at 0x10, level 0. Reads in ms after read A
    (which arms; the crossing follows within 5 ms): B +0, C +100, D +200
    (all inside the bracket) return one identical static record; E at +400
    (arms again) and F right after E return the NEXT record, identical to
    each other, different from B.
    Part B, 5 Hz drive (crossing up to 200 ms after arming): reads at +20,
    +60, +100 ms after the arming read. If the engine double-buffers they are
    byte-identical to the last held record; if the read path is the live
    buffer they differ from each other.
    Part C, bracket probe (read, wait 100, read => +1 edge normally) with
    reg 06 / 07 / 02 set to 01, 10, FF before the pair, restored after.
    +2 on any value => that register shortens the holdoff.
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600                         # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402

def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1)))

def body_max(v, fs, f):
    w = 2 * np.pi * f / fs; n = np.arange(len(v)); A = np.c_[np.sin(w * n), np.cos(w * n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i + 32] ** 2)) for i in range(128, 1024, 32))

def drive(f, amp=3.0):
    jds = JDS6600("/dev/ttyUSB0"); jds.output(False, False); jds.waveform("sine", 1); jds.freq(f, 1); jds.amp(amp, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()

def timed_reads(sc, offsets_ms):
    """One arming read at t=0 then reads at the given offsets (ms after the arming read's end)."""
    recs = []; e0, _ = counters(sc)
    recs.append(("A", sc.opread(0x04))); t_end = time.time()
    for off in offsets_ms:
        while (time.time() - t_end) * 1000 < off: time.sleep(0.002)
        t = (time.time() - t_end) * 1000
        recs.append(("+%.0f" % t, sc.opread(0x04)))
    time.sleep(0.6); e1, _ = counters(sc)
    return recs, e1 - e0

def report(recs, fs, f):
    prev = None
    for lab, v in recs:
        same = "" if prev is None else ("identical to previous" if np.array_equal(v, prev) else "%d/1024 differ from previous" % int(np.sum(v != prev)))
        print("    %-6s body max rms %5.1f  span %3d  %s" % (lab, body_max(v.astype(float), fs, f), int(v.max() - v.min()), same))
        prev = v

def main():
    rates = load_rates(); fs = rates[0x10]
    sc = Scope("/dev/ttyACM0")
    print("EXP-49  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga postedge 0"); sc.cmd("fpga autowait 0")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0); time.sleep(0.5)
    sc.cmd("fpga scope trigmode single"); time.sleep(2.5)
    g = [grab_frame(sc)["gen"] for _ in range(2)]; print("  parked: gen %s (%s)" % (g, "held" if g[0] == g[1] else "NOT HELD"))

    print("\n=== A. 201.2 Hz: reads inside and after the bracket ===")
    drive(201.2); time.sleep(1.5)
    for rep in range(2):
        time.sleep(1.5); recs, de = timed_reads(sc, [0, 100, 200, 400, 400]); print("  run %d: edges +%d" % (rep + 1, de)); report(recs, fs, 201.2)

    print("\n=== B. 5 Hz: reads between arming and the crossing ===")
    drive(5.0); time.sleep(1.5)
    for rep in range(3):
        time.sleep(1.5); recs, de = timed_reads(sc, [20, 60, 100, 140]); print("  run %d: edges +%d" % (rep + 1, de)); report(recs, fs, 5.0)

    print("\n=== C. bracket probe (read, 100 ms, read) under reg 02/06/07 values; +1 = holdoff intact ===")
    drive(201.2); time.sleep(1.5)
    def bracket(label):
        time.sleep(1.5); e0, _ = counters(sc); sc.opread_stats(0x04); time.sleep(0.1); sc.opread_stats(0x04); time.sleep(0.6); e1, _ = counters(sc)
        print("  %-22s edges +%d" % (label, e1 - e0))
    bracket("baseline")
    for reg, restore in ((0x06, 0x00), (0x07, 0x00), (0x02, 0x03)):
        for val in (0x01, 0x10, 0xFF):
            sc.cmd("spi3 seq %02x %02x" % (reg, val)); time.sleep(0.3)
            bracket("reg %02X = %02X" % (reg, val))
            sc.cmd("spi3 seq %02x %02x" % (reg, restore)); time.sleep(0.3)
    bracket("restored baseline")
    sc.cmd("fpga scope trigmode auto"); sc.close()

if __name__ == "__main__":
    main()
