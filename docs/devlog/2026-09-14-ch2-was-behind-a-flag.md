# CH2 was behind a flag

*2026-09-14, bench unit #1.*

Both halves of CH2 were solved in August. The channel mask on PC1/PC2 came out
of the per-mode GPIO posture work on the 17th; the offset reference — a TMR13
PWM on PA6 that stock programs and we never had — was confirmed the same day
and, on the 21st, gave the first independent CH2 tone and a Lissajous. Then
every September note on a plain `guest-coldtrace` build said "CH2 reads all
zeros", and the dev plan carried CH2 as an open bench item with a
positive-control protocol attached.

The arm was behind `FPGA_CH2_TRIGGER`, default 0, set only by a build target
nobody flashed once the experiment that needed it was done. The zeros were
not a mystery; they were the control condition.

EXP-38 is the A/B. Flip the default, build the image and its `noch2` control,
flash each, and on the first boot read TMR13 and PA6 with `mem read` — never
`trig2`, whose handler initialises the timer before it looks at its arguments —
then pull five records per channel with the JDS6600 driving 1 kHz into CH1 and
2 kHz into CH2. ON: TMR13 running at 2544, PA6 alternate-function, op04 at bin
81 and op05 at bin 163, five of five. OFF: timer dead, PA6 floating, op05 all
zeros. Same session, same drive, same range, one variable.

The control phase ran first on whatever was already on the device. DAC1 moved
CH1's mean 6.6 → 85 → 207, and `trig2 raw 2544` woke CH2 at the right bin. So
a zeros result from either boot would have meant something.

The small find is the one worth keeping. I predicted the OFF image would show
PA6 as a GPIO output, because `fpga_scope_frontend_enables()` has always set
it HIGH as an "analog enable". It read back as a floating input. Nothing ever
configured the pin, and on this family ODR is ignored in that mode, so the
write was a no-op for the whole life of the build — the fifth control this
year that looked right because it did nothing. The readback disagreeing with
my prediction is the only reason it surfaced; had I written the prediction as
"don't care", it would still be there.

CH2 moves to S2. Next for it: the attenuator ladder again, armed this time —
the "one usable CH2 tap" result of August was taken with the channel railed.
