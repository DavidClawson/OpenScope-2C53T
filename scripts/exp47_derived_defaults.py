#!/usr/bin/env python3
"""EXP-47 -- acceptance of the derived post-edge default: fresh boot, nothing typed.

PREDICTIONS (build with fpga_acq_post_edge_get() deriving fill + 230 ms)
    Readback first: `fpga postedge` says "derived"; `fpga autowait` derived.
    NORMAL at level 0 sustains at 0x0E, 0x10, 0x11, 0x12 with NO override
    and no shell prime beyond the mode's own priming read: generation
    advances, edges == commits.
    NORMAL at level +100 freezes (negative control), resumes at level 0.
    SINGLE: exactly one committed record per selection.
    AUTO at 0x10: edges/pair 1.00 and body metric 0/10 gross seams.
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


def watch(sc, n=4):
    e0, o0 = counters(sc); g = [grab_frame(sc)["gen"] for _ in range(n)]; e1, o1 = counters(sc)
    adv = np.diff(g)
    return ("advancing" if not np.all(adv == 0) else "FROZEN"), e1 - e0, o1 - o0, g


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()
    rates = load_rates()
    sc = Scope("/dev/ttyACM0")
    print("=== readback first ===")
    for c in ("fpga postedge", "fpga autowait", "fpga scope level", "fpga scope trigmode", "fpga pairgap", "fpga acqbr"):
        print("  " + next((l for l in sc.cmd(c).strip().splitlines() if l.strip() and not l.strip().startswith(">") and c not in l), "?")[:100])
    sc.cmd("fpga scope vdiv 1 5")

    print("\n=== NORMAL, defaults, level 0, no shell prime ===")
    for tb in (0x0E, 0x10, 0x11, 0x12):
        sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        pe = next((l for l in sc.cmd("fpga postedge").strip().splitlines() if "delay" in l), "?")
        sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
        st, de, do, g = watch(sc)
        print("  tb 0x%02X: %-9s edges +%d commits +%d  gens %s   [%s]" % (tb, st, de, do, g, pe[:44]))

    print("\n=== NORMAL negative control at 0x10 ===")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
    for lvl in (0, 100, 0):
        sc.cmd("fpga scope level %d" % lvl); time.sleep(1.5)
        st, de, do, g = watch(sc)
        print("  level %+4d: %-9s edges +%d commits +%d" % (lvl, st, de, do))

    print("\n=== SINGLE ===")
    for k in range(3):
        sc.cmd("fpga scope trigmode auto"); time.sleep(0.4); g_before = grab_frame(sc)["gen"]
        sc.cmd("fpga scope trigmode single"); time.sleep(2.5)
        g = [grab_frame(sc)["gen"] for _ in range(3)]
        print("  selection %d: gen before %d -> after %s  -> %s" % (k + 1, g_before, g,
              "ONE record then held" if (g[0] > g_before and len(set(g)) == 1) else "UNEXPECTED"))

    print("\n=== AUTO at 0x10, defaults ===")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0"); time.sleep(1.0)
    fs = rates[0x10]; e0, o0 = counters(sc); mx = []
    for _ in range(10):
        mx.append(body_max(np.asarray(grab_frame(sc)["ch1"], float), fs))
    e1, o1 = counters(sc)
    print("  edges/pair %.2f   body max rms %s   gross %d/10" % ((e1 - e0) / max(o1 - o0, 1), " ".join("%.1f" % x for x in mx), sum(x > 20 for x in mx)))
    sc.close()


if __name__ == "__main__":
    main()
