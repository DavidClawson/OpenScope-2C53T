# The read is the handshake

*2026-09-19 → 22, bench unit #1. Experiments 53 and 54.*

This one started as a margin sweep and ended with the acquisition loop rewritten around a
model that is simpler than any of the five we have held since June.

EXP-50 had committed the "held record" at edge + fill + 30 ms and accepted it at 0x10. The
same margin gave occasional torn records at 0x11 and 0x12, so the first job of the session
was to sweep the margin at the slow codes. The residual did not fall with margin. It **rose**
— 47, 51, 56, 65 at 0x12 for +30, +60, +100, +150 ms — and then dropped to the floor at
+230. No "record not complete yet" story predicts a tear that gets worse as you wait.

A two-segment fit located the tear, and the tear moved at exactly the sample rate: index
628, 704, 804, 928 for those four margins, extrapolating to index 0 at **edge + 189 ms**,
at both slow codes. So from about 189 ms after the PC0 edge the buffer is being overwritten
at fs, continuously, from wherever the pointer was. The "record" at fill + 230 — the one
EXP-46, 47 and 50 had built on, and the one a four-day soak had been reading — was a
free-running snapshot. It looked static because the whole loop is phase-locked to the
drive: a snapshot at a fixed delay after the edge has a fixed phase, and a fixed phase
passes any "static body" metric we own.

The frozen buffer, the one available from the edge until edge + 189, turned out to be the
real record, and a level sweep with a triangle told us its geometry. In every record a
crossing of **(level − 28)**, either polarity, sits 504–523 samples before the pointer.
Level 152 triggered on a peak that never reached 152; level 54 never triggered on a floor
of 33. The comparator sees the signal 28 codes above the record. The trigger is at
mid-record. And the FPGA fires on both edges.

Then the arming timestamp, fixed since EXP-52's image had stamped the wrong read, came back
at **6 to 9 microseconds**, fifty cycles running, at two codes. Nothing captures in nine
microseconds. The read was finding a record that was already complete, and PC0 was
strobing as the FPGA handed it over. Every earlier observation lines up behind that:
"one read, one edge" in EXP-41; zero edges at a level above the signal while AUTO kept
reading; two edges in twelve seconds with the arming read pushed to five seconds; and
NORMAL freezing on a slow square, because its single read landed before the crossing and
nothing ever read again.

So the FPGA is a free-running triggered capture engine with a handshake. Record complete;
held until a read; strobe on the read; 189 ms hold; roll; next trigger. Stock's 29 ms
cadence, which EXP-48/49 read as polling a held buffer, is polling for the handover. The
loop is now that poll, and on the first run through the acceptance script every EXP-47
line held, the slow squares triggered in NORMAL, and every committed record sat at the
floor with a strobe-to-commit latency of one millisecond.

Retired in one go: the arming read, the two-phase hold read, the latency-derived
un-rotation (which never ran), and the "invalid first 32–64 samples" — the head was the
seam, the seam is the FPGA's pointer, and no MCU timestamp can recover it. What is left
of the seam is real and bounded: whole-record consumers still see one, where the FPGA's
pointer happened to be. The display's soft trigger already steps over it.

The instrument lesson is the same one this project keeps relearning, now with a new
shape: a metric that cannot see the seam will accept a phase-locked free-run as a
triggered record forever. What broke it was not a better hypothesis. It was a tear
locator, a waveform that encodes time, and a timestamp that could not possibly be a
latency.
