# Right-Panel Event Path

Date: 2026-04-08

Purpose:
- trace the concrete runtime path that feeds the right-panel scope handoffs
  through `+0xE1A`, `+0xE1B`, `+0xE1C`, and `+0xE1D`
- separate submenu entry from cursor edits and staged-detail commits
- turn the recent byte-level findings into a user-action-level model we can
  compare against future bench experiments

Primary references:
- [panel_subview_action_meaning_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_action_meaning_2026_04_08.md)
- [panel_subview_writer_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_writer_bridge_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [packed_scope_state_disasm_08004220_08004820_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08004220_08004820_2026_04_08.txt)
- [mixed_scope_handlers_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handlers_force_2026_04_08.c)
- [high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c)

## Executive Summary

The right-panel scope family now has a usable action model.

1. Entering the family seeds `DAT_20000F13` (`+0xE1B`) from a resource-backed
   helper, clears `DAT_20000F12` (`+0xE1A`) and `DAT_20000F14` (`+0xE1C`), and
   sets visible state `5`.
2. `bRam20000F15` (`+0xE1D`) is a real bounded cursor/index, not a passive
   scratch byte. Stock edits it with both fine `+/-1` handlers and at least one
   coarse `-3` helper.
3. `DAT_20000F12` (`+0xE1A`) is the staged-detail latch. When it is armed, stock
   builds a bitmap across `+0xE12..+0xE19` and only then raises
   `DAT_20000F14 = 2`, which feeds selector `0x2A`.

So the best current read is:

- submenu entry -> seed count / reset latches
- opposite navigation actions -> edit `E1D`
- confirm/toggle action -> arm `E1A` and stage the bitmap
- commit action -> raise `E1C = 2` and hand off through `0x2A`

That is a much tighter model than "there is one missing raw FPGA byte."

## 1. State-5 entry is resource-backed setup, not free-running editing

The clearest entry-side evidence is the setup path captured in
[high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c).

At the end of that setup block, stock does four important things in sequence:

1. `DAT_20000F13 = FUN_08034878(0x80BC18B)`
2. `uRam20000F12 = 0`
3. `_DAT_20000F14 = 0`
4. `DAT_20001060 = 5`

That means the right-panel state-5 editor is entered with:

- a freshly loaded count/capacity byte in `E1B`
- the staged-detail latch `E1A` cleared
- the subview/handoff byte `E1C` cleared

So `E1B` is not just a boolean flag. It is the resource-backed bound for this
editor family.

## 2. `E1D` is the live coarse cursor

The strongest cursor evidence is the symmetric pair inside the mixed handlers:

- decrement path in
  [mixed_scope_handlers_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handlers_force_2026_04_08.c)
  case `5/6`, and the raw slices at
  [packed_scope_state_disasm_08004220_08004820_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08004220_08004820_2026_04_08.txt)
  `0x08004296..0x080042D2` and `0x08004470..0x0800455C`
- increment path in the same forced helper at case `5/6`

What these do:

- require `E1C == 0`
- bound-check `E1D`
- decrement or increment `E1D`
- emit:
  - `0x27`, then `0x28` for case `5`
  - `0x29` for case `6`

That is much stronger evidence for:

- `E1D` = user-editable selection index

than for:

- `E1D` = static config byte

There is also an older broad-owner helper in
[mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
at `0x080032E4..0x08003318` that:

- requires `E1C == 0`
- requires `E1D >= 3`
- subtracts `3`
- emits `0x27`, then `0x28`

So the editor appears to have:

- fine-step edits (`+/-1`)
- at least one coarse jump helper (`-3`)

I do not yet have a matching proven `+3` site, so the safest current wording is
"fine and coarse cursor edits," not a fully solved row/column map.

## 3. `E1A` is the staged-detail latch

The most useful correction from this pass is that `E1A` behaves like an explicit
detail-edit stage, not a passive style flag.

Inside
[mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
at `0x0800619E..0x08006236`, the state-`5` path does:

- require `E1C == 0`
- require `E1B != 0`
- if `E1A == 0`:
  - set `E1A = 1`
  - clear `+0xE12..+0xE19`
  - compute `byte_index = E1D / 6`
  - compute `bit = E1D % 6`
  - set that bit in the bitmap bytes
- if `E1A != 0`:
  - clear `E1A`
- then emit `0x28`, then `0x26`

That is an action-level pattern, not just state churn:

- enter staged-detail edit
- seed one selected item from the current coarse cursor
- emit the detail-selector family

The sibling helper at `0x080062F8` strengthens the same read. In the state-`5`
path it:

- requires `E1C == 0`
- requires `E1B != 0`
- toggles `E1A` between `1` and `2`
- rewrites the same bitmap bytes
- emits `0x26`, then `0x28`

So the best current interpretation is:

- `E1A = 0` -> not in staged-detail mode
- `E1A = 1/2` -> active staged-detail phases

I would not yet assign exact physical key labels to those two phases, but they
look much more like opposite navigation/confirm actions than like static mode
bits.

## 4. `E1C = 2` is the real detailed commit handoff

The helper-cluster writer at `0x080064A4`, documented in
[panel_subview_writer_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_writer_bridge_2026_04_08.md),
now has a much clearer role.

It requires:

- `E1C == 0`
- `E1A != 0`
- at least one nonzero byte across `+0xE12..+0xE19`

and then:

- writes `E1C = 2`
- emits `0x2A`

That means `0x2A` is not the first editor action. It is the handoff after the
bitmap/detail stage is already populated.

This is the cleanest current runtime sequence for the staged path:

1. enter state `5`
2. load `E1B`, clear `E1A`, clear `E1C`
3. move coarse cursor `E1D`
4. arm staged-detail mode through `E1A`
5. populate bitmap `+0xE12..+0xE19`
6. raise `E1C = 2`
7. emit `0x2A`

## 5. What this implies about the missing stock scope sequence

This makes the scope problem look more staged than before.

The stock right-panel family is probably not:

- "send `0x27/0x28` once and the FPGA should wake up"

It is more likely:

- enter the correct right-panel state
- seed the bounded item count
- move the right-panel cursor
- optionally enter the staged-detail bitmap editor
- only then commit via `0x2A`

That does not prove the right-panel path is the missing scope-enter blocker, but
it does explain why our isolated shell sweeps stayed flat: they were trying the
selector families without the stateful setup and handoff stock appears to use.

## 6. Current best action model

The safest current action-level map is:

- submenu-entry action:
  - loads `E1B`
  - clears `E1A`
  - clears `E1C`
  - enters visible state `5`
- opposite navigation actions:
  - edit `E1D` up/down by `1`
  - one older helper also supports a coarse `-3` jump
- detail-toggle / select action:
  - arms `E1A`
  - stages the bitmap from the current `E1D`
  - emits `0x26 / 0x28`
- detailed-commit action:
  - converts staged bitmap presence into `E1C = 2`
  - emits `0x2A`

What I cannot yet prove from the downloaded vendor image alone is the exact
physical key mapping for those actions. The interior slices still do not have a
clean caller tree, so "prev/next/select/commit" is stronger than "UP button" or
"PRM button" at this stage.

## 7. Best next move

The next high-value pass should target the event side, not just the state side:

1. identify the higher-level key/event owners that reach the paired `E1D`
   increment/decrement handlers
2. find the matching runtime path that sets or clears `E1B` before the state-`5`
   editor runs
3. trace whether the right-panel `0x2A` commit can feed the same scope-bank
   re-entry path as the acquisition/timebase families
4. compare that recovered order against our live firmware experiments before
   adding any new brute-force bench sweeps
