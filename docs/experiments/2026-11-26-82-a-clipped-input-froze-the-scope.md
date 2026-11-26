# EXP-82: a clipped input froze the scope

- **Date:** 2026-11-26
- **Unit:** my unit
- **Build:** before: `guest-coldtrace` 8c3caaa (`Nov 25 23:59:30`); after: the
  same + `acq_gate.h` (`Nov 26 11:01:05`)
- **Setup:** loopback, GEN into CH1 (BNC); CH1 x1, DC, timebase 0x0E, AUTO
- **Context:** found on the s00500 fork (`feat/remote-control`), where the
  generator runs on stock's DAC2/PA5 path (deOliveira-R's port, #68), so GEN
  served as the DC source. Any DC source that rails the ADC reproduces it; the
  gate below is the same on `main`. The commit hashes above are the fork's.
- **Status:** CONFIRMED and FIXED.

## 1. Problem

During the gain sweep (EXP-83), with GEN at 1.5 V DC into CH1 on range 4,
`openscope capture` timed out: 1 of 3 captures in 60 s, although AUTO always
commits. Why does a steady input stop the acquisition?

## 2. Hypothesis

The level rails the ADC (raw 0xFF, stored 0xFF − 28 = 227). The acquisition
loop accepts a frame only if a read carried the data-ready marker (0x80) or
CH1 varies, because a dead bus (MISO pulled up) also reads 0xFF everywhere.
A railed input is a flat 227 and is taken for a dead bus. If true: stalls only
at railed levels, the buffer holds 0xE3, rejected-frame counts climb, the
opcode-phase status byte is 0x00 (a dead bus would give 0xFF). If false: the
stall also happens below the rail, or the status byte is 0xFF.

## 3. Procedure

`openscope set ch1.vdiv 4`; `openscope shell "siggen wave 9 amp N on"`;
`capture.gen` read 4 s apart; `spi3 read` (CH1 buffer); `status`
(`SPI3 first byte`, rejected-frame totals).

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| unrailed level, same range (amp 10, 1.0 V) | commits | +290 captures in 4 s, buffer 0xAC | yes |

## 5. Results

Before the fix:

| GEN | `capture.gen` | CH1 buffer | notes |
|---|---|---|---|
| 1.5 V | 2604918, unchanged over 24 s | E3 E3 E3 … | poll reads since the last commit 3604 and climbing; rejected frames 2421 → 3829; first byte 0x00; hw poll expiries 0 |
| 1.0 / 0.5 / 0 V | advancing (+254 to +282 per 3 s) | | |

So the loop was reading normally and discarding every frame. Side effect:
after 32 rejected reads the "dry spell" recovery re-arms DAC1 to mid-scale, so
a clipped input also reset the CH1 vertical offset about once a second.

The same gate is on `origin/main` and on deOliveira-R's `bench/parity`.

Fix (`firmware/src/dsp/acq_gate.h`, used in `fpga.c`): a frame is live when a
read carried the marker, **either** channel varies, or **both** opcode-phase
status bytes are not 0xFF. Each term only admits frames a live FPGA produced;
a dead bus fails all three. The poll-expiry check stays as it was.
`tests/test_acq_gate.c`: 12 checks; the old gate fails 4 of them (both
channels railed, railed low, varying CH2).

After the fix:

| GEN (range 4) | `capture.gen` in 4 s | CH1 buffer |
|---|---|---|
| 1.0 V | +290 | AC AC AC AD … |
| 1.5 V (railed) | +300 | E3 E3 E3 … |
| 2.0 V (railed) | +302 | E3 … |
| 3.0 V (railed) | +288 | E3 … |
| 1.0 V | +294 | AB AD AC … |

Rejected frames 0. A 3 V square on range 4 now shows live, flat at the top
(`railed-square-r4.png`).

## 6. Blind spots

- The dead-bus case was not produced on hardware; it is covered by the host
  test and by the reasoning that an undriven MISO reads 0xFF in the status byte
  too. If a live FPGA ever drives 0xFF in that byte with flat data on both
  channels, the frame is still rejected, which is the old behaviour.
- The measurement row on a clipped trace is wrong without saying so (the
  screenshot shows Vpp 2.34 V for a clipped 3 V square). Not addressed here.

## 7. Conclusion

- **Established:** a railed input made every frame look like a dead bus, so
  the scope stopped updating and kept the last unclipped trace, and the
  dry-spell recovery re-armed DAC1 every ~32 polls. Fixed; railed frames now
  commit at the normal rate.
- **NOT addressed:** flagging clipped measurements.
