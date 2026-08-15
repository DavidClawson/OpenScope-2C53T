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

The main thread now is a *feature*, not a wall: **timebase control**, so the scope
can display waveforms across the frequency range instead of just tracking slow
levels.
