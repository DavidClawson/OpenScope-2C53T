# EXP-55 — reg 0x02 is not the trigger polarity select

- **Date:** 2026-09-22
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, commit `90e1cfc` (Build Sep 22 2026 13:17:33, the v8 image)
- **Status:** **REFUTED** (H: reg 0x02 is an edge enable) — control held both times

## 1. Problem
Trigger-modes S2 (c): can the FPGA be told which edge to fire on? EXP-53 (50j) showed it
fires on **either** polarity of the level − 28 crossing. Stock writes `02 03` to SPI3 reg
0x02 at boot and nothing since has explained what that register does. If it is a two-bit
edge enable, the UI's edge button can reach the fabric; if not, edge selection is an
MCU-side filter and must be labelled that way.

## 2. Hypothesis
Reg 0x02 bit0 enables one edge and bit1 the other; `03` is both.
- If true: `02 01` → every committed record has one slope at the trigger index; `02 02`
  → the other slope only; `02 00` → no new records in NORMAL (PC0 edges +0); `02 03`
  again restores the mix.
- If false: every value gives the same mix of slopes at the same edge rate.

Predictions were written into `scripts/exp55_polarity.py` before the run.

## 3. Procedure
JDS6600 CH1: triangle (`write_raw(21,"3")`), 2 Hz, 2.0 Vpp, +0.5 V offset (record spans
~33..133 on range 5, as in EXP-53 50j). Timebase `0x12` (2500 S/s: 1024 samples = 410 ms =
0.82 period). `fpga scope level 0` → code 128 → predicted crossing in record scale **100**.
`fpga scope trigmode normal` so only handover-strobed records commit (no AUTO fallback
reads). For each value in `03, 01, 02, 00, 03`: `spi3 seq 02 <val>` (the dispatcher parks
the acq task for the write), 3 s settle, then 10 records via `spi3 read` at ~1 s spacing
with PC0 edges and SPI3 OK counted before and after.

Per record: pointer k = index of the largest sample-to-sample jump; un-rotate so r[1023]
is newest; the trigger is r[512]; the edge is the sign of mean(r[516..523]) −
mean(r[500..507]). Record identity by md5 (a duplicate would mean NORMAL held).

**Preconditions verified by readback:**
| what | expected | measured |
|---|---|---|
| timebase | `0x10` → `0x12`, 2500 S/s | `timebase 0x12 (reg 0x01 = 0x12)` |
| level | code 128 | `code 0x80` |
| r[512] at control | 100 ± a few | 97–101, all 100 records |

## 4. Control
`02 03` (the boot value), run first each time:

| control | expected | measured (run 1 / run 2) | passed? |
|---|---|---|---|
| both slopes present | mix | rise 3 fall 7 / rise 4 fall 6 | yes |
| r[512] ≈ level − 28 = 100 | 100 ± few | 97–101 / 98–101 | yes |
| records advancing in NORMAL | 10 distinct | 10/10 / 10/10 | yes |
| edge rate | ~2 strobes per commit | +32 edges, +16 pair reads / +33, +16 | yes |

## 5. Results
The script ran twice back to back (a logging mishap; the second run is
`reverse_engineering/captures/exp55/exp55.log` and its records are in `exp55_records.npz`).
The first run's per-record slopes are in the session transcript; both runs are tabulated.

| reg 0x02 | run | PC0 edges | pair reads | distinct | rise | fall | r[512] range |
|---|---|---|---|---|---|---|---|
| `03` control | 1 | +32 | +16 | 10/10 | 3 | 7 | 97–101 |
| `01` | 1 | +32 | +16 | 10/10 | 5 | 5 | 96–101 |
| `02` | 1 | +33 | +16 | 10/10 | 5 | 5 | 98–101 |
| `00` | 1 | +31 | +15 | 10/10 | 3 | 7 | 97–101 |
| `03` recovery | 1 | +33 | +16 | 10/10 | 3 | 7 | 97–101 |
| `03` control | 2 | +33 | +16 | 10/10 | 4 | 6 | 98–101 |
| `01` | 2 | +33 | +16 | 10/10 | 5 | 5 | 98–100 |
| `02` | 2 | +31 | +15 | 10/10 | 3 | 7 | 97–101 |
| `00` | 2 | +31 | +15 | 10/10 | 5 | 5 | 97–100 |
| `03` recovery | 2 | +31 | +15 | 10/10 | 4 | 6 | 97–100 |

Slope magnitudes were 1.0–4.5 counts across the 16-sample window (the triangle moves 0.16
count/sample), never zero; the sign is unambiguous on all 100 records. The script's first
version scored anything below ±3 as "flat" (96/100 records); the printed slopes were
re-scored by sign, and the script now scores by sign.

Side result: **r[512] = 97–101 on all 100 records against a predicted 100** — the level − 28
offset (EXP-53 50j) replicates on a second day with a different level (code 128 vs 152/54)
on the same range.

## 6. Blind spots
- Only four values were written and none was followed by a re-arm write to reg 0x01 or a
  power cycle; a polarity select that latches only on arm, or only alongside another
  register (0x06/0x07 sit at 00), would look exactly like this.
- Range 5 only. A per-range comparator would not show here.
- The pointer is found as the largest jump; on a 2 Hz triangle the seam jump (~60–100
  counts) dwarfs the signal's 0.16/sample slope, so k is unambiguous, but the method
  would fail on a fast signal.
- `spi3 seq` writes are read back only as "no error" from the shell; the register's
  content is not readable, so a write that the FPGA ignored is indistinguishable from
  one it accepted and found irrelevant.

## 7. Conclusion
- **Established:** with reg 0x02 at `00`, `01`, `02` or `03` the FPGA triggers on
  **both** edges at the same rate, on the same level, with the trigger at mid-record.
  `02 00` does not stop triggering. The level − 28 transfer holds at a second level.
- **Excluded:** reg 0x02 as a standalone edge-polarity select (or trigger enable) in the
  runtime write path this firmware uses.
- **NOT excluded:** a polarity select that needs a re-arm or another register; a
  different meaning for reg 0x02 entirely (it may be a channel/mode field).
- **Follow-up:** the UI edge control is an **MCU-side filter**: classify the slope at
  index 512 of each committed record and, in NORMAL/SINGLE with a polarity chosen, discard
  the other edge (halves the trigger rate on a symmetric signal). Label it so in the spec.
  Trigger-modes S2 (c) closes with this; S2 (a) still wants a second range.
