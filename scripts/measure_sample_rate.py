#!/usr/bin/env python3
"""
Measure the scope's sample rate for a given reg-0x01 timebase code.

METHOD
------
Drive tones of known frequency, find the spectral peak with a SEARCH (never a
fixed bin), and fit peak-bin against frequency through the origin:

    bin = f * 1024/fs     ->     fs = 1024 / slope

The R2 of that fit is the real output. A high R2 says the record is a
uniformly-sampled time series and the rate is trustworthy; a low one says the
number that came out is not a measurement, whatever it looks like. Rates are
always reported WITH their R2, for exactly that reason.

TWO CONSTRAINTS THAT WILL BITE, both confirmed on the bench (EXP-10)
--------------------------------------------------------------------
1. `spi3 opread` clocks at /256 and takes ~35 ms per window, so at >=30 kS/s the
   engine LAPS the buffer while the read is still running and the record is
   torn. Use the acq-task buffer (`spi3 read 1024`, filled by a /2 read) above
   0x0F. This script measures BOTH paths at 0x10 as a control: they must agree,
   or the acq path is looking at something else and its fast-code numbers mean
   nothing.
2. Tones must sit well inside the band. Too high and they alias -- code 0x08
   runs near 1-2 kS/s, so its Nyquist is under 1 kHz. Too low and they fall
   below bin 1 -- at 15 kS/s anything under ~15 Hz is sub-bin and the "peak"
   found is drift. The ESP32 source tops out near 4.5 kHz, which is why codes
   0x0C and faster cannot be measured with it at all: their tones land in bins
   1-15 and the fit measures quantisation, not rate. Those need a
   higher-frequency generator.

THE FOLD TEST IS NOT OPTIONAL (EXP-12)
--------------------------------------
An in-band R2 is NOT sufficient evidence for a sample rate. At reg 0x01 = 0x08
a 14-tone sweep returned R2 0.9304 -- which reads as a decent fit -- on a record
that is demonstrably incoherent: three consecutive reads of one unchanged tone
gave peak bins [171, 132, 104], and two passes of the same sweep disagreed by
7.6%.

What caught it is the fold check. Above Nyquist a tone aliases to
|f - round(f/fs)*fs|, a bin position FAR more sensitive to fs than the in-band
slope is. A line through noise that happens to trend will pass the R2 test and
fail this one. At 0x08 the predictions missed by up to 227 bins; the same test
at 0x10, where fs is known to 0.5%, missed by at most 8.

So `fit()` here reports read-to-read scatter, and `fold_check()` should be run
before any rate is adopted.

WHY THIS SCRIPT EXISTS
----------------------
The previous answer to "does the time axis work" was "no" -- recorded in four
files -- and it was a double FFT in a throwaway analysis script (EXP-10). A
measurement worth trusting is one that can be re-run.

USAGE
    python3 scripts/measure_sample_rate.py
"""
import statistics
import sys, time
sys.path.insert(0, "/home/david/osc/scripts")
import numpy as np
from bench import Scope, Siggen, peaks, parse_dump

SETTLE = 1.0
RANGE = 6
AMP = 2000

sc = Scope()
sg = Siggen()

# ── the source must be told to deliver what it is asked for ──────────────
#
# Until 2026-08-19 this script fitted against the COMMANDED frequency while
# the generator delivered 0.8250x that, so every rate it produced was 1.212x
# too high -- including the 14,853 S/s published for code 0x10.
#
# Two things have to be right, and the second one bit on the first attempt:
#   1. the generator must divide by its MEASURED loop rate, not by FS=40000;
#   2. that rate must be measured in the SAME CHANNEL CONFIGURATION the sweep
#      runs in.  Each channel in a waveform mode costs ~300 Hz of loop rate,
#      so measuring with CH2 parked and then sweeping with it live (or the
#      reverse) builds in a 0.9% error -- which is larger than the gap we are
#      trying to resolve against Stlkv's rig.
#
# So: park CH2 and put CH1 in the mode the sweep uses FIRST, then measure.
sg.off(2)
sg.sine(1000, ch=1)
_fs_hz, _fs_ratio = sg.fs(window=8.0)
_div = sg.use_measured_fs(True, window=0.0)
print(f"source: CH1 sine + CH2 parked -> DDS loop {_fs_hz:.1f} Hz "
      f"({_fs_ratio:.4f} x nominal); set_freq divides by {_div:.1f}", flush=True)
if not 0.5 < _fs_ratio < 1.05:
    raise SystemExit("source rate ratio %.4f is not credible — stop and look"
                     % _fs_ratio)

print("build:", sc.version().strip().replace("\r\n", " | "), flush=True)

sc.scope_range(RANGE, 1)
sc.scope_range(RANGE, 2)
sc.cmd(f"fpga scope center ch1 {RANGE}", timeout=20)
sg.off(2)
time.sleep(SETTLE)


def read_opread():
    return sc.opread(0x04)


def read_acq():
    """The acquisition task's CH1 buffer — filled by a /2 read, so it is a far
    tighter snapshot than a /256 opread."""
    txt = sc.cmd("spi3 read 1024", timeout=15)
    v = parse_dump(txt)
    if v.size < 1024:
        raise RuntimeError(f"acq dump short: {v.size}")
    return v.astype(float)


def fold_check(fs, reader, tones, tag):
    """Verify that above-Nyquist tones land where `fs` says they must.

    This is the test that separates a real sample rate from a plausible fit.
    Returns the worst miss in bins; <= 12 is a pass at these record lengths.
    """
    print(f"    {tag} fold check (Nyquist {fs/2:.0f} Hz):", flush=True)
    misses = []
    for f in tones:
        if f <= fs / 2:
            continue
        k = round(f / fs)
        pred = int(round(1024.0 * abs(f - k * fs) / fs))
        sg.sine(f, ch=1)
        sg.amp(AMP, ch=1)
        time.sleep(SETTLE)
        b = int(statistics.median(
            [peaks(reader(), 1)[0][0] for _ in range(3)]))
        misses.append(abs(b - pred))
        print(f"      {f:7.0f} Hz  predicted {pred:4d}  measured {b:4d}  "
              f"miss {abs(b - pred):4d}", flush=True)
    if not misses:
        print("      (no tones above Nyquist — fold not tested)", flush=True)
        return None
    worst = max(misses)
    print(f"      worst miss {worst} bins -> "
          f"{'FOLD HOLDS' if worst <= 12 else 'FOLD FAILS, rate not trustworthy'}",
          flush=True)
    return worst


def fit(freqs, reader, tag):
    rows = []
    for f in freqs:
        sg.sine(f, ch=1)
        sg.amp(AMP, ch=1)
        time.sleep(SETTLE)
        # Three reads, so read-to-read scatter is visible. An incoherent
        # record still produces a bin; only the spread reveals it.
        got = [peaks(reader(), 1)[0] for _ in range(3)]
        bins = [g[0] for g in got]
        b = int(statistics.median(bins))
        m = float(statistics.median([g[1] for g in got]))
        spread = max(bins) - min(bins)
        if spread > 20:
            print(f"      !! {f:.0f} Hz: reads {bins} spread {spread} bins — "
                  "this record is not reproducing itself", flush=True)
        rows.append((f, b, m))
    fa = np.array([r[0] for r in rows], float)
    ba = np.array([r[1] for r in rows], float)
    ma = np.array([r[2] for r in rows], float)
    keep = ma > 1.0
    if keep.sum() < 3:
        print(f"    {tag}: only {int(keep.sum())} usable points", flush=True)
        return None
    slope = float((fa[keep] * ba[keep]).sum() / (fa[keep] ** 2).sum())
    resid = ba[keep] - slope * fa[keep]
    dd = ba[keep] - ba[keep].mean()
    r2 = 1.0 - float((resid ** 2).sum()) / float((dd ** 2).sum()) \
        if float((dd ** 2).sum()) > 0 else float("nan")
    fs = 1024.0 / slope if slope > 0 else float("nan")
    detail = "  ".join(f"{int(r[0])}->{r[1]}" for r in rows)
    print(f"    {tag}: fs = {fs:9.0f} S/s  R2 {r2:+.4f}   [{detail}]", flush=True)
    return fs, r2


# ── 1. code 0x08 refitted below its own Nyquist ──────────────────────────
print("\n=== 1. reg 0x01 = 0x08, sub-Nyquist tones ===", flush=True)
sc.timebase(0x08)
time.sleep(SETTLE)
fit([40, 80, 130, 200, 300, 420], read_opread, "0x08 opread")

# ── 2. control: both read paths must agree at 0x10 ───────────────────────
print("\n=== 2. CONTROL — the two read paths at 0x10 ===", flush=True)
sc.timebase(0x10)
time.sleep(SETTLE)
a = fit([250, 500, 1000, 2000, 3500], read_opread, "0x10 opread")
b = fit([250, 500, 1000, 2000, 3500], read_acq, "0x10 acq   ")
if a and b:
    d = abs(a[0] - b[0]) / a[0]
    print(f"    paths agree to {d*100:.1f}%  "
          f"{'PASS' if d < 0.05 else 'FAIL — acq path measures something else'}",
          flush=True)

# ── 3. the fast codes through the acq buffer ─────────────────────────────
print("\n=== 3. fast codes, acq-buffer read ===", flush=True)
for code in (0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A):
    sc.timebase(code)
    time.sleep(SETTLE)
    print(f"  reg 0x01 = 0x{code:02X}", flush=True)
    fit([500, 1000, 2000, 3500, 4500], read_acq, f"  0x{code:02X} acq")

# ── closing control: did the source hold its rate for the whole run? ─────
sg.sine(1000, ch=1)
_fs_end, _r_end = sg.fs(window=8.0)
_drift = abs(_fs_end - _fs_hz) / _fs_hz
print(f"\nsource at end: {_fs_end:.1f} Hz ({_r_end:.4f}) — drift {_drift*100:.2f}%  "
      f"{'PASS' if _drift < 0.002 else 'FAIL — rates above are not traceable'}",
      flush=True)

sg.off(1)
sc.timebase(0x10)
print("\nsiggen off; timebase restored to 0x10.")
print("done")
