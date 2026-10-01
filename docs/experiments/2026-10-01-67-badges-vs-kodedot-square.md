# EXP-67 — do the on-screen badges (Vpp, Vrms, frequency) report a known square correctly, through the same functions the UI calls?

- **Date:** 2026-10-01 (pre-registered before the run)
- **Unit:** unit #3 (V1.4)
- **Build:** evidence v3 (`feat/usb-wedge-evidence` @ `170d9f3`, `Build: Oct  1 2026 16:56:51`): the timebase table of this build is unit #1's (0x0E–0x14 measured, 0x0D provisional, 0x0A–0x0C and 0x06–0x09 no rate); EXP-63's rows are NOT in it. Scripts: `bench/signal-source-abstraction` (`exp19_badge_validation.py --source kodedot`, SimBench `vdiv`/`measure` for the dry run).
- **Status:** OPEN

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
| range 2 (no cal): Vpp refuses on every read | `-` on all 5 reads | | |
| code 0x0C (no rate in this build): frequency refuses on every read, Vpp still reports | `-` / number | | |
| `badge sources` k1_uV = table k for the range (scope_cal.c, raw × 0.92) | matches | | |

A refusal control that fails voids the whole run: a pipeline that reports where it must refuse cannot be trusted where it reports.

## 5. Results
_Empty until run._

## 6. Blind spots
- V_ref is the unit's own meter mode (EXP-60/64): Vpp/Vrms agreement is scope-vs-meter consistency; a wrong rail reading would move both.
- One amplitude (the rail); nothing about linearity or the clipping edge is tested; r5 would clip (164 counts + the low rail at 128).
- A square's Vpp depends on edge overshoot (probe, clamp): the Vpp criterion is weak by design; Vrms carries the decision.
- The frequency badge is tested only at four codes with a rate in this build; EXP-63's new rows are not in this firmware, so their badges are untested here (a build from PR #51 would test them: `--codes 0x0D 0x0C 0x0B 0x0A --norate-code 0x05`).
- Only CH1's badges are decoded by the script (CH2 contributes `pp2`/`Vpp2` only).

## 7. Conclusion
- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**
- **Follow-up:**
