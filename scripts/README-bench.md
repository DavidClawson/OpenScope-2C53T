# `scripts/bench.py` — the shared bench measurement library

Every experiment on this bench used to open its own serial port, hand-roll its
own hex-dump regex, and hand-roll its own "is this array long enough" check.
That is how the measurement bugs got in. This module is the one copy of all of
it.

It talks to the scope and to one of several stimulus sources:

| device | port (typical) | what it is |
|---|---|---|
| `Scope` | `/dev/ttyACM0`, or discovered by USB id `2e3c:5740` | the 2C53T's USB CDC debug shell |
| `Siggen` | `/dev/ttyUSB0` | the ESP32 bench source, [`esp32_siggen/`](../esp32_siggen/) |
| `KodeDotSource` | discovered by Espressif VID `0x303A` | a Kode Dot (ESP32-P4) running the sigsrc app: crystal-derived LEDC square, 1 Hz – 10 MHz, 0 V / 3V3 |
| `ManualSource` | — (the keyboard) | any generator; the operator types what a counter / DMM reads |

Dependencies: `python3`, `numpy`, `pyserial`. It imports cleanly with no device
attached — nothing opens a port until you construct a device.

```bash
python3 scripts/bench.py --selftest    # 71 checks, no hardware needed
python3 scripts/test_bench_source.py   # the source abstraction and both scripts' CLIs
```

Run that after touching the module. It exercises the dump parser, the framing
rule, the spectral detectors, the paired statistics and the verdict logic
against synthetic data, so the *instrument* is testable even when the bench is
busy.

---

## The bench regression — run before every release

```bash
python3 scripts/bench_regression.py            # all suites, ~30 min, unit + JDS6600 attached
python3 scripts/bench_regression.py --only mask
```

One command runs every hardware acceptance that guards a shipped feature:
`exp22_stability.py --trigger-only` (trigger modes, edge, position, record
integrity), `mask_bench.py` (mask pass/fail, EXP-69) and `coupling_bench.py`
(AC/DC relays and probe 1X/10X, EXP-70). Each suite's verdict is its own exit
code; the runner only refuses to call the run green when a suite failed,
crashed, timed out, or the device's build line changed mid-run (a reset).
Logs, one per suite plus `SUMMARY.txt`, go to
`reverse_engineering/captures/regression/<date>_<build>/`.

Wiring: JDS6600 CH1 → scope CH1, CH2 → CH2. Do DC-level work on JDS CH1 only;
its CH2 offset is unreliable (see the `JDS6600` docstring).

A new acceptance script joins `SUITES` in `bench_regression.py` when its
feature ships. Until 2026-10-03 "the regression" was exp22 alone, by
convention, and later acceptances were never re-run before a release.

---

## The one design idea

This project's characteristic failure is not a bad hypothesis. It is an
instrument that returns a **stable, plausible, wrong number** because it could
not detect what it claimed to: SSPI reads at `/2`, a floating MISO, Exp F
diffing output *levels* when the difference was in *configuration*, a fixed-bin
DFT reporting 1.44 where a peak search found 22.8, `fpga scope range 5 0`
silently addressing both channels, three header bytes discarded where stock
discards two.

So the library is built so that **controls are easy and uncontrolled negatives
are awkward**:

- `Result` has **no settable verdict**. It is computed from the controls
  attached to the result. A `NEGATIVE` with no passing control renders as
  `VOID`, and there is no flag to override that.
- `band(v, lo, hi)` refuses `hi <= lo`. A zero-width window *is* a fixed-bin
  detector. `fixed_bin()` exists only to raise and explain.
- `parse_dump()` verifies each line's declared offset against the running byte
  count, so a dropped USB CDC line raises instead of shifting every later
  sample.
- `Scope.opread()` raises `ShortReadError` rather than returning a truncated
  window.
- `Scope.scope_range(n, ch)` requires an explicit channel, because the shell's
  argument is 1-based and `0` means "both".
- `Siggen` setters parse the device's echo and raise if the device did not adopt
  what was asked. Assuming a serial write took effect is how `usart tx` frames
  got "sent" into a task that never existed.

---

## Public API

```python
from bench import (
    Scope, Siggen,                                   # devices
    SignalSource, Esp32Source, KodeDotSource, ManualSource,   # stimulus contract
    add_source_args, open_bench, find_port, SimBench,         # --source / --dry-run
    vpp_from_reading, expected_span_counts,                   # waveform-aware amplitude
    spectrum, peaks, band, band_peak, window_for, bin_of,
    paired_difference, paired_control, paired_experiment,
    Experiment, Result, Control, Verdict,
    BenchError, ShortReadError, PromptTimeout,
)
```

### `Scope(port="/dev/ttyACM0")`

| method | notes |
|---|---|
| `.cmd(line, timeout=3.0)` | send a shell line, read to the `>` prompt; raises `PromptTimeout` instead of returning a partial reply |
| `.opread(op, n=1026, drop=2)` | one `spi3 opread <op> <n> dump` window as a float array — **1024 samples** |
| `.opread_stats(op, n)` | the device's own `min/max/mean/span` line, parsed; cheap canary |
| `.reader(op)` | a zero-arg closure, ready for `paired_difference` |
| `.seq(*bytes, "|")` | `spi3 seq`, CS-framed; `"|"` inserts a mid-sequence CS pulse |
| `.timebase(i)` / `.trigger_level(code)` | named wrappers for registers `0x01` / `0x08` |
| `.gpio("E4", 1)` | `gpio set`, with pin/level validation |
| `.scope_range(n, ch)` | frontend range on **one** channel; `.scope_range_both(n)` if you mean both |
| `.version()` | record it — which build was running is part of the measurement |

**Framing.** `drop` defaults to `2`. Stock's `op04`/`op05` handlers discard
exactly two bytes (opcode echo + one dummy) then capture 1024 samples. This
project discarded three and kept 1023 for months, so every array was shifted one
byte late and one sample short; the third byte had been misread as a
"buffer-valid flag" when it is in fact `sample[0]`. Fixed in `ce22b49` and
independently confirmed on a second unit. Change this only with evidence.

**Hazards the library cannot protect you from.** Never read Gowin `STATUS`
(`0x41`) on a configured part during capture — it desynchronises the running
design and only a true power cycle recovers it (POWER → "Goodbye" → **unplug
USB** → replug; the pinhole reset does *not* remove FPGA power). Opcodes
`0x01/0x02/0x06/0x07/0x08` are register **writes** on the user design;
`Scope.WRITE_OPCODES` lists them so a sweep can skip them.

### `Siggen(port="/dev/ttyUSB0")`

`.sine(hz, ch)` `.square(hz, duty, ch)` `.tri` `.saw` `.dc(mv, ch)` `.amp(mvpp, ch)`
`.off(ch)` `.phase(deg, ch)` `.pwm(hz, duty)` `.pwm_off()` `.status()`

`.status()` returns `{1: SiggenStatus, 2: SiggenStatus, "pwm": PwmStatus}` and
raises if either channel is missing, rather than handing back a half-populated
dict. `SiggenStatus.freq_hz` is what the ESP32 *believes* it is generating — on
bench unit #1 a nominal 100 Hz read 82 Hz and 250 Hz read 208 Hz, the same 0.82
factor on both channels. Derive the real rate from the capture.

Two different waveform **shapes** beat an anti-phase pair as a two-channel test:
a display that inverts or rescales can make anti-phase look like one source
drawn twice. `sg.tri(500, ch=1); sg.square(500, ch=2)`.

### Signal sources — `--source esp32|kodedot|manual`

The calibration scripts used to be welded to the ESP32 on `/dev/ttyUSB0`.
`SignalSource` is the part of a stimulus a script uses, with the `Siggen`
contract kept: **every value returned is what the source reports (or what the
operator's instruments read), and a setter that cannot confirm raises.**

| method | notes |
|---|---|
| `.tone(hz)` | set a tone; returns the ACTUAL Hz, which is what a fit must use (EXP-14: fitting the request made every rate 1.21x high) |
| `.drive(mvpp, waveform, hz)` | set a drive level; returns the reference Vpp in mV (commanded for esp32, the DMM-measured rail for kodedot, the typed reading for manual) |
| `.quiet()` / `.hold(0\|1)` | quiet output; `hold` only where `can_hold` (kodedot) |
| `.freq_hz` | last REPORTED frequency |
| `.prepare_frequency()` / `.end_check()` | esp32: measure and adopt the DDS loop rate, then the drift control; kodedot: clock status |
| `.fixed_vpp_mv`, `.quiet_is_midpoint`, `.trusted_amplitude`, `.outputs` | what the scripts need to know to refuse what a source cannot do |

- **`Esp32Source(Siggen(...))`** — an adapter; `Siggen` is untouched. `tone()` is
  CH1 sine at 2 Vpp, `drive()` the historical pair (CH1 triangle 250 Hz, CH2
  square 400 Hz). With `--source esp32` both scripts produce the historical run.
- **`KodeDotSource`** — `f <hz>` (whole hertz; the reply carries the actual
  frequency from the LEDC registers, crystal-accurate), `d <pct>`, `dc 0|1`,
  `pwm`, `s`, `sweep [stop]`. Both reply shapes parse: the app's console framing
  (`>|freq hz=999.984741 req_hz=1000 …` … `>ok` / `>err msg=…`) and the bare
  `freq 999.98 Hz (req 1000, clk 80000000 res 13 div 9765.6)`. A reply without
  `>ok`, a `req` that is not what was sent, or a mode echo that did not change
  raises. The port is opened with DTR and RTS asserted (pyserial's default) and
  closed RTS-first: on the P4's USB-Serial-JTAG, DTR=0 with RTS=1 is reset, which
  drops the Dot out of the app back to kodeOS. Its amplitude is FIXED — the pin
  swings 0 V to the 3V3 rail, which you measure with a DMM and pass as `--v3v3`.
- **`ManualSource`** — prompts "set 1000 Hz, press Enter", then asks for what
  the counter reads; for amplitudes it wants a unit (`2.000 Vpp`, `0.707 Vrms`,
  `3.292 V` = a square's high level over 0 V). Garbage and implausible numbers
  are asked for again, never guessed.

Waveform matters for amplitude: a capture's span is peak-to-peak, so
`vpp_from_reading()` converts a sine's Vrms with 2√2 but a 50 % square's with
**2** (and a DC-measured high level is the square's Vpp). Expected span counts
are `Vpp / (mV_per_count × SCOPE_CAL_SOURCE_SCALE)`.

Scripts take `add_source_args(ap)`: `--source`, `--source-port` (alias
`--siggen-port`), `--source-serial` (pick one of several Espressif devices),
`--scope-port`, `--v3v3`, `--dry-run`. Ports are discovered by USB id, never by
glob — on macOS the scope's CDC shell and a P4 are both `/dev/cu.usbmodem*`.
`--dry-run` runs the whole flow against `SimBench`: a simulated shell and
source that answer every command consistently. **Nothing a dry run prints is
evidence about the hardware.**

```bash
python3 scripts/verify_scope_cal.py --source kodedot --v3v3 3.292 --timebase 0x10
python3 scripts/measure_sample_rate.py --source kodedot --codes 0x0A 0x0B 0x0C
python3 scripts/measure_sample_rate.py --source manual --codes 0x10   # any generator + counter
```

What a single fixed-amplitude square changes in `verify_scope_cal.py`: there is
one amplitude, so the two-point estimator is replaced by a **static-level**
one (mean of a `dc 1` capture minus a `dc 0` capture — no floor term, no edge
overshoot). Centring wants a quiet input and the Dot's quiet is its low rail,
so 0 V sits at code 128 and only the upper half of the ADC is usable: at
3.292 V that covers ranges 6/7 (8/9 coarsely) and clips 4/5. The script prints
that table, refuses amplitudes and ranges the source cannot serve, and
`--center-mid` (centre once on each rail, offset DAC to the midpoint via
`trig raw` / `trig2 raw`) should bring range 5 back — it assumes the offset DAC
is linear and has the travel, has only run against `SimBench` so far, and the
per-row CLIPPED flag is what tells you whether it worked. With one pin on both
probes the two-SHAPES control is not available, and the report says so.

### Analysis

| function | notes |
|---|---|
| `spectrum(v)` | rFFT magnitude / N, DC zeroed. Normalisation matches every number in `docs/experiments/` (a 2 Vpp tone reads ~22) — do not "fix" it |
| `peaks(v, k)` | top-k `(bin, magnitude)`. A **search**, never a fixed bin |
| `band(v, lo, hi)` | windowed max, inclusive; raises if `hi <= lo` |
| `band_peak(v, lo, hi)` | `(bin, magnitude)` — if the winner is pinned to a window edge, the tone has walked out and your number is a lower bound |
| `window_for(hz, fs, n)` | pick a window with 25% tolerance + padding |
| `fixed_bin(...)` | always raises, with the 1.44-vs-22.8 story |

### Statistics and evidence

| object | notes |
|---|---|
| `paired_difference(read_a, read_b, n=20, stat=np.mean)` | A,B,A triples; B against the **midpoint** of the two A reads, which cancels linear drift exactly. Returns `PairedStats(mean, sd, se, t, …)` |
| `paired_control(read_a, n=20)` | the identical-opcode A,A,A control. **The paired design is invalid without it** |
| `paired_experiment(read_a, read_b, …)` | runs the control *first*, then the test, and returns a `Result` whose verdict already accounts for it |
| `Control(name, expected, measured, passed)` | plus `Control.positive(...)` / `Control.null(...)` |
| `Result(name, observed, controls=…)` | `observed ∈ {POSITIVE, NEGATIVE, INCONCLUSIVE}`; `.verdict` is derived and may be `VOID`; `.markdown()` emits sections 4/5/7 of an experiment file |
| `Experiment(title, unit, build)` | collects controls and attaches them to every result recorded afterwards |

Verdict table:

| situation | `.verdict` |
|---|---|
| any attached control failed | `VOID` |
| `NEGATIVE`/`INCONCLUSIVE` with no controls | `VOID` |
| `POSITIVE` with no controls | `POSITIVE`, flagged `(uncontrolled)` |
| otherwise | the observed outcome |

Creating a `VOID` result also prints a line to stderr saying why. The way to
record a negative is to run the control; there is no other way.

---

## Worked example 1 — EXP-01, "are `op04` and `op05` the same memory?"

Reproduces [`docs/experiments/2026-08-17-01-two-converters.md`](../docs/experiments/2026-08-17-01-two-converters.md).
Content cannot separate the two hypotheses — both predict the same waveform at a
read-time lag — and no trigger value would freeze the buffer, so the design is
statistical: read **A, B, A** and compare B against the midpoint of the two A
reads. Run on a **flat DC input** on purpose: with no periodic content there is
no window-phase wobble to bias the mean.

```python
#!/usr/bin/env python3
import sys; sys.path.insert(0, "scripts")
import numpy as np
from bench import Scope, Siggen, Experiment, paired_experiment

sc = Scope("/dev/ttyACM0")
sg = Siggen("/dev/ttyUSB0")

exp = Experiment("EXP-01 — op04 vs op05: one memory or two converters?",
                 unit="bench unit #1", build=sc.version())

sg.off(ch=1); sg.off(ch=2)          # flat DC — deliberate
sc.timebase(0x10)
sc.trigger_level(0xFF)

# The control (A,A,A) runs FIRST inside paired_experiment, and its outcome is
# what decides whether a null test result is recordable at all.
offset = paired_experiment(sc.reader(0x04), sc.reader(0x05), n=40,
                           stat=np.mean, label="op05 vs op04, DC",
                           experiment=exp)
gain   = paired_experiment(sc.reader(0x04), sc.reader(0x05), n=40,
                           stat=np.std,  label="op05 vs op04, DC (gain)",
                           experiment=exp)
print(offset); print(gain)

# The sign-flip control: swap the roles and the effect must MIRROR.
sg.sine(100, ch=1); sg.amp(2000, ch=1)
mirror = paired_experiment(sc.reader(0x05), sc.reader(0x04), n=20,
                           label="op04 between op05s [SIGN FLIP]", experiment=exp)
exp.control("sign-flip control", "effect mirrors", str(mirror.data["test_mean"]),
            passed=mirror.data["test_mean"] * offset.data["test_mean"] < 0)

print(exp.summary())
print(exp.markdown())          # paste into docs/experiments/
```

Recorded outcome: op05 sat **2.645 codes** below the drift-cancelled op04 at
**t = −357**, while the identical-opcode control sat at **−0.003, t = −0.35**,
and the role-swapped run mirrored at **+3.429**. A re-read of one memory cannot
produce a consistent 2.6-code offset while the control sits at zero ⇒ two
physically distinct converters.

Note the history: an earlier session *retracted* this exact finding because the
means drift read-to-read. Drift is precisely what the paired design cancels, and
the control proves it. A correct result was thrown away by testing it sloppily —
which is the other half of why this module exists.

---

## Worked example 2 — EXP-03, sweeping a frontend bus with a detector control

From [`docs/experiments/2026-08-17-03-warm-handoff-2x2.md`](../docs/experiments/2026-08-17-03-warm-handoff-2x2.md).
The point of this example is the **positive control on the detector itself**, the
one this project keeps skipping: before believing "CH2's 250 Hz tone is absent",
put 250 Hz on the jack that demonstrably works and prove the detector can see a
tone in that window at all. Until that passes, every "no 250 Hz" line is VOID,
not negative.

```python
#!/usr/bin/env python3
import sys; sys.path.insert(0, "scripts")
from bench import Scope, Siggen, Experiment, band, band_peak, window_for

sc = Scope("/dev/ttyACM0")
sg = Siggen("/dev/ttyUSB0")
exp = Experiment("EXP-03 — which frontend bus gates CH2?",
                 unit="bench unit #1", build=sc.version())

FS = 14600.0                                     # approximate; drifts
W100 = window_for(100, FS)                       # never a single bin
W250 = window_for(250, FS)

sc.timebase(0x10)
sc.scope_range(5, 1); sc.scope_range(5, 2)       # establish both relay banks
                                                 # (`gpio set` does nothing on a
                                                 #  pin that is not an output)

# ---- CONTROL, run and recorded FIRST -------------------------------------
sg.sine(250, ch=1); sg.amp(2000, ch=1); sg.off(ch=2)
b, mag = band_peak(sc.opread(0x04), *W250)
exp.control("detector can see 250 Hz at all (on the WORKING jack)",
            "peak inside %s" % (W250,), "bin %d @ %.1f" % (b, mag),
            passed=mag > 5.0 and W250[0] < b < W250[1])

# ---- the sweep ------------------------------------------------------------
sg.sine(100, ch=1); sg.amp(2000, ch=1)
sg.sine(250, ch=2); sg.amp(2000, ch=2)

for name, pins in [("PE4/PE5/PE6", ["E4", "E5", "E6"]),
                   ("PB11/PB10/PA10", ["B11", "B10", "A10"])]:
    seen = []
    for code in range(8):
        for i, p in enumerate(pins):
            sc.gpio(p, (code >> i) & 1)
        v4, v5 = sc.opread(0x04), sc.opread(0x05)
        seen.append((code, band(v4, *W100), band(v4, *W250),
                             band(v5, *W100), band(v5, *W250)))
    moved = max(r[2] for r in seen) > 5.0 or max(r[4] for r in seen) > 5.0
    exp.record("%s: does any code bring up 250 Hz?" % name,
               "POSITIVE" if moved else "NEGATIVE",
               detail="; ".join("code %d -> %s" % (r[0], r[1:]) for r in seen),
               blind_spots=("only sweeps the 8 codes of this one bus",
                            "sample rate assumed ~%.0f S/s; windows sized for "
                            "25%% error" % FS))

print(exp.summary())
```

Recorded outcome: the control passed (250 Hz → bin 17 @ 23.4; 400 Hz → bin 27 @
23.4), `PE4/PE5/PE6` behaved as a clean 8-position attenuator affecting **both**
buffers, and `PB11/PB10/PA10` did nothing at all — a *recordable* negative,
because the detector had been shown able to see the thing it did not see.

Had the control failed, `Experiment` would still have accepted the `record()`
call, printed a stderr warning, and rendered every one of those results as
**VOID**.

---

## Conventions

- One branch per bench session: `bench/YYYY-MM-DD`; one commit per completed
  cycle, `exp(NN): <one-line conclusion>`.
- Write the experiment file from `docs/experiments/TEMPLATE.md`.
  `Result.markdown()` / `Experiment.markdown()` emit sections 4, 5, 6 and 7 for
  you — the problem, hypothesis and procedure are still yours to write.
- Withdraw in place, never delete. A withdrawn result keeps its banner.
- Anchor FPGA config-port measurements: read IDCODE (`0x11`) at `/256` and
  confirm `0x0120681B` before trusting anything else.
