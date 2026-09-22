# Spec: Trigger modes — auto / normal / single

**Track:** scope
**Stage now:** **S0.** Milestone M2, not started. The README matrix has a
*Trigger level* row (S1) and no trigger-*mode* row; this spec is the ladder for
the mode/edge/source/holdoff feature that row does not cover. S0 is the honest
reading even though `scope_state_t.trigger` exists and one acquisition-task
branch reads it: the controls a user can press do not reach the hardware, and
the one quantity that does reach hardware (SPI3 register `0x08`) has no control
surface at all.
**Champion:** —

## What it is

Press a button and pick how the scope decides when to draw: **Auto** (free-run,
never blank), **Normal** (draw only on a real trigger event, hold the last trace
otherwise), **Single** (arm once, capture one record, stop). Pick the **edge**
(rising/falling), the **source** (CH1/CH2), and move a **level** marker that
means something. The instrument then tells the truth about what it is doing —
armed, triggered, stopped — instead of restating which mode you selected.

This is [`docs/community_wishlist.md`](../../community_wishlist.md) **Tier 1
item 1**, the highest-recurrence complaint about stock across the whole model
family, and the one owners describe as making Normal mode unusable.

## Prior art

Stock has all three modes and they are what people complain about: Normal
"reverting to Auto mode at 50 ms and up", and "even at sweep rates where it's
supposed to be working, it will mostly miss the trigger events" (2C53T,
@sadfur8728); Normal failing on a 1 PPS GPS pulse (2C23T, @pixlewing, +18);
FNIRSI's own DSO152 tutorial shipping a four-step manual workaround for "unable
to trigger". Every bench scope in the class has the three modes plus holdoff and
a force-trigger key. This is table stakes, and stock's version being poor is the
*reason* it is the top ask.

## Our angle

Two things stock cannot offer. First, **the modes are scriptable**: the USB CDC
shell and `scripts/bench.py` mean "does Normal actually hold?" becomes a
measurement someone can run and publish, not a forum argument. Second,
**measure-or-refuse** applies to the trigger badge exactly as it now applies to
the volts/div and time/div labels: a badge that says `Trig'd` when nothing has
triggered is the same defect as a label that says `1V/div` when the range was
never measured, and this project has already paid for that four times over.

## What exists today, precisely

Read this before proposing an implementation. Three distinct things are
entangled under the word "trigger", and only one of them is a trigger.

**1. A real hardware trigger, in the fabric, with no control surface.**
SPI3 register `0x08` is a **digital post-ADC level** compared against ADC codes,
proven both ways on the bench: quiet input with the stock arm value `08 AD`
(level 173) gives 0 PC0 edges/s; `08 37` (level 55, inside the noise band) gives
4.6 edges/s; restoring `08 AD` gives 0 again
([`trigger_regime_findings_2026-08-14.md`](../../../reverse_engineering/analysis_v120/trigger_regime_findings_2026-08-14.md)
§ Addendum 1). PC0 (EXINT0 falling edges, counted into `fpga.pc0_edges`) is the
trigger-event observable. The register is written **once, as the constant
`0xAD`**, by the engine-arm block — `firmware/src/drivers/fpga.c:5211`, with the
same constant at `fpga.c:4184`, `fpga.c:4685`, `fpga.c:5344` and
`usb_debug.c:6239`. Nothing derives it from UI state. The only way to move it is
`spi3 seq 08 XX` from the shell (`scripts/bench.py:571`, `Scope.trigger_level()`).

**2. A software display trigger, in the renderer.**
`scope_soft_trigger_offset()` (`firmware/src/ui/scope_ui.c:828`) searches the
captured record for a level crossing and shifts the *drawn window*, both channels
by the same offset. It never changes what was captured. The threshold is the
record's own midline `(min+max)/2` plus `ss->trigger.level` (`scope_ui.c:874`);
edge comes from `ss->trigger.edge` (`scope_ui.c:881`); source from
`ss->trigger.source` (`scope_ui.c:933`). This is what EXP-22 measured at 11/11
scenarios locking to ≤1 px, and it is the only trigger the screen obeys today.

**3. An acquisition-task wait policy, which reads exactly one field.**
`fpga_warmtest_acq_task()` (`fpga.c:3506`) branches on `ss->trigger.mode` at
`fpga.c:3562`: `TRIG_AUTO` waits `FPGA_AUTO_TRIG_WAIT_MS` for a PC0 edge then
free-runs anyway; anything else waits `FPGA_NORMAL_TRIG_WAIT_MS` and on timeout
**holds** the last trace (`fpga.c:3583-3588`). That much is genuine, reaches real
hardware, and is the one part of the mode selector that is not decorative.

## Known defects — all present in `main`, none to be fixed by this spec

Documented here so the ladder has something falsifiable to close, and because
this list *is* the reason the spec exists. Do not fix these in the spec PR.

| # | Defect | Evidence |
|---|---|---|
| D1 | **Trigger level is immutable at runtime.** `scope_adjust_trigger_level()` has **zero callers** anywhere in `firmware/src/` — no button, no shell command. `ss->trigger.level` is set to 0 at `scope_state.c:78` and otherwise only restored from saved config (`settings_store.c:125`). The on-screen trigger marker and its dotted line (`scope_ui.c:147`) are therefore pinned to mid-screen forever, and the soft-trigger threshold is always exactly the 50% level. | `scope_state.c:183`, no callers |
| D2 | **Trigger source is unreachable.** `scope_cycle_trigger_source()` (`scope_state.c:142`) also has **zero callers**. The renderer honours `ss->trigger.source` (`scope_ui.c:933`) and `spi3 frame` reports it (`usb_debug.c:3588`), but nothing can change it. | `scope_state.c:142`, no callers |
| D3 | **The mode and edge buttons are decorative with respect to hardware.** `scope_cycle_trigger_mode()` / `scope_cycle_trigger_edge()` (`scope_state.c:132`, `scope_state.c:137`) mutate the struct and return; the button paths (`input_handler.c:302`, `input_handler.c:313`, settings sub-menu `input_handler.c:150-151`) then show a popup. Neither writes a register, neither checks that anything took. Mode reaches the acq-task wait policy; **edge reaches nothing but the renderer**. | `input_handler.c:302`, `:313` |
| D4 | **The only path that ever encoded mode/edge/source for the hardware is dead in the shipping build, twice over.** `fpga_scope_trigger_mode_byte()` (`fpga.c:2012`) packs source/edge/mode into a byte and `fpga_scope_trigger_lsb()` (`fpga.c:2002`) packs the level; both are consumed only by `fpga_send_scope_runtime_blocks()` (`fpga.c:1334-1338`) and `fpga_send_scope_sequence()` (`fpga.c:2122`), reached from `fpga_scope_reinit()` (`fpga.c:5952`) — which **returns immediately under `FPGA_WARM_HANDOFF_TEST`** (`fpga.c:5954-5958`), the flag `guest-coldtrace` sets (`firmware/Makefile:918`). Even if it ran, it transmits on USART2, which the same build holds electrically dark (`FPGA_USART_SILENT_SCOPE=1`), and the frame it would build carries the `00 00` header the meter SoC discards unparsed (`fpga.c:1204`, EXP-25/26). | `fpga.c:5954`, `Makefile:918` |
| D5 | **`Single` is `Normal`.** The acq task's only mode test is `mode == TRIG_AUTO` (`fpga.c:3562`). `TRIG_SINGLE` takes the same branch as `TRIG_NORMAL` and never stops after one record. The badge nonetheless prints a distinct label for it. | `fpga.c:3562` |
| D6 | **The trigger badge is a pure function of the mode enum, not of trigger state.** `draw_trigger_status()` (`scope_ui.c:232`) prints `Trig'd` whenever mode is Normal and `ss->running` is set, and `Ready` whenever mode is Single — with no signal, no trigger and no acquisition. It cannot report "armed, waiting" or "lost trigger". This is the badge-shaped version of the label that was never measured. | `scope_ui.c:237-249` |
| D7 | **RUN/STOP does not stop the live trace.** `ss->running` is honoured by the badge (`scope_ui.c:237`, `:266`) and by the *demo* waveform freeze (`scope_ui.c:902`), but the real-data render path (`scope_ui.c:913-961`) and the acquisition task never test it. The screen says `STOP` while the trace keeps updating. Adjacent to this spec because STOP is part of the same control cluster: claim it here or in its own spec, but do not leave it unclaimed. | `scope_ui.c:902` vs `:913` |
| D8 | **`scope_trigger.c` is named for a job it does not do.** The file implements stock's DAC1/PA4 path as a "trigger comparator"; DAC1 is the CH1 **vertical offset** injector (bench-proven, `trigger_regime_findings_2026-08-14.md` § Addendum 1 corollary). `usb_debug.c:1875-1878` already carries the correction inline while the driver's own header still asserts the wrong model. A rename is not this spec's job, but no trigger work should be built on that file by mistake. | `scope_trigger.h:1-20` |

## Hardware dependencies

- **Level: solved and measured.** Register `0x08`, digital, ADC codes,
  offset-binary (`fpga.c:5329`). PC0/EXINT0 edge counting gives a trigger-event
  rate without a logic analyzer.
- **Edge, mode and source in the fabric: UNKNOWN.** No register has been
  identified. Stock's candidate is the USART2 `0x16`–`0x19` block, which this
  build cannot use (D4). Until one is found, edge/mode/source can only be
  implemented in software on top of the hardware level — a legitimate answer,
  but one that must be *stated on the badge*, not hidden.
- **Sample rate:** only the measured timebase codes in
  `firmware/src/ui/scope_timebase.c` have a real seconds axis. The wishlist
  complaint is explicitly *per timebase*, so any "does Normal work?" measurement
  must be reported per code and must show `--` on unmeasured ones.
- **The acquisition record seam (M3) interacts directly.** The record is not
  time-contiguous at its edges: stale data roughly one read cadence old sits at
  indices ~32–96 and ~864–928 (EXP-22,
  [`docs/experiments/2026-09-03-22-display-stability.md`](../../experiments/2026-09-03-22-display-stability.md)).
  The display trigger steps over it with `SEAM_GUARD = 128` (`scope_ui.c:852`),
  which costs 128 of the 1024 samples and is explicitly labelled a symptom fix.
  **A software trigger that searches the whole record searches stale data**, and
  a crossing fabricated at a seam locks the trace to an arbitrary phase — the
  exact failure EXP-22 saw as frames flipping 180°. Any change that widens the
  search window, or that hands a hardware trigger a record boundary to respect,
  must re-run the EXP-22 lock metric rather than assume it still passes.
- **Build:** all of this is only exercisable in `guest-coldtrace`
  (`firmware/Makefile:918`), which is also the only build family whose USB CDC
  reliably enumerates.

## The pattern every trigger control must follow

Non-negotiable, and the reason this spec is written the way it is. Four
decorative controls are on record in 2026 — the timebase button (EXP-17), the
meter word table, the meter poll cadence, and the meter header toggle — each of
which moved a variable nothing read while reporting success. The fix pattern is
already in the tree, in `fpga_apply_timebase()` (`fpga.c:3404`):

1. **One entry point per quantity** — `fpga_apply_trigger_level()`,
   `fpga_apply_trigger_mode()`, and so on. Nothing else writes the register;
   nothing else updates the "in force" copy. If you add a second transmit path,
   call the first one — the rule `meter_build_tx_frame()` earned the expensive
   way (`fpga.c:1188-1198`).
2. **It writes the hardware, then records what is in force**, in a variable that
   mirrors the register — the `acq_rate_idx` role (`fpga.c:3326`).
3. **It parks the acquisition task first** (`fpga_acq_pause()`) and **returns
   `false` without writing** if it cannot, rather than tearing a CS frame.
4. **The UI refuses to lie on failure**: the popup reads `NOT SET`, exactly as
   the timebase button does (`input_handler.c:686`).
5. **The shell prints both numbers and refuses when they disagree**, as
   `fpga scope freq` does (`usb_debug.c:2732-2745`).

A criterion below is not met by code that follows this pattern. It is met by a
bench run showing the hardware moved.

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| **S1** | **(a)** A single entry point per trigger quantity exists and is the only writer, per the pattern above; `scope_adjust_trigger_level()` (D1) and `scope_cycle_trigger_source()` (D2) either gain callers through it or are deleted. **(b)** `fpga scope trigger` prints UI state *and* the in-force hardware value for every quantity, and refuses when they disagree. **(c) The bench proof that the control reached the hardware, driven through the UI/shell control and not through a raw `spi3 seq`:** quiet input, A/B/A on the trigger-level control — `0xAD` → 0 PC0 edges/s, a level inside the noise band → non-zero edges/s, back to `0xAD` → 0 again, reproducing § Addendum 1 *through the new path*. A run where the edge rate does not move is a failure, not a null result. **(d)** `Single` is one-shot and distinguishable from `Normal`: one arm produces exactly one increment of `spi3 frame gen=` and the next does not arrive until re-armed (closes D5). **(e)** Whichever of edge/mode/source cannot reach the fabric is implemented in software and *labelled as software on the badge* — never presented as a hardware trigger. |
| **S2** | Measured on the bench, one writeup in [`docs/experiments/`](../../experiments/), covering all four: **(a) level transfer** — sweep register `0x08` against a known-amplitude sine; the code at which triggering starts and stops matches the commanded ADC code within a stated tolerance, and that tolerance is derived rather than chosen after the fact. This also promotes the README's *Trigger level* row to S2, which is its stated Next. **(b) Normal holds** — with the signal below the level, `spi3 frame gen=` must **stop advancing**; raise the amplitude above the level and it must resume — same boot, one variable. **(c) Edge selects phase** — rising vs falling on a symmetric waveform shifts the locked phase by 180° ± `PHASE_TOL_DEG`, measured with `exp22_stability.py`'s existing `rel_phase_deg()`. **(d) The wishlist complaint, answered with a number** — capture success rate in Normal, **per measured timebase code**, against a fixed-rate periodic drive: the fraction of arms producing a fresh generation, reported per code, `--` on unmeasured codes. Stock's documented failure is "mostly miss the trigger events" and "reverts to Auto at 50 ms and up"; if we cannot state our own rate per code then we have not beaten it, only claimed to. |
| **S3** | **Guarded by extending `scripts/exp22_stability.py` — do not write a second harness.** Specifically: **(i)** add `trigger()` to `Scope` in `scripts/bench.py` next to `timebase()` / `trigger_level()` (`bench.py:549`, `bench.py:571`), driving the single entry point, with a `trigger_raw()` sibling that writes the register directly — the `timebase()` / `timebase_raw()` split (`bench.py:563`), kept so the divergence can be tested *on purpose*. **(ii)** add `trig_mode` / `trig_edge` / `trig_source` / `trig_level` keys to the `base` dict in `scenarios()` (`exp22_stability.py:177`), defaulting to today's behaviour, and apply them in `apply_scenario()` (`exp22_stability.py:199`) with the same send-only-on-change guard the timebase uses. **(iii)** add scenarios: `normal below level` (expect hold), `normal above level` (expect advance), `single one-shot`, `edge falling`. **(iv)** add checks to `eval_scenario()` (`exp22_stability.py:222`): `hold` (generations do **not** advance), `single` (exactly one), `edge-flip` (180° via the existing `rel_phase_deg()` at `exp22_stability.py:144`). **(v) negative controls, matching the existing `NEGCTL free-run` discipline** (`exp22_stability.py:194`) — the `hold` check must FAIL against a build without the hold, and `edge-flip` must FAIL when edge is not applied; a check that cannot fail proves nothing (held-out-sets lesson, EXP-17). **(vi)** extend `selftest()` (`exp22_stability.py:329`) so the new metric math runs with no hardware, as the existing metrics do. **(vii)** a host test `firmware/tests/test_scope_trigger.c` pinning the mode/edge/source → register-byte mapping, wired into the same `make test-*` set. |
| **S4** | **(a)** The badge reports trigger **state**, not the mode enum (closes D6): it must be demonstrable on the bench that Normal with no signal reads armed/waiting and **not** `Trig'd`, and that the state changes when the signal appears. **(b)** The level marker moves with the level and is labelled in the measured volts of the current range on calibrated ranges, `--` elsewhere — the rule the volts/div labels already follow. **(c)** RUN/STOP actually stops the live trace (closes D7). **(d)** Holdoff and a force-trigger key. **(e)** `scope_trigger.c` renamed to what it does (closes D8). |

## Open questions

1. **Do edge and mode exist as fabric registers at all?** Three options, and the
   choice shapes everything: **(a)** find them with an SPI3 register sweep, the
   way `0x01` and `0x08` were found; **(b)** bring USART2 up in `guest-coldtrace`
   (`guest-coldtrace-usart` exists, `firmware/Makefile:914`) with the `AA 55`
   header and use stock's `0x16`–`0x19` block — but that is gated behind the
   meter word table per `docs/dev_plan_2026-09-12.md` § 0; **(c)** keep level in
   hardware and do edge/mode in software. **(c) is shippable now and is the
   honest default**, provided the badge says so.
2. **How should Normal hold — skip the read, or read and refuse to commit?**
   Today it skips (`fpga.c:3583-3588`) and counts a timeout, which makes the
   `TO:` overlay climb during *correct* behaviour. Weigh that against
   read-and-discard, which costs SPI time but keeps the buffer fresh for the
   moment a trigger arrives.
3. **Does the software display trigger survive a working hardware trigger?**
   Two triggers layered is the two-renderers shape this repo has been bitten by.
   Proposal: keep it, scoped to Auto only, and disable it in Normal/Single where
   the hardware defines the record start. That is a design call, not a fact.
4. **Does this spec block on M3 (the record seam)?** For: a hardware trigger that
   defines the record start makes `SEAM_GUARD` either unnecessary or wrong, and
   the 128 samples it costs are 12.5% of the record. Against: the level control
   (S1) and the level transfer (S2a) are independent of the seam and are the two
   highest-value items here. Current position: **do not block**, but re-run the
   EXP-22 lock metric on every change.
5. **Pre-trigger / horizontal position** is deliberately out of scope — the
   catalog lists it as a research question (does the fabric's ring buffer support
   it?) and it needs an answer before it can have a ladder.

## Bench facts for S2, measured 2026-09-22 (EXP-53, unit #1, range 5)

These belong to S2 (a) and (c) and were found while characterising the record, not by
running the S2 procedure; they are recorded here so the procedure is written against them.

- **(a) Level transfer.** The comparator sees the signal **28 codes above the record**: on
  a triangle the record shows spanning 33..133, level 152 triggers and level 54 does not,
  and in every triggered record a crossing of (level − 28) sits 504–523 samples before the
  write pointer. Measured on range 5 only. The UI marker must be drawn at (code − 28) in
  record units, or the marker will sit 28 counts above where the hardware fires.
- **(c) Edge.** **The FPGA fires on either polarity.** The crossing before the pointer is
  rising in some records and falling in others at the same level, at every level tried.
  Whether reg 0x02 (stock writes `02 03` at boot) selects a polarity is untested; until
  it is, the edge button is software-only and must be labelled so (S1 (e)).
- **Trigger position.** 512 post-trigger samples: the trigger is at mid-record, at
  (pointer − 512) mod 1024. PC0 is a handover strobe on the read, not a completion flag
  (EXP-53 postscript, EXP-54); the acquisition loop polls for it.
- **Superseding the seam paragraph above:** the record is a rotation of a continuous
  1024-sample segment with one seam at the FPGA's write pointer; there is no stale head or
  tail. `SEAM_GUARD` still works as a display measure because the soft trigger searches
  past it; it is not a model of the record.
