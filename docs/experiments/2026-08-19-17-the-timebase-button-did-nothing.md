# EXP-17 — the timebase button moved a label, not the sample rate; and the rates were 0.7–2.9% off

- **Date:** 2026-08-19
- **Unit:** bench unit #1
- **Build under test:** `Aug 19 2026 12:08:33` (before), `12:38` (after)
- **Status:** CONFIRMED, two findings, both corrected

## 1. Problem

Two things were outstanding: validate the frequency estimator shipped this
morning against a **held-out** set (its thresholds were swept on the same 72
records the test asserts against — training data), and run the on-device
acceptance test.

Neither was expected to find anything. Both did.

## 2. Hypotheses

- **H1 (held-out):** if the thresholds were overfitted, coverage and error on
  fresh drive frequencies will be materially worse than 88% / 3.1%.
- **H2 (implicit):** the device's own estimator, reading its own buffer,
  reproduces the host numbers.

## 3. Procedure

`scripts/capture_scope_records.py` (new, so this is reproducible) captured 60
records at drive frequencies **150/330/700/1500/3000 Hz** — deliberately
disjoint from the fixture set's 100/250/500/1000/2000 — across ranges 5 and 8
and codes 0x0E/0x0F/0x10, two reps each. Read through `spi3 read`, the acq
buffer, which is the path the badge reads.

The shipped `scope_freq.c` was then compiled on the host and run over them.

## 4. Controls

| control | expected | measured | passed? |
|---|---|---|---|
| source rate pinned in the sweep's channel config | — | 33,300.1 S/s, ×0.8325 | ✓ |
| source drift across the run | <0.2% | **0.016%** | ✓ |
| build identity after flash | new image | `Aug 19 2026 12:08:33` | ✓ |
| `fpga scope freq` refuses on an uncalibrated code | refuse | refused at 0x0A | ✓ |
| held-out drives disjoint from tuned set | no overlap | 150/330/700/1500/3000 vs 100/250/500/1000/2000 | ✓ |

## 5. Results

### 5a. H1 — the tuning did not overfit

| set | answered | wrong | worst |
|---|---|---|---|
| tuned (100/250/500/1000/2000 Hz) | 63/72 (88%) | 0 | 3.1% |
| **held-out (150/330/700/1500/3000 Hz)** | **52/60 (87%)** | **0** | **3.7%** |

Held-out matches tuned. The thresholds generalise.

### 5b. The errors were not noise — they were per-code constants

The held-out error column was structured: 0x0E consistently negative, 0x0F
consistently ≈+3%, 0x10 consistently ≈+0.5%, across every frequency and both
ranges. That is not an estimator property. Solving for the rate that would
make each record correct:

| code | n | implied fs | table fs | change | vs round 1-2-5 |
|---|---|---|---|---|---|
| 0x0E | 37 | **49,930.1** | 49,056.0 | +1.78% | −0.14% |
| 0x0F | 29 | **24,979.1** | 25,736.0 | −2.94% | −0.08% |
| 0x10 | 49 | **12,490.0** | 12,575.0 | −0.68% | −0.08% |

**The blame argument.** A source-scale error — the EXP-14 bug — multiplies
every commanded frequency by one factor, so it moves every code the same
direction by the same amount. Here 0x0E needs +1.8% and 0x0F needs −2.9%:
**opposite signs, which no single source factor can produce.** And within one
code the error held constant from bin 6.8 to bin 246, a 36:1 span, so it is not
interpolation bias either (that varies with fractional bin position).

**A counter-argument, tested and rejected.** The original sweep took the
*slope* of a bin-vs-frequency fit; the above averages *ratios*. Those disagree
if the fit has a nonzero intercept — and if it does, the slope is right. The
intercept is real but tiny: **−0.045 bins, the same on all three codes** (a
small fixed bias in parabolic interpolation over a Hanning magnitude
spectrum), worth 0.03–0.11%. Both methods reject the old table. The published
values are the free-intercept slope fits, which remove that bias.

**Four independent checks:**

- two disjoint drive sets agree to **0.00%, 0.03%, 0.24%**;
- all three land within **0.14%** of the round ladder 12.5k/25k/50k;
- ratio-averaging agrees with the slope fit to **0.11%**;
- **0x10 against Stlkv's independent rig (12,437 S/s — different unit,
  generator, firmware, method) improves from +1.11% to +0.43%.**

The last is the one that counts: it is out-of-sample. Nothing in the fit knows
his number, and the correction moved toward it.

**Consequence, measured:** re-running the estimator against the corrected
rates drops worst error from **3.1% → 1.2%** (tuned set). Stratified by bin,
every error above 1% sits at bin ≈6, at the `MIN_BIN` floor; **every record at
bin ≥13 is within 0.81%.**

### 5c. H2 — the on-device test found a worse bug than it was looking for

`fpga scope freq` refused every reading, reporting `timebase 0x0A` no matter
what the bench script set. Tracing it:

- `bench.timebase()` writes reg `0x01` on the wire via `seq`;
- the status bar, the s/div label and the Freq badge derive `fs` from
  `scope_state.timebase_idx`;
- the acquisition task programs reg `0x01` from `acq_rate_idx` (`fpga.c:3268`),
  a **third** variable, default `0x08`, reachable only from `fpga rate`, and
  gated behind `acq_rearm_enable` which defaults **off**;
- `scope_adjust_timebase()` — the UI button — mutates the struct field **and
  nothing else**.

Measured on hardware, stock boot, nothing touched:

```
acq rate idx = 0x08          <- what the FPGA samples at
timebase 0x0A -> 0 S/s       <- what the display labels the axis from
```

**The timebase button does not change the sample rate. It moves a label.**

This stayed invisible because both codes sit in the uncalibrated band, so the
label read `--` and the badge refused — it failed safe **by luck**. Once this
morning's work gave the labels real measured rates, pressing the button up to
0x10 would have printed a confident `2.54ms/div` while the hardware sampled at
`0x08`, a code whose rate is INCOHERENT and unknowable.

There was a comment sitting directly above `acq_rate_idx` describing the
correct behaviour — "the re-arm must rewrite THIS … or it would silently undo
any timebase the UI or the shell has selected" — for code that never
implemented it. **The comment documented an intention, and it read as
documentation of a fact.**

## 6. Fixes

- `fpga_apply_timebase(code)` — one entry point that parks the acq task,
  writes reg `0x01`, and records the value in force. Returns false without
  writing if the task will not park.
- The UI button calls it, and shows `H=… NOT SET` if the write failed rather
  than labelling an axis whose rate did not change.
- `fpga scope freq` prints **both** codes and **refuses to derive a frequency**
  if they disagree.
- `fpga scope timebase <code>` sets both together, so bench scripts do not
  have to reproduce the bug.
- Rate table revised to 49,930.1 / 24,979.1 / 12,490.0.
- `test_scope_freq` gained a **bin-stratified** assertion: records at bin ≥13
  must be within 1%. Negative control — with the old rates restored it reports
  **17 misses and fails**, while the old global 5% bound passed them.
- The held-out set ships as a second fixture and runs in the same suite.

## 7. Blind spots

- **0x0D (119,678 S/s) was not re-measured** — the bench source cannot place a
  peak usefully that far up. It very likely carries a similar error and stays
  PROVISIONAL.
- **Why the first ladder was wrong is NOT established.** The leading candidate
  is that it was fitted partly on torn records (it swept through `opread`,
  which still tears 6/12); this fit used `spi3 read`, which tears 0/12 after
  the snapshot fix. Plausible mechanism, untested — the old data has not been
  re-fitted.
- **Absolute scale still traces to the ESP32 crystal.** All four checks are
  differential or cross-rig; none is against a calibrated reference. A crystal
  is good to tens of ppm, so this is likely small, but it is not verified.
- **The button fix is not yet bench-confirmed** — it builds and the shell path
  is testable, but the physical button press has not been run.
- One channel, one unit, one session.

## 8. Conclusion

- **Established:** the UI timebase control never reached the hardware; three
  variables tracked one register; the measured rates were 0.7–2.9% off.
- **Established:** the estimator generalises to held-out drives (87%, 0 wrong)
  and resolves to ~0.8% wherever the peak is above bin 13.
- **Not established:** the mechanism behind the original ladder's error.
- **Method note, third time this week:** every one of today's findings came
  from pointing an instrument at a case whose answer was independently known.
  The held-out capture was meant to be a formality. The most valuable thing
  built today is `fpga scope freq` printing both timebase codes — a diagnostic
  that makes one specific lie impossible to tell again.
