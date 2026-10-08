# EXP-73 — reg 0x07 is not a hardware edge select either

- **Date:** 2026-10-06
- **Unit:** bench unit #1 (V1.4)
- **Build:** `guest-coldtrace` + EXP-72 switches (Build Oct 5 2026 20:07:01)
- **Status:** **REFUTED.** The FPGA triggers on either edge whatever reg 0x07 holds. The MCU edge filter stays.

## 1. Problem
Stock's register map (desk sweep 2026-08-15) has **0x07 = trigger edge** (`ms[0x18]`), but our
firmware only ever writes `07 00`. EXP-55 refuted reg **0x02** as an edge select, not 0x07. If
0x07 worked, Rising/Falling could move from the MCU filter (`src/dsp/trig_edge.c`), which drops
wrong-edge records after the fact, into hardware.

## 2. Hypothesis
Some value of reg 0x07 makes every committed record one slope. The falsifier is every value
giving the mixed split of the control. Predictions are in `scripts/exp73_reg07_edge.py`.

## 3. Method
EXP-55's posture:
- **Signal:** JDS CH1 2 Hz triangle, 2 Vpp, +0.5 V; range 5.
- **Trigger:** level code 128 (crossing at record value ~100); timebase 0x12 (410 ms record = 0.82 period); NORMAL.
- **`fpga edgefilter off`**, so the hardware's choice is what gets committed.

For each of `00 01 02 03 FF 00`:
1. `spi3 seq 07 <v>`.
2. A timebase re-write, in case the select latches on arm.
3. 3 s settle.
4. 10 committed records (`spi3 frame`, time-ordered).

The slope at index 512 = mean(r[516..523]) − mean(r[500..507]).

## 4. Control
`07 00` first and last.
- **Expected:** both slopes and r[512] ≈ 100.
- **Measured:** 4 rise / 6 fall, then 4/6; r[512] 97–101.
- The mixed control also shows the MCU filter really was off.

## 5. Results

| reg 0x07 | handovers/s | distinct | rise | fall | r[512] |
|---|---|---|---|---|---|
| `00` control | 1.30 | 10/10 | 4 | 6 | 98–101 |
| `01` | 1.26 | 10/10 | 7 | 3 | 98–101 |
| `02` | 1.23 | 10/10 | 6 | 4 | 98–101 |
| `03` | 1.31 | 10/10 | 4 | 6 | 98–101 |
| `FF` | 1.24 | 10/10 | 8 | 2 | 98–101 |
| `00` recovery | 1.28 | 10/10 | 4 | 6 | 97–100 |

No value gives a single slope. 8/2 is the largest skew; under a fair 50/50 an 8/2 split or
worse happens about 11 % of the time per value. The handover rate and the trigger level are
unchanged throughout. Log: `reverse_engineering/captures/exp73/` (records in
`exp73_records.npz`).

## 6. Blind spots
- Register contents can't be read back. An ignored write looks like an irrelevant one.
- Not covered:
  - a select that needs reg 0x06 nonzero, or another register, at the same time;
  - a select that latches only at config time (stock writes 0x07 at boot and on user change; we wrote it at runtime);
  - an encoding outside the five values.
- 10 records per value can't exclude a weak bias, only a select.

## 7. Conclusion
- **Established:** with reg 0x07 at 00/01/02/03/FF, written at runtime and followed by a
  re-arm, the FPGA fires on either edge at the level − 28 crossing.
- **Combined with EXP-55 (reg 0x02):** neither register stock associates with trigger
  configuration selects the edge at runtime. Stock decodes 0x07 from its edge setting, so
  either stock filters too, or the select latches somewhere we haven't reached.
- **Unchanged:** Rising/Falling stays an MCU filter.
