# The buffer was always full — we were just reading it too slowly

**2026-08-14** (later the same day as [the wedge that never was](2026-08-14-the-wedge-that-never-was.md))

## Where we left off

The morning ended with a mystery worth chasing: stock's scope reads a fresh
capture buffer every 18 ms — about 23× faster than ours — over byte-identical
SPI traffic. Something outside the wire switches stock's engine into a faster
regime, and it looked like the timebase mechanism this scope still lacks. This
entry is where that thread went, plus two fixes that fell out of it.

## Chasing the 23× into the fabric

First we tried to reproduce it on the bench. A "faithful" build made our
cold-boot config sequence byte-exact to stock's captured wire protocol — no
mid-config port reads, the ERASE prelude, the exact arm timing, everything. Cold
boot, quiet input. The engine came up in the *same* regime as before, not
stock's. So it wasn't the bytes, the framing, the order, or the analog posture
(we A/B'd the meter relay bank too). Everything we could replay was excluded.

Which meant the answer was inside the FPGA, so we went into the netlist. This
project has a sister repo that unpacks the stock Gowin bitstream into a fabric
netlist, and it had already located the only slow, gated storage in the design:
a second pair of block RAMs, written on a *gated* clock rather than the raw
sample clock, storing something computed from both channels — the shape of a
decimated roll buffer.

Tracing the gate's driver cone turned up the thing worth finding: it isn't a
plain enable, it's a **counter** — a self-feeding accumulator, gated by the
buffer's own address phase, and reachable from the SPI receiver. A counter
feeding a clock gate, with a load path from the config writes, is the structural
definition of a programmable sample-rate divider. It sits on a buffer pair our
firmware has never read. That's the strongest candidate yet for both the
timebase and stock's 23×.

We couldn't *run* it in simulation. The SPI receiver's reset and clock nets ride
the chip's long-wire network, and that network is undriven in the unpacked
netlist, so the flops that would advance the divider can't be clocked in the sim.

Our first theory for *why* it was undriven turned out to be wrong, and the way it
fell is a fair example of how this project works. We'd guessed the open-source
chip database was simply missing the long-wire segment model for this particular
Gowin part — a gap we could derive and fill. So we derived it. And then the check
killed the theory cleanly: those same nets are undriven on the *other* Gowin
parts too, including ones whose toolchain works end to end, so an undriven
long-wire at unpack time is just how the tool represents that layer — not a
part-specific hole. The segment model we'd built was real and correct, and it
wasn't the lever; the unpacker never consults it. The actual fix is a simulation
harness that force-drives those long-wire nets itself, the same trick we'd
already used for some missing RAM connections. So the divider's exact divisor
stays a bench question for now, the sim has a concrete (and correctly identified)
next target, and we spent an afternoon proving a plausible shortcut wasn't one —
which is cheaper than believing it.

## The realization that changed the framing

Then a control measurement reframed the whole thing. We'd been assuming our
engine "only updates on a trigger." So we quieted the input completely — no
signal, nothing crossing any threshold — and read the raw buffer anyway.

It changed on every read.

The capture buffer free-runs at the sample clock **regardless of triggering**.
Feed it a real signal below the trigger level and a direct read returns a clean,
coherent waveform — thirteen tidy cycles of a 30 Hz sine across the 1024-sample
buffer, with the trigger never firing once. The buffer was always full of fresh,
real data. Our acquisition loop just wasn't reading it: it waited for a
trigger-completion edge, and a quiet or below-threshold input never produces one,
so the display sat still on top of a buffer that was quietly refreshing the
whole time.

So the slow "2.7 per second" number we'd measured was never the sample rate — it
was the *trigger-completion* rate. The sample clock is fast. The display being
frozen on a quiet input was a **read-pacing choice**, not a hardware limit.

That's a fix, not a mystery. The acquisition task now branches on the trigger
mode: in AUTO it free-run polls the buffer at ~30 Hz (using a triggered capture
when one is available, so a stable signal still locks), and in NORMAL/SINGLE it
waits for a real edge and holds the last trace otherwise — which is exactly how
an analog scope's auto vs normal modes behave. On the bench the display went from
effectively frozen on a quiet input to a steady ~21 reads a second.

(We also caught a self-inflicted bug in the first cut of that change: it re-armed
the vertical-offset DAC every free-run poll, which would have fought the user's
vertical-position control. Now gated to a genuine dead-bus dry spell. The bench
confirmed it never fires in normal use.)

## While we were in there: the front end is mostly digital

The other visible gap was that traces render tiny and volts/div does nothing, so
we started a per-range gain calibration. It turned into a small reverse-
engineering job with a genuinely surprising answer.

First, our relay table was wrong — a prior hand-guess had the coarse attenuator's
polarity backwards. We pulled the real per-range relay patterns out of the stock
decompile and implemented them, and the bench confirmed it: you can hear the
relays click through the ranges, and the gain now steps exactly where stock's
attenuator boundary is. (One relay had to be left alone — stock uses it as a
channel-2 range relay, but our cold-boot config holds it high as the capture
engine's run signal. Driving it would disarm the scope. So channel 2 loses one
fine-select bit, and the engine keeps running.)

Then the surprise. We measured the actual gain at two ranges: 20 mV/div and
2 V/div. Those settings are 100× apart. The analog gain between them differs by
**2.3×**. The relays are a coarse two-position attenuator; the entire
thousand-to-one volts/div range is done in a *digital* layer downstream, not in
the analog front end. That's a perfectly reasonable design for a cheap scope, but
it's not what the volts/div knob implies, and it means "per-range gain cal" is
really two jobs: the coarse analog step (done, and correct now) and the digital
scaling layer (still to be mapped).

We deliberately **didn't** wire the measurements to volts yet. We have clean gain
for two of ten ranges, and each range turns out to have its own DC operating
point, so an honest calibration needs a per-range centering pass. Showing a
confident wrong voltage is worse than honestly showing ADC counts, which is what
the measurement badges do today. The calibration procedure is proven on two
ranges; running it across all ten is a careful bench hour, not a guess.

## Where it stands

- **The scope shows a live trace on any input now** — the auto/free-run fix is
  the shippable win of the day.
- **The front-end relays are configured correctly** — the coarse attenuator
  works and is bench-verified.
- **The 23× has a concrete home**: a programmable-rate counter on a buffer we've
  never read. To read its divisor in simulation we need a harness that force-
  drives the FPGA's long-wire nets — a known technique, now correctly identified
  as the next step.

Four things we'd assumed were hardware limits turned out to be, in order: a
read-pacing choice we control, a relay table we'd guessed wrong, a gain range
that lives in a different layer than we were looking, and a simulation blocker
we'd blamed on the wrong missing piece. The recurring lesson of this project
keeps being the same one — measure the thing directly before theorizing about why
it can't work — and it keeps being worth relearning.

The netlist counter is the thread that could turn the scope from a level tracker
into a real timebase-controlled instrument. If gate-level FPGA simulation and
Gowin internals are your kind of problem,
[#18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18) is where this
project's hardest questions get answered.
