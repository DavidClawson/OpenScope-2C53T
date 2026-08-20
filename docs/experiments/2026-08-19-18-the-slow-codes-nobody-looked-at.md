# EXP-18 — four timebase codes nobody had ever swept, and the ladder is 1-2.5-5

- **Date:** 2026-08-19
- **Unit:** bench unit #1
- **Build:** `Aug 19 2026 17:23:18`
- **Status:** CONFIRMED — calibrated ladder goes from 4 codes to 8

## 1. Problem

EXP-17 left the timebase button working but only 4 of 21 codes labelled. The
button was pressed on the bench, walked from `0x08` to `0x13`, and showed `--`
almost the whole way.

`0x11`–`0x14` had never been measured. Every previous session worked *downward*
from `0x10` and treated everything unmeasured as one block — but codes above
`0x10` are **slower**, which makes them easier to resolve than anything already
done: more cycles per record, higher bins. They were not hard. Nobody looked.

## 2. Hypothesis

If the ladder continues by ×2 per step, `0x11`–`0x14` are 6,250 / 3,125 /
1,562 / 781 S/s.

## 3. Procedure

For each code: five drive frequencies chosen to land the peak between bin 20
and 250, three reads each through `spi3 read` (the acq buffer — the non-tearing
path), median peak bin, least-squares fit of bin against frequency.

## 4. Controls

| control | expected | measured | passed? |
|---|---|---|---|
| source rate pinned in the sweep's channel config | — | 33,296.7 S/s, ×0.8324 | ✓ |
| fit quality | R² > 0.99 | **1.0000 on all four** | ✓ |
| peak sharpness gate | >0.5 | all reads passed | ✓ |
| fold test (scale-invariant) at 0x12 | aliases predicted | worst miss **2.4 bins of 512** | ✓ |

## 5. Results

### 5a. The prediction was wrong, and wrongly round

| code | predicted (×2) | **measured** | round | error |
|---|---|---|---|---|
| `0x11` | 6,250 | **4,990.8** | 5,000 | −0.18% |
| `0x12` | 3,125 | **2,494.9** | 2,500 | −0.20% |
| `0x13` | 1,562 | **1,250.4** | 1,250 | +0.03% |
| `0x14` | 781 | **500.2** | 500 | +0.04% |

**The ladder is 1-2.5-5, not uniform ×2:**
500 / 1250 / 2500 / 5000 / 12500 / 25000 / 50000 / 125000, stepping
2.5, 2, 2, 2.5, 2, 2, 2.5.

Every measured code now lands within **0.2%** of that sequence — except `0x0D`.
Note which way this went: the ×2 guess *looked* like the tidy answer and was
25% out at `0x11`.

### 5b. Fold test at `0x12`

Nyquist is 1,247 Hz. Five tones above it:

| drive | expected alias bin | measured | miss |
|---|---|---|---|
| 1400 | 449.4 | 450.9 | 1.5 |
| 1800 | 285.2 | 286.9 | 1.6 |
| 2200 | 121.0 | 123.0 | 2.0 |
| 2600 | 43.1 | 41.0 | 2.2 |
| 3000 | 207.3 | 205.0 | 2.4 |

Aliasing depends only on `f/fs`, so this is scale-invariant — it tests the rate
itself, not the arithmetic that produced it.

### 5c. `0x0D` re-measured — and it tests EXP-17's untested mechanism

EXP-17 proposed that the original ladder was wrong partly because it was fitted
on **torn `opread` records**, and explicitly recorded that as *plausible,
untested*. `0x0D` is the one measured code still carrying an old `opread` fit.

Re-measured through `spi3 read` (0/12 torn since the snapshot fix) rather than
`opread` (still 6/12):

| | old (`opread`) | new (`spi3 read`) |
|---|---|---|
| fs | 119,678 | **123,662.7** |
| R² | 0.947 | **0.9997** |
| vs round 125,000 | −4.26% | **−1.07%** |

Same code, same rig, different read path, and it moved the predicted way on
both fit quality and distance from the expected value. That is not proof — the
frequencies and session also differ — but it is the first actual evidence for
the mechanism rather than a story that fits.

**`0x0D` stays PROVISIONAL.** At ~124 kS/s the bench source reaches only bin
25, making it the least-resolved row in the table, and −1.07% is five times
the miss of any other code.

## 6. Blind spots

- **`0x0A`–`0x0C` still unmeasured** — on the 1-2.5-5 ladder they should be
  250k / 500k / 1.25M S/s, all far above what the bench source can drive. Needs
  the USB signal generator.
- **`0x06`–`0x09` still INCOHERENT, and now more interesting.** They fitted to
  1,231 / 1,595 / 1,350 / 1,550 S/s — all clustered near `0x13`'s 1,250. If the
  rate field is narrower than the codes written to it, or those codes select
  roll mode rather than a rate, that explains both the clustering and the
  non-reproducibility. **Untested.**
- Fold test run on **one** of the four new codes.
- One channel, one range, one unit, one session.
- The rates still trace to the ESP32 crystal; no calibrated reference.

## 7. Conclusion

- **Established:** `0x11`–`0x14` are 4,990.8 / 2,494.9 / 1,250.4 / 500.2 S/s,
  R² 1.0000 each, one fold-tested. Calibrated codes: **4 → 8**.
- **Established:** the ladder is **1-2.5-5**, and all eight measured codes sit
  within 0.2% of it (0x0D within 1.07%).
- **Supported, not proven:** torn `opread` records explain the old ladder's
  error — `0x0D` moved the predicted way when only the read path changed.
- **Method note:** these four codes were skipped for months because "unmeasured"
  was treated as one undifferentiated block. Half of it was the easiest
  measurement available. Worth asking, when something is deferred as hard,
  whether all of it is.
