# Scope display / two-channel demo — next-session starting point (2026-08-14)

Written at the end of a long demo-bring-up session. The scope **captures** real
two-channel signals; the on-screen **display** is one firmware bug away from
showing them cleanly. This doc is the pick-up point.

## TL;DR

- Cold-boot open firmware captures **real, distinct CH1+CH2 signals** — verified
  directly on the SPI3 bus (`spi3 opread 04/05`).
- The **display** renders the acquisition task's buffer, and that buffer reads
  **flat** while a direct bus read of the same opcode shows the full swing. That
  mismatch is the whole reason the screen shows jitter instead of waveforms.
- Fix the acq-path read → the two-channel demo works on screen. Everything else
  (config, arm, relays, auto-mode, free-run, cal tooling) is done and committed.

## Verified working (don't re-litigate)

1. **Two distinct channels.** CH1 (ESP32 DAC1/GPIO25) and CH2 (DAC2/GPIO26) read
   as DIFFERENT signals on the bus — 57/64 samples differ >10 codes. NOT mirrored.
2. **Free-run readout is reliable.** With the trigger level (SPI reg `0x08`) pushed
   OUT of the signal's range (`spi3 seq 08 ff`), 4 consecutive full-buffer reads
   were bit-stable: CH1 69–175 (span 106), CH2 67–173 (span 106). Repeatable.
3. **Trigger-regime intermittency is the OLD bug** (Addendum 1/2): when the signal
   crosses the trigger level, the 0x04/0x05 readout serves stale/flat/partial
   frames. A slow square crossing level 0xAD(173) made the bus reads flicker
   between span-137 and span-4. **Free-run (0x08 out of range) avoids it.** So any
   display path MUST run free-run until the per-capture re-arm is solved.

## THE BUG to fix (the display blocker)

**`fpga_warmtest_read_channel` (acq task) reads a FLAT buffer while
`spi3_opread_window` (direct) reads the real swing — same opcode, same instant.**

Evidence (free-run, real signal on the bus):
- `fpga acq` → CH1 first bytes `71 71 71 70` (span ~1), CH2 `6E 6E 6B 6E`.
- `spi3 opread 04 1026` → min 86, max 172 (span 86). CH2 similarly swinging.
- The display renders `fpga.ch1_buf`/`ch2_buf` (the acq read), so it sees flat →
  the autofit render zooms flat noise → full-band jitter, both channels similar.

Concrete leads (in priority order):
1. **Clock divider.** `fpga.c` comments say **"/256 is the only valid read clock"**
   for SSPI, yet the acq task reads bulk data at **/2** (`spi3_set_br(0)`).
   `spi3_opread_window` may be reading at a different effective rate. Check what
   BR each path uses at read time; try making the acq read use the same clock as
   the (working) direct read. If the fix is "read 0x04 at a slower divider," that's
   a throughput hit but proves the mechanism.
2. **Back-to-back two-channel read desync.** The acq task reads 0x04 then 0x05
   every cycle (`fpga_warmtest_read_channel` twice). The second CS frame may
   desync the SSPI byte alignment (Exp L: reads on a running configured part can
   desync acquisition). Try a settle/CS gap between the two, or read one channel.
3. **`spi3_xfer` vs `spi3_raw_xfer`.** The two transfer primitives differ; the
   direct read uses `spi3_raw_xfer` (get its full body at usb_debug.c:3992 — I
   only saw the forward decl). Diff the two for timing/flag-wait differences.
4. **Acceptance gate / data_ready.** If the acq frames are being REJECTED
   (`varies==0` because the read is flat), `data_ready` may be stale and the
   display could be showing an old frame. Confirm `data_ready` toggles with fresh
   good frames once the read is fixed.

**Repro:** ESP32 `sine 5`/`amp 800` on CH1, `2 sine 8`/`2 amp 800` on CH2;
`fpga scope range 8`; `trig raw 2380` (center); `spi3 seq 08 ff` (free-run). Then
compare `fpga acq` first bytes vs `spi3 opread 04 256` min/max. They should match
after the fix.

## Uncommitted WIP (on the flashed device, NOT in git)

Two changes are in the working tree, flashed but held back pending a working display:
- **`firmware/Makefile`**: removed `-DSCOPE_DEBUG_OVERLAY` from `C_DEFS` (the debug
  strip painted over the bottom of the trace). Keep this — it's a clean win.
- **`firmware/src/ui/scope_ui.c`**: `draw_channel_autofit()` — autoscale render
  (fits each channel to its own min/max, connected lines, top/bottom split for two
  channels). Reasonable, but it AMPLIFIES a flat/noisy buffer into full-screen
  jitter — so it only looks good once the acq-path bug is fixed. Validate the
  display first, then decide whether autofit stays or a fixed volts/div scale is
  better. Commit both once the demo renders.

## Bench setup notes for next time

- ESP32 command ORDER matters: set waveform/freq FIRST, then `amp` (amp before
  sine resets amplitude). CH2 = `2` prefix (`2 sine 8`).
- Read the FULL 1024 buffer for amplitude; short 128/256 reads of a slow (<10 Hz)
  signal catch a flat phase-slice and mislead.
- After ANY DAC1 change, wait ≥430 ms (one buffer fill) before reading — the
  buffer free-runs and lags.
- Physical: CH1/CH2 jumpers on a breadboard are flaky; a loose input reads flat
  with a drifting level. Confirm ground too.
- Demo signal frequency is capped low (~5–10 Hz) by the ~1–2 kS/s sample rate;
  crisp higher-frequency traces need the timebase (23×) work.

## Device left in state
range 8, DAC1 2380 (centered), free-run (`08 ff`), CH1 5 Hz sine + CH2 8 Hz sine.
