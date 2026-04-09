# Scope Runtime Preset Action Map

Date: 2026-04-08

Purpose:
- connect the runtime preset/re-entry helpers to likely user-visible scope
  actions
- distinguish the strongest current mappings from the still-mixed branch
- tie the newer `9/1/x/0` preset helpers back to the broad-owner selector
  families already identified at `0x08003148`

Primary references:
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [scope_owner_start_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_start_map_2026_04_08.md)
- [state2_promotion_owner_family_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state2_promotion_owner_family_map_2026_04_08.md)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)

## Executive Summary

The runtime preset helpers now split into three useful action families:

1. `sub2 = 2` is the strongest current **timebase / right-panel** family
2. `sub2 = 3` is the strongest current **acquisition** family
3. `sub2 = 4` is still **mixed**, but it now looks more like the unresolved
   trigger-like branch than the timebase or auxiliary branches

This is still an inference from static evidence, but it is a better one than
the earlier flat "all packed presets are equally ambiguous" model.

## 1. `sub2 = 2` maps best to the timebase / right-panel family

This is currently the cleanest mapping.

The runtime helper family around `0x08006548` shows:

- visible state `2` can be promoted to preset `9 / 1 / 2 / 0`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 2`
- visible state `5` can reset directly back to `2`

The key detail is the direct reset helper at `0x08006592`, which does:

- `DAT_20001060 = 2`
- clear `0xE1C`
- clear `0xE12`
- clear `0xE16`
- clear `0xE1A`
- then re-enter `FUN_0800B908`

Those bytes are not generic scope state. They are the same right-panel family
already documented elsewhere:

- `0xE1C` = [panel_subview_state](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L254)
- `0xE12..0xE19` = bitmap/toggle bytes used by the partner helper cluster

The neighboring runtime cluster agrees:

- in [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md#L127),
  `DAT_20001060 == 5` toggles the `+0xE12..+0xE19` family and queues `0x26`,
  then `0x28`
- in the broad owner, the strongest timebase branch is the
  `DAT_20000F14 / bRam20000F15` path that queues `0x27`, then `0x28`
  [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md#L75)

Best current read:

- visible state `5` is a timebase-side / right-panel state
- preset `9 / 1 / 2 / 0` is the internal commit/collapse bridge for that same
  family
- this is the strongest current bridge from a visible scope panel state back to
  the broad-owner `0x27 / 0x28` timebase family

## 2. `sub2 = 3` maps best to the acquisition family

This is the next strongest mapping.

The runtime helper family around `0x08005FCC` shows:

- visible state `2` can be promoted to preset `9 / 1 / 3 / 0`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 3`
- otherwise it stages `0x0301` at `0xF69` and emits `0x13`, `0x14`

The important part is what this family edits:

- it keys directly on `DAT_20001062`
- it does **not** key on the `0xE1C / 0xE1D` panel-subview family
- it sits next to the broad-owner branch that also edits `DAT_20001062` and
  queues `0x20`, then `0x21`
  [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md#L54)

That is a much better fit for the acquisition branch than for timebase.

Best current read:

- selector target `3` is a packed acquisition-side substate
- preset `9 / 1 / 3 / 0` is the internal bridge that commits/collapses that
  acquisition choice
- this is the strongest current runtime counterpart to the broad-owner
  `0x20 / 0x21` family

## 3. `sub2 = 4` remains mixed, but now looks trigger-like

This branch is still not clean enough to call solved, but it is no longer
equally likely to belong anywhere.

The runtime helper family around `0x080066B0` shows:

- visible state `2` can be promoted to preset `9 / 1 / 4 / 0`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 4`

The raw app-slot audit now grounds those two sides more tightly:

- raw `0x0800A720` / decompiled `0x08006720`
  - writes `9 / 1 / 4 / 0`
  - sets `+0x355 = 1`
  - tail-calls the shared emitter
- raw `0x0800A6F2`
  - if `F6A == 4`, clears `+0x355`, collapses back to visible `2`, and clears
    `F69 / F6B`
  - otherwise stages `0x0401` at `0xF69`, clears `F6B`, and emits `0x13`,
    then `0x14`

Unlike the timebase family, it does **not** clear or step the `0xE1C / 0xE1D`
right-panel bytes.

Unlike the auxiliary `0x2C` branch, it does **not** work through `DAT_20001058`.

Instead, its visible-state-`1` side works through a more mixed state bundle:

- `0xF2D`
- `0xF2E`
- `0xF5D`
- `0xF3D`

and can emit raw words like:

- `0x02A0`
- `0x0503`

The raw app-slot audit now makes that more concrete too:

- raw `0x0800A784`
  - emits raw TX word `0x02A0`
  - then sets `+0xF2D = 8`
- raw `0x0800A7BC`
  - later emits raw TX word `0x0503`

That is exactly the kind of mixed behavior we already saw in the broad-owner
branch that queues `0x08`, `0x17`, `0x18`, `0x19`: useful, scope-visible, but
still not cleanly separable from shared/mixed machinery.

Best current read:

- selector target `4` is the strongest current runtime candidate for the still
  unresolved trigger-like / mixed preview family
- and the raw `0x02A0 -> ... -> 0x0503` path now makes that trigger-like /
  mixed label stronger than it was before
- but this branch should still remain labeled as an inference until we can
  connect it to a named UI path more directly

## 4. Updated working map

| Runtime preset family | Best current UI/action read | Confidence |
|---|---|---|
| `9 / 1 / 2 / 0` | timebase / right-panel commit-collapse family | strongest |
| `9 / 1 / 3 / 0` | acquisition commit-collapse family | medium-strong |
| `9 / 1 / 4 / 0` | trigger-like / mixed preview family | medium-low |

## 5. What changed

Before this pass, the runtime preset families were mostly treated as parallel
state shims.

Now the better interpretation is:

- the `sub2 = 2` family is anchored by the same right-panel bytes that already
  mark the timebase path
- the `sub2 = 3` family lines up with the broad-owner branch that edits
  `DAT_20001062` and emits `0x20 / 0x21`
- the `sub2 = 4` family is the one that still behaves "mixed," which matches
  the unresolved trigger-like branch better than the other families

That is a useful narrowing, even though one branch is still open.

## 6. Best next move

The next static trace should now focus on the owners that lead into these three
runtime families. The comparison in
[state2_promotion_owner_family_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state2_promotion_owner_family_map_2026_04_08.md)
shows that the visible-state-`1` setup sides are now the sharper discriminator
than the simple visible-state-`2 -> 9 / 1 / x / 0` stores.

1. trace which UI/button paths land in the visible-state-`5` family around
   `0x08006548`
2. trace which UI/button paths land in the visible-state-`2` family around
   `0x08005FCC`
3. trace which UI/button paths land in the mixed family around `0x080066B0`
4. compare those entries directly against the broad-owner branches for:
   - `0x20 / 0x21`
   - `0x27 / 0x28`
   - `0x08, 0x17, 0x18, 0x19`

That is now the shortest remaining path from recovered runtime presets to a
stock-faithful scope UI transition sequence.
