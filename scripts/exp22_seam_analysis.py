#!/usr/bin/env python3
"""Post-hoc seam hunt on EXP-22 saved frames.

Hypothesis: the acq buffer is a ring snapshot with the write-pointer seam at a
varying index. Test: local phase of the fundamental, measured in overlapping
short blocks, must advance LINEARLY with block index for a contiguous record;
a seam appears as a step discontinuity. Report, per frame: the max phase step
between adjacent blocks (in samples), its location, and whether the frame's
lock residual correlates with a seam inside the draw window [off, off+320].
"""
import sys
import math
import numpy as np

BLK = 64
HOP = 32
DRAW_N = 320


def dft_at(x, f, fs):
    x = np.asarray(x, dtype=float)
    x = x - x.mean()
    n = np.arange(len(x))
    return np.sum(x * np.exp(-2j * np.pi * f * n / fs))


def block_phases(x, f, fs):
    """(centers, phases) with the per-block expected advance removed, so a
    contiguous record gives a CONSTANT sequence and a seam gives a step."""
    cs, ps = [], []
    for s in range(0, len(x) - BLK + 1, HOP):
        z = dft_at(x[s:s + BLK], f, fs)
        # rotate back by the phase the wave should advance in s samples
        ps.append(np.angle(z * np.exp(-2j * np.pi * f * s / fs)))
        cs.append(s + BLK // 2)
    return np.array(cs), np.unwrap(np.array(ps))


def frame_seam(x, f, fs):
    cs, ps = block_phases(x, f, fs)
    steps = np.abs(np.diff(ps))
    k = int(np.argmax(steps))
    # phase step -> samples of time discontinuity
    return float(steps[k] / (2 * math.pi * f) * fs), int(cs[k]), cs, ps


def main(path, scen_key, f, fs):
    d = np.load(path)
    ch1 = d[scen_key + "__ch1"]
    off = d[scen_key + "__off"]

    print("== %s  (f=%g fs=%g) ==" % (scen_key, f, fs))
    # lock residuals vs frame 0, same math as exp22
    ph0 = None
    for i in range(len(ch1)):
        w = ch1[i][off[i]: off[i] + DRAW_N]
        p = np.angle(dft_at(w, f, fs))
        if ph0 is None:
            ph0, resid = p, 0.0
        else:
            resid = abs(math.atan2(math.sin(p - ph0), math.cos(p - ph0))
                        / (2 * math.pi * f) * fs)
        step, loc, cs, ps = frame_seam(ch1[i], f, fs)
        in_win = off[i] <= loc <= off[i] + DRAW_N
        print("  fr%-2d off=%-4d resid=%6.2f  max-step=%6.2f smp @ idx %-4d "
              "%s  phase-spread=%.2f rad"
              % (i, off[i], resid, step, loc,
                 "IN-WINDOW" if in_win else "         ",
                 float(ps.max() - ps.min())))


if __name__ == "__main__":
    path = sys.argv[1]
    d = np.load(path)
    keys = sorted({k.rsplit("__", 1)[0] for k in d.files})
    print("scenarios:", keys)
    freqs = {"baseline_500Hz_2Vpp": (500, 12490.0),
             "freq_200Hz": (200, 12490.0),
             "freq_1kHz": (1000, 12490.0),
             "amp_1Vpp": (500, 12490.0),
             "amp_4Vpp": (500, 12490.0),
             "phase_45": (500, 12490.0),
             "phase_90": (500, 12490.0),
             "phase_180": (500, 12490.0),
             "NEGCTL_free-run": (500, 12490.0),
             "tb_0x0E_2kHz": (2000, 49930.1)}
    for k in keys:
        if k in freqs:
            f, fs = freqs[k]
            main(path, k, f, fs)
