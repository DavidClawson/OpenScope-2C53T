# Spec: Auto-measurements in real units

**Track:** scope
**Stage now:** S2 — bench-validated 2026-08-20 (EXP-19). Vrms ≤3.8%, Period
(as 1/Freq) ≤0.2%, Vpp ≤7% with a documented residual bias; refusals verified
on-device. The validation found and fixed three bugs on the way: the vdiv
button was the timebase button's decorative twin (relays never driven, no
boot reconcile), the harness repeated EXP-14's fs-configuration mistake, and
raw max−min Vpp is noise-inflated — replaced by percentile-trimmed
`pp_robust`. See `docs/experiments/2026-08-20-19-badge-validation.md`.
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
| ~~S2~~ | **DONE 2026-08-20** (EXP-19): 3 ranges × 2 codes, Vrms ≤3.8%, f ≤0.2%, refusal controls 3/3. Two r7@0x12 rows flagged with an ADC-wander hypothesis and a stated test. Open questions 1 was decided as recommended: `scope_measure.c` stays the engine. |
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
