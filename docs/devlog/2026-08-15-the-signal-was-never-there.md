# The signal was never there — and the timebase was in the arm sequence all along

**2026-08-15**

## Where we left off

Yesterday's entry ended with a display blocker that looked like a firmware bug:
the acquisition task's buffer read *flat* while a direct read of the same opcode
showed the full swing of a two-channel test signal. Same opcode, same instant.
The plan for today was a single-variable build that slowed the acquisition
cadence, on the theory that the acq task was starving the buffer.

We built it, flashed it, and it didn't reproduce. Not "the mismatch went away" —
*everything* read the same, and everything read wrong. What followed was a day
of the kind this devlog exists for: almost every number we started with turned
out to be a measurement of something other than what we thought.

## Step one: prove the wire

The first thing we did right was slow down. David proposed doing this in
deliberate steps with a pass criterion for each — prove the signal reaches the
input, then prove the channels are independent, then look at raw data, and only
then worry about the screen. Step one already failed: a full-scale DC step on the
ESP32 test generator (300 mV → 3 V) moved the captured mean by 0.2 codes.

Meanwhile both channels showed the *same* ~137-sample-period sine, span ~125,
that ignored every generator setting — DC, square, sine, any frequency. We
called it pickup and asked David to check the ground.

Then, with the generator's channel 2 switched off, the "pickup" vanished. It had
been the ESP32's channel-2 output the entire time — an 8 Hz sine left running
from the previous session — arriving on **both** capture opcodes, while
channel 1's output arrived on neither. Yesterday's "two distinct channels" claim
(57 of 64 samples differ by more than 10 codes) would pass for two copies of the
same waveform with a small phase and gain difference, and nobody had checked the
period of what was captured. So: retracted. There was one signal, on both
opcodes.

Two soldering rounds later (a jumper was genuinely dead, and the ESP32's UART
briefly died to a stray solder joint), the picture held: **whichever DAC is on
the CH1 lead shows on op 0x04 *and* op 0x05; the CH2 lead shows on neither.**
More on that below — it's now the real open problem.

## What the front end actually is

Since we finally had a trustworthy single signal, we characterised the path.

- **The default read regime is AC-coupled**, and cleanly so: a 1 → 128 Hz sweep
  at fixed amplitude gives 15/25/40/61/82/90/92/92 codes — a textbook first-order
  high-pass with its −3 dB point near **9 Hz**. This is why the DC step did
  nothing, and why a 1 Hz square captured as spikes at the edges. Slow demo
  signals (the 5 Hz sine we'd been using) were being attenuated 3–4×.
- **PC12 is the input connect relay, not AC/DC coupling.** LOW kills the signal
  outright, at any frequency. That reinterprets the 08-12 battery test — LOW =
  "battery does nothing" was a *disconnected* input, not an AC-coupled one.
  We swept every other candidate pin we could name (PE4/5/6, PA15, PD2, PD3,
  PD6, PD13, PC1) and none of them switches the coupling. Stock has an AC/DC
  option, so the relay exists on a pin we haven't identified.
- **PE4/PE5/PE6 are the CH1 gain ladder**, not routing: at 32 Hz and 400 mVpp
  the eight combinations give off / off / 174 / 218 / 90 / rail / 46 / rail codes.
- **The 08-14 sample-rate number was wrong.** In this regime a 32 Hz sine is
  33.7 samples per period and an 8 Hz sine 137: **≈1.07 kS/s**, buffer ≈ 0.96 s.
  Yesterday's "~2.7 kS/s" was this same 8 Hz sine misread as a 20 Hz square.
  (Yesterday's other reading — "13 cycles of a 30 Hz sine" — implies ~2.4 kS/s,
  and today's 200 Hz and 1 kHz tones both produced ~190 crossings per buffer,
  which no fixed sample rate explains. The default regime is not a clean
  sampling mode. See open questions.)

## The timebase

Then the day turned. Sweeping the post-config SPI writes for anything that would
bring the CH2 lead onto op 0x05, register `0x01` — stock arms it with `08` —
changed the *period* of the captured sine without changing its amplitude. In
triggered mode (trigger level `0x08` set to mid-scale so frames come back
whole), the low nibble is a **1-2-5 sample-rate ladder**:

| reg 01 | measured rate |
|---|---|
| `0F` | ≈ 30 kS/s |
| `0E` | ≈ 60 kS/s |
| `0D` | ≈ 150 kS/s |
| `0C` | ≈ 300 kS/s |
| `0B` | ≈ 500–600 kS/s |
| `0A` | ≈ 1 MS/s |
| bit 4 (`10`, `1F`) | ≈ 15 kS/s |

Above ~300 kS/s the numbers are indicative only — the ESP32's 40 kSa/s software
DDS stops being a usable reference; its hardware PWM output would be, once wired.
But the shape is unmistakable, and it is the shape of stock's timebase table.
Stock's "23× faster than us" from yesterday's capture is almost certainly stock
running at code `0F` — 30 kS/s against our 1.07.

Yesterday we chased this into the FPGA netlist and found a programmable-rate
counter on a buffer pair we'd never read, and concluded the divisor was "a bench
question." It was: the register was one of the five arm writes we've been
sending since June, with the wrong value in it. The netlist finding was real —
it just described *what* the register drives, not *where* it is.

We recorded the strange one honestly rather than fitting it: the default `08` is
a regime of its own — the ~1.07 kS/s "free-run" behaviour with tone-independent
crossing counts, and garbage, torn frames when triggered. `0F` triggered gives
clean whole frames every time. That's the setting the display work should use.

## The "bug" that wasn't

With a clean 30 kS/s regime and a 1 kHz tone, the acquisition task's "first four
bytes" read `95 A1 A5 A8` — a rising sine at ~4 codes per sample, exactly right.
Yesterday's flat `71 71 71 70` was the first four samples of a **5 Hz sine at
1.07 kS/s: 214 samples per period.** Flat by construction. The direct read
reported min/max over the whole 1023-sample buffer. Two different statistics
of the same data, and we built a firmware variant to explain the difference.
The acq gate ("accept if any sample differs from sample 0") is fine.

## The display

So we put a display-friendly tone on (100 Hz, ~3.4 cycles per 30 kS/s buffer)
and asked David what the LCD showed: **a sine wave, two traces on top of each
other, a bit of quantization noise, and a phase jump once per trace.**

That's the first real signal rendered by open firmware from a cold boot, and
every part of the description is explained. Two identical traces because op
0x05 carries CH1's node. Noise because both ends are 8-bit. And the jump: the
capture memory is circular and the engine keeps rolling — three consecutive
frames share only ~20% of samples — while every read starts at address 0, so the
oldest→newest seam lands at a wandering index in every frame. Not a display bug;
a missing one-shot re-arm. (The read's third header byte varies 80–169 and
looks like it should encode the write pointer; my dumps were torn by the acq
task updating underneath them, so that correlation is untested.)

## Two more measurement traps, for the record

- A direct SPI read at /256 takes ~35 ms; at 30 kS/s the buffer refills in
  34 ms, so direct reads at that rate come back **lapped** — several ~85-code
  jumps per frame. Use the acquisition task's /2 read (`spi3 read 1024`) or a
  ≤15 kS/s code for direct-read measurements.
- Railing the ADC to `FF` **crosses the free-run trigger level `0xFF`** and the
  frame comes back stitched — the "positive edge holds at 255 for exactly 178
  samples then snaps back" square-wave capture was that, not analog behaviour.
  Keep test signals off the rails.

## Community

Three threads answered today. maksidze repaired his unit (bad solder joint) and
posted logic-analyzer captures of our four guest builds; the hardware-SPI one is
a clean **second-unit confirmation** of the wall (`00039020` before, during and
after the upload — Exp N bit-for-bit), the bit-bang ones were aliased at
24 MS/s. He also identified **PB9 as the onboard buzzer (TMR4_CH4 PWM)** — which
retires it from the config-candidate list and got our "shared analog enable"
drive of it removed — and published a **W25Q128 USB dump/restore GUEST firmware**,
which is both the flash recovery path this project lacked and the instrument
the factory-cal question needs (a dump from a unit we haven't reflashed). Komzpa
opened a draft PR reconstructing editable RTL for the FPGA. We added
`guest-coldtrace-la`, a bit-bang build slowed to ~1–2 MHz SCK for 24 MS/s
analyzers, and a Community Tools section to the docs index.

## Open questions

Numbered so the next entry can strike them.

1. **Why does op 0x05 carry CH1?** Two ADCs on one analog node (constant ~3-code
   offset), through all 16 states of the CH2 relay bank (PA15/PA10/PB10/PB11)
   and every value tried on regs 01/02/06/07. Single-channel interleave mode
   selected by something we don't send (a USART scope command — the silent-scope
   build sends none)? Or is the CH2 jack path dead on this unit? Cheapest
   discriminator: a stock boot with the CH2 lead driven.
2. **Where is the AC/DC coupling relay?** Not PC12/PE4-6/PA15/PD2/PD3/PD6/PD13/PC1.
   Stock's coupling-menu handler in the decompile should name the pin.
3. **What is the default `08` regime?** Apparent rates of 1.07 k, ~2.4 k, and
   tone-independent crossing counts, plus torn triggered frames. Decimated roll
   buffer? Peak-detect? Worth one careful session with the hardware PWM as the
   reference — but `0F` triggered is what to build on meanwhile.
4. **One-shot capture / the seam.** Does the engine stop after a trigger if we
   let it (pulse the run pin per frame instead of holding it)? And does the
   third header byte encode the write pointer? Both are one clean test each.
5. **Rates above ~300 kS/s** need a crystal-timed reference (ESP32 GPIO27 PWM).
6. **Factory cal / cross-unit diff** — now answerable with maksidze's dump tool.
7. Fold `01 0F` + a mid-scale trigger into the default cold-boot arm sequence,
   with the timebase menu driving reg 01.

## Where it stands

Cold boot → live, real waveform on the LCD, at a known 30 kS/s, with the
timebase register in hand. Two traces where there should be one, and a seam.
The measurements underneath are, for the first time this week, anchored to a
signal we proved was there.
