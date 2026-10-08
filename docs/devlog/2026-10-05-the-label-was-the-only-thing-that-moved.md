# The label was the only thing that moved

*2026-10-03 → 05, bench unit #1. Experiments 69, 70 and 71, and PR #45.*

Three days that started as feature work and turned, twice, into an audit. The common thread
is a reading that agrees with itself. A label that matches the setting, a register that
reads back what was written, a test fixture that matches its own expected value: each one
looked like evidence, and none of them was.

## Masks, and the record that wasn't from now

The feature was waveform pass/fail: teach a mask from eight good captures, then judge every
later capture PASS, FAIL, or SKIP-with-a-reason (EXP-69). The design leaned on everything
September built. Records are time-ordered with the trigger at index 512, so a mask can be
aligned on the trigger instead of on screen columns. Tolerances are in ADC counts, so a
calibration change can't silently move a mask. And anything that can't be compared (taken
at another range, untriggered, un-rotated) is skipped and counted, never scored.

The first host run against real captures found a design flaw before any hardware did. Eight
samples of horizontal slack let a 20-count glitch and a −30 % amplitude change through,
because slack in time widens the bound on every slope. Two samples is what the trigger
jitter justifies, so two it is.

The first device run found a better one. Stop-on-fail held the failing capture, OK released
it, and the unit immediately failed again and re-froze. The FPGA keeps a finished record
until something reads it, and the hold had stopped the reads. So the first capture after
OK was the one taken *during* the fault, before the button was pressed. It is now skipped
as "stale" rather than scored.

The other surprise was memory. The release image had about 110 bytes of static RAM free,
and the mask's 220-byte bookkeeping struct alone didn't fit. The whole mask is now one heap
block taken on first use.

## Four controls that only moved a label

Looking for a free button for the mask, the same question kept coming up: what does this
button actually write? Three answers in v0.4.0 were "nothing that matters" and one was
worse (EXP-70):

- **Coupling.** CH1/CH2 cycled DC → AC → GND, the popup said "AC", and the info bar said
  "AC". The relays (PD12/PD13) were set to DC at boot and never touched again.
- **Probe 1X/10X** fed one bit of a meter command we deliberately send unobeyed. A 10× probe
  read ten times low.
- **The 20 MHz limit** went to the same ignored bit.
- **SAVE** was gated on a bench flag the release image also sets. It didn't save anything;
  it flipped CH1's input-path relay, direct vs ~30× attenuated, which leaves the volts/div
  wrong until the next range change.

None of these failed a test, because each did exactly what its own code said. Coupling and
probe now go through one writer each, and coupling reads the pin back before the label is
allowed to change. GND left the cycle, because no ground path is known. The 20 MHz limit says
`n/a`. SAVE says it can't save. On the bench, AC now blocks a 1 V step to 0.0 counts and DC
passes it at 50.

The bench run produced its own case of the theme. Scope CH2 moved the opposite way to CH1 for
the same +1 V command. Before blaming the scope, the cables were swapped at the generator,
and the inversion followed the **JDS6600's CH2 offset**. That output ignores its own steps
after an output enable, and moves when CH1 is written, while its offset register reads back
exactly what was set. An instrument that confirms its own setting is not a reference.

## A 220 Ω on a 2.2 kΩ label

A component kit arrived, and with it the first meter session where every part was also
measured on an independent meter (EXP-71): a handheld DMM for resistance, and a FNIRSI
DMC100 for capacitance, diodes and volts. Every reading the decoder handled agreed within
0–1.5 %. The ones it didn't were the interesting part.

The sign was already fixed. Two days earlier, @saulvalenzuela23's logic-analyser captures
from his board had shown the minus sign living in a bit our own April notes on the stock
firmware already documented, and which the decoder had never read. A reversed AA cell now
reads −1.6153 V; before, it would have shown +1.6153.

A 1 kΩ resistor came back as "10103 kΩ". The frame carried a range flag (byte 8, bit 7) that
saul's captures said meant "four decimals". My first attempt to use it broke two older
tests built from real units, which carried the same flag on readings with no extra decade.
So the change came out, and the question was written up as open: manual vs auto range,
perhaps.

The reference meter made it answerable. Unit #1's old frame turned out to carry an explicit
decimal point, which outranks the flag. That left unit #2's "2.2 kΩ → 2168" fixture: flag
set, no point. It would only make sense under the rule if the part were a **220 Ω**, which
differs from a 2.2 kΩ by one colour band. So the prediction was written down first: a 220 Ω
here sends that exact frame shape. It did, `2203` with the flag and no point, while the
reference DMM read 218.2 Ω. The fixture had been a hand decode of a mislabelled part, and it
had stood as a counter-example for two days.

The diode function had its own version of this. Every real diode frame was rejected, because
it carries the same marker as a DC-volts frame, and the decoder's guard against stale
voltage frames threw it out. Unit #2's write-up had called that rejection "the expected
negative", with no diode on the leads. The meter says which function it is in, in a nibble
of byte 6: 8 on every diode frame, 0 on every DC frame. Now the diode reads 0.629 V against
0.635 V, and the stale-frame guard still holds in both directions.

The fixtures from this session are the first in the meter suite with a reference reading
beside them. They assert the decoded value against the *reference* meter, not against our
own output, so if the decoder and the frame agree with each other but not with the part, the
test fails.

## The install that finally finished

@mquerostudio's root cause for the Sept 22 USB-flash hang (#42) was that the installer, running
from RAM after erasing flash, called `memset`, which lives in flash. Whether the compiler
emits that call depends on its version and optimisation level. His fix, #45, forbids it and
adds a checker that reads a release `.bin` and says whether its installer can leave RAM. On
this machine the checker called the v0.4.0 release UNSAFE, at the exact byte he found. It
called today's main "ok", but only because this compiler happened to inline the loop. With
#45, the same build is ok by construction.

Then the hardware half. Unit #1 was flashed with main+#45, and a rebuilt image identical
except for its timestamp was staged over USB and applied. Twenty-two seconds later the unit
answered with the new timestamp, and its own record said `completed (code 170)`. It was
the first USB-staged install ever to finish on this unit.

## Housekeeping we got wrong

- **Two experiment numbers collided** with ones already claimed in mquerostudio's open PRs.
  Mine were renumbered 69/70 before anything was pushed. Numbers now count as claimed when
  a PR opens.
- **The first merge gate in a fresh worktree "passed" with two tests skipped**, because two
  build artifacts existed only in the old checkout. They were built and the gate re-run.
  A skipped test is not a passing one.
- **A 100 kΩ read 89–94 kΩ and drifted.** Fingers across the leads. Laid on the table, it
  read steady.
- **A reply on #37** said the range flag had one meaning, then looked wrong for a day, then
  turned out to be right. Saul was told each time.

v0.4.1 carries all of it.
