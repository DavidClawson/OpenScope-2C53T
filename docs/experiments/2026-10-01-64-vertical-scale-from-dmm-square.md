# EXP-64 — is SCOPE_CAL_SOURCE_SCALE = 0.92 right? A DMM-measured square against the gain table

- **Date:** 2026-10-01
- **Unit:** bench unit #3 (V1.4). The gain table and the 0.92 were both taken on unit #1.
- **Build:** unit #3 on a build of PR #41/#47 (record the `build:` line). Scripts at
  `bench/signal-source-abstraction` @ `2ce8a3a`. Source: `kodedot_sigsource`, Kode Dot (ESP32-P4),
  GPIO14 / J3 pin 9, 0 V / ~3.29 V square or held level. Reference: a DMM (model and spec below).
- **Status:** run 1 **VOID** (control 3), run 1b **edge** (S6 inside its window, S7 on the lower edge, r6/r7 agree to 4.4%), run 2 exploratory **A-shaped** (S5/S6/S7 = 0.916/0.909/0.895). Single-constant model not refuted; 0.92 holds to ~3% **against the unit's own meter path**, which is the only reference on this bench (§6).

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
**Deviations from the pre-registration (2026-10-01, before any run; same bench as EXP-63 §3):**
- Source pin **GPIO9** (EXP2, J3 pin 4), not GPIO14; `kodedot_sigsource` standalone on a Dot without panel; `ppm 35` set by hand. Static levels and 330 Hz are unaffected by GPIO9's ~190 pF clamp.
- **CH1 = crocodile-clip lead (×1 by construction), CH2 = the ×1 probe** — the BNCs were swapped while diagnosing the probe, whose tip cartridge was not seated (EXP-63 §3); once seated, both channels read the 1 kHz square at 87 / 88 codes p-p (range 6, 0x10). Both on GPIO9, grounds on the GND pair beside GPIO14/21.
- `fpga scope center` timeout 60 s (20.4 s measured).
- Probe switch at ×1 confirmed by the operator (the lead on CH1 has no switch).

**By hand first** (reference and baseline). Pin held with `d = bench.KodeDotSource(); d.hold(1)` (the
level persists after the port closes; `hold(0)` for `dc 0`). DMM across J3 pin 9 and pin 10, both probes
attached on pin 9, springs on 10/11, ×1 switches checked by eye.

| what | expected | measured |
|---|---|---|
| DMM model, DC range, accuracy spec at 3.3 V | recorded; ±1% assumed in the budget | **the "DMM" is the unit under test itself in multimeter mode** (OpenScope meter path, DCV, 1 mV display); accuracy unknown, NOT independent of the device — see §6 |
| `dc 1`, probes attached (this value is `--v3v3`) | ~3.29 V (3V3 rail alone read 3.292 V) | **3.303 V** |
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
| 1. DMM at `dc 1`, probes attached vs detached; and again after run 1 | within 0.3% (1 MΩ on ~100 Ω predicts 0.01%); drift < 0.3% | attached 3.303 V, detached 3.292 V: **0.33%, marginal**, and in the direction loading cannot produce (attaching raised the reading). Candidate: the ground loop closed by the probe springs (scope and Dot share the Mac's USB ground; the meter's COM sits on the same chassis). The attached value is what the channels see and is used. After run 1: pending | marginal (see §6) |
| 2. DMM at `dc 0` (the script takes the low level as exactly 0 V) | ≤ 15 mV | reads 0 V (display resolution 1 mV) | yes |
| 3. quiet baseline after centring, `dc 0`, r6 and r7, both channels | `median=` and opread mean 128 ± 6 | run 1: `fpga scope center` reports `median=128` on all four (acq RAM buffer: CH1 DAC1=2639/2643, CH2 TMR13_C1DT=2655/2639), but the **opread mean right after is 155.6 / 155.4 / 155.8 / 155.6** — 27.6 counts above the centred value on the path the script measures through → **no**. Run 1b (`--center-path opread`, DAC servoed on opread captures): 128.0 / 128.0 / 127.9 / 128.0 → **yes** | run 1 no, run 1b yes |
| 4. script's own control (r6 in run 1, r7 in run 2): drive lifts span ≥ 4× the quiet floor | `PASSED`, both channels | run 1: quiet 3.00 / 3.00, driven 87.00 / 85.00, `PASSED`; run 1b: quiet 6.00 / 2.00, driven 91.00 / 87.00, `PASSED` | yes |
| 5. static-level vs floor-corrected span × k (one quantity, two estimators; the span sees edge overshoot, the means do not) | r6 within 3%, r7 within 6% (sum of their floors); a bigger gap and neither is trusted | run 1: CH1 r6 3591 vs 3651 (1.7%), r7 3773 vs 3802 (0.8%); CH2 r6 3466 vs 3420 (1.3%), r7 3455 vs 3435 (0.6%). Run 1b: CH1 r6 3679 vs 3651 (0.8%), r7 3866 vs 3714 (4.1%); CH2 r6 3541 vs 3504 (1.1%), r7 3684 vs 3687 (0.1%) | yes |

## 5. Results
### Run 1 — VOID by control 3; the numbers are recorded, not judged (`dumps/exp64_run1.log`, unedited)
```
=== baseline at dc 0 (centring r6/r7, both channels) 16:15:49
6 1 ['CH1 range 6: center DAC1=2639 (median=128)'] 155.6
6 2 ['CH2 range 6: center TMR13_C1DT=2655 (median=128)'] 155.4
7 1 ['CH1 range 7: center DAC1=2643 (median=128)'] 155.8
7 2 ['CH2 range 7: center TMR13_C1DT=2639 (median=128)'] 155.6
=== run 1 16:17:13
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >
table: firmware/src/ui/scope_cal.c
source: Kode Dot LEDC square, 0 V / 3303 mV (DMM-measured rail); waveform square; one output wired to both probes
coverage at 3303 mVpp (scope_cal.c gains x SCOPE_CAL_SOURCE_SCALE 0.92; its LOW level centred at code 128, so a drive may use 119 counts):
  fits     r6 (CH1 84 / CH2 86), r7 (CH1 41 / CH2 43)
  coarse   r8 (CH1 13 / CH2 16), r9 (CH1 10 / CH2 8)  -- under 20 counts: +/-2 counts of span quantisation is over 10%
  clips    r4 (CH1 255 / CH2 378), r5 (CH1 164 / CH2 171)  -- over 119 counts (--center-mid allows 240)
  no cal   r0 r1 r2 r3  -- gain 0.0f in scope_cal.c

=== CONTROL (run first) ===
  range 6 quiet:  op04   3.00   op05   3.00
  range 6 driven: op04  87.00   op05  85.00
  control PASSED (drive must lift span >=4x over the quiet floor)

=== per-range, 3303 mVpp ===
rng ch |  floor     lo     hi | floor-corr | two-point | tier-gain | expect | static-lvl
 6  1 |    2.0      -   87.0 |     3651mV |        n/a |  42.95 mV/ct |   83.6 |     3591mV
 6  2 |    3.0      -   85.0 |     3420mV |        n/a |  41.71 mV/ct |   86.1 |     3466mV
 7  1 |    2.0      -   45.0 |     3802mV |        n/a |  88.42 mV/ct |   40.6 |     3773mV
 7  2 |    2.0      -   43.0 |     3435mV |        n/a |  83.79 mV/ct |   42.8 |     3455mV
 8  1 |    3.0      -   20.0 |     4744mV |        n/a | 279.05 mV/ct |   12.9 |     4812mV
 8  2 |    2.0      -   19.0 |     3803mV |        n/a | 223.71 mV/ct |   16.0 |     3747mV
 9  1 |    3.0      -   10.0 |     2465mV |        n/a | 352.17 mV/ct |   10.2 |     3042mV
 9  2 |    2.0      -   11.0 |     3825mV |        n/a | 425.00 mV/ct |    8.4 |     3551mV

=== agreement across ranges (reference 3303 mVpp) ===
  CH1 floor-corrected: 3651 / 3802 / 4744 / 2465 mVpp   spread 62.2%  (quantisation floor 28.6%, limit 42.9%)  INCONSISTENT  <-- exceeds quantisation floor   mean/reference 1.110   -> SCOPE_CAL_SOURCE_SCALE 0.901 (compiled 0.92)
  CH1 static-level  : 3591 / 3773 / 4812 / 3042 mVpp   spread 46.5%  (quantisation floor  5.8%, limit 10.0%)  INCONSISTENT  <-- exceeds quantisation floor   mean/reference 1.152   -> SCOPE_CAL_SOURCE_SCALE 0.868 (compiled 0.92)
  CH2 floor-corrected: 3420 / 3435 / 3803 / 3825 mVpp   spread 11.2%  (quantisation floor 22.2%, limit 33.3%)  consistent   mean/reference 1.096   -> SCOPE_CAL_SOURCE_SCALE 0.912 (compiled 0.92)
  CH2 static-level  : 3466 / 3455 / 3747 / 3551 mVpp   spread  8.2%  (quantisation floor  6.0%, limit 10.0%)  consistent   mean/reference 1.076   -> SCOPE_CAL_SOURCE_SCALE 0.929 (compiled 0.92)

BLIND SPOT: one output into both channels. The two-SHAPES control (triangle on CH1, square on CH2) is not available, so a display that duplicated one channel onto the other would not be caught here.

NOTE ON THE mean/reference COLUMN
  The reference is a MEASURED amplitude (a DMM), not the generator these gains
  were derived from, so this ratio IS an accuracy figure for the raw table,
  and 1/ratio is the measured value of SCOPE_CAL_SOURCE_SCALE — printed as
  "->". Compare it with the compiled constant; if it moves, change ONLY that
  constant in scope_cal.h, one range being enough, because a source scale
  error is uniform across the whole table by construction. Do not adjust
  individual rows; tests/test_scope_cal.c will fail if you do. Its accuracy
  is the DMM's plus this script's quantisation floor.
=== end 16:21:39
```
**Why VOID:** the script measures through `spi3 opread` (`verify_scope_cal.py` `capture()` → `sc.opread`), but `fpga scope center` centres the **acq** RAM buffer. On this unit the two paths sit **27.6 counts apart in DC** (acq median 128 → opread mean 155.6, all four range/channel pairs, at `dc 0`), which EXP-63 could not see (it compared frequencies, not levels). The pre-registered headroom (LOW at 128, drive ≤ 119 counts) was therefore not there on the measured path: CH1 r6 reached 155.6 + 87 = 242.6 of 255. Nothing shows clipping (control 5 passes, the r6/r7 spans scale 2.0×), but a control written to guarantee headroom failed, and the rule says VOID. The 27.6-count path offset is a finding in its own right (§7).

### Run 1b — same ranges, centring servoed on the opread path (`dumps/exp64_run1b.log`, unedited)
```
=== run 1b (opread-path centring) 16:35:43
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >
table: firmware/src/ui/scope_cal.c
source: Kode Dot LEDC square, 0 V / 3303 mV (DMM-measured rail); waveform square; one output wired to both probes
coverage at 3303 mVpp (scope_cal.c gains x SCOPE_CAL_SOURCE_SCALE 0.92; its LOW level centred at code 128, so a drive may use 119 counts):
  fits     r6 (CH1 84 / CH2 86), r7 (CH1 41 / CH2 43)
  coarse   r8 (CH1 13 / CH2 16), r9 (CH1 10 / CH2 8)  -- under 20 counts: +/-2 counts of span quantisation is over 10%
  clips    r4 (CH1 255 / CH2 378), r5 (CH1 164 / CH2 171)  -- over 119 counts (--center-mid allows 240)
  no cal   r0 r1 r2 r3  -- gain 0.0f in scope_cal.c

=== CONTROL (run first) ===
  r6 CH1: opread servo trig raw 2424 -> mean 128.0 (target 128, rising DAC)
  r6 CH2: opread servo trig2 raw 2443 -> mean 128.0 (target 128, rising DAC)
  range 6 quiet:  op04   6.00   op05   2.00
  range 6 driven: op04  91.00   op05  87.00
  control PASSED (drive must lift span >=4x over the quiet floor)

=== per-range, 3303 mVpp ===
rng ch |  floor     lo     hi | floor-corr | two-point | tier-gain | expect | static-lvl
  r6 CH1: opread servo trig raw 2426 -> mean 128.1 (target 128, rising DAC)
  r6 CH2: opread servo trig2 raw 2443 -> mean 128.0 (target 128, rising DAC)
 6  1 |    6.0      -   91.0 |     3651mV |        n/a |  42.95 mV/ct |   83.6 |     3679mV
 6  2 |    3.0      -   87.0 |     3504mV |        n/a |  41.71 mV/ct |   86.1 |     3541mV
  r7 CH1: opread servo trig raw 2426 -> mean 127.9 (target 128, rising DAC)
  r7 CH2: opread servo trig2 raw 2434 -> mean 128.0 (target 128, rising DAC)
 7  1 |    7.0      -   49.0 |     3714mV |        n/a |  88.42 mV/ct |   40.6 |     3866mV
 7  2 |    2.0      -   46.0 |     3687mV |        n/a |  83.79 mV/ct |   42.8 |     3684mV
  r8 CH1: opread servo trig raw 2428 -> mean 128.0 (target 128, rising DAC)
  r8 CH2: opread servo trig2 raw 2431 -> mean 128.1 (target 128, rising DAC)
 8  1 |    6.0      -   23.0 |     4744mV |        n/a | 279.05 mV/ct |   12.9 |     5013mV
 8  2 |    2.0      -   21.0 |     4250mV |        n/a | 223.71 mV/ct |   16.0 |     3878mV
  r9 CH1: opread servo trig raw 2429 -> mean 128.1 (target 128, rising DAC)
  r9 CH2: opread servo trig2 raw 2428 -> mean 128.0 (target 128, rising DAC)
 9  1 |    6.0      -   14.0 |     2817mV |        n/a | 352.17 mV/ct |   10.2 |     3339mV
 9  2 |    2.0      -   11.0 |     3825mV |        n/a | 425.00 mV/ct |    8.4 |     3875mV

=== agreement across ranges (reference 3303 mVpp) ===
  CH1 floor-corrected: 3651 / 3714 / 4744 / 2817 mVpp   spread 51.6%  (quantisation floor 25.0%, limit 37.5%)  INCONSISTENT  <-- exceeds quantisation floor   mean/reference 1.130   -> SCOPE_CAL_SOURCE_SCALE 0.885 (compiled 0.92)
  CH1 static-level  : 3679 / 3866 / 5013 / 3339 mVpp   spread 42.1%  (quantisation floor  5.3%, limit 10.0%)  INCONSISTENT  <-- exceeds quantisation floor   mean/reference 1.203   -> SCOPE_CAL_SOURCE_SCALE 0.831 (compiled 0.92)
  CH2 floor-corrected: 3504 / 3687 / 4250 / 3825 mVpp   spread 19.6%  (quantisation floor 22.2%, limit 33.3%)  consistent   mean/reference 1.155   -> SCOPE_CAL_SOURCE_SCALE 0.865 (compiled 0.92)
  CH2 static-level  : 3541 / 3684 / 3878 / 3875 mVpp   spread  9.0%  (quantisation floor  5.5%, limit 10.0%)  consistent   mean/reference 1.134   -> SCOPE_CAL_SOURCE_SCALE 0.882 (compiled 0.92)

BLIND SPOT: one output into both channels. The two-SHAPES control (triangle on CH1, square on CH2) is not available, so a display that duplicated one channel onto the other would not be caught here.

NOTE ON THE mean/reference COLUMN
  The reference is a MEASURED amplitude (a DMM), not the generator these gains
  were derived from, so this ratio IS an accuracy figure for the raw table,
  and 1/ratio is the measured value of SCOPE_CAL_SOURCE_SCALE — printed as
  "->". Compare it with the compiled constant; if it moves, change ONLY that
  constant in scope_cal.h, one range being enough, because a source scale
  error is uniform across the whole table by construction. Do not adjust
  individual rows; tests/test_scope_cal.c will fail if you do. Its accuracy
  is the DMM's plus this script's quantisation floor.
=== end 16:40:50
```
Controls 3–5 hold (table in §4). **The spans grew against run 1** (CH1 r6 91 vs 87 counts, r7 49 vs 45; CH2 87 vs 85, 46 vs 43): with the low rail at 155.6 the high rail sat at 242.6 of 255 and the top of CH1's range was compressing by ~4 counts — exactly the headroom control 3 was written for, which is why run 1 stays VOID rather than "near enough".

**Decision (pre-registered rules, §2), V_ref = 3303 mV:** s = V_ref / static-lvl: CH1 r6 0.8978, r7 0.8544; CH2 r6 0.9328, r7 0.8966 → **S6 = 0.915** (window 0.892–0.948: inside), **S7 = 0.876** (window 0.874–0.966: inside by 0.0015, i.e. 0.17%, with a quantisation floor of 1.2% at r7), |S6−S7|/mean = **4.4%** (≤ 5%). By the table this is the **edge** row: the two ranges agree, so "table × one constant" is not refuted at this precision, and S6 says 0.92 to within its ±3% window; but S7 straddles its lower edge, so no verdict on the value from r7. Secondary (non-gating): CH2's s exceeds CH1's by +3.9% at r6 (predicted +3.0%) and +4.9% at r7 (predicted +5.5%) — with the headroom restored, the two frontends track the table's row gap, which run 1's +9.2% at r7 (the compressed CH1) did not.

### Run 2 — exploratory (`--center-mid`, r5–r9, opread centring), aborted after r7 by the instrument (`dumps/exp64_run2.log`, unedited)
Beyond the plan: run 1b was "edge", not A/B, so the pre-registration did not authorise run 2; it was run anyway because r5 (164–171 counts of drive) resolves S better than any range above it, and it is labelled as such. The two-level servo put the square's midpoint at 128 on every row (low-rail / high-rail DAC pairs printed per row).
```
=== run 2 (exploratory: --center-mid, r5-r9, opread centring) 16:41:32
build: version | OpenScope 2C53T | Build: Oct  1 2026 14:36:14 | MCU: AT32F403A @ 240MHz | SRAM: 224KB (EOPB0=0xFE) | >  | >
table: firmware/src/ui/scope_cal.c
source: Kode Dot LEDC square, 0 V / 3303 mV (DMM-measured rail); waveform square; one output wired to both probes
coverage at 3303 mVpp (scope_cal.c gains x SCOPE_CAL_SOURCE_SCALE 0.92; its midpoint centred at code 128, so a drive may use 240 counts):
  fits     r5 (CH1 164 / CH2 171), r6 (CH1 84 / CH2 86), r7 (CH1 41 / CH2 43)
  coarse   r8 (CH1 13 / CH2 16), r9 (CH1 10 / CH2 8)  -- under 20 counts: +/-2 counts of span quantisation is over 10%
  clips    r4 (CH1 255 / CH2 378)  -- over 240 counts
  no cal   r0 r1 r2 r3  -- gain 0.0f in scope_cal.c

=== CONTROL (run first) ===
  r7 CH1: opread servo trig raw 2432 -> mean 127.9 (target 128, rising DAC)
  r7 CH1: opread servo trig raw 2089 -> mean 128.0 (target 128, rising DAC)
  r7 CH2: opread servo trig2 raw 2433 -> mean 128.0 (target 128, rising DAC)
  r7 CH2: opread servo trig2 raw 2098 -> mean 128.0 (target 128, rising DAC)
  r7 CH1: offset DAC low rail 2432, high rail 2089 -> midpoint 2260
  r7 CH2: offset DAC low rail 2433, high rail 2098 -> midpoint 2266
  range 7 quiet:  op04   3.00   op05   2.00
  range 7 driven: op04  45.00   op05  47.00
  control PASSED (drive must lift span >=4x over the quiet floor)

=== per-range, 3303 mVpp ===
rng ch |  floor     lo     hi | floor-corr | two-point | tier-gain | expect | static-lvl
  r5 CH1: opread servo trig raw 2422 -> mean 128.1 (target 128, rising DAC)
  r5 CH1: opread servo trig raw 1084 -> mean 128.0 (target 128, rising DAC)
  r5 CH2: opread servo trig2 raw 2458 -> mean 128.0 (target 128, rising DAC)
  r5 CH2: opread servo trig2 raw 1127 -> mean 128.0 (target 128, rising DAC)
  r5 CH1: offset DAC low rail 2422, high rail 1084 -> midpoint 1753
  r5 CH2: offset DAC low rail 2458, high rail 1127 -> midpoint 1792
 5  1 |    3.0      -  176.0 |     3777mV |        n/a |  21.83 mV/ct |  164.5 |     3690mV
 5  2 |    2.0      -  173.0 |     3584mV |        n/a |  20.96 mV/ct |  171.3 |     3528mV
  r6 CH1: opread servo trig raw 2426 -> mean 128.2 (target 128, rising DAC)
  r6 CH1: opread servo trig raw 1755 -> mean 128.2 (target 128, rising DAC)
  r6 CH2: opread servo trig2 raw 2441 -> mean 128.0 (target 128, rising DAC)
  r6 CH2: opread servo trig2 raw 1776 -> mean 128.0 (target 128, rising DAC)
  r6 CH1: offset DAC low rail 2426, high rail 1755 -> midpoint 2090
  r6 CH2: offset DAC low rail 2441, high rail 1776 -> midpoint 2108
 6  1 |    2.0      -   88.0 |     3694mV |        n/a |  42.95 mV/ct |   83.6 |     3649mV
 6  2 |    2.0      -   89.0 |     3629mV |        n/a |  41.71 mV/ct |   86.1 |     3615mV
  r7 CH1: opread servo trig raw 2428 -> mean 128.1 (target 128, rising DAC)
  r7 CH1: opread servo trig raw 2089 -> mean 127.9 (target 128, rising DAC)
  r7 CH2: opread servo trig2 raw 2433 -> mean 128.0 (target 128, rising DAC)
  r7 CH2: opread servo trig2 raw 2098 -> mean 128.0 (target 128, rising DAC)
  r7 CH1: offset DAC low rail 2428, high rail 2089 -> midpoint 2258
  r7 CH2: offset DAC low rail 2433, high rail 2098 -> midpoint 2266
 7  1 |    2.0      -   45.0 |     3802mV |        n/a |  88.42 mV/ct |   40.6 |     3819mV
 7  2 |    2.0      -   47.0 |     3771mV |        n/a |  83.79 mV/ct |   42.8 |     3574mV
  r8 CH1: opread servo trig raw 2429 -> mean 128.0 (target 128, rising DAC)
bench: opread 04: asked for 1026 bytes, parsed 0 — window unusable
=== end 16:49:49
```
- The run stopped at r8: `opread 04: asked for 1026 bytes, parsed 0 — window unusable` at 16:49:4x. The scope had **not** reset (uptime 7609 s afterwards, same session #2): its CDC self-heal had fired (`heals=1`, `stalls=4` of which 2 host_slow — i.e. two consecutive lost-completion stalls, the #39 signature) and the device re-enumerated on its own; the script has no reconnect logic and aborted. Written up as EXP-66 (PR #48 branch). r8/r9 were context, not decision, so nothing in §2 is lost.
- Controls: 4 at r7 `PASSED` (quiet 3 / 2, driven 45 / 47); 5: CH1 r6 3649 vs 3694 (1.2%), r7 3819 vs 3802 (0.4%); CH2 r6 3615 vs 3629 (0.4%), r7 3574 vs 3771 (5.5%, inside the 6%); r5 (no bar set): CH1 2.3%, CH2 1.6%.
- s = 3303 / static-lvl: CH1 r5 0.8951, r6 0.9052, r7 0.8649; CH2 r5 0.9362, r6 0.9137, r7 0.9242 → **S5 = 0.916, S6 = 0.909, S7 = 0.895**; |S6−S7|/mean = 1.6%; both inside their windows (0.892–0.948, 0.874–0.966): the shape of **outcome A**, with r5 agreeing (S5 within 0.8% of S6). Secondary: CH2/CH1 = +4.6% at r5 (table row ratio +4.2%), +0.9% at r6 (run 1b: +3.9%; predicted +3.0%), +6.9% at r7 (predicted +5.5%) — the r6 channel gap moved by 3% between the low-rail and the mid-scale centring, which bounds what one amplitude at one centring can say about a row.

**What run 1's data would have said, had control 3 held** (reported so a rerun has something to compare, no verdict): with V_ref = 3303 mV, s = V_ref / static-lvl: CH1 r6 0.9198, r7 0.8754; CH2 r6 0.9530, r7 0.9560 → S6 = 0.936 (window 0.892–0.948), S7 = 0.916 (0.874–0.966), |S6−S7|/mean = 2.2% → the shape of **outcome A**. Secondary: CH2's s exceeds CH1's by +3.6% at r6 (predicted +3.0%) and **+9.2% at r7 (predicted +5.5%)**: the two frontends differ at r7 by ~3.5% more than the table's row gap, i.e. the per-channel r7 gains of this unit are not those of unit #1 to that extent.

_Empty until the runs are done. Raw printed tables, the baseline readback and the DMM readings, unedited._

## 6. Blind spots
- **The reference is the device under test.** The "DMM" is the 2C53T's own multimeter mode (OpenScope meter path, uncalibrated against anything external; EXP-60 corrected the same way). Every `s` here is scope-channel-vs-meter-path consistency; an error common to both paths (a wrong rail reading) is invisible. Nothing in this file is an absolute volt.
- **Control 1 was marginal in the wrong direction** (attached 3.303 V vs detached 3.292 V: +0.33%): loading cannot raise a reading, so the 11 mV is the meter's own, the Dot's rail moving, or the ground loop the probe springs close between the scope's and the Dot's USB grounds. It enters the budget as ±0.35% on V_ref, below the 1% the budget already assumed.
- **acq and opread disagree on DC by 27.6 counts on this unit** (run 1). This experiment side-stepped it by centring on the path it measures through; which path carries the "true" zero, and whether the offset is constant across ranges and channels (it was 155.4–155.8 on four pairs here), is not established. It is the kind of thing a `GET_WAVEFORM` consumer must know about.
- **One amplitude, one centring per run.** The CH1/CH2 gap at r6 moved from +3.9% (low-rail centring) to +0.9% (mid-scale centring): an ADC-code-dependent gain at the ±2% level, which one amplitude cannot separate from a row error.
- **Run 2 was cut at r7 by the CDC wedge of EXP-66** (the self-heal fired; the script does not reconnect). r8/r9 were context only.
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
| run | centring | S5 | S6 | S7 | |S6−S7|/mean | rule |
|---|---|---|---|---|---|---|
| 1 | acq (`fpga scope center`) | — | 0.936 | 0.916 | 2.2% | **VOID** (control 3: opread low rail at 155.6, CH1 compressed near the top) |
| 1b | opread servo, low rail at 128 | — | 0.915 | 0.876 | 4.4% | **edge** (S7 0.0015 inside its lower bound) |
| 2 (exploratory) | opread servo, midpoint at 128 | 0.916 | 0.909 | 0.895 | 1.6% | A-shaped |

- **Established:** at r6 and r7 the scope channels and the unit's meter path agree with "`scope_cal.c` table × one constant" at the pre-registered precision (r6/r7 agree within 4.4% in 1b, 1.6% in run 2; S6 inside 0.92 ± 3% in both); the constant that the two valid runs measure is 0.895–0.916, i.e. 0.92 within ~3%, and r5 agrees with r6 within 0.8%. The low rail must be centred on the path the measurement reads: on this unit the acq buffer and `spi3 opread` sit 27.6 counts apart in DC, and centring on the wrong one compressed CH1's span by ~4 counts at r6 (run 1 vs 1b).
- **Excluded:** "table × one constant" at the > 5% level between r6 and r7; a constant outside 0.87–0.95.
- **NOT excluded (explicitly):** that the true constant is 0.90–0.91 rather than 0.92 (both valid runs sit below 0.92, by 0.5–2.7%, within the budget); any statement in absolute volts (the reference is the DUT's own meter); per-row errors at the 2–3% level (the channel gap moved 3% with the centring); S7's own value (edge in 1b, inside in run 2).
- **Follow-up:** an external voltage reference (any calibrated DMM, or a precision reference IC) to turn these ratios into volts; `GET_WAVEFORM` / cursor consumers must know which path's zero they inherit (the 27.6-count acq/opread offset); rerun r6/r7 at two amplitudes once the Dot's source can drive a divider (separates a row error from a source constant); keep `--center-path opread` as the default for this script when the measurement reads opread.
