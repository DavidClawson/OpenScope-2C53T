# EXP-48 / EXP-49 — stock's 29 ms is a poll rate, not a capture rate; the FPGA caps triggered captures at ~1/(fill + 190 ms)

- **Date:** 2026-09-14
- **Unit:** bench unit #1, `guest-coldtrace` at `5235d6f` (merged main)
- **Scripts:** `scripts/exp48_bracket_fast_codes.py`, `scripts/exp49_what_a_read_returns.py`, `scripts/exp49b_poll_static_fraction.py`, plus a 201 Hz / 20 Hz variant of the latter (log `exp49c.log`); desk: correct-edge re-decode of the June capture (`scratchpad/redecode_capture.py`, log `redecode.log`)
- **Status:** **ANSWERED** — the cadence question is closed; the origin of the ~190 ms constant is not

## 1. Problem

Stock's June capture reads a 04/05 pair every 28.9 ms. EXP-46 found the
FPGA refuses to re-arm until one fill + ~200 ms after the trigger, which
caps us at ~3.5 records/s at 0x10. How does stock get 34 Hz?

## 2. Hypotheses (predictions written before each run)

- H1 *stock reads stale buffers*: consecutive same-channel frames in the
  capture are byte-identical in runs of ~6. Falsifier: every frame differs.
- H2 *the bracket is short at fast timebases*: at 0x08–0x0D (fill ≤ 8 ms)
  NORMAL sustains at a post-edge delay ≪ 190 ms. Falsifier: threshold ≈ 190.
- H3 *a reg-01 write clears the holdoff* (stock's only re-arm is `01 tb+1;
  01 tb`): read, write, read inside the bracket → +2 edges. Falsifier: +1.
- H4 *reads inside the bracket return the held record*: polling at the
  task's clock gives static records; predicted static fraction at 25 ms
  polling ≈ 1 − (crossing + fill)/(fill + 190 + 25): 0.71 at 0x10, 0.94 at
  0x0D. Falsifier: ~0 static.

## 3. Procedure

JDS6600 → CH1, range 5, level 0 unless stated. NORMAL sustain = four
`spi3 frame` records bracketed by `status`; task parked for shell probes by
selecting SINGLE after its one record (generation verified held). Desk:
`digital.csv` re-decoded with MISO's pre-edge value at each SCK rising edge
(all 348 frame headers `80 00 00`, matching the 2026-08-15 desk sweep).

## 4. Controls

- 0x10 reproduced EXP-46: frozen at 250 ms, sustains at 300.
- Shell: quiet → +0; one read → +1; read, read (0 ms apart) → +1; read,
  350 ms, read → +2. The probe distinguishes inside from outside the bracket.
- Derived-default AUTO at 0x10 during the polling runs: 8/8 static.
- Stock's condition reproduced on our firmware (level +100 above the signal,
  AUTO budget 25 ms, 0x08): 0 edges, 15 pairs/s, consecutive frames differ
  in 1024/1024 samples — the June capture, exactly.

## 5. Results

**Desk (H1 refuted, and the capture reframed):** 0 of 346 consecutive
same-channel stock frames identical (op04 differs in 234–743 bytes, op05
355–1023; period 28.9 ms). But the frames sit at codes 165–168 (op04) /
78–81 (op05) with the arm level `08 AD` = 173: **no trigger could fire in
that capture.** Stock's frames were fresh because they were free-run reads
of a rolling buffer at 5 µs/div (refills every ~50 µs), not triggered
captures. The capture says nothing about triggered cadence.

**EXP-48 A — NORMAL sustain threshold vs post-edge delay (ms):**

| timebase | fill | 1 | 60 | 120 | 150 | 180 | 210 | 250 | 300 |
|---|---|---|---|---|---|---|---|---|---|
| 0x10 | 82 | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |
| 0x0D | 8 | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ | |
| 0x0C | ~4 | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ | |
| 0x0A | ~1 | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ | |
| 0x08 | ~0 | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ | |

H2 refuted: the constant is the whole bracket at fast codes, 180 < C ≤ 210.

**EXP-48 B — shell probes inside the bracket (0x10):** read, `01 10`, read
→ +1; read, `01 11`, `01 10`, read → +1; read, `08 80`, read → +1. H3
refuted. **EXP-49 C:** reg 06, 07 and 02 at 01 / 10 / FF → +1 every time.
None of the five arm registers shortens the holdoff.

**EXP-48 C — triggered rate at stock-like polling** (AUTO budget 25 ms,
post-edge 1, level 0, 0x08, `status` only): 19.5 pairs/s, 0.25 edges/pair =
**4.9 triggered captures/s**; cycle 204 ms ≈ 0 fill + 190 + ½ poll.

**EXP-49 D — what fast polling returns** (AUTO budget 25 ms, post-edge 1,
~17 pairs/s, body metric on `spi3 frame`, static = block RMS at the floor):

| drive | timebase | static | body values |
|---|---|---|---|
| 2 kHz | 0x10 | 20/20 | 17–18 (floor) |
| 2 kHz | 0x0D | 20/20 | 23–25 (floor) |
| 201.2 Hz | 0x10 | **15/20** | 6 ×15, **51–53** ×5 |
| 201.2 Hz | 0x12 | ~4/12 | 22–23 ×4, 39–70 ×8 |
| 20 Hz | 0x12 | 6/12 | 7–8 ×6, 62–100 ×6 |

H4 partly confirmed: at 0x10/201 Hz the static fraction is 0.75 against the
predicted 0.71. The 2 kHz rows are not evidence — triggered records of a
periodic drive are phase-aligned, so old-and-new mixtures show no seam when
the crossing is sub-millisecond. At 0x12 half of all polls come back torn.

**EXP-49 A (instrument note):** a shell `opread` at /256 that is itself the
arming read comes back torn (body 30–70 at 201 Hz, every one of 12), because
the capture it starts overwrites the buffer during its 35 ms readout. The
task's /2 reads (0.14 ms) do not have this problem. This is the likely
reason earlier `opread`-based ladders were torn.

## 6. Blind spots

- PC0 was not among the June capture's eight channels; stock's triggered
  behaviour has never been observed on a wire. What is established is that
  the June frames were untriggered, on the level/signal arithmetic.
- The ~190 ms constant's origin is still unknown. Two untested candidates:
  a fixed-clock counter in the design (netlist), or a holdoff parameter
  stock sets over USART2 in its scope-mode command sequence — our coldtrace
  build is USART-silent in scope mode and config kills the meter path, so
  this cannot be probed until that is untangled.
- Polling counts taken during `spi3 frame` dumps are depressed (the dump
  parks the task); rates quoted are from `status`-only windows.
- Two drives, four codes; the torn-fraction model was checked at one point.

## 7. Conclusion

- **Answered:** stock's 29 ms is its display poll rate. Fresh *triggered*
  records are bounded by the FPGA at **~1/(fill + 190 ms + poll latency)**
  at every timebase — 4.9/s at fast codes, ~2.9/s at 0x10, ~1.5/s at 0x12 —
  for stock as much as for us. No reg 01/02/06/07/08 value changes it.
- **Polling freely is not free:** reads that land between arming and the
  trigger's fill return the rolling buffer, torn — 25 % of polls at 0x10,
  ~50 % at 0x12. A stock-style free-running display would show them. Our
  edge-gated read (one record per edge, after the fill) is the correct
  design; what a free poll would add is repeat frames.
- **Available improvement is latency, not rate:** read the held record at
  edge + fill instead of edge + fill + 230, then issue the arming read at
  the bracket's end. Same records/s, each one ~200 ms sooner.
- **Retired:** the "stock is not honouring the bracket" tension in EXP-46.
