# Aiming the trigger

*2026-09-22, evening, bench unit #1. Experiments 55–57, images v9–v15.*

The morning's entry ended with a model of the acquisition engine and a trigger that
worked. By evening it was clear the trigger worked only for someone at a shell. Reg
`0x08` was bench-proven, the boot reconcile wrote it, `fpga scope level` was its one
writer — and `scope_adjust_trigger_level()` had zero callers. No button moved the level,
and the on-screen marker was drawn a fixed number of pixels above centre, a convention
that had nothing to do with an autofit trace. The wishlist's number one complaint had a
working register and no control.

## The two facts the controls needed

EXP-55 asked whether reg `0x02`, which stock writes as `03` at boot, is the edge select.
It is not: `00`, `01`, `02` and `03` all trigger on both edges at the same rate, and `00`
does not even stop triggering. EXP-56 asked whether the 28-code offset between the level
and the record is analog or digital. Range 7 has four times the gain of range 5 and showed
the same 28 at three levels, so it is digital — one constant, the same number as stock's
`FPGA_ADC_OFFSET`. That settled how the marker should be drawn and told us the edge
button had to be a filter in firmware, because the fabric would never do it.

## The controls

MOVE now hands UP/DOWN to the level; each press goes through the one writer, and the
marker is placed at code − 28 through the trace's own autofit, so it sits where the
hardware fires. David walked it up from the buttons: the trace froze at `Trig +70 code
214` against a predicted 207, one step, with the marker pinned at the top of the peaks.

The edge filter classifies each strobed record by the slope at its trigger point and
drops the wrong edge. The design rule was to refuse rather than guess, and the first
prototype broke it: reading the nearest noisy crossing on a slow sine, it was confidently
wrong 15 times in 3,200. The shipped version reads the slope at the smallest scale that
clears noise and gives up when the seam, the level crossing or the slope is unclear. On
the bench, a 2 Hz triangle committed 8/8 rising with Rising, 8/8 falling with Falling, and
both with the filter off. Trigger modes reached S2.

## The seam, and the sample that was never a sample

With edges classified, un-rotating the record was nearly free: find the seam, rotate, and
the trigger lands at index 512. It worked on slow signals at once and only half-worked on
a 201 Hz sine. A linear-prediction seam finder fixed that on 1,800 synthetic records and
changed nothing on the device — v14 failed the criterion written before the run, 4/10
records broken against a ceiling of 1.

The raw records said why. **Sample 0 of a read is often not a sample**: in 13 of 20 it
fits neither neighbour, 20–37 counts off, while samples 1023 → 1 run continuously. Its two
false steps outvoted the seam in both finders, and it was behind most of the edge
classifier's refusals too. The synthetic records had no such sample, which is why the
host tests were perfect. Skipping it: 19/20 real seams exact against a brute-force fit,
the 20th refused, and on v15 the criterion met — 0/10 broken against 5/10 raw, regression
17/17. Why sample 0 is stray is now its own open question; the read framing was already
moved to stock's two-byte discard in August.

The same sample then fooled the test. After un-rotation it sits inside the record, and
the harness re-found the seam by raw maximum jump, so one Falling record read as rising.
A selftest now demonstrates the confusion (raw picks the stray sample at 301, a 3-sample
median picks the seam at 887). That one frame was not saved, so a real misclassification
is recorded as not excluded rather than explained away.

## Two things that went wrong on the way

The firmware centre servo, run for the regression's precondition, reported `DAC1=2047
(median=101)` while the record read 185. It slept a fixed 480 ms per step, and under the
poll loop the buffer only changes on a commit — about once per 475 ms at 0x10 with nothing
to trigger on — so it steered on stale medians, and it never wrote its best value back.
Both fixed; verified from a railed start at two timebases.

And the first attempt at hands-free flashing bricked the app slot. The CDC loader staged
v9 with a matching CRC, `fwapply` printed its banner, and the device froze with interrupts
off and never reset. MENU + Power from off did nothing. **MENU held through a pinhole
reset** reached the factory bootloader, which is now in the DFU guide. The installer's
failure loop reports nothing, so which of its eight exits fired is unknown; it stays
blocked on unit #1 until every exit leaves a mark.

The pattern of the day: three times a check passed on synthetic data and failed on the
bench, and each time the bench was right. The tests got better because the device
disagreed with them.
