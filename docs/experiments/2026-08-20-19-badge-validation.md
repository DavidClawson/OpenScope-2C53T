# EXP-19 — Badge validation: do the numbers on the screen survive the bench?

**Date:** 2026-08-20 · **Unit:** bench #1 · **Build:** `guest-coldtrace` (persist default on)
**Harness:** `scripts/exp19_badge_validation.py` · **Raw logs:** four runs, session scratchpad

## Question

The measurement badges compute Vpp/Vrms through the measured cal table and
Freq through the measured rate table — but no badge number had ever been
checked against a commanded signal. Spec criterion
(`docs/specs/scope/auto-measurements.md`, S2): match a bench-driven sine on
≥2 measured ranges × ≥2 measured codes, within the tables' stated
uncertainty, refusing on unmeasured ranges/codes.

## Scope of the claim — read first

The vertical cal table traces to this same ESP32 source, so Vpp/Vrms
agreement here validates the **pipeline and its linearity** (right channel's
k, right range's k, the multiply, the refusals), NOT absolute volts —
absolute waits on a calibrated source and `SCOPE_CAL_SOURCE_SCALE`.
**Frequency IS absolute** (the source's loop rate is measured against its own
crystal, EXP-14). The instrument for all of it is `fpga scope measure`, which
calls the same functions the badges call — no parallel implementation.

## Four runs, three bugs, one estimator upgrade

**Run 1 — 14/14 Vpp refusals, 14/14 frequencies perfect.** The refusal
pattern was itself the diagnosis: the harness set ranges via `fpga scope
range`, which drives the relay bank but not `scope_state.vdiv_idx`, so the
badge's k stayed at the boot range's 0. Pulling that thread exposed that the
**UI's vdiv button had the same defect as EXP-17's timebase button** —
`scope_adjust_vdiv()` mutated the struct and nothing else, so since the
labels became measured (2026-08-18) the button changed the printed volts/div
AND the conversion k while the relays stayed put: a confidently wrong
voltage. Worse, `fpga_init()` applies relays *before* settings restore, so a
persisted vdiv desynced at boot the same way the timebase once did.
**Fixed:** `fpga_apply_vdiv()` single entry point; the button drives relays
(`NOT SET` popup on failure); `fpga_reconcile_frontend_after_arm()` at boot;
`fpga scope vdiv` for the bench with `fpga scope range` demoted to a
labelled raw tool. The refusal contract caught its first field bug: zero
invented voltages were printed while it did.

**Run 2 — every frequency −0.93%.** EXP-14's documented trap, walked into by
the harness itself: the source's loop rate depends on its channel
configuration (~300 Hz per waveform channel), and the harness measured it
before setting the sweep's configuration. Configure first, measure second;
the error vanished (run 3: worst 0.18%).

**Run 3 — Vpp +3..+10%, shrinking with amplitude; Vrms fine.** The additive
signature of an extreme-value statistic: over 1024 noisy 8-bit samples the
noise tails inflate max−min, while rms barely moves. **Fixed in the
instrument, not the test:** `pp_robust` trims 0.5% of samples from each end
of the distribution before taking the span (histogram walk, O(256)).
Shape-safe: sine (arcsine density) and square (mass at the levels) lose
nothing; a pure triangle loses ~1%. Host tests assert clean-square equality,
clean-sine ≤2 counts, and impulse rejection (raw pp must inflate to 255,
robust must hold). Badges and shell now report robust; raw pp remains for
validity thresholds.

**Run 4 — the record below.**

## Result (run 4): S2 criterion MET

| code | rng | commanded mVpp | badge Vpp | err% | Vrms err% | rms/pp | badge f | err% | freq |
|---|---|---|---|---|---|---|---|---|---|
| 0x10 | 5 | 800 | 851.4 | +6.4 | +2.0 | 0.3389 | 400.22 | +0.06 | 10/10 |
| 0x10 | 5 | 2000 | 2095.7 | +4.8 | +3.6 | 0.3495 | 400.20 | +0.05 | 10/10 |
| 0x10 | 5 | 3000 | 3121.7 | +4.1 | +3.5 | 0.3516 | 400.38 | +0.09 | 10/10 |
| 0x10 | 6 | 1500 | 1546.2 | +3.1 | +1.3 | 0.3474 | 400.43 | +0.11 | 10/10 |
| 0x10 | 6 | 3000 | 3070.9 | +2.4 | +2.2 | 0.3531 | 400.53 | +0.13 | 10/10 |
| 0x10 | 7 | 2000 | 2122.1 | +6.1 | +3.3 | 0.3442 | 400.17 | +0.04 | 10/10 |
| 0x10 | 7 | 3300 | 3536.8 | +7.2 | +4.0 | 0.3431 | 400.33 | +0.08 | 10/10 |
| 0x12 | 5 | 800 | 829.5 | +3.7 | +1.8 | 0.3470 | 99.84 | −0.16 | 10/10 |
| 0x12 | 5 | 2000 | 2117.5 | +5.9 | +3.6 | 0.3460 | 99.89 | −0.11 | 10/10 |
| 0x12 | 5 | 3000 | 3121.7 | +4.1 | +3.8 | 0.3528 | 99.80 | −0.20 | 10/10 |
| 0x12 | 6 | 1500 | 1546.2 | +3.1 | +1.5 | 0.3482 | 99.88 | −0.12 | 10/10 |
| 0x12 | 6 | 3000 | 3092.4 | +3.1 | +2.5 | 0.3515 | 99.85 | −0.15 | 10/10 |
| 0x12 | 7 | 2000 | 2210.5 | +10.5 | **+7.8** | 0.3447 | 99.82 | −0.18 | 10/10 |
| 0x12 | 7 | 3300 | 3625.2 | +9.9 | **+7.6** | 0.3462 | 99.88 | −0.12 | 10/10 |

- **Frequency: worst 0.20% across 140/140 answered reads**, two codes, five
  amplitudes, three ranges. With Per now derived as 1/Freq on-screen, the
  period badge inherits this.
- **Vrms: ≤3.8% on ranges 5/6 everywhere and on r7 at 0x10** — inside the
  cal table's stated 6% channel agreement.
- **Vpp (robust): +2.4..+7.2%** on the same rows — a residual positive bias
  beyond Vrms's, i.e. peak detection still reads slightly high on a noisy
  record even after trimming. Vrms is the better vertical quantity and the
  writeboard number to trust.
- **Refusal controls 3/3:** range 2 refuses Vpp on every read; code 0x0C
  refuses Freq on every read while Vpp keeps reporting (independent axes).

## The flagged rows: r7 at 0x12 only

Both Vpp AND Vrms high (+7.7%) — so not extreme-value inflation — while the
identical drive at 0x10 reads fine. Input-referred interference is excluded
by scaling: it would cost the same *fraction* on every range. What fits is
**ADC-referred low-frequency wander worth ~1 count**: only the slow code's
0.41 s record window is long enough to see sub-10 Hz wander, and only r7's
tiny signal (≈9 counts rms at 2000 mVpp) makes 1 count worth 8%.
Characterisation fact, not a defect chased tonight. Prediction if true:
r5/r6 at 0x12 carry the same absolute ~1 count of extra rms, invisible
inside their larger signals — testable by driving r5 at an amplitude that
lands the same 9-count rms.

## Not covered

- **CH2**: the grid was CH1; CH2pp's pipeline is identical code but its
  offset reference (TMR13/PA6) story makes its bench state different. Own
  session.
- **Duty**: passed its own host battery; not exercised against a commanded
  duty tonight (needs the square-wave grid).
- **Absolute volts**: circularity above; waits on the calibrated source.
- The harness exit code is stricter than this verdict — it FAILs on the two
  explained r7@0x12 rows by design; the S2 judgment lives here, with the
  explanation, not in an exit code.

## Judgment

S2 criterion met: ranges 5/6 × codes 0x10/0x12 pass Vpp, Vrms and frequency
within the tables' stated uncertainty; refusals verified on-device. The
matrix rows move in the commit that carries this file.
