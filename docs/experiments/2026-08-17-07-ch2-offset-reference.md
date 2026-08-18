# EXP-07 — PA6 / TMR13 is CH2's vertical-offset reference

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, commit `e3eae38` (build stamp `Aug 17 2026 23:26:37`)
- **Status:** **CONFIRMED**

## 1. Problem

EXP-06 measured CH1's full gain ladder but got exactly **one** usable tap out of
CH2: every other mux code parked at a fixed DC level (means of 0, 232, 255, 26)
and railed there regardless of how hard the input was driven. That is the
signature of a missing **vertical offset**, not a broken gain table — but the
distinction had never been tested, and the two failure modes look identical from
a spectral magnitude alone.

CH1's offset reference is DAC1 on PA4, which our firmware arms mid-scale and
`fpga scope center` tunes per range. Stock appears to drive CH2's from a **TMR13
CH1 PWM-DAC on PA6** (TMR13 setup at `0x0802B0FE`, `C1DT` write at `0x08008C3A`,
ripcord contract 38), which our firmware has never programmed. `HARDWARE_PINOUT.md`
has carried PA6 as "candidate, unconfirmed" since it was decoded.

## 2. Hypothesis

PA6 / `TMR13_C1DT` is CH2's vertical-offset reference, the exact analogue of
DAC1 for CH1.

- **If true:** sweeping `TMR13_C1DT` across 0..4095 moves CH2's capture mean
  across the ADC range, and leaves CH1's mean alone.
- **If false:** CH2's mean does not move while the CH1 control demonstrably
  does, in the same session through the same path.

A third outcome is possible and worth naming in advance: if a sweep moved
**both** channels, the pin would be a shared rail rather than a per-channel
offset, and the hypothesis would be wrong in a different way.

## 3. Procedure

`make guest-coldtrace` (deliberately **not** `guest-coldtrace-ch2` — that target
arms TMR13 at boot, which would erase the "before" half of the A/B). Flashed via
the factory IAP volume; the device auto-booted into the app and CDC enumerated as
`2e3c:5740`.

Runtime knobs added in `7ea472f`; `scope_trigger_ch2_raw()` self-inits, so no
build flag is required to drive TMR13:

```
trig  raw <0-4095>     -> DAC1 (PA4)
trig2 raw <0-4095>     -> TMR13_C1DT (PA6)
fpga scope center [ch1|ch2] [0-9]
```

Signal state: siggen **off** on both channels for the sweep, so the input is flat
DC and the mean is a clean statistic with no window-phase wobble. `timebase 0x10`,
`trigger_level 0xFF` (free-run), frontend range 5 on both channels.

Sweep `[0, 500, 1500, 2048, 2500, 3500, 4095]` on each reference in turn, and at
**every point read both `op04` and `op05`**, making it a 2x2 rather than two
independent measurements.

**Settle = 1.2 s per point.** The capture buffer free-runs, and a read taken too
soon returns the stale pre-write buffer — on 2026-08-14 a 10 ms settle made
`fpga scope center` look broken for an entire session and the first fix was aimed
at the wrong cause. 1.2 s is at least one full 1024-sample buffer even at the
slowest rate this bench has measured (~1.07 kS/s => ~957 ms).

**Preconditions verified by readback** (not assumed):

| what | expected | measured |
|---|---|---|
| CDC build stamp | the image just flashed | `Aug 17 2026 23:26:37` |
| two buffers are distinct sources | large difference | `mean\|op04-op05\| = 89.60` with a triangle on CH1 and a square on CH2 |

The precondition matters: if the PC1/PC2 channel mask had drifted to 1 or 2, both
buffers would carry the **same** channel and every "CH2" number below would be a
CH1 number wearing a label. Two different waveform *shapes* were used rather than
an anti-phase pair, because a display or readout that inverts can make anti-phase
look like one source drawn twice.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| DAC1 sweep moves CH1 (positive) | large `op04` swing | **span 255.00**, monotonic | ✅ |
| DAC1 sweep leaves CH2 alone (specificity) | ~0 `op05` swing | **span 0.00** | ✅ |
| TMR13 sweep leaves CH1 alone (specificity) | ~0 `op04` swing | **span 0.00** | ✅ |
| buffers distinct before measuring | large difference | 89.60 | ✅ |

The positive control ran in the same session, through the same shell, the same
serial link and the same read path as the test.

## 5. Results

Mean of one 1024-sample read window at each point.

**Sweeping DAC1 (PA4) — control:**

| code | 0 | 500 | 1500 | 2048 | 2500 | 3500 | 4095 | span |
|---|---|---|---|---|---|---|---|---|
| op04 (CH1) | 0.00 | 0.00 | 98.64 | 167.89 | 224.56 | 255.00 | 255.00 | **255.00** |
| op05 (CH2) | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |

**Sweeping TMR13_C1DT (PA6) — test:**

| code | 0 | 500 | 1500 | 2048 | 2500 | 3500 | 4095 | span |
|---|---|---|---|---|---|---|---|---|
| op04 (CH1) | 255.00 | 255.00 | 255.00 | 255.00 | 255.00 | 255.00 | 255.00 | 0.00 |
| op05 (CH2) | 0.00 | 0.00 | 83.58 | 153.58 | 210.37 | 255.00 | 255.00 | **255.00** |

(CH1 sits pinned at 255 through the second sweep because the first sweep left
DAC1 at 4095. That it stays *exactly* 255.00 at all seven points is the
specificity result, not an artifact.)

The two transfer curves are near-identical in shape and **the same direction** —
both monotonically increasing, usable window roughly 700..3400. TMR13's channel
polarity is active-low in stock's config (`CCTRL C1P`), so an inverted
duty->voltage sense was a live possibility; it does not occur.

**Per-range centering, run immediately afterwards** on a quiet DC input
(`fpga scope center ch1` / `ch2`, 11-step binary search per range, target ADC
code 128):

| range | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| CH1 DAC1 | 2047 | 2047 | 2047 | 7 | **727** | **2047** | **2079** | **2255** | **2359** | **2395** |
| CH1 mean | 171 | 255 | 255 | 250 | 128 | 127 | 128 | 128 | 128 | 128 |
| CH2 TMR13 | 2047 | 2047 | 2047 | 3 | **827** | **2047** | **2171** | **2303** | **2433** | **2463** |
| CH2 mean | 235 | 255 | 255 | 235 | 128 | 125 | 128 | 130 | 128 | 128 |

Ranges 4-9 centre cleanly on **both** channels. CH2's codes run consistently
~50-100 counts above CH1's on the same range — a stable per-channel trim, which
is the shape a real calibration table has.

## 6. Blind spots

- **This does not prove PA6 is the pin.** It proves that writing `TMR13_C1DT`
  moves CH2's DC level. TMR13 CH1's default output *is* PA6 and stock never
  remaps it (no `0x4001001C` literal in the image), but this test would report
  the same result if the effect reached CH2 by some other route from TMR13.
  A meter or scope on the physical pad would close that gap; maksidze has been
  asked on issue #18 whether PA6 routes into CH2's front end through an RC.
- **Nothing here is calibrated.** The sweep shows the offset *moves*; it says
  nothing about volts per count, linearity, or absolute accuracy.
- **DC only.** Every number is the mean of a flat-input capture. Offset
  behaviour under a live AC signal, at speed, is not tested.
- **Ranges 0-3 are unexplained.** They fail to centre on both channels
  identically, so this test cannot say whether they are grounded taps, a
  range-table error, or a real hardware limit — only that it is not a CH2
  problem.
- **One unit.** Bench unit #1 only.
- **The two sweeps were not interleaved.** They ran back to back, so a slow
  drift over the ~20 s between them would appear as a channel difference. The
  effect size (255 vs 0.00 exactly) makes this implausible, but the design that
  would have excluded it outright is an interleaved A/B/A.

## 7. Conclusion

- **Established:** writing `TMR13_C1DT` moves CH2's capture mean across the full
  ADC range and does not touch CH1; DAC1 does the reverse. PA6/TMR13 is CH2's
  vertical-offset reference, the analogue of DAC1/PA4 for CH1, and the polarity
  is **not** inverted. With it driven, CH2 centres on six ranges (4-9) instead of
  the single usable tap EXP-06 found.
- **Excluded:** "CH2's range table is broken" — retired for the second time.
  Also excluded: an inverted TMR13 duty sense, and a shared-rail reading in which
  one reference moves both channels.
- **NOT excluded (explicitly):** that the effect reaches CH2 from TMR13 by a
  route other than the PA6 pad; anything about gain, linearity or absolute volts;
  whatever is wrong with ranges 0-3; behaviour on any other unit.
- **Follow-up:** per-range gain ladder on both channels with the offset centred
  (EXP-08), then the cal table and counts->volts in `scope_measure`. Fold the
  boot-time arm (`make guest-coldtrace-ch2`) into the default path once the
  ladder confirms CH2 is usable end to end.
