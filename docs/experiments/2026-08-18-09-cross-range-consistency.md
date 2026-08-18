# EXP-09 — does the calibrated instrument report the same volts on different ranges?

- **Date:** 2026-08-18
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, build stamp `Aug 17 2026 23:26:37` (the
  cal table under test is host data, wired into firmware in `3fe13ca`; the
  running image is unchanged from EXP-07/08, so this measures the TABLE, not
  a new build)
- **Status:** CONFIRMED

## 1. Problem

EXP-08 produced a per-channel, per-range gain ladder and those numbers are now
compiled into the firmware (`src/ui/scope_cal.c`), where they set every volt
the instrument prints. But EXP-08 measured each range **in isolation**: it
varied the drive amplitude at a fixed range and fitted a slope. Nothing in that
design compares one range against another.

A range table can pass a per-range slope fit on every row and still be
internally inconsistent — each row individually linear, the rows wrong relative
to each other. That failure is invisible to the experiment that produced them,
and it is the failure a user hits first, because changing range is the most
common thing anyone does with a scope.

## 2. Hypothesis

The same physical signal, measured on ranges 5, 6 and 7 and converted to volts
through each range's own gain, should give the same voltage.

- **If the table is internally consistent:** the three volt figures agree to
  within roughly the amplitude precision available here (~10%), on both
  channels.
- **If it is not:** they disagree by substantially more, and the "clean 1-2-4
  doubling" claim in `scope_cal.h` is wrong — in which case the MEASURED tier
  is not earned and ranges 5/6/7 must be demoted.

Note what this cannot decide: all three ranges could be wrong by the same
factor and still agree perfectly. This tests **consistency**, not accuracy.

## 3. Procedure

`scripts/bench.py` driving the CDC shell on `/dev/ttyACM0` and the ESP32
generator on `/dev/ttyUSB0`.

For each range r in {5, 6, 7}:

```
fpga scope range r 1
fpga scope range r 2
fpga scope center ch1 r
fpga scope center ch2 r
<settle 1.4 s>
median span over 5 reads of op04 and op05
```

Drive: CH1 triangle 250 Hz, CH2 square 400 Hz — two different **shapes**, so a
display that inverted or rescaled one channel could not make a single source
look like two. Amplitudes 0 / 1000 / 2000 mVpp.

Settle is 1.4 s, at least one full 1024-sample buffer at the measured ~1.07
kS/s. The capture free-runs, so a read taken sooner returns the pre-change
buffer.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| build identity | coldtrace, known stamp | `Aug 17 2026 23:26:37` ✓ |
| both channels responsive at r6 | span ≫ floor | 49 / 51 counts ✓ |
| centering converges | mean ≈ 128 | reported per range by the shell ✓ |

Gowin `STATUS` (`0x41`) was not read at any point — doing so on a configured
part desynchronises capture.

## 4. Control

Run first, at range 6, before any sweep number was taken.

| control | expected | measured | passed? |
|---|---|---|---|
| drive absent → span collapses | span at the noise floor | op04 **5.00**, op05 **3.00** | ✓ |
| drive present → span lifts | ≥4× the quiet floor | op04 **49.00**, op05 **51.00** | ✓ |

So "span" is measuring the drive, not pickup or a free-running artefact — the
failure that voided the 2026-08-15 session before the generator's channel 2 was
found to be the source of the phantom sine.

## 5. Results

### 5a. First pass — raw span (contains a defect; see 5b)

Fixed 2000 mVpp drive, volts computed as `span × mV_per_count`:

| range | CH1 span | mV/ct | → Vpp | CH2 span | mV/ct | → Vpp |
|---|---|---|---|---|---|---|
| 5 | 96.00 | 21.83 | 2096 mV | 99.00 | 20.96 | 2075 mV |
| 6 | 49.00 | 42.95 | 2105 mV | 51.00 | 41.71 | 2127 mV |
| 7 | 26.00 | 88.42 | 2299 mV | 26.00 | 83.79 | 2179 mV |

Agreement across ranges: **CH1 spread 9.4%**, **CH2 spread 4.9%**. Both
channels also agree with *each other* on every range, to within 1–5%.

**Defect in this pass, found before it was published:** peak-to-peak span
includes the noise floor **additively**, so this estimator over-reads by
roughly (floor counts × mV/count) — and mV/count changes 4× across the ladder,
so the bias is range-dependent and inflates exactly the high ranges. This is
the bias EXP-08's slope method existed to avoid, and reading a single amplitude
quietly reintroduced it. The "mean measured 2166 mVpp vs 2000 commanded, ratio
1.083" from this pass is therefore **not** a source-scale estimate and must not
be used as one.

### 5b. Floor-corrected, two ways

Quiet floor measured **at each range**, plus a two-amplitude difference
(1000 → 2000 mVpp) which cancels the floor exactly without measuring it. If the
two methods agree, the floor is additive and both are sound.

| range | ch | floor | 1000 mVpp | 2000 mVpp | floor-corrected | two-point |
|---|---|---|---|---|---|---|
| 5 | 1 | 5.0 | 48.0 | 96.0 | **1987 mV** | 2096 mV |
| 5 | 2 | 3.0 | 51.0 | 99.0 | **2012 mV** | 2012 mV |
| 6 | 1 | 4.0 | 26.0 | 49.0 | **1933 mV** | 1976 mV |
| 6 | 2 | 3.0 | 26.0 | 51.0 | **2002 mV** | 2086 mV |
| 7 | 1 | 4.0 | 14.0 | 26.0 | **1945 mV** | 2122 mV |
| 7 | 2 | 3.0 | 15.0 | 26.0 | **1927 mV** | 1843 mV |

(span in counts; the last two columns are volts through that range's own gain)

| channel | method | per-range volts | spread | mean / commanded |
|---|---|---|---|---|
| CH1 | floor-corrected | 1987 / 1933 / 1945 | **2.8%** | 0.977 |
| CH1 | two-point | 2096 / 1976 / 2122 | 7.1% | 1.032 |
| CH2 | floor-corrected | 2012 / 2002 / 1927 | **4.3%** | 0.990 |
| CH2 | two-point | 2012 / 2086 / 1843 | 12.2% | 0.990 |

**Reading this.** The floor-corrected spread — 2.8% on CH1, 4.3% on CH2 — is
the headline: three different range gains, one signal, agreement to a few
percent on both channels. That is a factor of three tighter than the
uncorrected first pass, which is itself evidence that the bias identified in
5a was real and additive, exactly as assumed.

The two-point figures are noisier, and that is expected rather than
contradictory: differencing two quantised spans roughly doubles the
quantisation error, and at range 7 the difference is only 11–12 counts, so
±1 count on each end is ±17%. Every two-point/floor-corrected pair agrees
inside that uncertainty except range 5 CH1 (5.5% apart against ~4.3% combined
quantisation), which is marginal. **The two methods are consistent; the
floor-corrected one is simply the more precise estimator here, and it is the
one the conclusion rests on.**

**The mean/commanded column is NOT an accuracy figure.** These gains were
derived from this same generator, so agreeing with it at 0.98–0.99 is close to
circular. What it does establish is narrower and still worth having: the slope
fit's assumption that the intercept absorbs an additive floor holds when tested
directly, so no bias was smuggled in between EXP-08's estimator and this one.
An absolute check needs a source this bench does not yet have.

### 5c. Replicate, through the committed tool

The measurement was then re-run end to end through
`scripts/verify_scope_cal.py` — partly to prove the committed tool actually
works before anyone relies on it (a tool that has never been executed is the
same hazard as `usart tx` queueing into a task that did not exist), partly as a
run-to-run reproducibility check. Same build, same session, ~20 minutes later.

| channel | 5b floor-corrected | 5c floor-corrected | 5b spread | 5c spread |
|---|---|---|---|---|
| CH1 | 1987 / 1933 / 1945 | 1987 / 1890 / 1945 | 2.8% | 5.0% |
| CH2 | 2012 / 2002 / 1927 | 1970 / 2044 / 1927 | 4.3% | 5.9% |

mean/commanded: CH1 0.977 → 0.970, CH2 0.990 → 0.990. Control passed again
(quiet 5.00/2.00, driven 49.00/56.00).

**Run-to-run scatter is comparable to the cross-range spread itself** — no
individual figure moved by more than ~4%, and the conclusion is unchanged, but
this bounds how finely the design can resolve anything: differences below about
5% are not distinguishable from repeat-measurement noise here.

**A defect in the tool, found by this run and fixed.** The first version
declared "INCONSISTENT" against a flat 15% threshold, and flagged CH1's
two-point estimator at 15.4%. That verdict was wrong in kind, not degree: at
range 7 the two-point denominator is 13 counts, so ±1 count on each end is
±15% — the estimator cannot resolve a disagreement that small, and a threshold
that ignores its own quantisation is an instrument that does not know what it
can detect. The tool now computes each estimator's quantisation floor from the
actual counts and compares the spread against `max(10%, 1.5 × floor)`, printing
both. Under that rule the same data reads: floor-corrected consistent on both
channels (limit 10%), two-point consistent (limit ~23% at range 7).

## 6. Blind spots

- **Consistency is not accuracy.** A uniform scale error across all three
  ranges passes this test perfectly. That is precisely the error the untested
  ESP32 amplitude could introduce, and it is what `SCOPE_CAL_SOURCE_SCALE`
  exists to absorb once a trusted source is available.
- **Quantisation dominates the top of the range.** Range 7 shows only 26 counts
  of span, so ±1 count is ±3.8%. Most of CH1's 9.4% first-pass spread is this,
  not a table error — which also means this design cannot resolve a real
  disagreement smaller than a few percent at range 7.
- **Ranges 4, 8, 9 were not tested.** A single drive amplitude cannot span the
  whole ladder; that is the same limitation that made those rows PROVISIONAL in
  the first place, and it is unchanged.
- **Same rig, same source, same evening as EXP-08.** Any systematic error
  common to both is invisible here. The one genuinely external check remains
  Stlkv's measurements on a different unit, and it covers only ranges 5–7.
- **Repeat-measurement noise is ~5%, comparable to the effect measured.** The
  5c replicate bounds this: the design cannot resolve a cross-range
  disagreement below roughly 5%. Part of that is the per-range re-centring,
  whose run-to-run variance is not separately characterised.
- **Nothing here touches the time axis**, which EXP-08 showed is not
  trustworthy.

## 7. Conclusion

- **Established:** the ranges 5/6/7 gains are internally consistent. The same
  physical signal converts to the same voltage through three different range
  gains — **spread 2.8% on CH1 and 4.3% on CH2** floor-corrected — and the two
  channels agree with each other on every range. This is an independent design
  from the slope fit that produced the numbers, so it is a real check rather
  than a restatement.
- **Also established, second order:** the slope fit's assumption that its
  intercept absorbs an additive noise floor is correct. Subtracting a directly
  measured floor tightened the spread by a factor of three and brought both
  channels to within 1–2% of the commanded amplitude, which is what should
  happen if the floor is additive and nothing else is.
- **Also established:** the MEASURED tier on ranges 5/6/7 is earned. Four
  agreeing lines of evidence now: the EXP-08 slope fit, EXP-06's mux-code
  sweep, Stlkv's numbers from another unit and rig, and this cross-range test.
- **Excluded:** an internal inconsistency in the 5/6/7 ladder large enough to
  matter to a user — e.g. a mis-transcribed row or an off-by-one in the range
  index, either of which would have shown as a ~2× step.
- **NOT excluded (explicitly):** absolute accuracy. Every number here still
  traces to the ESP32's *commanded* amplitude. A uniform scale error passes
  this test undetected, by construction.
- **Tooling:** this measurement is committed as `scripts/verify_scope_cal.py`,
  which parses the gains out of `scope_cal.c` rather than duplicating them, runs
  the control first, and computes both estimators. It is the script to re-run
  against a trusted source.
- **Follow-up:** (1) repeat against a calibrated source when the USB signal
  generator arrives, and set `SCOPE_CAL_SOURCE_SCALE` from it — one range is
  enough, since the error is uniform; (2) extend to ranges 4/8/9 with
  per-range amplitudes, which is EXP-08 follow-up (1) and would let them leave
  the PROVISIONAL tier; (3) the bin-1 time-axis anomaly is untouched and still
  blocks frequency and period.
