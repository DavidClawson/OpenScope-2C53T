#!/usr/bin/env python3
"""EXP-55 -- is SPI3 reg 0x02 the trigger edge-polarity select?

Stock writes `02 03` at boot and the FPGA triggers on EITHER edge of the level
(EXP-53 50j). If reg 0x02 is a two-bit edge enable (bit0 rising, bit1 falling),
`02 01` and `02 02` each leave one edge and `02 00` leaves none.

PREDICTIONS (written before the run)
    Method: 2 Hz triangle at 0x12 (2500 S/s, record = 410 ms = 0.82 period),
    NORMAL mode so only triggered records commit. Per committed record the
    pointer k is the max jump; un-rotated, the trigger sits at index 512 and
    the slope there (r[520] - r[504]) is the edge that fired.
    Control `02 03` (boot value): both slopes present across 10 records, and
      r[512] within a few counts of level - 28 (the EXP-53 offset).
    If H (edge enable) is true:
      `02 01` -> all rising (or all falling; which bit is which is the reading)
      `02 02` -> all the other slope
      `02 00` -> no new records in NORMAL (PC0 edges +0, records identical)
      `02 03` again -> both slopes (recovery control)
    Falsifier: every value behaves like 03 (mixed slopes, same edge rate), or
      a value changes something other than the slope mix (rate, level).
    Blind spot: reg 0x02 may select polarity only in combination with another
      register (e.g. reg 0x06/0x07 at 00), or the write may be ignored unless
      followed by a re-arm write to reg 0x01; the script does not re-arm.
      A value that kills triggering (00) is indistinguishable from a value
      that breaks the engine until the recovery control restores it.
"""
import sys, time, re, hashlib
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame

OUT = "/home/david/osc/reverse_engineering/captures/exp55/"

def st(sc):
    t = sc.cmd("status", timeout=6.0)
    return int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1))

def classify(v, L):
    dv = np.abs(np.diff(np.r_[v, v[0]])); k = int(np.argmax(dv)) + 1
    r = np.r_[v[k:], v[:k]]                       # r[0] oldest, r[1023] newest, trigger at 512
    s = float(np.mean(r[516:524]) - np.mean(r[500:508]))
    # Sign, not magnitude: the 2 Hz / 100-count triangle moves 0.16 count per
    # sample, so the 16-sample window spans ~2.5 counts. The first run used a
    # +/-3 threshold and scored 96/100 records "flat"; the signs were
    # unambiguous (|s| >= 1.0 on every record) and are what is reported.
    edge = "rise" if s > 0 else ("fall" if s < 0 else "flat")
    return k, int(r[512]), s, edge

def main():
    import os; os.makedirs(OUT, exist_ok=True)
    sc = Scope("/dev/ttyACM0"); saved = {}
    print("EXP-55 device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "3"); jds.freq(2.0, 1); jds.amp(2.0, 1); jds.offset(0.5, 1); jds.output(True, False); jds.close()
    sc.cmd("fpga scope timebase 12", timeout=6.0)
    r = sc.cmd("fpga scope level 0"); L = int(re.search(r"code 0x([0-9A-Fa-f]+)", r).group(1), 16)
    print("level code %d -> expected crossing in record scale %d" % (L, L - 28))
    sc.cmd("fpga scope trigmode normal"); time.sleep(3.0)
    for tag, val in (("control", 0x03), ("bit0", 0x01), ("bit1", 0x02), ("none", 0x00), ("recovery", 0x03)):
        t = sc.cmd("spi3 seq 02 %02x" % val, timeout=6.0)
        if "error" in t.lower() or "usage" in t.lower(): print("  write failed:", t.strip()[:120])
        time.sleep(3.0)
        e0, o0 = st(sc); rows = []; hashes = set()
        for i in range(10):
            v = np.asarray(grab_frame(sc)["ch1"], float); saved["%s_%02x_%d" % (tag, val, i)] = v
            h = hashlib.md5(v.tobytes()).hexdigest()[:8]; dup = h in hashes; hashes.add(h)
            rows.append(classify(v, L) + (dup,)); time.sleep(1.0)
        e1, o1 = st(sc)
        edges = [r_[3] for r_ in rows if not r_[4]]
        print("reg02=%02x %-8s edges +%3d reads +%3d | distinct records %2d/10 | rise %d fall %d flat %d | r[512] %s" % (
            val, tag, e1 - e0, (o1 - o0) // 2, len(hashes), edges.count("rise"), edges.count("fall"), edges.count("flat"),
            " ".join("%3d" % r_[1] for r_ in rows if not r_[4])))
        for k, v512, s, edge, dup in rows:
            print("      k %4d  r[512] %3d  slope %+6.1f  %-4s%s" % (k, v512, s, edge, "  (dup)" if dup else ""))
    np.savez(OUT + "exp55_records.npz", **saved)
    sc.cmd("spi3 seq 02 03", timeout=6.0); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(1000.0, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.close(); sc.close(); print("done")

if __name__ == "__main__":
    main()
