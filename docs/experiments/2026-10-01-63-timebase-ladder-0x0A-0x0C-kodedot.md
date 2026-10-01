# EXP-63 — timebase codes 0x0A–0x0C and 0x06–0x09, with a source that can reach them

- **Date:** 2026-10-01
- **Unit:** bench unit #3 (V1.4). The rate table being tested is unit #1's.
- **Build:** unit #3 on PR #48's branch (`feat/usb-wedge-evidence` @ `f912890` = #41 + the evidence record; no
  timebase or acquisition change vs #41), `Build: Oct  1 2026 14:36:14`. Scripts at `bench/signal-source-abstraction`
  @ `66a1802`. Source: `kodedot_sigsource` on a Kode Dot (ESP32-P4), standalone, output GPIO9 (§3).
- **Status:** **CONFIRMED** — H1 confirmed at 0x0C/0x0B/0x0A; H2 = O1 (the instrument, not the device) at 0x08 and 0x09; 0x07/0x06 fitted beyond the plan, PROVISIONAL. Pre-registered before any capture; thresholds unchanged.

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
**Deviations from the pre-registration (2026-10-01, before any run):**
- Source pin is **GPIO9** (EXP2, J3 pin 4), not GPIO14: the operator's probes were already there. GPIO9 sits behind the ESDA6V1SC6 array (~190 pF) with the 68 Ω series resistor (τ ≈ 13 ns), which rounds MHz edges; the fits use the fundamental's peak bin, so the edges do not matter for the rates, but the fold tones above ~1 MHz lose harmonic content. `kodedot_sigsource` was changed to make GPIO9 its default output (`p 14` / `p 21` still select the low-capacitance pins).
- The Dot runs `kodedot_sigsource` **standalone** (its flash was rewritten with the operator's consent: no kodeOS on this unit, no panel assembly, no KTD2026 LED). The SDK's LED driver retried every 40 ms and logged each attempt on the console, so (a) that log is silenced in the app and (b) `bench._DotSerialTransport` now reads replies to the `>ok`/`>err` terminator instead of quiet time (commit 66a1802).
- **CH1 is the crocodile-clip lead** (no attenuator, ×1 by construction) for every run below; the operator's ×1 probe does not conduct. Shown before run 1, with the Dot's square on GPIO9 at 0x10, range 6: the lead's channel saw 86–89 codes p-p at 1000 Hz, the probe's channel 2–4 codes (noise) through (a) a static `dc 0`/`dc 1` step (lead: 141 → 225 codes; probe: 81.3 → 81.3), (b) a pin finder — the app was extended to drive any of the 14 J3 GPIOs (`p <gpio>`) and the square was walked over all of them: only GPIO9 moved the lead's channel, nothing moved the probe's — and (c) swapping the two BNCs at the scope: the square followed the lead to CH1 and the probe stayed flat on CH2. CH1's input itself is fine (at range 0 it picks up 50 Hz, 22× the floor, as an open high-impedance input does; after the swap it shows the square). The lead is on GPIO9, its ground clip on the GND pair beside GPIO14/GPIO21. CH2 (dead probe) is set to the same range and ignored.
- `ppm 35` is set by hand after each Dot reset (not persisted).

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| `version` (the `build:` line, all four runs) | one string, recorded | `OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE)` (runs 1, 2) |
| `fpga scope timebase`, no argument, by hand: before run 1 / after each run (script restores 0x10) | `0xNN (reg 0x01 = 0xNN)`, equal / `0x10 (reg 0x01 = 0x10)  12490 S/s` | before run 1: `timebase 0x08 (reg 0x01 = 0x08)  0 S/s --/div` (the UI's own setting; the script sets 0x10 itself); after run 5b: `timebase 0x10 (reg 0x01 = 0x10)  12490 S/s  2.56ms/div` |
| Dot `s`: sigsrc up, `clk_hz`; `ppm` | `80000000`; 35 (reported only, not applied to the fits) | `clock src=PLL_F80M clk_hz=80000000 (SPLL=12x X1 40 MHz, /6)`; `xtal ppm=+35.000` |
| probes ×1 (switch checked by eye), tips J3 pin 9, springs pins 10/11 | both channels on one pin | **deviation:** CH1 = crocodile lead on GPIO9 (the ×1 probe is dead, see above); CH2 = the dead probe, ignored |

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
| 0x10 opread vs acq (script's own check) | agree < 5%, `PASS` | run 1: 12489 / 12489 S/s, `paths agree to 0.0%  PASS` | yes |
| 0x10, each path vs table 12,490.0 | within 1% | 12489 S/s both paths: −0.01% | yes |
| 0x0F vs table 24,979.1 | within 1% | 25009 S/s: +0.12% | yes |
| 0x0E vs table 49,930.1 | within 1% | 50020 S/s: +0.18% | yes |
| 0x0D vs table 123,662.7 (PROVISIONAL, −1.07% from round 125 k) | within 1.5%; ~125,000 passes | 124968 S/s: +1.06% vs the table, −0.03% vs round 125 k | yes |
| fold at 0x0F / 0x0E / 0x0D | worst miss ≤ 12 bins | 1 / 1 / 1 bins | yes |
| closing `source at end` clock check | `PASS` | `Kode Dot clk 80000000 Hz … PASS` (run 1) | yes |

Runs 2–4 repeat the 0x10 control at their start; if it fails, that run is **VOID, not negative**. A 0x0D
miss beyond 1.5% voids runs 2–4: it is the nearest known code and uses the method the new codes use.
H2's own control is the opread/acq contrast under identical conditions.

## 5. Results
### Run 2 — 0x0C / 0x0B / 0x0A (`dumps/exp63_run2.log`, unedited)
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.0%  PASS

=== reg 0x01 = 0x0C, acq read ===
  table: no rate (0.0f); tones placed for ~250000 S/s (ladder GUESS)
    0x0C acq   : fs =    250089 S/s  R2 +1.0000   [2500->10  5000->20  11999->49  25000->102  50000->205]
    0x0C acq    fold check (Nyquist 125044 Hz):
       160000 Hz  predicted  369  measured  369  miss    0
       200000 Hz  predicted  205  measured  205  miss    0
       280003 Hz  predicted  122  measured  123  miss    1
       339996 Hz  predicted  368  measured  369  miss    1
       429999 Hz  predicted  287  measured  287  miss    0
      worst miss 1 bins -> FOLD HOLDS

=== reg 0x01 = 0x0B, acq read ===
  table: no rate (0.0f); tones placed for ~500000 S/s (ladder GUESS)
    0x0B acq   : fs =    500203 S/s  R2 +1.0000   [5000->10  10000->20  25000->51  50000->102  100000->205]
    0x0B acq    fold check (Nyquist 250101 Hz):
       310002 Hz  predicted  389  measured  389  miss    0
       409994 Hz  predicted  185  measured  184  miss    1
       569997 Hz  predicted  143  measured  143  miss    0
       689980 Hz  predicted  389  measured  389  miss    0
       859998 Hz  predicted  287  measured  287  miss    0
      worst miss 1 bins -> FOLD HOLDS

=== reg 0x01 = 0x0A, acq read ===
  table: no rate (0.0f); tones placed for ~1250000 S/s (ladder GUESS)
    0x0A acq   : fs =   1249691 S/s  R2 +1.0000   [11999->10  25000->20  62000->51  120000->98  250000->205]
    0x0A acq    fold check (Nyquist 624846 Hz):
       769983 Hz  predicted  393  measured  393  miss    0
      1000000 Hz  predicted  205  measured  205  miss    0
      1400055 Hz  predicted  123  measured  123  miss    0
      1699867 Hz  predicted  369  measured  369  miss    0
      2100082 Hz  predicted  327  measured  328  miss    1
      worst miss 1 bins -> FOLD HOLDS

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
Read against §2's rules (per code, per-tone table, not the label):

| code | fitted fs | H1 prediction | Δ | R² | per-tone spread | fold worst miss | verdict |
|---|---:|---:|---:|---|---|---|---|
| 0x0C | 250,089 S/s | 250,000 | +0.04% | 1.0000 | 25 kHz → bin 102 (predicted 102.4; accept 101–104) | 1 bin | **H1 confirmed** |
| 0x0B | 500,203 S/s | 500,000 | +0.04% | 1.0000 | 50 kHz → bin 102 | 1 bin | **H1 confirmed** |
| 0x0A | 1,249,691 S/s | 1,250,000 | −0.02% | 1.0000 | 250 kHz → bin 205 (predicted 204.8; uniform ×2 would put it at 256) | 1 bin (2.1 MHz fold tone) | **H1 confirmed** |

Ladder shape (±3% allowed): fs(0x0C)/fs(0x0D) = 250,089/124,968 = **2.001**; fs(0x0B)/fs(0x0C) = **2.000**; fs(0x0A)/fs(0x0B) = **2.498**. The 2.5/2/2 cadence continues to 1.25 MS/s. Every fit is a distinct rate per code (not EXP-15's one-value symptom), the folds hold through 2.1 MHz through the 68 Ω + ~190 pF of GPIO9, and no fit sits near 1/3 of its prediction (the harmonic trap in §6).

### Run 3 — 0x06–0x09, both paths, EXP-12/15's tones (`dumps/exp63_run3.log`, unedited)
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12519 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->286]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.2%  PASS

=== reg 0x01 = 0x06, opread+acq read ===
  table: no rate (0.0f); tones placed for ~1250 S/s (ladder GUESS)
      !! 420 Hz: reads [241, 193, 160] spread 81 bins — this record is not reproducing itself
    0x06 opread: fs =      3731 S/s  R2 +0.5416   [40->3  80->5  130->8  200->13  300->19  420->193]
      !! 40 Hz: reads [116, 41, 91] spread 75 bins — this record is not reproducing itself
      !! 80 Hz: reads [37, 8, 178] spread 170 bins — this record is not reproducing itself
      !! 300 Hz: reads [19, 2, 89] spread 87 bins — this record is not reproducing itself
      !! 420 Hz: reads [11, 115, 1] spread 114 bins — this record is not reproducing itself
    0x06 acq   : only 1 usable points
    0x06 opread fold check (Nyquist 1866 Hz):
         2300 Hz  predicted  393  measured  102  miss  291
         3000 Hz  predicted  201  measured  128  miss   73
         4200 Hz  predicted  129  measured  141  miss   12
         5100 Hz  predicted  376  measured  198  miss  178
         6400 Hz  predicted  292  measured  218  miss   74
      worst miss 291 bins -> FOLD FAILS, rate not trustworthy

=== reg 0x01 = 0x07, opread+acq read ===
  table: no rate (0.0f); tones placed for ~1250 S/s (ladder GUESS)
      !! 130 Hz: reads [74, 58, 50] spread 24 bins — this record is not reproducing itself
    0x07 opread: fs =      5795 S/s  R2 +0.4284   [40->3  80->5  130->58  200->52  300->58  420->54]
      !! 40 Hz: reads [118, 1, 122] spread 121 bins — this record is not reproducing itself
      !! 80 Hz: reads [5, 182, 7] spread 177 bins — this record is not reproducing itself
      !! 130 Hz: reads [93, 26, 9] spread 84 bins — this record is not reproducing itself
      !! 200 Hz: reads [12, 55, 64] spread 52 bins — this record is not reproducing itself
      !! 420 Hz: reads [18, 93, 1] spread 92 bins — this record is not reproducing itself
    0x07 acq   : only 1 usable points
    0x07 opread fold check (Nyquist 2898 Hz):
         3600 Hz  predicted  388  measured  102  miss  286
         4700 Hz  predicted  194  measured  173  miss   21
         6500 Hz  predicted  125  measured  172  miss   47
         7900 Hz  predicted  372  measured  147  miss  225
         9900 Hz  predicted  299  measured   83  miss  216
      worst miss 286 bins -> FOLD FAILS, rate not trustworthy

=== reg 0x01 = 0x08, opread+acq read ===
  table: no rate (0.0f); tones placed for ~1250 S/s (ladder GUESS)
    0x08 opread: fs =      7598 S/s  R2 +0.8010   [40->13  80->20  130->25  200->26  300->38  420->54]
      !! 40 Hz: reads [6, 204, 301] spread 295 bins — this record is not reproducing itself
      !! 80 Hz: reads [1, 157, 1] spread 156 bins — this record is not reproducing itself
      !! 130 Hz: reads [185, 73, 223] spread 150 bins — this record is not reproducing itself
      !! 200 Hz: reads [206, 412, 401] spread 206 bins — this record is not reproducing itself
      !! 300 Hz: reads [437, 43, 155] spread 394 bins — this record is not reproducing itself
      !! 420 Hz: reads [1, 66, 213] spread 212 bins — this record is not reproducing itself
    0x08 acq   : only 0 usable points
    0x08 opread fold check (Nyquist 3799 Hz):
         4700 Hz  predicted  391  measured   11  miss  380
         6200 Hz  predicted  188  measured   21  miss  167
         8600 Hz  predicted  135  measured   83  miss   52
        10000 Hz  predicted  324  measured   77  miss  247
        13000 Hz  predicted  296  measured    8  miss  288
      worst miss 380 bins -> FOLD FAILS, rate not trustworthy

=== reg 0x01 = 0x09, opread+acq read ===
  table: no rate (0.0f); tones placed for ~1250 S/s (ladder GUESS)
    0x09 opread: fs =     12588 S/s  R2 +0.5113   [40->5  80->10  130->17  200->26  300->24  420->27]
      !! 40 Hz: reads [446, 463, 3] spread 460 bins — this record is not reproducing itself
      !! 80 Hz: reads [123, 213, 198] spread 90 bins — this record is not reproducing itself
      !! 130 Hz: reads [143, 349, 1] spread 348 bins — this record is not reproducing itself
      !! 200 Hz: reads [301, 143, 409] spread 266 bins — this record is not reproducing itself
      !! 300 Hz: reads [199, 332, 463] spread 264 bins — this record is not reproducing itself
      !! 420 Hz: reads [1, 127, 1] spread 126 bins — this record is not reproducing itself
    0x09 acq   : only 1 usable points
    0x09 opread fold check (Nyquist 6294 Hz):
         7800 Hz  predicted  389  measured  218  miss  171
        10000 Hz  predicted  211  measured   65  miss  146
        14001 Hz  predicted  115  measured  118  miss    3
        17000 Hz  predicted  359  measured    5  miss  354
        22000 Hz  predicted  258  measured   12  miss  246
      worst miss 354 bins -> FOLD FAILS, rate not trustworthy

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
Verdict against §2 H2: **O3** on both paths at this tone placement — opread R² 0.43–0.80 with read-to-read spreads up to 460 bins, acq with 0–1 usable points (every tone's three reads disagree by 24–394 bins), every fold fails. This is the outcome the pre-registration said to *expect* if the ladder continues above 1.25 MS/s: at 2.5–25 MS/s a 1024-sample record spans 41–410 µs, a fraction of one 40–420 Hz period, so each record is a different slice of a near-DC level and no bin reproduces. It therefore says nothing yet about H2's O1/O2 — run 4 places the tones for 5 MS/s. Control at the start of this run: opread 12519 / acq 12489 S/s (0.2% apart, 3500 Hz read at bin 286 vs 287), PASS.

### Run 4 — 0x08 with tones placed for 5 MS/s, both paths (`dumps/exp63_run4.log`, unedited)
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.0%  PASS

=== reg 0x01 = 0x08, opread+acq read ===
  table: no rate (0.0f); tones placed for ~5000000 S/s (--fs-guess)
    0x08 opread: fs =   5474279 S/s  R2 -0.2223   [50000->33  100000->112  250000->48  500000->160  1000000->143]
    0x08 acq   : fs =   4990070 S/s  R2 +1.0000   [50000->11  100000->21  250000->51  500000->103  1000000->205]
    paths agree to 8.8%
    0x08 opread fold check (Nyquist 2737139 Hz):
      3399734 Hz  predicted  388  measured  310  miss   78
      4400516 Hz  predicted  201  measured   54  miss  147
      6198547 Hz  predicted  135  measured  346  miss  211
      7501832 Hz  predicted  379  measured  374  miss    5
      9403122 Hz  predicted  289  measured  165  miss  124
      worst miss 211 bins -> FOLD FAILS, rate not trustworthy
    0x08 acq    fold check (Nyquist 2495035 Hz):
      3100212 Hz  predicted  388  measured  389  miss    1
      4000000 Hz  predicted  203  measured  204  miss    1
      5598688 Hz  predicted  125  measured  122  miss    3
      6799469 Hz  predicted  371  measured  368  miss    3
      8497925 Hz  predicted  304  measured  307  miss    3
      worst miss 3 bins -> FOLD HOLDS

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
Verdict against §2 H2: **O1.** The acq path is coherent — fs = **4,990,070 S/s** (−0.20% of 5.0 M; prediction ±2%), R² 1.0000, tones at bins 11/21/51/103/205 against 10.2/20.5/51.2/102.4/204.8, no `!!` line, fold worst miss 3 bins through 8.5 MHz — while opread on the same code, unit, build, source and tones is torn (R² −0.22, every tone off by 30–200 bins, fold fails), as EXP-10 found for opread at high rates. The read path is the only difference between the two columns, so EXP-12/15's "incoherent" 0x06–0x09 was the instrument. Ladder: fs(0x08)/fs(0x0A) = 4,990,070/1,249,691 = **3.993** (×2 ×2 ✓ via 0x09, measured in run 5).

### Run 5a — 0x09 with tones placed for 2.5 MS/s, both paths (`dumps/exp63_run5a.log`, unedited)
Beyond the pre-registration (which placed only 0x08); same rules as run 4, placement from the ladder (2.5 M).
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12487 S/s  R2 +1.0000   [250->21  500->41  1000->82  2000->164  3500->287]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.0%  PASS

=== reg 0x01 = 0x09, opread+acq read ===
  table: no rate (0.0f); tones placed for ~2500000 S/s (--fs-guess)
      !! 25000 Hz: reads [32, 103, 123] spread 91 bins — this record is not reproducing itself
      !! 500000 Hz: reads [143, 206, 206] spread 63 bins — this record is not reproducing itself
    0x09 opread: fs =   2336864 S/s  R2 +0.0857   [25000->103  50000->53  120000->97  250000->99  500000->206]
    0x09 acq   : fs =   2500893 S/s  R2 +1.0000   [25000->10  50000->20  120000->49  250000->102  500000->205]
    paths agree to 7.0%
    0x09 opread fold check (Nyquist 1168432 Hz):
      1400055 Hz  predicted  411  measured  379  miss   32
      1900167 Hz  predicted  191  measured  198  miss    7
      2600305 Hz  predicted  115  measured   53  miss   62
      3200000 Hz  predicted  378  measured  256  miss  122
      4000000 Hz  predicted  295  measured  424  miss  129
      worst miss 129 bins -> FOLD FAILS, rate not trustworthy
    0x09 acq    fold check (Nyquist 1250446 Hz):
      1600000 Hz  predicted  369  measured  369  miss    0
      2000000 Hz  predicted  205  measured  205  miss    0
      2800109 Hz  predicted  123  measured  123  miss    0
      3399734 Hz  predicted  368  measured  369  miss    1
      4300714 Hz  predicted  287  measured  286  miss    1
      worst miss 1 bins -> FOLD HOLDS

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
acq: fs = **2,500,893 S/s** (+0.04% of 2.5 M), R² 1.0000, bins 10/20/49/102/205, fold worst miss 1 bin through 4.3 MHz → O1 again. opread torn (R² 0.09, fold fails). Ladder: fs(0x09)/fs(0x0A) = **2.001**, fs(0x08)/fs(0x09) = **1.995**.

### Run 5b — 0x07 and 0x06 with tones placed for 12.5 and 25 MS/s, acq only, no fold (`dumps/exp63_run5b.log`, unedited)
Beyond the pre-registration. No fold test: the fold tones (0.62–1.71 × fs = 7.8–43 MHz) are above the Dot's 10 MHz; the fit tones (1–20% of fs) are not.
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12487 S/s  R2 +1.0000   [250->21  500->41  1000->82  2000->164  3500->287]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.0%  PASS

=== reg 0x01 = 0x07, acq read ===
  table: no rate (0.0f); tones placed for ~12500000 S/s (--fs-guess)
    0x07 acq   : fs =  12498676 S/s  R2 +0.9996   [120000->9  250000->23  620005->50  1200047->100  2500000->204]

=== reg 0x01 = 0x06, acq read ===
  table: no rate (0.0f); tones placed for ~25000000 S/s (--fs-guess)
    0x06 acq   : fs =  24849896 S/s  R2 +0.9973   [250000->13  500000->26  1200047->51  2500000->107  5000000->203]

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
- 0x07: fs = **12,498,676 S/s** (−0.01% of 12.5 M), R² 0.9996. Per tone, the implied rate (f·1024/bin) is 13.7 / **11.1** / 12.7 / 12.3 / 12.5 MS/s: the 250 kHz tone sits at bin 23 where 20.5 was predicted (−11%); the other four are within quantisation. No `!!` line: the three reads of each tone agreed.
- 0x06: fs = **24,849,896 S/s** (−0.60% of 25 M), R² 0.9973. Implied rate per tone 19.7 / 19.7 / 24.1 / 23.9 / 25.2 MS/s: the two lowest tones (250 k, 500 k: bins 13 and 26 against 10.2 and 20.5) land ~20% high, reproducibly; the top three agree with 25 M. The slope fit is carried by the high tones.
- Both are entered as **PROVISIONAL**: fitted (R² ≥ 0.99, within 2% of the ladder) but not fold-checked, and with a reproducible low-tone deviation that this run cannot explain — the record at these rates spans 41–82 µs, i.e. 10–40 periods of the offending tones, so it is not a resolution problem. Candidates: a record that is not one contiguous 1024-sample capture at these codes (two segments, a held tail), or a frontend effect; to be tested with tones ≥ 1 MHz only and a source that reaches the fold band.

### Run 2, first attempt — aborted by the instrument, not the device
`fpga scope center ch1 6` answered nothing within the script's 20 s (`got 25 bytes`); by hand the same command completed in **20.4 s** (`CH1 range 6: center DAC1=2639 (median=128)`), so run 1 had passed the same step with < 0.4 s to spare. The scope's shell and protocol answered normally afterwards (`usbstat`: no stall; `dropped_closed=4` = the lines the device printed after the script had closed the port). Timeout raised to 60 s in both bench scripts; run 2 repeated from the start, controls included.

### Run 1 — controls and 0x0F/0x0E/0x0D (`dumps/exp63_run1.log`, unedited)
```
source: Kode Dot LEDC square (clk 80000000 Hz; PLL_F80M = X1 40 MHz x12/6); every frequency below is the one its timer registers produce
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >

=== CONTROL — the two read paths at 0x10 ===
    0x10 opread: fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    0x10 acq   : fs =     12489 S/s  R2 +1.0000   [250->20  500->41  1000->82  2000->164  3500->287]
    paths agree to 0.0%  PASS

=== reg 0x01 = 0x0F, acq read ===
  table: 24979.1 S/s; tones placed for ~24979 S/s (scope_timebase.c)
    0x0F acq   : fs =     25009 S/s  R2 +1.0000   [250->10  500->20  1200->49  2500->102  5000->205]
    acq vs table 24979.1 S/s: +0.12%
    0x0F acq    fold check (Nyquist 12504 Hz):
        16000 Hz  predicted  369  measured  369  miss    0
        20000 Hz  predicted  205  measured  205  miss    0
        28001 Hz  predicted  123  measured  123  miss    0
        33999 Hz  predicted  368  measured  369  miss    1
        43000 Hz  predicted  287  measured  287  miss    0
      worst miss 1 bins -> FOLD HOLDS

=== reg 0x01 = 0x0E, acq read ===
  table: 49930.1 S/s; tones placed for ~49930 S/s (scope_timebase.c)
    0x0E acq   : fs =     50020 S/s  R2 +1.0000   [500->10  1000->20  2500->51  5000->102  10000->205]
    acq vs table 49930.1 S/s: +0.18%
    0x0E acq    fold check (Nyquist 25010 Hz):
        31000 Hz  predicted  389  measured  389  miss    0
        41000 Hz  predicted  185  measured  184  miss    1
        57000 Hz  predicted  143  measured  143  miss    0
        69000 Hz  predicted  389  measured  389  miss    0
        86000 Hz  predicted  287  measured  287  miss    0
      worst miss 1 bins -> FOLD HOLDS

=== reg 0x01 = 0x0D, acq read ===
  table: 123662.7 S/s; tones placed for ~123663 S/s (scope_timebase.c)
    0x0D acq   : fs =    124968 S/s  R2 +1.0000   [1200->10  2500->20  6200->51  11999->98  25000->205]
    acq vs table 123662.7 S/s: +1.06%
    0x0D acq    fold check (Nyquist 62484 Hz):
        76997 Hz  predicted  393  measured  393  miss    0
       100000 Hz  predicted  205  measured  205  miss    0
       140000 Hz  predicted  123  measured  123  miss    0
       170001 Hz  predicted  369  measured  369  miss    0
       210000 Hz  predicted  327  measured  328  miss    1
      worst miss 1 bins -> FOLD HOLDS

source at end: Kode Dot clk 80000000 Hz — crystal-derived, no loop to drift; X1 error (+31..+41 ppm on three Dots) is below this method's resolution  PASS

Kode Dot off; timebase restored to 0x10.
done
```
- Every control passes (§4). 0x0D fits 124,968 S/s: the table's PROVISIONAL 123,662.7 is 1.06% low and the round 125,000 is within 0.03%, on a second unit and a second source (EXP-18 measured it on unit #1 with the ESP32 sketch).

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
Measured ladder, unit #3, acq path, Kode Dot source (crystal-derived frequencies), 1024-sample records:

| code | fs (S/s) | vs 2.5/2/2 ladder | R² | fold | tier |
|---|---:|---:|---|---|---|
| 0x10 (control) | 12,489 | −0.01% vs table 12,490.0 | 1.0000 | — | (unit #1's MEASURED, confirmed) |
| 0x0F (control) | 25,009 | +0.12% vs table | 1.0000 | 1 bin | (confirmed) |
| 0x0E (control) | 50,020 | +0.18% vs table | 1.0000 | 1 bin | (confirmed) |
| 0x0D | 124,968 | −0.03% vs 125 k (+1.06% vs the provisional table) | 1.0000 | 1 bin | MEASURED |
| 0x0C | 250,089 | +0.04% | 1.0000 | 1 bin | MEASURED |
| 0x0B | 500,203 | +0.04% | 1.0000 | 1 bin | MEASURED |
| 0x0A | 1,249,691 | −0.02% | 1.0000 | 1 bin | MEASURED |
| 0x09 | 2,500,893 | +0.04% | 1.0000 | 1 bin | MEASURED |
| 0x08 | 4,990,070 | −0.20% | 1.0000 | 3 bins | MEASURED |
| 0x07 | 12,498,676 | −0.01% | 0.9996 | not possible (> 10 MHz) | PROVISIONAL |
| 0x06 | 24,849,896 | −0.60% | 0.9973 | not possible | PROVISIONAL (two low tones ~20% off) |

- **Established:** (H1) `0x0C`/`0x0B`/`0x0A` select 250 k / 500 k / 1.25 M S/s within 0.04%, every coherence test passing; the 2.5/2/2 cadence continues from 0x0D to 0x08 with every step within 0.3% (ratios 2.001, 2.000, 2.498, 2.001, 1.995). (H2 = O1) `0x08` and `0x09` are coherent on the acq path — distinct rates, R² 1.0000, folds within 3 bins through 8.5 MHz — while opread on the same code, unit, build, source and tones is torn: EXP-12/15's INCOHERENT band was the opread instrument at those rates, not the device. The acq path is validated to 5 MS/s by fold tests (the §6 extrapolation is closed up to 0x08). Cross-unit: unit #3 reproduces unit #1's 0x0E–0x10 within 0.18%, so unit #1's table and these numbers can share one ladder at this resolution.
- **Excluded:** uniform ×2 above 0x0D (0x0A would be 1.0 MS/s: its 250 kHz tone sits at bin 205, not 256); saturation (one rate for all codes); "no rate at all" for 0x06–0x0C; the device as the cause of EXP-15's incoherence at 0x08/0x09.
- **NOT excluded (explicitly):** that 0x07/0x06 are exactly 12.5 M / 25 M (fitted, not fold-checked, and their lowest tones land 11–20% high for a reason this run cannot name); anything about 0x00–0x05; that unit #1 would give these exact numbers above 0x0D (0.2% unit spread is the one cross-check); the mechanism by which opread tears (Stlkv's #18 hypothesis stays open); CH2's timebase (never read).
- **Follow-up:** write these rates into `scope_timebase.c` with tiers as above and provenance "unit #3, EXP-63" (done in this branch, separate commit); re-measure 0x07/0x06 with fit tones ≥ 1 MHz and a source that reaches 43 MHz for the fold; 0x05–0x00 (50 M – 250 M by the ladder) need that source too; run the opread path at 0x08 with a logic analyser on SPI3 to settle the tearing mechanism; EXP-64 (vertical) once a second working input lead is on hand.
