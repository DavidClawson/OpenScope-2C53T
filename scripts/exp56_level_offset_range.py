#!/usr/bin/env python3
"""EXP-56 -- is the level - 28 trigger offset the same on a second range?

EXP-53 50j and EXP-55 measured the comparator seeing the record + 28 on range 5
(21.8 mV/count). If the offset is digital (an ADC-code bias between the trigger
comparator's view and the record) it is the same on every range; if it is analog
(a front-end offset before the attenuator) it scales with the range gain.

PREDICTIONS (written before the run)
    Per range in (5, 7) and level code L in (128, 143, 113): 2 Hz triangle,
    0x12, NORMAL, 8 records; r[512] = the trigger sample after un-rotating at
    the max-jump pointer.
    Control: range 5 reproduces EXP-55: r[512] = L - 28 +/- 3 at all three L.
    Digital offset (H1): range 7 also r[512] = L - 28 +/- 3.
    Analog offset (H2): range 7 shows a different constant (28 x 21.83/88.42
      = 6.9 if it is a fixed input voltage, so r[512] ~ L - 7).
    Falsifier for both: r[512] - L differs between the three levels on one
      range (then it is not an offset at all).
    Void: the range-5 control does not give L - 28.
    Blind spot: the triangle must span the crossing on both ranges; the
      script prints the record span and skips a level outside it. Range 7
      needs ~8 Vpp for a ~90-count span; DAC1 centring is per range
      (`fpga scope center`), and the record mean is printed as a check.
"""
import sys, time, re
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame

OUT = "/home/david/osc/reverse_engineering/captures/exp56/"

def trig_sample(v):
    dv = np.abs(np.diff(np.r_[v, v[0]])); k = int(np.argmax(dv)) + 1
    r = np.r_[v[k:], v[:k]]
    s = float(np.mean(r[516:524]) - np.mean(r[500:508]))
    return k, int(r[512]), "rise" if s > 0 else "fall"

def main():
    import os; os.makedirs(OUT, exist_ok=True)
    sc = Scope("/dev/ttyACM0"); saved = {}
    print("EXP-56 device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    sc.cmd("fpga scope trigmode auto")
    for rng, amp in ((5, 2.0), (7, 8.0)):
        jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "3"); jds.freq(2.0, 1); jds.amp(amp, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
        # Centre at the fast code (short fills), one range only: with no range
        # argument `fpga scope center` walks ALL ten ranges, minutes at 0x12
        # (the first attempt at this script hung the shell on exactly that).
        sc.cmd("fpga scope timebase 10", timeout=6.0)
        print(sc.cmd("fpga scope vdiv 1 %d" % rng, timeout=6.0).strip().splitlines()[-1][:100])
        t = sc.cmd("fpga scope center ch1 %d" % rng, timeout=120.0); print("  center:", [l.strip() for l in t.splitlines() if "DAC" in l or "mean" in l][-2:])
        sc.cmd("fpga scope timebase 12", timeout=6.0); sc.cmd("fpga scope trigmode auto"); time.sleep(2.0)
        v = np.asarray(grab_frame(sc)["ch1"], float)
        print("  range %d amp %.1f Vpp: record span %d..%d mean %.1f" % (rng, amp, v.min(), v.max(), v.mean()))
        # Levels from the measured span (first run used fixed codes 128/146/110
        # and every range-5 crossing fell outside 113..204): target crossings at
        # 30/50/70 % of the span, level code = crossing + 28. `fpga scope level n`
        # maps n -> code 128 + 1.2 n (readback: +15 -> 146, -15 -> 110).
        for frac in (0.5, 0.3, 0.7):
            target = v.min() + frac * (v.max() - v.min()) + 28
            lvl = int(round((target - 128) / 1.2))
            r = sc.cmd("fpga scope level %d" % lvl); L = int(re.search(r"code 0x([0-9A-Fa-f]+)", r).group(1), 16)
            if not (v.min() + 5 < L - 28 < v.max() - 5):
                print("  level %+d code %d: crossing %d outside the span, skipped" % (lvl, L, L - 28)); continue
            sc.cmd("fpga scope trigmode normal"); time.sleep(3.0)
            rows = []
            for i in range(8):
                x = np.asarray(grab_frame(sc)["ch1"], float); saved["r%d_L%d_%d" % (rng, L, i)] = x
                rows.append(trig_sample(x)); time.sleep(1.0)
            sc.cmd("fpga scope trigmode auto")
            t512 = [r_[1] for r_ in rows]
            print("  range %d level code %3d  predicted L-28 = %3d | r[512] %s | median offset L - r[512] = %.0f | edges %s" % (
                rng, L, L - 28, " ".join("%3d" % x for x in t512), L - float(np.median(t512)), "".join(r_[2][0] for r_ in rows)))
    np.savez(OUT + "exp56_records.npz", **saved)
    sc.cmd("fpga scope level 0"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.cmd("fpga scope vdiv 1 5", timeout=6.0); sc.cmd("fpga scope center ch1 5", timeout=120.0)
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(1000.0, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.close(); sc.close(); print("done")

if __name__ == "__main__":
    main()
