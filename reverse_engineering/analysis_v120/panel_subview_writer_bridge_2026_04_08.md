# Panel Subview Writer Bridge

Date: 2026-04-08

Purpose:
- trace concrete runtime writers of `DAT_20000F14` (`+0xE1C`) and connect them
  to the state-`5` / state-`6` controller cases
- distinguish the `E1C = 1` path from the `E1C = 2` path
- reduce the remaining ambiguity around whether these are true UI-side handoffs
  or just incidental state writes

Primary references:
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md)

## Executive Summary

The right-panel subview byte `DAT_20000F14` (`+0xE1C`) now has two concrete
runtime raisers that matter for the broad controller:

1. `E1C = 1` is raised by the older broad scope owner at `0x08003224`, which
   then queues selector `0x2A`
2. `E1C = 2` is raised by the helper-cluster path at `0x080064A4`, which also
   queues selector `0x2A`

That is a useful split because the state-`5` / state-`6` controller cases at
`0x0800511C` / `0x080051A4` already branch differently on those two values:

- `E1C == 1` leads into the `0x2B -> state 5 -> state 11/12` path
- `E1C == 2` leads into the direct `state 5` or `0x26 / 0x28` path

So `E1C` is no longer just a descriptive UI byte. It is now a concrete bridge
between upstream scope UI logic and the runtime controller cases.

## 1. Runtime writer for `E1C = 1`

The cleanest writer is inside the broad owner at `0x08003148`, in the timebase
/ right-panel family:

- `0x080032D6` loads `E1C`
- if `E1C == 2`, it falls into `0x08003324`
- `0x08003324..0x08003326` writes `E1C = 1`
- then it queues selector `0x2A`

Important nearby context:

- the same surrounding branch also owns the `0x27 / 0x28` family through
  `E1D` at `0x080032E4..0x08003318`
- that family was already our strongest timebase-side branch in
  [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)

Best current read:

- `E1C = 1` is raised by a real timebase / right-panel UI path, not by an
  unrelated housekeeping helper
- selector `0x2A` is probably the handoff token between that older owner and the
  later state-`5` / state-`6` controller family

## 2. Runtime writer for `E1C = 2`

The cleanest writer is inside the helper-cluster path at `0x08006468`:

- require `E1C == 0`
- require `E1A != 0`
- require at least one nonzero byte in `0xE12..0xE19`
- then `0x080064A4..0x080064A6` writes `E1C = 2`
- and again queues selector `0x2A`

That is a different precondition set from the `E1C = 1` writer:

- it depends on the right-panel bitmap/toggle family already being populated
- it does not look like the first entry into the timebase editor
- it looks more like a second-phase or armed subview state

Best current read:

- `E1C = 2` is a richer follow-on subview, not just another spelling of
  `E1C = 1`
- the repeated `0x2A` strongly suggests both states feed the same higher-level
  handoff mechanism, but with different meaning

## 3. How the two writers line up with the broad controller

The broad controller case map now makes the split actionable.

From
[scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md):

- state `5` case (`0x0800511C`)
  - `E1C == 2` -> direct `0x26 / 0x28` path
  - `E1C == 1` -> `0x00, 0x2B -> state 5 -> state 11`
- state `6` case (`0x080051A4`)
  - `E1C == 2` -> clear `E1C`, write state `5`
  - `E1C == 1` -> `0x2B -> state 5 -> state 12`

That gives a much cleaner bridge:

| Upstream writer | Upstream family | `E1C` value | Downstream controller effect |
|---|---|---|---|
| `0x08003324` | older broad timebase/right-panel owner | `1` | `0x2B` handoff into state `5`, then state `11/12` |
| `0x080064A4` | helper-cluster armed/toggled path | `2` | direct state-`5` or `0x26 / 0x28` path |

## 4. `E1A` looks like the companion toggle latch

The helper-cluster path strengthens the idea that `E1A` is the companion latch
for this same family.

Inside `0x0800619E..0x08006234`:

- if `E1C == 0` and `E1B != 0`, the helper checks `E1A`
- if `E1A == 0`, it sets `E1A = 1`
- clears `0xE12..0xE16`
- repopulates a bitmap byte based on `E1D`
- then queues selector `0x28`

So `E1A` now looks less like a generic display byte and more like a
panel-side "toggle active / staged" latch that precedes the `E1C = 2` writer.

## 5. What This Changes

This is the strongest writer-to-controller bridge we have so far for the
timebase-side scope path.

The key shift is:

- `E1C = 1` is not arbitrary; it is staged by the older timebase owner
- `E1C = 2` is not arbitrary; it is staged by the helper-cluster armed/toggled
  branch
- both converge on the state-`5` / state-`6` controller through selector `0x2A`

That makes the scope runtime much more structured than it looked when these
bytes were just isolated reads in different blobs.

## 6. Best Next Move

The next static pass should now focus on two narrow questions:

1. what exact UI action inside the older broad owner raises `E1C = 1` before the
   `0x2A` handoff
2. what exact condition causes the helper-cluster path to raise `E1C = 2` after
   the `E1A / E1D / 0xE12..0xE19` bitmap staging

Those two answers should tell us whether the missing scope choreography is
primarily:

- a right-panel timebase commit sequence
- or a second-phase armed/toggled subview sequence layered on top of it
