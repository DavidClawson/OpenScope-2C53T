# Flash read protection, and why the debugger kept killing the device

**2026-08-11**

## The thing we wanted

The firmware has a debug shell — commands like "re-run the FPGA configuration
sequence" and "send these raw bytes over SPI". It has been unreachable for weeks
because it speaks over USB serial, and USB serial has never enumerated on the
bench unit.

The fix looked easy. The debug probe is already soldered to the board, so we
implemented **RTT** — a protocol where the target writes into a ring buffer in
RAM and the debugger reads it out. Same shell, different pipe.

The payoff would have been large. A 50-pin hardware sweep currently means 50
flash-and-reboot cycles, each needing someone to physically put the device into
upgrade mode. Over RTT it would be a shell loop against one running device.

## It didn't work, and the way it didn't work was strange

The buffers were right. The debugger found the control block and wrote our
command into it. The device never read it.

Instrumenting the firmware showed the shell task started, ran 5,479 loops, and
stopped. The system tick had stopped at the same moment. The CPU reported
"running" but every task was dead.

Reading the ARM fault registers gave the answer — sort of:

```
HardFault active. Escalated. Instruction access violation.
```

Something had jumped to an address it couldn't execute. Worse, the loop counter
put the freeze at **54.8 seconds** after boot, and that reproduced. So we now had
a second bug: a firmware that crashes about a minute into every boot.

Which would have been a serious finding, and would have explained months of
intermittent "the device just hung" reports.

## Except it wasn't happening

We built a proper fault handler — one that records the fault type, the exact
faulting instruction and the CPU registers into a region of RAM that survives a
reset. It worked: we deliberately crashed the firmware and it captured the
crash byte-perfectly, naming the exact instruction.

But it never captured the "55 second crash". Not once.

A handler that catches a deliberate crash perfectly but never catches the real
one is telling you the real one isn't what you think.

Then the obvious question, which nobody had asked: **does it crash when nobody is
watching?**

Five minutes with the device on the bench and the debugger unplugged. The
waveform kept animating. Plug the probe in physically: still fine. Run the
debugger software: **frozen instantly.**

There is no 55-second crash. The counter froze at 54.8 seconds because that is
when we first attached.

## The actual cause

```
FLASH_OBR = 0x03FFFFFE   ->  read protection is enabled
```

The microcontroller has **flash read protection** switched on — an anti-piracy
feature that stops anyone dumping the firmware.

On this family of chips, that protection doesn't merely block the debugger from
reading flash. **Connecting a debugger disables the flash memory entirely.** And
the CPU runs its code *from* flash. So the instant the debugger attaches, the
processor can't fetch its next instruction and dies — and the crash handler is
also in flash, so that can't run either. The chip ends up in a hardware lockup
state.

Everything falls out of that one fact:

| What we saw | Why |
|---|---|
| Freezes whenever the debugger connects | flash switched off under it |
| Every debugger halt reporting a lockup | it really was locked up |
| "Instruction access violation" | fetching from disabled flash |
| Crash handler never recording anything | the handler is in flash |
| **RTT never working** | it needs the CPU *alive* while the debugger reads RAM |
| The deliberate crash test working fine | it crashed *before* we attached |

That last row is the tidy confirmation.

## Two retractions

**There is no ~55-second crash bug.** We wrote several commits saying there was.
There isn't. The device runs indefinitely.

The mistake is worth naming: we observed something only ever *through* an
instrument, and blamed the target instead of the instrument — the same error as
the FPGA clock-speed saga in the
[previous entry](2026-07-28-the-fpga-was-listening.md). The control experiment
took five minutes and should have come first.

**The RTT console can't work on this device**, in any form, while read protection
is on. Neither can a related plan to sample all the chip's pins live over the
debug port. Both need a running CPU with a debugger attached, which this chip
does not permit.

## What still stands

The debug probe remains useful in a different mode: attach, accept that the CPU
stops, and then drive the hardware *from the host*. Peripherals keep whatever the
firmware configured before the attach, and the FPGA doesn't care why the CPU
stopped. That is exactly how our most productive FPGA measurements were taken,
and they're unaffected.

Live firmware state is read the way it always has been: printed on the LCD.

## Where this leaves things

Read protection could be cleared — but doing so triggers a full chip erase by
design, which would take the factory bootloader with it and require opening the
case to recover. That's a real trade for a real payoff, and not one to make
casually.

Meanwhile the fault handler built along the way is a genuine improvement and is
staying: crashes now identify themselves specifically instead of collapsing into
a generic "HardFault", and the record survives a reset.
