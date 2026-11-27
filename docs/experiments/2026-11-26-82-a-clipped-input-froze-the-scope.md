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

## Addendum 2026-11-27: review of #109 (open config port, MISO pull-up)

**Question (maintainer's review):** on a part whose configuration failed
(port still open), what do 0x04/0x05 put in the opcode-phase byte? If it is
driven, the third term commits flat frames and latches off the demo trace.

**Answer from existing evidence, no new bench run:** EXP-31's wire capture
(2026-08-26, failed hardware-SPI config, port anchored open, /256) shows
0x04/0x05 returning `00 01 C8 10 ...`: a **0x00** opcode-phase byte, then the
STATUS register streaming. MISO is driven the whole time on an open port.
So the term as first written did not tell a configured capture engine from
a failed config. At the acquisition clock (/2) no record of these bytes on an
open port exists.

**Fix (6ec5544):** the third term is trusted only when this boot's config
sequence read the IDCODE after the upload and it did not answer
(`acq_opphase_trusted()`; Exp L: a configured part closes the port, Exp M: a
failed one keeps answering 0x0120681B). DONE_FINAL cannot be the test:

| boot | STATUS (our read) | IDCODE after upload |
|---|---|---|
| cold, config took (Build B) | `0003F460` (D1) | silent |
| warm, part still configured (my unit, CDC flash, 2026-11-27, live capture) | `00000000` (D0) | silent |
| config failed (Exp M/N, EXP-31) | `00039020` (D0) | answers |

**Found on the way:** `fpga_bitbang_config_sequence()` handed PB4 (MISO) back
to SPI3 as a FLOATING input, overriding the pull-up `fpga_init()` sets, so the
"dead bus reads 0xFF" premise above did not hold in `guest-coldtrace`. Fixed
in the same commit.

**Bench, my unit** (`guest-coldtrace` of `feat/remote-control` b8a4904,
`Nov 27 2026 10:21:53`, flashed over CDC = warm FPGA; GEN DC into CH1, CH1
range 4 DC, CH2 open at range 7, AUTO):

| check | before (`Nov 26 23:53:37`) | after |
|---|---|---|
| `gpio read B4` | `CRL 4 = floating in` | `CRL 8 = pull-up in, odt=1` |
| `status` STATUS / IDCODE after close | `00000000` / silent | `00000000` / silent |

| step (timebase 0x08, then repeated at 0x0E) | `capture.gen` in 4 s | CH1 | CH2 |
|---|---|---|---|
| control: GEN 1.0 V, TMR13 2114 | +292 / +292 | 170-176 | 50-52 |
| GEN 3.0 V (CH1 railed), CH2 open | +292 | 227 flat | 50-52 |
| GEN 3.0 V, TMR13 4095 (CH2 railed high) | +292 / +290 | 227 flat | 227 flat |
| GEN 3.0 V, TMR13 0 (CH2 railed low) | +286 / +292 | 227 flat | 0 flat |
| control again: GEN 1.0 V | +292 / +290 | 170-173 | 50-52 or 0 |

**What this run could not see:** the CH1 opcode-phase byte read **0x80** (the
data-ready marker) on every sampled read, at 0x08 and at 0x0E, so the marker
term admitted these frames before the third term was reached. On the
pre-fix image of 2026-11-26 the same setup read 0x00 there (section 5); what
changed in between is not known. So this run shows no regression (railed
frames on both channels still commit at the normal rate) and confirms the
pull-up and the warm-boot anchor by readback, but it does **not** exercise the
third term or the trust decision on hardware. The CH2 half of the `acq hdr`
status line was cut off in the shell output (print buffer), so CH2's byte is
not recorded. The failed-config case itself was not produced on hardware; it
needs a config-off image and a true FPGA power cycle. Host side:
`test_acq_gate` 13 scenarios, the first EXP-82 gate fails the two open-port
cases, the pre-EXP-82 gate the four railed ones.

**Follow-up, same day:** `status` now prints whether the term is trusted
(7138d85). On my unit, after a CDC flash (warm FPGA, `Nov 27 2026 10:55:23`):
`acq opcode-phase term: trusted (IDCODE silent after our upload: port closed)`,
PB4 still `CRL 8 = pull-up in`. So the trust decision is confirmed true on a
warm boot by readback; that it is false on an open port remains host-tested
only.
