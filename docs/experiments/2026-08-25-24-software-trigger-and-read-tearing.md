# EXP-24 — software display trigger, and the dancing was read-tearing

- **Date:** 2026-08-25
- **Unit:** bench unit #1 (mars.local), `guest-coldtrace`
- **Source:** JDS6600 sine into CH1, range 5 (699 mV/div, measured)
- **Status:** software trigger LANDED and largely working; residual jumps traced
  to read-tearing at fast timebases (an acquisition-coherency issue, filed).

## Problem

The live scope trace "dances" — a clean sine that slides horizontally frame to
frame and tears mid-wave. Real scopes hold a waveform still with a trigger; ours
free-ran. Two questions: (1) does a software trigger stabilise it, and (2) what
is the tearing?

## What was built

`scope_measure_find_trigger()` — a pure, host-tested Schmitt-armed edge search
(arm `hyst` counts on the far side, fire on the crossing) returning the first
qualifying index, `-1` = no edge. The render offsets **both** channels' draw
window by that index so the trace starts on a repeatable phase; `-1` free-runs
(AUTO behaviour). `fpga scope softtrig [on|off|toggle]`, default on.

Two implementation traps hit and fixed **on the bench**, both this project's
signature "edited the plausible-looking path" failure:

1. **Wrong render path.** First put it in `draw_demo_waveform` — which is the
   FULL-redraw path (so M3's true-scale showed there on toggle), but the
   continuous live trace is drawn by a *separate* streamed compositor,
   `draw_scope_live_frame` (REDRAW_INCREMENTAL). The offset had to go in both.
2. **Wrong threshold.** Used a fixed ADC 128. But the capture is not centred —
   measured **mean 84, range 34..136** — so 128 sat up near the peak where the
   slope is shallow and the crossing jitters every frame. Fixed to auto-level:
   base the threshold on the signal's own midline `(min+max)/2`, so it locks on
   the steep mid-slope crossing regardless of DC offset (and needs no `center`,
   which was hanging on the flat CH2 anyway).

## Control and the real finding — the tearing is read-lapping

Dumped the full CH1 capture (`spi3 opread 04 1024 dump`) and measured it rather
than trusting the screen.

**At timebase `0x0E` (~50 kS/s):** sample-to-sample **jumps of 60–70 counts**
(a sine this size can only move ~6 counts/sample) at **different positions every
read** (217/251 → 134/107 → 331/298 → 94/43), and an irregular period
(48–66 samples). Those are seams — the buffer is not a coherent capture.

**At timebase `0x12` (~2495 S/s), 60 Hz in:** max jump **9–10**, period **rock
stable at 41–42 samples** (60 Hz ÷ 2495 = 41.6, exactly), across every read.
Clean, coherent, continuous.

⇒ **The dancing is read-tearing.** The `/256` readout of 1024 samples takes
~35 ms; at ~50 kS/s the engine fills the buffer in ~20 ms, so the read **laps
the acquisition** and stitches samples from different times. The software
trigger locks the left edge, but moving seams keep tearing the rest. Slow the
timebase below the lap threshold and the capture is coherent and the trigger
holds. This matches the documented note that fits degrade above ~30 kS/s through
`opread`.

## Result

- Software trigger + slow timebase: trace **near-stable** on the glass (bench
  observation — weak evidence, but corroborated by the buffer numbers above).
- Residual: "a couple little jumps every second or two" even at `0x12` —
  occasional tearing / edge-selection, i.e. still an acquisition-coherency
  effect, not a trigger fault.
- Host tests: `scope_measure_find_trigger` covered for rising/falling, no-edge,
  the **phase-lock property** (two phase-shifted sines overlay after alignment),
  and a **hysteresis control** (hyst=0 fires on threshold noise; the two differ).

## Blind spots

- The `(min+max)/2` midline is noise-sensitive to a single extreme sample; the
  hysteresis + inside-signal clamp mitigate but do not remove this.
- Screen stability is a visual observation; the buffer-jump statistics are the
  real evidence and they were taken via `opread`, which is itself the torn path
  at fast rates — the slow-rate coherence result is the trustworthy half.
- The render reads the acq-task buffer, not `opread`; they were assumed to share
  the tearing characteristic (both go through the 0x04 read), not proven equal.

## Conclusion

- **Established:** a software display trigger stabilises the trace; the residual
  free-run instability at fast timebases is **read-lapping** (read slower than
  acquisition), coherent below ~30 kS/s.
- **Not the fix, explicitly:** the Tang Nano config-transport bisect is a
  different problem (config entry), not readout coherency.
- **Next (filed, not now):** coherent readout — snapshot / double-buffer the acq
  path, read in a quiet window, or gate on a fabric "buffer ready" line if one
  exists. That is the acquisition-path session, separate from the FPGA bisect.

Companion bench validation the same session (not re-detailed here): M1 factory-cal
backup round-trips by CRC (`cal backup`/`status`), and M3 true-scale graticule
tracks amplitude (2 Vpp → ~3 div, autofit → full band) — both PASS.
