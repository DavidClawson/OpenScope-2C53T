# EXP-68 — does unit #3's acquisition loop trigger, hold and single-shot as specified? exp22's trigger block on a Kode Dot square

- **Date:** 2026-10-02 (pre-registered before the run)
- **Unit:** unit #3 (V1.4). Every trigger-block result so far is unit #1 with a JDS6600 (EXP-54; exp22 block v13–v20, `reverse_engineering/captures/exp54/`).
- **Build:** record the `device Build:` line the script prints. Expected: evidence v3 (`feat/usb-wedge-evidence` @ `170d9f3`, `Build: Oct  1 2026 16:56:51`, the EXP-67 build), which carries the trigger-block firmware (edge filter `276b368`, LPC seam finder `659c6e8`, trigger position `c1930f4`, `ord` flag `08b647d` are all its ancestors). Scripts: `bench/signal-source-abstraction` @ `db309bf` (`exp22_stability.py --source kodedot`) plus the setup `pwm` guard committed right after this file (`exp22: put a quiet Dot back on the square`). Source: Kode Dot running `kodedot_sigsource` standalone, GPIO9 (EXP2, J3 pin 4), 0 V / 3.303 V square (EXP-64 §3).
- **Status:** OPEN

## 1. Problem
The acquisition loop's trigger policy is NORMAL advances only on a strobed handover, holds while the level is above the signal, SINGLE takes one record and AUTO falls back. It has been checked only on unit #1 with a two-channel sine generator. Does the same firmware behave the same way on unit #3? The test uses the only stimulus on this bench, one Kode Dot square pin on both probes. Which of the block's claims can such a source not test at all?

## 2. Hypothesis
Claims are numbered as in the `exp22_stability.py` docstring. Below, "strobes" means ΔPC0 edges and "ΔOK" means ΔSPI3 OK over the scenario. The script prints `commits` = ΔOK // 2. Every criterion below is the one the script already applies; none were changed for this source. The drive is a 0 V / 3.303 V square: 201 Hz where the JDS block uses a 201.2 Hz sine, and native 4 Hz and 1 Hz squares. Scenario names carry `sine-as-square` / `(square)`.

| claim | scenarios run with the Dot | prediction (as coded) | falsifier: what we see if false, and what it means |
|---|---|---|---|
| **H5 NORMAL advances** | NORMAL 0x0E / 0x10 / 0x11 / 0x12 sine-as-square (201 Hz); NORMAL square 4Hz 0x10; NORMAL square 1Hz 0x12; NORMAL resume level 0 (square) | `advancing`: ≥ 3 distinct generations in 5 grabs (unit #1: 5/5). `strobed`: strobes > 0 and ΔOK ≥ 2. `no-commit-unstrobed`: strobes ≥ ΔOK − 2. | Generations repeat on the 4 Hz / 1 Hz squares: EXP-53's freeze on slow signals is present on unit #3. ΔOK > strobes + 2: NORMAL commits records that nothing strobed. |
| **H6 NORMAL holds** | NORMAL hold above level (square), level +100 → reg 0x08 `0xFC` → record 224, above the square's top (≈ 196–202) | `hold`: one generation across 5 grabs. `no-strobe`: strobes +0. | A new generation or any strobe means the level is not in force, or the FPGA strobes without a crossing. Exception: if the control in §4 shows the signal reaching 224, this scenario is VOID, not FAIL. |
| **H7 SINGLE** | SINGLE one-shot (square) | The first grab after re-arming is > g0, and three grabs are identical. | g[0] == g0 means it never fired. Grabs that differ mean it kept acquiring. |
| **H8 record body** (0x10, 0x11, 0x12 only) | the three sine-as-square NORMAL scenarios | `body`: ≤ 1 phase-step anomaly run on all 5 records. `freq`: \|f_meas / f_Dot − 1\| ≤ 2.5 %, where f_Dot is the frequency the Dot **reports** (200.997 Hz for `f 201`). | A record with ≥ 2 runs is a torn committed record. A frequency error > 2.5 % means a wrong table rate for unit #3 at that code, or a stale record. |
| **H9 AUTO** | AUTO strobed commits (square) | `advancing`, `strobed`, and edges/commit ≥ 1.5 over 10 s (unit #1 v20: 2.03). | A ratio < 1.5 means AUTO commits fallback reads while a trigger is available. |
| **H-sq** (an observation, not a gate: the printed `square` lines) | every strobed square scenario | Edge filter kept +0 and dropped +0. Unclassified equals strobes within ±2. `ord=1` on 0 frames. A square's edges are steps as large as the seam, so trig_edge.c cannot place the seam. Unit #1 v20 agrees: unclassified 19/19 and 13/13 on its 4 Hz / 1 Hz squares. | kept/dropped > 0, or `ord=1`, at some code means the seam finders **did** place a square's seam there (edges softer than one sample after decimation). The skip reason below is then too strong for that code, and the edge scenarios could be redesigned for it. |

**Not tested, SKIPPED by the script with the reason printed** (listed as `SKIP`, never as PASS):
- **SEAM:** EDGE rising, EDGE falling, NEGCTL edge filter off, NEGCTL unrotate off, UNROTATE fast sine, NEGCTL unrotate off fast sine.
  - Their checks read the record around its seam.
  - Two seam finders are involved: the firmware's (one step ≥ 2× every other, or a unique LPC error peak) and the host's `host_seam()` (≥ 2× every other step).
  - On a 0/rail square every edge is a step as large as the seam, so neither finder can place it. The scenarios were written for a 2 Hz triangle or a sine.
- **HPOS:** HPOS x=40/160/280 and NEGCTL hpos soft anchor.
  - The hardware anchor exists only on time-ordered records.
  - The check compares the sample at the trigger column with the level (±6 counts). A square jumps past the level within one sample, so that sample is never at the level.
- **The display matrix** (claims 1–4, including its soft-trigger-OFF negative control NEGCTL free-run) is JDS-only. It needs two independent sines with amplitude and phase control. The script refuses `--source kodedot` without `--trigger-only`.

## 3. Procedure
**Wiring:** as EXP-64 §3 and EXP-67.
- Dot GPIO9 (EXP2, J3 pin 4) goes to both channels: CH1 on the crocodile lead, CH2 on the ×1 probe.
- Grounds go on the GND pair.
- `kodedot_sigsource` runs standalone in `pwm` mode; the script sets `d 50`.
- The scope shell is discovered by USB 2e3c:5740 and the Dot by Espressif VID 0x303A. If two Espressif devices are attached, pass `--source-serial`.

```
mkdir -p reverse_engineering/captures/exp22
python3 scripts/exp22_stability.py --source kodedot --v3v3 3.303 --trigger-only \
    --save reverse_engineering/captures/exp22/exp68_unit3_kodedot_frames.npz \
    2>&1 | tee reverse_engineering/captures/exp22/exp68_unit3_kodedot_2026-10-02.log
```

What the script does, in order:
1. Prints the build. Reads the Dot's status (`clk_hz`) and sets 50 % duty.
2. Prints the readback of `fpga pollgap`, `fpga postedge` and `fpga autowait`.
3. Sets `trigmode auto`, `level 0`, `timebase 10` and `vdiv 1 5`, then drives a 201 Hz square.
4. Host-centres CH1 on the acquisition record (`trig raw`, record midline target 114) and checks the precondition.
5. Runs 21 scenarios: 11 run and 10 are SKIPPED.
6. Restores unrotate on, edge filter on, edge rising, hpos 160, AUTO, level 0, timebase 0x10 and vdiv 1 6.
7. Checks the Dot's clock again and leaves it at `dc 0`.

The run takes about 3 minutes. `--v3v3 3.303` is EXP-64's reading of the rail with the probes attached (taken with the unit's own meter). It only labels the drive: **no check in this block depends on volts.**

If the run aborts, for example when the CDC self-heal of EXP-66 fires during a 7 KB `spi3 frame`: `grab_frame` has no reconnect, so the script stops. That is **not a FAIL**. Restore by hand (`fpga scope trigmode auto`, `fpga scope level 0`, `fpga scope timebase 10`, `fpga scope vdiv 1 6`) and rerun.

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| `device Build:` | evidence v3 or a later build carrying the trigger-block firmware | |
| `fpga pollgap` / `postedge` / `autowait` lines at block start | poll gap 30 ms; poll start 181 ms (derived, 0x10); AUTO edge-wait 475 ms (derived), as unit #1 v20. A `usage`/unknown-command reply means a build without the poll loop: **stop** | |
| Dot back on the square at setup (the script leaves it at `dc 0`; if `s` reports another mode it sends `pwm` and prints `Dot pin at ...`) | `mode=pwm` | |
| Dot accepts `f 201`, `f 4`, `f 1` (try by hand first; a refusal aborts the run at that scenario) | `>ok`, `hz=` ≈ 200.997 / 4.000000 / 1.000000 | |
| `host centre` line | DAC1 ≈ 1870, midline 114 ± 4. Basis: EXP-64 r5 CH1 midpoint 1753 on opread; the acq record sits 28 below opread; +14 counts × 8.2 codes/count | |
| `precondition` line: CH1 record min..max, span | ≈ 30..200, span ≈ 164–170 (EXP-64 r5 CH1 rail-to-rail 163 counts; EXP-67 +3–4 % edge overshoot). The script requires min > 5, max < 222 and span > 40; otherwise the block is **VOID** | |
| level codes in the replies | level 0 → `0x80` (record 100, inside the square's swing); level +100 → `0xFC` (record 224) | |

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| Dry run against SimBench: `python3 scripts/exp22_stability.py --source kodedot --trigger-only --dry-run` (a policy model, not evidence) | every reply parses and the run reaches a verdict | done 2026-10-02 before the run: `OVERALL: PASS -- trigger block on a square-only, single-output source: 11 scenarios run, all pass; 10 SKIPPED and NOT tested by this run (edge filter, un-rotation, trigger position)` | yes |
| `--selftest`: the tear metric on squares | clean 0 / rotated 1 / torn ≥ 2 runs; same run counts as on a sine at 201 Hz on 0x10/0x11/0x12 for ≥ 90 % of seeded pairs | 0 / 1 / 2; 169/180 | yes |
| The JDS path is unchanged by this work | same command stream and stdout as HEAD (`9d61bdf`), recorded with a fake JDS6600 on SimBench | identical in 6 argument combinations | yes |
| **Negative control of the hold metric:** NEGCTL AUTO above level (square), same level +100 | AUTO **advances** (fallback) and strobes **nothing** (+0). This proves the `hold` classifier can say "not held" and that the level sits above the signal | | |
| The signal stays below the level (readback from the same control) | max of the control's saved records (`TRIG_NEGCTL_AUTO_above_level_(square)__ch1` in the npz) < 224 | | |
| Precondition (record in range, §3) | OK | | |
| Source clock unchanged at the end (`source at end: ... PASS`) | PASS | | |

The soft-trigger-OFF negative control (NEGCTL free-run) belongs to the display matrix, which this source cannot run. The trigger block's own negative control is NEGCTL AUTO above level.

**VOID rules (fixed now):**
- A failed precondition makes the whole block VOID; the script prints `VOID`.
- A failed NEGCTL AUTO above level makes H6 VOID: the hold metric is blind, or the level is not above the signal. H5, H7, H8 and H9 are still read.
- A control record maximum ≥ 224 makes H6 VOID.
- A Dot clock change makes H8 `freq` VOID.
- A script abort means rerun, not FAIL.

## 5. Results
_Pending: the operator runs the command in §3._

## 6. Blind spots
- **Square only.** The edge filter, un-rotation and trigger position (10 scenarios) are not exercised on unit #3 at all.
  - On a square the firmware's classifier answers UNCLASSIFIED and commits unfiltered, so the scenarios that do run exercise NORMAL/SINGLE/AUTO with the edge filter effectively out of the loop.
  - A classifier defect that drops or wrongly keeps records on sines or ramps cannot show here.
- **Square for sine.**
  - Claims 5 (0x0E–0x12) and 8 were written for a 201.2 Hz sine.
  - The square crosses level 0 on a vertical edge, so the comparator's slope dependence is invisible.
  - At 0x12 (2,495 S/s, 12.4 samples per period) its harmonics alias.
  - The tear metric is shown equivalent on squares only on synthetic records: those have no edge overshoot or ringing.
- **201 Hz, not 201.2 Hz.** The source takes whole hertz. The checks use the 200.997 Hz the Dot reports.
- **One pin on both probes.** CH2 is a copy of CH1. The block judges CH1 only (trigger source CH1), so CH2 as a trigger source is untested here, as it also is in the JDS block.
- **One amplitude and one offset.** The rail is 164 counts at r5, against the JDS sine's ~100.
  - Level transfer is not tested; that is spec S2(a).
  - Level +100 is only 22–28 counts above the square's top, and edge overshoot at r5 is unmeasured on unit #3 (EXP-67 measured r6).
  - The FPGA may compare at a higher rate than the record's and see spikes the record does not. The §4 readback can only see what the record sees.
- **Unit #3 vs unit #1.**
  - DAC1 slope: 8.2 vs 8.0 codes per count.
  - The r5 gain differs.
  - The host servo starts at DAC 2400, far from unit #3's ≈ 1870. It has 8 iterations and should converge in 3–4.
  - 0x11's rate (4,990.8 S/s from the table) is unverified on unit #3; EXP-67 verified 0x0E/0x0F/0x10/0x12 to within 0.11 %.
- **Rates and latency are printed, not judged.** "Advancing" means ≥ 3 distinct generations in 5 grabs taken 0.4–1.0 s apart through the USB shell.
- **SimBench proves the flow and the parsers, not the firmware.** Its loop is a policy model: one cycle per `spi3 frame`, no seams, no timing, no classifier.

## 7. Conclusion
- **Established:** _pending_
- **Excluded:** _pending_
- **NOT excluded (explicitly):**
  - Edge filter, un-rotation and trigger-position behaviour on unit #3 (not tested).
  - Level transfer.
  - CH2 as trigger source.
  - Anything about the display matrix (claims 1–4).
- **Follow-up:** a ramp-capable source on unit #3 (the ESP32 siggen's `tri`, or the JDS6600) for the 10 skipped scenarios.
