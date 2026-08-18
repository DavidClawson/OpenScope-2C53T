# EXP-01 — Are op04 and op05 the same memory, or two converters?

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace-slow`, framing fix `ce22b49`
- **Status:** **CONFIRMED** — two physically distinct converters

## 1. Problem
CH2 is absent. Two hypotheses survived every prior test: (a) the readout
serialises BSRAM_1 for both opcodes, so CH2's data exists but is unreachable;
(b) two converters both watch the CH1 analog node. These have opposite
consequences and nothing so far distinguished them.

## 2. Hypothesis
Two physically distinct converters have different offset and gain — that is
*why* per-device calibration exists on this platform. So:
- **If (b)**, op05 differs from op04 by a consistent offset that survives drift.
- **If (a)**, op04 and op05 are draws from one source and any difference is
  purely time-ordering, which an identical-opcode control will reproduce.

## 3. Procedure
Content cannot separate them — both predict the same waveform at a read-time
lag. Attempting to freeze the buffer failed: all nine values of the trigger
register left ~1000/1024 bytes differing between consecutive op04 reads, so
byte-identity was unavailable.

Instead, each trial reads three windows and compares the **middle** against the
**midpoint of the two outer** ones, which cancels linear drift exactly. Run with
a **flat DC input**, deliberately: with no periodic content there is no window-
phase wobble to bias the mean, and framing errors cannot matter either.

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| `04 04 04`, DC — identical opcode, same read order/timing/drift | ~0 | **−0.003, t=−0.35** | ✅ |
| `05 04 05`, 100 Hz — roles swapped, effect must MIRROR | +3.6 | **+3.429, t=+54.9** | ✅ |

## 5. Results
| condition | statistic | mean | t |
|---|---|---|---|
| DC, `04 05 04` | op05 − midpoint(op04) | **−2.645 codes** | **−357** |
| DC, `04 04 04` control | — | −0.003 | −0.35 |
| 100 Hz, `04 05 04` | offset | −3.622 | −30.4 |
| 100 Hz, `04 05 04` | gain (sd) | −0.362 (~1%) | −7.8 |
| 100 Hz, `05 04 05` | offset | +3.429 | +54.9 |
| 100 Hz, `04 04 04` control | — | −0.086 | −0.84 |

The ~3-code offset then reproduced in 27 further rows across the reg
`02`/`06`/`07` sweeps that evening.

## 6. Blind spots
- Does not identify *which* physical converter op05 reads, only that it differs.
- Cannot distinguish "different converter" from "same converter through a
  different digital path applying a constant offset" — though stock applies
  gain/offset cal in MCU software, not in the fabric, so no such path is known.
- Says nothing about where either converter's input is connected.

## 7. Conclusion
- **Established:** op05 reads a different physical converter than op04. A
  re-read of one memory cannot produce a consistent 2.6-code offset while the
  identical-opcode control sits at zero.
- **Excluded:** hypothesis (a). Also removes Komzpa's `debugclk_hw_top` image
  from the critical path — deciding this was its entire purpose. Also kills the
  AD9288 channel-B-standby lead: a sleeping converter cannot deliver CH1's
  waveform at matching amplitude.
- **NOT excluded:** where the two converter inputs are connected.
- **Supersedes:** the 2026-08-16 retraction of "op04/op05 differ by ~3 codes ⇒
  two converters". That retraction was itself the error — the means do drift
  read-to-read, but drift is exactly what a paired design cancels, and the
  control proves it. **A correct result was thrown away by testing it sloppily.**
