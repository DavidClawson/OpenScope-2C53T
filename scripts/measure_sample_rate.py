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
before any rate is adopted. It now runs after every fit whenever the source can
reach above Nyquist (on by default except for the ESP32, whose historical run
did not include it; --fold / --no-fold).

SOURCES (--source, see scripts/README-bench.md)
-----------------------------------------------
esp32 (default)  The historical run, unchanged: the loop rate is measured and
                 adopted first, sections 1-3 below, the drift check last.
kodedot          A Kode Dot running sigsrc: LEDC square, 1 Hz - 10 MHz, from
                 the 40 MHz crystal, and it REPORTS the frequency its timer
                 registers produce -- the fit uses that, never the request.
                 This is the source that can reach 0x0A-0x0C, and the fold test
                 above them. A square's odd harmonics alias too, at a third of
                 the fundamental or less, so tones are kept at or below 0.2 fs
                 (by the guess) and the peak search finds the fundamental.
manual           Any generator plus a counter: the operator sets each tone and
                 types the frequency the counter reads.

With --codes, tones are placed from the rate the code is expected to have:
the measured table in scope_timebase.c where it has one, otherwise the 1-2.5-5
ladder continued (0x0C ~250 kS/s, 0x0B ~500 kS/s, 0x0A ~1.25 MS/s -- GUESSES)
and EXP-15's ~1.25 kS/s cluster for the incoherent 0x06-0x09. The placement
(1-20 % of the guess) tolerates a guess wrong by 2x either way; --fs-guess
CODE=HZ overrides one, --tones overrides them all.

WHY THIS SCRIPT EXISTS
----------------------
The previous answer to "does the time axis work" was "no" -- recorded in four
files -- and it was a double FFT in a throwaway analysis script (EXP-10). A
measurement worth trusting is one that can be re-run.

USAGE
    python3 scripts/measure_sample_rate.py                      # historical run, ESP32
    python3 scripts/measure_sample_rate.py --source kodedot --codes 0x0A 0x0B 0x0C
    python3 scripts/measure_sample_rate.py --source kodedot --codes 0x06 0x07 0x08 0x09 \\
        --path opread --tones 40 80 130 200 300 420
    python3 scripts/measure_sample_rate.py --source kodedot --codes 0x0C --dry-run
"""
import argparse
import os
import re
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from bench import (BenchError, add_source_args, open_bench, parse_dump,  # noqa: E402
                   peaks)

TB_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      "..", "firmware", "src", "ui", "scope_timebase.c")

SETTLE = 1.0
RANGE = 6
AMP = 2000

# The historical run's tone sets (ESP32, <= 4.5 kHz).
SUBNYQ_TONES = [40, 80, 130, 200, 300, 420]       # 0x08, and EXP-15's 0x06-0x09
CONTROL_TONES = [250, 500, 1000, 2000, 3500]      # 0x10, both read paths
FAST_TONES = [500, 1000, 2000, 3500, 4500]        # 0x0F..0x0A through acq
FAST_CODES = (0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A)
INCOHERENT_CODES = (0x06, 0x07, 0x08, 0x09)

#: Rates used ONLY to place tones where scope_timebase.c has none. Never
#: reported as a result. 0x0A-0x0C continue the measured 1-2.5-5 ladder down
#: from 0x0D; 0x06-0x09 are EXP-15's incoherent fits, which clustered near
#: 0x13's 1,250 S/s whatever the code.
LADDER_GUESS = {0x06: 1250.0, 0x07: 1250.0, 0x08: 1250.0, 0x09: 1250.0,
                0x0A: 1.25e6, 0x0B: 5e5, 0x0C: 2.5e5}
#: Tone placement as fractions of the expected rate: bins ~10..205 of 512, and
#: still below 0.4 fs if the true rate is half the guess.
FIT_FRACTIONS = (0.01, 0.02, 0.05, 0.1, 0.2)
#: Fold-test tones as multiples of the FITTED rate: all above Nyquist, aliasing
#: to bins ~389 / 195 / 133 / 379 / 297 -- none near 0 or 512, none shared.
FOLD_MULTIPLES = (0.62, 0.81, 1.13, 1.37, 1.71)
FOLD_PASS_BINS = 12


def load_rates(path=TB_SRC):
    """{code: S/s} from scope_timebase.c's sample_rate[] (0.0 = no rate).

    Read from the source for the reason verify_scope_cal.py reads its table:
    a copy here would go stale the first time the ladder is re-measured."""
    with open(path) as f:
        src = f.read()
    m = re.search(r"sample_rate\[SCOPE_TIMEBASE_CODE_COUNT\]\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        raise SystemExit(f"could not find sample_rate[] in {path}")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    vals = [float(x) for x in re.findall(r"(\d+\.\d+)f", body)]
    if len(vals) != 21:
        raise SystemExit(f"expected 21 rates in sample_rate[], parsed {len(vals)}")
    return dict(enumerate(vals))


def nice_hz(x):
    """Round to two significant figures, whole hertz, at least 1 Hz."""
    if x < 1:
        return 1
    e = int(np.floor(np.log10(x))) - 1
    return max(1, int(round(x / 10 ** e) * 10 ** e))


def tones_for(guess, max_hz=None):
    out = sorted({nice_hz(frac * guess) for frac in FIT_FRACTIONS})
    return [f for f in out if max_hz is None or f <= max_hz]


def fold_tones(fs, max_hz=None):
    out = [nice_hz(m * fs) for m in FOLD_MULTIPLES]
    return [f for f in out if max_hz is None or f <= max_hz]


def default_path(code):
    """The historical choice: opread for the slow codes and the incoherent band
    (0x08 was always read that way), the acq buffer for 0x0A-0x0F."""
    return "acq" if 0x0A <= code <= 0x0F else "opread"


def _code(s):
    v = int(s, 0)
    if not 0 <= v <= 0x14:
        raise argparse.ArgumentTypeError("timebase codes are 0x00..0x14, got %s" % s)
    return v


def _guess(s):
    try:
        c, hz = s.split("=", 1)
        return _code(c), float(hz)
    except ValueError:
        raise argparse.ArgumentTypeError("--fs-guess wants CODE=HZ, e.g. 0x08=5e6")


def build_parser():
    ap = argparse.ArgumentParser(
        description="Fit peak bin against a known tone frequency, per timebase code.",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--codes", type=_code, nargs="+", default=None, metavar="CODE",
                    help="reg-0x01 codes to measure, e.g. 0x0A 0x0B (default: the "
                         "historical set 0x08, 0x0F..0x0A)")
    ap.add_argument("--path", choices=("auto", "opread", "acq", "both"), default="auto",
                    help="read path for --codes (auto: acq for 0x0A-0x0F, opread "
                         "otherwise)")
    ap.add_argument("--tones", type=float, nargs="+", default=None, metavar="HZ",
                    help="fit tones for every --codes code (default: placed from the "
                         "expected rate)")
    ap.add_argument("--fs-guess", type=_guess, action="append", default=[],
                    metavar="CODE=HZ", help="expected rate for tone placement only")
    ap.add_argument("--fold", dest="fold", action="store_true", default=None,
                    help="run the fold test after each fit (default: on, except esp32)")
    ap.add_argument("--no-fold", dest="fold", action="store_false")
    ap.add_argument("--no-control", action="store_true",
                    help="skip the two-path control at 0x10 (it is the only check "
                         "that the acq path sees the same record)")
    ap.add_argument("--range", type=int, default=RANGE,
                    help="frontend range for both channels (default: %(default)s)")
    ap.add_argument("--settle", type=float, default=SETTLE,
                    help="seconds after each change (default: %(default)s)")
    add_source_args(ap)
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.codes is None and (args.tones or args.fs_guess or args.path != "auto"):
        raise SystemExit("--tones / --fs-guess / --path apply to --codes; give --codes")
    if args.tones and args.source == "esp32" and max(args.tones) > 4500:
        raise SystemExit("--source esp32 tops out near 4.5 kHz (software DDS); tones "
                         "above that need --source kodedot or a manual generator")
    rates = load_rates()
    sleep = (lambda _s: None) if args.dry_run else time.sleep
    sc, src, _sim = open_bench(args)
    try:
        run(args, sc, src, rates, sleep)
    finally:
        src.close()
        sc.close()


def run(args, sc, src, rates, sleep):
    def log(msg):
        print(msg, flush=True)

    fold_on = args.fold if args.fold is not None else src.kind != "esp32"

    # ── the source must be told to deliver what it is asked for ──────────
    #
    # Until 2026-08-19 this script fitted against the COMMANDED frequency while
    # the generator delivered 0.8250x that, so every rate it produced was 1.212x
    # too high -- including the 14,853 S/s published for code 0x10. The ESP32
    # now measures and adopts its own loop rate (in the channel configuration
    # the sweep runs in: each live channel costs ~300 Hz of it); the Kode Dot
    # reports the frequency its timer registers produce; a manual generator is
    # read off a counter. Either way, the fit below uses what the source
    # REPORTS, never the number it was asked for.
    src.prepare_frequency(log)

    log("build: " + sc.version().strip().replace("\r\n", " | "))

    sc.scope_range(args.range, 1)
    sc.scope_range(args.range, 2)
    if src.center_on_quiet:
        src.quiet()
    sc.cmd(f"fpga scope center ch1 {args.range}", timeout=60)  # 20.4 s measured on unit #3 (EXP-63)
    sleep(args.settle)

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

    readers = {"opread": read_opread, "acq": read_acq}

    def fold_check(fs, reader, tones, tag):
        """Verify that above-Nyquist tones land where `fs` says they must.

        This is the test that separates a real sample rate from a plausible fit.
        Returns the worst miss in bins; <= 12 is a pass at these record lengths.
        """
        log(f"    {tag} fold check (Nyquist {fs/2:.0f} Hz):")
        misses = []
        for f in tones:
            if f <= fs / 2:
                continue
            fa = src.tone(f)
            sleep(args.settle)
            k = round(fa / fs)
            pred = int(round(1024.0 * abs(fa - k * fs) / fs))
            b = int(statistics.median(
                [peaks(reader(), 1)[0][0] for _ in range(3)]))
            misses.append(abs(b - pred))
            log(f"      {fa:7.0f} Hz  predicted {pred:4d}  measured {b:4d}  "
                f"miss {abs(b - pred):4d}")
        if not misses:
            log("      (no tones above Nyquist — fold not tested)")
            return None
        worst = max(misses)
        log(f"      worst miss {worst} bins -> "
            f"{'FOLD HOLDS' if worst <= FOLD_PASS_BINS else 'FOLD FAILS, rate not trustworthy'}")
        return worst

    def fit(freqs, reader, tag):
        rows = []
        for f in freqs:
            fa = src.tone(f)            # what the source REPORTS it is generating
            sleep(args.settle)
            # Three reads, so read-to-read scatter is visible. An incoherent
            # record still produces a bin; only the spread reveals it.
            got = [peaks(reader(), 1)[0] for _ in range(3)]
            bins = [g[0] for g in got]
            b = int(statistics.median(bins))
            m = float(statistics.median([g[1] for g in got]))
            spread = max(bins) - min(bins)
            if spread > 20:
                log(f"      !! {fa:.0f} Hz: reads {bins} spread {spread} bins — "
                    "this record is not reproducing itself")
            rows.append((fa, b, m))
        fa = np.array([r[0] for r in rows], float)
        ba = np.array([r[1] for r in rows], float)
        ma = np.array([r[2] for r in rows], float)
        keep = ma > 1.0
        if keep.sum() < 3:
            log(f"    {tag}: only {int(keep.sum())} usable points")
            return None
        slope = float((fa[keep] * ba[keep]).sum() / (fa[keep] ** 2).sum())
        resid = ba[keep] - slope * fa[keep]
        dd = ba[keep] - ba[keep].mean()
        r2 = 1.0 - float((resid ** 2).sum()) / float((dd ** 2).sum()) \
            if float((dd ** 2).sum()) > 0 else float("nan")
        fs = 1024.0 / slope if slope > 0 else float("nan")
        detail = "  ".join(f"{r[0]:.0f}->{r[1]}" for r in rows)
        log(f"    {tag}: fs = {fs:9.0f} S/s  R2 {r2:+.4f}   [{detail}]")
        return fs, r2

    def control():
        sc.timebase(0x10)
        sleep(args.settle)
        a = fit(CONTROL_TONES, read_opread, "0x10 opread")
        b = fit(CONTROL_TONES, read_acq, "0x10 acq   ")
        if a and b:
            d = abs(a[0] - b[0]) / a[0]
            log(f"    paths agree to {d*100:.1f}%  "
                f"{'PASS' if d < 0.05 else 'FAIL — acq path measures something else'}")
        return a, b

    def fold_after(res, reader, tag):
        if fold_on and res and np.isfinite(res[0]) and res[0] > 0:
            fold_check(res[0], reader, fold_tones(res[0], src.max_hz), tag)

    if args.codes is None and src.kind == "esp32":
        # ── the historical run, unchanged ────────────────────────────────
        log("\n=== 1. reg 0x01 = 0x08, sub-Nyquist tones ===")
        sc.timebase(0x08)
        sleep(args.settle)
        res = fit(SUBNYQ_TONES, read_opread, "0x08 opread")
        fold_after(res, read_opread, "0x08 opread")

        log("\n=== 2. CONTROL — the two read paths at 0x10 ===")
        control()

        log("\n=== 3. fast codes, acq-buffer read ===")
        for code in FAST_CODES:
            sc.timebase(code)
            sleep(args.settle)
            log(f"  reg 0x01 = 0x{code:02X}")
            res = fit(FAST_TONES, read_acq, f"  0x{code:02X} acq")
            fold_after(res, read_acq, f"  0x{code:02X} acq")
    else:
        codes = args.codes or [0x08] + list(FAST_CODES)
        guesses = dict(args.fs_guess)
        if not args.no_control:
            log("\n=== CONTROL — the two read paths at 0x10 ===")
            control()
        for code in codes:
            path = default_path(code) if args.path == "auto" else args.path
            paths = ("opread", "acq") if path == "both" else (path,)
            table = rates.get(code, 0.0)
            if code in guesses:
                guess, why = guesses[code], "--fs-guess"
            elif table > 0:
                guess, why = table, "scope_timebase.c"
            else:
                guess, why = LADDER_GUESS.get(code, 12500.0), "ladder GUESS"
            if args.tones:
                tones = [int(round(t)) if src.kind == "kodedot" else t for t in args.tones]
            elif code in INCOHERENT_CODES and code not in guesses:
                tones = SUBNYQ_TONES              # comparable with EXP-12/EXP-15
            else:
                tones = tones_for(guess, src.max_hz)
            log(f"\n=== reg 0x01 = 0x{code:02X}, {'+'.join(paths)} read ===")
            log(f"  table: {'%.1f S/s' % table if table > 0 else 'no rate (0.0f)'};"
                f" tones placed for ~{guess:.0f} S/s ({why})")
            if len(tones) < 3:
                log(f"  only {len(tones)} tone(s) within this source's range — "
                    "not enough to fit; skipped")
                continue
            sc.timebase(code)
            sleep(args.settle)
            results = {}
            for p in paths:
                results[p] = fit(tones, readers[p], f"0x{code:02X} {p:<6}")
            if len(paths) == 2 and all(results.values()):
                d = abs(results["opread"][0] - results["acq"][0]) / results["opread"][0]
                log(f"    paths agree to {d*100:.1f}%")
            for p in paths:
                res = results[p]
                if res and table > 0 and np.isfinite(res[0]):
                    log(f"    {p} vs table {table:.1f} S/s: {(res[0] / table - 1) * 100:+.2f}%")
                fold_after(res, readers[p], f"0x{code:02X} {p:<6}")

    # ── closing control: did the source hold its rate for the whole run? ─────
    if src.end_check(log) is None:
        log("\nsource at end: no closing rate check for this source — re-read your "
            "counter now; the rates above are only as good as what was typed")

    src.quiet()
    sc.timebase(0x10)
    log(f"\n{src.label} off; timebase restored to 0x10.")
    log("done")


if __name__ == "__main__":
    try:
        main()
    except BenchError as exc:
        raise SystemExit(f"bench: {exc}")
