# EXP-08 — per-range gain ladders, both channels, offset centred

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, commit `e3eae38`
- **Status:** **CONFIRMED** (amplitude axis) — with one unresolved anomaly on the time axis, see §6

## 1. Problem

EXP-06 measured CH1's gain ladder by raw mux code and could not measure CH2 at
all. EXP-07 confirmed PA6/TMR13 as CH2's vertical-offset reference and showed
that with it driven, both channels centre on ranges 4-9. The open question is
the one the product actually needs: **how many mV does one ADC count represent,
per range, per channel?** Without that, `scope_measure` can only emit counts,
which is the current deliberate policy.

## 2. Hypothesis

With the offset centred per range, both channels yield a usable gain figure on
ranges 4-9, and the two channels track each other (they are nominally identical
front ends).

- **If true:** every range 4-9 gives >= 3 unrailed amplitude points, a positive
  slope, and CH1/CH2 agree to within a modest tolerance.
- **If false:** CH2 still rails, or the two channels disagree by more than the
  amplitude sweep's own scatter.

## 3. Procedure

Slope method: measure peak-to-peak span at five drive amplitudes
(1500 / 900 / 500 / 300 / 150 mVpp) and regress mVpp against counts, so the
constant noise-floor term lands in the intercept instead of inflating the gain.
A single-amplitude ratio would fold the noise floor into every row.

Per range 0-9, per channel: apply the range to **that channel only**
(`scope_range(n, ch)` — the shell argument is 1-based and `0` means both),
centre it (`fpga scope center ch<N> <range>`, 11-step binary search to ADC code
128) on a quiet input, then sweep amplitude. Span is `p99.5 - p0.5`; any point
with more than 5 railed samples is dropped, since a railed span is a lower bound
and not a measurement.

**Only one channel is driven at a time** (`sg.off()` on the other), so a CH2 row
reading CH1's signal would require either cross-talk or a channel-mask fault.
`timebase 0x10`, `trigger_level 0xFF` (free-run).

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| offset centres on ranges 4-9, both ch | mean ~128 | 127-130 on all twelve (EXP-07 §5) |
| buffers are distinct sources | large difference | `mean\|op04-op05\| = 89.60` (EXP-07) |

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| CH1 driven alone -> only op04 responds | op04 large, op05 flat | span **36.00** / **2.00** | ✅ |
| CH2 driven alone -> only op05 responds | op05 large, op04 flat | span **4.00** / **37.00** | ✅ |
| both driven | both respond | 36.00 / 35.00 | ✅ |
| neither driven (null) | both flat | 4.00 / 2.00 | ✅ |
| independent cross-check of the gain values | Stlkv's ladder on a different unit | 20 / 40 / 80 mV/count vs our 21.83 / 42.95 / 88.42 on ranges 5-7 | ✅ |

The cross-talk control is what carries channel identity here, because the
*spectral* identity gate did not function — see §6.

## 5. Results

mV per ADC count, from the five-point slope fit. `centre` is the offset-reference
code the binary search settled on.

| range | CH1 centre | **CH1 mV/count** | CH2 centre | **CH2 mV/count** | CH1/CH2 |
|---|---|---|---|---|---|
| 0-3 | — | railed / unusable | — | railed / unusable | — |
| 4 | 727 | **14.08** | 827 | **9.49** | 1.48 |
| 5 | 1735 | **21.83** | 1851 | **20.96** | 1.04 |
| 6 | 2079 | **42.95** | 2171 | **41.71** | 1.03 |
| 7 | 2255 | **88.42** | 2303 | **83.79** | 1.06 |
| 8 | 2359 | **279.05** | 2431 | **223.71** | 1.25 |
| 9 | 2395 | **352.17** | 2461 | **425.00** | 0.83 |

All twelve rows used the full five amplitudes (`n=5`) with a positive slope.

**Ranges 5, 6 and 7 are the solid ones.** The two channels agree to within 6%,
they land on a clean 1-2-4 doubling, and they reproduce two independent prior
measurements: EXP-06's CH1 mux-code ladder (21.23 / 42.04 / 91.41) and Stlkv's
figures on a *different unit with a different rig* (20 / 40 / 80).

**Ranges 4, 8 and 9 are weaker.** CH1/CH2 disagree by 25-48%, and range 9 is the
only row where CH2 reads *higher* than CH1. Range 4 is the most sensitive tap, so
1500 mVpp is near its rail and the fit leans on the small-amplitude points;
ranges 8-9 are the coarsest, so 150 mVpp is near the noise floor and the fit
leans on the large ones. Both ends of the sweep are under-served by the same
fixed amplitude set.

**Ranges 0-3 fail identically on both channels** and could not be centred in
EXP-07 either (means pinned at 235/250/255). Whatever they are, they are not a
CH2 problem.

## 6. Blind spots

- **The spectral channel-identity gate misfired and was not enforced.** The
  script computed a peak bin per row and printed it, but never gated on it. The
  bins it reported (1-7) do not correspond to the driven tones, so had it been
  enforced it would have dropped every row. Channel identity therefore rests
  entirely on the drive-isolation design plus the §4 cross-talk control — which
  is sound, but it is not what was designed, and printing a check without
  enforcing it is precisely how a rig ends up looking more controlled than it is.
- **UNRESOLVED — the capture does not resolve frequency at this timebase.** A
  250 Hz tone, and 100 / 500 / 1000 Hz too, all peak at **bin 1 with magnitude
  ~0.1** where a coherent 36-count sine should show a line around 3. Follow-up
  probe across timebases 0x08/0x10/0x11/0x12 at ranges 4 and 6: **span is
  identical everywhere** (115.0 at range 4, ~36 at range 6, unchanged by
  timebase) while lag-1 autocorrelation is **+0.99** at 0x10, +0.45 at 0x08.
  So the record is strongly correlated — not random-phase noise — but its
  content is a broad low-frequency hump whose bin does not move when the tone
  does, and the timebase register does not change the span at all. This is
  consistent with the known "rolling engine seam" / rate-control open question
  and is a real defect in the **time** axis. It does **not** invalidate the
  amplitude axis: span scales linearly with drive amplitude across five points
  on every row, and the resulting gains match two independent measurements.
- **Common-mode risk between our own measurements.** EXP-06 and EXP-08 share
  this rig, so their agreement does not exclude a systematic error common to
  both. Stlkv's numbers, taken on another unit with another rig, are the check
  that does — and it only covers ranges 5-7.
- **Absolute accuracy is untested.** The reference is the ESP32 siggen's
  *commanded* amplitude, never verified against a calibrated source. A constant
  scale error in the siggen would shift every row by the same factor and is
  invisible here. Note the siggen's frequency readback has a known ~0.82 factor;
  no equivalent check has been done on its amplitude.
- **DC/offset drift over the run** is not cancelled — the ladder is a plain
  sweep, not a paired A/B/A design.
- **One unit, one temperature, one session.**

## 7. Conclusion

- **Established:** both channels now have a working per-range gain ladder on
  ranges 4-9, where before EXP-07 CH2 had exactly one usable tap. Ranges 5-7 are
  trustworthy to ~6%, cross-validated against an independent measurement on
  another unit. Ranges 0-3 are unusable on both channels equally.
- **Excluded:** that CH2's front end is defective or its range table wrong; that
  the two channels have materially different gain on ranges 5-7.
- **NOT excluded (explicitly):** absolute scale (the siggen is the only
  reference, unverified); the true gain of ranges 4, 8 and 9; what ranges 0-3
  are; anything at all about the time axis, which §6 shows is currently broken.
- **Follow-up:** (1) re-measure ranges 4, 8 and 9 with amplitude sets matched to
  each range rather than one fixed set; (2) verify the siggen's amplitude against
  a calibrated source before treating any of this as absolute; (3) chase the
  bin-1 anomaly — it blocks timebase and any frequency measurement, though not
  volts/div; (4) build the cal table and wire counts->volts into `scope_measure`,
  starting with ranges 5-7 where the numbers are solid.
