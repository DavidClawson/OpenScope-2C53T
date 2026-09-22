"""EXP-50h -- where does the trigger sit in the RAW record?  10 %-duty pulse, one rising edge per period.
NORMAL mode (no fallback reads).  Report the raw index of the rising and falling edges in every record.
PREDICTIONS: late record (edge+189+fill on): rising edge at a CONSTANT raw index -> trigger-anchored
  (~0 = starts at the trigger; ~960 = ends 64 after it; ~1024-L varying = rotation by latency).
  early buffer (edge+100): frozen roll -> rising edge index varies with the arm->edge latency.
"""
import os, sys, time, re
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def edges(v):
    mid = (np.percentile(v, 5) + np.percentile(v, 95)) / 2
    hi = v >= mid; r = np.flatnonzero(~hi[:-1] & hi[1:]) + 1; f = np.flatnonzero(hi[:-1] & ~hi[1:]) + 1
    return r.tolist(), f.tolist(), float(hi.mean())
rates = load_rates(); sc = Scope("/dev/ttyACM0"); saved = {}
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga postedge 0"); sc.cmd("fpga scope trigmode auto")
for tb, F in ((0x12, 1.0), (0x10, 4.0)):
    sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); fs = rates[tb]; fill = 1024/fs*1000
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "1"); jds.write_raw(29, "100"); jds.freq(F, 1); jds.amp(2.0, 1); jds.offset(0.5, 1); jds.output(True, False); jds.close()
    sc.cmd("fpga scope level -20"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread %d" % int(fill + 330)); time.sleep(3.0)
    v0 = np.asarray(grab_frame(sc)["ch1"], float); r, f, frac = edges(v0)
    print("tb 0x%02X pulse %.0f Hz fs %.0f fill %.0f ms: span %.0f..%.0f high-fraction %.2f (expect ~%.2f if duty=10%%)  level readback: %s" % (
        tb, F, fs, fill, v0.min(), v0.max(), frac, min(1.0, 0.1/F*fs/1024) if 0.1/F*fs < 1024 else 1.0, sc.cmd("fpga scope level").splitlines()[1][:60]))
    sc.cmd("fpga scope trigmode normal"); time.sleep(1.0)
    for label, hr in (("LATE  edge+fill+330", int(fill + 330)), ("LATE  edge+fill+230", int(fill + 230)), ("EARLY edge+100", 100)):
        sc.cmd("fpga holdread %d" % hr); time.sleep(3.5 if tb == 0x12 else 2.0)
        t0 = sc.cmd("status", timeout=6.0); e0 = int(re.search(r"PC0 edges: (\d+)", t0).group(1)); o0 = int(re.search(r"SPI3 OK: (\d+)", t0).group(1))
        rows = []
        for i in range(8):
            v = np.asarray(grab_frame(sc)["ch1"], float); saved["tb%02x_%s_%d" % (tb, label.split()[0], i)] = v; r, f, frac = edges(v); rows.append((r, f))
            time.sleep(0.8 if tb == 0x12 else 0.3)
        t1 = sc.cmd("status", timeout=6.0); e1 = int(re.search(r"PC0 edges: (\d+)", t1).group(1)); o1 = int(re.search(r"SPI3 OK: (\d+)", t1).group(1))
        print("  %-20s edges +%d commits +%d   rising-edge raw index per record: %s   falling: %s" % (label, e1-e0, (o1-o0)//2, [r[0] if r else None for r, f in rows], [f[0] if f else None for r, f in rows]))
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread 0")
np.savez("/tmp/claude-1000/-home-david-osc/7295c197-e28f-4e77-9e4f-ae5fa413ac94/scratchpad/exp50h_records.npz", **saved)
sc.cmd("fpga scope level 0"); sc.cmd("fpga holdread 0"); sc.cmd("fpga scope timebase 10", timeout=6.0)
jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.write_raw(29, "500"); jds.freq(201.2, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.close(); sc.close(); print("done")
