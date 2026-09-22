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

## Postscript, same day: FFT-live on the screen

With the poll loop on the device, the spectrum view got its first look at real data. The
header read `LIVE CH1 pk 1.0kHz` at 0x10 and 0x0F, `pk bin N` on an unmeasured code, and the
record side of the S2 acceptance ran clean: bins 328, 164 and 82 exactly at the three codes,
and the 8 kHz fold at bin 1475 against 1472 predicted — which is the sample rate talking,
not the transform (a fold at 8 kHz moves 0.2 bins per sample per second; 1475 says
12,497 S/s, within 0.06 % of the table).

Two things only a screen could show. The axis labels were there and invisible: grey text
drawn transparent onto the amber bars that fill the bottom of the region. And a tall peak
sat at the far left of the spectrum, above the fundamental — the input stage subtracted a
nominal 128 where this signal sits around 67, so sixty counts of DC went into the transform.
The peak search starts at bin 2, so the header had been right all along while the picture
was wrong. Both fixed, the DC one with a fixture whose negative control reproduces the
spike. That is the third time this month a number was correct and the thing it described
was not.

The second look, on the fixed image, found three more, and they are the same shape. The
header said `pk 4.4kHz` for a peak the estimator put at 4,497 Hz: the formatter truncated
the tenth, so every frequency on the screen was biased down and nothing about the reading
said so. The right-hand axis label read `24.9kH`: the string renderer's overflow check priced
every glyph at the font height, so any label aligned to the screen edge lost its last
character. And the `Fund` tag sat on top of the header. Each got a fixture whose negative
control reproduces the exact screen reading. Five display defects in one day on a feature
whose numbers had all been right, which is the argument for a screen check being a step in
the acceptance rather than a courtesy.
