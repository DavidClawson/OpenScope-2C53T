# EXP-22 — display stability: the trace holds still, once the trigger stops reading the seam

- **Date:** 2026-09-03 (test authored 2026-09-01)
- **Unit:** bench unit #1, `guest-coldtrace-ch2`
- **Source:** JDS6600, CH1 → scope CH1 and CH2 → scope CH2 (two cables)
- **Harness:** `scripts/exp22_stability.py` (+ `scripts/exp22_seam_analysis.py`)
- **Question:** with both channels driven by periodic signals, does the rendered
  trace stay steady — and stay steady while amplitude, frequency and relative
  phase change — at parity with a working bench scope?

## The test

"The screen stabilizes" decomposed into four measurable claims, all taken
through the REAL display path — a new `spi3 frame` shell command returns a
COHERENT two-channel snapshot of the acq-task RAM buffers (generation-checked
against the atomic CH1+CH2 commit) plus the trigger offset computed by the
renderer's own `scope_ui_soft_trigger_offset` (exported, not host-replicated):

1. **Horizontal lock** — frames aligned at the renderer's own offset show
   ≤ 2.0 samples p95 residual jitter (1 sample = 1 screen px in the 320-px
   draw window).
2. **Vertical** — midline holds 128 ± 10 across every scenario; span
   consistent frame-to-frame; Vpp tracks the commanded amplitude.
3. **Two-channel alignment** — CH2-vs-CH1 phase tracks commanded JDS phase
   steps within 10°.
4. **Frequency** — captured fundamental within 2.5% of commanded.

11 scenarios: 500 Hz/2 Vpp baseline, 200 Hz, 1 kHz, 1 Vpp, 4 Vpp, phase
45/90/180°, square, timebase 0x0E at 2 kHz — plus an on-hardware **negative
control** (soft trigger OFF must FAIL the lock metric; a stability metric that
cannot detect a dancing trace proves nothing). Script selftest validates the
metric math on synthetic frames, including recovery of a deliberately injected
0.4-sample shift.

## Run 1: everything passed except horizontal lock — with structure

Vertical, frequency and phase were excellent everywhere (Vpp within ±3.6%,
phase steps measured 44.9/89.7/179.9°, freq within 0.04%, negctl detected
free-run at 10.8 samples). But lock jitter:

| scenario | jitter p95 (samples) |
|---|---|
| 500 Hz / 2 Vpp (4 scenarios) | 10–12 ≈ free-run |
| 200 Hz | 24 |
| 500 Hz / 1 Vpp | 1.56 ✓ |
| 500 Hz / 4 Vpp | 6.6 |
| 1 kHz | 1.97 ✓ |
| 2 kHz @ 0x0E | 0.85 ✓ |

Per-frame residuals (added in run 2) were **bimodal**: ~0.0 or ~12 samples —
and 12.5 samples is exactly half a period at 500 Hz. Frames were not
wandering; individual frames locked **180° out of phase**.

## The finding: the acquisition record is not time-contiguous at its edges

`exp22_seam_analysis.py` measures local fundamental phase in overlapping
64-sample blocks (with the expected per-block advance removed, so a contiguous
record is a flat line and a discontinuity is a step). Validated on synthetics:
contiguous sine → 0.31-sample max step; injected 9.3-sample seam at index
400 → detected as 7.2 samples at index 384.

On the saved hardware frames, the correlation with lock failure is essentially
perfect:

- **Every high-residual frame carries a genuine ~9–14-sample phase
  discontinuity inside the draw window** (baseline fr1: seam @96;
  amp 4 Vpp fr6: @64; phase-180 fr3/fr6: @96/@64; the three 22-sample
  200 Hz frames: @32).
- **Frames with the same seam OUTSIDE the window lock to < 0.1 sample**
  (amp 4 Vpp fr1: 11.3-sample seam @928, residual 0.02). The renderer's
  arm/fire algorithm is sound; the data under it isn't contiguous.
- Seams cluster at the buffer **edges only**: indices 32–96 (stale head —
  consistent with the servo-era finding that the first bytes are the oldest)
  or 864–928. Never mid-buffer.
- The discontinuity magnitude fits a **~33 ms age gap ≈ one acq read
  cadence**: at 500 Hz that is ~π (the observed half-period flips), at 200 Hz
  ~2.4 rad (observed), and at 1 kHz an integer number of periods —
  **phase-invisible, which is exactly why 1 kHz passed run 1**. The stale
  prefix behaves like residue from the *previous* readout — same defect
  family as the known "our reads are off by one byte" finding
  ([[spi3-runtime-dispatch-decoded]]).

The amplitude dependence in run 1 (1 Vpp passing, 2 Vpp failing) was luck of
seam positions across 8-frame samples, not a real amplitude effect.

## The fix (renderer-side guard; acquisition defect FILED, not fixed)

`scope_soft_trigger_offset` now starts the trigger search — and the
threshold's min/max — at `SEAM_GUARD = 128`. With the crossing found within
one period past 128, the draw window `[off, off+320]` also stays clear of the
late seam region for any signal above ~35 Hz at the slow timebases. The
underlying FPGA readout-alignment defect (stale ~one-cadence-old data at the
record edges) is a real acquisition bug this guard deliberately does NOT fix —
it belongs with the readout-alignment work, and full-buffer consumers
(measurement engine, freq estimator) still see the seam. The freq estimator's
robustness to it is demonstrated (−0.04% with seams present); Vpp uses robust
percentiles.

## Run 3 (seam-guard build): 11/11 PASS

| scenario | jitter p95 before | after |
|---|---|---|
| baseline 500 Hz 2 Vpp | 11.05 | **0.09** |
| 200 Hz | 24.05 | **0.49** |
| 1 kHz | 1.97 | 0.60 |
| 1 Vpp | 1.56 | 0.53 |
| 4 Vpp | 6.60 | **0.09** |
| phase 45/90/180 | 9.96/11.49/12.09 | **0.94/0.09/0.10** |
| square | 11.95 | **0.01** |
| NEGCTL free-run | 10.8 (pass) | 10.7 (still fails-when-it-should) |
| 0x0E 2 kHz | 0.85 | 1.01 |

All offsets ≥ 129 (past the guard). Vertical, Vpp, frequency and relative
phase all held. **OVERALL PASS: both channels driven, phase/amplitude/
frequency all changed, trace steady at ≤ 1 px through the real render path.**

## Also fixed along the way

- The test's own vertical check used the **median**, which sits on one of a
  square wave's two levels (read 149 on a correctly centered square). Now the
  min/max midline. Note the same property means the **centering servo**
  (median-based) would mis-center on a square-wave input — fine for a scope
  (centering runs on whatever is connected, usually not a perfect 50% square),
  noted for completeness.
- `JDS6600.phase()` added to `bench.py` (register 31, 0.1° units,
  readback-verified).

## Evidence

- Pre-guard raw frames: `reverse_engineering/captures/exp22_frames_preguard.npz`
  (8 frames × 11 scenarios × both channels, with per-frame renderer offsets).
- Analyzer: `scripts/exp22_seam_analysis.py` (selftest documented above).
- Harness: `scripts/exp22_stability.py --selftest` for the metric checks.

## Open follow-ups

1. **The acquisition seam itself** — stale ~one-read-cadence-old data at
   record edges (head 32–96, tail 864–928). Likely readout-pointer residue in
   the FPGA FIFO / off-by-one family. Affects any full-buffer consumer;
   currently mitigated only for the display trigger.
2. NEGCTL drift observation: free-run residuals advance ~1 sample/grab at
   500 Hz — the JDS crystal and FPGA sample clock beat slowly; harmless,
   quantifiable clock-offset measure if ever needed.
3. The draw window's right edge can still meet the late seam region for
   signals slower than ~35 Hz on slow timebases; if sub-35 Hz display becomes
   a use case, revisit with the real acquisition fix.
