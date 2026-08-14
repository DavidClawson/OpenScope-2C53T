# The wedge that never was — and the register we found instead

**2026-08-14**

## Background

Yesterday ended with a cold boot into a live scope and a short list of polish
items. Today was supposed to be one of them: sweep the SPI read-opcode space
looking for the readout command of a mystery buffer pair in the FPGA netlist.

We did run that sweep, and it worked. But the day turned into something better:
by the end of it we had killed a false failure mode that has been distorting
our bench results for two days, identified the FPGA's **trigger-level
register**, measured the default sample rate, and captured the first **properly
triggered** sweeps of a real signal on open firmware.

Also worth recording: this was the first session run almost entirely without
hands on the device. The debug shell over USB CDC on one port, a
serial-controllable ESP32 signal generator on another, and every experiment
below — including the ones that "broke" the device and the ones that un-broke
it — was driven remotely. The only human interventions were flashing and power
cycles.

## The morning: everything is broken

Fresh build, cold boot, and the scope was dead. The acquisition counter froze
at exactly 20 frames and the timeout counter climbed forever — the same
signature as a cadence experiment the day before, which we had written up with
some confidence as: *reading the FPGA too fast desynchronises it, and the only
recovery is a full power cycle. A reset does not fix it.*

We power-cycled. It froze at 20 again. We rebooted with everything quiet. Same.
Cold-boot configuration itself was fine — DONE_FINAL set every time — but the
capture engine appeared to arm and then die within 200 milliseconds, boot after
boot after boot.

If you have read the earlier entries you know where this is going: when this
project sees a deterministic failure with a suspiciously round signature, the
instrument is usually lying.

## The unmasking

The clue was environmental. Yesterday's successful cold boots were made with a
**floating probe**. Today's failing ones had the ESP32 wired into CH1, playing
a sine wave.

Unplug the signal: the engine arms at cold boot and streams ~137 frames a
second, indefinitely. Plug a signal in: frozen counter, garbage frames.
Remove it again — **and it recovers instantly. Same boot. No reset, no power
cycle, nothing.**

So there is no wedge. There never was a wedge. What we had been calling a
latched hardware failure requiring a power cycle is a **live, reversible
readout regime**: our read protocol worked only while the input signal never
crossed the trigger threshold, and fell apart the moment real triggering
began. Every "recovery" we ever performed had worked or failed according to
what the input signal happened to be doing at the time — the power cycles were
theater.

Three claims from our own notes — some written *that morning* — did not
survive the day, and have been corrected in place:

- "the only recovery is an FPGA power cycle" — refuted;
- "a 30 ms read cadence wedges the engine, 150 ms is safe" — confounded; the
  variable was the signal, not the cadence;
- "cold-boot arm fails" — never happened; both "failing" boots simply had a
  signal attached.

## What the stock firmware actually does

With the false model cleared, the question became: how does stock's runtime
read the engine without hitting this? We went back to the June logic-analyzer
capture of a stock boot ([#18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18),
maksidze's — that capture keeps on giving) and re-read all 348 runtime
acquisition windows at the byte level.

Findings:

- **Stock sends nothing between reads.** No re-arm command, no acknowledgment,
  no USART traffic. The paced read pair *is* the protocol.
- **Stock is event-paced, not timer-paced.** It reads CH1 and CH2 back-to-back
  (~0.2 ms apart), then waits for a fresh data-ready event — median 18 ms,
  never re-reading on a level. The "~29 ms cadence" in our old notes was a
  simplification.
- In 348 windows, stock never once read a partial buffer.
- The third header byte of a CH1 read is a **buffer-valid flag** (`01` on
  143 of 174 windows) that our firmware had been discarding unread.

So we rebuilt our acquisition loop to match: an interrupt-driven edge counter
on the data-ready pin (a polled level read cannot see a short pulse — the
same class of instrument blindness as the /2 clock reads and the floating
MISO, and we keep having to relearn it), reads paced one pair per fresh
ready event, headers captured, and the valid flag honoured.

## The register

The edge counter immediately produced numbers worth having. Captures complete
at trigger rates up to ~2.8 Hz and not at all above ~3 Hz; the completion rate
saturates around 2.7 per second. That means the 1024-sample buffer takes
**~375 ms to fill** — the engine's default free-running sample clock is about
**2.7 kS/s**. (A lovely cross-check: stock's capture contains exactly one
anomalous 416 ms gap, right after its engine arms — the same slow first fill.)

Then the best result of the day. Stock's arm sequence ends with a write of
`AD` to register 8, glossed in our notes for months as "trigger level
(candidate)". If that is a *digital* trigger level — compared against ADC
codes inside the fabric — then our quiet-input noise floor (codes ~52–101)
sits below 0xAD = 173, and "free-run" is simply *never having triggered*.

Test: write the level **into** the noise band and watch a quiet input.

- level `0xAD` (173), quiet input: 0 captures/s
- level `0x37` (55): **4.6 captures/s — the noise floor itself triggers**
- level restored: 0 captures/s

**Register 0x08 is the trigger level, in ADC codes, and the comparator lives
inside the FPGA.** Which also means the DAC we have been calling "the trigger
comparator reference" for months (DAC1 on PA4) is very probably the channel's
**vertical offset** instead — the earlier finding that capture reads flat
until that DAC is restored stays true, but the mechanism reads differently
now: the signal was pushed out of the ADC's window, not deprived of a
comparator.

## Corrections desk, same-day edition

Midway through the afternoon we concluded, in writing, that `FF` bytes in a
frame were *unwritten buffer words*. That conclusion did not survive the
evening: with the amplitude reduced so the signal physically cannot rail the
ADC, the FF bytes vanished entirely and a freshly-cleared buffer turned out to
read `0x00`. **FF was the ADC pinned at full scale** — our 3.3 V test signal
was clipping the range — dressed up by a validity gate that then rejected the
evidence. The findings document now corrects itself in place, next to the
original wrong reasoning, per house rules.

And with the trigger level matched to the signal and the railing avoided:
**full, live, triggered frames on every read.** A scope that triggers is a
scope. The UI's trigger-level control still moves the wrong knob (the offset
DAC) — wiring it to register 0x08 is now a straightforward feature task.

## The sweep, for completeness

The opcode sweep this session was built for also ran clean: the design decodes
only the **low 5 bits** of the read opcode (0x20–0x3F alias 0x00–0x1F
exactly), so the space is fully mapped — channel reads at 0x04/0x05, a
level/status read at 0x03, two small dynamic registers at 0x09/0x0A that look
like windowed min/max, zeros elsewhere. No hidden bulk-buffer opcode in this
regime; the netlist's mystery buffer pair stays open, to be re-hunted under
triggered conditions.

## The open question — and it looks like the timebase

One mystery got sharper all day and is now the most valuable thing anyone
could solve: after its first slow 416 ms fill, **stock's engine runs its
capture cycle at 18 ms — about 23× faster than ours — with byte-identical
SPI traffic and a silent USART.** Something outside the traffic we replay
switches stock's engine into a faster sample clock, and whatever that
something is, it is almost certainly the timebase mechanism this scope still
lacks. Candidates we can name: stock boots into multimeter mode with a
different analog posture (meter mux enabled, different relay bank, the offset
DAC at a meter-derived value), or fabric state we have not identified. If
reading this gives you an idea, [#18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18)
is where this project's best ideas have come from.

## Credits

Every load-bearing measurement today traces back to **maksidze's** June Saleae
capture — five months of value from one afternoon of careful probing, and it
was the re-read of *his* capture that handed us the pacing protocol and the
416 ms clue. His bench FPGA is currently down awaiting a reball; no rush, and
nothing here needed new captures. The bit-bang loader lineage that makes cold
boot work at all remains **Stlkv's** and **maksidze's** (see the 2026-08-12/13
entries). The full findings document, including everything this entry
simplifies, is
[`analysis_v120/trigger_regime_findings_2026-08-14.md`](../../reverse_engineering/analysis_v120/trigger_regime_findings_2026-08-14.md).
