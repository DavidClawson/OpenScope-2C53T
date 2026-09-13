#!/usr/bin/env python3
"""EXP-30 -- does stock's PRE-read data-ready gate close the acquisition seam?

EXP-29 refuted the re-arm (stock's third step) and located the mechanism: one
discontinuity per record whose location marches through the buffer, which is
reading across a live write pointer. In AUTO mode the acq task, on a missing
trigger, "falls through and reads the live buffer" -- a read with no interlock
at all, and in AUTO that is the common case.

`fpga acqgate on` is stock's FIRST step: no data-ready edge, no read.

ARMS   A1 gate off + re-arm off (today's default)
       B  gate ON  + re-arm ON  (stock's shape; they are load-bearing for
          each other -- the gate waits for PC0, the re-arm starts the next
          capture so another PC0 can arrive)
       A2 back to the default, for drift

CONTROLS, and B needs all three or it is VOID not positive:
  1. The OFF arms must SHOW seams, or the metric is blind.
  2. **The frame generation counter must ADVANCE between records in arm B.**
     A gate that starves the acq task freezes the published buffer, and a
     frozen record has a constant seam -- which would read as success. This
     is the trap this experiment is most likely to fall into.
  3. Gate and re-arm states are read back off the device, never trusted from
     the setter.
"""
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, BenchError                       # noqa: E402
from exp22_stability import grab_frame, load_rates        # noqa: E402
from exp22_seam_analysis import frame_seam                # noqa: E402

F_DRIVE = 201.2
TB_CODE = 0x10
N_PER_ARM = 10
ARMS = [("A1", False, False), ("B", True, True), ("A2", False, False)]


def states(sc):
    g = re.search(r"acq gate (ON|OFF)", sc.cmd("fpga acqgate", timeout=4.0))
    r = re.search(r"acq re-arm (ON|OFF)", sc.cmd("fpga rearm", timeout=4.0))
    if not g or not r:
        raise BenchError("cannot read gate/re-arm state")
    return g.group(1) == "ON", r.group(1) == "ON"


def run_arm(sc, gate, rearm, n, fs):
    sc.cmd("fpga rearm %s" % ("on" if rearm else "off"), timeout=4.0)
    sc.cmd("fpga acqgate %s" % ("on" if gate else "off"), timeout=4.0)
    g, r = states(sc)
    if (g, r) != (gate, rearm):
        raise BenchError("toggles did not take: asked gate=%s rearm=%s, "
                         "device says gate=%s rearm=%s" % (gate, rearm, g, r))
    steps, gens, incoh = [], [], 0
    for _ in range(n):
        fr = grab_frame(sc)
        if fr["coherent"] != 1:
            incoh += 1
        step, _, _, _ = frame_seam(fr["ch1"], F_DRIVE, fs)
        steps.append(step)
        gens.append(fr["gen"])
    return np.array(steps), np.array(gens), incoh


def main():
    fs = load_rates()[TB_CODE]
    sc = Scope("/dev/ttyACM0")
    print("EXP-30  seam vs stock's pre-read gate")
    print("drive %.1f Hz, timebase 0x%02X = %.1f S/s, %d records/arm\n"
          % (F_DRIVE, TB_CODE, fs, N_PER_ARM))

    out = {}
    for name, gate, rearm in ARMS:
        steps, gens, incoh = run_arm(sc, gate, rearm, N_PER_ARM, fs)
        out[name] = (steps, gens)
        adv = np.diff(gens)
        frozen = bool(np.all(adv == 0))
        print("-- arm %s  gate=%s re-arm=%s (both readback-confirmed) --"
              % (name, "ON" if gate else "off", "ON" if rearm else "off"))
        print("   max-step samples: " + " ".join("%.1f" % v for v in steps))
        print("   median %.2f  p90 %.2f  max %.2f  incoherent %d/%d"
              % (np.median(steps), np.percentile(steps, 90), steps.max(),
                 incoh, N_PER_ARM))
        print("   gen advance/record: min %d max %d  %s\n"
              % (adv.min(), adv.max(),
                 "*** FROZEN -- ARM IS VOID ***" if frozen else "live"))

    print(sc.cmd("fpga acqgate", timeout=4.0).strip())
    a = np.concatenate([out["A1"][0], out["A2"][0]])
    b = out["B"][0]
    print("\n== result ==")
    print("   default   (n=%d): median %.2f  p90 %.2f"
          % (len(a), np.median(a), np.percentile(a, 90)))
    print("   gate+rearm(n=%d): median %.2f  p90 %.2f"
          % (len(b), np.median(b), np.percentile(b, 90)))
    print("   A1/A2 medians %.2f / %.2f (drift)"
          % (np.median(out["A1"][0]), np.median(out["A2"][0])))
    sc.cmd("fpga acqgate off", timeout=4.0)
    sc.cmd("fpga rearm off", timeout=4.0)
    sc.close()


if __name__ == "__main__":
    main()
