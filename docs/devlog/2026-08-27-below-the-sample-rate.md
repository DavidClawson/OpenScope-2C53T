# Below the sample rate

**2026-08-27**

## Background

The scope came up cold on open firmware back on 2026-08-13, and the way it
happened left a loose end. The config upload works when we **bit-bang** it on
GPIO, and walls when we drive the identical bytes over the **hardware SPI
peripheral**. Two months of work had already excluded the bytes, the payload, the
pins, the chip-select, the MCU state, and the clock speed. On the 13th we found
the last axis that mattered — *how* the bus is clocked — and shipped the working
bit-bang path. But we never actually understood *why* the hardware-SPI path fails.
This session was about closing that, one way or another.

## Ruling out the transaction, byte by byte

Four experiments, each removing a candidate:

- **The upload isn't DMA.** We'd wondered if stock streams the 115 KB via DMA
  while we poll with gaps. Disassembling stock's own upload loop settled it: it's
  a plain CPU per-byte loop, no DMA channel anywhere near the SPI data register.
  Both paths poll. (Ours is actually *less* gapped than stock's.)
- **The `0x15` frame is identical to stock's.** Byte for byte, chip-select
  framing, SPI mode, even the pin's drive-strength register — all match the stock
  firmware that configures fine. And they match our bit-bang. Nothing in the
  frame distinguishes the path that works from the path that walls.
- **It isn't the clock rate.** The hardware-SPI path walls at 60 MHz, 1.9 MHz, and
  470 kHz. The bit-bang path configures at ~2 MHz *and*, we confirmed this
  session, at 34 kHz. Whatever rate you pick, GPIO configures and hardware-SPI
  doesn't. Rate is out.
- **It isn't a stray clock edge from switching the prescaler.** The one dynamic
  thing our path did that stock's didn't — toggling the SPI enable when it changed
  clock divider — we suppressed entirely. Still walls.

By the end, every axis a firmware writer can control was either matched to stock
or excluded. Which left exactly one difference between our configuring path and
our walling path: whether pins PB3/4/5 are driven by **GPIO push-pull** or by the
**SPI peripheral's alternate-function** output.

## The capstone: the two waveforms are the same

So we put the logic analyzer on the bus and captured both — sixteen identical
`0x15` frames driven over hardware SPI, then sixteen driven by bit-bang — same
pins, same frame, in one window. Then decoded and diffed them edge by edge.

They're identical. Exactly 32 clock edges inside chip-select on every single
frame, both decoding to the same two bytes, both in SPI mode 3, both dead
consistent across all sixteen repetitions. No extra edge, no glitch, no phase
shift, no framing difference. The frame that walls and the frame that configures
look **the same** on the wire.

Which means the thing that makes one work and one fail is smaller than our
analyzer can see. The HiLetgo samples at 24 MHz — 42 ns per sample. The most
likely culprit is a sub-42-ns transient the SPI peripheral emits that explicit
GPIO writes don't: a runt clock, a glitch at a byte boundary, something on the
order of nanoseconds. At 24 MHz it's invisible. Catching it needs a scope or a
buffered analyzer at 100+ MS/s — and the bench doesn't have one.

(There's a lovely bit of honesty owed here: while measuring the slow bit-bang we
discovered it runs at 34 kHz, not the ~470 kHz we'd estimated — the estimate was
14× off. The config result didn't change, but the number in the previous writeup
was wrong, and it's corrected now. This project has a long history of stable,
plausible, wrong numbers; this is one more.)

## Why we're leaving it here

Here's the thing that turns a frustrating dead-end into an easy decision: **it
doesn't cost us anything.** A bitstream is a bitstream. Once the FPGA is
configured — and the bit-bang path configures it, cold, every boot — the fabric
runs the exact same 250 MS/s design at full spec, no matter how the bits got in.
The hardware-SPI-vs-bit-bang difference is purely the *load method*. Its entire
cost is a second or two of extra boot time while the slower bit-bang clocks the
upload in. No feature depends on it. No runtime performance is lost.

So the choice is: spend $150 on a fast analyzer to chase a nanosecond-scale
transient whose only prize is a slightly faster boot — or spend that attention on
the parts of the instrument people actually hold in their hands. We're doing the
second one.

The config-entry mystery is **parked, not abandoned.** It's characterized down to
one physical difference below our resolution, and it's all written up
(experiments 27 through 37). If a fast analyzer ever wanders onto the bench, the
capture rig is already built — one shell command replays both waveforms. And if
you're reading this with a DSLogic and an evening to spare: the door's open, and
we'd love to know what that transient is.

## What's next

Back to features. The standout gap is **channel 2** — it rails on most ranges,
and (importantly) that's *not* the config wall. It's the analog front end: CH2's
vertical offset is a PWM-DAC on a timer our firmware never programmed. The runtime
knobs are already in place; it needs a bench session with the working CH1 offset
as a positive control. After that: nailing down the absolute vertical scale
against a trusted source, and meter polish.

The scope works. Time to make it good.
