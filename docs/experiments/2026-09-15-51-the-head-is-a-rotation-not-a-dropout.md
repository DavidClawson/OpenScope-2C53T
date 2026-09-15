# EXP-51 — the "invalid head" is a rotation: the record is the circular buffer read from address 0, and the trigger sits at index L

- **Date:** 2026-09-15
- **Unit:** bench unit #1, `guest-coldtrace` (EXP-50 image, two-phase read)
- **Scripts:** `scripts/exp51_head_artifact.py` (log `exp51.log`); follow-ups `scratchpad/exp50b_0x12.py` (hold-read sweep, `exp50b.log`), `scratchpad/exp51b_head_vs_rate.py` (record plots + head length with the search cap lifted, `exp51b.png`)
- **Status:** **MODEL REPLACED** — "first 32–64 samples invalid" (EXP-42) withdrawn in favour of a rotation; the head is valid data

## 1. Problem

Every readout since EXP-42 carried a "head" of 32–64 samples that did not
fit the drive, covered by `SEAM_GUARD = 128` on the display. Is it a byte
count (SPI/readout), a time (engine), and what is in it?

## 2. Hypotheses

H_bytes: the head length is a fixed sample count independent of rate and
changes with the SPI clock. H_time: it is a fixed time (scales with fs).
Falsifiers: neither — length varies per record with something else.

## 3. Procedure

Per record: sine fit on samples 128–1023 (later 512–1023), head = last index
below the cut where |residual| > 4 × floor on 3 consecutive samples; the head
fitted on its own at the drive frequency for shape and phase offset.
Arms: baseline 0x10 / 201.2 Hz; `fpga acqbr 7` and `0`; 0x0E and 0x12;
`fpga pairgap 50`; CH2 via the JDS second channel; baseline repeat. Then
20 Hz / 201.2 Hz / 1 kHz drives at 0x10 / 0x0E / 0x0D with the search cap at
512 and two records per arm plotted.

## 4. Control

Baseline reproduced the EXP-42 range (heads 5–55 at 0x10) and the repeat
matched (20–51). Knob readbacks asserted against the handlers' strings.

## 5. Results

**Both hypotheses refuted.** Head length at 0x10 / 201.2 Hz: 5–61 samples,
varying per record; `acqbr 7` vs `0` medians 39 vs 23 (same spread); pair
gap no change; CH2 the same (14–60, floor 1.4). At 0x0E: 0–128+ (capped).

**The head is SHAPED**: it fits the drive sine at a constant phase offset —
**+176° at 0x10, +40° at 0x0E** — whatever its length. A rotation by one
record predicts exactly that: 1024 samples at 0x10 = 16.5 periods of
201.2 Hz → 180°; at 0x0E = 4.13 periods → 46°.

**Head length tracks the drive period, not the rate:**

| drive | timebase | head samples (6 records) | head ms |
|---|---|---|---|
| 20 Hz | 0x10 | 421 223 473 49 488 66 | 4–39 |
| 201.2 Hz | 0x0E | 14 148 80 64 244 176 | 0.3–4.9 |
| 201.2 Hz | 0x10 | 48 41 46 50 55 0 | 0–4.4 |
| 1 kHz | 0x0D | 192 200 206 216 226 238 | 1.6–1.9 |

Max ≈ one drive period at 20 Hz (50 ms) and 201 Hz (5 ms) = the latency from
arming to the rising crossing. (0x0D sits ~190 samples high with a slow
ramp; its rate is still provisional — not resolved here.)

**The plots (`exp51b.png`)**: the 20 Hz record is a sine cut at index L and
swapped — the segment before L is the newest data, the segment after is the
oldest, and index 1023 joins index 0 without a step. The body starts at the
trigger crossing (level 0x80, which on this low-centred, bottom-clipped
input is near the positive peak — visible as the jump *to* a peak at L).

**Side result (`exp50b.log`)**: the 201.2 Hz body floor at 0x12 is 22 on
every path including the old single read, so EXP-51's "0x12 torn" verdict
was the metric's threshold, not the EXP-50 change. At 0x11/0x12 a hold-read
at fill + 30 or + 130 produced occasional torn records (1/5 – 3/5) where
fill + 230 gave none — the margin needs a sweep before it is trusted at the
slow codes.

## 6. Blind spots

- Whether the write pointer starts at 0 on the arming read or at CS, at the
  opcode byte, or at the end of the read is not measured; only that L spans
  0 … one drive period.
- 0x0D's ~190-sample excess is unexplained (provisional rate, or a fixed
  offset at fast codes).
- Two records per arm plotted; head-length statistics are from 6–10 records.

## 7. Conclusion

- **Withdrawn:** "the first 32–64 samples of every readout are invalid". They
  are the **newest** samples. The record is the FPGA's circular capture
  memory read from address 0; the pointer starts near 0 at the arming read,
  reaches L at the trigger crossing, and the 1024-sample post-trigger fill
  wraps — so the readout is rotated by L, with the trigger at index L.
- **Established:** L = trigger latency × fs (bounded by one period of the
  trigger source); `SEAM_GUARD = 128` is a wrong-model fix that fails for any
  source below ~fs/128 (20 Hz at 0x10 puts the seam at index 400+).
- **Fix that follows (EXP-52):** timestamp the arming read and the PC0 edge
  (cycle counter), compute L, un-rotate on commit so index 0 is the trigger
  crossing; then the record is contiguous, the trigger point is hardware-
  stable, and the head skip disappears for the FFT.
- **Retired from the spec:** the head-artifact item; replaced by un-rotation.
