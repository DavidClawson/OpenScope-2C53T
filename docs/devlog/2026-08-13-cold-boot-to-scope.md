# Cold boot to a live scope

**2026-08-13**

## Background

For two months the whole project has been one sentence: our firmware sends the
FPGA the same 115,638-byte configuration the stock firmware does, over a bus we
proved works, and the FPGA ignores it. The multimeter runs, the scope is dead.
Every entry before this one is a lap around that wall — ruling things out, mostly,
and occasionally discovering that a thing we'd "ruled out" was measured wrong.

Tonight the wall came down, and by the end of the session the device booted from
cold — no stock firmware, no warm handoff, no cracking the case — into a live,
probe-responsive oscilloscope running our open firmware.

## What broke it

Not a byte. Not the payload. Not a pin, not the chip-select, not the MCU state,
not the clock speed. All of those were already excluded, some of them twice.

It was **how we clocked the bus.** For two months we drove the config upload with
the hardware SPI peripheral. The loader that works — maksidze's, transplanted by
Stlkv from the sibling 2C23T firmware — **bit-bangs** the same bytes on GPIO pins.

We built both and ran them back to back on the same unit the same night:

- **Build A** — our hardware-SPI sequence, cranked all the way to /256 on both
  phases, with the richer prelude reads added. `CFG:00039020`, DONE_FINAL clear.
  The same wall, one more time.
- **Build B** — a byte-for-byte transplant of the bit-bang loader onto our pins
  and our payload. `CFG:0003F460`, **DONE_FINAL set.** Cold. Reproducible.

The GW1N's configuration engine, it turns out, wanted the slow, gapped, ragged
timing that falls out of toggling GPIO in a loop — not the tight, uniform edges of
a hardware SPI controller. We had spent two months perfecting *what* to send. The
answer was in *how*.

(Honest caveat, still open: Build B also drops the `0x05` ERASE_SRAM step that our
sequence sends, so the clean single-variable proof — bit-bang versus AF, or the
missing 0x05 — isn't nailed down yet. But config entry works, and that's the thing
that mattered.)

## Then the engine wouldn't arm

Config succeeding just moved the wall six inches. DONE was set, but the demo trace
stayed on screen — the capture engine boots unarmed, grabs one buffer, and halts.

The five post-config register writes that stock sends (`01 08 / 02 03 / 06 00 /
07 00 / 08 AD`) had "done nothing" in every previous run. They don't do nothing.
They arm the engine — **but only if the run and enable lines are held at the same
time.** Earlier tonight's netlist trace had said exactly this: the arm is a
three-input coincidence — a run line (PB11), an enable (PC6), and one bit shifted
in over SPI — all high together, held. We'd been asserting the pieces one at a
time and watching nothing happen.

The build that finally worked (`guest-coldtrace`) holds PB11 and PC6 high, does
the bit-bang config, fires the five writes, and then — critically — runs the
readout pipeline that actually *reads back* channel data. Demo trace vanished.
`OK:` counter climbing. Power-cycle it: it comes back up to a live trace. Touch
the probe: it responds. Both channels active.

## The measurement traps, again

This project's recurring lesson showed up twice more in one evening.

- The overlay's `S1` field, which is supposed to show the config clock, was being
  sampled at *init* time — before the config sequence changes the clock — so it
  read `/2` no matter what. We nearly concluded Build A "didn't run at /256" from a
  number that could never have shown anything else. Fixed the capture point.
- Build B's "demo trace still there" told us *nothing* about arming, because that
  build doesn't run a readout that could turn the demo trace off. A blind
  instrument reads the same whether the thing works or not. The win only became
  legible once we bolted the config onto a pipeline that actually looks.

Half of tonight's real work, as usual, was building readouts we could believe.

## Where it actually stands

It is a real oscilloscope that captures from a cold boot. It is not yet a
*useful* one, and the reason is instructive. We wired up an ESP32 as a signal
generator and fed it waveforms. A 1 Hz square is crisp. A 2 Hz sine stair-steps
recognizably. At 50 Hz and up the trace freezes to a stuck level.

That's not a fault — it's the absence of a timebase. Each hardware sweep is a
~microsecond, 1024-sample snapshot refreshed about 34 times a second, and this
build never configures the FPGA's sample rate. So it faithfully tracks slow
signals as a moving level (which is exactly why the probe "responds"), and
anything above ~15 Hz aliases into that refresh and stops making sense. To draw an
actual audio-frequency waveform we have to send the timebase commands and slow the
capture down. The scope has a vertical axis and no horizontal knob.

So the next thread is timebase control — and for the first time in the project's
history, the next thread is a *feature*, not a wall.
