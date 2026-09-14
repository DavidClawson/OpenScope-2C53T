#!/usr/bin/env python3
"""EXP-44 -- NORMAL and SINGLE trigger modes, now that a read starts the capture.

QUESTION
    EXP-43 accepted the AUTO defaults but NORMAL froze at level 0 with zero
    edges and zero reads: it waited for an edge before its first read, and a
    read is what starts a capture (EXP-41). The acq task now PRIMES -- one
    staging-only read when nothing is in flight -- and SINGLE holds after
    one triggered record until the mode is re-selected. Do the modes behave?

PREDICTIONS
    NORMAL, level 0 (inside the signal): generation advances, ~1 PC0 edge per
    record; the trace is a completed capture (body seam metric at the floor).
    NORMAL, level +100 (code 0xFC, above the signal): generation FREEZES,
    edges 0 -- the negative control, showing the mode waits on the edge.
    NORMAL, level 0 again: resumes without any other intervention.
    SINGLE, level 0: exactly ONE generation advance after selection, then
    frozen; re-selecting SINGLE gives exactly one more.
"""
import os
import re
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600                         # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402

F_DRIVE = 201.2


def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def body_max(v, fs):
    w = 2 * np.pi * F_DRIVE / fs
    n = np.arange(len(v)); A = np.c_[np.sin(w * n), np.cos(w * n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i + 32] ** 2)) for i in range(128, 1024, 32))


def watch(sc, label, n=5, fs=None):
    e0, o0 = counters(sc); gens, mx = [], []
    for _ in range(n):
        fr = grab_frame(sc); gens.append(fr["gen"])
        if fs: mx.append(body_max(np.asarray(fr["ch1"], float), fs))
    e1, o1 = counters(sc); adv = np.diff(gens)
    state = "FROZEN" if np.all(adv == 0) else "advancing (adv %d..%d)" % (adv.min(), adv.max())
    extra = ("  body max rms %s" % " ".join("%.1f" % x for x in mx)) if mx else ""
    print("   %-26s gens %s -> %s  edges +%d OK +%d%s" % (label, gens, state, e1 - e0, o1 - o0, extra))
    return gens


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()
    fs = load_rates()[0x10]
    sc = Scope("/dev/ttyACM0")
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    print(sc.cmd("fpga scope level 0").strip().splitlines()[1])

    print("=== NORMAL ===")
    sc.cmd("fpga scope trigmode normal"); time.sleep(1.0)
    watch(sc, "level 0", fs=fs)
    sc.cmd("fpga scope level 100"); time.sleep(1.5)
    watch(sc, "level +100 (above signal)")
    sc.cmd("fpga scope level 0"); time.sleep(1.5)
    watch(sc, "level 0 again", fs=fs)

    print("=== SINGLE ===")
    sc.cmd("fpga scope trigmode auto"); time.sleep(0.5)
    sc.cmd("fpga scope trigmode single"); time.sleep(1.5)
    g = watch(sc, "after selecting SINGLE")
    print("   -> distinct generations seen: %d (predict 1, then held)" % len(set(g)))
    sc.cmd("fpga scope trigmode auto"); time.sleep(0.3); sc.cmd("fpga scope trigmode single"); time.sleep(1.5)
    g2 = watch(sc, "SINGLE re-selected")
    print("   -> new generation vs before: %s" % ("YES (+%d)" % (g2[-1] - g[-1]) if g2[-1] != g[-1] else "no"))

    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0")
    print("restored: AUTO, level 0")
    sc.close()


if __name__ == "__main__":
    main()
