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

TRIGGER / ACQUISITION BLOCK (folded in 2026-09-22 from exp54_poll_acceptance.py,
trigger-modes spec S3, acquisition-record-integrity S3): after the display
matrix the same run drives the acquisition loop through its own entry points
(`fpga scope trigmode`, `fpga scope level`, `fpga scope timebase`) and checks

  5. NORMAL ADVANCES  at 0x0E/0x10/0x11/0x12 on a sine, and on a 4 Hz square
                      at 0x10 and a 1 Hz square at 0x12 (EXP-53's open defect,
                      fixed by the poll loop) -- every commit carries a PC0
                      handover strobe (edges == 2 x commits).
  6. NORMAL HOLDS     with the level above the signal: generations frozen,
                      zero strobes; resumes at level 0.
  7. SINGLE           exactly one record, then held.
  8. RECORD BODY      committed records are not torn: the fundamental's
                      phase steps evenly around the circular record with at
                      most one anomaly run (the rotation seam at the FPGA's
                      pointer, EXP-53/54); a torn record has two. And the
                      fundamental matches the drive.
  9. AUTO             with a signal present, strobed commits dominate
                      (edges/commit >= AUTO_STROBED_MIN).

Negative control for the hold metric, same build: AUTO with the level above
the signal must ADVANCE (the fallback) while strobing nothing -- the `hold`
classifier must say "not held" there or it proves nothing.

Edge: the FPGA fires on either edge and reg 0x02 does not select one
(EXP-55), so Rising/Falling is an MCU-side filter (src/dsp/trig_edge.c).
EDGE rising / EDGE falling check that every committed record on a 2 Hz
triangle has the chosen slope; NEGCTL edge filter off must show both.

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
    r"FRAME gen=(\d+) coherent=(\d+) src=CH(\d) off=(\d+) soft=(\d+)(?: tx=(-?\d+) anchor=(\d+))?(?: ord=(\d))?")

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

# Trigger / acquisition block
TEAR_MAX_RUNS    = 1     # phase-step anomaly runs around the circular record:
                         # 1 = the rotation seam of a committed record, 2 = torn
TEAR_STEP_DEG    = 30.0  # a step this far off the median is an anomaly
AUTO_STROBED_MIN = 1.5   # PC0 edges per committed pair in AUTO with a signal
                         # (2.0 = every commit strobed; fallback commits pull
                         # it down)
STATUS_RE = dict(edges=r"PC0 edges: (\d+)", ok=r"SPI3 OK: (\d+)",
                 latency=r"acq latency: (\d+) ms", polls=r"poll reads before it: (\d+)",
                 kept=r"acq edge filter: \w+  kept (\d+)", dropped=r"dropped (\d+)  unclassified",
                 unclassified=r"unclassified (\d+)")

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
               src=int(m.group(3)), off=int(m.group(4)), soft=int(m.group(5)),
               tx=int(m.group(6)) if m.group(6) is not None else None,
               anchor=int(m.group(7)) if m.group(7) is not None else None,
               ord=int(m.group(8)) if m.group(8) is not None else None)
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


def tear_clusters(x, f, fs, win=64, hop=32, tol_deg=30.0):
    """Is a record one continuous segment? Returns (clusters, max_step_deg).

    The phase of the fundamental is measured in a 64-sample window every 32
    samples, circularly (the last window wraps to the start). Along one
    continuous segment the phase advances by the same step every hop, so the
    steps minus their median are ~0. A discontinuity shows as a run of
    anomalous steps (|residual| > tol_deg), and runs are counted:

      * a COMMITTED record is one continuous 1024-sample segment rotated so
        its seam sits at the FPGA's write pointer (EXP-53/54) -- at most ONE
        run, and none when the segment is an integer number of periods;
      * a TORN record (two segments with different phase, a read that caught
        the roll) has TWO runs -- the tear and the wrap.

    No un-rotation is needed, so the seam is never mistaken for a tear when
    its amplitude jump is small (EXP-56's outliers), and a frequency error
    in f moves every step equally, so the median removes it. Blind spots: a
    tear whose phase jump is below tol_deg (also invisible on screen), and a
    tear in amplitude only.
    """
    v = np.asarray(x, dtype=float)
    v = v - v.mean()
    N = len(v)
    fx = freq_peak(v, fs)
    if not (0.8 * f <= fx <= 1.25 * f):
        fx = f
    n_ = np.arange(win)
    kern = np.hanning(win) * np.exp(-2j * np.pi * fx * n_ / fs)
    starts = range(0, N, hop)
    phases = [np.angle(np.sum(np.take(v, np.arange(i, i + win), mode="wrap") * kern))
              for i in starts]
    steps = np.array([math.atan2(math.sin(b - a), math.cos(b - a))
                      for a, b in zip(phases, phases[1:] + phases[:1])])
    med = float(np.median(steps))
    resid = np.array([math.degrees(math.atan2(math.sin(st - med), math.cos(st - med)))
                      for st in steps])
    bad = np.abs(resid) > tol_deg
    if bad.all():
        return len(bad), float(np.max(np.abs(resid)))
    # count runs of True on the circle: start from a False so a run that
    # wraps around is counted once
    i0 = int(np.argmin(bad))
    order = np.r_[bad[i0:], bad[:i0]]
    runs = int(np.sum(order[1:] & ~order[:-1]) + (1 if order[0] else 0))
    return runs, float(np.max(np.abs(resid)))


def internal_breaks(x, fs, f, win=64, hop=32, tol=30.0):
    """Phase-step anomaly runs NOT counting the wrap: a time-ordered record
    (seam at index 0) has none; a raw record has one wherever the FPGA's
    pointer stopped (unless the phase happens to match)."""
    v = np.asarray(x, dtype=float); v = v - v.mean()
    fx = freq_peak(v, fs)
    if not (0.8 * f <= fx <= 1.25 * f):
        fx = f
    kern = np.hanning(win) * np.exp(-2j * np.pi * fx * np.arange(win) / fs)
    ph = [np.angle(np.sum(v[i:i + win] * kern)) for i in range(0, len(v) - win + 1, hop)]
    st = np.array([math.atan2(math.sin(b - a), math.cos(b - a)) for a, b in zip(ph, ph[1:])])
    med = float(np.median(st))
    res = np.degrees(np.arctan2(np.sin(st - med), np.cos(st - med)))
    bad = np.abs(res) > tol
    return int(np.sum(bad[1:] & ~bad[:-1]) + (1 if bad[0] else 0))


_LEVEL_CODE = {}

def state_level_record(scen):
    """The level in record units (reg 0x08 code - 28), as the firmware reported
    it when the scenario set the level."""
    return _LEVEL_CODE.get(scen["level"], 128) - 28


def host_seam(v):
    """Seam of a record for the HOST-side checks: largest circular step of the
    3-sample median, so a single stray sample (sample 0 of a read is often
    one on unit #1, and after firmware un-rotation it sits INSIDE the record)
    cannot outvote the seam. Returns (k, confident)."""
    v = np.asarray(v, dtype=float)
    m3 = np.median(np.stack([np.roll(v, 1), v, np.roll(v, -1)]), axis=0)
    dv = np.abs(m3 - np.roll(m3, 1))
    k = int(np.argmax(dv))
    others = np.delete(dv, [(k + o) % len(v) for o in range(-2, 3)])
    return k, bool(dv[k] >= 6 and dv[k] >= 2 * others.max())


def gens_state(gens, n_expected=None):
    """'advancing' / 'held' / 'ambiguous' from the generation counters of
    consecutive frame grabs. Held = every grab returned the same generation.
    Advancing = at least half the grabs saw a new one (slow codes deliver a
    record every ~0.6-0.8 s, so consecutive grabs can legitimately repeat)."""
    distinct = len(set(gens))
    if distinct == 1:
        return "held"
    if distinct >= max(3, (len(gens) + 1) // 2):
        return "advancing"
    return "ambiguous"


# ---------------------------------------------------------------------------
# Trigger / acquisition block (EXP-54 arms as a regression)
# ---------------------------------------------------------------------------

def read_status(sc):
    t = sc.cmd("status", timeout=6.0)
    out = {}
    for k, rx in STATUS_RE.items():
        m = re.search(rx, t)
        if not m:
            raise BenchError("status: no '%s' line" % k)
        out[k] = int(m.group(1))
    return out


def fresh_frame(sc, commits=2, timeout=6.0):
    """A frame whose record was committed at least `commits` commits after
    this call (generation is a seqlock, +2 per commit). Under the poll loop the
    buffer changes only on a commit, so a fixed sleep can return a record from
    before the last change -- the defect the firmware centre servo had."""
    g0 = grab_frame(sc)["gen"] & ~1
    t0 = time.time()
    while True:
        fr = grab_frame(sc)
        if (fr["gen"] & ~1) >= g0 + 2 * commits:
            return fr
        if time.time() - t0 > timeout:
            raise BenchError("no fresh commit within %.0f s (gen stuck at %d)"
                             % (timeout, fr["gen"]))
        time.sleep(0.15)


def centre_ch1_host(sc, target=114.0, tol=4.0, dac=2400, slope=0.125, iters=8):
    """Put CH1's record midline at `target` by steering DAC1 (`trig raw`) on
    the acquisition record itself. The record scale is ADC - 28, so its top
    is 227 and mid-scale is ~114. Slope from the 2026-09-22 hand sweep:
    +0.125 record counts per DAC1 step on range 5 (2600->125, 3400->225).

    Host-side on purpose: the precondition must not depend on the firmware
    centre servo, which read stale buffers under the poll loop (fixed in the
    v9 image, `fpga scope center` now waits for fresh commits)."""
    mid = None
    for _ in range(iters):
        sc.cmd("trig raw %d" % dac, timeout=6.0)
        v = np.asarray(fresh_frame(sc)["ch1"], float)
        mid = (np.percentile(v, 0.5) + np.percentile(v, 99.5)) / 2.0
        if abs(mid - target) <= tol:
            return dac, mid
        dac = int(max(0, min(4095, dac + (target - mid) / slope)))
    return dac, mid


def trigger_scenarios():
    """Each entry: what to drive, which mode/level/timebase, and what a
    correct acquisition loop does. `expect`: advance | hold | single |
    advance-negctl (AUTO fallback: advances with zero strobes)."""
    base = dict(wave="sine", f=201.2, amp=2.0, off=0.0, tb=0x10, mode="normal",
                level=0, expect="advance", n=5, gap=0.4, settle=1.5,
                body=False, negctl=False, auto_window=0.0,
                edge="rising", edgefilter=True, check_edge=None,
                unrotate=True, check_order=None, check_breaks=None,
                hpos=160, check_hpos=None)
    def s(name, **kw):
        d = dict(base, name=name); d.update(kw); return d
    return [
        s("NORMAL 0x0E sine",      tb=0x0E),
        s("NORMAL 0x10 sine",      tb=0x10, body=True),
        s("NORMAL 0x11 sine",      tb=0x11, body=True, gap=0.6),
        s("NORMAL 0x12 sine",      tb=0x12, body=True, gap=1.0, settle=2.5),
        s("NORMAL square 4Hz 0x10", wave="square", f=4.0, amp=2.0, off=0.5,
          tb=0x10, settle=2.5),
        s("NORMAL square 1Hz 0x12", wave="square", f=1.0, amp=2.0, off=0.5,
          tb=0x12, gap=1.0, settle=2.5),
        s("NORMAL hold above level", level=100, expect="hold"),
        s("NEGCTL AUTO above level", mode="auto", level=100,
          expect="advance-negctl", negctl=True),
        s("NORMAL resume level 0",  level=0),
        s("SINGLE one-shot",        mode="single", expect="single"),
        s("AUTO strobed commits",   mode="auto", expect="advance",
          auto_window=10.0),
        # Edge filter (MCU-side; the FPGA fires on either edge, EXP-55). A
        # 2 Hz triangle at 0x12 puts one slow crossing per record, so every
        # committed record's edge is readable by the host from the slope
        # around the un-rotated trigger index.
        s("EDGE rising",  wave="triangle", f=2.0, amp=2.0, off=0.0, tb=0x12,
          n=8, gap=1.0, settle=3.0, check_edge="rising", check_order="ordered"),
        s("EDGE falling", wave="triangle", f=2.0, amp=2.0, off=0.0, tb=0x12,
          n=8, gap=1.0, settle=3.0, edge="falling", check_edge="falling"),
        s("NEGCTL edge filter off", wave="triangle", f=2.0, amp=2.0, off=0.0, tb=0x12,
          n=12, gap=1.0, settle=3.0, edgefilter=False, check_edge="mixed"),
        # Un-rotation (dev plan 2.3): strobed records in time order, seam at
        # index 0, trigger at 512. Negative control: raw records keep the seam
        # wherever the FPGA's pointer stopped.
        s("NEGCTL unrotate off", wave="triangle", f=2.0, amp=2.0, off=0.0, tb=0x12,
          n=8, gap=1.0, settle=3.0, unrotate=False, check_order="raw"),
        # Fast periodic record (16.5 periods): the value-step seam finder
        # missed ~40% here (v13: 3/10 records kept an internal phase break,
        # 5/10 with un-rotation off). Criterion written before the v14 run:
        # <= 1/10 with it on; >= 3/10 with it off (the control).
        s("UNROTATE fast sine", edgefilter=False, n=10, gap=0.5, check_breaks="on"),
        s("NEGCTL unrotate off fast sine", edgefilter=False, unrotate=False, n=10, gap=0.5,
          check_breaks="off"),
        # Horizontal position (2026-09-22): the trigger point lands on the
        # asked column, anchored on the hardware level crossing of the
        # time-ordered record. Control: un-rotation off + level off the
        # midline -> soft (midline) anchor, so the sample at the column is NOT
        # the level and the check must fail there.
        s("HPOS x=40",  hpos=40,  n=6, gap=0.4, check_hpos="hw"),
        s("HPOS x=160", hpos=160, n=6, gap=0.4, check_hpos="hw"),
        s("HPOS x=280", hpos=280, n=6, gap=0.4, check_hpos="hw"),
        s("NEGCTL hpos soft anchor", hpos=160, level=30, unrotate=False, n=6, gap=0.4,
          check_hpos="soft"),
    ]


def apply_trigger_scenario(sc, sg, scen, state):
    """Send only what changed, through the same entry points the UI uses.
    A timebase change is made in AUTO (the acq loop re-primes on the write;
    EXP-54 switched to AUTO around it) and the mode is re-applied after."""
    if (scen["wave"], scen["f"], scen["amp"], scen["off"]) != state.get("drive"):
        sg.waveform(scen["wave"], 1)
        sg.freq(scen["f"], 1)
        sg.amp(scen["amp"], 1)
        sg.offset(scen["off"], 1)
        state["drive"] = (scen["wave"], scen["f"], scen["amp"], scen["off"])
        time.sleep(0.8)
    if scen["tb"] != state.get("tb"):
        sc.trigger_mode("auto")
        sc.timebase(scen["tb"])
        state["tb"] = scen["tb"]
        state["mode"] = "auto"
        time.sleep(0.5)
    if scen["hpos"] != state.get("hpos"):
        sc.cmd("fpga scope hpos %d" % scen["hpos"])
        state["hpos"] = scen["hpos"]
    if scen["unrotate"] != state.get("unrotate"):
        sc.cmd("fpga unrotate %s" % ("on" if scen["unrotate"] else "off"))
        state["unrotate"] = scen["unrotate"]
    if scen["edge"] != state.get("edge"):
        sc.cmd("fpga scope edge %s" % scen["edge"])
        state["edge"] = scen["edge"]
    if scen["edgefilter"] != state.get("edgefilter"):
        sc.cmd("fpga edgefilter %s" % ("on" if scen["edgefilter"] else "off"))
        state["edgefilter"] = scen["edgefilter"]
    if scen["level"] != state.get("level"):
        r = sc.trigger_level(scen["level"])
        m = re.search(r"code 0x([0-9A-Fa-f]+)", r)
        state["level"] = scen["level"]
        state["level_code"] = int(m.group(1), 16) if m else None
        if m:
            _LEVEL_CODE[scen["level"]] = int(m.group(1), 16)
    if scen["mode"] != state.get("mode") or scen["mode"] == "single":
        sc.trigger_mode(scen["mode"])
        state["mode"] = scen["mode"]
    time.sleep(scen["settle"])


def run_trigger_scenario(sc, scen):
    """Returns dict(frames, st0, st1, g0) -- g0 only for SINGLE."""
    if scen["expect"] == "single":
        # Arm from AUTO so the one-shot has a fresh generation to beat.
        sc.trigger_mode("auto"); time.sleep(0.4)
        g0 = grab_frame(sc)["gen"]
        sc.trigger_mode("single"); time.sleep(scen["settle"])
        frames = [grab_frame(sc) for _ in range(3)]
        return dict(frames=frames, st0=None, st1=None, g0=g0)
    st0 = read_status(sc)
    frames = []
    for _ in range(scen["n"]):
        frames.append(grab_frame(sc))
        time.sleep(scen["gap"])
    if scen["auto_window"]:
        time.sleep(scen["auto_window"])
    st1 = read_status(sc)
    return dict(frames=frames, st0=st0, st1=st1, g0=None)


def eval_trigger_scenario(scen, run, fs):
    out, ok = [], True
    def chk(cond, label, detail):
        nonlocal ok
        out.append("  %-5s %-14s %s" % ("PASS" if cond else "FAIL", label, detail))
        ok = ok and cond
    frames = run["frames"]
    gens = [fr["gen"] for fr in frames]
    state = gens_state(gens)
    out.append("        gens           %s -> %s" % (gens, state))

    if scen["expect"] == "single":
        g0, g = run["g0"], gens
        chk(g[0] > g0 and len(set(g)) == 1, "single",
            "gen %d -> %s: %s" % (g0, g, "one record then held"
                                  if (g[0] > g0 and len(set(g)) == 1) else "UNEXPECTED"))
        return out, ok

    st0, st1 = run["st0"], run["st1"]
    edges = st1["edges"] - st0["edges"]
    commits = (st1["ok"] - st0["ok"]) // 2
    ok_raw = st1["ok"] - st0["ok"]
    dropped = st1["dropped"] - st0["dropped"]
    out.append("        status         edges +%d commits +%d dropped +%d unclassified +%d latency %d ms polls-before-last %d"
               % (edges, commits, dropped, st1["unclassified"] - st0["unclassified"],
                  st1["latency"], st1["polls"]))

    if scen["expect"] == "advance":
        chk(state == "advancing", "advancing",
            "%d distinct generations of %d grabs" % (len(set(gens)), len(gens)))
        chk(edges > 0 and commits > 0, "strobed",
            "PC0 edges +%d, commits +%d" % (edges, commits))
        if scen["mode"] == "normal":
            # The invariant: in NORMAL nothing is committed without a strobe
            # (SPI3 OK moves only on commits). The exact strobes-per-pair for
            # a DROPPED record is an open question: on unit #1 (2026-09-22)
            # the raw counters fit edges = OK + dropped in all six scenarios,
            # i.e. ONE strobe per dropped pair vs two per committed one. Not
            # asserted until it is explained.
            chk(edges >= ok_raw - 2, "no-commit-unstrobed",
                "PC0 edges +%d >= SPI3 OK +%d (dropped +%d; edges-OK = %d)"
                % (edges, ok_raw, dropped, edges - ok_raw))
        if scen["auto_window"]:
            ratio = edges / commits if commits else 0.0
            chk(ratio >= AUTO_STROBED_MIN, "auto-strobed",
                "edges/commit %.2f >= %.1f over %.0f s" % (ratio, AUTO_STROBED_MIN,
                                                          scen["auto_window"]))
    elif scen["expect"] == "hold":
        chk(state == "held", "hold", "generations frozen at %d" % gens[0])
        chk(edges == 0, "no-strobe", "PC0 edges +%d with the level above the signal" % edges)
    elif scen["expect"] == "advance-negctl":
        chk(state != "held", "negctl-hold",
            "AUTO above the level ADVANCES (fallback) -- the hold metric can fail")
        chk(edges == 0, "negctl-strobe",
            "and strobes nothing (PC0 edges +%d): the advance is the fallback, not a trigger" % edges)

    if scen["check_hpos"]:
        R = state_level_record(scen)
        rows = []
        for fr in frames:
            v = np.asarray(fr["ch1"], float)
            tx, an, off = fr["tx"], fr["anchor"], fr["off"]
            at = v[off + tx] if (tx is not None and tx >= 0) else float("nan")
            prev = v[off + tx - 3] if (tx is not None and tx >= 3) else float("nan")
            rows.append((tx, an, at, at - prev, fr.get("ord")))
        if scen["check_hpos"] == "hw":
            # Criterion revised 2026-09-23 after v18 (stated in the commit):
            # the v17/v18 check demanded a hardware anchor on EVERY frame,
            # which contradicts the design's fallback for records whose seam
            # was not found. Now: every frame on the asked column; every
            # time-ordered frame hardware-anchored ON the level and rising;
            # every other frame soft-anchored; and time-ordered frames in the
            # majority (else the hardware path was not exercised at all).
            on_col = all(r[0] == scen["hpos"] for r in rows)
            ordered = [r for r in rows if r[4] == 1]
            hw_ok = all(r[1] == 2 and abs(r[2] - R) <= 6 and r[3] > 0 for r in ordered)
            fb_ok = all(r[1] != 2 for r in rows if r[4] == 0)
            desc = " ".join("%s/%s/%.0f/o%s" % (r[0], r[1], r[2], r[4]) for r in rows)
            chk(on_col, "hpos-column", "all %d frames land on column %d (%s)" % (len(rows), scen["hpos"], desc))
            chk(hw_ok and len(ordered) * 2 > len(rows), "hpos-hw",
                "%d/%d frames time-ordered; each hardware-anchored within 6 of level %d and rising" % (len(ordered), len(rows), R))
            chk(fb_ok, "hpos-fallback", "frames not time-ordered use the soft anchor")
        else:
            far = [r for r in rows if r[1] != 2 and abs(r[2] - R) > 10]
            chk(len(far) >= len(rows) - 1, "hpos-negctl",
                "un-rotation OFF: soft anchor, sample at the column off the level %d on %d/%d frames (%s) -- the check can fail"
                % (R, len(far), len(rows), " ".join("%s/%s/%.0f" % (r[0], r[1], r[2]) for r in rows)))
    if scen["check_breaks"]:
        br = [internal_breaks(np.asarray(fr["ch1"], float), fs, scen["f"]) for fr in frames]
        nb = sum(1 for b in br if b > 0)
        if scen["check_breaks"] == "on":
            chk(nb <= 1, "no-internal-break", "%d/%d records keep a phase break inside (%s) <= 1" % (nb, len(br), br))
        else:
            chk(nb >= 3, "breaks-negctl", "unrotate OFF: %d/%d records with an internal break (%s) >= 3 -- the check can fail"
                % (nb, len(br), br))
    if scen["check_order"]:
        ks = []
        for fr in frames:
            k, ok_ = host_seam(fr["ch1"])
            ks.append(k if ok_ else None)
        found = [k for k in ks if k is not None]
        if scen["check_order"] == "ordered":
            chk(len(found) >= 3 and all(k == 0 for k in found), "time-order",
                "seam at index 0 on %d/%d records with a visible seam (seams %s)" % (
                    sum(1 for k in found if k == 0), len(found), ks))
        else:
            chk(len(found) >= 3 and sum(1 for k in found if k != 0) >= len(found) - 1, "order-negctl",
                "unrotate OFF: seam away from index 0 (%s) -- the order check can fail" % ks)
    if scen["check_edge"]:
        seen = []
        for fr in frames:
            v = np.asarray(fr["ch1"], float)
            k, _ = host_seam(v)
            r = np.roll(v, -k)
            d = r[552:592].mean() - r[432:472].mean()     # +/-40..80 around the trigger
            seen.append("r" if d > 0 else "f")
        uniq = [s for i, s in enumerate(seen) if i == 0 or frames[i]["gen"] != frames[i - 1]["gen"]]
        tag = "".join(uniq)
        if scen["check_edge"] == "mixed":
            chk("r" in tag and "f" in tag, "edge-negctl",
                "filter OFF: both edges committed (%s) -- the edge check can fail" % tag)
        else:
            want = scen["check_edge"][0]
            chk(len(uniq) >= 3 and all(s == want for s in uniq), "edge",
                "%d distinct records, all %s (%s)" % (len(uniq), scen["check_edge"], tag))
    if scen["body"] and scen["wave"] == "sine":
        tears = [tear_clusters(fr["ch1"], scen["f"], fs, tol_deg=TEAR_STEP_DEG) for fr in frames]
        runs = [t[0] for t in tears]
        chk(max(runs) <= TEAR_MAX_RUNS, "body",
            "phase-step anomaly runs %s (<= %d: one seam, no tear; max step %.0f deg)"
            % (runs, TEAR_MAX_RUNS, max(t[1] for t in tears)))
        fm = float(np.mean([freq_peak(fr["ch1"], fs) for fr in frames]))
        chk(abs(fm / scen["f"] - 1.0) <= FREQ_TOL, "freq",
            "%.1f Hz vs %.1f commanded (%+.2f%%)" % (fm, scen["f"], (fm / scen["f"] - 1) * 100))
    return out, ok


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

    # host_seam: a stray single sample inside the record must not outvote the
    # seam. A committed-record shape: a 3.3-period segment (41.9 Hz at
    # 12,490 S/s) rotated so its wrap -- the seam -- lands at 887, plus one
    # stray sample at 300 whose two steps are bigger than the seam's.
    seg, _ = _synth(40.25, fs, t0=0.0, seed=9)
    rs = np.roll(seg, 887).astype(float)
    dv0 = np.abs(rs - np.roll(rs, 1))
    spiky = rs.copy(); spiky[300] = min(255.0, spiky[300] + dv0[887] + 25)
    k_raw = int(np.argmax(np.abs(spiky - np.roll(spiky, 1))))
    k_med, conf = host_seam(spiky)
    chk(dv0[887] >= 20 and k_raw in (300, 301) and k_med == 887 and conf,
        "host_seam ignores a stray sample (seam step %.0f at 887; raw max-jump picks %d; median picks %d)"
        % (dv0[887], k_raw, k_med))

    # Trigger block metrics: the tear detector on the three record shapes it
    # must tell apart. Clean = one continuous segment (0 runs); committed =
    # the same rotated, seam mid-record, at a frequency that leaves a 126 deg
    # seam step (1 run); torn = two segments spliced with a 174 deg phase
    # jump and a 170 deg wrap (2 runs).
    c1, _ = _synth(f, fs, t0=0.0, seed=7)
    runs, mx = tear_clusters(c1, f, fs)
    chk(runs == 0, "clean synthetic record: %d anomaly runs (max step %.0f deg)" % (runs, mx))
    c480, _ = _synth(480.0, fs, t0=0.0, seed=8)         # 39.35 periods: seam step 126 deg
    runs, mx = tear_clusters(np.roll(c480, 300), 480.0, fs)
    chk(runs == 1, "rotated committed record (seam at 300, 126 deg): %d run, not a tear (max step %.0f deg)" % (runs, mx))
    torn = np.r_[c1[:600], np.roll(c1, 137)[600:]]      # 174 deg jump at 600, 170 deg at the wrap
    runs, mx = tear_clusters(torn, f, fs)
    chk(runs >= 2, "torn record (tear at 600 + wrap): %d runs > %d (metric detects a tear; max step %.0f deg)"
        % (runs, TEAR_MAX_RUNS, mx))
    chk(gens_state([10, 10, 10, 10, 10]) == "held", "gens 10x5 -> held")
    chk(gens_state([10, 12, 12, 14, 16]) == "advancing", "gens 10,12,12,14,16 -> advancing")
    chk(gens_state([10, 10, 10, 10, 12]) == "ambiguous", "gens one new of five -> ambiguous (not 'advancing')")

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
    ap.add_argument("--trigger-only", action="store_true",
                    help="skip the display matrix; run the trigger/acquisition block")
    ap.add_argument("--no-trigger", action="store_true",
                    help="run the display matrix only")
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
    for scen in ([] if a.trigger_only else scenarios()):
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

    # -- trigger / acquisition block --------------------------------------
    if not a.no_trigger:
        print("\n===== TRIGGER / ACQUISITION BLOCK (EXP-54 arms) =====")
        print("setup: range 5 CH1, level 0, AUTO; JDS CH1 only")
        for c in ("fpga pollgap", "fpga postedge", "fpga autowait"):
            print("  " + next((l.strip() for l in sc.cmd(c).splitlines()
                                if l.strip() and not l.strip().startswith(">") and c not in l), "?")[:110])
        sc.trigger_mode("auto")
        sc.trigger_level(0)
        sc.timebase(0x10)
        sc.vdiv(1, 5)
        sg.waveform("sine", 1); sg.freq(201.2, 1); sg.amp(2.0, 1); sg.offset(0.0, 1)
        sg.output(True, True)
        time.sleep(0.8)
        # Precondition, checked by readback: centre range 5 with the drive on,
        # then require a record that is in range. The first run of this block
        # (2026-09-22) skipped centring and read CH1 pinned at code 227 --
        # every check then failed for a reason that had nothing to do with the
        # acquisition loop. A railed record makes the block VOID, not FAIL.
        dac, mid = centre_ch1_host(sc)
        print("  host centre: DAC1=%d -> record midline %.0f (target 114)" % (dac, mid))
        v = np.asarray(fresh_frame(sc)["ch1"], float)
        pre_ok = v.min() > 5 and v.max() < 222 and (v.max() - v.min()) > 40
        print("  precondition: CH1 record %d..%d span %d -> %s"
              % (v.min(), v.max(), v.max() - v.min(),
                 "OK" if pre_ok else "VOID (railed or no signal)"))
        if not pre_ok:
            results.append(("trigger block precondition", False))
        tstate = {"tb": 0x10, "mode": "auto", "level": 0,
                  "drive": ("sine", 201.2, 2.0, 0.0)}
        for scen in (trigger_scenarios() if pre_ok else []):
            if a.only and a.only not in scen["name"]:
                continue
            fs = rates[scen["tb"]]
            print("\n== %s  (fs %.0f S/s, %s, level %+d) ==" % (scen["name"], fs, scen["mode"], scen["level"]))
            apply_trigger_scenario(sc, sg, scen, tstate)
            run = run_trigger_scenario(sc, scen)
            if a.save:
                key = "TRIG_" + scen["name"].replace(" ", "_")
                saved[key + "__ch1"] = np.stack([fr["ch1"] for fr in run["frames"]])
                saved[key + "__gen"] = np.array([fr["gen"] for fr in run["frames"]])
            lines, ok = eval_trigger_scenario(scen, run, fs)
            print("\n".join(lines))
            results.append((scen["name"], ok))
        sc.trigger_mode("auto")
        sc.trigger_level(0)
        sc.timebase(0x10)
        sc.vdiv(1, 6)

    if a.save and any(k.startswith("TRIG_") for k in saved):
        np.savez_compressed(a.save, **saved)
        print("\ntrigger-block frames saved to %s" % a.save)

    sg.output(False, False)

    print("\n===== EXP-22 SUMMARY =====")
    for name, ok in results:
        print("  %-22s %s" % (name, "PASS" if ok else "FAIL"))
    total_ok = all(ok for _, ok in results)
    print("OVERALL: %s" % ("PASS -- trace holds still under phase/amp/freq "
                           "changes on both channels; acquisition loop "
                           "triggers, holds and single-shots as specified"
                           if total_ok else "FAIL"))
    sys.exit(0 if total_ok else 1)


if __name__ == "__main__":
    main()
