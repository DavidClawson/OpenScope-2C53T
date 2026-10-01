# EXP-64 — is SCOPE_CAL_SOURCE_SCALE = 0.92 right? A DMM-measured square against the gain table

- **Date:** 2026-10-01
- **Unit:** bench unit #3 (V1.4). The gain table and the 0.92 were both taken on unit #1.
- **Build:** unit #3 on a build of PR #41/#47 (record the `build:` line). Scripts at
  `bench/signal-source-abstraction` @ `2ce8a3a`. Source: `kodedot_sigsource`, Kode Dot (ESP32-P4),
  GPIO14 / J3 pin 9, 0 V / ~3.29 V square or held level. Reference: a DMM (model and spec below).
- **Status:** OPEN — pre-registered before any capture. Thresholds below are fixed now.

## 1. Problem
Every volts/div label, Vpp and Vrms the instrument prints is `table × SCOPE_CAL_SOURCE_SCALE`, and the
constant is 0.92. It was fitted on unit #1 against a JDS6600 (~1–2% amplitude spec): EXP-20 r6 0.919,
r7 0.884, DC r7 0.874; EXP-21 per-channel r6 0.924 / 0.948, r5 0.943 / 0.966, a 0.874–0.966 spread put down
to the source plus "a mild range trend". It has never faced a plain DC reference. README: "Absolute
vertical scale is unverified." **Is 0.92 right, to what precision, and is it one constant across
ranges, as the model assumes?**

## 2. Hypothesis
Per row, `s(ch, r) = V_ref / static-lvl(ch, r)`, from the script's `static-lvl` column (mean of a `dc 1`
capture minus mean of a `dc 0` capture, times the *unscaled* table mV/count). **H: the constant is 0.92
and uniform**, so every row reads `static-lvl ≈ 1.087 × V_ref` (3578 mV at 3.292 V). Predicted counts
(dm = high-rail minus low-rail mean) at 3292 mV, from the script's coverage table:

| range | r5 (`--center-mid` only) | r6 | r7 | r8 | r9 |
|---|---|---|---|---|---|
| dm, CH1 / CH2 | 164 / 171 | 83.3 / 85.8 | 40.5 / 42.7 | 12.8 / 16.0 | 10.2 / 8.4 |
| script quantisation floor (0.5 count / dm) | 0.3% | 0.6% | 1.2% | 3–4% | 5–6% |

Decision on **S6, S7** = channel mean of `s` at r6, r7 (r8/r9 are context and never gate):

| outcome | rule | consequence |
|---|---|---|
| **A** 0.92 holds, uniform | S6 in 0.92 ±3% (0.892–0.948), S7 in 0.92 ±5% (0.874–0.966), \|S6−S7\|/mean ≤ 5% | 0.92 stands, to ~3–5% |
| **B** uniform, value wrong | \|S6−S7\|/mean ≤ 5%, both outside their windows on the same side | candidate = mean(S6,S7), 0.01 steps. Not applied here |
| **C** not uniform | \|S6−S7\|/mean > 5% | "table × one constant" refuted at this precision; SCALE untouched |
| edge | agree ≤ 5% but one value straddles a window edge | report both numbers, no verdict on the value |

Why these numbers (budget, worst-case sum): DMM ±1% (replace with the meter's spec) + quantisation
(0.6% r6, 1.2% r7) + half the table's own CH1/CH2 row gap (3.0% r6, 5.5% r7, which enters the channel
mean) = 3.1% / 5.0%. The 5% bar: the r7/r6 ladder uncertainty (2.0–2.17 across the table, EXP-06 and Stlkv)
plus 1.3% quantisation; the DMM error is common to both ranges and cancels. The script prints "consistent"
up to 10% pooled over r6–r9, which is too loose; the bar above decides. **C is a result, not VOID**:
r6-vs-r7 agreement is the falsifier of the single-constant model, so it is deliberately not a control.
C cannot say whether the source constant or a table row is at fault; one amplitude cannot separate them.

History, so a surprise is recognisable: EXP-21 gives S6 ≈ 0.94; if both frontends respond alike (EXP-21:
25.2 vs 25.3 counts/Vpp at r6) S7 ≈ 0.91, i.e. outcome A. Secondary, non-gating, same assumption: dm equal
on both channels, so CH2's `s` exceeds CH1's by the table's row ratio, +3.0% at r6, +5.5% at r7 (EXP-21: +2.6% at r6).

## 3. Procedure
**By hand first** (reference and baseline). Pin held with `d = bench.KodeDotSource(); d.hold(1)` (the
level persists after the port closes; `hold(0)` for `dc 0`). DMM across J3 pin 9 and pin 10, both probes
attached on pin 9, springs on 10/11, ×1 switches checked by eye.

| what | expected | measured |
|---|---|---|
| DMM model, DC range, accuracy spec at 3.3 V | recorded; ±1% assumed in the budget | |
| `dc 1`, probes attached (this value is `--v3v3`) | ~3.29 V (3V3 rail alone read 3.292 V) | |
| `version`; Dot `s` (`clk_hz`); `fpga scope timebase` after the script's 0x10 | recorded; 80000000; `0x10 (reg 0x01 = 0x10)` | |

Baseline readback at `dc 0` (the script discards the centring reply, so this is taken by hand):
```
python3 - <<'EOF'
import sys; sys.path.insert(0, "scripts"); import bench
sc = bench.Scope(bench.find_port(*bench.SCOPE_USB_ID, what="2C53T")); dot = bench.KodeDotSource(); dot.hold(0)
for r in (6, 7):
    sc.scope_range(r, 1); sc.scope_range(r, 2)
    for ch in (1, 2):
        c = [l for l in sc.cmd(f"fpga scope center ch{ch} {r}", timeout=20).splitlines() if "median" in l]
        print(r, ch, c, round(float(sc.opread(0x03 + ch).mean()), 1))
dot.close(); sc.close()
EOF
```
```
python3 scripts/verify_scope_cal.py --source kodedot --v3v3 <V_hi_att> --timebase 0x10 --ranges 6 7 8 9                          # run 1
python3 scripts/verify_scope_cal.py --source kodedot --v3v3 <V_hi_att> --timebase 0x10 --center-mid --ranges 5 6 7 8 9    # run 2
```
Run 2 only if run 1 is clean: controls 1–5 passed and outcome A or B. Per range the script centres on the
quiet low rail (code 128), captures `dc 0`, `dc 1`, then a 330 Hz square (median of 5 reads each); its own
control runs at r6 in run 1 and r7 in run 2. S6/S7 are computed by hand from the printed `static-lvl`
column: `s = V_ref(mV) / static-lvl(mV)`. The script's `-> SCALE` line pools every range it ran, r8/r9
included (provisional rows, ±3–6% quantisation), so it is recorded and does not decide.

## 4. Control
Record before section 5. Any failure of 1–5 makes the experiment **VOID, not negative**.

| control | expected | measured | passed? |
|---|---|---|---|
| 1. DMM at `dc 1`, probes attached vs detached; and again after run 1 | within 0.3% (1 MΩ on ~100 Ω predicts 0.01%); drift < 0.3% | | |
| 2. DMM at `dc 0` (the script takes the low level as exactly 0 V) | ≤ 15 mV | | |
| 3. quiet baseline after centring, `dc 0`, r6 and r7, both channels | `median=` and opread mean 128 ± 6 | | |
| 4. script's own control (r6 in run 1, r7 in run 2): drive lifts span ≥ 4× the quiet floor | `PASSED`, both channels | | |
| 5. static-level vs floor-corrected span × k (one quantity, two estimators; the span sees edge overshoot, the means do not) | r6 within 3%, r7 within 6% (sum of their floors); a bigger gap and neither is trusted | | |

## 5. Results
_Empty until the runs are done. Raw printed tables, the baseline readback and the DMM readings, unedited._

## 6. Blind spots
- **One amplitude (3.29 V).** It uses 83 / 40 counts at r6 / r7. No linearity check: this is the gain at
  one operating point, where the table's slopes came from five amplitudes per range.
- **Upper half of the ADC only without `--center-mid`.** The low rail sits at code 128. `--center-mid`
  assumes the offset DAC is linear with enough travel and has only run against `SimBench`; its CLIPPED
  flags and `centring FAILED` lines decide which rows to exclude.
- **One unit.** The constant comes from unit #1, tested on unit #3. A unit-to-unit gain difference
  (attenuator tolerances; the factory cal in flash at 0x08006000 is overwritten, not used) cannot be told
  apart from a source-scale error. A or B here says nothing about other boards.
- **8-bit quantisation on r8/r9** (< 20 counts). The 0.5-count floor assumes noise dithers the mean; if
  noise is under one count the mean does not average below a step and the true floor is nearer ±8–10%.
- **Probe ×1, and a held-pin reference.** The table is mV at the probe tip with a ×1 probe; a ×10 switch
  would show as ~9.2. DC loading is covered by control 1. The DMM reads a held pin, not the 330 Hz square's
  level while switching; the static-level estimator never sees the square, only control 5's span does.
- **No two-shapes control.** One pin feeds both channels, so a readout that mirrored one channel into the
  other would not be caught. CH1 vs CH2 here compares one signal through two readouts.
- **Confounded model.** A non-uniform source scale and a wrong table row both show up as outcome C.
- **r4 and r0–r3 are untouched:** r4 clips even with `--center-mid` (254 / 377 vs 240 counts); r0–r3 have no gain.

## 7. Conclusion
- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**
- **Follow-up:**
