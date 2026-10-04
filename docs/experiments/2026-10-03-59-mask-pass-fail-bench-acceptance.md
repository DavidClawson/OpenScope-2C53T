# EXP-59 — waveform pass/fail: bench acceptance on unit #1

- **Date:** 2026-10-03
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, working tree on `bench/2026-09-14` (uncommitted at
  run time). v1 Build Oct 3 2026 15:44:28 (runs 1–2); v2 Build Oct 3 2026 16:57:43 (runs 3–4)
- **Status:** **CONFIRMED** on v2: 9/9 pre-stated criteria, twice. v1 failed one
  criterion, a real defect, fixed in v2 (§5). **Addendum (v3, Build Oct 3 2026
  17:21:19):** 10/10 with the button criterion H added; bounds drawn; a two-channel
  mask gave 0 false fails in 143 records (§8).

## 1. Problem
Mask testing (`docs/specs/scope/mask-pass-fail.md`) was host-tested on real records
(`tests/test_mask_pf.c`) but had never run on a device. The host test cannot show:
- whether the acquisition-task hook judges every live commit;
- whether "refuse, don't score" holds with real settings changes;
- whether stop-on-fail freezes and releases the live engine;
- whether the overlay renders.

## 2. Hypothesis
With a 201.2 Hz, 2.0 Vpp sine taught at default tolerance (8 counts / 2 samples), the
unit passes the unchanged signal (false-fail ≤ 1/30) and fails +30% amplitude and +5%
frequency on ≥ 95% of records. A trigger-level change gives SKIPs, not scores.
Stop-on-fail freezes judging and releases it. No commit goes unjudged.

**Falsifiers:**
- a baseline fail rate above 1/30;
- a fault window under 95% fail;
- any record scored after the level moved;
- judging that advances during a hold;
- `missed` ≠ 0.

## 3. Procedure
`scripts/mask_bench.py` (criteria fixed in its docstring before run 1):
- **Scope:** range 5 CH1, level 0, NORMAL, timebase 0x10, hpos 160.
- **Source:** JDS6600 CH1, sine 201.2 Hz, 2.0 Vpp, 0 V offset.
- **Teach:** `mask teach 8 ch1`, `mask tol 8 2`.
- **Windows, each after `mask reset`:**
  - baseline: 40 records;
  - 2.6 Vpp, then back to 2.0;
  - 211.3 Hz;
  - `fpga scope level 10`, then 0;
  - stop-on-fail ON + 2.6 Vpp, hold 2 s, restore, `mask run`.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| image on device | Build line of the flashed image | `Oct 3 2026 15:44:28` / `16:57:43` |
| `mask` command present | in `help` | yes |
| taught settings | range 5, tb 0x10, trig code 128, rising, CH1 | `range 5/6 … tb 0x10 trig code 128 rising src CH1` |
| heap after teach | > 0 free | 11,504–11,512 B |

## 4. Control
The fault windows are the negative control for the baseline; the restore windows are
the control for the fault windows. The level-change window is the control for "refuse".

| control | expected | measured (run 3 / run 4) | passed? |
|---|---|---|---|
| unchanged signal | ~all pass | 42/42, 40/40 | yes |
| +30% amplitude | ~all fail | 23/23, 20/20 | yes |
| amplitude restored | ~all pass | 23/23, 20/20 | yes |

## 5. Results

**Run 1 (v1): VOID.** The script's state parser matched the shell's echo of
`mask status` and read the state as "status". The device was in fact READY with 44/44
passing. Script bug, fixed (anchored regex), no firmware change.

**Run 2 (v1): 8/9.** F failed:
- The hold engaged and froze judging (3 → 3), and the release resumed it.
- But the unit was **holding again** 3 s after release.

Traced on the device:
- Gen 748 failed → hold.
- Amplitude restored for 2 s → release.
- **Gen 750, the very next commit, failed** and re-held.
- A second release: 752+ passed.

The FPGA keeps a completed record until it is read (EXP-53/54). The hold stops the
reads, so the first read after release returns the record captured just after the
failing one, *during the fault*. The criterion encodes the intended behaviour (OK means
resume from now), so the firmware changed, not the criterion. v2 marks that one record
`stale` and SKIPs it. It is not silently dropped and not scored.

**Runs 3 and 4 (v2): 9/9 both.**

| criterion | run 3 | run 4 |
|---|---|---|
| A teach | READY 2.8 s, spread 8 | READY 2.3 s, spread 10 |
| B baseline | 42 tested, 0 fail, 0 skip | 40 tested, 0 fail, 2 skip (not time-ordered) |
| C +30% / restored | 23/23 fail / 23/23 pass | 20/20 fail / 20/20 pass |
| D +5% freq | 20/20 fail | 20/20 fail |
| E level moved / restored | 0 scored, 14 skipped / 12/12 pass | 0 scored, 14 skipped / 11/11 pass |
| F hold | 2→2 held, 13 after release, `stale`=1, not re-held | same |
| G missed commits | 0 | 0 |

**Screen (one held FAIL, CRC-verified dump, `captures/exp59/mask_fail_held.png`):**
- the trace is red at the peaks and troughs, which is where +30% amplitude leaves the
  mask;
- the strip is solid red under those columns and dotted red elsewhere;
- the readout says `FAIL 1/10 HOLD-OK`.

Logs: `reverse_engineering/captures/exp59/mask_bench_run{1..4}.log`.

## 6. Blind spots
- **One signal, one range, one timebase.** The sensitivity figures (spec: +18% / −20%
  amplitude, 20-count glitch) belong to this signal's slope and amplitude.
- **The JDS is the only fault source.** No real intermittent glitch (a runt, a dropout)
  was injected. Those are the use case, and the source cannot make them on demand.
- **"Not time-ordered" skips (0–2 per window)** are un-rotation refusing a record. The
  run shows they are rare at this signal, not why they happen.
- **CH2 masks and two-channel masks** have host coverage only.
- **The PASS-strip rendering was not captured.** A live screen fails the dump's CRC.
  Only the held FAIL screen was dumped.
- **Buttons were not exercised.** Everything went through the shell, except that OK →
  release shares its code path with `mask run`, and OK itself was not pressed.

## 7. Conclusion
- **Established:**
  - mask teach / test / refuse / hold work end to end on unit #1, reproducibly (2 × 9/9);
  - every commit is judged exactly once (`missed` 0);
  - a settings change is refused, not scored;
  - the overlay renders on the failing columns.
- **Excluded:** a false-fail rate above ~1/40 on a steady signal at default tolerance.
- **NOT excluded:** false fails on noisier signals, other ranges, or fast timebases where
  un-rotation refuses more often.
- **Fixed by this experiment:** the stale-record-after-hold defect (v1 → v2).
- **Follow-up:**
  - button UI (spec open question 1);
  - bounds drawn on the trace (open question 2);
  - fold `mask_bench.py` into the regression set (S3);
  - a glitch source for the real use case.

## 8. Addendum — v3: buttons, drawn bounds, two channels (same day)

v3 adds:
- **AUTO** in the scope time view: teach, cancel, or clear.
- A fourth **MOVE** stage, "Mask" (only while a mask exists): UP/DOWN set tol_v in
  2-count steps, LEFT/RIGHT set tol_h, SELECT toggles stop-on-fail.
- The mask **bounds drawn** as dotted lines. The shared autofit now spans the mask's
  extent, for every caller.
- A `btn <name>` shell command that injects presses into the key-scan queue.

**Criterion H** was added to `mask_bench.py` before the v3 run. Its control is the
Position stage: there, RIGHT must move the trigger column and must not touch the mask.

**Run 5: 10/10.** H read:

    auto->READY; position: col 160->176 tol (8, 2); mask focus: tol (12, 3) col 160;
    select->stop True; ok: held True released True; auto->EMPTY focus vdiv

This shows:
- the same RIGHT press goes to the position in one stage and to the mask in the next,
  never to both;
- popups report the tolerance the mask holds after the acknowledgement.

**Two-channel mask.** AUTO teaches every enabled channel, and CH2 had no input (a noisy
open baseline, `CH2pp 77mV`). Over 45 s at default tolerance: **143 tested, 143 pass, 0
fail**, 7 not time-ordered. So an unconnected CH2 in the mask does not cause false fails
on this unit.

**Screens** (CRC-verified dumps):
- `mask_pass_single.png`: a SINGLE-held PASS frame. Dotted bounds hug the sine, the
  strip is solid green, the readout says `P11 F0`.
- `mask_fail_bounds.png`: a held FAIL. The trace leaves the dotted bounds at the peaks
  and troughs, exactly those stretches are red, and the strip is red under them. The
  readout says `FAIL 1/17 HOLD-OK`.

**Blind spots added:** the physical buttons were not pressed. `btn` enters below the key
scan, whose 15/15 mapping is hardware-confirmed separately. And the two-channel result
covers one CH2 state (open input) only.
