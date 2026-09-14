#!/usr/bin/env python3
"""EXP-29 -- is the acquisition record a rotation of a live buffer?

QUESTION
    The record is not time-contiguous at its edges (EXP-22). Root cause open
    since 2026-09-03. `docs/re/acq_seam_root_cause.md` proposes: we clock the
    capture memory out from address 0 regardless of where the FPGA's write
    pointer sits, so the record's chronological origin IS the write pointer and
    the record is a ROTATION of a live buffer, not stale edge data.

HYPOTHESIS AND FALSIFIER
    Stock's acquisition loop gates on reg 0x01, reads the 0x04/0x05 pair, then
    re-arms. Ours has only ever done the middle step (fpga.c:3280, 2026-08-17).
    `fpga rearm on` adds the re-arm.

    If the seam is the un-gated read, records taken with re-arm ON carry
    materially smaller phase discontinuities than with it OFF.
    If re-arm ON leaves the distribution unchanged, the missing re-arm is NOT
    the cause and the rotation model needs another mechanism.

DRIVE
    201.2 Hz at timebase 0x10 (12,490 S/s) -- deliberately NOT 500 Hz or 1 kHz.
    A rotation's phase jump is frac(1024*f/fs) of a cycle, which is ~0.04 cycle
    at 500 Hz and 1 kHz: nearly invisible. That is why EXP-22's affected-frame
    count is a DETECTION FLOOR, not an occurrence rate.

CONTROL
    A/B/A. The OFF arms must SHOW seams; if they do not, this instrument cannot
    detect what the ON arm would be claimed to exclude, and the run is VOID.
    The re-arm state is read back from the device each arm -- never trusted
    from the setter, which is the defect class this project keeps finding.
"""
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, BenchError, parse_dump          # noqa: E402
from exp22_stability import grab_frame, load_rates       # noqa: E402
from exp22_seam_analysis import frame_seam               # noqa: E402

F_DRIVE = 201.2
TB_CODE = 0x10
N_PER_ARM = 10
ARMS = [("A1", "off"), ("B", "on"), ("A2", "off")]


def rearm_state(sc):
    """Read the re-arm flag back off the device. Returns 'on' or 'off'."""
    txt = sc.cmd("fpga rearm", timeout=4.0)
    m = re.search(r"acq re-arm (ON|OFF)", txt)
    if not m:
        raise BenchError("cannot read re-arm state:\n%s" % txt[:200])
    return m.group(1).lower()


def run_arm(sc, want, n, fs):
    sc.cmd("fpga rearm %s" % want, timeout=4.0)
    got = rearm_state(sc)
    if got != want:
        raise BenchError("re-arm did not take: asked %s, device says %s"
                         % (want, got))
    steps, locs, incoherent = [], [], 0
    for _ in range(n):
        fr = grab_frame(sc)
        if fr["coherent"] != 1:
            incoherent += 1
        step, loc, _, _ = frame_seam(fr["ch1"], F_DRIVE, fs)
        steps.append(step)
        locs.append(loc)
    return np.array(steps), np.array(locs), incoherent


def main():
    rates = load_rates()
    fs = rates[TB_CODE]
    sc = Scope("/dev/ttyACM0")

    print("EXP-29  seam vs acq re-arm")
    print("drive %.1f Hz, timebase 0x%02X = %.1f S/s, %d records/arm"
          % (F_DRIVE, TB_CODE, fs, N_PER_ARM))
    print(sc.cmd("fpga scope timebase %02x" % TB_CODE, timeout=6.0).strip()
          .splitlines()[-2] if True else "")

    out = {}
    for name, want in ARMS:
        steps, locs, incoh = run_arm(sc, want, N_PER_ARM, fs)
        out[name] = (steps, locs, incoh)
        print("\n-- arm %s  (re-arm %s, readback confirmed) --" % (name, want))
        print("   max-step samples: " +
              " ".join("%.1f" % s for s in steps))
        print("   median %.2f   p90 %.2f   max %.2f   incoherent %d/%d"
              % (np.median(steps), np.percentile(steps, 90), steps.max(),
                 incoh, N_PER_ARM))
        print("   seam locations:   " + " ".join("%d" % l for l in locs))

    a = np.concatenate([out["A1"][0], out["A2"][0]])
    b = out["B"][0]
    print("\n== result ==")
    print("   re-arm OFF (n=%d): median %.2f  p90 %.2f" %
          (len(a), np.median(a), np.percentile(a, 90)))
    print("   re-arm ON  (n=%d): median %.2f  p90 %.2f" %
          (len(b), np.median(b), np.percentile(b, 90)))
    print("   A1 vs A2 medians: %.2f / %.2f  (drift check)"
          % (np.median(out["A1"][0]), np.median(out["A2"][0])))
    sc.close()


if __name__ == "__main__":
    main()
