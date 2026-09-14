#!/usr/bin/env python3
"""EXP-42 -- does a timebase-aware AUTO edge-wait close the acquisition seam?

QUESTION
    EXP-41 established the data-ready model: a READ starts a capture, the
    capture completes when the ADC crosses the reg-0x08 trigger level, and
    completion pulses PC0 once. Nothing else pulses it (idle 0, reg-01 write
    0). The AUTO path waits 25 ms for that edge and then reads anyway; at
    timebase 0x10 the capture takes 82 ms, so the fallback read always lands
    mid-capture -- the rotating-buffer seam EXP-29 measured. Does raising the
    wait past the capture time make every record a completed, static capture?

HYPOTHESIS AND FALSIFIER
    With the trigger level inside the signal and `fpga autowait` set above
    the capture time, records are read only after PC0 (or, rarely, on
    fallback), so the max phase step falls to the noise floor (~0.3 sample,
    the value EXP-30's frozen arm showed for a static buffer) and PC0 edges
    advance ~1 per record. With the compiled 25 ms the seam stays ~20 samples.
    If the seam stays with the long wait, the completed capture is not static
    either and the model is wrong.

ARMS  A1  autowait 25 ms (compiled default, must SHOW seams)
      B   autowait ceil(1.6 * 1024/fs * 1000) ms
      A2  autowait 25 ms (drift)
    gate OFF, re-arm OFF, level 0x80 in every arm (set once, readback by the
    edge counter moving in arm A1).

CONTROLS
  1. A1/A2 show seams.
  2. Generation counter advances in every arm (frozen = VOID).
  3. `fpga autowait` read back per arm; PC0 edge delta recorded per arm.
"""
import argparse
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600, BenchError            # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402
from exp22_seam_analysis import frame_seam               # noqa: E402

F_DRIVE = 201.2
LEVEL = 0x80


def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def autowait(sc, ms=None):
    txt = sc.cmd("fpga autowait" + ("" if ms is None else " %d" % ms), timeout=4.0)
    m = re.search(r"edge-wait (\d+) ms", txt)
    if not m:
        raise BenchError("cannot read autowait:\n" + txt)
    return int(m.group(1))


def run_arm(sc, wait_ms, n, fs):
    got = autowait(sc, wait_ms)
    if got != wait_ms:
        raise BenchError("autowait did not take: asked %d, device says %d" % (wait_ms, got))
    e0, o0 = counters(sc)
    steps, locs, gens, incoh = [], [], [], 0
    for _ in range(n):
        fr = grab_frame(sc)
        if fr["coherent"] != 1:
            incoh += 1
        step, loc, _, _ = frame_seam(fr["ch1"], F_DRIVE, fs)
        steps.append(step); locs.append(loc); gens.append(fr["gen"])
    e1, o1 = counters(sc)
    return np.array(steps), np.array(locs), np.array(gens), incoh, e1 - e0, o1 - o0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tb", type=lambda x: int(x, 0), default=0x10)
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--scope", default="/dev/ttyACM0")
    ap.add_argument("--jds", default="/dev/ttyUSB0")
    a = ap.parse_args()

    fs = load_rates()[a.tb]
    long_ms = int(math.ceil(1.6 * 1024.0 / fs * 1000.0))
    arms = [("A1", 25), ("B", long_ms), ("A2", 25)]

    jds = JDS6600(a.jds)
    jds.output(False, False)
    jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False)
    jds.close()

    sc = Scope(a.scope)
    print("EXP-42  seam vs AUTO edge-wait budget")
    print("drive %.1f Hz, timebase 0x%02X = %.1f S/s, capture %.0f ms, long wait %d ms, %d records/arm"
          % (F_DRIVE, a.tb, fs, 1024.0 / fs * 1000.0, long_ms, a.n))
    sc.cmd("fpga scope timebase %02x" % a.tb, timeout=6.0)
    sc.cmd("fpga acqgate off", timeout=4.0)
    sc.cmd("fpga rearm off", timeout=4.0)
    sc.seq(0x08, LEVEL)
    print("trigger level reg08 <- 0x%02X, gate off, re-arm off\n" % LEVEL)
    out, void = {}, False
    for name, w in arms:
        steps, locs, gens, incoh, de, do = run_arm(sc, w, a.n, fs)
        adv = np.diff(gens); frozen = bool(np.all(adv == 0)); void |= frozen
        out[name] = steps
        print("-- arm %s  autowait=%d ms (readback-confirmed) --" % (name, w))
        print("   max-step samples: " + " ".join("%.1f" % v for v in steps))
        print("   median %.2f  p90 %.2f  max %.2f  incoherent %d/%d"
              % (np.median(steps), np.percentile(steps, 90), steps.max(), incoh, a.n))
        print("   seam locations:   " + " ".join("%d" % l for l in locs))
        print("   gen advance/record: min %d max %d  %s   PC0 edges +%d  SPI3 OK +%d\n"
              % (adv.min(), adv.max(), "*** FROZEN -- VOID ***" if frozen else "live", de, do))
    autowait(sc, 0)
    sc.close()
    a_ = np.concatenate([out["A1"], out["A2"]])
    print("== result ==")
    print("   autowait 25 ms  (n=%d): median %.2f  p90 %.2f" % (len(a_), np.median(a_), np.percentile(a_, 90)))
    print("   autowait %3d ms (n=%d): median %.2f  p90 %.2f" % (long_ms, a.n, np.median(out["B"]), np.percentile(out["B"], 90)))
    print("   A1/A2 medians %.2f / %.2f (drift)" % (np.median(out["A1"]), np.median(out["A2"])))
    if void:
        print("   *** an arm was FROZEN -> VOID ***")


if __name__ == "__main__":
    main()
