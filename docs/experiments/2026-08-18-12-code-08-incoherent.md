# EXP-12 — reg 0x01 = 0x08 has no sample rate to measure

- **Date:** 2026-08-18
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, build stamp `Aug 17 2026 23:26:37`
- **Status:** CONFIRMED — and it **WITHDRAWS every rate figure this project has
  published for code `0x08`**, including EXP-10's

## 1. Problem

`0x08` is the power-on default timebase code and the weakest number we have. It
is also the only place two independent benches flatly contradict each other:

| source | figure |
|---|---|
| an earlier session of ours | 1.07 kS/s |
| EXP-10 | 1,660 S/s, R² 0.862, one clear outlier |
| Stlkv (2C23T port, #18) | **5.00 MSa/s** |

A factor of 3000 between the outer two. Somebody is measuring something else.

## 2. Hypothesis

`0x08` is a sample-rate code like the others, and a denser sub-Nyquist sweep
will pin it down.

- **If true:** a dense in-band fit is linear and repeatable, and — the real
  test — the fitted rate correctly **predicts where above-Nyquist tones fold**.
- **If false:** either the fit is unstable, or it looks fine in band and its
  fold predictions miss.

The fold test is the point of this design. An alias lands at
`|f − round(f/fs)·fs|`, a position that is *far* more sensitive to `fs` than
the in-band slope is. A plausible-looking line through a narrow band can be
produced by noise that trends; a correctly predicted fold cannot.

## 3. Procedure

Range 6, CH1, 2000 mVpp, offset centred, free-run, `spi3 opread 0x04`
(at ~1.5 kS/s a 35 ms read cannot lap a 575 ms buffer, so lapping is not in
play). **Three reads per tone, median taken** — a torn buffer should not decide
a point, and the individual reads are printed so scatter is visible rather than
absorbed.

- Phase A: 14 tones, 30–700 Hz, fitted through the origin. Run **twice**.
- Phase B: 5 tones above the fitted Nyquist, each compared against its
  predicted alias bin.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| in-band fit at `0x10` | ~14,850 S/s, R² > 0.95 | **14,853.6 S/s, R² 0.9994** | ✓ |
| read repeatability at `0x10` | reads agree | 250 Hz → [19,19,19]; 3500 Hz → [241,242,241] | ✓ |
| **fold test at `0x10`** | predictions hold where fs is known | **worst miss 8 bins over 5 tones** | ✓ |
| null (generator off) | no peak | mag **0.08** | ✓ |

The third one was added after the fact and is the one that makes this
experiment mean anything. Without it, "the fold prediction failed at `0x08`"
could equally have meant "the fold arithmetic is wrong". Run at `0x10`, where
`fs` is known to 0.5% from five fits:

| f (Hz) | predicted bin | measured | miss |
|---|---|---|---|
| 9,000 | 404 | 403 | 1 |
| 11,000 | 266 | 272 | 6 |
| 14,000 | 59 | 67 | 8 |
| 17,000 | 148 | 145 | 3 |
| 20,000 | 355 | 350 | 5 |

**Side benefit:** this is a fifth confirmation of `0x10` = ~14,850 S/s, by a
mechanism (aliasing) entirely different from the in-band linearity of the
previous four.

## 5. Results

### 5a. Phase A looks acceptable and is not

| pass | fs | R² | max residual |
|---|---|---|---|
| 1 | 1,413.8 S/s | **+0.9304** | 97 bins |
| 2 | 1,525.9 S/s | +0.8361 | 172 bins |

Two passes of the identical sweep, minutes apart, **7.6% apart**. And pass 1's
R² of 0.93 is exactly the trap: taken alone it reads as a decent fit.

The raw reads are what give it away. Three consecutive reads of one unchanged
tone:

| tone | reads (peak bin) |
|---|---|
| 180 Hz | **[171, 132, 104]** |
| 280 Hz | [270, 268, 178] |
| 400 Hz | **[229, 385, 238]** |
| 630 Hz | [256, 484, 474] |

against `0x10`'s [19,19,19] and [241,242,241] in the same session on the same
rig. The record is not reproducing itself between reads.

### 5b. Phase B — the fold prediction fails, and by a lot

Using the two-pass mean fs = 1,470 S/s (Nyquist 735 Hz):

| f (Hz) | predicted bin | measured | miss |
|---|---|---|---|
| 900 | 397 | 170 | **227** |
| 1,100 | 258 | 54 | **204** |
| 1,400 | 49 | 118 | 69 |
| 1,700 | 160 | 306 | **146** |
| 2,000 | 369 | 225 | **144** |

Worst miss 227 bins, against 8 bins for the same test at `0x10`.

## 6. Blind spots

- **This does not say what `0x08` IS**, only that it does not present a
  coherent uniformly-sampled record on this build in free-run. Bit 3 set with
  the low nibble clear may not be a rate code at all — the measured ladder
  lives at `0x0A`–`0x0F` — but that is a hypothesis, not a finding.
- **Free-run only.** Earlier bench notes record `0x08` giving "garbage frames
  when triggered"; this shows free-run is no better, but a third mode was not
  tried.
- **One read path.** `opread` is not lapping at this rate, but the acq-buffer
  path was not cross-checked here.
- **Does not touch Stlkv's 5 MSa/s.** If his firmware's arm sequence gives the
  register a different meaning, both results can be right about different
  things.

## 7. Conclusion

- **WITHDRAWN: every sample-rate figure this project has published for `0x08`.**
  1.07 kS/s, EXP-10's 1,660 S/s, and today's 1,414 / 1,526 are all artifacts of
  fitting a record that does not reproduce between reads. None should be
  quoted.
- **Established:** at `0x08` the capture does not present a coherent
  uniformly-sampled record. Consecutive reads of an unchanged tone disagree by
  hundreds of bins, the fit is unstable at the 7.6% level between passes, and
  the fitted rate misses its own fold predictions by up to 227 bins where the
  same test at `0x10` misses by 8.
- **Established, incidentally:** `0x10` = 14,853.6 S/s is now confirmed a fifth
  time, by aliasing rather than in-band linearity.
- **Method finding worth carrying forward:** **R² over a narrow in-band sweep
  is not sufficient evidence for a sample rate.** Pass 1 returned R² 0.9304 on a
  record that is demonstrably incoherent. The fold test is cheap, is far more
  sensitive to `fs`, and should be run before any rate figure is adopted — it
  is now part of `scripts/measure_sample_rate.py`.
- **NOT excluded:** that `0x08` is a mode selector rather than a rate code; that
  a different arm sequence makes it coherent; that Stlkv's 5 MSa/s is correct
  for his firmware.
- **Follow-up:** report the withdrawal on #18 — the disagreement with Stlkv is
  no longer "1.66 kS/s vs 5 MSa/s" but "we cannot measure it, and here is why",
  which is a cleaner question to put to him.
