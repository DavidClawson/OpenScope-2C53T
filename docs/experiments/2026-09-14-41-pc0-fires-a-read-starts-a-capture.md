# EXP-41 — PC0 was never silent: the trigger level was parked above the signal, and a READ is what starts a capture

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` + `fpga rearmwait` (the EXP-40 image), branch
  `bench/2026-09-14`
- **Status:** **CONFIRMED** (three probes, each with its own null arm). Shell
  driven; raw logs `exp41_gate_lvl80.log`, `exp42_edge_source.log` in the
  session scratchpad; every number is reproduced below.

## 1. Problem

EXP-30 reported PC0 has never edged on this unit — 0 edges in 1,315 reads
with EXINT0 armed — and concluded every record ever taken was a free-run read.
EXP-39 removed the in-band marker as an alternative, and EXP-40 removed
"re-arm then wait". What is left is the pin itself. Is PC0 actually silent,
or has this project never created the condition it reports?

## 2. Hypothesis

The boot arm sequence, copied from stock's June wire capture, ends with
`08 AD`: trigger level 0xAD = 173 on the 8-bit ADC scale. Every bench drive
since has been centred at ~85 with a peak near 160 (3 Vpp at range 5). **If
PC0 is a trigger-completion line, it cannot fire on a level the signal never
reaches**, and moving the level inside the signal produces edges
immediately; moving it back stops them. If PC0 is silent for a hardware or
configuration reason, the level makes no difference.

Second question, once edges exist: what *starts* the capture whose
completion pulses PC0 — nothing (free-running engine), the reg-01 write
(stock's "re-arm"), or a read?

## 3. Procedure

- JDS6600 1 kHz 3 Vpp sine into CH1 (from EXP-38), range 5, timebase 0x10.
  `op04` at the time: min 9, max 162, mean 86.
- Probe 1 — level sweep: `spi3 seq 08 <lvl>` for 0x80, 0x40, 0xA0, 0x20,
  then 0xAD; `PC0 edges` from `status` sampled twice, 2 s apart, per level.
  Acq task free-running (gate off, re-arm off).
- Probe 2 — EXP-30's own A/B/A (`scripts/exp30_seam_gate.py`) re-run with
  the level at 0x80, PC0 edge counter read before and after.
- Probe 3 — edge source: `fpga acqgate on` with re-arm off, which parks the
  acq task's reads (EXP-30's deadlock, used here as a parking brake); then
  one action at a time with the edge counter read before and 1.5 s after:
  nothing for 3 s; `seq 01 10`; `opread 04`; `opread 05`; 04 then 05;
  `seq 08 80`; `seq 01 10` again; nothing for 3 s; then gate off, 3 s.

## 4. Control

Each probe carries its null: the 0xAD arm in probe 1 (edges stop), the
quiet arms in probe 3 (edges 0 without an action), and the gate-off free-run
arm at the end (edges resume, +9 in 3 s). The counter is the same EXINT0
ISR EXP-30 used; nothing about the instrument changed between "never fires"
and "fires".

## 5. Results

**Probe 1 — the level is the switch.**

| level (reg 0x08) | inside the 9–162 signal? | PC0 edges in 2 s | SPI3 OK in 2 s |
|---|---|---|---|
| 0x80 | yes | **+7** | +37 |
| 0x40 | yes | **+6** | +37 |
| 0xA0 | yes | **+6** | +36 |
| 0x20 | yes | **+6** | +37 |
| **0xAD** | **no** (173 > 162) | **+0** | +32 |

Baseline before the sweep, after ~1,776 reads on this boot: 0 edges.

**Probe 2 — EXP-30's gate arm, level 0x80.** Edge counter 55 → 143 across
the run. A1/A2 (gate off): medians 13.0 / 16.7, generation advancing 48–52
per record. **Arm B (gate ON + re-arm ON): generation FROZEN, 596 skips,
zero frames**, max step 0.30 on a buffer that never changed. VOID again —
but this time with edges demonstrably available in the other arms, so the
gate is losing them, not waiting for a pin that never fires.

**Probe 3 — what produces an edge.** Acq task parked behind the gate.

| action | PC0 edges after 1.5 s | SPI3 OK |
|---|---|---|
| nothing, 3 s | **+0** | +0 |
| `seq 01 10` (the "re-arm" write) | **+0** | +0 |
| `opread 04` (one CH1 read) | **+1** | +1 |
| `opread 05` (one CH2 read) | **+1** | +1 |
| `opread 04` then `05` (a pair, ~0.4 s apart) | **+1** | +1 |
| `seq 08 80` (level rewrite) | +0 | +0 |
| `seq 01 10` again | **+0** | +0 |
| nothing, 3 s | +0 | +0 |
| gate off, free-run 3 s | +9 | +54 |

## 6. Blind spots

- The 1.5 s window after each action is long compared with the 82 ms
  capture; whether the edge lands at exactly capture-completion or somewhat
  later is not resolved here (EXP-42's per-record edge count will bound it).
- "A pair gives +1, not +2" was not chased: with the shell's ~0.4 s command
  settle the second read should have started a second capture. Either the
  second capture completed after the 1.5 s window, or a capture in progress
  is *restarted* rather than *started* by a read. Both fit the model below;
  neither was distinguished.
- Only 1 kHz and 201.2 Hz drives; a level "inside the signal" was tested at
  four codes, all crossed hundreds of times per second. Whether a single
  slow crossing produces a single edge (true trigger semantics) is untested.
- Single unit. Unit #2 (Stlkv) has reported PC0 activity under his loader
  since August, which this result finally agrees with.

## 7. Conclusion

- **Established:** PC0 is a live data-ready line on this unit. It pulses
  once when a capture completes; a capture completes when the ADC crosses
  the reg-0x08 trigger level; **a capture is started by a 0x04/0x05 read**,
  not by the reg-0x01 write and not spontaneously. Free-run at ~18 reads/s
  restarts most captures before they complete, which is why free-run shows
  ~3 edges/s rather than 12.
- **Established:** every "PC0 never fired" observation in this project
  (EXP-30 headline included) was taken with the boot arm's `08 AD` level
  above every bench signal ever applied. EXP-30's *fact* stands — every
  record was a free-run read — but its *cause* is the trigger level, not the
  pin. Sixth "inert control" of the year: the level was written faithfully
  from a stock capture and never revisited, and `scope_adjust_trigger_level()`
  has no callers, so the UI could not move it either.
- **Why EXP-30's gate deadlocked, both times:** the gate waits for an edge
  *before* the first read, and no read means no capture and no edge. Its
  "stock's first step" framing was wrong — stock's first step is a **time**
  wait after the read, not an edge wait before it.
- **Why EXP-40's settle did nothing:** it waited after a reg-01 write, which
  starts nothing.
- **The fix this predicts (EXP-42):** keep the AUTO fallback, but make the
  edge-wait budget exceed the capture period for the timebase in force
  (82 ms at 0x10 against the compiled 25 ms). Then each read's capture
  completes, PC0 fires, and the next read takes a static, trigger-aligned
  buffer: seam at the noise floor, ~1 edge per record.
- **Also predicted:** NORMAL and SINGLE trigger modes work as coded once the
  level is inside the signal; and the boot default of 0xAD must go — the
  UI's trigger level (mid-scale by default) should own reg 0x08.
