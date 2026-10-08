#!/usr/bin/env python3
"""EXP-73 -- is SPI3 reg 0x07 the hardware trigger-edge select?

Stock's register map (desk sweep 2026-08-15, decoded from its SPI dispatch):
0x06 = trigger source channel, 0x07 = trigger edge (ms[0x18], written on user
change and at boot). Our firmware only ever writes `07 00`, and with it the
FPGA fires on EITHER edge (EXP-53 50j, EXP-55), so Rising/Falling is an MCU
filter (`src/dsp/trig_edge.c`). EXP-55 tested reg 0x02, not 0x07. EXP-72's
hold sweep wrote 0x07 bits but only measured the cycle, not the slope.

PREDICTIONS (written before the run)
    Method: EXP-55's posture -- 2 Hz triangle, 2 Vpp, +0.5 V, range 5, level
    code 128 (crossing at record value ~100), timebase 0x12 (1024 samples =
    410 ms = 0.82 period), NORMAL. **MCU edge filter OFF** (it would drop
    wrong-edge records and hide a hardware effect). Per value: 10 committed
    records; slope at the trigger index = mean(r[516..523]) - mean(r[500..507])
    on the time-ordered record (ord=1), else un-rotated at the largest jump.
    Control `07 00` (boot value): both slopes present (EXP-55: 3-5 of 10 each).
    If H (0x07 is an edge select):
      some value (01 most likely) -> all 10 records one slope, and `00` -> the
      other slope or both (00 = 'both' would explain why we've always seen both).
    Falsifier: every value gives the mixed 3-7 split at the same handover rate.
    Also watch: a value that stops handovers (edges +0) = an enable, not a select.
    Blind spot: an edge select that latches only on a re-arm (reg 0x01 write)
    -- covered by re-writing the timebase after each value. A select that also
    needs reg 0x06 nonzero is not covered.
"""
import os, re, sys, time, hashlib
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame

OUT = "/home/david/osc/reverse_engineering/captures/exp73/"


def st(sc):
    t = sc.cmd("status", timeout=8.0)
    return int(re.search(r"PC0 edges: (\d+)", t).group(1))


def slope(v, ordered):
    if ordered:
        r = v
    else:
        dv = np.abs(np.diff(np.r_[v, v[0]])); k = int(np.argmax(dv)) + 1
        r = np.r_[v[k:], v[:k]]
    s = float(np.mean(r[516:524]) - np.mean(r[500:508]))
    return int(r[512]), s, ("rise" if s > 0 else "fall" if s < 0 else "flat")


def main():
    os.makedirs(OUT, exist_ok=True)
    sc = Scope("/dev/ttyACM0"); saved = {}
    print("EXP-73 device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "3"); jds.freq(2.0, 1); jds.amp(2.0, 1)
    jds.offset(0.5, 1); jds.output(True, False); jds.close()
    sc.scope_range(5, 1)
    sc.cmd("fpga scope timebase 12", timeout=6.0)
    r = sc.cmd("fpga scope level 0"); L = int(re.search(r"code 0x([0-9A-Fa-f]+)", r).group(1), 16)
    print("level code %d -> expected crossing in record scale %d" % (L, L - 28))
    print(sc.cmd("fpga edgefilter off").strip().splitlines()[-2:])
    sc.cmd("fpga scope trigmode normal"); time.sleep(3.0)
    for tag, val in (("control", 0x00), ("v01", 0x01), ("v02", 0x02), ("v03", 0x03), ("vFF", 0xFF), ("recovery", 0x00)):
        sc.cmd("spi3 seq 07 %02x" % val, timeout=6.0)
        sc.cmd("fpga scope timebase 12", timeout=6.0)        # re-arm write, in case the select latches on arm
        time.sleep(3.0)
        e0 = st(sc); rows = []; hashes = set(); t0 = time.time()
        for i in range(10):
            fr = grab_frame(sc)
            v = np.asarray(fr["ch1"], float); saved["%s_%02x_%d" % (tag, val, i)] = v
            h = hashlib.md5(v.tobytes()).hexdigest()[:8]; dup = h in hashes; hashes.add(h)
            rows.append(slope(v, fr.get("ord") == 1) + (dup,)); time.sleep(1.0)
        e1 = st(sc); dt = time.time() - t0
        ed = [x[2] for x in rows if not x[3]]
        print("reg07=%02x %-8s edges %.2f/s | distinct %2d/10 | rise %d fall %d flat %d | r[512] %s" % (
            val, tag, (e1 - e0) / dt, len(hashes), ed.count("rise"), ed.count("fall"), ed.count("flat"),
            " ".join("%3d" % x[0] for x in rows if not x[3])), flush=True)
    np.savez(OUT + "exp73_records.npz", **saved)
    sc.cmd("spi3 seq 07 00", timeout=6.0); sc.cmd("fpga edgefilter on"); sc.cmd("fpga scope trigmode auto")
    sc.cmd("fpga scope timebase 10", timeout=6.0)
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(1000.0, 1); jds.amp(2.0, 1); jds.offset(0.0, 1)
    jds.output(False, False); jds.close(); sc.close(); print("done")


if __name__ == "__main__":
    sys.exit(main())
