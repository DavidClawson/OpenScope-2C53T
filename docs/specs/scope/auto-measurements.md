# Spec: Auto-measurements in real units

**Track:** scope
**Stage now:** S1 (badges compute from real samples; volts/seconds withheld)
**Champion:** DavidClawson

## What it is

The measurement badges (Vpp, Vrms, Vavg, duty, period — Freq is already done)
report in **volts and seconds** instead of ADC counts and samples, on every
range and timebase code that has a measured calibration — and refuse with `--`
on the ones that don't.

## Prior art

Stock shows a full auto-measurement strip. Community demand is explicit:
wishlist Tier 1 #3 ("Correct Min/Max/Avg semantics") and #4 ("Honest
resolution — drop padded trailing zeros") are both complaints about stock
*lying* in this exact UI surface. Every bench scope has this; it is table
stakes.

## Our angle

We can be the instrument whose badges are **provably** right: every conversion
constant traces to an experiment doc, and the badge refuses rather than
guesses. Stock cannot say that; neither can most budget bench scopes.

## Hardware dependencies

Both prerequisites landed in the last week and this spec is the reason they
matter:

- **Vertical:** `scope_cal.c` per-(channel, range) gains — ranges 5/6/7
  measured, 4/8/9 provisional (`docs/experiments/2026-08-17-08…`, `…-09…`).
  Absolute scale still rides on `SCOPE_CAL_SOURCE_SCALE` (uniform, one
  constant, pending a calibrated source).
- **Horizontal:** `scope_timebase.c` — 8 of 21 codes measured
  (`docs/experiments/2026-08-19-17…`, EXP-18).

`firmware/src/ui/scope_measure.h` predicted this moment in its header: it
withheld volts/seconds *because* no cal and no timebase existed, and states
that when both exist, the right move is to fix `measurement_compute()`'s
hardware-wrong constants (3.3/32768 — a 16-bit ADC we don't have) and pass the
true sample rate. Both now exist. Note the header's dev-plan §F2/§F4 citations
are stale — those items are done.

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| S2 | Vpp and Vrms of a bench-driven sine match the commanded amplitude within the cal table's stated uncertainty, and period matches the commanded frequency within the timebase table's, on ≥2 measured ranges × ≥2 measured codes. Writeup in `docs/experiments/`. On any unmeasured range/code the badge shows `--`, verified on-device. |
| S3 | Host regression over captured records (reuse `scripts/capture_scope_records.py` fixtures) with a **negative control**: perturbing a cal row or rate must make the test fail, in the same spirit as `test_scope_timebase.c`. |
| S4 | Badge strip is legible at a glance; user can choose which measurements show; provisional ranges keep their `~` marker in the badge, not just the status bar. |

## Open questions

1. **Unify on `measurement_compute()` or extend `scope_measure.c`?**
   `scope_measure.h` recommends the former once real constants exist. Counter-
   argument: `scope_measure.c` is already the honest, shipped path and the
   badges' refusal logic lives there. Recommendation: keep `scope_measure.c`
   as the engine, lift the extra quantities (rise/fall) out of
   `measurement_compute()`, and delete the rest — one path, not two.
2. Do duty/cycle badges gain anything from cal? (No — they are affine-
   invariant by design. They stay as-is at every stage.)
