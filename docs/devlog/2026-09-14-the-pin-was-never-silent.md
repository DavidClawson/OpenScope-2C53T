# The pin was never silent

*2026-09-14, bench unit #1, the second half of a long day.*

Yesterday's EXP-30 said PC0 had never fired on this unit — zero edges in
1,315 reads with the interrupt armed — and that every oscilloscope record
this project has produced was therefore a free-run read of a buffer being
written. The fact was right. The cause was not the pin.

Three refutations first. The in-band `0x80` byte that EXP-30 nominated as the
replacement data-ready source is `0x80` on every CH1 read and `0x00` on every
CH2 read, at 82 ms and 2-second capture periods, with and without re-arm: a
bank bit, not a flag (EXP-39). Re-arming and then waiting a full capture
period before the next read leaves the seam exactly where it was, with the
controls holding and the generation counter proving the wait was applied
(EXP-40). And stock's op-01 "gate" turns out to be a per-timebase *time* table
in flash, not a status poll — which is why a settle keyed to the wrong event
waits for nothing.

Then the thing that should have been checked in August. The boot arm sequence,
copied faithfully from stock's wire capture, ends with `08 AD`: trigger level
173 on an 8-bit scale. Every bench drive since has peaked around 160. I wrote
`0x80` into the register and the edge counter started moving within two
seconds; wrote `0xAD` back and it stopped. Then, with the acquisition task
parked, one action at a time: idle produces no edge, the reg-01 write produces
no edge, and a single read produces exactly one. **A read starts a capture, a
level crossing completes it, and completion pulses PC0** (EXP-41). EXP-30's
gate deadlocked both times because it waited for an edge before the first
read, and no read means no capture.

The fix that follows is small: the AUTO path's 25 ms edge-wait is shorter than
the capture at every timebase slower than 0x0E, so its fallback read always
lands mid-capture. Raise the budget past the capture cycle and the body of
every record is static — 0 of 10 gross seams against 4 of 20 free-run, same
floor (EXP-42). Edges per read saturate at one per 04/05 pair by 300 ms, so
the capture cycle at 0x10 is ~200–300 ms, not the 82 ms a 1024-sample buffer
suggests; the shape of that is the next question.

Two things to keep from the day. The seam metric this project has used since
EXP-22 is bimodal — two identical arms gave medians of 3.9 and 21.0 — and the
A/B/A would have been quoted as a win on medians alone. The block-residual
metric and the *location* column carry the conclusion. And the first 32–64
samples of every readout are invalid in every mode, zeros or a dropout, not a
rotation: a new bounded defect, hiding under the same guard that hid the seam.

Sixth inert control of the year: a register written correctly from a capture,
then never read against the signal it gates.

## Postscript, the same night: NORMAL and SINGLE

With the level inside the signal the two modes EXP-30 had written off as
impossible were supposed to work as coded. They did not, three times, and
each freeze was the same lesson from a different side.

NORMAL froze at level 0 with zero edges and zero reads (EXP-43): it waited
for an edge before its first read, and a read is what starts a capture.
Priming with one read got exactly one record, then froze again (EXP-44): a
shell read seconds later always produced the next edge, the task's own pair
issued the instant PC0 fell never did. Not the SPI clock, not the gap between
the 04 and 05 reads (EXP-45). It was time since the edge, and it scales with
the timebase: the FPGA refuses to arm a new capture until **one 1024-sample
fill plus ~200 ms** after the trigger, bracketed to 180–209 ms across five
rates with the predictions written before the runs (EXP-46). Then, on the
image that derives that delay from the timebase, everything passed except
one line: NORMAL did not come back after a level change, because the reg-08
write drops the capture in flight and the task still thought it had one
(EXP-47).

The state now, fresh boot, nothing typed: NORMAL captures at four timebases
with one edge per record, holds above the signal, resumes below; SINGLE is
one record per press; AUTO's body is static. Wishlist Tier 1 item 1, at S1.
The bill is ~3.5 records per second at 0x10, and stock's capture shows it
reading every 29 ms. How it does that is the next question.
