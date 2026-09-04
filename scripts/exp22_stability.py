#!/usr/bin/env python3
"""EXP-22 -- display-stability acceptance test: does the screen hold still?

The question, stated as a user would: with the generator driving BOTH channels
with periodic signals, does the rendered trace stay steady -- and stay steady
while amplitude, frequency and relative phase change -- the way a working
bench scope's does?

"Steady" decomposes into four verifiable claims, each mapped to a real
subsystem, each measured through the REAL display path (`spi3 frame` returns
the acq-task RAM buffers the renderer draws, plus the trigger offset computed
by the renderer's own scope_ui_soft_trigger_offset -- not a host replica):

  1. HORIZONTAL LOCK  consecutive frames, aligned at the renderer's own
                      trigger offset, differ by <= JITTER_PASS samples p95
                      (1 sample == 1 screen px in the 320-px draw window).
  2. VERTICAL         the median stays at ~128 across every scenario (the
                      centering servo's operating point holds), and the span
                      is consistent frame to frame.
  3. TWO-CHANNEL      CH2's phase relative to CH1 tracks the commanded JDS
                      phase steps -- the "offset applied to BOTH channels"
                      contract of the soft trigger.
  4. FREQUENCY        the captured fundamental matches the commanded
                      frequency at the measured sample rate.

Plus one on-hardware NEGATIVE CONTROL: soft trigger OFF must make the lock
metric FAIL (large jitter). A stability metric that cannot detect a
free-running trace proves nothing (held-out-sets lesson, EXP-17).

Cabling: JDS6600 CH1 -> scope CH1, JDS6600 CH2 -> scope CH2 (two cables).

Usage:
  exp22_stability.py                run the full matrix on hardware
  exp22_stability.py --selftest     validate the metric math on synthetic
                                    frames, no hardware needed
"""
import argparse
import math
import os
import re
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from bench import Scope, JDS6600, BenchError, parse_dump  # noqa: E402

FRAME_RE = re.compile(
    r"FRAME gen=(\d+) coherent=(\d+) src=CH(\d) off=(\d+) soft=(\d+)")

N_FRAMES     = 8       # frames per scenario
FRAME_GAP_S  = 0.12
DRAW_N       = 320     # renderer window: min(LCD_WIDTH, 512)
BUF_N        = 1024

JITTER_PASS  = 2.0     # samples p95 -- 2 px on screen, visually steady
JITTER_NEGCTL = 5.0    # free-run must exceed this, or the metric is blind
MEDIAN_TOL   = 10      # counts around 128 after centering
SPAN_CV_TOL  = 0.05    # frame-to-frame span consistency (std/mean)
VPP_TOL      = 0.12    # absolute Vpp vs commanded (percentile span carries
                       # the additive noise floor EXP-21's slope fit absorbs,
                       # so this is deliberately looser than the +-1.2%
                       # the device's own slope-validated path achieves)
FREQ_TOL     = 0.025
PHASE_TOL_DEG = 10.0

# r6 gains, raw table * SOURCE_SCALE (scope_cal.c / scope_cal.h). Volts/count.
K_R6 = {1: 42.95e-3 * 0.92, 2: 41.71e-3 * 0.92}


def load_rates():
    """Sample rates straight out of scope_timebase.c -- one source of truth."""
    src = os.path.join(os.path.dirname(__file__), "..", "firmware",
                       "src", "ui", "scope_timebase.c")
    rates = {}
    for line in open(src):
        m = re.match(r"\s*/\*\s*(0x[0-9A-Fa-f]{2})\s*\*/\s*([0-9.]+)f", line)
        if m:
            rates[int(m.group(1), 16)] = float(m.group(2))
    return rates


# ---------------------------------------------------------------------------
# Frame grab + parse
# ---------------------------------------------------------------------------

def grab_frame(sc):
    txt = sc.cmd("spi3 frame", timeout=25.0)
    m = FRAME_RE.search(txt)
    if not m:
        raise BenchError("no FRAME header in reply:\n%s" % txt[:300])
    hdr = dict(gen=int(m.group(1)), coherent=int(m.group(2)),
               src=int(m.group(3)), off=int(m.group(4)), soft=int(m.group(5)))
    # CH1 and CH2 dumps both start at offset 0000: split at the CH2 header so
    # parse_dump's strict drop-detection still applies to each half.
    i = txt.find("CH2 (")
    if i < 0:
        raise BenchError("no CH2 dump in reply")
    ch1 = parse_dump(txt[:i])
    ch2 = parse_dump(txt[i:])
    if len(ch1) != BUF_N or len(ch2) != BUF_N:
        raise BenchError("short dump: ch1=%d ch2=%d" % (len(ch1), len(ch2)))
    hdr["ch1"], hdr["ch2"] = ch1, ch2
    return hdr


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def dft_at(x, f, fs):
    """Complex DFT of ``x`` at the EXACT frequency ``f`` (no bin quantizing)."""
    x = np.asarray(x, dtype=float)
    x = x - x.mean()
    n = np.arange(len(x))
    return np.sum(x * np.exp(-2j * np.pi * f * n / fs))


def wrap_deg(d):
    return (d + 180.0) % 360.0 - 180.0


def lock_jitter_samples(frames, f, fs):
    """p95 |residual shift| in samples between trigger-aligned draw windows.

    Each frame's window starts at the renderer's own offset; the phase of the
    fundamental measured at the exact drive frequency converts any residual
    time shift to samples: dt = dphase / (2*pi*f). Frame 0 is the reference.
    """
    phases = []
    for fr in frames:
        w = fr["ch1"][fr["off"]: fr["off"] + DRAW_N]
        phases.append(np.angle(dft_at(w, f, fs)))
    ref = phases[0]
    resid = []
    for p in phases[1:]:
        dph = math.atan2(math.sin(p - ref), math.cos(p - ref))
        resid.append(abs(dph / (2 * math.pi * f) * fs))
    p95 = float(np.percentile(resid, 95)) if resid else 0.0
    return p95, resid


def rel_phase_deg(frames, f, fs):
    """Circular-mean CH2-vs-CH1 phase at the fundamental, full buffers."""
    zs = []
    for fr in frames:
        z1 = dft_at(fr["ch1"], f, fs)
        z2 = dft_at(fr["ch2"], f, fs)
        zs.append(z2 * np.conj(z1))
    return math.degrees(np.angle(np.sum(zs)))


def freq_peak(x, fs):
    """Fundamental from the full buffer: rFFT argmax + parabolic refinement."""
    x = np.asarray(x, dtype=float)
    mag = np.abs(np.fft.rfft(x - x.mean()))
    mag[0] = 0.0
    k = int(np.argmax(mag))
    if 0 < k < len(mag) - 1 and mag[k] > 0:
        denom = mag[k - 1] - 2 * mag[k] + mag[k + 1]
        d = 0.5 * (mag[k - 1] - mag[k + 1]) / denom if denom else 0.0
        d = max(-0.5, min(0.5, d))
    else:
        d = 0.0
    return (k + d) * fs / len(x)


def span(x):
    return float(np.percentile(x, 99.5) - np.percentile(x, 0.5))


# ---------------------------------------------------------------------------
# Scenario runner
# ---------------------------------------------------------------------------

def scenarios():
    base = dict(wave="sine", f=500.0, a1=2.0, a2=2.0, ph=0.0,
                tb=0x10, softtrig=True, expect_lock=True)
    def s(name, **kw):
        d = dict(base, name=name)
        d.update(kw)
        return d
    return [
        s("baseline 500Hz 2Vpp"),
        s("freq 200Hz",  f=200.0),
        s("freq 1kHz",   f=1000.0),
        s("amp 1Vpp",    a1=1.0, a2=1.0),
        s("amp 4Vpp",    a1=4.0, a2=4.0),
        s("phase 45",    ph=45.0),
        s("phase 90",    ph=90.0),
        s("phase 180",   ph=180.0),
        s("square 500Hz", wave="square"),
        s("NEGCTL free-run", softtrig=False, expect_lock=False),
        s("tb 0x0E 2kHz", tb=0x0E, f=2000.0),
    ]


def apply_scenario(sc, sg, scen, state):
    if scen["tb"] != state.get("tb"):
        sc.timebase(scen["tb"])
        state["tb"] = scen["tb"]
    if scen["softtrig"] != state.get("softtrig"):
        sc.cmd("fpga scope softtrig %s" % ("on" if scen["softtrig"] else "off"))
        state["softtrig"] = scen["softtrig"]
    for ch, key in ((1, "a1"), (2, "a2")):
        if (scen["wave"], ch) != state.get(("wave", ch)):
            sg.waveform(scen["wave"], ch)
            state[("wave", ch)] = (scen["wave"], ch)
        if scen["f"] != state.get(("f", ch)):
            sg.freq(scen["f"], ch)
            state[("f", ch)] = scen["f"]
        if scen[key] != state.get(("a", ch)):
            sg.amp(scen[key], ch)
            state[("a", ch)] = scen[key]
    if scen["ph"] != state.get("ph"):
        sg.phase(scen["ph"])
        state["ph"] = scen["ph"]
    time.sleep(0.6)


def eval_scenario(scen, frames, fs, base_rel):
    """Returns (list of 'CHECK: PASS/FAIL detail' strings, ok, rel_deg)."""
    out, ok = [], True
    def chk(cond, label, detail):
        nonlocal ok
        out.append("  %-5s %-14s %s" % ("PASS" if cond else "FAIL", label, detail))
        ok = ok and cond

    gens = [fr["gen"] for fr in frames]
    chk(all(fr["coherent"] == 1 for fr in frames), "coherent",
        "gen even+stable on all %d frames" % len(frames))
    chk(len(set(gens)) == len(gens) and gens == sorted(gens), "distinct",
        "generations %d..%d strictly increasing" % (gens[0], gens[-1]))

    f = scen["f"]
    jit, resid = lock_jitter_samples(frames, f, fs)
    offs = [fr["off"] for fr in frames]
    out.append("        per-frame      offs %s" % offs)
    out.append("        per-frame      resid %s"
               % ["%.2f" % r for r in resid])
    if scen["expect_lock"]:
        chk(all(o > 0 for o in offs), "lock-found",
            "renderer offset nonzero on all frames (offs %s..%s)"
            % (min(offs), max(offs)))
        chk(jit <= JITTER_PASS, "lock-jitter",
            "p95 %.2f samples (= px) <= %.1f" % (jit, JITTER_PASS))
    else:
        chk(jit >= JITTER_NEGCTL, "negctl",
            "free-run jitter p95 %.1f samples >= %.1f (metric can fail)"
            % (jit, JITTER_NEGCTL))

    for ch, amp_key in ((1, "a1"), (2, "a2")):
        # Vertical operating point: the MIDLINE (p0.5+p99.5)/2, not the
        # median -- a square wave's median sits on one of its two levels
        # (run 1 read 149 on a correctly-centered square), while the midline
        # is the DC operating point for sine and square alike.
        meds = [(np.percentile(fr["ch%d" % ch], 0.5)
                 + np.percentile(fr["ch%d" % ch], 99.5)) / 2.0
                for fr in frames]
        chk(all(abs(m - 128) <= MEDIAN_TOL for m in meds),
            "midline ch%d" % ch,
            "%.0f..%.0f (128 +- %d)" % (min(meds), max(meds), MEDIAN_TOL))
        spans = [span(fr["ch%d" % ch]) for fr in frames]
        mean_span = float(np.mean(spans))
        cv = float(np.std(spans)) / mean_span if mean_span else 99.0
        chk(cv <= SPAN_CV_TOL, "span-cv ch%d" % ch,
            "std/mean %.3f <= %.2f (span %.1f cts)" % (cv, SPAN_CV_TOL, mean_span))
        vpp = mean_span * K_R6[ch]
        err = vpp / scen[amp_key] - 1.0
        chk(abs(err) <= VPP_TOL, "vpp ch%d" % ch,
            "%.3f V vs %.1f V commanded (%+.1f%%)"
            % (vpp, scen[amp_key], err * 100))

    fmeas = float(np.mean([freq_peak(fr["ch1"], fs) for fr in frames]))
    chk(abs(fmeas / f - 1.0) <= FREQ_TOL, "freq",
        "%.1f Hz vs %.1f commanded (%+.2f%%)" % (fmeas, f, (fmeas / f - 1) * 100))

    rel = rel_phase_deg(frames, f, fs)
    if scen["ph"] != 0.0 and base_rel is not None:
        d = wrap_deg(rel - base_rel)
        best = min(abs(wrap_deg(d - scen["ph"])), abs(wrap_deg(d + scen["ph"])))
        chk(best <= PHASE_TOL_DEG, "rel-phase",
            "delta %.1f deg vs commanded %.0f (mismatch %.1f <= %.0f)"
            % (d, scen["ph"], best, PHASE_TOL_DEG))
    else:
        out.append("        rel-phase      %.1f deg (reference)" % rel)
    return out, ok, rel


# ---------------------------------------------------------------------------
# Selftest -- metric math on synthetic frames, no hardware
# ---------------------------------------------------------------------------

def _soft_trigger_replica(x):
    """Python port of scope_soft_trigger_offset for SYNTHETIC frames only.

    The hardware run never uses this -- it takes the offset the firmware
    prints. This exists so the selftest can hand the metrics the same *shape*
    of input they get on hardware."""
    mn, mx = int(x.min()), int(x.max())
    if mx - mn < 8:
        return 0
    thr = (mn + mx) // 2
    max_start = BUF_N - DRAW_N
    hyst, armed = 3, False
    for i in range(max_start):
        v = int(x[i])
        if not armed:
            if v <= thr - hyst:
                armed = True
        elif v >= thr:
            return i
    return 0


def _synth(f, fs, t0, rel_deg=90.0, amp=52.0, noise=1.2, seed=0):
    rng = np.random.default_rng(seed)
    n = np.arange(BUF_N)
    ph = 2 * np.pi * f * (n / fs + t0)
    c1 = 128 + amp * np.sin(ph) + rng.normal(0, noise, BUF_N)
    c2 = 128 + amp * np.sin(ph + math.radians(rel_deg)) \
             + rng.normal(0, noise, BUF_N)
    c1 = np.clip(np.round(c1), 0, 255).astype(np.uint8)
    c2 = np.clip(np.round(c2), 0, 255).astype(np.uint8)
    return c1, c2


def selftest():
    fs, f = 12490.0, 500.0
    rng = np.random.default_rng(42)
    frames = []
    for i in range(8):
        t0 = float(rng.uniform(0, 1.0 / f))       # random capture phase
        c1, c2 = _synth(f, fs, t0, rel_deg=90.0, seed=i)
        frames.append(dict(gen=2 * i + 2, coherent=1, src=1, soft=1,
                           off=_soft_trigger_replica(c1), ch1=c1, ch2=c2))

    fails = 0
    def chk(cond, msg):
        nonlocal fails
        print("  %s %s" % ("PASS" if cond else "FAIL", msg))
        fails += 0 if cond else 1

    jit, _ = lock_jitter_samples(frames, f, fs)
    chk(jit < 1.2, "aligned synthetic frames: jitter p95 %.2f < 1.2 samples "
        "(integer trigger index leaves sub-sample residual)" % jit)

    # Negative control: the SAME frames unaligned must read as free-running.
    free = [dict(fr, off=0) for fr in frames]
    jfree, _ = lock_jitter_samples(free, f, fs)
    chk(jfree > JITTER_NEGCTL, "unaligned frames: jitter p95 %.1f > %.1f "
        "(metric detects a dancing trace)" % (jfree, JITTER_NEGCTL))

    rel = rel_phase_deg(frames, f, fs)
    chk(abs(wrap_deg(rel - 90.0)) < 2.0,
        "relative phase recovered %.2f deg (commanded 90)" % rel)

    fm = freq_peak(frames[0]["ch1"], fs)
    chk(abs(fm / f - 1) < 0.01, "freq peak %.1f Hz vs 500" % fm)

    # Known sub-sample shift recovery: shift one frame by 0.4 samples in time
    # and confirm the phase metric sees ~0.4, not 0.
    c1s, _ = _synth(f, fs, t0=0.4 / fs, seed=99)
    c1r, _ = _synth(f, fs, t0=0.0, seed=99)
    pair = [dict(gen=2, coherent=1, src=1, soft=1, off=0, ch1=c1r, ch2=c1r),
            dict(gen=4, coherent=1, src=1, soft=1, off=0, ch1=c1s, ch2=c1s)]
    j, _ = lock_jitter_samples(pair, f, fs)
    chk(abs(j - 0.4) < 0.1, "0.4-sample injected shift measured as %.2f" % j)

    print("selftest: %s" % ("ALL PASS" if fails == 0 else "%d FAILED" % fails))
    return fails == 0


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--scope-port", default="/dev/ttyACM0")
    ap.add_argument("--jds-port", default="/dev/ttyUSB0")
    ap.add_argument("--frames", type=int, default=N_FRAMES)
    ap.add_argument("--skip-center", action="store_true",
                    help="skip the centering servo pass (already centered)")
    ap.add_argument("--save", default=None,
                    help="save every raw frame to this .npz for post-hoc "
                         "analysis (keys: <scenario>__ch1/ch2/off/gen)")
    ap.add_argument("--only", default=None,
                    help="run only scenarios whose name contains this")
    a = ap.parse_args()

    if a.selftest:
        sys.exit(0 if selftest() else 1)

    rates = load_rates()
    sc = Scope(a.scope_port)
    sg = JDS6600(a.jds_port)

    build = next((l for l in sc.version().splitlines()
                  if l.startswith("Build:")), "?")
    print("device %s" % build)

    # -- setup ------------------------------------------------------------
    print("setup: range 6 both channels, timebase 0x10, soft trigger on")
    sc.vdiv(1, 6)
    sc.vdiv(2, 6)
    sc.timebase(0x10)
    sc.cmd("fpga scope softtrig on")

    sg.waveform("sine", 1); sg.waveform("sine", 2)
    sg.freq(500, 1);        sg.freq(500, 2)
    sg.amp(2.0, 1);         sg.amp(2.0, 2)
    sg.offset(0.0, 1);      sg.offset(0.0, 2)
    sg.phase(0.0)
    sg.output(True, True)

    if not a.skip_center:
        for ch in (1, 2):
            print("centering ch%d..." % ch, end=" ", flush=True)
            r = sc.cmd("fpga scope center ch%d 6" % ch, timeout=60.0)
            line = next((l for l in r.splitlines() if "center" in l), "?")
            print(line.strip())

    # -- matrix -----------------------------------------------------------
    state = {"tb": 0x10, "softtrig": True, "ph": 0.0}
    base_rel = None
    results = []
    saved = {}
    for scen in scenarios():
        if a.only and a.only not in scen["name"]:
            continue
        fs = rates[scen["tb"]]
        print("\n== %s  (fs %.0f S/s) ==" % (scen["name"], fs))
        apply_scenario(sc, sg, scen, state)
        frames = []
        for _ in range(a.frames):
            frames.append(grab_frame(sc))
            time.sleep(FRAME_GAP_S)
        if a.save:
            key = scen["name"].replace(" ", "_")
            saved[key + "__ch1"] = np.stack([fr["ch1"] for fr in frames])
            saved[key + "__ch2"] = np.stack([fr["ch2"] for fr in frames])
            saved[key + "__off"] = np.array([fr["off"] for fr in frames])
            saved[key + "__gen"] = np.array([fr["gen"] for fr in frames])
        lines, ok, rel = eval_scenario(scen, frames, fs, base_rel)
        if scen["name"].startswith("baseline"):
            base_rel = rel
        print("\n".join(lines))
        results.append((scen["name"], ok))
    if a.save and saved:
        np.savez_compressed(a.save, **saved)
        print("\nraw frames saved to %s" % a.save)

    sg.output(False, False)

    print("\n===== EXP-22 SUMMARY =====")
    for name, ok in results:
        print("  %-22s %s" % (name, "PASS" if ok else "FAIL"))
    total_ok = all(ok for _, ok in results)
    print("OVERALL: %s" % ("PASS -- trace holds still under phase/amp/freq "
                           "changes on both channels" if total_ok else "FAIL"))
    sys.exit(0 if total_ok else 1)


if __name__ == "__main__":
    main()
