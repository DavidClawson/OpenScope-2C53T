# A trusted source, and the scope grew an X-Y mode

*2026-08-21 (evening)*

Two things arrived tonight: a **JDS6600** function generator — the first
*crystal-referenced* source this project has ever put on the probe — and, by the
end of the night, a working **X-Y (Lissajous) mode** rendering live phosphor
figures from both channels. The two are connected: the generator is what let us
build and verify the feature in a tight bench loop, and it also settled several
measurements that had been resting on an untrusted source.

## The problem the generator solves

Every absolute number this project has published traces back to the ESP32 bench
source, and the ESP32's DDS loop free-runs at **0.825×** what you command it
(EXP-14). Frequency, sample rate, volts-per-count — all of it was one unverified
multiply away from the truth. A real generator with a crystal is the fix.

**USB control, both ways.** The scope's CDC shell and the JDS6600's register
protocol both came up on the same machine. First contact was read-only — we read
the generator's front-panel state back over USB and it matched exactly. One
thing that looked like a quirk turned out not to be: the amplitude *write* needs
a trailing period (`:w25=5000.`), and without it the device answers `:ok` and
silently ignores you. It's not a quirk — the trailing period is the documented
format; the *frequency* register just happens to tolerate its omission, which is
what fooled us. A quick web check confirmed the protocol is thoroughly
documented (sigrok wiki, a maintained Python lib), so no new repo — just a
`JDS6600` driver added to `bench.py`, with the "read back and raise if the write
didn't take" discipline the module is built around.

**The timebase, confirmed against a crystal.** Command 5 kHz and 10 kHz over
USB; the scope's own estimator reads 4.99 kHz and 9.99 kHz at code `0x0E`. That
puts `0x0E = 49,930 S/s` (EXP-17) within 0.1% of a trusted source — the first
time this project has agreed with an independent instrument on an *absolute*
quantity.

**Vertical scale, three ways, ~0.87.** An amplitude sweep 1→5 Vpp on two ranges
says the ESP32-derived gains over-read by ~10-13% (`SCOPE_CAL_SOURCE_SCALE ≈
0.88 / 0.92`). Then an AA battery — measured on two DMMs at 1.616 V — *appeared*
to prove the DC path rails (mean pinned at 255). It doesn't. That was an
artifact: the generator was still connected to CH1, fighting the battery. Driven
by the generator's own controllable DC offset instead, the DC path reads
perfectly linearly to 2.8 V with no rail, and 1.616 V lands exactly where the
fit predicts. Its DC gain (77.3 mV/ct) matches the AC gain (78.1) to ~1% — a
*third* read on the same ~0.87 scale. Left uncommitted at `1.0f` all the same:
the JDS6600's amplitude is only ~1-2% accurate, so this is strong internal
consistency, not yet metrology-grade.

## CH2 was never broken

With a two-channel generator we could finally answer a months-old question: is
the scope's CH2 (`opread 0x05`) real, or does it just echo CH1? Cold, op05 comes
back a dead all-zero buffer. But arm its vertical-offset reference —
`trig2 raw 2048`, the TMR13/PA6 PWM-DAC the pin table flagged as never
programmed — and it comes alive. Drive CH1 at 1 kHz and CH2 at 2 kHz and the two
reads come back **988 Hz and 1988 Hz**: genuinely independent channels, not an
echo. Both at 1 kHz land on the same FFT bin with a phase spread of 20.8° across
reads — meaning the two buffers come from one synchronised acquisition, so
`(ch1[i], ch2[i])` is a single instant in time. Which is exactly the
make-or-break requirement for a coherent Lissajous.

## Building the X-Y mode

There was already an orphaned `xy_mode.c` in the tree — a renderer nobody had
wired to anything, expecting `int16_t` samples we don't produce. So we built the
view fresh, and the whole thing came together as a bench loop: flash, look at the
figure on the LCD, fix the thing that's wrong, reflash.

- **First light** was a scatter of dots forming a 1:2 "butterfly" — the feature
  worked, but it looked like a dot cloud.
- **Connect the samples** into a polyline (time-adjacent samples are adjacent on
  the curve) — now it's a continuous trace. But it flat-out lied at the edges and
  jumped horizontally across the figure.
- **The jumps were a tear**: reading the two buffers without a coherence guard,
  an acq commit mid-draw drew a chord from the old frame to the new. Snapshot
  both under a stable frame generation → gone.
- **The off-centre, flattened figure** was the signal itself: the AC-coupled
  frontend baseline sits low (mean ~81, not mid-scale 128), pushing the negative
  swing into a compressed region. Fixed two ways — a smoothed **auto-scale** (an
  EMA of centre and span, so the figure holds still instead of breathing on
  per-frame min/max jitter) for the display, and an **auto-centre servo** on the
  offset DACs to park each analog baseline at mid-scale. (The on-device `center`
  command exists, but it drove the DAC to the wrong extreme and *collapsed* the
  amplitude — so the servo replaces it.)
- **Phosphor.** The last ask, and the best part. The flash-every-second was the
  full clear-then-redraw; the fix is to not clear at all. There's already a
  64.4 KB persistence buffer in the shared pool (used by the time-domain scope's
  trace persistence, and X-Y never runs at the same time), so we borrowed it:
  decay the buffer each frame, draw the locus into it, then **stream the square
  row-by-row** from the buffer to the LCD (`lcd_set_window` + `lcd_write_pixels`,
  ~10 ms) with empty pixels rendered as background. No blank phase → no flash,
  *and* the fading trail is the CRT-phosphor look. Points that used to flash as
  artifacts now just fade out. It had to tick at the fast floor (200 ms) instead
  of the 1 s anim cadence, or the trail faded in visible steps — and `xy_row` had
  to move to the display-task stack, because the extra 412 bytes of static
  overflowed RAM by 176.

Then, because it's Friday night: a slow sweep of the generator through a sequence
of ratios — ellipse, figure-8, pretzel, woven knot, triple loop, five-point star
— each with a tiny frequency detune so it rotates, leaving trailing phosphor
patterns. That part was purely for fun, and it was.

## What's solid, what isn't

Solid: USB control both ways; `0x0E = 49,930 S/s` against a crystal; X-Y mode
with independent time-aligned channels, auto-centre, auto-scale, and phosphor,
cold-boot on open firmware. Not yet: `SCOPE_CAL_SOURCE_SCALE` stays `1.0f` until
a metrology-grade source pins the absolute (the JDS6600's amplitude isn't good
enough to bake in ~0.87), and the X-Y view still throws the occasional divergent
artifact — livable, and the phosphor decay hides it.

Writeups: `docs/experiments/2026-08-21-20-trusted-source-first-contact.md`,
`docs/experiments/2026-08-21-21-ch2-wakes-two-channel-lissajous.md`.
