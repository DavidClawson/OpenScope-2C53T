# EXP-47 — NORMAL and SINGLE work on a fresh boot with nothing typed; a level change must re-prime

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Builds:** `guest-coldtrace` at `4187066` (derived post-edge default) for run 1; `2c10625` (re-prime on level/timebase change) for run 2
- **Script:** `scripts/exp47_derived_defaults.py` (logs `exp47.log`, `exp47b.log`)
- **Status:** **CONFIRMED** — run 1 failed one line of fourteen, run 2 passed all fourteen

## 1. Problem

EXP-46 found the FPGA's re-arm bracket (one fill + ~200 ms after the trigger)
and shipped a derived post-edge delay. Do the trigger modes now work on a
boot with no shell override, across the measured timebases, including SINGLE?

## 2. Hypothesis

If the derived defaults are sufficient: readback says "derived" for the
post-edge delay and the AUTO budget; NORMAL at level 0 sustains at 0x0E,
0x10, 0x11 and 0x12 with edges equal to commits; NORMAL at level +100
freezes and resumes at level 0 (falsifier: any code frozen at level 0, or
+100 advancing); SINGLE gives exactly one record per selection; AUTO at
0x10 gives 1.00 edges per pair and a static body.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp → CH1, range 5. First commands after boot are
readbacks. Per timebase: AUTO → `fpga scope timebase` → NORMAL, 1.5 s,
then four `spi3 frame` records bracketed by `status` (PC0 edges, SPI3 OK).
No `fpga postedge`, `fpga autowait`, `fpga pairgap` or `fpga acqbr` override
is issued anywhere in the run.

Readback (run 2, before any write): AUTO edge-wait 475 ms derived at 0x10;
level 0 → code 0x80 in force, boot reconcile wrote 0x80; mode Auto; pair
gap 0; read clock not set.

## 4. Control

- NORMAL at level +100 (code 0xFC, above the signal) froze in both runs.
- The generation counter advanced under every "advancing" verdict and held
  under every "FROZEN" one; edges and commits were counted, not inferred.

## 5. Results

**NORMAL, level 0, no prime, both runs identical:**

| timebase | post-edge (derived) | result | edges / commits |
|---|---|---|---|
| 0x0E | 250 ms | advancing | +22 / +22 |
| 0x10 | 311 ms | advancing | +18 / +18 |
| 0x11 | 435 ms | advancing | +13 / +13 |
| 0x12 | 640 ms | advancing | +9 / +9 |

**Negative control at 0x10:**

| level | run 1 (`4187066`) | run 2 (`2c10625`) |
|---|---|---|
| 0 | advancing +19/+19 | advancing +19/+19 |
| +100 | FROZEN +0/+0 | FROZEN +0/+0 |
| 0 again | **FROZEN +0/+0** | **advancing +19/+19** |

**SINGLE:** three selections, each one record then held (382→394, 396→412,
414→428), both runs.

**AUTO at 0x10:** edges/pair 1.00; body max RMS 5.6–6.2 over ten records,
0/10 gross, both runs.

## 6. Blind spots

- One drive frequency and amplitude; the trigger level's transfer against
  ADC code (spec S2a) is not measured here.
- Edge polarity is not exercised (S2c).
- 0x0D and faster codes are not covered: their post-edge delay is the
  600 ms fallback for unmeasured rates, untested.
- "Sustains" is four records per code; a rare miss would not show.

## 7. Conclusion

- **Established:** with nothing typed, NORMAL captures at every measured
  timebase with one PC0 edge per committed record, SINGLE is one-shot, and
  AUTO is edge-gated with a static body. Trigger-modes spec **S1 met**, and
  S2(b) (Normal holds below the level, resumes above) and S2(d) (success
  rate: 4 of 4 codes, edges = commits) are measured.
- **Run 1's defect:** the reg-0x08 write in `fpga_apply_trigger_level()`
  drops the capture in flight (EXP-44 probe A showed the write yields no
  edge), but the task still believed one was in flight and waited for an
  edge that could not come. Fix (`2c10625`): the apply entry points for
  level and timebase raise `acq_reprime_req`; the loop clears
  `capture_in_flight` and primes again.
- **Open:** level transfer (S2a), edge (S2c), the 32–64 invalid head
  samples, and stock's 29 ms cadence against a bracket that costs us
  ~3.5 records/s at 0x10.
