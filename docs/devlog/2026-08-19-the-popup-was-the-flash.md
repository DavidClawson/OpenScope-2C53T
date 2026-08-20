# The popup wasn't flickering. It was the flicker.

*2026-08-19*

Tonight's work was supposed to be four numbers. It ended up being two rendering
bugs, and the second one only existed to be found because the first one was
fixed. The pattern is familiar enough by now that it is worth writing down
again rather than filed under "UI polish".

## What was asked

The timebase button had just been made to actually reach the FPGA — until
today it mutated a display variable and nothing else, which is its own story
([EXP-17](../experiments/2026-08-19-17-the-timebase-button-did-nothing.md)).
With that working, the bench report was simple:

> the little modal shows the new timebase, but the whole trace screen flashes
> and the modal flashes with it. It's not easy to read the numbers.

A reasonable reading is "the popup needs to be drawn more carefully". That
reading is wrong, and the code said so:

```c
q.force = scope_popup_active();
```

`force` pins the renderer to the full clear-then-redraw path. So the popup did
not flicker *despite* the redraw — for its entire ~500 ms life it **was** the
redraw. Showing a two-word box blanked the whole screen ten times.

Directly above it, in a comment written in the same commit that added the
flicker-free compositor:

> *the full clear-then-redraw path stays for popups (they overwrite the band
> and need erasing afterward)*

That is a true statement of a real constraint, sitting in a position where it
reads as a decision. The popup does overwrite the band and does need erasing.
It does not follow that the screen must be blanked every frame in the
meantime, and nobody re-examined the inference because it was written down.

## The fix, and why it is a hole

The compositor recomputes every pixel's final colour per column — trace over
trigger over grid over background — which is exactly why it never blanks.
Glyphs are the one layer it cannot recompute that way.

So the popup became a **hole**: each column is emitted as up to two vertical
segments that step around the box, the box is painted once, and when the
countdown ends the hole closes and the very next live frame paints the band
straight back over it. No full repaint at either end.

One trap, avoided by looking rather than by luck: `POPUP_DURATION` is measured
in **draws**, and on the incremental path draws follow capture. A quiet input
would have stretched a 500 ms popup to ten seconds. A live popup now counts as
`new_data` so the compositor runs every 50 ms tick — wall-clock timing, still
zero full repaints.

## The bug underneath

Flashed it, and the bench came back with something better than "fixed":

> I go from some default state with what looks like a yellow band at the
> bottom. When I press right I get a brief period where the new timebase trace
> is shown, then the modal shows, then the modal goes away, and then the trace
> goes back to the bottom?

Two renderers, two different vertical transforms:

| | full path | compositor |
|---|---|---|
| transform | autoscale from the buffer's own min/max | fixed `(v−128)/256` about mid-screen |
| layout | CH1-top / CH2-bottom split when both live | both over the whole band |
| extent | `SCOPE_TOP … SCOPE_BOT` | `SCOPE_TOP … BADGE_ROW2_Y` |
| channel position offset | ignored | subtracted |

Same samples, two places on the glass. The trace moved whenever the renderer
changed. On a near-zero signal the fixed transform pins it to the bottom edge
while the autoscale spreads it across the band, which is exactly the sequence
described: press → full redraw (autoscaled, looks right) → popup → popup
clears → compositor takes over → trace slams to the bottom.

It had been there since the compositor landed on 2026-08-12. It was
structurally unobservable: every path that could have shown the disagreement —
popup, cursors — forced the full path, and everything else was the demo
waveform. **Fixing the flash is what made it visible.**

The fix is not "make the compositor match". Two copies of a transform that are
supposed to agree will not stay agreeing. It is now one `autofit_prep` /
`autofit_y` pair with two callers.

## The one that is still open

Both fixes are bench-confirmed. But the second one surfaces a third problem
that is not a bug in either renderer:

**the vertical graticule does not mean the volts/div the status bar prints.**

The autoscale was added deliberately on 2026-08-14, when CH2 railed, we had no
offset control and no measured gains. Under those conditions it was the right
call, and its own comment says plainly that it is *"a visualisation autoscale,
NOT a volts reference"*.

That was four days ago. Since then the vertical axis has been calibrated on
both channels and the status bar prints a measured volts/div. So the screen
now states a scale that the renderer does not honour — which is the **same
defect as the `vdiv_table` labels deleted the night before**, moved from the
label table into the renderer.

It is not fixed tonight, and the choice is a real one: a true fixed graticule
makes an off-centre or small signal genuinely look that way, which is what a
scope does, and which is worse to look at. Whatever we pick, the screen should
not claim a scale it is not using.

Worth noting the option only exists because the calibration does. A month ago
"draw it to the measured scale" was not on the menu.

## What to take from it

Three things, all of which this project has now learned more than once:

1. **A true comment in the wrong place becomes a decision.** "Popups need
   erasing afterward" was correct and load-bearing, and it stopped anyone
   asking whether the conclusion drawn from it still held.
2. **Fixing an observability bug is how you find the bug underneath.** The
   trace jump was a week old and could not have been seen while the flash
   existed.
3. **Two copies that agree today are a bug with a delay on it.** The
   transforms disagreed in four separate ways, and each one individually looks
   like a detail.

The regression guard is a gate test asserting that a popup over a live trace
produces ten incremental frames and **zero** full repaints. Reinstating the
unconditional `force` fails it.
