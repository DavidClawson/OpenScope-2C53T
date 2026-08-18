# The labels were never measured

**2026-08-18**

## The thing we set out to do

Yesterday's two experiments finished the bench half of the vertical axis. EXP-07
established that PA6's TMR13 PWM is CH2's vertical-offset reference, which meant
CH2 could finally be centred instead of railed. EXP-08 then measured, on both
channels, how many millivolts one ADC count is worth on every frontend range.
CH2 went from one usable range to six.

Today's job was the boring half: take those numbers off the bench and put them
in the firmware, so the instrument shows volts instead of counts. That is what
we did. But the interesting part is what we found while doing it.

## What was already in there

`scope_ui.c` had a function called `scope_cal_volts_per_count()`. It was two
cases wide:

```c
case 2:  return 1.0f / 347.0f;   /* 20mV/div */
case 8:  return 1.0f / 154.0f;   /* 2V/div (default range) */
default: return 0.0f;            /* uncalibrated -> show ADC counts */
```

That code was written carefully. The comment above it says, correctly, that the
`default` case must fall back to honest ADC counts because "a confident wrong
number is worse than a raw count". The fallback logic is right. The design is
right. Both of the numbers are wrong.

Range 2 rails on both channels at every drive amplitude we can produce — it has
no usable span at all, so the claim that one count is 2.882 mV there is not
merely inaccurate, it is a measurement of a pinned input. And range 8 measures
279 mV per count on CH1, not 6.494. That is a factor of 43.

Range 8 is the power-on default. So the number the instrument has been showing
most confidently, for days, on the range it boots into, was off by more than an
order of magnitude — inside a function whose own comment warns against exactly
that.

We don't know where those two constants came from; they predate the two-channel
work and were plausibly taken during a warm-handoff session before the channel
mask was understood, when both buffers were being fed from CH1 through an
un-centred frontend. It doesn't much matter. What matters is that they were
stable, plausible, and unfalsifiable at a glance — the fourth time this project
has been bitten by precisely that shape, after the `/2` status reads, the
floating MISO line, and Exp F's level-only pin diff.

## The labels were worse

Then we looked at where the "20mV/div" and "2V/div" in those comments came from:
`vdiv_table` in `scope_state.c`, ten entries running "5mV" through "5V". It fed
the status bar, the volts/div popup, and the comments above.

Nothing derived it from anything. It is a list of the divisions a scope of this
class *ought* to have.

Our renderer maps 256 ADC counts across a 206-pixel plot and rules a grid line
every 26 pixels, so one division is 32 counts. Multiply that by the measured
gain and you get what a division on our screen actually means:

| range | nominal label | measured CH1 | measured CH2 |
|---|---|---|---|
| 5 | 200mV | **699mV** | 671mV |
| 6 | 500mV | **1.37V** | 1.33V |
| 7 | 1V | **2.83V** | 2.68V |

Between 2.7x and 3.5x out, on the three ranges we trust most. A user reading a
two-division peak-to-peak off the grid on range 7 would have called it 2 V. It
was 5.7.

So the fix isn't only "wire the cal into the badges". A volts/div label that the
instrument cannot support is the same defect as an invented measurement — it is
just wearing the grid instead of a badge. We deleted `vdiv_table` rather than
leaving it unused, because an unused table of plausible numbers is an invitation
to wire it back up, and derived every label from the measured gain instead.

## What landed

A new `scope_cal.c` / `.h`, holding one gain per (channel, range) — per channel,
because CH1 and CH2 have different frontends and measure different gains on the
same range index, which a single shared table structurally cannot express.

Each entry carries a tier:

- **MEASURED** (ranges 5/6/7) — the two channels agree within 6%, the ladder is
  a clean doubling, and the numbers reproduce both an independent mux-code sweep
  on this unit and Stlkv's measurements on a *different* unit with a different
  rig. Three methods, two units, one answer.
- **PROVISIONAL** (ranges 4/8/9) — measured the same way, but the channels
  disagree by 25-48%. That is one fixed amplitude set under-serving both ends of
  the sweep, not the hardware being strange. These still return a number,
  because "roughly 280 mV per count" beats a raw count, but the status bar marks
  them with a leading `~` so the face of the instrument tells you which is which.
- **NONE** (ranges 0-3) — railed on both channels at every amplitude. Returns
  exactly `0.0f`, and callers fall back to counts. What these taps are for is
  still an open question.

The badges, the status bar and the volts/div popup all read from it, so there is
one volts/div string in the system rather than three that can drift apart. And
`fpga scope cal` dumps the whole table over the shell, so the number on the
screen can be checked against the number in the source without a rebuild.

## Then we checked it, in a way the bench had not

The numbers going into that table came from a slope fit: vary the drive, hold
the range fixed, regress. Every row is individually sound. But nothing in that
design compares one range against another — a table can pass a per-range fit on
every row and still be internally inconsistent, and that is the failure a user
hits first, because changing range is the most common thing anyone does with a
scope.

So we ran the complementary test on the running device. Fix the signal, change
the range, and ask whether the instrument reports the same voltage each time.
Control first, as always: with the generator off, span sat at 5 and 3 counts;
with it on, 49 and 51. The drive is what we are measuring.

First pass came out at 9.4% spread on CH1 and 4.9% on CH2 — fine, but with a
defect we caught before publishing it. Peak-to-peak span includes the noise
floor *additively*, so `span x mV_per_count` over-reads by (floor x
mV_per_count), and since mV/count changes 4x across the ladder that bias is
range-dependent. It is precisely the bias the slope method was chosen to avoid,
and reading a single amplitude quietly reintroduced it. The 1.083
"measured/commanded" ratio that first pass produced looked like a source-scale
error and was not one.

Measuring the floor at each range and subtracting it:

| channel | per-range volts (r5 / r6 / r7) | spread |
|---|---|---|
| CH1 | 1987 / 1933 / 1945 mVpp | **2.8%** |
| CH2 | 2012 / 2002 / 1927 mVpp | **4.3%** |

Three different range gains, one signal, agreement to a few percent on both
channels — and a factor of three tighter than the uncorrected pass, which is
itself evidence the floor really is additive. A second estimator that cancels
the floor by differencing two amplitudes agrees with this one inside its
(much larger) quantisation error.

That is now four independent lines of evidence on ranges 5/6/7: the slope fit,
EXP-06's mux-code sweep, Stlkv's numbers from another unit on another rig, and
this. The MEASURED tier is earned.

## The one multiply we owe

Every figure in that table traces back to an amplitude *commanded* from the
ESP32 signal generator. Nobody has ever checked it against a calibrated source.
Its frequency readback is already known to be about 0.82x out, so assuming its
amplitude is exact would be optimistic.

If it is off by a scale factor, every entry is off by that same factor —
uniformly, because they were all taken through the same source on the same
evening. That is a much better failure than a scattered one: it means one
measurement against a trusted instrument recovers the whole table.

So the factor lives in `SCOPE_CAL_SOURCE_SCALE`, applied at lookup rather than
baked into the rows, and a host test asserts that every row is exactly the raw
bench number times that one shared constant. Hand-editing a single row to make
it agree with a reference would fail the build — which is the point, because the
moment one row is nudged, the single-multiply recovery stops working.

A proper USB-controlled signal generator is in the post. When it lands, one
range measured properly rescales the instrument.

## What is still not true

The vertical axis is calibrated. The horizontal axis is not merely unscaled —
it is not yet trustworthy. EXP-08 found that 100, 250, 500 and 1000 Hz all peak
in bin 1 of the spectrum, and that span is identical across timebases
0x08/0x10/0x11/0x12, so the timebase register is not changing what gets
captured. Lag-1 autocorrelation is +0.99, so the record is strongly correlated
rather than noise; its content is just a broad low-frequency hump that doesn't
move when the input does.

Frequency, period in seconds, and any rise-time measurement all sit behind that.
The badges continue to print period in *samples* and a blank for Hz, and now say
so for a reason we can name rather than "no timebase yet".

## The pattern, again

Four times now: a number that was stable, plausible, and produced by an
instrument that could not have measured what it claimed. The `/2` SSPI reads. The
floating MISO. Exp F's output-level diff that could not distinguish "driven LOW"
from "floating". And now two calibration constants and ten volts/div labels.

The countermeasure that keeps working is the same one every time — find a case
where the right answer is known independently, and check the instrument against
it before trusting anything it says. Ranges 5/6/7 are believable today because
three different methods on two different units agree. Ranges 4/8/9 are marked
provisional because nothing yet cross-checks them.
