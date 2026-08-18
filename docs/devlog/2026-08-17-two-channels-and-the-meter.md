# Two channels, and the meter came back — a floating pin and a half-implemented sequence

**2026-08-17**

## Where we left off

CH2 had been dead for a month. Not attenuated, not noisy — absent. Both capture
buffers carried the CH1 signal at full amplitude, CH1's attenuator moved both of
them, CH2's relay bank moved neither, and the CH2 jack at 2 Vpp was
indistinguishable from silence. Every MCU-reachable channel had been swept. The
plan for today was a synthetic diagnostic bitstream, because we had run out of
things our own firmware could distinguish.

We never flashed it. It turned out the question it was built to answer could be
settled with statistics on the build we already had.

## Two converters, and a retraction of a retraction

The two survivors were: the readout serialises one memory twice (so CH2's data
exists but is unreachable), or two converters both watch the CH1 node. Content
can't separate them — both predict the same waveform at a read-time lag, which
is exactly why the diagnostic image existed.

But two *physical* converters have different offset and gain. That is, after all,
why per-device calibration exists on this platform. So: read three windows per
trial, compare the middle against the **midpoint of the two outer ones** (which
cancels linear drift exactly), on a **flat DC input** so no window-phase wobble
can bias the mean.

| condition | mean | t |
|---|---|---|
| DC, `04 05 04` | **−2.645 codes** | **−357** |
| DC, `04 04 04` — identical-opcode control | −0.003 | −0.35 |
| 100 Hz, `05 04 05` — roles swapped | **+3.429** | +54.9 |

Same read order, same timing, same drift. The control is exactly zero and the
effect mirrors when the roles swap. **op04 and op05 read different physical
converters.**

The uncomfortable part: we had measured this on the 16th and *retracted it*,
because the means drifted read-to-read. Drift is precisely what a paired design
cancels. A correct result sat withdrawn in our notes for a day because it was
re-tested sloppily and the sloppy test was trusted more than the original.

## The hardware was fine, and that was bad news

Everything about CH2 rested on "stock shows both channels on this unit" — an
observation from the 15th that was, honestly, someone looking at a screen.

So we flashed stock and fed it a **triangle on one jack and a square on the
other**. Two different shapes, not the anti-phase sine pair used originally:
anti-phase can be mistaken for one source drawn twice if a display inverts or
rescales, two different shapes cannot.

Stock drew a triangle and a square. Then, minutes later on a freshly configured
FPGA, its multimeter read 1.61 V on an AA cell, matching an external DMM.

Then the warm handoff: stock configures the FPGA, and — without cutting power —
our firmware boots on top of that same configuration, deliberately reconfiguring
nothing. Result: one channel, dead meter. **Our configuration was exonerated and
the fault was in our runtime code.**

## The answer was two pins we never drove

A decode of stock's scope-mode entry found `ms[0x14]` — the CH1/CH2/BOTH
selector — driving **GPIO as well as** SPI3 op `0x02`. On the bench:

| mask | PC2 | PC1 | op04 | op05 |
|---|---|---|---|---|
| 3 = BOTH | H | L | CH1 | **CH2** |
| 1 = CH1 | H | H | CH1 | CH1 |
| 2 = CH2 | L | L | CH2 | CH2 |

We left PC1/PC2 **floating**, which lands in mask 1 — CH1 routed into *both*
converters. One fact, every symptom: both buffers carrying CH1, CH1's attenuator
moving both, CH2's bank moving neither, two real converters fed from one source.

We had been sending op `0x02 = 3` all along. The GPIO half is the load-bearing
one, which is why every reg-`0x02` sweep read as negative.

And PC1/PC2 were in our notes as **"swept negative"** — from a sweep that
*pulsed* them on an **unconfigured** part while hunting config entry. A pin
tested for a different question had been filed as ruled out.

## The meter, and the same mistake twice

The meter turned out to be dead because `main.c` drove **PC11** (the meter mux)
LOW unconditionally, commented "(scope)". Driving it high: A/B/A of **276 bytes
/ 0 / 276**. Clean result, and we called it solved.

Then David switched to DMM mode and reported an audible relay click with a
frozen display.

PC11 was load-bearing, but "this pin matters" is not "this pin is the fix."
Stock's mode entry is **pins, then bus, then commands**: USART2 UEN, resume the
dvom tasks, PC11, reseed. A coldtrace build did none of the rest — `fpga_init`
returns early *before* the meter block, and the task-creation branch makes only
the acquisition task. Supplying the bus, the tasks and stock's activation words
gave **1.6 V** on the cell, with PC11 verified high and `rx_bytes` climbing 300
in 4 seconds.

Both faults, one shape: **a per-mode sequence we had partially implemented.**

## Seven ways the instruments lied

Today produced seven measurement defects on our side, most of which returned
confident, plausible, wrong answers first:

- `fpga scope range <n> 0` — the channel argument is 1-based, so `0` silently
  meant *both*, and a relay-click test drove both banks.
- `usart tx` queues into a task coldtrace never creates — those frames were never
  transmitted, which also voids a "USART is excluded" negative from the 16th.
- The re-arm rewrote reg `0x01` from a *tracked* index, so an A/B/A never
  returned to baseline.
- A stale PC12 HIGH from a previous block left a later test measuring a dead path.
- The 250 Hz detector had never been shown able to detect 250 Hz.
- `gpio set` on a floating input does nothing, silently — three times.
- And a build target nobody exercises stopped linking without anyone noticing.

So the tooling changed. There's now a `bench-experiment` skill with **control**
and **blind spots** as required fields; `VOID` is a first-class outcome distinct
from `NEGATIVE`; the debug shell refuses to report what it didn't do; and
`bench.py` makes an uncontrolled negative structurally unrecordable. The ESP32
signal generator — previously gitignored — is published, because nobody could
reproduce any of this without it.

## Where that leaves it

A two-channel oscilloscope and a working multimeter, from a cold boot, on open
firmware. No stock, no warm handoff, no case-crack.

Still broken and worth saying plainly: CH2's range table disconnects its input
above range 4, only 3 of 8 mux taps connect, and it clips above ~300 mV.
Absolute vertical calibration is still a placeholder. Only DCV was tested on the
meter. And `echo_frames` sits at zero while data frames arrive, which we don't
understand yet.
