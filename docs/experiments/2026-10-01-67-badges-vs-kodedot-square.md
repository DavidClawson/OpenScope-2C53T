# EXP-67 — do the on-screen badges (Vpp, Vrms, frequency) report a known square correctly, through the same functions the UI calls?

- **Date:** 2026-10-01 (pre-registered before the run)
- **Unit:** unit #3 (V1.4)
- **Build:** evidence v3 (`feat/usb-wedge-evidence` @ `170d9f3`, `Build: Oct  1 2026 16:56:51`): the timebase table of this build is unit #1's (0x0E–0x14 measured, 0x0D provisional, 0x0A–0x0C and 0x06–0x09 no rate); EXP-63's rows are NOT in it. Scripts: `bench/signal-source-abstraction` (`exp19_badge_validation.py --source kodedot`, SimBench `vdiv`/`measure` for the dry run).
- **Status:** **CONFIRMED at r6** (all four codes, all three badges, refusals correct); **r7: frequency badge intermittent** (0/10 answered at 0x10, 7–9/10 at 0x0E/0x0F with 41 counts of span) and Vrms at the 6% edge — a finding, not a pass; r8/r9 context only.

## 1. Problem
EXP-19 validated the badge pipeline (`fpga scope measure`, the functions the status bar calls) on unit #1 against the ESP32 sine source. On unit #3 nothing on screen has been checked against a known signal. With a 0 V / 3.303 V square of exact frequency on both channels: do Vpp, Vrms and the frequency badge report it, through the UI's own `vdiv` path (display state + relay bank), and do they refuse where the table says they must (a range with no cal, a code with no rate)?

## 2. Hypothesis
Per (code, range) cell the badge pipeline reports: **Vrms within 6%** of 0.5 × V_ref (a 50% square, mean removed; V_ref = 3303 mV, the unit's own meter reading with probes attached, EXP-64), **Vpp within 12%** of V_ref (pp is an extreme-value statistic: edge overshoot and noise inflate it, so it is secondary), **frequency within 2%** of the Dot's register-reported Hz, answered on every read. Decision on **r6 and r7** at 0x10/0x12/0x0F/0x0E; r8/r9 (10–16 counts of span) are context, where ±2 counts of quantisation alone is >10%.

- **Falsifier (pipeline):** a cell at r6/r7 misses Vrms by >6% or the frequency by >2% — a wrong k for the channel/range, a wrong rate, or a mean-removal error (an un-removed mean would read Vrms ≈ 0.75 × V_ref for this square).
- **Falsifier (refusals):** range 2 (cal tier NONE) reports a Vpp, or the no-rate code (0x0C in this build) reports a frequency. Then the badges invent numbers, which is the defect EXP-19 was written to catch.
- **Expected secondary:** Vpp high by a few % at r6 (overshoot of the edges through the 68 Ω + clamp + probe) and by more at r8/r9 (quantisation); the Vrms/Vpp ratio therefore slightly below 0.5.

## 3. Procedure
Dot on GPIO9, `pwm`, 50% duty, tones per code from the script (0x10 400 Hz, 0x12 100 Hz, 0x0F 800 Hz, 0x0E 1600 Hz), actual Hz read back from the Dot. CH1 = crocodile lead, CH2 = ×1 probe (EXP-64 §3 wiring). Range set through `fpga scope vdiv 1 r` (the button's path), centring `fpga scope center ch1 r` (acq path — the badges read the acq buffer, so this is the right path here, unlike EXP-64's opread), 10 reads per cell.
```
python3 scripts/exp19_badge_validation.py --source kodedot --v3v3 3.303 --ranges 6 7 8 9 --codes 0x10 0x12 0x0F 0x0E --norate-code 0x0C --reps 10
```
**Preconditions verified by readback:** `badge sources:` line per cell (rng1, k1_uV, tb, fs) must show the range just set and the code's rate; `version`.

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| dry run against SimBench (no hardware) | the script parses `measure` and reaches a verdict | done before the run: parses, grid printed, verdict computed | yes |
| range 2 (no cal): Vpp refuses on every read | `-` on all 5 reads | `Vpp refuses on all reads: True` | yes |
| code 0x0C (no rate in this build): frequency refuses on every read, Vpp still reports | `-` / number | `freq refuses on all reads: True; Vpp still reports: True` | yes |
| `badge sources` k1_uV = table k for the range (scope_cal.c, raw × 0.92) | matches | r6 k1_uV=39510 (42.95 × 0.92 = 39.51 mV/ct ✓), r7 81350, r8 256726, r9 324000 (table × 0.92 ✓); tb/fs per code as the table | yes |

A refusal control that fails voids the whole run: a pipeline that reports where it must refuse cannot be trusted where it reports.

## 5. Results
`dumps/exp67_run1.log` (M lines and `badge sources` lines elided here, 10 reads per cell):
```
=== EXP-67 run 1 18:20:02
# source: Kode Dot LEDC square, 0 V / 3303 mV (DMM-measured rail); square, Vrms/Vpp expected 0.5
build: version | OpenScope 2C53T | Build: Oct  1 2026 16:56:51 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

== timebase 0x10, tone 400.0 Hz ==
fpga scope timebase 10
boot reconcile: pushed the persisted code to the FPGA
timebase 0x10 (reg 0x01 = 0x10)  12490 S/s  2.56ms/div
>
  r6 3303mVpp -> Vpp 3398.2mV (2.9%)  Vrms err 1.4%  rms/pp 0.4929  f 400.26Hz (0.07%) [10/10 freq]
  r7 3303mVpp -> Vpp 3660.6mV (10.8%)  Vrms err 5.5%  rms/pp 0.476  f NoneHz (None%) [0/10 freq]
  r8 3303mVpp -> Vpp 4877.8mV (47.7%)  Vrms err 35.2%  rms/pp 0.4578  f 400.72Hz (0.18%) [10/10 freq]  <-- FAIL
  r9 3303mVpp -> Vpp 3240.0mV (-1.9%)  Vrms err -12.1%  rms/pp 0.4479  f 400.21Hz (0.05%) [3/10 freq]  <-- FAIL

== timebase 0x12, tone 100.0 Hz ==
fpga scope timebase 12
boot reconcile: pushed the persisted code to the FPGA
timebase 0x12 (reg 0x01 = 0x12)  2495 S/s  12.83ms/div
> 
>
  r6 3303mVpp -> Vpp 3437.7mV (4.1%)  Vrms err 1.3%  rms/pp 0.4866  f 99.89Hz (-0.11%) [10/10 freq]
  r7 3303mVpp -> Vpp 3660.6mV (10.8%)  Vrms err 5.0%  rms/pp 0.4736  f 99.87Hz (-0.13%) [10/10 freq]
  r8 3303mVpp -> Vpp 4877.8mV (47.7%)  Vrms err 35.6%  rms/pp 0.4592  f 99.81Hz (-0.19%) [10/10 freq]  <-- FAIL
  r9 3303mVpp -> Vpp 3564.0mV (7.9%)  Vrms err -16.8%  rms/pp 0.3854  f 99.91Hz (-0.09%) [10/10 freq]  <-- FAIL

== timebase 0x0F, tone 800.0 Hz ==
fpga scope timebase 0F
boot reconcile: pushed the persisted code to the FPGA
timebase 0x0F (reg 0x01 = 0x0F)  24979 S/s  1.28ms/div
> 
>
  r6 3303mVpp -> Vpp 3437.7mV (4.1%)  Vrms err 1.2%  rms/pp 0.486  f 800.51Hz (0.06%) [10/10 freq]
  r7 3303mVpp -> Vpp 3660.6mV (10.8%)  Vrms err 6.1%  rms/pp 0.4785  f 800.7Hz (0.09%) [9/10 freq]  <-- FAIL
  r8 3303mVpp -> Vpp 4877.8mV (47.7%)  Vrms err 36.7%  rms/pp 0.4627  f 800.57Hz (0.07%) [10/10 freq]  <-- FAIL
  r9 3303mVpp -> Vpp 3240.0mV (-1.9%)  Vrms err -18.2%  rms/pp 0.4169  f NoneHz (None%) [0/10 freq]  <-- FAIL

== timebase 0x0E, tone 1600.0 Hz ==
fpga scope timebase 0E
boot reconcile: pushed the persisted code to the FPGA
timebase 0x0E (reg 0x01 = 0x0E)  49930 S/s  641us/div
> 
>
  r6 3303mVpp -> Vpp 3398.2mV (2.9%)  Vrms err 0.7%  rms/pp 0.4896  f 1600.08Hz (0.0%) [10/10 freq]
  r7 3303mVpp -> Vpp 3660.6mV (10.8%)  Vrms err 4.8%  rms/pp 0.4728  f 1600.23Hz (0.01%) [7/10 freq]
  r8 3303mVpp -> Vpp 4621.1mV (39.9%)  Vrms err 33.4%  rms/pp 0.4768  f 1601.68Hz (0.11%) [8/10 freq]  <-- FAIL
  r9 3303mVpp -> Vpp 3564.0mV (7.9%)  Vrms err -17.4%  rms/pp 0.3826  f 1600.27Hz (0.02%) [4/10 freq]  <-- FAIL

== refusal controls ==
  range 2 (no cal): Vpp refuses on all reads: True
  code 0x0C (no rate): freq refuses on all reads: True; Vpp still reports (independent axes): True

RESULT: FAIL (9 grid misses, controls True/True/True)
# grid rows (markdown):
| code | rng | commanded mVpp | badge Vpp | err% | Vrms err% | rms/pp | badge f | err% | freq answered |
|---|---|---|---|---|---|---|---|---|---|
| 0x10 | 6 | 3303 | 3398.2 | 2.9 | 1.4 | 0.4929 | 400.26 | 0.07 | 10/10 |
| 0x10 | 7 | 3303 | 3660.6 | 10.8 | 5.5 | 0.476 | None | None | 0/10 |
| 0x10 | 8 | 3303 | 4877.8 | 47.7 | 35.2 | 0.4578 | 400.72 | 0.18 | 10/10 |
| 0x10 | 9 | 3303 | 3240.0 | -1.9 | -12.1 | 0.4479 | 400.21 | 0.05 | 3/10 |
| 0x12 | 6 | 3303 | 3437.7 | 4.1 | 1.3 | 0.4866 | 99.89 | -0.11 | 10/10 |
| 0x12 | 7 | 3303 | 3660.6 | 10.8 | 5.0 | 0.4736 | 99.87 | -0.13 | 10/10 |
| 0x12 | 8 | 3303 | 4877.8 | 47.7 | 35.6 | 0.4592 | 99.81 | -0.19 | 10/10 |
| 0x12 | 9 | 3303 | 3564.0 | 7.9 | -16.8 | 0.3854 | 99.91 | -0.09 | 10/10 |
| 0x0F | 6 | 3303 | 3437.7 | 4.1 | 1.2 | 0.486 | 800.51 | 0.06 | 10/10 |
| 0x0F | 7 | 3303 | 3660.6 | 10.8 | 6.1 | 0.4785 | 800.7 | 0.09 | 9/10 |
| 0x0F | 8 | 3303 | 4877.8 | 47.7 | 36.7 | 0.4627 | 800.57 | 0.07 | 10/10 |
| 0x0F | 9 | 3303 | 3240.0 | -1.9 | -18.2 | 0.4169 | None | None | 0/10 |
| 0x0E | 6 | 3303 | 3398.2 | 2.9 | 0.7 | 0.4896 | 1600.08 | 0.0 | 10/10 |
| 0x0E | 7 | 3303 | 3660.6 | 10.8 | 4.8 | 0.4728 | 1600.23 | 0.01 | 7/10 |
| 0x0E | 8 | 3303 | 4621.1 | 39.9 | 33.4 | 0.4768 | 1601.68 | 0.11 | 8/10 |
| 0x0E | 9 | 3303 | 3564.0 | 7.9 | -17.4 | 0.3826 | 1600.27 | 0.02 | 4/10 |
=== end 18:26:12
```
Decision cells (r6, r7), per §2:

| code | r6 Vrms err | r6 Vpp err | r6 f err (answered) | r7 Vrms err | r7 Vpp err | r7 f err (answered) |
|---|---|---|---|---|---|---|
| 0x10 | +1.4% | +2.9% | +0.07% (10/10) | +5.5% | +10.8% | **none (0/10)** |
| 0x12 | +1.3% | +4.1% | −0.11% (10/10) | +5.0% | +10.8% | −0.13% (10/10) |
| 0x0F | +1.2% | +4.1% | +0.06% (10/10) | **+6.1%** | +10.8% | +0.09% (**9/10**) |
| 0x0E | +0.7% | +2.9% | +0.00% (10/10) | +4.8% | +10.8% | +0.01% (**7/10**) |

- **r6: every criterion holds** — Vrms within 1.4%, Vpp +2.9–4.1% (the predicted edge overshoot; rms/pp 0.487–0.493), frequency within 0.11% and answered on every read, at all four codes. Vpp through the UI's `vdiv` path uses exactly the table k (`badge sources` k1_uV = raw × 0.92).
- **r7: the frequency badge is not reliable at 41 counts of span.** 0/10 answers at 0x10 (400 Hz, 31 samples per period), 9/10 at 0x0F (800 Hz, 31 s/p), 7/10 at 0x0E (1600 Hz, 31 s/p), 10/10 at 0x12 (100 Hz, 25 s/p). The unanswered reads print `per1_smp100=-`: the period detector declines. The same signal at r6 (84 counts) is answered 40/40. Vrms at r7 sits at +4.8–6.1% (the 6% bar is met at three codes and missed by 0.1% at 0x0F), Vpp +10.8% everywhere — both the r7 row's k being ~5% high for this unit (EXP-64 saw the same: CH1 r7 static 3773–3866 mV for 3303) and the pp inflation.
- **r8/r9 (context):** r8 Vrms +33–37% and Vpp +40–48% on all codes — not quantisation (±2 of 13 counts is 15%): the r8 row's k does not fit this unit's CH1, as EXP-64 run 1b/2 also measured (CH1 r8 static 4744–5013 mV for 3303). r9 Vrms −12 to −18% with frequency answered 0–4/10 (10 counts of span).
- Refusals: range 2 reports no Vpp; 0x0C reports no frequency and still reports Vpp. The pipeline refuses where the table says.

### Runs 2–3 — the r7 flicker, chased to its cause (`dumps/exp67_run2_r7.log`, `exp67_run3_r7.log` + `exp67_run3_r7_raw.log`)
- Rerun of r7 alone: 0x10 answered 10/10 (0/10 in run 1), 0x0F 5/10, 0x0E 8/10, 0x12 6/10 — the flicker moves between codes; by hand at r7/0x10 `fpga scope measure 6` answered 6/6. Not reproducible per code: a per-read effect.
- With every `measure` reply logged (`--raw-log`, added for this), the declined reads show **`per1_smp100=3103` with `f1_mHz=-`**: the Schmitt period detector finds the period (31.03 samples = 800 Hz at 24,979 S/s) on every read; what refuses is `scope_freq_estimate()` (`scope_freq.c`), the FFT estimator the badge actually uses, whose harmonic-aware sharpness must reach 0.90.
- Replica of `analyse()` on real records (`spi3 read 1024`, 800 Hz, 0x0F), sharpness with the firmware's harmonic windows at integer multiples of the integer peak bin vs. windows at multiples of the interpolated fractional bin:

| range | pp | fundamental bin | sharp (firmware: integer harmonics) | sharp (fractional harmonics) |
|---|---|---|---|---|
| r7 | 44 | 32.82 | 0.876 → refused | 0.930 |
| r7 | 45 | 33.06 | 0.790 → refused | 0.790 |
| r7 | 45 | 32.77 | 0.898 → refused | 0.962 |
| r7 | 45 | 32.76 | 0.905 | 0.970 |
| r7 | 44 | 33.24 | 0.735 → refused | 0.695 |
| r7 | 45 | 33.00 | 0.804 → refused | 0.804 |
| r6 | 87 | 32.76 | 0.906 / 0.904 / 0.905 / 0.906 / 0.905 (one record at pp 109: 0.900 → refused) | 0.967–0.970 |

  Three effects, the third found in run 4: (1) **harmonic-window drift**: the fundamental sits at bin 32.76, the windows are placed at 33·h ± 1, so from the 7th harmonic (229.3 vs window 230–232) upward the square's harmonic power is counted as "not signal"; that alone leaves a clean square at 0.90–0.91, exactly on the gate, which is why r6 passes by 0.005 and r7 (less signal over the same quantisation noise) does not. Windows at round(32.76·h) ± 1 lift the same records to 0.93–0.97. (2) **Records with a discontinuity** (fundamental bin reading 33.00–33.24 instead of 32.76, sharpness 0.70–0.80 under either rule): a phase jump inside the raw acq buffer — the seam the renderer un-rotates before drawing (EXP-22) — which the period detector tolerates (one odd cycle) and the FFT does not. The measure path feeds `fpga_get_ch1_buf()` raw. (3) **A stale record after a tone change at slow codes** — see run 4 — which is the script's settle, not the device.

## 6. Blind spots
- V_ref is the unit's own meter mode (EXP-60/64): Vpp/Vrms agreement is scope-vs-meter consistency; a wrong rail reading would move both.
- One amplitude (the rail); nothing about linearity or the clipping edge is tested; r5 would clip (164 counts + the low rail at 128).
- A square's Vpp depends on edge overshoot (probe, clamp): the Vpp criterion is weak by design; Vrms carries the decision.
- The frequency badge is tested only at four codes with a rate in this build; EXP-63's new rows are not in this firmware, so their badges are untested here (a build from PR #51 would test them: `--codes 0x0D 0x0C 0x0B 0x0A --norate-code 0x05`).
- Only CH1's badges are decoded by the script (CH2 contributes `pp2`/`Vpp2` only).

## 7. Conclusion
### Run 4 — r7 and r6 on a build with the harmonic windows on the interpolated comb (`dumps/exp67_run4.log`, `exp67_run4_raw.log`)
Build: `guest-coldtrace` from `fix/freq-sharpness-fractional-harmonics` (origin/main + the one-line change in `scope_freq.c`), `Build: Oct  1 2026 18:39:14`; same wiring, same script, same tones.

| code | r7 freq answered (runs 1–3 → run 4) | r7 f err | r6 freq answered | r7 Vrms err | r7 Vpp err |
|---|---|---|---|---|---|
| 0x0F | 9/10, 5/10, 0/10 → **10/10** | +0.22% | 10/10 | +3.6% | +9.6% |
| 0x10 | 0/10, 10/10, 10/10 → **10/10** | +0.06% | 10/10 | +5.7% | +13.3% |
| 0x0E | 7/10, 8/10 → **10/10** | +0.01% | 10/10 | +4.7% | +10.8% |
| 0x12 | 10/10, 6/10, 7/10 → **6/10** | −0.18% | 10/10 | +6.5% | +10.8% |

- The three codes with the fundamental between bins (32.8) now answer on every read; the comb placement was their cause. r6 answers 40/40 as before.
- r7 at 0x12 (100 Hz at 2,495 S/s) still declines 4 of 10 — **WITHDRAWN 2026-10-01 (same evening): not a seam.** The raw log shows those four reads (0–3, identical) with `per1_smp100=312`: a 3.12-sample period at 2,495 S/s is **800 Hz, the previous cell's tone**. At a slow code a record takes 410 ms to fill and the acq buffer refreshes it slowly; the script's 0.6 s settle after the tone change was too short, so the first reads measured a stale record of the old tone, whose fundamental (bin 328) has no harmonics below Nyquist — refused honestly. Run 3's first reads at 0x12 (`per=605`: 6.05 samples = 412 Hz, the 400 Hz of the cell before) were the same thing. The script now settles for at least three record durations. The seam hypothesis stays only for the by-hand records with the fundamental read at 33.0–33.24 (§ runs 2–3), which were not taken across a tone change.
- Refusal controls still hold (range 2: no Vpp 5/5; 0x0C: no frequency 5/5, Vpp reported).

- **Established:** on unit #3 the badge pipeline reports a known 3.303 V square correctly at r6 through the UI's own range path (Vrms ≤ 1.4%, frequency ≤ 0.11%, answered on every read, four codes from 2.5 k to 50 kS/s), and refuses on a range without cal and on a code without a rate. The frequency badge flickers on a clean square because `scope_freq_estimate()`'s sharpness gate (0.90) is met by only ~0.005 for a square whose fundamental falls between bins (harmonic windows placed at integer multiples of the integer peak bin lose the ≥ 7th harmonics), and because the raw acq buffer sometimes carries a seam the FFT cannot forgive; the Schmitt period detector reports the right period on every one of those reads.
- **Excluded:** a wrong k selection or a missing ×0.92 in the badge path (k1_uV matches the table); mean-removal errors in Vrms (a square's mean would give ≈ 0.75 × V_ref); a frequency badge that invents a value where the table has no rate.
- **NOT excluded (explicitly):** that the r7 and r8 rows of unit #1's table are wrong for this unit by ~5% and ~40% on CH1 (two independent methods now agree; one unit, one amplitude); the detector's actual criterion (hysteresis in counts? edges per record?) — to be read in `scope_measure.c`; CH2's badges.
- **Follow-up:** (1) **done** — harmonic windows at multiples of the interpolated fundamental bin (`fix/freq-sharpness-fractional-harmonics`, host test on a 44-count square between bins that the integer comb fails at 0.916), run 4 above is its hardware check; (2) measure on the un-rotated record, or fall back to the Schmitt period when it is valid with many cycles — the maintainer's call (issue, core src); (3) rerun this r7 row on a build with (1) and count answers. Rerun r7 with a build carrying EXP-63's table to see whether the badge answers at 0x0D–0x0A.
