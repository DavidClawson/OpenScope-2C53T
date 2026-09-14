#!/usr/bin/env python3
"""EXP-45 -- does a gap between the 0x04 and 0x05 reads let the engine cycle?

QUESTION
    Every shell read (0x04 or 0x05, /256, >= 50 ms apart) produces exactly one
    PC0 edge; a shell pair 50 ms apart produces two. The acquisition task's
    back-to-back pair (~140 us apart at /2) produces NONE, which is why NORMAL
    cannot sustain itself after its first record (EXP-44). Is the spacing of
    the two reads what decides whether a capture completes?

PREDICTION
    NORMAL at level 0 with `fpga pairgap 0` freezes after one record (edges
    +0). With a gap of some ms the generation advances continuously and edges
    arrive ~2 per pair (one per read, as the shell showed). If no gap up to
    50 ms helps, the spacing is not the variable (clock rate /2 vs /256 is
    the next suspect).

CONTROL
    gap 0 must reproduce the EXP-44 freeze; gap is read back per arm.
"""
import os
import re
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600                         # noqa: E402
from exp22_stability import grab_frame                    # noqa: E402

F_DRIVE = 201.2


def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()
    sc = Scope("/dev/ttyACM0")
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    sc.cmd("fpga scope level 0"); sc.cmd("fpga acqgate off"); sc.cmd("fpga rearm off")
    print("EXP-45  NORMAL sustain vs pair gap;  then AUTO edges/pair vs gap")
    for gap in (0, 2, 10, 50, 0):
        sc.cmd("fpga scope trigmode auto"); time.sleep(0.3)
        got = int(re.search(r"gap (\d+) ms", sc.cmd("fpga pairgap %d" % gap)).group(1))
        assert got == gap, "pairgap did not take"
        sc.cmd("fpga scope trigmode normal"); time.sleep(1.0)
        sc.opread_stats(0x04)                       # one shell prime so the mode has its first edge
        time.sleep(1.0)
        e0, o0 = counters(sc); gens = [grab_frame(sc)["gen"] for _ in range(6)]; e1, o1 = counters(sc)
        adv = np.diff(gens)
        print("NORMAL gap %2d ms: gens %s -> %s   edges +%d  OK +%d"
              % (gap, gens, "FROZEN" if np.all(adv == 0) else "advancing (adv %d..%d)" % (adv.min(), adv.max()), e1 - e0, o1 - o0))
    print()
    for gap in (0, 10, 50):
        sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga pairgap %d" % gap); time.sleep(1.0)
        e0, o0 = counters(sc); time.sleep(6.0); e1, o1 = counters(sc)
        print("AUTO  gap %2d ms: 6 s free-run+gated  edges +%d  pairs +%d  edges/pair %.2f  pairs/s %.1f"
              % (gap, e1 - e0, o1 - o0, (e1 - e0) / max(o1 - o0, 1), (o1 - o0) / 6.0))
    sc.cmd("fpga pairgap 0"); sc.cmd("fpga scope trigmode auto")
    sc.close()


if __name__ == "__main__":
    main()
