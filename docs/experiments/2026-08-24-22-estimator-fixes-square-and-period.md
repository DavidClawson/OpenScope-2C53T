# EXP-22 — freq gate admits squares (#27); period estimator loses its few-periods bias (#26)

- **Date:** 2026-08-24
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace` (Build Aug 24 2026 14:04:44), CDC shell up
- **Source:** JDS6600 → scope CH1 (BNC). Crystal-accurate in frequency, driven
  via `bench.JDS6600`. Timebase code `0x10` = 12,490 S/s (non-tearing).
- **Status:** BOTH CONFIRMED on hardware. Fixes host-tested with negative
  controls, then validated against the crystal source.

## 1. Problems

Two estimator bugs reported by @Stlkv on unit #2, v0.3.0 ([#18], filed as [#26]
and [#27]):

- **#27** — `fpga scope freq` structurally refuses **every** square wave. The
  admission gate (`SCOPE_FREQ_MIN_SHARP = 0.90`) measures "sharpness" as the
  fraction of spectral power in the peak bin ±1. An ideal square's fundamental
  carries only **8/π² = 0.811** of the power (the rest is in the odd harmonics,
  real signal), so the *maximum achievable* sharpness for a perfect square is
  ~0.81 — permanently below the gate. His clean 100 Hz square locked bin 41.0
  every rep at sharpness 0.81 and was refused 0/6.
- **#26** — the period estimator reads **low when few periods fit the window**
  (−0.4% at ~41 periods worsening to ~−5% at ~6).

## 2. Fixes

### #27 — harmonic-aware sharpness (`scope_freq.c`)

Count power in the fundamental mainlobe **plus its integer-harmonic mainlobes**
(`2·peak`, `3·peak`, … ±1, clamped so low fundamentals never double-count)
before dividing by total. A clean square then scores ~0.95+; a sine stays ~0.99
(harmonics ≈ 0); broadband noise stays low (its power is not on a harmonic
comb). The 0.90 gate is **kept** — it still catches a slow-read regression, it
just no longer mislabels non-sinusoids.

### #26 — genuine-crossing period (`scope_measure.c`)

Two causes, not one. The issue named the smaller: **integer-sample quantization**
of the crossing index, fixed by linearly interpolating the sub-sample point
where the signal crosses `hi_thr2`. The larger cause, found here: a record that
**opens mid-high-phase** counts its `i=0` boundary as a rising edge and anchors
the period on it, inflating `(rising−1)` and dragging the period **low, worse at
few periods** (∝ 1/K). The fix anchors the period only on genuine `i>0`
crossings; the `i=0` boundary still counts as a cycle but not as a period anchor.

## 3. Host tests (with negative controls)

`test_scope_freq.c` gains a synthetic clean square: admitted at the correct bin,
sharpness ≥ 0.90 — teeth, because reverting the harmonic logic drops it to ~0.81
and the check fails. `test_scope_measure.c` gains a period-bias sweep at a fixed
non-integer period, record starting at the peak, from ~40 down to ~5
periods/window, with the pre-fix integer estimator kept as a control that **must**
drift:

```
P=  25.60 (40/win): real +0.00%   int +0.49%
P= 102.40 (10/win): real +0.00%   int +2.05%
P= 170.67 ( 6/win): real +0.00%   int +3.42%   ← reproduces Stlkv's ~-5%
P= 204.80 ( 5/win): real +0.02%   int +4.10%   (reads low, matching his sign)
```

Full suite green; `guest-coldtrace` builds and fits (bss 228,688 / 229,376).

## 4. Bench confirmation (JDS6600, unit #1)

### #27 — the exact reported case now answers

Sine vs square at matched frequencies, `fpga scope freq`, code `0x10`:

| wave | f_cmd | bin | answered | median Hz | sharp |
|---|---|---|---|---|---|
| sine | 100 | 8.2 | 10/10 | 99.40 | 0.99 |
| **square** | **100** | **8.2** | **10/10** | **99.41** | **0.94** |
| sine | 500 | 41.0 | 10/10 | 499.72 | 0.99 |
| square | 500 | 41.0 | 10/10 | 499.72 | 0.99 |
| square | 1000 | 82.0 | 10/10 | 999.44 | 0.94 |

Stlkv's exact failing case — **100 Hz square — goes from refused 0/6 (sharp
0.81) to answered 10/10 (sharp 0.94)**, at the correct frequency. Sine and
square track in frequency at every point.

### #26 — period-method now agrees with FFT-method at few periods

The `measure` badge's `f1_mHz` is period-derived; the `freq` badge is
FFT-derived — two independent methods on the same acquisition. Before the fix
the period method read high at few periods; after it, they agree everywhere:

| f_cmd | periods/win | period (smp) | f_period | f_fft | agree |
|---|---|---|---|---|---|
| 488 | 40 | 25.59 | 487.69 | 487.68 | 0.00% |
| 244 | 20 | 51.22 | 243.84 | 243.84 | 0.00% |
| 122 | 10 | 102.41 | 121.92 | 121.92 | 0.00% |
| 73 | 6 | 170.53 | 73.01 | 73.02 | 0.02% |
| 61 | **5** | 204.68 | 60.97 | 60.97 | 0.00% |

≤0.02% at every count, down to 5 periods/window — the regime that drifted −5%.

## 5. Side fix

`bench.JDS6600.read_raw`/`write_raw` formatted single-digit registers unpadded
(`:r0=`), but the JDS6600 protocol is two-digit and the device echoes `:r00=`.
The 20–30 range worked by accident (already two digits); register 0 did not.
Fixed to `%02d`.

## 6. Not established

Absolute frequency reads ~0.5% low across the board — that is timebase `0x10`
(12,490 S/s, itself a measured value) against the JDS crystal, not the
estimators, which are being tested on *relative* and *cross-method* agreement
here. The absolute is a separate calibration job (`SCOPE_CAL_SOURCE_SCALE`
territory) and unchanged by this work.

[#18]: https://github.com/DavidClawson/OpenScope-2C53T/issues/18
[#26]: https://github.com/DavidClawson/OpenScope-2C53T/issues/26
[#27]: https://github.com/DavidClawson/OpenScope-2C53T/issues/27
