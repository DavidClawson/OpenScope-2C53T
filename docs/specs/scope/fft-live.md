# Spec: FFT on live capture

**Track:** scope
**Stage now:** S1 by code (2026-09-15, branch `feat/fft-live`) — the S2 bench tone test is the next gate
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
| S1 | FFT screen consumes the live acq buffer in `guest-coldtrace`; the synthetic square-wave source (`fft_test_signals.c`) is unreachable in that build. **MET BY CODE 2026-09-15:** `fft_prepare_input()` (scope_ui.c) feeds spectrum, split and waterfall from the CH1 record through the same `fpga_data_ready()` gate and generation tear-check as the trace; the first `SCOPE_RECORD_HEAD_SKIP` (128, the trace's SEAM_GUARD, now in `scope_record.h`) samples are skipped; `fft.c` stretches the window over the 896-sample record (`FFT_WINDOW_AT`) instead of applying the first 22 % of a 4096-point Hann; the axis and the peak are in hertz only when `scope_timebase_sample_rate()` for the code in force is measured AND the display and reg-0x01 codes agree, else bin index + `--`; in `FPGA_WARM_HANDOFF_TEST` builds the demo source is compiled out (`FFT_DEMO_REACHABLE 0`) and the views say "waiting for capture". Host: `make test-fft-live` (23 checks, 3 negative controls). Not yet seen on a screen. |
| S2 | A bench tone at a known frequency (source-rate corrected per EXP-14) lands its peak within ±1 bin of prediction on ≥3 measured codes, including one above-Nyquist fold check. Writeup in `docs/experiments/`. |
| | **Proposed acceptance (bench, unit #1):** JDS6600 1.000 kHz sine 3 Vpp → CH1, range 5, PRM to the FFT view. At timebase 0x10 (12,490 S/s) the header must read `LIVE CH1  pk 1.0kHz`, axis labels `3.0Hz` left / `6.2kHz` right, peak marker at bin 328 ± 1 (x ≈ 328·320/2047 = 51 px). Repeat at 0x0F (bin 164 ± 1, header `pk 1.0kHz`) and 0x0E (bin 82 ± 1). Fold check: 8.000 kHz at 0x10 must land at bin 4096−2624 = 1472 ± 1 (`pk 4.5kHz`). Negative control: `fpga scope timebase 0a` (unmeasured) → header `pk bin N  --`, axis `--`/`--`; and `spi3 seq 01 0f` alone (hardware moved, display not) → same refusal. Shell readback of the same record via `spi3 frame` + `scripts/bench.py` spectrum must agree on the bin. |
| | **Bench 2026-09-22, unit #1, poll-loop image (EXP-54):** record side complete — host 4096-pt FFT of the device's own CH1 record (head skipped as the device does), 4 records each: **0x10 → bin 328 328 328 328, 0x0F → 164 ×4, 0x0E → 82 ×4** (all exact); fold, 8.000 kHz at 0x10 → **bin 1475 ×4** vs 1472 predicted — that is the sample rate, not the FFT: a fold at 8 kHz moves 0.21 bin per S/s, so 1475 ⇒ fs ≈ 12,497 S/s, 0.06 % from the table's 12,490 and 0.02 % from the round 12,500; the device's estimator says 4,497 Hz on the same record. The ±1-bin tolerance was too tight for a fold at 8 kHz; ±3 is what the rate table supports. Screen (David's eyes): at 0x10 the header reads **`LIVE CH1 pk 1.0kHz`** with the harmonics visible; negative control `fpga scope timebase 0a` → header **`LIVE CH1 pk bin N`** with N wandering 6–50 (a fast unmeasured code puts 1 kHz in the first bins beside the DC leakage) — hertz refused as specified; the `--` suffix was not called out by eye. **Defect found:** the axis end labels (`3.0Hz` / `6.2kHz`) and the dB labels were drawn transparent grey (`fg == bg`) straight onto the bars that fill the bottom of the region and were invisible on the bench; fixed to opaque on black (`draw_fft_axis_labels`, dB labels), pending a screen check on the next flash. Also on the record: the FFT input still carries one seam wherever the FPGA's pointer stopped (EXP-53/54: the head skip was the wrong model), which is leakage, not a wrong peak — S3 material. |
| S3 | Host regression over captured records; a negative control shifts the assumed fs and must fail. |
| S4 | Waterfall stops issuing one fill per pixel column (20,480 draws/frame today — it visibly rasters); axis labeled in Hz on measured codes, bins otherwise; window/averaging controls reachable by button. |

## Open questions

1. Transform size on-device: accumulate 4 records for 4096, or run 1024-point
   per record? (Latency vs. resolution; `scope_freq` chose small-and-fast.)
   **Decided for S1 (2026-09-15): one 896-sample record, zero-padded into the
   4096-point transform** — the padding interpolates, the window now spans the
   record, and one record per frame keeps the view at the capture rate. Bin
   width is still fs/4096 but the true resolution is fs/896.
2. Does the FFT screen share `scope_freq.c`'s transform or the big `fft.c`
   path? Two spectral engines is the two-renderers bug shape all over again —
   pick one. **Still two engines** — `fft.c` for the views, `scope_freq.c`
   for the badge; not resolved by S1, and the S2 bench should quote both.
