#!/usr/bin/env python3
"""EXP-52 -- un-rotate the record by L = (PC0 edge - arming read) x fs: is index 0 the trigger?

RETIRED 2026-09-22 before it ever ran (EXP-53 postscript / EXP-54): PC0 strobes 6-9 us after
the read, so "arming read -> edge" is not a latency and the pointer is set by the FPGA's own
cycle. Kept for the record; `fpga holdread` is retired and `fpga unrotate` reports only.

PREDICTIONS (written before the run; build with `fpga unrotate`, default ON)
    With un-rotation ON the whole record (0..1023) fits the drive sine at the
    body floor: residual seam index ("head") ~ a small CONSTANT (the pointer
    start offset, same at every drive frequency at one rate) instead of
    0..one period; the trigger crossing (rising through the level) sits at
    index 0 + that constant. `status` "acq rotation" == the seam index the
    OFF control measures on the same cycle geometry.
    OFF control reproduces EXP-51: head spans 0..one drive period.
    Falsifier: ON leaves the seam spread as wide as OFF (timing model wrong),
    or the residual constant differs between drives (pointer start is not
    tied to the arming read).
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates

def seam_index(v, fs, f, cap=512):
    """Largest index < cap where the record leaves the sine fitted on cap..1023 (0 = none)."""
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(cap, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    floor = np.sqrt(np.mean(r[idx]**2)); bad = np.abs(r) > 4*max(floor, 1.5)
    for i in range(cap-1, -1, -1):
        if bad[i] and bad[max(i-1,0):i+3].sum() >= 3: return i + 1, floor
    return 0, floor

def whole_max(v, fs, f):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    c, *_ = np.linalg.lstsq(A, v, rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(0, 1024, 32))

def rot(sc):
    t = sc.cmd("status", timeout=6.0); m = re.search(r"acq rotation: (-?\d+) samples \(arm -> edge (\d+) us\)", t)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)

def drive(f):
    jds = JDS6600("/dev/ttyUSB0"); jds.waveform("sine", 1); jds.freq(f, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close(); time.sleep(1.0)

def main():
    rates = load_rates(); sc = Scope("/dev/ttyACM0")
    print("EXP-52  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    print("  " + next((l for l in sc.cmd("fpga unrotate").splitlines() if "unrotate" in l and not l.startswith(">")), "?").strip())
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0")
    for tb in (0x10, 0x0E, 0x12):
        sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); fs = rates[tb]
        for f in ((20.0, 201.2, 1000.0) if tb != 0x12 else (20.0, 201.2)):
            drive(f); time.sleep(1.0)
            for state in ("off", "on"):
                sc.cmd("fpga unrotate %s" % state); time.sleep(1.5)
                seams = []; wm = []; rots = []; lats = []
                for _ in range(6):
                    v = np.asarray(grab_frame(sc)["ch1"], float); s_, fl = seam_index(v, fs, f); seams.append(s_); wm.append(whole_max(v, fs, f))
                    r, us = rot(sc); rots.append(r); lats.append(us)
                print("  tb 0x%02X fs %6.0f drive %6.1f Hz unrotate %-3s seam %-28s whole-record max %-28s rot %s  lat us %s" % (
                    tb, fs, f, state.upper(), seams, " ".join("%.0f" % x for x in wm), rots, lats))
    sc.cmd("fpga unrotate on"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.close()

if __name__ == "__main__":
    main()
