# The pins the diff couldn't see

**2026-08-11** — second entry today. The [first one](2026-08-11-read-protection.md)
was about the debugger killing the device. This one is about the bench work that
followed it.

## The plan for tonight

The session plan had a clear first item: test two GPIO pins, PC1 and PC2,
**pulsed together**.

The reasoning was three months old and looked solid. A static scan of the stock
firmware had listed every pin the original firmware drives both LOW and HIGH
before it configures the FPGA — the only pins that are "pulse-shaped" at all.
PC1 and PC2 were on that list, driven from the same pair of instructions. An
earlier sweep had pulsed PC1 on its own and found nothing, but never the pair.

We were hunting a reset pulse. The FPGA's configuration port has a dedicated
reset pin, and the Gowin documentation is explicit: a part that has already
booted its own resident design will not accept a new configuration until that pin
is pulsed. Stock firmware succeeds, so stock must be pulsing something.

## The premise was wrong

Before writing any code, we disassembled the two instructions.

```
==0                    neither pin touched
==1   BSRR=4, BSRR=2   ->  PC2 HIGH, PC1 HIGH
==2   BRR =4, BRR =2   ->  PC2 LOW,  PC1 LOW
==3   BSRR=4, BRR =2   ->  PC2 HIGH, PC1 LOW
```

It's a four-way selector writing a **2-bit code** onto the two pins. Every branch
writes either the set register or the clear register — never both. **No path
through that code produces a low-then-high edge.** Stock never pulses these pins
at all.

The old scan had reported them as "driven both ways" by collecting writes from
*different branches of the same switch statement* and adding them together. Two
mutually exclusive arms of one `if` looked like one pin going down and then up.

The same pattern turns up at eight sites scattered across the firmware image.
Eight call sites is what a functional control looks like — a range selector, a
signal mux. A configuration reset pin gets driven once, during startup.

So the test we were about to spend a bench session on would have been testing a
waveform the original firmware never generates.

## Why PC1 was never on the candidate list

That's the part worth writing down.

Three weeks ago we ran an experiment that compared our firmware against stock at
the exact instant the FPGA configuration begins — halt both, dump every register,
diff. It found five differences, we matched all five, and the FPGA still refused.
We wrote down: *static MCU state is excluded.*

That diff compared **output levels**. And there is a case it cannot see:

| | pin configuration | output level |
|---|---|---|
| stock | output, actively driven LOW | 0 |
| ours | input, floating, not driven at all | 0 |

A pin held low by the processor and a pin connected to nothing report the same
number. They are completely different signals on the wire.

Re-running the comparison against the **configuration** registers instead of the
output levels turned five differences into about twenty. After removing pins with
known jobs, seven were genuinely open — including PC1, which no build of ours had
ever driven, and which is still listed as "unknown function" in our pinout notes.

So "static MCU state is excluded" had been true only of what that particular
comparison was capable of detecting. We'd been treating it as a closed door for
three weeks.

## The results

Two firmware images, both matching stock exactly.

| what we drove | FPGA status |
|---|---|
| PC1 alone, at stock's level | `00039020` |
| all seven pins, together | `00039020` |

`00039020` is the same value we have been staring at since June. No change.

So the class really is closed now — but it's closed by a measurement that could
actually see it, which wasn't true this morning.

## Two things we got for free

**Nobody had ever power-cycled the FPGA.** The Gowin docs give two ways to make a
running part accept a new configuration: pulse the reset pin, or remove power. We
had been assuming a reset button counted.

It doesn't. The pinhole reset resets the processor only — the FPGA stays powered
through it. And holding the power button while the USB cable is attached doesn't
work either: the shutdown releases the power-hold latch, but USB is still feeding
the rail, so the device sits on the "Goodbye" screen indefinitely, fully powered.

The only sequence that genuinely removes power from the FPGA is:

> hold POWER → "Goodbye" → **unplug USB** → replug

Which means a large fraction of this project's measurements were taken on an FPGA
that had not lost power in a very long time. We did it properly. The status was
identical. A fresh power-on doesn't open the configuration port for us either.

**A serial-port theory died without a bench test.** With every GPIO form of a
trigger excluded, the only remaining channel from the processor to the FPGA is the
serial link they use for multimeter commands. Maybe stock sends a "get ready to be
reconfigured" message before it starts.

It doesn't. Stock's serial port setup ends by explicitly *disabling* the
peripheral, and never writes to the transmit register at all. Nothing can leave
the processor on that wire before configuration starts. Ten minutes with a
disassembler, no hardware involved.

## The thing that doesn't add up

Left unresolved deliberately, because it's the most interesting thing we found.

Before our firmware sends a single configuration byte, the FPGA already reports
itself as **not configured** — and it answers our queries. We know from a separate
test that a *successfully* configured part goes silent on that interface entirely:
the port closes and the pins hand over to the running design.

But our multimeter works. Voltage and resistance, both accurate.

If the FPGA were doing the metering, it would have to be configured. It plainly
isn't. So either that status bit means something different than we think, **or the
multimeter was never the FPGA** — a separate chip sharing the same serial line.

Our entire model of this board says the FPGA holds a resident meter-only design.
That model rests on one experiment: disable the configuration upload, and the
scope dies while the meter keeps working. Which supports "the resident design is
meter-only" — and supports "the meter was never the FPGA" exactly as well. We
never separated the two.

The counter-argument is real and we're keeping it in view: that same experiment
*did* kill the oscilloscope, so stock's scope genuinely depends on the upload. Any
theory where the configuration port is shut to everybody still has to explain how
stock gets through it.

We've asked the contributor who mapped this board where those serial lines
physically land. It's a continuity check on a board that's already open, and it
costs us nothing.

## The lesson, which we keep re-learning

Three of tonight's four results came from reading code, not from the bench. The
pulse that stock never generates. The branch we needed, already sitting in a dump
file from three weeks ago. The serial theory killed by a disassembler.

And the headline finding is the fourth time on this project that a stable,
repeatable, believable measurement turned out to be blind to the exact thing it
was being used to rule out. The others were reading a register at a clock speed
that garbles it, reading a pin with no defined idle level, and a script that
rotated bytes.

The pattern is always the same: the number looks fine, it reproduces, and nobody
asks what the instrument is incapable of showing.

So that's the question going in the plan document, ahead of the experiment list:
**before running it, ask what this measurement cannot see.**
