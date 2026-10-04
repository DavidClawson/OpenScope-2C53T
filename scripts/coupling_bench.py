#!/usr/bin/env python3
"""Bench acceptance for AC/DC coupling from the buttons (2026-10-03).

Until 2026-10-03 the CH1/CH2 coupling control was INERT: it cycled a DC/AC/GND
label while the PD12/PD13 relays stayed DC from init. This drives coupling only
through `btn ch1` / `btn ch2` (the real button handler, injected below the key
scan) and measures what the input actually does.

Method: JDS sine 201.2 Hz, 0.5 Vpp on both channels, range 5. Step the DC
offset 0 V -> +1.0 V and take the change in the committed record's mean
(whole 1024-sample record, ~16 periods). At range 5 (~21 mV/count) a DC-coupled
input should move ~47 counts; an AC-coupled one (~9 Hz high-pass) should not.

PASS CRITERIA, FIXED BEFORE THE RUN:
  readback  after every press `fpga scope coupling` reports the label and the
            relay pin AGREE
  DC        |delta mean| >= 30 counts                       (each channel)
  AC        |delta mean| <= 6 counts                        (each channel)
  restore   back to DC: |delta mean| >= 30 again            (each channel)
  isolation with CH1 in AC, CH2 (DC) still moves >= 30, and vice versa --
            each relay drives its own channel only
  label     the popup path never shows GND (GND left the cycle)

Sign: CH2 moves the OPPOSITE way to CH1 for the same +1 V step (run 1:
+50 vs -50). The criteria use |delta|; whether the inversion is the scope's
CH2 front end or the JDS CH2 output is OPEN (needs a cable swap).

Usage:  python3 scripts/coupling_bench.py
"""
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench import Scope, JDS6600  # noqa: E402
from exp22_stability import grab_frame  # noqa: E402

OFFSET_V = 1.0


def coupling(sc):
    r = sc.cmd("fpga scope coupling")
    out = {}
    for line in r.splitlines():
        if line.startswith("CH"):
            ch = int(line[2])
            out[ch] = dict(label=line.split()[2], relay=line.split("relay ")[1].split()[0],
                           agree="DISAGREE" not in line)
    return out


def set_coupling(sc, ch, want):
    """Press CHn until its label reads `want`; at most 3 presses."""
    for _ in range(3):
        c = coupling(sc)[ch]
        if c["label"] == want:
            return c
        sc.cmd("btn ch%d" % ch)
        time.sleep(0.3)
    return coupling(sc)[ch]


def mean_shift(sc, sg, frames=4):
    """Mean of CH1/CH2 records at 0 V and at +OFFSET_V; returns (d1, d2)."""
    out = []
    for off in (0.0, OFFSET_V):
        sg.offset(off, 1)
        sg.offset(off, 2)
        time.sleep(2.0)
        m1, m2 = [], []
        for _ in range(frames):
            fr = grab_frame(sc)
            m1.append(np.mean(fr["ch1"]))
            m2.append(np.mean(fr["ch2"]))
            time.sleep(0.3)
        out.append((np.median(m1), np.median(m2)))
    sg.offset(0.0, 1)
    sg.offset(0.0, 2)
    return out[1][0] - out[0][0], out[1][1] - out[0][1]


def main():
    sc = Scope()
    sg = JDS6600()
    print(next(l for l in sc.version().splitlines() if l.startswith("Build:")))
    sc.trigger_mode("auto")
    sc.vdiv(1, 5)
    sc.vdiv(2, 5)
    sc.timebase(0x10)
    for ch in (1, 2):
        sg.waveform("sine", ch); sg.freq(201.2, ch); sg.amp(0.5, ch); sg.offset(0.0, ch)
    sg.output(True, True)
    time.sleep(1.0)
    # JDS6600 quirk (found by this script's first run, 2026-10-03): after its
    # outputs are (re)enabled, the FIRST CH2 offset change does not reach the
    # output although the register reads back correct (d2 ~0, then -50 on
    # every repeat; isolated to the output enable, not the scope's range
    # change). Prime one offset step before measuring anything.
    for ch in (1, 2):
        sg.offset(OFFSET_V, ch)
    time.sleep(0.5)
    for ch in (1, 2):
        sg.offset(0.0, ch)
    time.sleep(0.5)

    ok = []

    def check(tag, cond, msg):
        print("  %s  %-22s %s" % ("PASS" if cond else "FAIL", tag, msg))
        ok.append(cond)

    labels_seen = set()
    for ch in (1, 2):
        c = set_coupling(sc, ch, "DC")
        check("ch%d readback DC" % ch, c["label"] == "DC" and c["agree"], str(c))
    d1, d2 = mean_shift(sc, sg)
    check("DC both", abs(d1) >= 30 and abs(d2) >= 30, "d1 %+.1f d2 %+.1f counts" % (d1, d2))

    for ch, other in ((1, 2), (2, 1)):
        sc.cmd("btn ch%d" % ch)                     # DC -> AC
        time.sleep(0.3)
        c = coupling(sc)
        labels_seen.add(c[ch]["label"])
        check("ch%d readback AC" % ch, c[ch]["label"] == "AC" and c[ch]["agree"]
              and c[ch]["relay"] == "AC", str(c[ch]))
        d = mean_shift(sc, sg)
        dc_, dothr = d[ch - 1], d[other - 1]
        check("ch%d AC blocks DC" % ch, abs(dc_) <= 6, "delta %+.1f counts" % dc_)
        check("ch%d isolation" % other, abs(dothr) >= 30,
              "CH%d (DC) delta %+.1f while CH%d is AC" % (other, dothr, ch))
        sc.cmd("btn ch%d" % ch)                     # AC -> DC (GND must not appear)
        time.sleep(0.3)
        c = coupling(sc)
        labels_seen.add(c[ch]["label"])
        check("ch%d restore readback" % ch, c[ch]["label"] == "DC" and c[ch]["agree"], str(c[ch]))
        d = mean_shift(sc, sg)
        check("ch%d restore DC" % ch, abs(d[ch - 1]) >= 30, "delta %+.1f counts" % d[ch - 1])

    check("no GND in cycle", "GND" not in labels_seen, "labels seen %s" % sorted(labels_seen))
    sg.output(False, False)
    print("\n%d/%d criteria met" % (sum(ok), len(ok)))
    return 0 if all(ok) else 1


if __name__ == "__main__":
    sys.exit(main())
