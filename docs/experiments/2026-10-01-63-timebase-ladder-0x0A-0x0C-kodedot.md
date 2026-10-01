# EXP-63 — timebase codes 0x0A–0x0C and 0x06–0x09, with a source that can reach them

- **Date:** 2026-10-01
- **Unit:** bench unit #3 (V1.4). The rate table being tested is unit #1's.
- **Build:** unit #3 on a build of PR #41/#47 (record the `build:` line the script prints). Scripts at
  `bench/signal-source-abstraction` @ `2ce8a3a`. Source: `kodedot_sigsource` on a Kode Dot (ESP32-P4).
- **Status:** OPEN — pre-registered before any capture. Thresholds below are fixed now.

## 1. Problem
Eight of 21 timebase codes have a rate (0x0D–0x14, 500 S/s – ~124 kS/s). `0x0A`–`0x0C` have none because
the ESP32 source topped out near 4.5 kHz and its tones landed in bins 1–15: a limit of the instrument,
not evidence about the device (`scope_timebase.c`, EXP-18 §6). `0x06`–`0x09` are INCOHERENT (EXP-12,
EXP-15), but EXP-15 only read them through `spi3 opread`; its §6 says the acq path "was not tried".
**Which sample rates do 0x0A–0x0C select, and were 0x06–0x09 incoherent in the device or in our
instrument?** (README §3, "what is still open".)

## 2. Hypotheses
**H1 — the ladder continues.** `0x0C`/`0x0B`/`0x0A` = 250 k / 500 k / 1.25 M S/s (×2, ×2, ×2.5 from `0x0D`,
the 2.5/2/2 cadence of 500 → 1250 → 2500 → 5000). One-tone form: a 25 kHz square at `0x0C` lands at bin
1024·25000/250000 = **102.4** (accept 101–104, ±2%). It must beat: uniform ×2 (`0x0A` = 1.0 MS/s, a
250 kHz tone at bin 256 instead of 204.8), saturation (one fit at every code, EXP-15's symptom), and no
rate at all. Verdict per code, read against the per-tone table and not the printed label (EXP-15 §5a):

| outcome | rule |
|---|---|
| H1 confirmed | R² ≥ 0.99, no tone with read-to-read spread > 20 bins, fold worst miss ≤ 12 bins, fs within ±2% of prediction |
| H1 refuted, rate measured | those three coherence tests pass but fs is > 2% off the ladder: the number is the finding |
| no rate (NOT a refutation) | a coherence test fails: peak bin matches no ladder rate, or the fold fails. Code stays `0.0f` |

Scale-free ladder shape, ±3% each: fs(0x0C)/fs(0x0D) = 2, fs(0x0B)/fs(0x0C) = 2, fs(0x0A)/fs(0x0B) = 2.5.

**H2 — `0x06`–`0x09` were incoherent in the instrument, not the device.** Run 3 reads each code through
both paths with identical tones, unit, build and source, so the read path is the only difference between
its two columns. **O1:** acq coherent (R² ≥ 0.99, no `!!` line) with a different fs per code while opread
scatters: EXP-15 was the read path. **O2:** acq coherent but one fs for all four (~1.25 kS/s): the codes do
not select distinct rates, a device property, instrument excluded. **O3:** acq as scattered as opread:
EXP-15 survives a new path, source, unit and build. Run 3's tones (40–420 Hz) can only see fs ≲ 20 kS/s;
if the ladder continued up (0x09 = 2.5 M, 0x08 = 5 M) they sit below bin 1 and O3 is expected. Run 4
therefore places tones for 5 MS/s. Prediction: **acq R² ≥ 0.99, fs within ±2% of 5.0 M, fold ≤ 12 bins.**
Falsifier: scatter > 20 bins on acq at both placements. Opread is expected torn at 5 MS/s (EXP-10); in
run 4 its column is the contrast, not a test.

## 3. Procedure
**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| `version` (the `build:` line, all four runs) | one string, recorded | |
| `fpga scope timebase`, no argument, by hand: before run 1 / after each run (script restores 0x10) | `0xNN (reg 0x01 = 0xNN)`, equal / `0x10 (reg 0x01 = 0x10)  12490 S/s` | |
| Dot `s`: sigsrc up, `clk_hz`; `ppm` | `80000000`; 35 (reported only, not applied to the fits) | |
| probes ×1 (switch checked by eye), tips J3 pin 9, springs pins 10/11 | both channels on one pin | |

```
python3 scripts/measure_sample_rate.py --source kodedot --codes 0x0F 0x0E 0x0D                    # run 1
python3 scripts/measure_sample_rate.py --source kodedot --codes 0x0C 0x0B 0x0A                    # run 2
python3 scripts/measure_sample_rate.py --source kodedot --codes 0x06 0x07 0x08 0x09 --path both   # run 3
python3 scripts/measure_sample_rate.py --source kodedot --codes 0x08 --path both --fs-guess 0x08=5e6  # run 4, optional
```
Each run starts with the 0x10 two-path control (unless `--no-control`), sets range 6 on both channels and
ends with the Dot clock check. Run 1 first; runs 2–4 only if run 1's controls pass. Tones sit at
1/2/5/10/20% of the expected rate (taken from the script's own functions, no device touched). The fit uses
the Dot's reported Hz. Only CH1 is read.

| code | fit tones (Hz) | predicted bins (N=1024) | fold tones (0.62–1.71 × fitted fs) |
|---|---|---|---|
| 0x0F / 0x0E / 0x0D | 250…5000 / 500…10000 / 1200…25000 | 10–205 on all three | 15–43 k / 31–85 k / 77–210 k |
| 0x0C | 2500 5000 12000 25000 50000 | 10.2 20.5 49.2 **102.4** 204.8 | ~160–430 kHz |
| 0x0B | 5000 10000 25000 50000 100000 | 10.2 20.5 51.2 102.4 204.8 | ~310–860 kHz |
| 0x0A | 12000 25000 62000 120000 250000 | 9.8 20.5 50.8 98.3 204.8 | ~0.78–2.1 MHz |
| 0x06–0x09, run 3 | 40 80 130 200 300 420 (EXP-12/15 set, for comparability) | < 1 if fs ≥ 2.5 M | n/a |
| 0x08, run 4 | 50 k 100 k 250 k 500 k 1 M | 10.2 20.5 51.2 102.4 204.8 | ~3.1–8.6 MHz |

No firmware or table change here. A later `scope_timebase.c` edit writes the fitted value, never the round one.

## 4. Control
Record before section 5; same session, same path as the new codes. 1% is about twice the one cross-unit
gap on record (0x10 vs Stlkv's rig, 0.43%). Integer-bin quantisation alone gives ~0.1% rms (0.3% worst).

| control | expected | measured | passed? |
|---|---|---|---|
| 0x10 opread vs acq (script's own check) | agree < 5%, `PASS` | | |
| 0x10, each path vs table 12,490.0 | within 1% | | |
| 0x0F vs table 24,979.1 | within 1% | | |
| 0x0E vs table 49,930.1 | within 1% | | |
| 0x0D vs table 123,662.7 (PROVISIONAL, −1.07% from round 125 k) | within 1.5%; ~125,000 passes | | |
| fold at 0x0F / 0x0E / 0x0D | worst miss ≤ 12 bins | | |
| closing `source at end` clock check | `PASS` | | |

Runs 2–4 repeat the 0x10 control at their start; if it fails, that run is **VOID, not negative**. A 0x0D
miss beyond 1.5% voids runs 2–4: it is the nearest known code and uses the method the new codes use.
H2's own control is the opread/acq contrast under identical conditions.

## 5. Results
_Empty until the runs are done. Raw `fs`, R², per-tone bins and the fold table, pasted unedited._

## 6. Blind spots
- **One unit, one session.** Unit #3 against unit #1's table; a 1% control miss could be unit-to-unit
  clock spread, not method. Nothing here speaks for other boards.
- **Square-wave harmonics.** The script does **not** use `window_for`; it takes a global top-1
  `peaks(v, 1)` over bins 1–512. The 3rd harmonic of a 50% square is 1/3 of the fundamental (−9.5 dB)
  and sits at 3f (folded to fs−3f past fs/2), so it cannot win, and it shares the fundamental's bin only
  at fs/4, beyond the tones (≤ 0.2 fs). The 9th harmonic of a 0.2 fs tone does alias onto the fundamental's
  bin: ≤ 11% magnitude, no bin shift. `window_for` (±25% + 2 bins) would exclude both, but it would make
  H1 partly self-fulfilling and park a ×2-uniform `0x0A` (+25%) on its edge, so it is a post-hoc
  `band_peak` diagnostic for bad fits only. Watch for a fit at ~1/3 of the predicted rate.
- **Crystal.** The Dot's X1 is ~+35 ppm fast, uncorrected; the scope's own clock is unmeasured. Both are
  < 0.004%, 30× below fit resolution. No absolute claim finer than ~0.3%.
- **Aliasing.** Fit tones stay ≤ 0.2 fs of the *predicted* rate (≤ 0.4 fs if the rate is half). A rate
  below ~0.4× the prediction aliases them and reads as INCOHERENT, not as a rate. Fold tones are placed
  from the *fitted* rate, so a wrong fit moves its own test, and the fold output prints no magnitude:
  a miss on the highest tones is weak evidence.
- **Acq path above 124 kS/s is extrapolated.** The control proves it only up to 0x0D. Snapshot and
  hold behaviour at 250 k–1.25 MS/s is assumed.
- **Dithered tones.** Several auto-placed tones (62 k, 120 k, most fold tones > 300 kHz) are `dithered` per
  `plan_table`: ≤ 250 ppm off, period jitter ≤ 12.5 ns, reported Hz = long-run average. Harmless to a bin;
  if a fit is marginal, rerun with `--tones` at 2^a·5^b values.
- **Probe loading.** Two ×1 probes on one pin (68 Ω series) round MHz edges and cut amplitude. That cannot
  move a peak bin; it can only sink a weak fold tone into the noise.
- **No two-shapes control.** One pin feeds both channels, so a readout that mirrored CH1 into CH2 would
  not be caught. Only CH1 is read: CH2's timebase is assumed, not tested.
- **reg 0x01 is not read back between codes.** `sc.timebase()` discards the reply, including `reg 0x01
  NOT written`. The in-data guard is distinct fits per code; one value everywhere is EXP-15's symptom.
- Run 4 tests one rate for one code (0x09 = 2.5 M, 0x07 = 12.5 M, 0x06 = 25 M need their own runs).

## 7. Conclusion
- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**
- **Follow-up:**
