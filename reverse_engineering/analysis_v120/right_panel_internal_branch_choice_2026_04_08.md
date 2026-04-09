# Right-Panel Internal Branch Choice 2026-04-08

## Summary

This pass answers the next question raised by the `sub2 = 5` versus `sub2 = 2`
comparison:

- is there a higher external chooser above those two siblings?

The strongest current answer is:

- maybe not, or at least not in the simple sense we were imagining

Update after the raw owner pass in
[right_panel_stage_entry_event_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_entry_event_split_2026_04_08.md):

- the downstream `sub2 = 5` versus `sub2 = 2` distinction still looks local to
  visible `state 5`
- but the higher **single-selection versus mass-toggle** choice now looks much
  more like separate sibling event owners (`0x0800A120` and `0x0800A2F8`), not
  a hidden late branch inside one editor body

The downloaded app now looks more like the split happens **inside the visible
state-`5` editor posture itself**, based on whether staged-detail state exists:

- no staged detail yet -> coarse editor path -> transient `state 6` ->
  `sub2 = 2` family
- staged detail present -> bitmap/commit path -> `sub2 = 5` family

That is a cleaner and more stock-like explanation than "a hidden top-level
chooser directly picks selector `2` or selector `5`."

So this note should now be read as a **downstream branch-choice** model, not a
complete explanation for how stock chooses between the two stage helpers in the
first place.

Primary references:

- [right_panel_stage_helper_convergence_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_helper_convergence_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [right_panel_shim_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_shim_case_map_2026_04_08.md)

## 1. The coarse branch is already visible inside state `5`

The broad controller at visible `state 5` already contains the key coarse-side
escalation.

From
[scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md):

- when `E1C == 0`
- and `E1A == 0`
- and `E1B != 0`

the branch at raw `0x08009382..0x08009398` / decompiled `0x08005382..0x08005398`:

- writes visible `state 6`
- re-enters the shared emitter

That gives us a grounded coarse path:

1. visible `state 5`
2. no staged-detail latch
3. transient `state 6`
4. compact `sub2 = 2` sibling converts `6 -> 5`

That is a much better fit for the `sub2 = 2` family than imagining an
independent external chooser for selector `2`.

## 2. The detailed branch is also already visible inside state `5`

The staged-detail side is equally concrete.

From
[right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
and
[right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md):

- `0x08006120` can set `E1A = 1` and seed exactly one bit in `0xE12..0xE19`
- `0x080062F8` can set `E1A = 2` and fill the bitmap more broadly
- `0x08006418` then:
  - requires `E1C == 0`
  - requires `E1A != 0`
  - requires bitmap nonzero
  - writes `E1C = 2`
  - queues `0x2A`

That is exactly the staged-detail precondition the `sub2 = 5` sibling expects.

So the detailed path is:

1. visible `state 5`
2. staged-detail helper arms `E1A`
3. bitmap becomes nonzero
4. commit bridge raises `E1C = 2` and queues `0x2A`
5. `sub2 = 5` sibling handles the packed-selector side

Again, this looks like an internal branch choice inside the state-`5` editor,
not a detached top-level selector pick.

## 3. The two families are separated by staged state, not just by command number

Putting the two together:

| Condition inside visible `state 5` | Best current downstream family |
|---|---|
| `E1A == 0`, `E1C == 0`, `E1B != 0` | coarse editor / `sub2 = 2` |
| `E1A != 0`, bitmap nonzero, `E1C == 0` | staged-detail commit / `sub2 = 5` |

That makes the runtime distinction much less mysterious.

The key discriminator is not:

- selector `2` versus selector `5`

It is:

- whether staged detail has been armed and populated yet

## 4. What this changes

This is a useful narrowing because it moves the next question one step earlier.

The best next question is now:

- what event/action inside visible `state 5` flips the path from the coarse
  `E1A == 0` posture into the staged-detail `E1A != 0` posture?

We already have strong candidates:

- single-selection stage at `0x08006120`
- mass-toggle stage at `0x080062F8`

The new convergence note
[right_panel_stage_helper_convergence_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_helper_convergence_2026_04_08.md)
argues that both of those helpers feed the same `sub2 = 5` packed-selector
commit family, so the remaining distinction is likely in their entry events, not
in a later packed-selector split.

So the remaining gap is getting smaller:

- from broad runtime state
- to state-`5` editor posture
- to staged-detail branching

That is much better than the older flat model.

## 5. Best current working model

The right-panel branch choice now looks like:

1. enter visible `state 5`
2. if still coarse:
   - `E1A == 0`
   - editor can escalate to transient `state 6`
   - `sub2 = 2` family handles editor posture / reset side
3. if detail has been staged:
   - `E1A != 0`
   - bitmap at `0xE12..0xE19` is nonzero
   - `sub2 = 5` family handles staged-detail commit side

That is now the cleanest explanation for the `2` versus `5` split.

## Best Next Move

1. Trace the exact visible-state-`5` event path that flips `E1A` from `0` to
   `1/2`.
2. Compare the single-selection stage (`0x08006120`) against the mass-toggle
   stage (`0x080062F8`) to see whether stock routes them to the same
   `sub2 = 5` packed-selector path or to different later branches.
3. Keep treating `sub2 = 2` as the coarse editor family and `sub2 = 5` as the
   staged-detail commit family unless new raw evidence contradicts it.
