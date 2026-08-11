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
| 2026-08-11 | [Flash read protection, and why the debugger kept killing the device](2026-08-11-read-protection.md) | Attaching SWD disables the flash array. The CPU executes from flash. Every "mysterious hang" for two months was us. |
| 2026-07-28 | [The FPGA was listening the whole time](2026-07-28-the-fpga-was-listening.md) | Six weeks of conclusions rested on reading a register at the wrong clock speed. |

## The standing problem

The scope's analog front end is driven by a **Gowin GW1N-UV2 FPGA**. The stock
firmware uploads a fresh FPGA configuration over SPI at every boot; without it,
the multimeter works but the oscilloscope is completely dead.

Our firmware sends the same bytes, from the same state, over a bus we have proven
works — and the FPGA ignores them. Finding out why is the main thread running
through these entries.
