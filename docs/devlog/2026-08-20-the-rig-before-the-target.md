# The rig before the target

*2026-08-20 (evening)*

The config-transport bisect has been on the plan for a week as three
experiments — BIS-1 (prelude reads), BIS-2 (the missing `0x05`), BIS-3 (the
waveform) — all waiting on an FPGA dev board in the mail. Tonight, instead of
waiting, we built and commissioned the instrument that will run them. It works,
it already caught a bug, and against *no target at all* it surfaced a testable
hypothesis about the wall.

The wall itself is not the point anymore. `guest-coldtrace` bit-bangs the FPGA
up cold every boot; the scope ships. The open question is *why* bit-bang
configures a part that hardware-SPI can't — a mystery that has sat in the
project's core narrative since 2026-08-13, and whose answer buys a config path
~100× faster and, plausibly, the fix for the USB-CDC-drops-on-non-coldtrace
correlation.

## The rig

A Blue Pill (STM32F103) standing in for the scope's MCU. The AT32F403A is
STM32F1-family compatible, so both of `fpga.c`'s config sequences —
`fpga_spi3_config_sequence` (hardware SPI, the one that failed for two months)
and `fpga_bitbang_config_sequence` (the Stlkv/maksidze transplant that broke the
wall) — port with a pin table and a clock setup. The port keeps every fidelity
detail we've learned is load-bearing: the F103's SPI1 remap lands the bus on
**the scope's exact pins** (PB3/4/5 + PB6 soft CS), stock's SWJ posture
(`MAPR=0x02000000`, the Exp E value), MISO pull-up by default (the instrument
bug the floating-MISO saga taught us), mode-3 MSB-first, reads at /256, and each
transport's *native* read framing rather than a unified one.

It's `bluepill_bis/` in the repo — a standalone 3.4 KB bare-metal firmware plus
a UART host driver. Single-letter legs map 1:1 to the plan: `a` hardware-SPI
base, `1` BIS-1, `2` BIS-2, `b` bit-bang V0.4, `B` bit-bang-with-`0x05`
cross-check, `i`/`I` the IDCODE anchor. Every leg prints the wall signature
directly — `ST15:xxxxxxxx EDIT:0 DONE:0`.

## Commissioning, in the order it actually happened

Starting from "does USB program these things?" — no. The F103 ROM bootloader
speaks UART, not USB, and our rig has no USB stack anyway, so a blank Blue Pill
is invisible on its own USB port. The **ST-Link** is the tool.

- First OpenOCD probe: `UNEXPECTED idcode: 0x2ba01477`. The bench Blue Pill is a
  **clone** — CS32-class, not a genuine ST F103 (which reads `0x1ba01477`) — and
  it carries the full 128 KiB. `make flash` now passes `CPUTAPID 0` to accept
  it. Irrelevant to the sequence experiments; already a documented BIS-3
  waveform caveat, since a clone's SPI silicon isn't the AT32's either.
- Console came up over the **FT232H** — its first use on this bench, three wires
  to PA9/PA10 at 115200. Banner and help printed. Both IDCODE probes answered
  `FFFFFFFF`, `OFF:-1`: the correct null-target result — MISO pulled up, nothing
  driving it, exactly the "undriven MISO reads FF" signature from the stock
  capture, and the anchor search correctly refusing to find an IDCODE in it.

## The bug it caught before the FPGA even arrived

Leg `i` (an IDCODE read on a fresh bus) worked. Leg `a` (the same read, but
*after* the prelude's `05/12/15`) hung. The logic-analyzer capture showed why in
one glance: the prelude clocked out perfectly — bare CS pulse, then `05 00`,
`12 00`, `15 00`, 100 ms apart, stock-faithful — and then the trace just
stopped, CS stuck low, mid-read.

The difference between the two legs is that leg `a` has a byte in flight when the
read begins. `spi_set_br()` cleared SPE and rewrote `CR1` while the SPI was still
`BSY` from the prelude's last byte — the classic STM32F1 reconfigure-while-BSY
trap, which wedges the next transfer. Fixed with a BSY-wait before touching
`CR1`. And `spi_xfer()` is now timeout-guarded like `fpga.c`'s own `spi3_xfer`,
so a stuck bus returns `0xFF` instead of hanging — because over a UART, a hung
rig and a dead target look identical, and we have learned the hard way not to
build instruments that can't tell you they're stuck.

Both are lessons the main firmware already encodes. Better to relearn them on a
null target on a Tuesday than mid-bisect against the Tang Nano.

## The dry run that wasn't supposed to find anything

BIS-3 — the waveform diff — was filed as the *last* resort, the thing to build a
capture rig for only if BIS-1 and BIS-2 both fail. But the capture rig is just
"the HiLetgo clipped to four pins," and the rig drives both transports on
command, so there was nothing to wait for. Capture leg `a`, capture leg `b`,
compare.

Even against a null target, the two transports are visibly different animals:

| | hardware SPI (leg `a`) | bit-bang GPIO (leg `b`) |
|---|---|---|
| clock period | 3.500–3.625 µs (**281 kHz** = /256 off 72 MHz — exactly right) | 0.25–1.375 µs, jittery |
| byte-to-byte | **gapless** — the FIFO streams | **~1.4 µs inter-byte gaps** from the loop |

That inter-byte gap is the interesting one. The hardware peripheral finishes a
byte and the next byte's clock begins immediately; the bit-bang loop pauses
between bytes while the C code goes around. If the Gowin config engine cares
about *how* the bytes arrive rather than *what* they are — and everything we
know says the bytes and the payload are identical between the two paths — then
the inter-byte gap and the clock character are the prime suspects. That is
exactly the layer byte-level fidelity work was structurally blind to, and now we
have a name for it before the target is even on the bench.

## What this is NOT

Discipline, because this project has been burned by stable-but-wrong numbers
more than once:

- **Null target.** All-`FF` is the absence of a device, not a measurement of
  one. The waveform *shapes* are real; any claim about how the FPGA *responds*
  to them is not yet on the table.
- **Clone F103.** Fine for the sequence experiments; for the waveform question a
  clone is only "STM32F1-class" evidence, and neither clone nor genuine is the
  AT32. A positive BIS-3 must eventually be confirmed at the scope's own pins.
- **The quick frame-parser is rough** — it mis-segmented some CS blips and its
  per-frame clock counts aren't trustworthy. The clean comparison uses sigrok's
  actual SPI protocol decoder (the decode line is sitting in `capture.sh`), and
  it needs a target driving MISO to be worth running.

## Monday

The Tang Nano 9K arrives — GW1NR-9, LittleBee, embedded-NV auto-boot: the same
silicon config engine and the same boot mechanism as the scope's GW1N-UV2, on a
part whose config pins are finally *observable*. Opening move: wire the four bus
lines + GND, run leg `i` first to read its real IDCODE and set it with `e`,
confirm the anchor — then walk the legs and watch whether `EDIT` or `DONE` ever
flips.

## Lessons

- **Build the instrument while you wait for the subject.** A week of "blocked on
  hardware" turned into a commissioned rig, a fixed bug, and a hypothesis — none
  of which needed the hardware. The capture rig we'd deferred as expensive was
  four clips and a script.
- **A rig that can hang is a rig that can lie.** The timeout guard isn't
  defensive padding; it's the difference between "the target is dead" and "my
  instrument is stuck," and over a serial line those are the same three
  characters until you make them different.
- **The dry run is a real experiment.** We filed BIS-3 as the fallback and it
  produced the evening's most concrete lead. Running the cheap version of the
  last-resort experiment first cost nothing and moved the question forward.
