#!/usr/bin/env python3
"""EXP-40 -- does a settle time after the re-arm close the acquisition seam?

QUESTION
    EXP-29 refuted "re-arm after the pair" as the seam fix and EXP-39 refuted
    the in-band 0x80 marker as a data-ready source. Stock's op-01 handler is
    gated on `tbl[0x0804D833 + tb] + 0x32` elapsed since the last arm -- a
    per-timebase TIME table (u8, ~1024/fs in 10 ms units), not a status bit.
    EXP-29 re-armed and then read ~30 ms later at every timebase, i.e. in the
    middle of the re-armed capture. Is the missing interlock simply "wait for
    the capture to finish before reading"?

HYPOTHESIS AND FALSIFIER
    If a reg-01 write restarts a capture that then completes and holds, then
    re-arm + a settle >= 1024/fs before the next read yields records with NO
    seam (max phase step at the noise floor), while re-arm + settle 0 (EXP-29's
    shape) and no re-arm keep the ~20-sample seam.
    If the seam is unchanged with the settle, the engine free-runs regardless
    of the reg-01 write and the interlock is something else (bank swap, a
    command we do not send).

ARMS  A1  re-arm off, wait 0          (default; must SHOW seams)
      B   re-arm ON,  wait 0          (EXP-29's arm B, replicated)
      C   re-arm ON,  wait ceil(1.15 * 1024/fs * 1000) ms
      A2  re-arm off, wait 0          (drift)

CONTROLS, all three or the C arm is VOID:
  1. A1/A2 must show seams (metric not blind).
  2. The frame generation counter must ADVANCE between records in every arm
     (a starved acq task freezes the buffer, and a frozen record has a constant
     seam that reads as success -- EXP-30's trap).
  3. Re-arm and wait are read back from the device each arm, never trusted
     from the setter.
"""
import argparse
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600, BenchError            # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402
from exp22_seam_analysis import frame_seam               # noqa: E402

F_DRIVE = 201.2
N_PER_ARM = 10


def states(sc):
    r = re.search(r"acq re-arm (ON|OFF)", sc.cmd("fpga rearm", timeout=4.0))
    w = re.search(r"acq re-arm wait (\d+) ms", sc.cmd("fpga rearmwait", timeout=4.0))
    if not r or not w:
        raise BenchError("cannot read re-arm / wait state")
    return r.group(1) == "ON", int(w.group(1))


def run_arm(sc, rearm, wait_ms, n, fs):
    sc.cmd("fpga rearmwait %d" % wait_ms, timeout=4.0)
    sc.cmd("fpga rearm %s" % ("on" if rearm else "off"), timeout=4.0)
    r, w = states(sc)
    if (r, w) != (rearm, wait_ms):
        raise BenchError("toggles did not take: asked rearm=%s wait=%d, device says rearm=%s wait=%d"
                         % (rearm, wait_ms, r, w))
    steps, locs, gens, incoh = [], [], [], 0
    for _ in range(n):
        fr = grab_frame(sc)
        if fr["coherent"] != 1:
            incoh += 1
        step, loc, _, _ = frame_seam(fr["ch1"], F_DRIVE, fs)
        steps.append(step)
        locs.append(loc)
        gens.append(fr["gen"])
    return np.array(steps), np.array(locs), np.array(gens), incoh


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tb", type=lambda x: int(x, 0), default=0x10)
    ap.add_argument("--n", type=int, default=N_PER_ARM)
    ap.add_argument("--scope", default="/dev/ttyACM0")
    ap.add_argument("--jds", default="/dev/ttyUSB0")
    a = ap.parse_args()

    fs = load_rates()[a.tb]
    wait_ms = int(math.ceil(1.15 * 1024.0 / fs * 1000.0))
    arms = [("A1", False, 0), ("B", True, 0), ("C", True, wait_ms), ("A2", False, 0)]

    jds = JDS6600(a.jds)
    jds.output(False, False)
    jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False)
    jds.close()

    sc = Scope(a.scope)
    print("EXP-40  seam vs re-arm settle")
    print("drive %.1f Hz, timebase 0x%02X = %.1f S/s, capture %.0f ms, settle %d ms, %d records/arm\n"
          % (F_DRIVE, a.tb, fs, 1024.0 / fs * 1000.0, wait_ms, a.n))
    sc.cmd("fpga scope timebase %02x" % a.tb, timeout=6.0)
    out = {}
    void = False
    for name, rearm, w in arms:
        steps, locs, gens, incoh = run_arm(sc, rearm, w, a.n, fs)
        adv = np.diff(gens)
        frozen = bool(np.all(adv == 0))
        void |= frozen
        out[name] = steps
        print("-- arm %s  re-arm=%s wait=%d ms (readback-confirmed) --"
              % (name, "ON" if rearm else "off", w))
        print("   max-step samples: " + " ".join("%.1f" % v for v in steps))
        print("   median %.2f  p90 %.2f  max %.2f  incoherent %d/%d"
              % (np.median(steps), np.percentile(steps, 90), steps.max(), incoh, a.n))
        print("   seam locations:   " + " ".join("%d" % l for l in locs))
        print("   gen advance/record: min %d max %d  %s\n"
              % (adv.min(), adv.max(), "*** FROZEN -- ARM IS VOID ***" if frozen else "live"))
    sc.cmd("fpga rearm off", timeout=4.0)
    sc.cmd("fpga rearmwait 0", timeout=4.0)
    sc.close()

    a_ = np.concatenate([out["A1"], out["A2"]])
    print("== result ==")
    print("   re-arm off          (n=%d): median %.2f  p90 %.2f" % (len(a_), np.median(a_), np.percentile(a_, 90)))
    print("   re-arm, wait 0      (n=%d): median %.2f  p90 %.2f" % (a.n, np.median(out["B"]), np.percentile(out["B"], 90)))
    print("   re-arm, wait %3d ms (n=%d): median %.2f  p90 %.2f" % (wait_ms, a.n, np.median(out["C"]), np.percentile(out["C"], 90)))
    print("   A1/A2 medians %.2f / %.2f (drift)" % (np.median(out["A1"]), np.median(out["A2"])))
    if void:
        print("   *** at least one arm FROZEN -> VOID, not a result ***")


if __name__ == "__main__":
    main()
