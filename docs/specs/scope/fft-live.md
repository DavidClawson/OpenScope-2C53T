# Spec: FFT on live capture

**Track:** scope
**Stage now:** S0 (fed a synthetic 1 kHz square generated on the spot)
**Champion:** —

## What it is

The FFT screen (spectrum + waterfall + split view) analyses the **live
acquisition buffer** in the `guest-coldtrace` build, with a frequency axis in
real hertz derived from the measured timebase table — and an honest axis
(`--`/bins) when the current code is unmeasured.

## Prior art

Stock's FFT is one of its most-mocked features: wishlist Tier 1 #5 is
literally "Real, labeled, scalable FFT" — owners call stock's unlabeled,
unscalable. Our DSP side is written and host-tested (19 tests: 4096-point,
5 windows, averaging, max hold, harmonic labeling). The device-side spectral
path is *already proven at S3* in miniature: `scope_freq.c` runs an on-device
transform over the acq buffer and passed the 15/15 acceptance test (EXP-17).

## Our angle

A labeled, calibrated FFT with harmonic markers on a $70 handheld — axis
numbers that trace to a measured sample-rate table — is a headline feature.
And our own history is the warning label: EXP-08's "broken time axis" was a
double FFT in our own script, so the S2 criterion below is deliberately an
end-to-end tone test, not an eyeball check.

## Hardware dependencies

- One capture record is ~1 KB/channel per read (opcode 0x04/0x05); the
  4096-point transform needs record accumulation or a shorter N. Decide, don't
  assume.
- Frequency axis is only real on the 8 measured timebase codes
  (`scope_timebase.c`); elsewhere the axis must degrade to bins.
- `spi3 opread` tears at fast codes (~35 ms at /256, documented in EXP-14);
  the acq-task buffer (`spi3 read`) is the valid source, same as EXP-17 found.

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| S1 | FFT screen consumes the live acq buffer in `guest-coldtrace`; the synthetic square-wave source (`fft_test_signals.c`) is unreachable in that build. |
| S2 | A bench tone at a known frequency (source-rate corrected per EXP-14) lands its peak within ±1 bin of prediction on ≥3 measured codes, including one above-Nyquist fold check. Writeup in `docs/experiments/`. |
| S3 | Host regression over captured records; a negative control shifts the assumed fs and must fail. |
| S4 | Waterfall stops issuing one fill per pixel column (20,480 draws/frame today — it visibly rasters); axis labeled in Hz on measured codes, bins otherwise; window/averaging controls reachable by button. |

## Open questions

1. Transform size on-device: accumulate 4 records for 4096, or run 1024-point
   per record? (Latency vs. resolution; `scope_freq` chose small-and-fast.)
2. Does the FFT screen share `scope_freq.c`'s transform or the big `fft.c`
   path? Two spectral engines is the two-renderers bug shape all over again —
   pick one.
