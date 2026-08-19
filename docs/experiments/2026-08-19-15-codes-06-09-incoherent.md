# EXP-15 — Stlkv's prediction, and what a pass/fail label got wrong

- **Date:** 2026-08-19
- **Unit:** bench unit #1, device build `Aug 18 2026 17:21:17`
- **Source:** ESP32 siggen with the EXP-14 correction on (33,299.1 Hz, 0.8325)
- **Status:** **CONFIRMED** — the prediction holds; the mechanism offered with it
  does not follow from what was measured

## 1. Problem

EXP-12 withdrew a sample rate for reg-`0x01` code `0x08`: the record does not
reproduce between reads, so the fit was measuring nothing. It was treated as one
bad code.

On issue #18 Stlkv offered a mechanism and, to his credit, a falsifiable
prediction with it: if the record at `0x08` is **torn** rather than slow — the
engine lapping the buffer during our read — then the fit returns *our read
cadence*, not the engine's rate. **Prediction: `0x07` measures the same number.**

## 2. Hypothesis

- **If the numbers at these codes are the device's rates:** they form a ladder,
  stepping 2–2.5× per code as `0x0D`–`0x10` demonstrably do.
- **If they are an artifact of our measurement:** they pile up on one value
  regardless of code.

## 3. Procedure

Fit `0x06`, `0x07`, `0x08`, `0x09` identically — six sub-Nyquist tones
(40–420 Hz), median of three reads each, through `spi3 opread`, with the source
correction on and its rate pinned in the sweep configuration. Report the
worst read-to-read spread alongside every fit, per EXP-12.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| source rate pinned and reported | ~0.8325 | 33,299.1 Hz, 0.8325 | ✓ |
| a code known to be coherent behaves differently | clean | `0x10` fits R² 0.999, fold-checked (EXP-14) | ✓ |
| ladder step size where the ladder is real | 2–2.5× | `0x0D`–`0x10` step 2.0× each | ✓ |

The third control is the one that makes this experiment readable: it establishes
what a genuine step between adjacent codes looks like on this device.

## 5. Results

| code | fitted fs | R² | worst read-to-read spread | 40 Hz lands on |
|---|---|---|---|---|
| 0x06 | 1,231 | 0.974 | 144 bins | bin 28 |
| 0x07 | 1,595 | 0.966 | 160 bins | bin 28 |
| 0x08 | 1,350 | 0.978 | 309 bins | bin 35 |
| 0x09 | 1,550 | 0.903 | 255 bins | bin 28 |

**All four codes return the same number.** The whole block spans 1,231–1,595
S/s — 30% — where one ladder step is 100–150%. The 40 Hz tone lands on bin 28 in
three of four codes and bin 35 in the other. Read-to-read spreads are 144–309
bins on a 512-bin spectrum.

**A number that does not change when the timebase code changes is not the
device's rate.**

### 5a. The scripted verdict was wrong, and that is worth recording

The test harness asked "do `0x07` and `0x08` agree to 15%?", got 18.1%, and
printed **PREDICTION FAILS**. That threshold was chosen before the data existed
and it is nonsense here: the disagreement *between* codes (18%) is far smaller
than the scatter *within* each one (144–309 bins). The right question was never
"do these two numbers match" but "do these codes behave as a ladder", and they
do not. Read the data, not the label.

### 5b. The proposed mechanism does NOT follow

Tempting: one `opread` round trip measured **802 ms** at `0x08`, and
1024 / 0.802 = **1,277 S/s**, sitting neatly inside the observed band. That
looks like confirmation and it is not one.

- The 802 ms is a **host round trip**, dominated by dumping 1026 bytes as hex
  over 115200 baud (~270 ms) plus shell overhead. It is not the sampling window.
- The actual SPI burst at /256 off a 120 MHz bus is ~17.5 ms, which under a
  pure-tearing model predicts ~58 kS/s — not 1.4 k.
- And `0x10` has the *same* ~900 ms round trip while fitting cleanly at 12,575
  S/s with R² 0.999 and passing the fold test. If round-trip duration set the
  answer, `0x10` would be ruined too.

So: **prediction confirmed, mechanism open.** Recorded that way deliberately —
adopting a mechanism because its arithmetic nearly lands is exactly how this
project acquired `0x8001C810`, the 2.882 mV/count constant, and last week's
double FFT.

## 6. Blind spots

- **Only `opread` was tested.** The acq-buffer path may behave differently at
  these codes; it was not tried.
- **What the engine is actually doing at `0x06`–`0x09` remains unknown.** This
  experiment says our number is not it; it does not say what is.
- **Codes below `0x06` untested.**
- One session, one channel, one read path.

## 7. Conclusion

- **Established:** codes `0x06`–`0x09` all return ~1.2–1.6 kS/s irrespective of
  the code, with records that do not reproduce. EXP-12's withdrawal is widened
  from `0x08` alone to the whole block; `scope_timebase.c` now marks all four
  INCOHERENT rather than leaving three as merely unmeasured.
- **Stlkv's prediction holds.** He called it from another bench, on another
  board, before it was run.
- **His mechanism is not established by this**, and the number that appeared to
  confirm it (1,277 S/s from an 802 ms round trip) is a coincidence of the wrong
  interval. Left open.
- **Method note:** a pass/fail threshold written before the data can be more
  confident than the data supports. This one printed FAILS on a result that
  confirms the hypothesis, and the only thing that caught it was reading the
  table underneath.
