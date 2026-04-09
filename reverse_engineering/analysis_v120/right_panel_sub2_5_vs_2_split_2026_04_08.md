# Right-Panel `sub2 = 5` vs `sub2 = 2` Split 2026-04-08

## Summary

This pass compares the two most relevant right-panel packed-selector siblings:

- raw `0x0800A418` -> packed selector `5`
- raw `0x0800A548` -> packed selector `2`

The strongest new result is that they are **not** just two nearby selector
variants. They sit on opposite sides of the right-panel choreography:

- `sub2 = 5` looks like the **staged-detail commit bridge**
- `sub2 = 2` looks like the **coarse editor entry/exit family**

That is useful because it narrows the next trace surface. The higher chooser we
still want is probably not picking between two equivalent submenu options. It is
more likely distinguishing between:

1. "commit the staged bitmap/detail work"
2. "enter or reset the coarse right-panel editor posture"

Primary references:

- [right_panel_internal_branch_choice_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_internal_branch_choice_2026_04_08.md)
- [runtime_owner_cluster_extension_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_owner_cluster_extension_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_handoff_directness_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_handoff_directness_2026_04_08.md)
- [right_panel_shim_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_shim_case_map_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)

## 1. The two siblings have different visible-state maps

Recovered `TBB` case maps:

### Raw `0x0800A418` (`sub2 = 5` sibling)

| Visible state (`F68`) | Raw target | Best current read |
|---|---:|---|
| `1` | `0x0800A43A` | small side gate / `F5D`-style prep |
| `2` | `0x0800A44E` | stamp `9 / 1 / 5 / 0` |
| `3` | `0x0800A4D4` | return |
| `4` | `0x0800A4D4` | return |
| `5` | `0x0800A468` | staged-detail commit path |
| `6` | `0x0800A4CC` | transient handoff into the same commit path |
| `7` | `0x0800A4D4` | return |
| `8` | `0x0800A4D4` | return |
| `9` | `0x0800A4D6` | consume/collapse `sub2 = 5` |

### Raw `0x0800A548` (`sub2 = 2` sibling)

| Visible state (`F68`) | Raw target | Best current read |
|---|---:|---|
| `1` | `0x0800A56A` | coarse right-panel/timebase setup side |
| `2` | `0x0800A578` | stamp `9 / 1 / 2 / 0` |
| `3` | `0x0800A576` | return |
| `4` | `0x0800A576` | return |
| `5` | `0x0800A592` | direct reset to visible `2` + clear panel staging |
| `6` | `0x0800A5B2` | direct entry to visible `5` |
| `7` | `0x0800A576` | return |
| `8` | `0x0800A576` | return |
| `9` | `0x0800A5C0` | consume/collapse `sub2 = 2` |

That already suggests the important split:

- `sub2 = 5` centers on visible states `5/6` as **commit-side** logic
- `sub2 = 2` centers on visible states `6 -> 5 -> 2` as **editor posture**
  logic

## 2. `sub2 = 5` is downstream of staged-detail work

The visible-state-`5` branch in the `sub2 = 5` sibling requires the same staged
right-panel bytes we already recovered in the event-cluster notes.

Grounded conditions at raw `0x0800A468..0x0800A4C8`:

- require `E1C == 0`
- require `E1A != 0`
- OR across `0xE12..0xE19`
- if bitmap is nonzero:
  - write `E1C = 2`
  - queue `0x2A`

That is exactly the staged-detail commit pattern already documented in:

- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md)

Visible `state 6` in this same sibling does not look like a separate feature.
At raw `0x0800A4CC`, it simply rechecks `E1C` and can fall back into the same
commit-side `0x2A` bridge.

Best current read:

- `sub2 = 5` is the packed-selector family you enter **after** staged detail is
  already armed and the bitmap exists
- it is a bridge from right-panel staged-detail work into the packed-selector
  cluster

## 3. `sub2 = 2` is the coarse editor posture family

The `sub2 = 2` sibling behaves very differently.

Visible `state 6` at raw `0x0800A5B2`:

- write visible state `5`
- re-enter the shared emitter

Visible `state 5` at raw `0x0800A592`:

- write visible state `2`
- clear `E1C`
- clear `0xE12`
- clear `0xE16`
- clear `E1A`
- re-enter the shared emitter

So this sibling is not centered on staged-detail commit at all. It is centered
on the coarse editor posture:

- enter visible state `5`
- work there
- collapse back to visible state `2`

This matches the earlier compact-owner reading in
[right_panel_shim_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_shim_case_map_2026_04_08.md).

Best current read:

- `sub2 = 2` is the packed-selector family associated with coarse right-panel /
  timebase editor entry and reset
- it is not the staged-detail commit bridge

## 4. The two siblings sit on opposite sides of the same user flow

Putting the two together gives a cleaner runtime picture:

1. coarse editor posture:
   - visible `state 6 -> 5`
   - handled by the `sub2 = 2` family
2. staged-detail work:
   - `E1A`
   - `E1C`
   - `0xE12..0xE19`
   - `0x2A`
3. detailed commit bridge:
   - handled by the `sub2 = 5` family

So the question is no longer:

- "why would stock choose selector `5` instead of selector `2`?"

The better question is:

- "what runtime/UI path takes the editor from the coarse `state 6 -> 5`
  posture into the staged-detail commit bridge?"

That is a much better search target.

## 5. Practical consequence for the scope problem

This explains why flat right-panel command sweeps were weak evidence.

If stock distinguishes:

- coarse editor posture (`sub2 = 2`)
- from staged-detail commit (`sub2 = 5`)

then replaying only `0x26/0x27/0x28/0x2A` fragments without reproducing the
editor posture and the staged bitmap is skipping the most important transition.

That does not prove this is the one missing scope blocker, but it gives us a
much more realistic stock model than "one raw command family must be wrong."

## 6. Best current working model

The cleanest current local model is:

1. higher path enters visible `state 6`
2. `sub2 = 2` sibling converts `6 -> 5`
3. right-panel editor runs in visible state `5`
4. staged-detail helpers populate `E1A` and `0xE12..0xE19`
5. `sub2 = 5` sibling sees that staged state and converts it into
   `E1C = 2 -> 0x2A`
6. later logic collapses/normalizes back through the packed-selector cluster

## Best Next Move

1. Trace the exact visible-state-`5` event path that flips `E1A` from `0` to
   `1/2`, because the newer synthesis in
   [right_panel_internal_branch_choice_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_internal_branch_choice_2026_04_08.md)
   suggests the `sub2 = 5` versus `sub2 = 2` split may happen inside the
   editor itself.
2. Trace the higher runtime path that enters visible `state 6` before the
   `sub2 = 2` sibling runs.
3. Trace the higher runtime path that reaches the staged-detail preconditions
   (`E1A != 0`, bitmap nonzero) before the `sub2 = 5` sibling runs.
4. Compare those two entry paths directly, because that now looks like the
   cleanest unresolved split between coarse right-panel editing and detailed
   commit.
