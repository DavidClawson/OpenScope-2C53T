# Devlog

Dated notes on what we tried, what worked, and — mostly — what didn't.

This project is a reverse-engineering effort, which means progress is lumpy.
Weeks go by where the interesting output is "we ruled out four things." Those
weeks are worth writing down: a negative result that is properly measured is the
thing that stops the next person repeating it.

So these entries are deliberately unpolished. They include the wrong turns, the
measurements that turned out to be instrument artifacts, and the conclusions we
had to retract. If you are looking for release notes, see the
[releases](https://github.com/DavidClawson/OpenScope-2C53T/releases) page
instead.

## Entries

| Date | Title | Short version |
|---|---|---|
| 2026-08-17 | [Two channels, and the meter came back](2026-08-17-two-channels-and-the-meter.md) | CH2 was a 2-bit channel mask on PC1/PC2 we left floating (mask 1 = CH1 into both converters); the meter was PC11 plus a mode-entry sequence we had only half implemented. Both fixed, cold boot, no stock. Also: a paired test proved two physical converters, un-retracting a correct result we had wrongly withdrawn — and seven instrument defects that drove new tooling. |
| 2026-08-15 | [The signal was never there](2026-08-15-the-signal-was-never-there.md) | Yesterday's "two channels" and "acq-path bug" were one ESP32 output on both opcodes and a first-4-bytes-vs-full-buffer artifact. Then: the timebase is SPI reg 0x01 (1-2-5 ladder, stock's `0F` ≈ 30 kS/s = the 23×), the path is AC-coupled at ~9 Hz, PC12 is the input-connect relay, and the LCD showed its first real waveform. Open: why op 0x05 carries CH1, the seam, the coupling relay. |
| 2026-08-14 | [The buffer was always full](2026-08-14-the-buffer-was-always-full.md) | Chased the 23× into the FPGA netlist (a programmable-rate counter on a buffer we've never read), then realized the capture buffer free-runs regardless of triggering — the frozen display was our read-pacing, not a hardware limit. Shipped auto/free-run mode + stock's real relay table. Found the analog front end is only a ~2× attenuator; volts/div is digital. |
| 2026-08-14 | [The wedge that never was](2026-08-14-the-wedge-that-never-was.md) | The "engine wedge needing a power cycle" was never real — it was the trigger regime plus lying instruments. Found the FPGA's digital trigger-level register (0x08), measured the default sample rate (~2.7 kS/s), got real triggered captures. Open question: what makes stock 23× faster. |
| 2026-08-13 | [Cold boot to a live scope](2026-08-13-cold-boot-to-scope.md) | The wall came down. It wasn't the bytes — it was that we drove config over hardware SPI instead of bit-banging GPIO. Cold-boot to a live, probe-responsive scope on open firmware. |
| 2026-08-12 | [The wall came down, and it wasn't us who pushed it](2026-08-12-the-wall-came-down.md) | The four-month arc, the six theories that died on the way, and the two contributors — Stlkv and maksidze — who actually solved it. Plus the first live trace, via warm handoff. |
| 2026-08-11 | [The pins the diff couldn't see](2026-08-11-invisible-pins.md) | We'd ruled out a whole class of hardware difference using a comparison that couldn't detect it. |
| 2026-08-11 | [Flash read protection, and why the debugger kept killing the device](2026-08-11-read-protection.md) | Attaching SWD disables the flash array. The CPU executes from flash. Every "mysterious hang" for two months was us. |
| 2026-07-28 | [The FPGA was listening the whole time](2026-07-28-the-fpga-was-listening.md) | Six weeks of conclusions rested on reading a register at the wrong clock speed. |

## The standing problem — SOLVED 2026-08-13

The scope's analog front end is driven by a **Gowin GW1N-UV2 FPGA**. The stock
firmware uploads a fresh FPGA configuration over SPI at every boot; without it,
the multimeter works but the oscilloscope is completely dead. For two months our
firmware sent the same bytes, from the same state, over a bus we had proven works
— and the FPGA ignored them.

**On 2026-08-13 the wall came down.** The difference was never the bytes; it was
that we drove the upload over the hardware SPI peripheral, while the loader that
works (maksidze/Stlkv's, from the sibling 2C23T firmware) **bit-bangs** the same
bytes on GPIO. Bit-bang it, hold the run/enable lines for the engine arm, and the
device boots from cold into a live scope on open firmware. See the
[2026-08-13 entry](2026-08-13-cold-boot-to-scope.md).

Timebase control is now **solved and measured** — it is SPI3 register `0x01`, a
1-2-5 rate ladder, eight codes calibrated and agreeing with an independent rig on
a different unit. Both scope axes carry real numbers as of 2026-08-19.

The main thread now is **wiring the layer above acquisition**: the FFT, math,
measurement and decoder code that is written, host-tested, and still fed
synthetic input. The [feature maturity table](../../README.md#feature-maturity)
tracks how far each one has actually been taken.

Recent entries worth reading for method rather than result:

- [The labels were never measured](2026-08-18-the-labels-were-never-measured.md) — a
  volts/div table that nothing had ever derived from anything.
- [The popup wasn't flickering. It was the flicker.](2026-08-19-the-popup-was-the-flash.md) —
  a true comment in the wrong place becomes a decision, and fixing an
  observability bug is how you find the bug underneath it.
