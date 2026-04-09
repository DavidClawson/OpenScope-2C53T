# Visible State 6 Entry Gate

Date: 2026-04-08

Purpose:
- trace the immediate inbound path to visible `state 6`
- determine whether `state 6` is entered from an external owner or raised from
  inside the broad right-panel/timebase controller itself
- connect that result back to the compact `0x08006548` shim owner

Primary references:
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md)
- [right_panel_shim_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_shim_case_map_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)

## Executive Summary

This pass tightens the visible `state 6` story substantially:

1. The broad runtime controller at `0x08004D60` is still the immediate writer
   of visible `state 6`.
2. More specifically, the write at `0x0800538C` is gated by `E1B != 0` inside
   the visible `state 5` controller body.
3. So `state 6` is not floating above the right-panel editor. It is raised from
   inside the editor-side controller once the panel has a nonzero `E1B` seed.
4. That makes `state 6` look even more like a transient "armed/non-empty panel
   handoff" than a standalone user-facing submenu.

Best current staged order:

1. enter visible `state 5`
2. load or preserve nonzero `E1B`
3. broad controller raises visible `state 6`
4. compact shim owner at `0x08006548` collapses `6 -> 5`
5. the right-panel editor proceeds from there

## 1. The raw `state 6` writer

The existing broad-owner note was already correct that:

- `0x0800538C..0x08005398` writes visible `state 6`
- then tail-calls `FUN_0800B908`

Raw disassembly:

- `0x0800538C`: `movs r0, #6`
- `0x0800538E`: `strb.w r0, [r5, #3944]`  (`DAT_20001060 = 6`)
- `0x08005398`: tail-call `FUN_0800B908`

The raw app bytes at `0x0800538C` also match that interpretation directly:

- `06 20 85 F8 68 0F ...`

## 2. Immediate gate: `E1B != 0`

The more useful new result is the immediate gate just above that writer.

At `0x08005382..0x08005388` the broad controller does:

- load `E1B` (`ldrb.w r0, [r5, #3611]`)
- compare against zero
- if zero, branch away from the `state 6` write
- if nonzero, fall into `0x0800538C`

So the local condition is simple:

- `E1B == 0` -> do **not** enter visible `state 6`
- `E1B != 0` -> enter visible `state 6`

That is the cleanest direct inbound condition for state `6` we currently have.

## 3. Why this matters

Earlier, `state 6` looked like a transient handoff above visible `state 5`, but
it was still unclear whether it came from a different owner or from inside the
same right-panel controller.

This pass answers that:

- visible `state 6` is raised from inside the broad `state 5` family itself
- the gate is not `E1C`
- the immediate gate is `E1B`

That means the likely semantic role of `state 6` is tied to the panel having a
nonzero resource/count/capacity posture, not just to a later commit byte.

## 4. Updated meaning of `state 6`

This result fits the current `E1B` interpretation better than the older
"mystery pre-commit submenu" read.

`E1B` has already looked like:

- a resource-backed bound or count byte for the right-panel family
- a prerequisite for the staged-detail helpers

So visible `state 6` now looks more like:

- transient armed panel posture
- non-empty/nonzero right-panel handoff
- editor-side transition state that only exists when the panel has real content

and less like:

- an independent user-visible settings screen

## 5. Connection to the compact shim owner

This also makes the compact owner at `0x08006548` easier to place:

- broad owner raises `state 6` from inside the editor-side family
- compact owner handles `state 6 -> state 5`

So the compact owner is not the creator of the handoff. It is the next-stage
consumer of a handoff already raised by the broad controller.

That gives the right-panel family a cleaner local hierarchy:

1. broad controller in visible `state 5`
2. `E1B != 0` gate
3. visible `state 6`
4. compact shim collapses back to visible `state 5`
5. later detailed/toggle/commit helpers continue from there

## 6. What remains unresolved

This pass does not yet answer two neighboring questions:

1. who originally seeds `E1B` before the broad controller takes this branch
2. which exact local branch or event inside visible `state 5` reaches
   `0x08005382` before the `E1B` check

Those are now the best remaining inbound questions above the `state 6` writer.

## 7. Best Next Move

The next static pass should target:

1. the local branch path inside visible `state 5` that reaches `0x08005382`
2. the earlier owner that seeds or reloads `E1B`

That is now the shortest route from the `state 6` handoff to a more explicit
stock action order we can try to reproduce on the bench.
