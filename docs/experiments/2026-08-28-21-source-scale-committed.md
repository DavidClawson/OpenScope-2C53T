# EXP-21 — one source into both channels: the CH1/CH2 gap was the generator, and SOURCE_SCALE = 0.92 is committed

- **Date:** 2026-08-28
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace-ch2` (running image, cold-boot bit-bang config, CH2 boot-center 2544)
- **Source:** JDS6600 on `/dev/ttyUSB0`, crystal-referenced (~1–2% amplitude accuracy)
- **Question:** Is the ~5% CH1-vs-CH2 vertical-scale gap seen earlier a real per-channel
  device difference, or the generator's own CH1/CH2 output mismatch? And commit a
  `SCOPE_CAL_SOURCE_SCALE` (EXP-20 left it at the un-committed `1.0f`).

## Method

One-source-into-both, so the source is the *same* for both channels and cancels out of
the comparison. A single BNC–BNC cable, JDS end fixed on **CH1** the whole time:

- **Pass A:** JDS CH1 → scope **CH1** (op 0x04)
- **Pass B:** JDS CH1 → scope **CH2** (op 0x05)

Both passes identical otherwise: sine, timebase **0x10** (12,490 S/s), channel **centered**
per range, span = median of 7 `spi3 acqread` reads, slope fit of span vs commanded Vpp
(intercept absorbs the noise floor). Ranges 6 (1–5 Vpp) and 5 (0.5–2 Vpp, smaller to avoid
clipping the more sensitive range).

### The undersampling trap (why the first attempt was garbage)

The first Pass B run gave non-monotonic spans (4 Vpp reading *below* 3 Vpp) and per-range
SCALE swinging 0.70–1.20. Cause: the default timebase after cold boot is **0x12 = 2495 S/s**,
and I was driving 1 kHz — right at Nyquist (1247 Hz). Undersampling a near-Nyquist sine gives
a beat whose min/max wanders read-to-read. Dropping to 100–200 Hz at 2495 S/s made it *worse*
(span ~10), because `acqread`'s effective window is short and low frequencies then fit <1 cycle.
Setting timebase **0x10** (the config EXP-20 used) fixed it completely: at fixed 5 Vpp the span
held 128–130, **stdev < 1 count**. Every number below is from timebase 0x10.

## Results

| | CH1 (op 0x04) | CH2 (op 0x05) |
|---|---|---|
| raw slope r6 (cts/Vpp) | **25.2** | **25.3** |
| raw slope r5 (cts/Vpp) | 48.6 | 49.4 |
| SCALE r6 | 0.9239 | 0.9476 |
| SCALE r5 | 0.9426 | 0.9658 |
| mean SCALE | 0.9332 | 0.9567 |

r6 spans (CH1): 28 / 53 / 78 / 103 / 129 at 1–5 Vpp — clean linear.
r6 spans (CH2): 28 / 53 / 79 / 104 / 129 — clean linear.

## Conclusions

1. **The channels are physically identical.** The raw slopes — the measurement *before* any
   stored gain — are CH1 25.2 vs CH2 25.3 cts/Vpp, agreeing to 0.4%. The same source through
   each frontend at the same range produces the same counts. So the earlier apparent ~5%
   CH1/CH2 gap was **the JDS's own CH1-vs-CH2 output mismatch** (Pass A used JDS CH1; the old
   CH2 run used JDS CH2), **not a device difference**. → one common `SOURCE_SCALE` is correct.

2. **The small residual SCALE difference (0.933 vs 0.957) is a stored-table artifact, not
   physical.** It comes entirely from the table's own CH1/CH2 rows differing at r6 (42.95 vs
   41.71 mV/ct, ~3% — EXP-08 measurement noise), which no single scale can reconcile and which
   is within the source's amplitude accuracy anyway.

3. **Cross-session reproduction.** CH1 r6 today = 0.9239 vs EXP-20's 0.919 — 0.5% apart, two
   sessions. EXP-20's higher ranges ran lower (r7 0.884, DC 0.874), so the grand center across
   ranges is **~0.92** with a mild range trend.

## Action

`SCOPE_CAL_SOURCE_SCALE` 1.0 → **0.92** (`firmware/src/ui/scope_cal.h`). At 1.0 every reported
voltage was ~8% high (the raw table was taken through the ESP32 siggen, which over-reads gain).
Correcting a measured, reproduced ~8% error with a constant good to ~2% is strictly better than
leaving it uncorrected. Gain is now right at the top of each range (5 Vpp r6 → 5.10 V reported,
+1.9%); small amplitudes still carry an additive noise-floor bias (span-based Vpp does not
subtract an intercept) — that is a measurement-engine issue, not a calibration one.

`tests/test_scope_cal.c` label expectations updated for the new scale (r6 1.37V → 1.26V,
r5 699mV → 643mV); the source-scale-uniformity test reads the constant dynamically and is
unaffected. **No table rows were hand-edited** — the whole point of the one-constant design.

## Still open

- Absolute scale is limited by the JDS's ~1–2% amplitude accuracy; a lab-grade source would
  refine (not overturn) 0.92.
- Ranges 4/8/9 PROVISIONAL, 0–3 rail — unchanged by this.
- The ~3% CH1/CH2 row inconsistency at r6 (42.95 vs 41.71) is real EXP-08 noise; the channels
  are identical, so those two rows *should* be nearly equal. A future re-measure could tighten
  them, but the test forbids hand-editing.
