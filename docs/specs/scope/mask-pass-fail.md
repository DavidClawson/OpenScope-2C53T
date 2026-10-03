# Spec: Waveform pass/fail (mask testing)

**Track:** scope
**Stage now:** **S2** (EXP-59, 2026-10-03). On unit #1, `scripts/mask_bench.py`
passed 9/9 pre-stated criteria twice. The first device run found and fixed a real defect
(a stale record judged after a stop-on-fail release). Not S3: the bench script is not in
the regression set yet. Not reachable from the buttons except OK (release).
**Champion:** David / Claude (2026-10-03)

## What it is

Show the scope a known-good waveform, press teach, and from then on every
captured record is judged against it: **PASS**, **FAIL** (with the failing
columns of the trace drawn red), or **SKIP, and why**. The instrument keeps
pass/fail/skip counts, and can **stop on the first failure** and hold the
offending record on the glass. This is the "catch the intermittent glitch" and
"is this the same as the good one?" tool.

## Prior art

Every current bench scope in the class has it: Rigol "Pass/Fail", Siglent
"Mask Test", Keysight "Mask Test". The common shape is a mask made from the
current waveform plus X and Y tolerances in divisions, a stop-on-fail option,
and pass/fail counters. Stock 2C53T has nothing like it. The feature catalog
lists it ("Mask / limit testing", medium), and the industry-module vision
leans on it ("Pass/fail — green/red against industry standards",
`docs/ideas/feature_catalog.md`). A guided procedure saying "compare against
the known-good CAN waveform" is a mask.

`src/tasks/mask_test.c` (2026-03-21, removed by this work) predates real
captures. It had **zero callers**, applied a tolerance as a percentage of a
*signed* value to unsigned ADC codes (so the margin scaled with DC offset),
and indexed by screen column rather than by trigger.

## Our angle

1. **Teach from several records, not one.** The envelope of N known-good
   records (default 8) captures the instrument's own noise and the trigger's
   jitter. The tolerance is then a margin on top of measured variation, not a
   guess at it.
2. **Refuse instead of guess.** A record that is not strobed, not time-ordered,
   has no trigger crossing near index 512, or was taken under different
   settings (range, coupling, timebase, trigger level, edge or source, read
   from the codes *in force*, per EXP-17) is SKIPPED and counted by reason,
   never scored. A pass rate counts only comparable records. The screen says
   `MASK skip: settings` rather than freezing a stale PASS count, which is the
   project's signature stable-plausible-wrong number.
3. **Scriptable.** The `mask` shell command plus `bench.py` make "does it catch
   a 30% amplitude fault?" a test anyone can re-run.

## Design

- **Alignment: the hardware trigger, not the screen.** The mask spans 320
  samples starting `pre` samples before `trig_edge_anchor()`. That is the same
  function the display uses, moved out of `scope_ui.c` so the two cannot
  disagree. `pre` is the trigger's screen column at teach time, so the mask
  covers what was on the glass, but moving the window later does not move
  what is tested.
- **Units are the record's own:** ADC counts vertically, samples
  horizontally. Volts and seconds never enter a verdict, so a calibration
  change cannot silently move a mask.
- **Bounds** = envelope extreme within ±`tol_h` samples, widened by `tol_v`
  counts. The envelope is taught over the span plus a 16-sample margin each
  side, so the bounds at the span edges see real data. A bound that reaches a
  rail is reported (`RAIL-LIMITED`), because excursions past the rail are then
  invisible.
- **Defaults: `tol_v` 8 counts, `tol_h` 2 samples.** Horizontal slack is
  deliberately small: it widens the vertical bound on every slope by
  slope × `tol_h`. The first host run at 8 samples let a 20-count glitch and a
  −30% amplitude change through on a 201 Hz sine at 12.5 kS/s. The data
  justify 2 samples: the anchor jitters by about 1 sample (508..510 across 33
  records), and the same signal re-captured minutes later stays within 4
  counts of the envelope with no horizontal slack at all.
- **Where it runs.** In the acquisition task, right after each commit. Every
  record is judged exactly once, from the published buffers while nothing else
  writes them, and the display path does no analysis (inline analysis there
  killed acquisition once; see the 2026-08-14 note in `scope_ui.c`). Requests
  from the shell and buttons go through a sequence-numbered mailbox and are
  acknowledged; the shell says `NOT APPLIED` if no acknowledgement arrives.
- **Memory.** The mask is one ~1.6 KB FreeRTOS heap block, taken on the first
  teach and never freed. Static RAM in `guest-coldtrace` had about 110 bytes
  free when this was written. If the heap cannot supply the block, teaching is
  refused with a reason.
- **Screen.** Trace pixels on failing columns turn red. A 3-px strip at the
  bottom of the live band shows the mask span: green = passed, red = this
  column failed, dotted red = the record failed elsewhere, dotted amber =
  refused, amber = teaching. It is painted only when the window sits on the
  hardware anchor, and only for the record generation that was judged. The
  counts take over the legend slot in badge row 2, refreshed on their own
  epoch (like the info bar), so they never wait on a full repaint.
- **Stop on fail** (default ON) holds the acquisition the way SINGLE does, so
  the failing record stays in the buffers. OK releases the hold instead of
  toggling RUN/STOP. The first record after a release is SKIPPED as `stale`:
  the FPGA held it through the hold, so it was captured during the fault, not
  after OK (EXP-59 found it re-holding on exactly that record).

### Measured sensitivity (host, real unit #1 records, default tolerance)

| Fault | Smallest that fails all 20 held-out records |
|---|---|
| Amplitude | +18% / −20% |
| One-sample glitch, mid-slope column | 20 counts |
| Time shift | 8 samples fail; ±1 passes |
| Same drive at the next timebase (0x11) | fails by 88 counts |

These are properties of *this* signal (a ±50-count sine, 62 samples/period),
not of the feature: sensitivity is roughly `tol_v` + slope × `tol_h` +
teach spread. Tighten with `mask tol`.

## Hardware dependencies

- Time-ordered records with the trigger at index 512 (un-rotation, dev plan
  2.3; v15+). Records the seam finder cannot place are SKIPPED (`no seam`), so
  fast periodic signals where un-rotation fails are refused, not scored.
- The hardware anchor is measured on CH1 (EXP-55/56), so a CH2 trigger source
  is refused at teach time. A mask may still *cover* CH2: both channels share
  the FPGA's write pointer.
- Untriggered AUTO free-run commits are refused. For anything to be judged
  under AUTO, the signal must actually cross the level.

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| ~~S1~~ | ✓ EXP-59. The mask image runs on unit #1: `mask teach` reaches READY on a live signal, the strip and red tint appear, and `OK:` keeps climbing with a mask active. |
| ~~S2~~ | ✓ EXP-59 (2 × 9/9). `scripts/mask_bench.py` passes A–G on unit #1 against the JDS6600 (baseline false-fail ≤ 1/30 with ≥ 80% of commits judged; +30% amplitude and +5% frequency ≥ 95% FAIL; level change → SKIP not scored; stop-on-fail freezes and releases; no missed commits), written up in `docs/experiments/`. |
| S3 | `test_mask_pf` (held-out records + negative controls, in the build) **and** `mask_bench.py` in the regression set, with its negative controls (fault windows) passing. |
| S4 | Reachable from the buttons alone (teach, tolerance, stop-on-fail, clear); bounds drawn on the trace; refusal reasons legible on screen; masks persisted to W25Q and reloadable by a module. |

## Open questions

1. **Button UI.** SAVE in scope mode currently shows `SAVED #n` and writes
   nothing (audit P3, dev plan 2.6). Options: give SAVE to mask teach in scope
   mode until screenshots are real, add a MOVE-cycle stage, or use a Settings
   page. Proposal: a Settings → Mask page for teach/tolerance/stop, with OK as
   the run/release key it already is.
2. **Drawing the bounds.** The default autofit transform rescales to the
   trace's own min/max, so bounds 8 counts outside the trace clamp to the band
   edge. Drawing them needs autofit to include the mask extent, which is one
   change to `autofit_prep()`, used by both renderers.
3. **Soft-anchored records.** Fast signals where un-rotation fails are refused
   today. Could the display's soft anchor be trusted for masks? Only with a
   held-out test showing it is as stable as the hardware anchor.
4. **Beep on fail.** `continuity_buzzer_force_ms()` exists; is it alive in the
   coldtrace image?
