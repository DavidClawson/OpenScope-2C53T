# The trace that wouldn't hold still

*2026-08-25*

The FPGA dev board was out for delivery, so this was a day for the work that
doesn't need it. It turned into two things: a handful of milestones knocked out
by a team of agents working in parallel, and then a long bench session chasing a
sine wave that refused to stand still — which ended, as these things do, with the
instrument telling us something we didn't know.

## Four milestones, four worktrees

The pre-FPGA window is the time to do everything that *isn't* the FPGA, because
the moment the board arrives all attention goes to the config-transport bisect.
So four independent pieces of work went out to four agents at once, each in its
own git worktree so they couldn't step on each other:

- **Factory-cal self-protection.** Our own reflashing is the standing threat to
  the per-device factory calibration at MCU flash `0x08006000` — a page we can't
  regenerate. Now `cal backup` mirrors it to a dedicated 8 KB region on the
  external W25Q, `cal restore` writes it back (refusing to overwrite a programmed
  page without `force`), and `cal status` reports both. Bench-validated on unit
  #1: the live page (crc `0xC221D02A`) round-trips to the W25Q and back with a
  matching CRC. And a happy finding — unit #1's factory cal is *still there*,
  which the "our app overwrites it" note had left in doubt.
- **True-scale graticule + measurement cleanup.** The vertical grid now optionally
  means the measured volts/div instead of autoscaling to fill the band. Bench A/B:
  a 2 Vpp sine at 699 mV/div spans ~3 of 8 divisions in true-scale and jumps to
  full-band in autofit — same signal, only the mode changed. The agent also caught
  a stale README bullet (the FFT waterfall "20,480 draw calls" was fixed long ago)
  and retired a dead measurement engine.
- **The config-transport bisect runbook**, ready for the Tang Nano.
- **A watchdog feed for the USB firmware-update path** — which, in a small comedy
  of timing, Stlkv had landed on his branch the same day with the identical line,
  and then *proved under a live watchdog*, which the code review couldn't. Ours
  was redundant. Good.

All four came back, got reviewed, and merged. Then the bench.

## The dancing sine

With a real signal on the probe, the live scope trace danced — a clean sine that
slid sideways frame to frame and tore mid-wave. That's the one thing a scope must
do that we hadn't built: **triggering**. A real scope starts every sweep at the
same point on the wave — a level crossing in a chosen direction — so the frames
overlay and the trace freezes, without touching the samples themselves.

So we built a software trigger: find the crossing in the captured buffer, start
drawing from there. It's a small, pure function with host tests for the
phase-lock property. And it did nothing.

The reason is the kind of thing that only shows up on hardware, and we hit it
twice:

1. **The wrong render path.** The trigger went into `draw_demo_waveform` — which
   is a real render path, but only for *full* redraws. The continuously-updating
   live trace is drawn by a separate streamed compositor, `draw_scope_live_frame`.
   The frames we were actually watching never saw the offset.
2. **The wrong threshold.** Fixed at ADC 128 — except the capture sits at mean 84,
   range 34–136. 128 was up near the peak, on the shallow part of the curve, where
   the crossing jitters every frame. Dumping the buffer and *looking* is what
   showed this. The fix: track the signal's own midline, `(min+max)/2`, which is
   the steep part where a crossing is unambiguous.

Both fixed, and it got better — but a couple of jumps a second remained. David's
instinct was "is the capture itself discontinuous?" It was.

## The instrument was lying, again

Dumping the full 1024-sample buffer and measuring it — instead of trusting the
screen — showed sample-to-sample **jumps of 60–70 counts** (a sine this size can
only move ~6 counts per sample), at *different positions every read*. The capture
wasn't a coherent snapshot. Slow the timebase from ~50 kS/s to ~2.5 kS/s and the
jumps drop to 9–10 and the period locks to a rock-steady 41–42 samples.

That's **read-lapping**: the `/256` readout of 1024 samples takes ~35 ms, but at
50 kS/s the engine fills the buffer in ~20 ms, so the read overtakes the
acquisition and stitches together samples from different times. The software
trigger locks the left edge, but the moving seams tear the rest. It's the same
family as every other ghost in this project — a stable, plausible, wrong number,
this time a torn buffer that *looks* like a signal.

So: software trigger plus a slow enough timebase gives a near-stable trace, and
the residual instability is now named — **coherent readout**, an
acquisition-path problem (snapshot, double-buffer, or a fabric "buffer ready"
line), and explicitly *not* what the FPGA dev board is for. Filed for a focused
session. The dev board is about getting *into* configuration; this is about
reading *out* cleanly once we're there. Two different problems, and it's worth
being clear about which is which before the board lands and everything looks like
a nail.

Full evidence in [EXP-24](../experiments/2026-08-25-24-software-trigger-and-read-tearing.md).
The grid divisions got brighter too, because they were hard to read — which is
how we ended up staring at the trace long enough to notice it was dancing.
