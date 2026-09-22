"""EXP-50d -- where is the tear, and what is the real rate at 0x11/0x12?
For each margin m: grab records, locate the best two-segment split k (sine fitted separately
on [0,k) and [k,1024)), report k and the phase jump across it. Also measure fs from the peak
bin of the clean (+330) records with a 201.2 Hz drive.
PREDICTION: if the table rate is too high (fill longer than computed) the tear index k grows
linearly with m at the TRUE fs and the spectral fs at +330 comes out below the table.
If instead the buffer rolls inside the bracket, k also grows with m but spectral fs matches
the table and the phase jump equals 2*pi*f*1024/fs_true (one buffer-length old data).
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def fit(v, fs, f, idx):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v[idx] - A[idx] @ c
    return c, np.sum(r**2)
def tear(v, fs, f):
    best = (1e18, 0, 0.0)
    for k in range(64, 1024-64, 4):
        c1, s1 = fit(v, fs, f, np.arange(0, k)); c2, s2 = fit(v, fs, f, np.arange(k, 1024))
        if s1 + s2 < best[0]:
            ph = (np.degrees(np.arctan2(c1[1], c1[0]) - np.arctan2(c2[1], c2[0])) + 180) % 360 - 180
            best = (s1 + s2, k, ph)
    c0, s0 = fit(v, fs, f, np.arange(1024))
    return best[1], best[2], np.sqrt(s0/1024), np.sqrt(best[0]/1024)
def peak_fs(v, f):
    x = v - v.mean(); X = np.abs(np.fft.rfft(x * np.hanning(1024))); b = np.argmax(X[2:]) + 2
    # parabolic interpolation
    a, bb, c = np.log(X[b-1]+1e-9), np.log(X[b]+1e-9), np.log(X[b+1]+1e-9); d = 0.5*(a-c)/(a-2*bb+c)
    return f * 1024 / (b + d)
rates = load_rates(); sc = Scope("/dev/ttyACM0")
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0")
for tb in (0x12, 0x11):
    sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5); fs = rates[tb]; fill = 1024 / fs * 1000
    # rate first, from clean records at +330, 201.2 Hz
    jds = JDS6600("/dev/ttyUSB0"); jds.waveform("sine", 1); jds.freq(201.2, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
    sc.cmd("fpga holdread %d" % int(fill + 330)); time.sleep(2.5)
    est = [peak_fs(np.asarray(grab_frame(sc)["ch1"], float), 201.2) for _ in range(6)]
    print("tb 0x%02X table fs %.0f  spectral fs from +330 records: %s  median %.0f" % (tb, fs, " ".join("%.0f" % e for e in est), np.median(est)))
    for F in (50.0,):
        jds = JDS6600("/dev/ttyUSB0"); jds.freq(F, 1); jds.close(); time.sleep(1.0)
        for m in (30, 60, 100, 150, 190, 210, 230, 330):
            sc.cmd("fpga holdread %d" % int(fill + m)); time.sleep(2.5)
            rows = []
            for _ in range(5):
                v = np.asarray(grab_frame(sc)["ch1"], float); k, ph, r0, r2 = tear(v, fs, F); rows.append((k, ph, r0, r2))
            print("tb 0x%02X drive %4.0f Hz hold fill+%-4d  tear k %s  phase-jump deg %s  1-seg rms %s  2-seg rms %s" % (
                tb, F, m, [r[0] for r in rows], [int(r[1]) for r in rows], " ".join("%.0f" % r[2] for r in rows), " ".join("%.0f" % r[3] for r in rows)))
    sc.cmd("fpga holdread 0")
sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.close(); print("done")
