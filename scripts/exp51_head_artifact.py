#!/usr/bin/env python3
"""EXP-51 -- the invalid head of every readout: how long, what is in it, and what moves it.

PROBLEM
    Every record the acq task commits carries 32-64 invalid samples at its head
    (EXP-42: zeros in one record, a 110 -> 19 jump in another, a rising ramp in
    a third; clean sinusoid from ~sample 64 to 1023; the memory wraps at 1024).
    It is present gated or free-run, in every trigger mode. It is the last
    thing between the record and a whole-buffer consumer (FFT-live). Which of
    the four candidate mechanisms is it?

        H_time    a fixed TIME after the trigger/arm (analog settle, relay,
                  trigger holdoff): head length in SAMPLES scales with the
                  sample rate -- 4x more samples at 0x0E than at 0x10, 4x fewer
                  at 0x12. Falsifier: same sample count at all three codes.
        H_bytes   a fixed BYTE count in the readout path (SPI clock, FIFO,
                  the task's read timing): head length constant across codes
                  and changed by `fpga acqbr` (/2 vs /256) or `fpga pairgap`.
                  Falsifier: identical head under both clocks and both gaps.
        H_rotate  the head is stale data from the previous record (a rotation
                  or the previous capture's tail): the head correlates with
                  the previous record's tail, or fits the SAME sine at a
                  different phase. Falsifier: head is zeros/constant, or fits
                  no sine.
        H_dropout the head is a converter/mux dropout: samples are zeros or a
                  constant (note the firmware adds FPGA_ADC_OFFSET = -28 and
                  clamps, so a committed 0 means raw <= 28, not raw 0).
                  Falsifier: head samples are shaped.

    These are not exclusive; the script reports every measure per record so
    a mixed answer is visible rather than averaged away.

MEASUREMENT
    Per record: fit A sin + B cos + C at the drive frequency on samples
    128-1023 (as the EXP-42 body metric does); residual r over the whole
    record; floor = RMS(r[128:1024]). Head length = 1 + the largest index
    i < 128 for which |r| > 4 x floor on at least 3 of the 4 samples i-3..i
    (0 if none). Also: fraction of head samples equal to 0, head std, the
    best-fitting phase of a same-frequency sine on the head alone with its
    residual RMS ("shaped" if < 3 x floor), and the peak normalised
    cross-correlation of the head against the previous record's tail.

PREDICTIONS (written before the run)
    (a) derived defaults, 0x10, 201.2 Hz, 10 records: head 32-64 samples,
        matching EXP-42; this is the baseline the other arms compare to.
    (b) `fpga acqbr 7` (/256) and `fpga acqbr 0` (/2): H_bytes predicts a
        change (a /256 read is 128x longer -- the head grows or the record
        tears); H_time predicts no change.
    (c) 0x0E (fs ~49.9k) and 0x12 (fs ~2.5k): H_time predicts ~4x and ~1/4x
        the 0x10 sample count (constant milliseconds); H_bytes predicts the
        same sample count at all three.
    (d) `fpga pairgap 50`: no change under any hypothesis except a readout
        FIFO that drains between the 04 and 05 reads (H_bytes variant).
    (e) CH2 (op05) with the JDS second channel at the same frequency: the
        same head under H_time/H_dropout (shared engine), possibly different
        under H_bytes if the 05 read's position in the pair matters.

CONTROL
    Every knob is read back and asserted against the handler's exact text
    before the arm runs. Arm (a) must reproduce EXP-42's 32-64 or the
    session is VOID (the drive, range or level moved). Body floor must stay
    at the EXP-47 level (~6 at 201.2 Hz, 0x10) in every arm -- a floor above
    20 means the record is torn and the head measure is meaningless for it.
    Timebase changes are confirmed by `fpga scope timebase` not answering
    NOT SET, and every knob is restored at the end.

PRECONDITIONS
    JDS6600 CH1 -> scope CH1 and (arm e) JDS6600 CH2 -> scope CH2, as in
    EXP-38. Range 5 on both. Level 0. AUTO. Derived post-edge / autowait.
    A CH2 span below 40 counts is reported as "not driven" and arm (e) is
    skipped rather than classified.

BLIND SPOT (known before the run)
    With a periodic drive, "the head fits the same sine at another phase"
    and "the head is the previous record's tail" are indistinguishable by
    correlation alone -- any same-frequency segment correlates with the tail
    at some lag inside one period. The discriminator is the PHASE OFFSET
    (dphase): a fixed rotation by k samples gives the same dphase in every
    record (k x 360 / samples-per-cycle); a stale segment from an unrelated
    time gives a random dphase per record. Read dphase across records, not
    the correlation.
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
N_REC = 10


def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def sine_basis(n_idx, fs, f):
    w = 2 * np.pi * f / fs
    return np.c_[np.sin(w * n_idx), np.cos(w * n_idx), np.ones(len(n_idx))]


def analyse(v, fs, f, prev):
    """Return a dict of per-record head measures (see MEASUREMENT)."""
    v = np.asarray(v, float)
    n = np.arange(len(v))
    A = sine_basis(n, fs, f)
    body = np.arange(128, 1024)
    c, *_ = np.linalg.lstsq(A[body], v[body], rcond=None)
    r = v - A @ c
    floor = float(np.sqrt(np.mean(r[body] ** 2)))
    bad = np.abs(r[:128]) > 4.0 * floor
    head = 0
    for i in range(127, 2, -1):
        if bad[i - 3:i + 1].sum() >= 3:
            head = i + 1
            break
    out = dict(head=head, floor=floor, body_phase=float(np.degrees(np.arctan2(c[1], c[0]))))
    if head >= 8:
        h = v[:head]
        out["zeros"] = float(np.mean(h == 0))
        out["std"] = float(np.std(h))
        # does the head fit the SAME sine at some other phase? (fit stops 4
        # samples short of the detected boundary so the edge sample does not
        # dominate a 40-sample fit)
        hf = h[:max(8, head - 4)]
        Ah = sine_basis(np.arange(len(hf)), fs, f)
        ch, *_ = np.linalg.lstsq(Ah, hf, rcond=None)
        rh = hf - Ah @ ch
        out["head_fit_rms"] = float(np.sqrt(np.mean(rh ** 2)))
        out["head_phase"] = float(np.degrees(np.arctan2(ch[1], ch[0])))
        out["shaped"] = out["head_fit_rms"] < 3.0 * floor and np.hypot(ch[0], ch[1]) > 0.5 * np.hypot(c[0], c[1])
        # is the head the previous record's tail?
        if prev is not None:
            p = np.asarray(prev, float)
            hz = h - h.mean()
            best = 0.0
            for lag in range(0, 65):
                seg = p[1024 - head - lag:1024 - lag] if lag else p[1024 - head:]
                sz = seg - seg.mean()
                d = np.linalg.norm(hz) * np.linalg.norm(sz)
                if d > 0:
                    best = max(best, float(np.dot(hz, sz) / d))
            out["prev_tail_corr"] = best
    return out


def fmt(o):
    s = "head %3d  floor %5.1f" % (o["head"], o["floor"])
    if "zeros" in o:
        s += "  zeros %.2f std %5.1f  headfit %5.1f (%s, dphase %+4.0f deg)" % (
            o["zeros"], o["std"], o["head_fit_rms"], "SHAPED" if o["shaped"] else "unshaped",
            ((o["head_phase"] - o["body_phase"] + 180) % 360) - 180)
        if "prev_tail_corr" in o:
            s += "  prev-tail corr %.2f" % o["prev_tail_corr"]
    return s


def run_arm(sc, label, fs, f=F_DRIVE, ch="ch1", n=N_REC):
    e0, o0 = counters(sc)
    prev = None
    res = []
    for _ in range(n):
        fr = grab_frame(sc)
        v = fr[ch]
        if ch == "ch2" and (max(v) - min(v)) < 40:
            print("  %s: CH2 span %d -- not driven, arm skipped" % (label, max(v) - min(v)))
            return None
        o = analyse(v, fs, f, prev)
        res.append(o)
        prev = v
    e1, o1 = counters(sc)
    heads = [o["head"] for o in res]
    floors = [o["floor"] for o in res]
    print("  %-34s edges +%d commits +%d" % (label, e1 - e0, o1 - o0))
    for o in res:
        print("      " + fmt(o))
    print("      => head samples %s  median %d = %.2f ms at fs %.0f;  floor median %.1f%s"
          % (heads, int(np.median(heads)), np.median(heads) * 1000.0 / fs, fs, np.median(floors),
             "  ** TORN RECORDS, head measure unreliable **" if np.median(floors) > 20 else ""))
    return res


def readback(sc, cmd, must_contain):
    r = sc.cmd(cmd)
    assert must_contain in r, "readback of %r lacks %r:\n%s" % (cmd, must_contain, r)
    return r


def set_tb(sc, tb):
    r = sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0)
    assert "NOT SET" not in r and "sage" not in r, r
    time.sleep(0.8)


def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False)
    for ch in (1, 2):
        jds.waveform("sine", ch); jds.freq(F_DRIVE, ch); jds.amp(3.0, ch); jds.offset(0.0, ch)
    jds.output(True, True); jds.close()

    rates = load_rates()
    sc = Scope("/dev/ttyACM0")
    print("EXP-51  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope vdiv 2 5")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope level 0")
    readback(sc, "fpga postedge 0", "derived")
    readback(sc, "fpga autowait 0", "derived")
    readback(sc, "fpga acqbr off", "not set by the task")
    readback(sc, "fpga pairgap 0", "acq pair gap 0 ms")
    set_tb(sc, 0x10)

    print("\n=== (a) baseline: derived defaults, 0x10, %.1f Hz ===" % F_DRIVE)
    base = run_arm(sc, "baseline 0x10", rates[0x10])
    if base is None or np.median([o["head"] for o in base]) == 0:
        print("  baseline shows NO head -- EXP-42's defect did not reproduce; later arms are uninterpretable")

    print("\n=== (b) task read clock ===")
    readback(sc, "fpga acqbr 7", "br=7 (/256)"); time.sleep(0.5)
    run_arm(sc, "acqbr 7 (/256)", rates[0x10])
    readback(sc, "fpga acqbr 0", "br=0 (/2)"); time.sleep(0.5)
    run_arm(sc, "acqbr 0 (/2)", rates[0x10])
    readback(sc, "fpga acqbr off", "not set by the task"); time.sleep(0.5)

    print("\n=== (c) sample rate: 0x0E and 0x12 (0x10 baseline above) ===")
    for tb in (0x0E, 0x12):
        set_tb(sc, tb)
        run_arm(sc, "timebase 0x%02X" % tb, rates[tb])
    set_tb(sc, 0x10)

    print("\n=== (d) pair gap 50 ms between the 04 and 05 reads ===")
    readback(sc, "fpga pairgap 50", "acq pair gap 50 ms"); time.sleep(0.5)
    run_arm(sc, "pairgap 50", rates[0x10])
    readback(sc, "fpga pairgap 0", "acq pair gap 0 ms"); time.sleep(0.5)

    print("\n=== (e) CH2 (op05), same drive on the JDS second channel ===")
    run_arm(sc, "CH2 0x10", rates[0x10], ch="ch2")

    print("\n=== (a') baseline again (drift control) ===")
    run_arm(sc, "baseline 0x10 again", rates[0x10], n=5)

    readback(sc, "fpga acqbr off", "not set by the task")
    readback(sc, "fpga pairgap 0", "acq pair gap 0 ms")
    set_tb(sc, 0x10)
    sc.close()


if __name__ == "__main__":
    main()
