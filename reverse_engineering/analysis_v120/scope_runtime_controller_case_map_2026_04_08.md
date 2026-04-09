# Scope Runtime Controller Case Map

Date: 2026-04-08

Purpose:
- decode the top-level jump-table cases inside the broad scope runtime controller
  at `0x08004D60`
- separate stable scope editor states from transient handoff states
- connect the newly found case map back to the already-mapped `state 5 / 2 / 1`
  runtime families

Primary references:
- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md)
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)
- [mode_scope_state_cluster_08006840_080076A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006840_080076A0_2026_04_08.txt)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)

## Executive Summary

The broad controller at `0x08004D60` now has a concrete top-level case map.

Using the `TBH` table at `0x08004D78`, the visible low-byte states dispatch to:

| Visible state (`DAT_20001060`) | Target | Best current read |
|---|---|---|
| `0` | `0x08004D90` | packed-state promotion / reset setup |
| `1` | `0x08004E24` | mixed cleanup / selector `0x1C` branch |
| `2` | `0x08004E5E` | callback/deferred side path |
| `3` | `0x08004E74` | packed `0x19` transition shim |
| `4` | `0x08004E86` | mixed `0x1F / 0x20` branch |
| `5` | `0x0800511C` | right-panel / timebase editor |
| `6` | `0x080051A4` | transient handoff above state `5` |
| `7` | `0x08004F08` | hardware-prep shim that collapses into state `4` |
| `8` | `0x080051E6` | cleanup/reset shim |
| `9` | `0x08004F28` | staged preview/posture owner |

That changes the search in a useful way:

- visible state `5` is a real stable editor case
- visible state `6` is probably not an independent submenu; it looks like a
  one-step handoff that feeds state `5`
- visible state `7` looks like a transient hardware-prep path that immediately
  collapses into visible state `4`

## 1. How the case map was recovered

The `TBH` at `0x08004D78` uses the halfword table that begins at `0x08004D7C`.

Interpreting those halfwords as branch offsets gives:

- `0 -> 0x08004D90`
- `1 -> 0x08004E24`
- `2 -> 0x08004E5E`
- `3 -> 0x08004E74`
- `4 -> 0x08004E86`
- `5 -> 0x0800511C`
- `6 -> 0x080051A4`
- `7 -> 0x08004F08`
- `8 -> 0x080051E6`
- `9 -> 0x08004F28`

This is much stronger than the earlier "broad owner" label, because it exposes
which visible states are actually first-class cases inside the controller.

## 2. State `5` is the stable right-panel / timebase editor case

Case `5` starts at `0x0800511C`.

Its first split is on `DAT_20000F14` (`+0xE1C`), the right-panel subview byte:

- `E1C == 2` -> `0x080052CE`
- `E1C == 1` -> `0x08005304`
- `E1C == 0` -> stay in the local editor path

That local editor path then keys on `DAT_20000F12` (`+0xE1A`) and
`DAT_20000F15` (`+0xE1D`) while manipulating the `0xE12..0xE19` bitmap family.

What it emits:

- if `E1C == 0` and `E1A != 0`, it edits the bitmap/toggle path and ultimately
  queues selector `0x28`
- if `E1A == 2`, it prefixes that with selector `0x26`
- if `E1C == 2`, it directly queues `0x26`, then `0x28`

Best current read:

- state `5` is the live right-panel / timebase-side editor
- this is the first place where the `0x26 / 0x28` family is clearly attached to
  a top-level visible case, not just to a narrow helper

## 3. State `6` is a transient handoff above state `5`

Case `6` starts at `0x080051A4`.

Unlike state `5`, it does not contain a rich editor body. It only checks
`DAT_20000F14` (`+0xE1C`) and reacts to two values:

- `E1C == 2`
  - clear `E1C`
  - write visible state `5`
- `E1C == 1`
  - queue selector `0x2B`
  - write visible state `5`
  - re-enter `FUN_0800B908`
  - stamp visible state `12`

That is much narrower than state `5`, and it matches the earlier feeling that
state `6` was "close to" the timebase family without being the same thing.

Best current read:

- state `6` is a transient handoff / pre-commit phase above the stable state-`5`
  editor
- it is probably not a user-facing submenu by itself

## 4. State `5` can escalate into state `6`

One especially useful link is inside case `5`.

When:

- `E1C == 0`
- `E1A == 0`
- `E1B != 0`

the branch at `0x08005382..0x08005398` writes visible state `6` and immediately
re-enters the shared bank emitter.

That gives a concrete runtime chain:

- state `5` -> state `6` -> state `5` / `12`

So states `5` and `6` are not sibling independent editors. They are a paired
family.

## 5. State `7` looks like a transient hardware-prep shim for state `4`

Case `7` starts at `0x08004F08`.

It does two very specific things:

- writes visible state `4`
- flips hardware-side registers at `0x40005C40` and `+0x20`

Then it falls back into the shared flow. Later, the common exit at
`0x0800558A..0x080055A6` queues selector `0x21` only if the visible state is
`4`.

Best current read:

- state `7` is a transient prelude that prepares hardware and hands off to the
  actual state-`4` family
- state `4`, not `7`, is the more meaningful stable mixed branch

## 6. State `4` remains the mixed `0x1F / 0x20` family

Case `4` starts at `0x08004E86`.

This is the same mixed family we already associated with selectors `0x1F` and
`0x20`, but the case map improves the interpretation:

- state `4` is a real top-level case
- state `7` feeds it from above
- the `0x21` selector is emitted on the shared exit only after the controller is
  already sitting in visible state `4`

That makes the `4 -> 0x21` relationship look more structured and less like an
isolated curiosity.

## 7. State `8` is only a cleanup shim, and `state 9` owns the staged preview path

A later raw pass sharpened the two remaining high states:

- visible `state 8` (`0x080051E6`) just writes visible state `0` and re-enters
  the shared emitter
- visible `state 9` (`0x08004F28`) is the real staged owner above the later
  preview/posture family

The important current split inside visible `state 9` is:

1. `F69` is a phase gate
2. once `F69` reaches the active stage, the low nibble of `F6A` selects among:
   - `0x080055AE` (`display_mode` latch front edge)
   - `0x080055C0` (`+0x23A` preview/arming flag path)
   - `0x080055D8` (direct `F6B` trigger/channel control path)
   - `0x0800564C` (`+0x3C/+0x40` list-drain posture)
   - `0x08005702` (`+0x35/+0x3C` mirror cleanup)
3. a later phase re-enters through `0x08005218` and edits the control word at
   `+0x4C`

See:

- [state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md)

## 8. What This Changes

This is the cleanest hierarchy so far for the scope-side controller:

1. state `5` = stable right-panel / timebase editor
2. state `6` = transient handoff above state `5`
3. state `4` = stable mixed `0x1F / 0x20` family
4. state `7` = transient hardware-prep shim above state `4`

That is a sharper model than the earlier one, where states `5`, `6`, and `7`
were all just "other branches in the same blob."

## 9. Best Next Move

The next static pass should now focus on the inbound writers and UI conditions
for these specific case transitions:

1. who raises `E1C` to `1` versus `2` before state `5` enters the
   `0x2B` / `0x26,0x28` paths
2. what user-visible event makes state `5` escalate into state `6`
3. what top-level action enters state `7` before the hardware-prep-to-state-`4`
   collapse
4. what user-visible/runtime condition enters visible `state 9`, and what
   exactly `F6A low nibble = 3/4` means inside that staged preview branch
5. how those transitions line up with the stock scope UI draw helpers for the
   right panel and the packed preview lists

That is now the shortest path from the broad runtime controller to concrete,
reproducible scope-mode transition sequences.
