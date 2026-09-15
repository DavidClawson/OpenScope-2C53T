#!/usr/bin/env python3
"""EXP-50 -- read the held record at edge + fill + 30 ms, arm at the bracket's end.

PREDICTIONS (build with fpga_acq_hold_read_get(), written before the run)
    Readback: `fpga holdread` says derived fill + 30 ms, arming read derived.
    EXP-47's acceptance still holds: NORMAL at 0x0E/0x10/0x11/0x12 advancing
    with edges == commits; NORMAL +100 freezes / 0 resumes; SINGLE one-shot;
    AUTO 1.00 edges/pair, 0/10 gross seams.
    NEW: `status` "acq latency" (edge -> committed held record) ~ fill + 30
    + read: ~55 ms at 0x0E, ~115 at 0x10, ~240 at 0x11, ~445 at 0x12 --
    against fill + 230 + read on the previous image (250/311/435/640).
    Falsifier: latency unchanged, or any EXP-47 line regressing (a held-record
    read at fill + 30 that is NOT static would show as gross seams in AUTO).
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
F_DRIVE = 201.2
def status(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1)),
            int(re.search(r"acq latency: (\d+) ms", t).group(1)))
def body_max(v, fs):
    w = 2*np.pi*F_DRIVE/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(128, 1024, 32))
def watch(sc, n=4):
    e0, o0, _ = status(sc); g = []; lat = []
    for _ in range(n): g.append(grab_frame(sc)["gen"]); lat.append(status(sc)[2])
    e1, o1, _ = status(sc)
    return ("advancing" if not np.all(np.diff(g) == 0) else "FROZEN"), e1 - e0, o1 - o0, lat
def main():
    jds = JDS6600("/dev/ttyUSB0"); jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
    rates = load_rates(); sc = Scope("/dev/ttyACM0")
    print("EXP-50  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    print("=== readback first ===")
    for c in ("fpga holdread", "fpga postedge", "fpga autowait", "fpga scope level", "fpga scope trigmode"):
        print("  " + next((l for l in sc.cmd(c).strip().splitlines() if l.strip() and not l.strip().startswith(">") and c not in l), "?")[:110])
    sc.cmd("fpga scope vdiv 1 5")
    print("\n=== NORMAL, defaults, level 0 ===")
    for tb in (0x0E, 0x10, 0x11, 0x12):
        sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
        st, de, do, lat = watch(sc)
        print("  tb 0x%02X: %-9s edges +%d commits +%d  latency ms %s  (fill %.0f)" % (tb, st, de, do, lat, 1024 / rates[tb] * 1000))
    print("\n=== NORMAL negative control at 0x10 ===")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
    for lvl in (0, 100, 0):
        sc.cmd("fpga scope level %d" % lvl); time.sleep(1.5); st, de, do, _ = watch(sc)
        print("  level %+4d: %-9s edges +%d commits +%d" % (lvl, st, de, do))
    print("\n=== SINGLE ===")
    for k in range(3):
        sc.cmd("fpga scope trigmode auto"); time.sleep(0.4); g0 = grab_frame(sc)["gen"]
        sc.cmd("fpga scope trigmode single"); time.sleep(2.5); g = [grab_frame(sc)["gen"] for _ in range(3)]
        print("  selection %d: gen %d -> %s  -> %s" % (k + 1, g0, g, "ONE record then held" if (g[0] > g0 and len(set(g)) == 1) else "UNEXPECTED"))
    print("\n=== AUTO at 0x10 ===")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0"); time.sleep(1.0)
    e0, o0, _ = status(sc); t0 = time.time(); mx = []; lat = []
    for _ in range(10): mx.append(body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[0x10])); lat.append(status(sc)[2])
    e1, o1, _ = status(sc)
    print("  edges/pair %.2f  body max rms %s  gross %d/10  latency ms %s" % ((e1 - e0) / max(o1 - o0, 1), " ".join("%.1f" % x for x in mx), sum(x > 20 for x in mx), lat))
    e0, o0, _ = status(sc); time.sleep(6.0); e1, o1, _ = status(sc)
    print("  status-only 6 s: edges/s %.2f  pairs/s %.2f  (previous image: ~2.9 edges/s at this drive)" % ((e1 - e0) / 6.0, (o1 - o0) / 6.0))
    sc.close()
if __name__ == "__main__":
    main()
