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
