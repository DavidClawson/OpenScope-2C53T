#!/usr/bin/env python3
"""EXP-43 -- acceptance of the trigger defaults: a fresh boot, nothing typed.

QUESTION
    EXP-41/42 found the interlock and its two missing pieces: reg 0x08 must
    sit inside the signal (the boot burst wrote stock's 0xAD, above every
    bench signal) and the AUTO edge-wait must exceed the capture cycle. Both
    are now defaults: fpga_reconcile_trigger_after_arm() writes the UI level
    (0 -> 0x80) after the arm, and fpga_acq_auto_wait_get() derives the
    budget from the timebase in force. Do the defaults do the job on a boot
    with no shell writes, and does NORMAL mode now capture?

HYPOTHESIS AND FALSIFIER
    Fresh boot: `fpga scope level` reports reconcile code 0x80 and code in
    force 0x80; `fpga autowait` reports "derived"; PC0 edges ADVANCE with
    nothing set. After `fpga scope timebase 10` the derived budget is ~328 ms
    and the body metric shows 0 gross seams in 10 (EXP-42's result, now by
    default). NORMAL mode with level 0 keeps capturing (gen advances, edges
    ~1 per record); NORMAL with level +100 (code 252, above the signal)
    FREEZES (gen constant) -- that freeze is the negative control that shows
    the mode is really waiting on the edge. If the fresh boot reports 0xAD,
    or edges do not advance untouched, the reconcile did not run.

PROCEDURE
    JDS6600 201.2 Hz 3 Vpp -> CH1, range 5 (set by the script -- relays only,
    not trigger state). Everything else is read before it is written.
"""
import os
import re
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600                         # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402

F_DRIVE = 201.2


def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def body_blocks(v, fs):
    w = 2 * np.pi * F_DRIVE / fs
    n = np.arange(len(v)); A = np.c_[np.sin(w * n), np.cos(w * n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    b = np.array([np.sqrt(np.mean(r[i:i + 32] ** 2)) for i in range(0, 1024, 32)])
    return b[4:].max(), np.median(b[4:])


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()

    sc = Scope("/dev/ttyACM0")
    print("=== EXP-43: fresh-boot readback, nothing written yet ===")
    print(sc.cmd("fpga scope level").strip().splitlines()[1])
    print(sc.cmd("fpga autowait").strip().splitlines()[1])
    print(sc.cmd("fpga scope trigmode").strip().splitlines()[1])
    e0, o0 = counters(sc); time.sleep(3.0); e1, o1 = counters(sc)
    print("PC0 edges in 3 s, untouched boot: +%d  (SPI3 OK +%d)  -> %s" % (e1 - e0, o1 - o0, "EDGES ARRIVE" if e1 > e0 else "NO EDGES"))

    print("\n=== timebase 0x10 (relays to range 5), derived budget ===")
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    print(sc.cmd("fpga autowait").strip().splitlines()[1])
    fs = load_rates()[0x10]
    e0, o0 = counters(sc)
    mx, gens = [], []
    for _ in range(10):
        fr = grab_frame(sc); m, _ = body_blocks(np.asarray(fr["ch1"], float), fs); mx.append(m); gens.append(fr["gen"])
    e1, o1 = counters(sc)
    print("AUTO, defaults: body max rms per record: " + " ".join("%4.1f" % x for x in mx))
    print("   gross seams %d/10   gen adv %d..%d   edges/OK %.2f"
          % (sum(x > 20 for x in mx), np.diff(gens).min(), np.diff(gens).max(), (e1 - e0) / max(o1 - o0, 1)))

    print("\n=== NORMAL mode ===")
    print(sc.cmd("fpga scope trigmode normal").strip().splitlines()[1][:40])
    for level in (0, 100, 0):
        print(sc.cmd("fpga scope level %d" % level).strip().splitlines()[1])
        time.sleep(1.0)
        e0, o0 = counters(sc); g = [grab_frame(sc)["gen"] for _ in range(4)]; e1, o1 = counters(sc)
        adv = np.diff(g)
        print("   level %+4d: gen %s -> %s   edges +%d  OK +%d"
              % (level, list(g), "FROZEN (holds last trace)" if np.all(adv == 0) else "advancing", e1 - e0, o1 - o0))
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0")
    sc.close()


if __name__ == "__main__":
    main()
