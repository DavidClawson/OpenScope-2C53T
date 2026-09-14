# EXP-44 / EXP-45 — NORMAL still freezes after a priming read; the 04/05 gap is not the variable

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Builds:** `30effa9` (priming read + SINGLE hold) for EXP-44; `f7ad3f1` (+ `fpga pairgap`) for EXP-45
- **Scripts:** `scripts/exp44_trigger_modes.py`, `scripts/exp45_pair_gap.py`, plus two shell probes (logs `exp44b.log`, `exp44c.log`)
- **Status:** **EXP-44 REFUTED (priming alone does not sustain NORMAL); EXP-45 REFUTED (gap irrelevant)** — but the probes between them located the variable.

## 1. Problem

EXP-43: NORMAL froze at level 0 with zero reads. Hypothesis: it waits for an
edge before its first read. `30effa9` adds a priming pair read (staging
only) whenever nothing is in flight. Does NORMAL now sustain itself?

## 2. Hypothesis

EXP-44: NORMAL at level 0 advances, ~1 edge per record; at +100 it freezes;
SINGLE gives one record per selection.
EXP-45: if the spacing of the task's two reads (~140 µs at /2) is what stops
the next capture, a gap of a few ms between 0x04 and 0x05 lets NORMAL
sustain; gap 0 must reproduce the freeze.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp → CH1, range 5, timebase 0x10, level 0 (0x80).
`grab_frame` (spi3 frame) for generations; `status` for PC0 edges / SPI3 OK.
Probes with the acq task's reads parked behind `fpga acqgate on` (re-arm
off), one action at a time, edge counter before and 1.5 s after.

## 4. Control

- EXP-44: none of the arms advanced, so the +100 arm cannot serve as a
  control; the SINGLE "+4 generations" were the 0.5 s AUTO interlude between
  selections, not SINGLE records.
- EXP-45: gap 0 reproduced the freeze; each gap read back; the AUTO arms
  measure edges/pair with reads demonstrably happening (pairs/s 4–5).

## 5. Results

**EXP-44 (priming build):** NORMAL level 0 / +100 / 0 — generation 178 ×5
each, FROZEN, edges +0, OK +0. SINGLE — no record of its own.

**Probe A (what kills or primes it), NORMAL mode, task running:**

| action | generation | PC0 edges | SPI3 OK |
|---|---|---|---|
| nothing since the mode switch, 2 s | 842 → 842 | +0 | +0 |
| shell `opread 04` | 842 → **844** | **+1** | +1 |
| +2 s more, nothing | 844 → 844 | +0 | +0 |
| `fpga scope level 0` (park + reg 08 write) | 844 → 844 | +0 | +0 |
| shell `opread 04` | 844 → **846** | **+1** | +1 |
| `seq 01 10` (reg 01 write) | 846 → 846 | +0 | +0 |

A shell read primes an edge; the task consumes it and commits one record;
the task's own pair read never produces the next edge.

**Probe B (which reads yield edges), task reads parked:**

| action | edges |
|---|---|
| quiet 3 s | +0 |
| single `opread 04` | +1 |
| single `opread 04` again | +1 |
| single `opread 05` | +1 |
| `opread 04` then `05` (~50 ms apart) | **+2** |
| same again | +2 |
| single `opread 04` | +1 |

**EXP-45 (gap sweep):**

| NORMAL, pair gap | result |
|---|---|
| 0 / 2 / 10 / 50 / 0 ms | FROZEN every arm, edges +0, OK +0 |

| AUTO gated (327 ms), pair gap | edges/pair | pairs/s |
|---|---|---|
| 0 | 0.50 | 5.3 |
| 10 | 0.50 | 5.3 |
| 50 | 0.50 | 4.3 |

## 6. Blind spots

- Probe B's PC0 *level* trace failed to parse (took the prompt); only the
  edge counts are used.
- Shell reads run at /256 and seconds apart; the task's run at /2 and
  immediately after the edge. Probe B cannot separate those two
  differences; EXP-46 does.

## 7. Conclusion

- **Excluded:** priming alone; the inter-read gap up to 50 ms; the reg 01
  and reg 08 writes as arm or kill.
- **Established:** every shell read yields one edge; the task's pair issued
  on the edge yields none; gated AUTO's edges/pair is *exactly* 0.50 at any
  gap — the pairs alternate: the fallback read 327 ms after the previous one
  arms, the read issued immediately on the edge does not. The variable is
  when the read happens relative to the edge, or the clock — EXP-46.
