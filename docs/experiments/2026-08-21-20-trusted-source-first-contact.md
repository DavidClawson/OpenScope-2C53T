# EXP-20 — first trusted source on the bench: USB control both ways, timebase confirmed, vertical SCALE ≈ 0.90 (preliminary)

> **✅ CORRECTED, same session (later that evening): the DC-path negative in §3
> is WITHDRAWN, and SCALE is now confirmed a THIRD way.** With the JDS6600's
> *controllable* DC offset (verified encoding: reg 27/28, 1000 = 0 V, 1 LSB =
> 10 mV) as a clean DC source, a sweep 0→2.8 V DC-coupled at range 7 tracks the
> capture **mean linearly, no railing** (DC gain **77.28 mV/ct**). A 1.616 V
> offset reads mean ≈ 102 — exactly where the linear fit predicts, **not** the
> rail. So the AA battery's "rail at 1.6 V" was an ARTIFACT (almost certainly
> the generator still connected to CH1, fighting the battery), NOT a DC-path
> limitation. **The DC path reads absolute DC fine.** And its gain (77.28)
> matches the *AC* range-7 gain (78.12) to ~1 %, so DC and AC paths share a
> gain — a third independent read on **SCALE ≈ 0.87** (AC r6 0.919 / AC r7
> 0.884 / DC r7 0.874). Still uncommitted: the JDS6600 offset DAC is the same
> ~1–2 % class as its amplitude, so this is stronger internal consistency, not
> yet metrology-grade. See §3 note.

- **Date:** 2026-08-21
- **Unit:** bench unit #1
- **Build:** running image (`version` = Build Aug 20 2026 14:59:16), CDC shell up
- **Source:** **JDS6600** DDS generator (USB `/dev/ttyUSB0`, CH340) — the first
  crystal-referenced source this project has put on CH1. Plus one AA battery
  (1.616 V, agreed by two handheld DMMs) as a DC reference.
- **Status:** MIXED. Three solid positives, one documented negative. **No
  firmware changed** — `SCOPE_CAL_SOURCE_SCALE` stays `1.0f`.

## 1. Problem

Every absolute quantity this project has published traces to the ESP32 bench
source, which was never checked against a trusted instrument (its frequency
delivers 0.825× commanded — EXP-14). The vertical gain table (`scope_cal.c`,
EXP-08) could be off by one uniform factor; `SCOPE_CAL_SOURCE_SCALE` exists to
absorb exactly that, and CLAUDE.md flagged it as "one multiply away, set it when
a trusted source is on the bench." Tonight a JDS6600 arrived.

## 2. What was established (positives)

### 2a. USB control works both directions

- **Scope CDC shell** (`2e3c:5740`, `/dev/ttyACM0`): full shell, `version`,
  `help`, `fpga scope *`, `opread` — all responding.
- **JDS6600** (`/dev/ttyUSB0`): read/write over its ASCII register protocol.
  Read back the exact front-panel state (`:r21=0` sine, `:r23=1000000,0` =
  10.000 kHz, `:r25=5000` = 5.000 V, `:r27=1000` = 0 offset, `:r29=500` = 50%).
- **WRITE FORMAT (not a quirk — the documented format):** the JDS6600 protocol
  specifies writes as `:w25=<value>.<CR><LF>` — **the trailing period is part of
  the spec**, on every write. `:w25=2500.` takes; `:w25=2500` (no period)
  returns `:ok` and is silently no-op'd. The real asymmetry is that the
  *frequency* register tolerates the omission (lenient parsing), which is what
  first misled us into thinking amplitude needed something special — it doesn't;
  the period was simply missing. Missing it froze amplitude at a stale 20 V for
  the first sweep attempt (see §3). Documented on the sigrok wiki
  (https://sigrok.org/wiki/Joy-IT_JDS6600) and in a maintained Python lib
  (WimDH/JDS6600); a `bench.py` driver should just emit the period on every write.

### 2b. Timebase 0x0E confirmed against a trusted source

Closed loop: commanded the generator to 5 kHz and 10 kHz over USB; the scope's
own `fpga scope freq` on its own capture read **4.99 kHz** and **9.99 kHz**
respectively at timebase `0x0E`. Since freq = fs·bin/N, reading 9.99 kHz for a
crystal-true 10 kHz **confirms `0x0E = 49,930 S/s`** (EXP-17) to ~0.1% — the
**first cross-technology agreement on an absolute quantity** in this project.
Different source, different tech, no correction applied.

### 2c. Vertical SCALE ≈ 0.90 (PRELIMINARY)

Amplitude sweep 1→5 Vpp (JDS amplitude confirmed = **Vpp**, from a 20 V accident
railing exactly as 20 Vpp should), sine at **200 Hz** (≈62 samples/cycle) on
timebase **0x10** (12,490 S/s — below the ~30 kS/s `opread` tearing threshold),
CH1 relay ranges 6 and 7. Slope fit of pp-counts vs Vpp (floor-robust: an
additive noise floor lands in the intercept, not the slope — intercepts came out
+0.9 / +1.0 counts, which *is* the floor):

| range | pp counts (1→5 V) | slope | measured gain | stored | SCALE |
|---|---|---|---|---|---|
| 6 | 27, 51, 76.5, 102.5, 128 | 25.35 cts/Vpp | 39.45 mV/ct | 42.95 | **0.919** |
| 7 | 14, 26, 40, 52, 65 | 12.80 cts/Vpp | 78.12 mV/ct | 88.42 | **0.884** |

Both ranges say the stored gains **over-read ~10%**. Mean SCALE ≈ **0.90**,
cross-range agreement 3.9%.

## 3. What was NOT established (negatives / caveats)

- **NOT committed.** Two reasons: (1) the JDS6600's *amplitude* accuracy is
  ~1–2%, not metrology-grade (it is crystal-accurate for *frequency*, which is
  why 2b is trustworthy and 2c is only preliminary); (2) the 3.9% inter-range
  spread means part of the discrepancy is the range6:range7 *ratio* in the
  table, not a pure uniform scale. Range 6 (bigger counts, less quantization) is
  the more reliable single number at 0.919.
- **[WITHDRAWN — see top banner. The DC path does NOT rail; a controllable
  offset sweep reads DC linearly at 77.28 mV/ct and 1.616 V lands at mean 102.
  The battery rail was an artifact of the generator being co-connected.]**
  ~~The AA battery could not calibrate the vertical scale — the DC path rails.~~
  1.616 V DC, DC-coupled (PC12 HIGH), **saturates the ADC at code 255 at every
  usable range (5/6/7)**, and a full DAC1 offset sweep (0→4095, `trig raw`) does
  **not** budge it. So it is not the offset DAC; the DC path itself has a large
  gain/bias that makes 1.6 V exceed full scale even where the *AC* full-scale is
  ~22 V (range 7). The AC/DC-coupling-toggle trick is also contaminated: the
  AC→DC shifts (130 vs 153 counts at ranges 6/7) are **not** in the 2× ratio the
  gains demand, so a coupling-induced DC offset swamps the battery. **Our front
  end is an AC-trace instrument; it does not read absolute DC yet.** Why 1.6 V
  rails range 7 is an open DC-path question (hidden DC gain stage? fixed frontend
  bias?), left for a dedicated session.

## 4. Conclusion

- **Bankable now:** USB control both ways; `0x0E = 49,930 S/s` confirmed against
  a trusted source; JDS6600 writes take the documented `:w..=X.` trailing-period
  format (freq writes tolerate its omission, amplitude does not).
- **Preliminary:** `SCOPE_CAL_SOURCE_SCALE ≈ 0.90` (ESP32 gains over-read ~10%),
  from the JDS AC sweep. Left uncommitted at `1.0f`.
- **Open:** DC path cannot read absolute DC (rails at 1.6 V, DAC1 can't recover);
  formalize SCALE via `verify_scope_cal.py` once a JDS6600 driver is in
  `bench.py`, and cross-check with a metrology-grade or DC reference before
  baking it in.
- **Follow-up:** add a JDS6600 `Siggen` to `bench.py`; investigate the DC path;
  then either the battery (once DC works) or a better AC reference nails SCALE.
