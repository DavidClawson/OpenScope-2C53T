# EXP-56 — the trigger offset is 28 codes on range 7 too: it is digital

- **Date:** 2026-09-22
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, commit `90e1cfc` (Build Sep 22 2026 13:17:33, the v8 image)
- **Status:** **CONFIRMED** (H1: digital offset) — control held

## 1. Problem
Trigger-modes S2 (a): what ADC code does the comparator fire at when reg `0x08` holds L?
EXP-53 (50j) and EXP-55 found the comparator sees the record + 28 on range 5. If that 28 is
a digital bias between the comparator's and the record's view of the ADC, it is the same
number on every range and the UI marker is drawn at code − 28 everywhere. If it is an
analog offset ahead of the attenuator, it scales with range gain (range 7 is 4.05× range 5:
88.42 vs 21.83 mV/count), and the marker needs a per-range table.

## 2. Hypothesis
- H1 digital: range 7 shows r[512] = L − 28 ± 3 at every level.
- H2 analog: range 7 shows a different constant, ~7 if the offset is a fixed input voltage.
- Falsifier for both: L − r[512] differs between levels on one range.
- Void: the range-5 control does not give 28.

Predictions were written into `scripts/exp56_level_offset_range.py` before the run.

## 3. Procedure
2 Hz triangle from the JDS6600 (2.0 Vpp on range 5, 8.0 Vpp on range 7, 0 V offset).
Per range: `fpga scope vdiv 1 <r>`, DAC1 centred with `fpga scope center ch1 <r>` at `0x10`
(the no-argument form walks all ten ranges and hung the shell for a minute on the first
attempt), then `0x12` (2500 S/s, 0.82 period per record). One AUTO record gives the span;
three levels are chosen so the crossing sits at 50/30/70 % of it, level code = crossing + 28
(`fpga scope level n` maps to code 128 + 1.2 n by readback). Per level: NORMAL, 3 s settle,
8 records; pointer k = largest jump, un-rotate, r[512] is the trigger sample.

**Preconditions verified by readback:**
| what | expected | measured |
|---|---|---|
| range 5 centre | DAC1 near 2811 (EXP-55 session) | `DAC1=2815 (median=131)` |
| range 7 centre | — | `DAC1=2559 (median=139)` |
| span range 5 / 7 | ~100 counts | 105..204 / 65..170 |
| level codes | as requested | 183 164 205 / 146 125 167 |

## 4. Control
Range 5, same session, first:

| control | expected | measured | passed? |
|---|---|---|---|
| L = 183 → r[512] = 155 | 155 ± 3 | 153–156, median offset **29** | yes |
| L = 164 → 136 | 136 ± 3 | 135–136, median **28** | yes |
| L = 205 → 177 | 177 ± 3 | 176–178 (+ two at 182, 185), median **28** | yes |

## 5. Results
| range | mV/count | L | predicted L − 28 | r[512] (8 records) | median L − r[512] |
|---|---|---|---|---|---|
| 5 | 21.83 | 183 | 155 | 156 153 156 154 153 154 153 155 | 29 |
| 5 | 21.83 | 164 | 136 | 136 135 136 136 136 135 135 135 | 28 |
| 5 | 21.83 | 205 | 177 | 178 177 177 177 178 176 185 182 | 28 |
| 7 | 88.42 | 146 | 118 | 119 118 117 117 118 119 117 118 | 28 |
| 7 | 88.42 | 125 | 97 | 97 97 99 88 99 89 92 97 | 28 |
| 7 | 88.42 | 167 | 139 | 139 139 139 140 138 138 139 138 | 28 |

43 of 48 records within ±3 of the prediction; the five outliers (182, 185; 88, 89, 92) are
6–9 counts off in the direction of the waveform and sit at the levels nearest the span
ends. Both edges appear at every level (`edges` column in the log: mixed r/f).
Log: `reverse_engineering/captures/exp56/exp56.log`; records: `exp56_records.npz`.

**Tolerance, derived:** at 2 Hz and 2500 S/s the triangle moves 0.16 count per sample, so a
±3-count band is ±19 samples of trigger-position uncertainty, well inside the 504–523 spread
EXP-53 measured for the crossing before the pointer. The five outliers are consistent with a
mis-found pointer (the seam jump is small when the waveform value one record-length apart
happens to match) rather than a comparator effect; the medians are unaffected.

## 6. Blind spots
- Two ranges, one channel. Ranges 0–3 rail and 4/8/9 are provisional in `scope_cal`; the
  offset there is untested, and CH2 is untested.
- The pointer is found by the largest jump; a wrong k moves r[512] by the waveform's local
  slope, which is why the ±3 band, not the outliers, is the reading.
- Level codes were read back from the shell, not from the register.
- An offset that is digital but *also* range-dependent by a small amount (< 3 codes)
  would not show at this resolution.

## 7. Conclusion
- **Established:** the comparator fires at (reg 0x08 − 28) in record codes on range 5 and
  range 7 alike, at six levels spanning 97..177 in record scale. The offset is **digital**.
- **Excluded:** an analog (voltage) offset, which would read ~7 on range 7.
- **NOT excluded:** a different constant on the railing/provisional ranges or on CH2.
- **Follow-up:** draw the UI level marker at code − 28 (one constant, `scope_ui`), and
  the S2 (a) criterion of trigger-modes is met: the README's *Trigger level* row promotes
  to S2. The level still has no user control (dev plan 2.2).
