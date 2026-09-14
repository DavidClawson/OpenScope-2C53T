#!/usr/bin/env python3
"""EXP-46 -- what makes a read arm the next capture: time since the edge, or clock?

QUESTION
    Gated AUTO shows exactly 0.50 edges per pair at any 04/05 gap (EXP-45):
    the read issued immediately on a PC0 edge never yields the next edge; the
    fallback read 327 ms later always does. Shell reads (/256, seconds apart)
    yield an edge every time. Is it the delay after the edge, or the clock?

ARMS (NORMAL, level 0, one shell prime each; frozen = the mode could not
      sustain itself; advancing = every task pair arms the next capture)
    postedge 0 / 5 / 20 / 100 / 300 ms   at the task's default clock
    acqbr 7 (/256) with postedge 0       -- the shell's clock
    acqbr 0 (/2) with postedge 0         -- explicit stock runtime clock
    then AUTO gated: edges/pair for the arm(s) that sustained NORMAL.

CONTROL
    postedge 0 / acqbr off must reproduce the EXP-44/45 freeze; every knob
    read back per arm.
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


def normal_arm(sc, label):
    sc.cmd("fpga scope trigmode auto"); time.sleep(0.3)
    sc.cmd("fpga scope trigmode normal"); time.sleep(1.0)
    sc.opread_stats(0x04); time.sleep(1.0)               # one shell prime
    e0, o0 = counters(sc); gens = [grab_frame(sc)["gen"] for _ in range(6)]; e1, o1 = counters(sc)
    adv = np.diff(gens)
    print("NORMAL %-28s gens %s -> %-24s edges +%d OK +%d"
          % (label, gens, "FROZEN" if np.all(adv == 0) else "advancing (adv %d..%d)" % (adv.min(), adv.max()), e1 - e0, o1 - o0))
    return not np.all(adv == 0)


def auto_arm(sc, label):
    sc.cmd("fpga scope trigmode auto"); time.sleep(1.0)
    e0, o0 = counters(sc); time.sleep(6.0); e1, o1 = counters(sc)
    print("AUTO   %-28s 6 s: edges +%d pairs +%d edges/pair %.2f pairs/s %.1f"
          % (label, e1 - e0, o1 - o0, (e1 - e0) / max(o1 - o0, 1), (o1 - o0) / 6.0))


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()
    sc = Scope("/dev/ttyACM0")
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    sc.cmd("fpga scope level 0"); sc.cmd("fpga acqgate off"); sc.cmd("fpga rearm off"); sc.cmd("fpga pairgap 0")
    print("EXP-46")
    sustained = []
    sc.cmd("fpga acqbr off")
    for pe in (0, 5, 20, 100, 300):
        r = sc.cmd("fpga postedge %d" % pe); assert re.search(r"delay %d ms" % pe, r), r
        if normal_arm(sc, "postedge %3d ms, acqbr off" % pe): sustained.append(("postedge", pe))
    sc.cmd("fpga postedge 0")
    for br in (7, 0):
        r = sc.cmd("fpga acqbr %d" % br); assert ("br=%d" % br) in r, r
        if normal_arm(sc, "postedge 0, acqbr %d" % br): sustained.append(("acqbr", br))
    sc.cmd("fpga acqbr off")
    print("\nsustained NORMAL:", sustained or "NONE")
    print()
    auto_arm(sc, "postedge 0, acqbr off (ctl)")
    for kind, v in sustained[:2]:
        sc.cmd("fpga postedge 0"); sc.cmd("fpga acqbr off")
        sc.cmd("fpga %s %d" % (kind, v))
        auto_arm(sc, "%s %d" % (kind, v))
    sc.cmd("fpga postedge 0"); sc.cmd("fpga acqbr off"); sc.cmd("fpga scope trigmode auto")
    sc.close()


if __name__ == "__main__":
    main()
