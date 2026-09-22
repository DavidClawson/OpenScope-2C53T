"""EXP-50c -- hold-read margin sweep at the slow codes (0x11, 0x12) and 0x10 as control.
PREDICTION (written first): if the record is complete at edge + fill, body max is at the
per-code floor (6 at 0x10, ~11 at 0x11, ~22 at 0x12, 201.2 Hz) at EVERY margin >= 30 ms and
the +230 control is no better. If it climbs at small margins and settles by some M*, the fill
is longer than 1024/fs at that code (rate table or a post-trigger tail) and the derived
margin must become >= M*. Metric: max 32-block residual RMS vs the drive fit on 128..1023,
reported RAW (no threshold) beside the +230 floor from the same run.
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def body_max(v, fs, f):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(128, 1024, 32))
rates = load_rates(); sc = Scope("/dev/ttyACM0")
print("EXP-50c device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0")
N = 8
for F in (201.2, 50.0):
    jds = JDS6600("/dev/ttyUSB0"); jds.waveform("sine", 1); jds.freq(F, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
    for tb in (0x12, 0x11, 0x10):
        sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        fill = 1024 / rates[tb] * 1000
        # control first: the arming-read path (single read at fill+230)
        sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge %d" % int(fill + 230)); time.sleep(1.5)
        ctl = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[tb], F) for _ in range(N)]
        print("drive %5.1f Hz tb 0x%02X fill %4.0f ms  CONTROL single@+230  body %s  median %.0f" % (F, tb, fill, " ".join("%3.0f" % x for x in ctl), np.median(ctl)))
        sc.cmd("fpga postedge 0")
        for m in (30, 60, 100, 150, 230, 330):
            sc.cmd("fpga holdread %d" % int(fill + m)); time.sleep(1.5)
            mx = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[tb], F) for _ in range(N)]
            print("drive %5.1f Hz tb 0x%02X fill %4.0f ms  hold fill+%-4d       body %s  median %.0f  >2x ctl: %d/%d" % (F, tb, fill, m, " ".join("%3.0f" % x for x in mx), np.median(mx), sum(x > 2*np.median(ctl) for x in mx), N))
        sc.cmd("fpga holdread 0")
sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.close()
print("done")
