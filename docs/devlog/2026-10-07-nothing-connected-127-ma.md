# Nothing connected, 127 mA

*2026-10-06 → 07, bench unit #1. The first live validation run, the softkey UI (P0 + P1),
and the fuse tester rebuilt.*

Two days of user-interface work, which usually means two days without a story. This time
the story came from putting the screen in front of a person instead of a script. Every
finding below came from David holding the unit, not from a test.

## The buttons said nothing

The validation program (`docs/validation/`) was supposed to be a checklist: each screen,
each button, expected vs. seen. The first live pass on 2026-10-06 turned it into a design
review within an hour (U01–U04 in `findings.md`). A channel couldn't be switched off.
A debug counter was on the release screen. CH2's trace ran under the measurement text.
TRIGGER, in the time view, silently cycled *cursors*: it drew an orange line that looked
like a trigger mode and made the stats refresh at 2 Hz.

None of this was a bug in the sense of code that disagreed with itself. The buttons did
what their code said. The person holding the device couldn't tell what that was.

The answer is the one bench scopes settled on decades ago: **softkeys**. MOVE, SELECT,
TRIGGER and PRM sit directly under the screen, so the bottom of the screen now labels each
one with what it does *here* and its current value. One rule makes the bar honest: no label
without a press handler, and a host test enforces it (`softkey_bar_valid()`). The spec is
`docs/specs/platform/softkey-ui.md`.

P0 (the bar, popups in every mode, flicker-free text) and P1 (the meter) were merged on
the 6th:
- **Big view.** The reading is in 70 px digits, readable across the bench.
- **Limits view.** Pass/fail against a Low/High band.
- **Hold and Relative.** OK is an instant Hold, and Relative finally has a key.
- **Faster function changes.** A change now takes 0.21 s, where it took 1.4–4.5 s.
- **Regression.** The release image passed it 3/3 on the bench.

## The fuse tester could never have worked

The fuse view is the automotive feature: probe the two metal tips on top of a fuse that is
still in the box, and the drop across it, divided by the fuse's typical resistance, gives
the current. It is how you find a parasitic drain without pulling fuses.

Redrawing it from mockups meant reading its arithmetic for the first time in a while:

```c
float current = (voltage_mv * 1000.0f) / (float)r_uohm;   /* "mA" */
```

mV × 10³ / µΩ is **amps**. The view printed amps and labelled them mA, so a 48 mA draw
on a 10 A fuse read "0 mA". A second slip passed a reading in volts straight through as
millivolts, which made it a thousand times worse again. The integer helper in `fuse_table.h`
had the same mistake independently. As written, the feature could not have found a drain
in any car (F46).

The logic now lives in `fuse_model.c`, away from the LCD, and `make test-fuse` checks it
against a worked example: 0.379 mV across a 10 A ATO (7.9 mΩ) is 48 mA. The old formula
is kept in the test as a **negative control** that must fail the same example. A fix whose
test would also have passed the bug isn't tested.

The first build on the bench then showed the voltage drop as a bare "mV". The firmware
links newlib-nano, whose `printf` has no `%f`, so every float format printed nothing.
Every number in the view is now formatted by hand in fixed point.

The rebuilt view has four pages (Detail, Table, Scan, and a "which fuse is this?" Types page
drawing the five fuse families to scale). It draws in place: across a run of reading
updates it did zero screen clears, at about 6 ms per draw.

## Nothing connected, 127 mA

With the new view on the screen and the probes lying on the bench, David reported
**DRAW**, jittering now and then to NO DRAW. Nothing was connected.

The obvious explanation was stray pickup on an open, high-impedance input. Forty readings
over USB seemed to agree: −2.8 to +5.1 mV, wandering. That is noise, and on a 7.9 mΩ fuse
every 0.1 mV is 13 mA, so the view turned noise into a draw.

The decisive measurement was the other one: **leads shorted**. If the meter is honest, two
probe tips touching read zero. They read **−0.9 mV, 40 readings out of 40, ±0.1**. That is
not pickup. It is an offset in the meter's own input, and a few minutes later the open leads,
left alone, read just as steadily: −1.0 to −1.1 mV. The wandering had been the leads
being handled. Open or shorted, the meter says about −1 mV, which on a 10 A fuse is a
phantom 114–127 mA.

David's suggestion was the right shape for the fix: a **"Cal leads"** key, now on MOVE in
the fuse view. Touch the tips together and press it; the settled mean of the last eight
meter frames becomes the lead offset and comes off every reading. It refuses a reading that
hasn't settled, and anything over 5 mV, which is a live fuse rather than two tips touching.
On the bench it stored −1.00 mV, after which the drop read −0.1 mV and the verdict was a
green "under 50 mA".

The views also stopped giving a verdict until the last eight frames agree (within 0.5 mV
plus 20 % of the reading). That catches leads being handled. It does **not** catch open
leads left still, which are as steady as a short; the calibration is what fixes those. The
steadiness check is written up as what it is, not as an open-lead detector.

## The check that couldn't see its own readings

The first Cal leads build refused a rock-steady short with "Hold the tips together." The
counters explained it. The settling check sampled the reading each time the view redrew,
and the view redraws only when the *displayed* value changes. A steady −0.9 mV changes
nothing, so it redrew about once a second: 361 draws against 11,222 meter frames. The
window never filled, so the check called the steadiest reading of the day unsteady.

The meter already keeps its last eight frames, so the check now reads that history
directly (under the reading's seqlock, which the history writes had been sitting just
outside of). The first attempt parsed the numbers with `strtof`, which added 16 KB to the
image; a twelve-line parser does the same job in 464 bytes.

## What we'd like from anyone with a 2C53T

This is where other units help most, and it doesn't need any bench gear:

- **Is the offset per-unit?** Put the leads together in DC V (meter → View → Fuse, then read
  "Voltage drop"). Unit #1 reads −0.9 to −1.0 mV. Does yours? If every unit reads about the
  same, it may be something we can fix at the source, not with a calibration key.
- **Real fuses.** Probe a fuse in a car with the engine off, after the modules have gone to
  sleep. Does the current look plausible? What does Scan say as you walk the box?
- **Colours.** The USB screenshots quantise to 16 colours, so the fuse colours have only been
  checked by eye on one unit. Do they look like the fuses in your car?

## Still open

- **F48:** the DC V range resolves 0.1 mV, which is 13 mA steps on a 10 A fuse. That's enough
  to find a draw, but coarse for measuring a small one.
- The lead calibration lives in RAM and is lost at power-off.
- The release image doesn't honour "Startup: Meter" (a known `button_manual` finding): unit #1
  boots into the scope.
- P2: the scope's softkeys, measurement strip and trigger menu.
