# EXP-29 — the acquisition seam is a rotation, and the re-arm does not close it

**Date:** 2026-09-13 · **Unit:** bench #1 · **Build:** `guest-coldtrace-meter` @ `12c038c`
**Status:** NEGATIVE for the tested fix, POSITIVE for the underlying model. Control held.

## Problem

The acquisition record is not time-contiguous at its edges (EXP-22, 2026-09-03).
The display trigger steps over it with `SEAM_GUARD`, but every whole-buffer
consumer still eats it, which is what stands between this project and an FFT
anyone should trust. Root cause has been open for ten days.

`docs/re/acq_seam_root_cause.md` proposed that the record is a **rotation of a
live buffer**: we clock the capture memory out from address 0 regardless of
where the FPGA's write pointer sits, so the record's chronological origin is the
write pointer, not index 0.

It also named the candidate fix. `fpga.c:3280`, written 2026-08-17, records
stock's loop as **gate on reg 0x01 → read the 0x04/0x05 pair → re-arm**, states
that our acquisition task "has always done the middle step only", and names the
consequence as "a buffer being read while it is still being written". The
`fpga rearm on` toggle has existed since then, default off, and had never been
pointed at this question.

## Hypothesis

If the seam is caused by the missing re-arm, records taken with re-arm ON carry
materially smaller phase discontinuities than records taken with it OFF.

**Falsifier:** if the distribution is unchanged, the missing re-arm is not the
cause.

## Procedure

Drive 201.2 Hz sine, 2.0 Vpp, JDS6600 CH1 into scope CH1. Timebase code `0x10`
= 12,490 S/s, confirmed by readback.

**The drive frequency is a deliberate choice and it is the reason this
experiment could see anything.** A rotation's phase jump is `frac(1024·f/fs)` of
a cycle, which is about 0.04 cycle at both 500 Hz and 1 kHz — nearly invisible.
EXP-22 tested at exactly those frequencies, so **its affected-frame count is a
detection floor, not an occurrence rate.**

Records via `spi3 frame` (coherent CH1+CH2 snapshot through the real display
path). Metric: `frame_seam()` from `scripts/exp22_seam_analysis.py` — local
phase of the fundamental in overlapping 64-sample blocks, hop 32, expected
advance removed, so a contiguous record gives a constant sequence and a
discontinuity gives a step. Step reported in samples.

A/B/A, 10 records per arm. Driver: `scripts/exp29_seam_rearm.py`.

## Control

- **Positive control, and the one that decides whether a negative means
  anything:** the OFF arms must SHOW seams. If they sit at the noise floor the
  instrument cannot detect what the ON arm would be claimed to exclude, and the
  run is void. They did not: median 21.6 samples.
- **A/B/A against drift:** the two OFF arms must agree. 21.55 and 22.32.
- **The re-arm flag is read back off the device each arm**, never trusted from
  the setter. All three arms confirmed.
- `coherent=1` on **30 of 30** records, which excludes the MCU-side read race.

## Results

Max phase discontinuity per record, in samples:

| arm | re-arm | n | median | p90 | max |
|---|---|---|---:|---:|---:|
| A1 | off | 10 | 21.55 | 27.25 | 30.16 |
| B  | **on** | 10 | **24.07** | **30.31** | **30.88** |
| A2 | off | 10 | 22.32 | 28.73 | 30.81 |

Pooled: OFF median 21.59, ON median 24.07. **The re-arm does not reduce the
seam.** If anything it is marginally worse, within the spread.

### The seam location marches, and that is the real finding

A second run of 16 consecutive records with re-arm off, reporting the top two
block steps per record. The two largest steps are almost always **adjacent
blocks** (256/288, 800/832, 864/896), which is one discontinuity smeared across
two overlapping windows — so each record carries **one** seam, not two.

Its location advances monotonically, by 32 to 64 samples per record, wrapping
around the 1024-sample buffer:

```
272  816  304  880  592  336  880  400  944  464  --  496  --  528  48  784
```

**A seam that moves by a roughly fixed step per read, wrapping the buffer, is
what a rotation of a live buffer looks like.** The write pointer advances
between successive reads and the record's origin moves with it.

## Blind spots

- Block resolution is 32 samples (the hop), so the per-record advance is known
  only to ±32. This cannot resolve the exact rotation step.
- One drive frequency, one timebase code, one unit, one session.
- The metric reports the LARGEST discontinuity. A record with two genuinely
  separate seams of similar size would be reported as one.
- **This experiment says nothing about the gate**, which is the untested half —
  see below.
- Amplitude, coupling and the analog frontend were not varied.

## Conclusion

**Established:** the re-arm is not the fix. Stock's third step, performed alone,
leaves the seam exactly where it was. The `fpga rearm` toggle can be left at its
default.

**Strengthened, not weakened:** the rotation model. A single seam whose location
marches through the buffer at a near-constant rate is a direct signature of
reading across a live write pointer, and it is a stronger result than the
distribution comparison this experiment set out to make.

**Not excluded, and now the leading candidate:** stock's FIRST step. Its loop
gates on reg `0x01` — blocking until enough samples have accumulated — *before*
the read. `fpga rearm on` re-arms *after* the 0x04/0x05 pair (`fpga.c:3720`),
so it has never tested the gate. **The gate is the half that would actually
guarantee a static buffer, and it is not implemented.** That is the next
experiment, and it needs firmware rather than a shell toggle.

**Correction to EXP-22 carried here:** its affected-frame count was a property
of the drive frequency it used, not of the hardware.
