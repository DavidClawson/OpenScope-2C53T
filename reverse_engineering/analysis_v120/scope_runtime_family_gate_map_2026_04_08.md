# Scope Runtime Family Gate Map

Date: 2026-04-08

Purpose:
- map the three recovered runtime preset families to their visible top-level
  gate states
- distinguish the genuinely timebase-side branch from the acquisition-side
  branch and the still-mixed branch
- record the shortest remaining trace target for reconnecting stock UI actions
  to the FPGA-facing selector families

Primary references:
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c)

## Executive Summary

The runtime preset families are no longer just "internal `9/1/x/0` shims."
They now split cleanly by the visible top-level gate state that feeds them:

1. visible state `5` is the strongest current **timebase / right-panel**
   owner, feeding the `9 / 1 / 2 / 0` family
2. visible state `2` is the strongest current **acquisition** owner, feeding
   the `9 / 1 / 3 / 0` family
3. visible state `1` is the strongest current **mixed raw-word / trigger-like**
   owner, feeding the `9 / 1 / 4 / 0` family

That is still an inference from static evidence, but it is a better one than
the earlier model where all three runtime families looked equally ambiguous.

## 1. Visible state `5` owns the timebase / right-panel family

This is now the strongest top-level mapping in the cluster.

The helper family around `0x08006548` shows:

- visible state `2` can be promoted to preset `9 / 1 / 2 / 0` at
  `0x08006578`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 2` at
  `0x080065CA`
- visible state `5` can reset directly back to visible state `2` at
  `0x08006592`

The most important detail is what the visible-state-`5` side edits:

- clear `0xE1C`
- clear `0xE12`
- clear `0xE16`
- clear `0xE1A`
- then tail-call the shared emitter `FUN_0800B908`

That same byte family already appears in the nearby top-level helper at
`0x08006326..0x08006386`, which:

- requires `DAT_20001060 == 5`
- gates on `0xE1C`, `0xE1B`, `0xE1A`, and the `0xE12..0xE19` bitmap bytes
- then queues `0x26` followed by `0x28`

That is the same right-panel / timebase-side family already identified in the
broad owner:

- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
  maps `0x27 / 0x28` to the strongest current timebase submenu
- this narrower family uses the adjacent `0x26 / 0x28` pair while operating on
  the right-panel bytes

Best current read:

- visible `DAT_20001060 = 5` is the stock runtime entry into the right-panel /
  timebase-side editor
- preset `9 / 1 / 2 / 0` is the internal packed commit/collapse path for that
  same family

## 2. Visible state `2` owns the acquisition family

This branch is now the cleanest acquisition-side mapping.

The helper family around `0x08005FCC` shows:

- visible state `2` can be promoted to preset `9 / 1 / 3 / 0` at
  `0x08006034`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 3` at
  `0x08006010`
- otherwise it stages `0x0301` at `0xF69` and emits `0x13`, then `0x14`

What it does **not** touch is just as important:

- it does not work through `0xE1C / 0xE1D`
- it keys directly on `DAT_20001062`

That lines up with the broad owner at `0x08003148`, where:

- the strongest acquisition branch decrements / normalizes `DAT_20001062`
- then queues `0x20`, followed by `0x21`
- and the nearby draw helper
  [FUN_08015D58](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4360)
  renders the six-choice list directly from that packed selector byte

Best current read:

- visible `DAT_20001060 = 2` is the active scope owner that feeds the
  acquisition submenu path
- preset `9 / 1 / 3 / 0` is the internal packed bridge used to commit or
  collapse that acquisition choice before re-entering the shared bank emitter

## 3. Visible state `1` owns the mixed raw-word / trigger-like family

This branch is still not solved, but it is cleaner than before.

The helper family around `0x080066AC` shows:

- visible state `2` can be promoted to preset `9 / 1 / 4 / 0` at
  `0x08006720`
- visible state `9` can collapse back to `2` when `DAT_20001062 == 4` at
  `0x080066FC`

The visible-state-`1` side is the interesting part:

- it requires `DAT_20000F2D == 3`
- it works through the mixed state bundle `0xF3C`, `0xF5D`, and `0xF2D`
- it toggles `0xF5D` between `0`, `0x22`, and `0x53`
- when that path reaches `0x53`, it emits raw word `0x02A0` and then sets
  `DAT_20000F2D = 8`
- once the high nibble reaches `0xB0`, it emits raw word `0x0503`

That is much closer to a staged preview / arm sequence than to the simpler
timebase or acquisition owners.

It also lines up better with the broad-owner mixed branch than with the others:

- the broad owner's most unresolved family is still the `0x08, 0x17, 0x18,
  0x19` branch
- that branch already looked like a trigger-style preview with shared/mixed
  behavior
- the visible-state-`1` helper has the same "mixed but purposeful" feel: it is
  not just a panel redraw and not just a simple selector decrement

Best current read:

- visible `DAT_20001060 = 1` is the best current top-level owner for the
  unresolved mixed trigger-like family
- preset `9 / 1 / 4 / 0` is the internal packed bridge that corresponds to that
  branch

## 4. Updated working map

| Visible top-level gate | Runtime preset family | Best current meaning | Confidence |
|---|---|---|---|
| `DAT_20001060 = 5` | `9 / 1 / 2 / 0` | timebase / right-panel editor | strongest |
| `DAT_20001060 = 2` | `9 / 1 / 3 / 0` | acquisition editor | strong |
| `DAT_20001060 = 1` | `9 / 1 / 4 / 0` | mixed trigger-like / raw-word preview | medium |

## 5. What This Changes

This is a useful narrowing for the scope blocker.

It means the next trace does **not** need to reopen the whole packed-state
space. The better question is now:

- who drives visible `DAT_20001060` into `5`, `2`, and `1` at runtime, and
  under which scope UI conditions?

That is a much tighter target than "find every writer to `0xF68..0xF6B`."

## 6. Best Next Move

The next static pass should now focus on the owners that visibly enter these
top-level gate states:

1. trace the higher-level scope path that drives `DAT_20001060 = 5` before the
   `0x08006326 / 0x08006548` family runs
2. trace the higher-level scope path that drives `DAT_20001060 = 2` before the
   `0x08005FCC` family runs
3. trace the higher-level scope path that drives `DAT_20001060 = 1` before the
   `0x080066AC` family runs
4. compare those three paths directly against the broad-owner selector families:
   - acquisition: `0x20 / 0x21`
   - timebase: `0x27 / 0x28`
   - mixed trigger-like: `0x08, 0x17, 0x18, 0x19`

That is now the shortest path from recovered runtime gates to a stock-faithful
scope transition order we can try to reproduce in firmware.
