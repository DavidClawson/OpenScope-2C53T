#!/usr/bin/env python3
"""EXP-49 part D -- fast polling at the task's clock: what fraction of records is the held (static) record?

PREDICTION (read returns the record held at the read; a read after the bracket
arms the next capture; only reads landing between arming and trigger+fill see
the rolling buffer). AUTO, autowait 25, postedge 1, 2 kHz drive:
    0x10 (fill 82 ms):  static fraction ~ 1 - (5+82)/(82+190+25) ~ 0.71
    0x0D (fill  8 ms):  static fraction ~ 1 - (5+8)/(8+190+25)   ~ 0.94
Falsifier: static fraction near 0 at both => in-bracket reads return the live buffer.
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates

F = 2000.0
def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1)))
def body_max(v, fs, f):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(128, 1024, 32))
jds = JDS6600("/dev/ttyUSB0"); jds.output(False, False); jds.waveform("sine", 1); jds.freq(F, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
rates = load_rates(); sc = Scope("/dev/ttyACM0")
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga postedge 1"); sc.cmd("fpga autowait 25")
for tb in (0x10, 0x0D):
    sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(1.0)
    e0, o0 = counters(sc); t0 = time.time(); mx = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[tb], F) for _ in range(20)]; e1, o1 = counters(sc); dt = time.time() - t0
    print("tb 0x%02X: pairs/s %.1f edges/pair %.2f  static (body<20) %d/20   body: %s" % (tb, (o1-o0)/dt, (e1-e0)/max(o1-o0,1), sum(x < 20 for x in mx), " ".join("%.0f" % x for x in mx)))
print("--- control: derived defaults at 0x10 (all static expected)")
sc.cmd("fpga postedge 0"); sc.cmd("fpga autowait 0"); sc.cmd("fpga scope timebase 10", timeout=6.0); time.sleep(1.0)
mx = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[0x10], F) for _ in range(8)]
print("derived: static %d/8   body: %s" % (sum(x < 20 for x in mx), " ".join("%.0f" % x for x in mx)))
sc.close()
