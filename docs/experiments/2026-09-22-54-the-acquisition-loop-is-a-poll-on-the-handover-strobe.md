# EXP-54 — the acquisition loop is a poll on the PC0 handover strobe

**Date:** 2026-09-22 · **Unit:** bench #1 · **Branch:** `bench/2026-09-14` · **Image:** `guest-coldtrace`, Build Sep 22 2026 09:20:17 (poll loop, `fpga pollgap` 30 ms, poll start fill + 100 ms)
**Script:** `scripts/exp54_poll_acceptance.py` · **Log:** `reverse_engineering/captures/exp53/exp54.log`

## Problem

EXP-53's postscript found PC0 strobes 6–9 µs after the read that hands over a completed record, so the FPGA runs its own capture cycle and the MCU's job is to poll for the handover. Does a loop built on that model keep every EXP-47 acceptance line, fix NORMAL on slow signals, and commit clean records at every code?

## Hypothesis

If the model is right: (a) NORMAL triggers on a 1 Hz square at 0x12 and a 4 Hz square at 0x10 (both froze on the two-phase image, EXP-53 (50h)); (b) EXP-47's lines hold (NORMAL at 0x0E/10/11/12 advancing with edges tracking commits, holds at level +100, resumes at 0, SINGLE one-shot 3/3, AUTO edge-gated); (c) committed records at the body floor at 0x10/0x11/0x12 for 201.2 Hz and 50 Hz; (d) strobe → commit ≈ 1 ms. *Falsifiers:* commits without strobes in NORMAL, torn committed records, or the squares still frozen.

## Procedure

Loop (fpga.c, `fpga_warmtest_acq_task`): after a handover sleep `fpga postedge` (derived fill + 100 ms), then read a 04/05 pair every `fpga pollgap` (30 ms). A pair whose reads advanced `pc0_edges` is committed as the record; one that did not is the rolling buffer and is discarded — AUTO commits it as free-run once `fpga autowait` (3 × fill + 230) has elapsed since the last commit, NORMAL/SINGLE hold. No hold read, no arming read, no un-rotation. `fpga holdread` retired. Predictions above were written into the script before it ran. Controls: the level +100 hold (NORMAL must freeze) and the same body metric/floors as EXP-53 (50i-b).

## Results

```
(a) square 1 Hz tb 0x12 NORMAL: advancing edges +13 commits +6   latency 1 ms  polls-before-last 50
    square 4 Hz tb 0x10 NORMAL: advancing edges +20 commits +10  latency 1 ms  polls-before-last 11
(b) tb 0x0E NORMAL: advancing edges +37 commits +18   0x10: +30/+15   0x11: +21/+10   0x12: +14/+7   (latency 1 ms, polls 4)
    level +0 advancing +30/+15 · level +100 FROZEN +0/+0 · level 0 advancing +30/+15
    SINGLE 3/3 "ONE record then held" · AUTO 10 s: edges +33 commits +16
(c) 201.2 Hz  0x10: 6 6 6 6 6 6 · 0x11: 12 12 11 10 11 11 · 0x12: 23 22 22 22 23 23
    50 Hz     0x10: 8 9 32 8 32 8 (metric floor, see EXP-53) · 0x11: 8 7 7 7 7 8 · 0x12: 8 8 8 8 8 8
```

"commits" in the script is SPI3 OK / 2, and `spi3_ok_count` increments once per committed pair, so the true count is edges / 2 pairs — **two strobes per handover, one per channel read** (04 and 05 each strobe). The same 2:1 appears in every earlier log where both numbers were printed (EXP-50i: 37/19); EXP-47's "edges = commits" was this ratio read through the same `//2`.

The 50 polls before the 1 Hz handover are the FPGA waiting for a crossing (either edge, 2/s) — the loop reads the roll 50 times, discards it 50 times, and commits the first read that strobes.

## Blind spots

- Poll reads during the FPGA's cycle are assumed harmless; EXP-53 saw the cycle run at its normal rate with reads inside it, but nobody has looked for a subtler effect (e.g. a read exactly at the crossing).
- The poll start (fill + 100) is a margin below EXP-46's bracket, not a measured minimum; earlier polling only costs SPI time.
- Record rotation is untouched: the seam is wherever the FPGA's pointer was, and no MCU timestamp can recover it (EXP-53 postscript). The display's soft trigger covers it; whole-record consumers still see one seam.

## Conclusion

Accepted: every EXP-47 line holds, the slow-square NORMAL defect is closed, committed records are at the floor at every code, and the trigger-to-display path no longer contains a 40 ms re-read or a fill + 230 wait for a record that was already there. Model on the record: **the FPGA is a free-running triggered capture engine with a handshake — record complete → held until an opcode-04/05 read → strobe on each read → ~189 ms hold → roll → next trigger.** Stock's 29 ms read cadence is this poll. Retired with this: the "arming read" (EXP-41/43/46/47 wording), the two-phase hold read (EXP-50), the latency-derived un-rotation (EXP-52, never ran), EXP-42's "invalid head".
