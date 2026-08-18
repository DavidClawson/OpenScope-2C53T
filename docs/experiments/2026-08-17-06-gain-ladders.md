# EXP-06 — Vertical gain ladders, and why CH2 cannot be calibrated yet

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` @ 18:45:30
- **Status:** **PARTIAL** — CH1 characterised, CH2 blocked on a missing offset reference

## 1. Problem
Volts/div is a label only: `scope_measure` deliberately emits ADC counts and
refuses to invent volts without per-range calibration. What is the real
mV/count for each mux tap on each channel?

## 2. Hypothesis
Each channel's 3-bit mux is an attenuator ladder, so each code has a fixed
mV/count that a slope fit will recover. Falsifier: codes that produce no usable,
non-railed response at any drive level.

## 3. Procedure
Slope method (Stlkv's, and the right one): span at five amplitudes per code,
fit mV against counts so the constant noise floor drops out; railed points
excluded. Mask 3 throughout; CH1 on op04, CH2 on op05.

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| CH1 taps agree with the earlier magnitude sweep | code 2 ≈ 2× code 1's gain | 228.30/108.83 = 2.10× (magnitudes gave 1.90×) | ✅ |
| independent ladder, second unit | Stlkv's top rows | 21/42/91/228/492 vs 20/40/80/200/400 | ✅ |

## 5. Results

**CH1 (PE4/PE5/PE6, op04)** — codes 0 and 4 are grounded taps.

| code | 5 | 7 | 6 | 2 | 1 | 3 |
|---|---|---|---|---|---|---|
| mV/count | **21.23** | **42.04** | **91.41** | **108.83** | **228.30** | **491.67** |
| ratio | 1.00 | 1.98 | 4.31 | 5.13 | 10.75 | 23.16 |

These match **Stlkv's top five rows** (20/40/80/200/400). His sensitive half —
0.4 to 8 mV/count — is unreachable here, so **this mux is only the coarse half
of the ladder and a ~×50 stage is not being enabled.**

**CH2 (PB11/PB10/PA10, op05)** — one usable tap, and not for gain reasons:

| PD13 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| 1 | mean 0, railed | 232 | 255 | **8.82 mV/ct** | 0 | 255 | 255 | 255 |
| 0 | 0 | 0 | 0 | 0 | 0 | 29 (725 railed) | 0 | 0 |

Taps park at **different DC levels** and rail there regardless of drive
amplitude. Even the usable tap sits at mean 26/255. PD13 moves the levels; no
setting gives a usable window across taps.

## 6. Blind spots
- Only the **attenuated** path was measured — PC12/PA15 were held LOW throughout
  (see the correction below), so the sensitive half of both ladders is untested.
- Slope fits assume the generator's amplitude dial is accurate; its *frequency*
  reads ~18% low, so absolute values carry that risk.
- One probe, one unit, no probe-attenuation factor applied.

## 7. Conclusion
- **Established:** CH1's coarse ladder, six taps, cross-validated against a
  second unit's independent measurement.
- **Established:** CH2 cannot be calibrated until it has a **vertical offset
  reference**. Its taps rail at fixed DC levels — a baseline problem, not a gain
  problem. CH1's offset is DAC1 on PA4 (our trigger code sets it); **CH2's is
  TMR13 CH1 PWM on PA6, which our firmware has never programmed.** That is the
  blocking item, and it is a firmware change rather than a bench sweep.
- **⚠ CORRECTION to EXP-03:** "PC12 is a hard on/off (LOW passes, HIGH kills)" is
  probably WRONG. That sweep measured spectral magnitude only, and a *railed
  flat line* reads near-zero exactly like a dead input. With 2000 mVpp on a much
  more sensitive path, railing is expected. PC12 is most likely the
  sensitive/attenuated path select the relay table always said it was. Ninth
  instrument slip of the session, same family: a metric that could not
  distinguish the two states it was being used to decide between.
- **Also retracted:** "CH2's range table is broken." The bit-0 semantics look
  correct; the measurements were taken with the other half of the hardware off.
- **Follow-up:** re-measure both ladders with PC12/PA15 HIGH at small
  amplitudes; bring up TMR13 on PA6; then build the cal table and wire
  counts→volts into `scope_measure`.
