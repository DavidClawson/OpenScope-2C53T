"""EXP-50f -- trigger anchoring of the late record vs the early buffer, triangle drive, records SAVED.
Analysis per record: seam k = argmax circular |dv| (a triangle is continuous, the seam is the only jump);
un-rotate so r[j] = v[(j+k) % 1024]; brute-force fit the triangle phase on r; report where the level
crossing (rising AND falling candidates) falls in r's timeline, modulo one drive period.
PREDICTIONS (late record): trig ~0 = post-trigger; ~1024 = ends at the trigger (roll copied);
~period-0.189*fs = capture begins 189 ms after the trigger. Early buffer (before edge+189): ~1024 (frozen roll).
"""
import os, sys, time
import numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def tri(ph): ph = np.mod(ph, 1.0); return np.where(ph < 0.5, -1 + 4*ph, 3 - 4*ph)
def analyse(v, fs, f, code):
    dv = np.abs(np.diff(np.r_[v, v[0]])); k = int(np.argmax(dv)) + 1; jump = dv[k-1]
    if jump <= 2: k = 0
    r = np.r_[v[k:], v[:k]]; n = np.arange(1024); best = (1e18, 0, None)
    for ph0 in np.arange(0, 1, 1/2048):
        A = np.c_[tri(ph0 + f*n/fs), np.ones(1024)]; c, *_ = np.linalg.lstsq(A, r, rcond=None); s = np.sum((r - A@c)**2)
        if s < best[0]: best = (s, ph0, c)
    s, ph0, (amp, off) = best; rms = np.sqrt(s/1024)
    if amp < 0: amp, ph0 = -amp, np.mod(ph0 + 0.5, 1.0)
    u = np.clip((code - off)/(amp if abs(amp) > 1e-6 else 1e-6), -1, 1)
    p_r = (u + 1)/4; p_f = (3 - u)/4                      # rising / falling crossing phases
    ti_r = np.mod(p_r - ph0, 1.0) * fs / f; ti_f = np.mod(p_f - ph0, 1.0) * fs / f
    return k, jump, rms, ti_r, ti_f, amp, off
rates = load_rates(); sc = Scope("/dev/ttyACM0"); saved = {}
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga postedge 0")
for tb, F in ((0x12, 2.0), (0x10, 10.0)):
    sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); fs = rates[tb]; fill = 1024/fs*1000; per = fs/F
    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "3"); jds.freq(F, 1); jds.amp(2.0, 1); jds.offset(0.5, 1); jds.output(True, False); jds.close()
    sc.cmd("fpga holdread %d" % int(fill + 330)); time.sleep(3.0); v0 = np.asarray(grab_frame(sc)["ch1"], float); code = int((np.percentile(v0, 2) + np.percentile(v0, 98))/2); level = int(round((code - 128)*206/256)); sc.cmd("fpga scope level %d" % level); time.sleep(1.0); print("  span %.0f..%.0f" % (v0.min(), v0.max()))
    print("tb 0x%02X tri %.0f Hz fs %.0f  level %d (code %d)  period %.0f samples; predictions: post=0  roll-end=1024  189ms-late=%.0f" % (tb, F, fs, level, code, per, np.mod(-0.189*fs, per)))
    for label, hr in (("LATE  edge+fill+330", int(fill + 330)), ("LATE  edge+fill+230", int(fill + 230)), ("EARLY edge+100", 100)):
        sc.cmd("fpga holdread %d" % hr); time.sleep(3.0 if tb == 0x12 else 2.0)
        for i in range(6):
            v = np.asarray(grab_frame(sc)["ch1"], float); saved["tb%02x_%s_%d" % (tb, label.split()[0], i)] = v
            k, jump, rms, ti_r, ti_f, amp, off = analyse(v, fs, F, code)
            print("  %-20s seam k %4d (jump %3.0f)  fit rms %4.1f amp %5.1f off %5.1f   trig index: rising %6.0f  falling %6.0f" % (label, k, jump, rms, amp, off, ti_r, ti_f))
    sc.cmd("fpga holdread 0")
np.savez("/tmp/claude-1000/-home-david-osc/7295c197-e28f-4e77-9e4f-ae5fa413ac94/scratchpad/exp50g_records.npz", **saved)
sc.cmd("fpga scope level 0"); sc.cmd("fpga holdread 0"); sc.cmd("fpga scope timebase 10", timeout=6.0)
jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(201.2, 1); jds.close(); sc.close(); print("done")
