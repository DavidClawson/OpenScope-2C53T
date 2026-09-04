# The trigger was reading the seam

*2026-09-03*

The ask was simple, and it's the right one to be asking now that both channels
capture and the vertical axis is calibrated: drive both inputs with a real
signal and see whether the trace **holds still** — steady while you change the
amplitude, the frequency, the phase — the way a working bench scope does. Not
"does a waveform appear," which it has for weeks. Does it *stop dancing*.

The interesting part is that turning that question into a test found a hardware
bug we didn't know we had, and the answer to the original question turned out to
be "yes, once we stop the trigger from reading a piece of the buffer that lies."

## Making "steady" mean something

"The screen stabilizes" is a feeling, not a measurement, so the first job was to
break it into things a number can decide:

- **Horizontal lock** — do consecutive frames, once aligned at the trigger
  point, sit at the same phase? Measured as residual jitter in samples, which is
  screen pixels: 1 sample = 1 px in the 320-px draw window.
- **Vertical** — does the midline stay at 128 (the centering servo's job) and
  does the amplitude read consistently frame to frame?
- **Two-channel** — does CH2's phase relative to CH1 track the phase step we
  actually command on the generator?
- **Frequency** — does the captured fundamental match what we asked for?

The important discipline, learned the hard way on this project more than once:
measure through the **real** path, not a host-side replica. So a new `spi3 frame`
shell command returns a coherent two-channel snapshot of the exact RAM buffers
the renderer draws — generation-checked against the acq task's atomic commit, so
CH1 and CH2 come from *one* capture — plus the trigger offset computed by the
renderer's *own* function, exported rather than reimplemented. A host replica
that passes while the on-screen code fails is precisely the stable-wrong-number
trap we keep paying for.

And one more piece of the same discipline: an on-hardware **negative control**.
Turn the soft trigger off and the lock metric must *fail* — a stability test
that can't detect a free-running trace proves nothing. (It reads free-run at
~11 samples, comfortably failing, so the test can tell steady from dancing.)

## Run 1: almost everything passed, and the failures had a shape

Vertical, frequency, phase — all excellent everywhere. Vpp within a few percent,
the phase steps measured 44.9° / 89.7° / 179.9° against 45 / 90 / 180 commanded,
frequency within 0.04%. But horizontal lock failed on 500 Hz — at *free-run
levels*, ~11 samples — while 1 kHz passed clean at 2 samples. That split is the
tell. A trace that's merely noisy fails everywhere; a trace that fails at one
frequency and not another is being betrayed by something frequency-dependent.

The per-frame residuals were bimodal: roughly 0, or roughly 12.5 samples. And
12.5 samples is exactly half a period at 500 Hz. The frames weren't wandering.
Individual frames were locking **180° out of phase** — a coin flip, every frame.

## What the buffer was hiding

If some frames lock half a period off, the trigger is finding its crossing at a
place where the waveform isn't continuous. So: measure the local phase of the
fundamental in short overlapping blocks across the buffer, with the expected
per-block advance subtracted out. A contiguous record comes back flat; a
discontinuity comes back as a step. (Validated on synthetic data first — a
contiguous sine reads a 0.3-sample step, an injected 9.3-sample seam reads 7.2
samples at the right index.)

On the real frames the correlation was as clean as this project ever gets:

- **Every** high-residual frame had a genuine ~9–14-sample phase discontinuity
  sitting *inside* the drawn window.
- **Every** frame whose discontinuity fell *outside* the window locked to under
  0.1 sample.
- The seams clustered at the buffer **edges only** — the first ~30–96 samples,
  or the last ~100 — never in the middle.

The magnitude fits a stale segment about one acquisition read-cadence old
(~33 ms). At 500 Hz that's ~half a period — the observed flip. At 1 kHz it's a
whole number of periods, so it's phase-invisible, which is *why 1 kHz passed run
1*. The buffer's leading bytes are residue from the previous readout — the same
off-by-a-bit family as the "our reads are off by one byte" finding from August.

The renderer's trigger algorithm was never the problem. The data underneath it
just isn't time-contiguous at its edges.

## The fix, and what it deliberately doesn't fix

The display trigger now starts its search — and its threshold min/max — 128
samples in, past the stale head, which also keeps the whole 320-px draw window
clear of the tail seam for anything above ~35 Hz. Lock jitter collapsed from
10–24 samples to 0.01–1.0 across the board. **11 of 11 scenarios pass**: both
channels driven, phase and amplitude and frequency all changing, trace steady at
a pixel or better, and the negative control still correctly fails.

What the guard does *not* do is fix the acquisition seam. That's a real readout
defect — stale data at the record edges — and it still reaches any consumer that
reads the whole buffer. The measurement engine and frequency estimator are
demonstrably robust to it (the freq number was fine with the seams present), so
the guard buys correct *display* now while the underlying readout-alignment work
waits its turn. It's filed, not buried.

## The smaller lesson

The test's own vertical check first used the median — which, on a square wave,
sits on one of the two levels and read 149 on a perfectly centered trace. Switched
to the min/max midline. Worth noting because the centering *servo* is median-based
too, so it would mis-center a square-wave input; fine for a scope, where you're
centering on whatever's connected, but written down.

As usual, the acceptance test earned its keep not by passing but by failing in a
way that pointed at something true. The screen holds still now. And we know one
more specific thing about how the FPGA hands us its data than we did yesterday.
