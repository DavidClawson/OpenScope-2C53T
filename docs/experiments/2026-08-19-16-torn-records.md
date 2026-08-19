# EXP-16 — it was never the estimator: a fifth to a half of capture records are torn

> **⚠ CORRECTED 2026-08-19, same day, by the measurement below.** The headline
> claim — "a fifth to a half of capture records are torn" — attributed to
> ACQUISITION something that is substantially an artifact of **how this
> experiment read the buffer**.
>
> `cmd_spi3_read` hex-printed the live acquisition buffer one byte at a time,
> interleaved with USB output, so a 1024-byte dump walked the buffer for ~270 ms
> while the acq task refilled it every ~29 ms — rewriting it about nine times
> underneath the read. Every fixture in §5 was captured that way.
>
> Measured directly, on one unchanged 500 Hz tone, twelve reads each:
>
> | path | read duration | mean sharpness | torn (<0.90) |
> |---|---|---|---|
> | `spi3 read` (live walk) | ~270 ms | 0.520 | **11/12** |
> | `spi3 opread` (snapshots) | ~17.5 ms | 0.809 | 7/12 |
> | acq task's own read (/2 SPI) | ~137 µs | not yet measured | predicted ~0 |
>
> **Tearing tracks READ DURATION**, which is the signature of the reader. At /2
> the acq task's own read overwrites about two samples, so the badge — which
> copies the buffer in ~50 µs — should see almost none of this.
>
> **What survives:** §5a is unaffected and is still the load-bearing result —
> three unrelated estimators failing on the same records and succeeding on the
> same others is exactly what a *reader-side* artifact predicts too. The
> detector, the thresholds and the shipped module are all still correct, and
> are now known to be tuned against pathologically bad data, i.e. conservative.
> The min-bin rule and the quantisation-harmonic finding stand independently.
>
> **What is withdrawn:** the rate, and the attribution to acquisition. Whether
> acquisition tears at all is now **open** and must be re-measured through the
> fixed dump path. `cmd_spi3_read` now snapshots before printing.
>
> Left in place rather than rewritten, per the project's rule.



- **Date:** 2026-08-19
- **Unit:** bench unit #1, build `Aug 19 2026 10:44:58`
- **Source:** ESP32 siggen with the EXP-14 correction on, so drive frequencies
  are **delivered**, not merely commanded
- **Status:** **CONFIRMED** — and it puts a frequency badge back on the screen

## 1. Problem

EXP-13 measured the Freq badge at **+1.6% to +112%** against a known drive and
reverted it. Blame was assigned to `period_samples`: the rising-crossing
estimator finds more crossings than the signal contains. The obvious follow-up
was to replace edge counting with something better.

## 2. Hypothesis

The edge counter is the fault; a spectral peak search — the method that measured
the sample rate to 0.1% — will fix it.

- **If true:** a spectral estimator succeeds where edge counting fails.
- **If false:** it fails on the same records, and the fault is in the data.

## 3. Procedure

Capture **72 real records** off the device (`spi3 read 1024`, the same buffer
the badge reads) at known drives: two vertical ranges (5 and 8), three timebase
codes (`0x10`/`0x0F`/`0x0E`), five frequencies, and a seven-point amplitude
sweep. Store them as a test fixture, then run three unrelated estimators over
the identical data on the host:

1. **edge counting** — a transcription of `scope_measure.c` pass 3;
2. **autocorrelation** — normalised, first-peak-after-zero-crossing;
3. **spectral peak** — Hanning window, parabolic interpolation.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| source delivers what it is asked | ratio 0.8324 | pinned and re-checked, drift 0.01% | ✓ |
| records read through the badge's own path | acq buffer | `spi3 read`, not `opread` | ✓ |
| a clean synthetic sine is measurable | <1% | all three estimators nail it | ✓ |
| the fixture is what the device produced | — | 72 records stored verbatim, no filtering | ✓ |

The third control matters: a synthetic sine is exactly the case that has always
worked, which is why it cannot be the acceptance test.

## 5. Results

### 5a. All three estimators fail, and they fail together

| estimator | records worse than 5% | worst error |
|---|---|---|
| edge counting (shipped, reverted) | **15 / 30** | +196% |
| autocorrelation | **19 / 30** | −92% |
| spectral peak | **13 / 30** | +111% |

Three unrelated algorithms, similar failure counts — and, decisively, **they
fail on the same records and succeed on the same others.** That is not an
algorithm problem.

### 5b. The records are torn

The failing records have their spectral energy **smeared across ~6 adjacent
bins** instead of concentrated. Measuring the fraction of spectral power in the
peak bin ±1, over progressively shorter sub-windows:

| case | N=1024 | N=512 | N=256 | N=128 |
|---|---|---|---|---|
| rng5 0x10 100 Hz | **0.380** | **0.986** | 0.979 | 0.967 |
| rng5 0x0F 250 Hz | 0.426 | 0.503 | 0.798 | 0.964 |
| rng5 0x0E 250 Hz | 0.789 | 0.867 | 0.937 | 0.984 |
| rng5 0x0E 100 Hz *(works)* | 0.992 | 0.972 | 0.994 | 0.996 |

The first row is the whole finding in one line: **a record that is incoherent
over its full length whose halves are each essentially perfect.** The data is
stitched, not noisy.

### 5c. What it is not

- **Not a fixed structural seam.** Only 2/15 broken records have clean halves
  either side of sample 512, so the "two 512-sample buffers spliced" reading —
  tempting given the two-stage capture pipeline — is not supported.
- **Not a signal-quality problem.** A dedicated 42-record amplitude sweep found
  records at span **11 counts measuring correctly** (+0.1%, −2.4%, −1.3%) and
  records at span **107 unusable**. Tearing does not track amplitude at all:

  | span band | n | wrong | refused |
  |---|---|---|---|
  | 0–15 | 6 | 0 | 1 |
  | 15–25 | 6 | 0 | 0 |
  | 50–70 | 6 | 1 | 0 |
  | 95+ | 6 | 0 | 1 |

  This killed the guard I was about to write. A quality gate on the input
  cannot screen out a defect that is uncorrelated with input quality.
- **Not deterministic.** The same nominal condition tears sometimes and not
  others, which is why it must be detected **per record**.

### 5d. Detection works

The sharpness figure separates the two populations cleanly: torn records score
**0.54–0.75**, clean ones **0.83–1.00**. Sweeping the acceptance thresholds over
all 72 records:

| sharpness floor | min bin | correct | **WRONG** | refused | worst error |
|---|---|---|---|---|---|
| 0.80 | 2 | 53 | **5** | 14 | 69.5% |
| 0.85 | 6 | 43 | **1** | 28 | 7.5% |
| **0.90** | **6** | **39** | **0** | **33** | **3.4%** |
| 0.92 | 6 | 32 | 0 | 40 | 3.4% |

The min-bin rule earns its place separately: three of the four
confidently-wrong answers at 0.80/2 were peaks below bin 6, where a Hanning
mainlobe overlaps DC and a **quantisation harmonic outranks the fundamental** —
at range 8 a 2 Vpp sine is only ~10 ADC counts tall, and quantising a sine to
10 levels generates a strong second harmonic.

Adding a fallback to the record's **last half** when the whole is not sharp
recovers most torn records (the 0.380 case becomes +0.1%), at the cost of half
the frequency resolution — which is why it is a fallback and not the default.

## 6. Blind spots

- **The cause of the tearing is unknown.** "Rolling buffer, stale head, fresh
  tail" fits the second-half-is-cleaner observation but is not established, and
  no mechanism has been shown.
- **CH1 only, one session, one unit.** CH2 untested.
- **Only three timebase codes**, all in the MEASURED tier.
- **The 5% contract is a choice**, not a hardware limit. Worst observed is 3.4%.
- **Refusal rate is 46%** on this fixture set. Whether that is representative of
  live use is not established.
- **Nothing here measures how often the DISPLAYED trace is torn** — only the
  measurement path. A torn record presumably renders as a glitched waveform too.

## 7. Conclusion

- **Established:** roughly a fifth to a half of capture records are torn —
  spectrally smeared, with coherent sub-windows — and this, not the choice of
  estimator, is what made the Freq badge wrong. The hypothesis in §2 is
  **refuted**: the spectral estimator failed on the same records.
- **Shipped:** `src/ui/scope_freq.c`, which detects tearing per record, retries
  on the fresher half, and **refuses** rather than guess. Against the 72 bench
  captures it answers 54% of the time, is within **3.4%** when it does, and is
  never wrong. `tests/test_scope_freq.c` runs the shipped code over the real
  records and fails the build if any answer exceeds 5%.
- **The Freq badge is back**, with a short hold across refusals so it does not
  strobe at the tearing rate.
- **NOT fixed:** the tearing itself. This is a workaround at the measurement
  layer. The real repair is in acquisition, and finding it is now a much better
  defined problem than it was this morning — including whether the rendered
  trace suffers the same glitches.
- **Method note:** the deciding move was capturing real records as a **fixture**
  instead of reasoning about the algorithm. Three estimators agreeing on which
  records they cannot handle is information that no amount of thinking about
  edge counting would have produced. It also means the module's own test
  exercises the code that ships, on data the device really made — after a week
  in which three separate conclusions turned out to be artifacts of our own
  analysis code.
