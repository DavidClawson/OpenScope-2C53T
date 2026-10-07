# Spec: Softkey UI

**Track:** platform
**Stage now:** **S1** (2026-10-06): P0 + P1 (meter) built and shown on unit #1. Scope (P2) not started.
**Champion:** David (design), Claude (implementation)

## What it is

The four buttons directly under the screen (MOVE, SELECT, TRIGGER, PRM, left to right) become
**softkeys**: a bar along the bottom edge of the screen labels each one with what it does
*here* and its current value (`Function / DC V`, `Trigger / Auto ▲`). Sub-menus replace the
bar, and the rightmost key is always `◂ Back`. The other keys keep one meaning everywhere:
- **CH1/CH2:** the channel (off → on + select; selected → off; opens its menu).
- **Arrows:** adjust.
- **OK ▶‖:** Run/Stop, Hold, or Output.
- **AUTO:** Autoset or auto function.
- **SAVE:** screenshot.
- **MENU:** mode.
- **POWER**

Every screen defaults to a **big, across-the-bench reading**. Every state change says what
happened in a popup, in every mode.

**Mockups:** [OpenScope softkey UI](https://claude.ai/artifact/XfcgsxJjWnw5kCNMvDAGxj): meter
(big / graph / limits), scope (time view with measurement strip, channel menu, trigger
menu) and the button map. The page is private to David until shared.

## Prior art

- **Benchtop scopes** (Rigol, Siglent, Keysight) and **handhelds** (Owon, Hantek): a softkey bar whose labels follow context; a channel button that turns its channel on, selects it, and turns it off when pressed while selected; a trigger key that opens mode/edge/source with the level on the knob; 2–5 user-chosen measurements in a strip, Vpp and Freq by default.
- **Bench DMMs:** a big main reading, with graph/trend and limit (pass/fail) views as softkey choices.

## Our angle

It fixes what the 2026-10-06 validation run and code read found (`docs/validation/findings.md`):

| Finding | What it is | Fixed by |
|---|---|---|
| U01 | No channel off | Channel button semantics |
| U02 | Debug counter on screen | Removed |
| U03 | Stats over CH2 | Measurement strip |
| U04 | TRIGGER silently toggled cursors | TRIGGER = trigger menu |
| F05 | Fake "Trig'd" | Real trigger state |
| F06 | STOP didn't stop | Run/Stop |
| F14 | Hidden menu row | Softkeys replace menus |
| F15 | Popups only in Scope | Popups in every mode |
| F18 | Stale stats | Strip refreshes every capture |
| F41 | Inert items | No label without a handler |

**The rule that makes it honest:** a softkey label exists only if pressing it does something
visible, and that's enforced by a host test.

## Hardware dependencies

None new. Constraints:
- **Static RAM:** the coldtrace image has ~110 B free, so softkey state goes on the heap (the `scope_mask` pattern).
- **Font size:** the meter's main reading needs a ~88 px digit font. Today's largest is 39 px (`scripts/generate_font.py`). That's flash only, and there's ~370 KB free.
- **Flicker:** redraws must draw over the old content instead of blanking first (the meter flash, 2026-10-06).

## Where it stands (2026-10-06)

**Built (P0 + P1):**
- **Softkey bar component** (`src/ui/softkey.{h,c}`): table-driven, `softkey_bar_valid()` enforces the label-has-a-handler rule.
- **Flicker-free text** (`font_draw_string_box`).
- **Popups in every mode:** outside the scope they're a tick-timed overlay sized to the text. The scope's own popup is unchanged.
- **Two fonts:** a 70 px tabular digit font and a 38 px unit font with Ω, µ and ° (`scripts/generate_font.py` gained `bignum`/`unit`, `--tabular`, `--spacing`).

**Meter:**
- **Big view** replaces "Full"; **Limits** (pass/fail) is new; Graph, Stats and Fuse stay, re-labelled.
- **OK = instant Hold**; **Relative** is reachable (F45).
- **Function changes run in the meter poll task:** a press is handled in 0.21 s, where it took 1.4–4.5 s (F35).
- **Continuity:** short below a threshold (default 30 Ω, softkey 10/30/50/100), with one rule for screen and beep (F12), host-tested.

**Measured on unit #1:**
- Softkey changes persist across a reboot (the first image skipped the settings hook; caught and fixed).
- Static RAM went *down* 936 B because the Full view's waveform panel was removed. New meter state is one heap block.

**Bench, unit #1, image Build Oct 6 2026 20:58:34:**
- **Final full regression: GREEN 3/3** (trigger PASS, mask 10/10, coupling 17/17).
- **On the way there:**
  - **Mask criterion H failed 2 of 3 runs.** Two quick tolerance presses landed one step short, because the acquisition task can sleep longer than the button's 100 ms ack wait. It now builds on the pending request: 3/3 mask runs plus the full run passed.
  - **One coupling run saw a cal dump differ.** It didn't reproduce in 4 runs.
  - **One full run stalled the shell for over 60 s,** during the trigger suite's centering, while the host was running five firmware builds. No reset (uptime confirms). It didn't reproduce on the clean rerun. **Unexplained**, so watch for it.
- Logs: `captures/regression/2026-10-06_Oct_6_2026_20_19_45*`, `…_p1_masktol_fix_run*`, `…_20_58_34*`.

**Open:**
- The Fuse view still blanks its whole area on every update; its redraw is due with the fuse graphics redesign.
- Graph and Stats keep their old layouts.
- The `coldtrace` image offers functions it can't measure (only DC V works there); release decision pending.

## Stage ladder

| Next stage | Criterion (checkable) |
|---|---|
| **S1 Wired** | A softkey bar component with a per-screen table (label, value, handler). Popups render in every mode. The meter's default screen is the big reading with `Function · Range · Relative · View`, and OK = Hold. Shown on unit #1. |
| **S2 Measured** | `docs/validation/screens/meter.md` and `scope-time.md`, rewritten for softkeys, run on a release image with no ✗ against the spec. Meter digit redraw shows no visible blank. |
| **S3 Guarded** | Host test: every softkey label in every table has a handler, and every handler changes state (no label-only keys: the inert-controls lesson). Popup-everywhere test. Bench regression unchanged. |
| **S4 Polished** | Every screen pass and workflow page passes on a release, community-run at least once. |

**Order of work:**
1. **P0 foundations:** softkey bar, popups everywhere, flicker-free text.
2. **P1 meter:** big reading, Graph/Limits views, Hold.
3. **P2 scope:** strip, channel and trigger menus, real Stop, cursors under Measure.
4. **P3:** Autoset, screenshot to flash, siggen and settings.

Each phase ships with its checklist pages updated and the bench regression green.
