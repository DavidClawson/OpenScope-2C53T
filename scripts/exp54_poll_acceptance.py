#!/usr/bin/env python3
"""EXP-54 -- the acquisition loop is a poll on the PC0 handover strobe.

PREDICTIONS (written before the run, image = poll loop, `fpga pollgap` 30 ms, poll start fill + 100)
  (a) NORMAL triggers on a 1 Hz square at 0x12 and a 4 Hz square at 0x10 (EXP-53's open defect):
      edges advance and commits == edges (the frozen 0/0 of exp50h is the falsifier).
  (b) EXP-47 acceptance intact: NORMAL at 0x0E/0x10/0x11/0x12 advancing, edges == commits;
      NORMAL holds at level +100 and resumes at 0; SINGLE one-shot 3/3; AUTO edges/pair ~1.
  (c) Records at the body floor at 0x10/0x11/0x12 for 201.2 Hz and 50 Hz (as edge+40 gave in EXP-53).
  (d) status: strobe->commit latency ~0-2 ms; poll reads before a handover small (0-3 at 201 Hz).
  Falsifier for the model itself: commits without strobes in NORMAL, or torn committed records.
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def status(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1)),
            int(re.search(r"acq latency: (\d+) ms", t).group(1)), int(re.search(r"poll reads before it: (\d+)", t).group(1)))
def body_max(v, fs, f):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(128, 1024, 32))
def drive(wave, f, amp=3.0, off=0.0):
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, wave); jds.freq(f, 1); jds.amp(amp, 1); jds.offset(off, 1); jds.output(True, False); jds.close(); time.sleep(0.8)
def watch(sc, n=4, gap=0.5):
    e0, o0, _, _ = status(sc); g = []
    for _ in range(n): g.append(grab_frame(sc)["gen"]); time.sleep(gap)
    e1, o1, lat, polls = status(sc)
    return ("advancing" if not np.all(np.diff(g) == 0) else "FROZEN"), e1 - e0, (o1 - o0)//2, lat, polls
def main():
    rates = load_rates(); sc = Scope("/dev/ttyACM0")
    print("EXP-54  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    for c in ("fpga pollgap", "fpga postedge", "fpga autowait", "fpga scope level", "fpga scope trigmode"):
        print("  " + next((l for l in sc.cmd(c).strip().splitlines() if l.strip() and not l.strip().startswith(">") and c not in l), "?")[:120])
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0")
    print("\n=== (a) NORMAL on a square (EXP-53 open defect) ===")
    for tb, f in ((0x12, 1.0), (0x10, 4.0)):
        drive("1", f, 2.0, 0.5); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        sc.cmd("fpga scope trigmode normal"); time.sleep(2.5)
        st, de, do, lat, polls = watch(sc, 5, 1.0 if tb == 0x12 else 0.4)
        print("  square %.0f Hz tb 0x%02X NORMAL: %-9s edges +%d commits +%d  latency %d ms  polls-before-last %d" % (f, tb, st, de, do, lat, polls))
    print("\n=== (b) EXP-47 lines, sine 201.2 Hz ===")
    drive("0", 201.2)
    for tb in (0x0E, 0x10, 0x11, 0x12):
        sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
        st, de, do, lat, polls = watch(sc)
        print("  tb 0x%02X NORMAL: %-9s edges +%d commits +%d  latency %d ms  polls %d  (fill %.0f ms)" % (tb, st, de, do, lat, polls, 1024/rates[tb]*1000))
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
    for lvl in (0, 100, 0):
        sc.cmd("fpga scope level %d" % lvl); time.sleep(1.5); st, de, do, _, _ = watch(sc)
        print("  level %+4d NORMAL: %-9s edges +%d commits +%d" % (lvl, st, de, do))
    for k in range(3):
        sc.cmd("fpga scope trigmode auto"); time.sleep(0.4); g0 = grab_frame(sc)["gen"]
        sc.cmd("fpga scope trigmode single"); time.sleep(2.5); g = [grab_frame(sc)["gen"] for _ in range(3)]
        print("  SINGLE %d: gen %d -> %s -> %s" % (k + 1, g0, g, "ONE record then held" if (g[0] > g0 and len(set(g)) == 1) else "UNEXPECTED"))
    sc.cmd("fpga scope trigmode auto"); time.sleep(1.0); e0, o0, _, _ = status(sc); time.sleep(10.0); e1, o1, _, _ = status(sc)
    print("  AUTO 10 s: edges +%d commits +%d  edges/commit %.2f" % (e1-e0, (o1-o0)//2, (e1-e0)/max(1,(o1-o0)//2)))
    print("\n=== (c) body floors, AUTO ===")
    for f in (201.2, 50.0):
        drive("0", f)
        for tb in (0x10, 0x11, 0x12):
            sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(1.5 if tb != 0x12 else 2.5)
            mx = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[tb], f) for _ in range(6)]
            _, _, lat, polls = status(sc)
            print("  drive %5.1f Hz tb 0x%02X body %s  median %.0f  latency %d ms polls %d" % (f, tb, " ".join("%3.0f" % x for x in mx), np.median(mx), lat, polls))
    sc.cmd("fpga scope timebase 10", timeout=6.0); sc.close()
if __name__ == "__main__":
    main()
