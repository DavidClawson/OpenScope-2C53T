# EXP-42 — an edge-wait longer than the capture makes the record body static; the head is a separate artifact

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` + `fpga autowait <ms>` knob, branch
  `bench/2026-09-14`, image `exp42_coldtrace_autowait.bin`
- **Script:** `scripts/exp42_seam_autowait.py` (A/B/A), plus three shell
  probes recorded below (raw logs `exp42_sweep.log`, `exp42_head.log`,
  `exp42_body.log` in the session scratchpad)
- **Status:** **CONFIRMED for the body; the head is a new, bounded defect.**

## 1. Problem

EXP-41 established that a read starts a capture, a reg-0x08 level crossing
completes it, and completion pulses PC0. The AUTO path waits 25 ms for that
edge before reading anyway; the capture at 0x10 takes ≥ 82 ms. Does an
edge-wait budget longer than the capture make every record a completed,
static capture — i.e. close the M3 seam?

## 2. Hypothesis

With the level inside the signal and `fpga autowait` above the capture time,
reads happen only after PC0 (or on a rare fallback), so the phase step falls
to the ~0.3-sample floor and edges advance ~1 per record. If the seam stays,
the completed capture is not static either.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp sine → CH1, range 5, timebase 0x10 (12,490 S/s,
82 ms/1024 samples), reg 0x08 ← 0x80, gate off, re-arm off. Records via
`spi3 frame`; `fpga autowait` read back per arm; PC0 edges and SPI3 OK read
from `status` before/after each arm.

Metrics: (a) EXP-22's `frame_seam` max phase step + location; (b) a
block-residual metric — fit a 201.2 Hz sinusoid to samples 128–1023, then
RMS residual per 32-sample block; "body" = blocks 4–31 (samples 128–1023).

## 4. Control

| control | result |
|---|---|
| A1/A2 (25 ms) show seams | yes — but see §5: the two 25 ms arms disagreed (3.9 vs 21.0) on the phase-step metric |
| generation counter live, every arm | yes: 54–60 (25 ms), 26–36 (132 ms), 16–22 (300 ms), 12–16 (500 ms) per record — the wait demonstrably slows the loop |
| autowait read back | yes, every arm |

## 5. Results

**A/B/A on the phase-step metric (10 records/arm).**

| arm | autowait | median step | p90 | seam locations | PC0 edges / SPI3 OK |
|---|---|---|---|---|---|
| A1 | 25 | 14.74 | 24.46 | 448 32 928 96 32 32 928 32 32 928 | +47 / +285 |
| B | 132 | **5.46** | 22.76 | **32 32 32 32 32 32** 768 **32 32 32** | +47 / +142 |
| A2 | 25 | 17.21 | 26.20 | 32 928 96 32 32 928 32 32 96 96 | +48 / +286 |

**Budget sweep (8 records/arm).** Edges per SPI3-OK count saturate at
**0.50 = one edge per 04/05 pair**, i.e. every cycle edge-triggered, by
300 ms — the capture cycle at 0x10 is therefore ~200–300 ms, not 82 ms
(pre-trigger fill + trigger wait + post-trigger fill is the obvious shape;
not measured).

| autowait | median step | p90 | locations | edges/OK |
|---|---|---|---|---|
| 25 | 3.91 | 13.05 | 32 32 96 32 32 928 96 32 | 0.16 |
| 200 | 5.39 | 16.12 | 928 32 32 640 32 32 640 32 | 0.34 |
| 300 | 8.97 | 26.63 | 640 32 32 32 32 32 32 32 | **0.50** |
| 500 | 7.89 | 27.89 | **32 ×8** | **0.50** |
| 25 (repeat) | 20.96 | 27.44 | 32 32 928 96 32 32 928 96 | 0.17 |

The phase-step metric is **bimodal and drifts** (the two 25 ms arms: 3.9 vs
21.0). What it does resolve is *location*: gated, the seam sits at the head
(index 32) in 8/8 and 9/10; free-run, it wanders (928, 96, 640, 448).

**Head inspection, 500 ms, three records** (fit on 160–1023, RMS per block):

| record | first 12 samples | head blocks 0–3 | body/tail |
|---|---|---|---|
| 0 | `133 132 130 128 123 120 115 110 19 24 32 40` | 62.1 6.1 2.8 6.0 | 2–6 |
| 1 | `2 0 0 0 0 0 0 0 0 0 0 0` | 98.1 89.0 4.5 4.7 | 2–6 |
| 2 | `0 2 8 12 19 25 33 40 48 55 63 70` | 101.8 24.5 6.2 2.2 | 2–6 |

The first 32–64 samples are not a rotation: record 1 is **zeros**, record 0
drops from 110 to 19 in one sample. After sample ~64 the record is a clean
sinusoid to the end.

**Reading past 1024** (shell `opread 04`, 1536 samples): residual stays at
the floor to sample 1023 and jumps to ~107 from block 32 on; samples
1024–1031 = `144 149 152 156 159 160 161 161` ≈ samples 0–7 =
`141 146 151 154 157 160 161 161`, one sample offset. **The capture memory is
1024 samples and wraps.**

**Body metric, 10 records/arm** (max block RMS over samples 128–1023):

| autowait | max body RMS per record | gross seams (RMS > 20) in body |
|---|---|---|
| 25 | 6.1 6.1 6.2 6.0 **87.7** 6.1 6.1 6.1 5.9 5.9 | 1/10 |
| **300** | 5.9 6.1 6.0 6.1 6.0 6.0 5.6 5.9 6.0 5.9 | **0/10** |
| 25 (repeat) | 5.8 **88.5** 6.0 6.0 5.9 **89.4** 5.8 6.0 6.1 **87.5** | 3/10 |

Floor (median body RMS) 4.5–4.7 in every arm. Head settle point (first
block under 2× floor): 32–64 in every mode, gated or not.

## 6. Blind spots

- The phase-step metric's own A/A spread (3.9 vs 21.0) means the A/B/A
  medians alone would not have been evidence; the block metric and the
  location column carry the conclusion. Recorded so nobody quotes the 16 → 5.5.
- The head artifact (first 32–64 samples) is present under every wait, so
  this experiment cannot say what it is: readout-pipeline latency at the
  SPI clock, the write pointer parking near the memory start after
  completion, or the pre/post-trigger junction. Stlkv's "glitch in the
  first ~26 µs" (issue #18) is plausibly the same thing at his clock.
- Capture-cycle length (~200–300 ms at 0x10) is bounded only from the
  edges/OK saturation, not timed; the firmware does not timestamp edges.
- One timebase, one drive frequency, one unit. The display cost is real:
  300 ms budget ≈ 8 records/s at 0x10 against ~18 free-run.

## 7. Conclusion

- **Established:** with the trigger level inside the signal and an AUTO
  edge-wait above the capture cycle, every record's body (samples 128–1023)
  is a static, completed capture: 0/10 gross seams vs 4/20 free-run, at
  the same floor. **The M3 seam is the free-run fallback reading a buffer
  mid-capture, and it closes when reads follow PC0.** The interlock stock
  has is a *time* wait after the read (its per-timebase table); ours is now
  the edge itself, which is at least as good.
- **Established:** the capture memory is 1024 samples, circular.
- **New defect, bounded:** the first 32–64 samples of every readout are
  invalid (zeros or a dropout), in every mode. The display's `SEAM_GUARD =
  128` already steps over it; whole-buffer consumers (FFT, measurements)
  must skip the head until it is understood. It is *not* the seam.
- **Excluded:** the re-arm write as any part of the interlock (EXP-40/41).
- **NOT excluded:** that the edge-wait can be shortened by re-arming
  differently; that a smaller pre-trigger depth exists in a register.
- **Follow-ups, in order:** (1) make the AUTO budget timebase-aware by
  default (≥ 3× the 1024/fs capture at the code in force, capped for slow
  codes where AUTO must still refresh) and hand reg 0x08 to the UI trigger
  level instead of the boot constant 0xAD; (2) the head artifact — vary the
  SPI clock and read 64 extra bytes to see whether it moves; (3) NORMAL /
  SINGLE trigger modes, predicted working now, one button press each.
