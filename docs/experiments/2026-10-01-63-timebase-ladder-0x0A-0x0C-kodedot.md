# EXP-63 — timebase codes 0x0A–0x0C and 0x06–0x09, with a source that can reach them

- **Date:** 2026-10-01
- **Unit:** bench unit #3 (V1.4). The rate table being tested is unit #1's.
- **Build:** unit #3 on PR #48's branch (`feat/usb-wedge-evidence` @ `f912890` = #41 + the evidence record; no
  timebase or acquisition change vs #41), `Build: Oct  1 2026 14:36:14`. Scripts at `bench/signal-source-abstraction`
  @ `66a1802`. Source: `kodedot_sigsource` on a Kode Dot (ESP32-P4), standalone, output GPIO9 (§3).
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
**Deviations from the pre-registration (2026-10-01, before any run):**
- Source pin is **GPIO9** (EXP2, J3 pin 4), not GPIO14: the operator's probes were already there. GPIO9 sits behind the ESDA6V1SC6 array (~190 pF) with the 68 Ω series resistor (τ ≈ 13 ns), which rounds MHz edges; the fits use the fundamental's peak bin, so the edges do not matter for the rates, but the fold tones above ~1 MHz lose harmonic content. `kodedot_sigsource` was changed to make GPIO9 its default output (`p 14` / `p 21` still select the low-capacitance pins).
- The Dot runs `kodedot_sigsource` **standalone** (its flash was rewritten with the operator's consent: no kodeOS on this unit, no panel assembly, no KTD2026 LED). The SDK's LED driver retried every 40 ms and logged each attempt on the console, so (a) that log is silenced in the app and (b) `bench._DotSerialTransport` now reads replies to the `>ok`/`>err` terminator instead of quiet time (commit 66a1802).
- **CH1 is the crocodile-clip lead** (no attenuator, ×1 by construction) for every run below; the operator's ×1 probe does not conduct. Shown before run 1, with the Dot's square on GPIO9 at 0x10, range 6: the lead's channel saw 86–89 codes p-p at 1000 Hz, the probe's channel 2–4 codes (noise) through (a) a static `dc 0`/`dc 1` step (lead: 141 → 225 codes; probe: 81.3 → 81.3), (b) a pin finder — the app was extended to drive any of the 14 J3 GPIOs (`p <gpio>`) and the square was walked over all of them: only GPIO9 moved the lead's channel, nothing moved the probe's — and (c) swapping the two BNCs at the scope: the square followed the lead to CH1 and the probe stayed flat on CH2. CH1's input itself is fine (at range 0 it picks up 50 Hz, 22× the floor, as an open high-impedance input does; after the swap it shows the square). The lead is on GPIO9, its ground clip on the GND pair beside GPIO14/GPIO21. CH2 (dead probe) is set to the same range and ignored.
- `ppm 35` is set by hand after each Dot reset (not persisted).

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| `version` (the `build:` line, all four runs) | one string, recorded | `OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE)` (runs 1, 2) |
| `fpga scope timebase`, no argument, by hand: before run 1 / after each run (script restores 0x10) | `0xNN (reg 0x01 = 0xNN)`, equal / `0x10 (reg 0x01 = 0x10)  12490 S/s` | before run 1: `timebase 0x08 (reg 0x01 = 0x08)  0 S/s --/div` (the UI's own setting; the script sets 0x10 itself); after the runs: see §5 |
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
- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**
- **Follow-up:**
