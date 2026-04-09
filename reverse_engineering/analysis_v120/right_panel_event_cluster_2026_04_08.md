# Right-Panel Event Cluster

Date: 2026-04-08

Purpose:
- consolidate the right-panel scope event peers into one staged runtime cluster
- separate coarse cursor edits from single-selection staging, mass-toggle
  staging, and final commit
- connect the event cluster back to state-`5` entry and the later
  state-`5` / state-`6` controller consumption

Primary references:
- [right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md)
- [right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md)
- [panel_subview_writer_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_writer_bridge_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [mixed_scope_handlers_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handlers_force_2026_04_08.c)

## Executive Summary

The right-panel/timebase side now looks like a real staged event cluster rather
than a loose set of selector families.

1. There is no single recovered owner that does everything by itself. The stock
   path is better modeled as three layers:
   - entry/setup into visible state `5`
   - a five-peer event cluster
   - later consumption by the broad controller in visible states `5` and `6`
2. The two staged helpers are meaningfully different:
   - `0x08006120` seeds a **single** detail bit from the coarse cursor `E1D`
   - `0x080062F8` toggles the **whole** detail bitmap between all-set and clear
3. `0x08006418` is the first clean commit point. Once the staged bitmap is
   nonzero, it raises `E1C = 2` and queues `0x2A`.
4. The older broad owner around `0x08003148` still matters: it owns the coarse
   `E1D -= 3` jump and rewrites `E1C = 2` back into `E1C = 1` before its own
   `0x2A` handoff.

So the best current stock-shaped model is:

- enter state `5`
- adjust the coarse cursor
- choose one of the staged detail helpers
- commit through `E1C = 2 -> 0x2A`
- let the older owner and the broad controller consume that handoff

That is much tighter than treating `0x27/0x28/0x2A` as isolated raw commands.

## 1. State-`5` entry is still the setup layer

The entry-side result from
[right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md)
still stands:

- `E1B` is loaded from a resource-backed helper
- `E1A` is cleared
- `E1C` is cleared
- visible state `5` is entered

That means the event peers below are not the entry owner. They assume the panel
is already live in its state-`5` editor posture.

## 2. Peer 1 and peer 2: generic coarse adjustment

The first two peers are still the generic event owners recovered in
[right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md):

- `0x080041F8` = adjust-prev
- `0x080047CC` = adjust-next

Inside the right-panel family they:

- require `E1C == 0`
- decrement or increment `E1D`
- emit:
  - `0x27`, then `0x28` in visible state `5`
  - `0x29` in visible state `6`

So these are the coarse cursor editors for the right-panel family, not narrow
timebase-only one-offs.

## 3. Peer 3: single-selection staging at `0x08006120`

The state-`5` branch at `0x0800619E..0x08006236` is now specific enough to
describe as a **single-selection stage**.

Preconditions:

- visible state `5`
- `E1C == 0`
- `E1B != 0`

Behavior:

- if `E1A == 0`
  - set `E1A = 1`
  - clear `0xE12..0xE19`
  - derive one bit position from `E1D / 6` and `E1D % 6`
  - set exactly that one bit in the bitmap
- if `E1A != 0`
  - clear `E1A`

Then it emits:

- `0x28`
- `0x26`

That is stronger than the earlier generic "toggle/detail" label. This helper is
the stock path that converts the coarse cursor into a single active detail
selection.

## 4. Peer 4: mass-toggle staging at `0x080062F8`

The sibling state-`5` branch at `0x08006326..0x08006382` is not just a mirror
of `0x08006120`. It is a different staged action.

Preconditions:

- visible state `5`
- `E1C == 0`
- `E1B != 0`

Behavior:

- if `E1A == 2`
  - write `E1A = 1`
  - clear `0xE12..0xE19`
- otherwise
  - write `E1A = 2`
  - fill `0xE12..0xE19` with the repeated mask `0x3F3F3F3F`

Then it emits:

- `0x26`
- `0x28`

So this peer is better read as a **mass-toggle / all-items stage** than as
another generic confirm key. It alternates the detailed bitmap between a
full-mask posture and a cleared posture.

## 5. Peer 5: commit bridge at `0x08006418`

The commit side at `0x08006468..0x080064C8` is now the clearest bridge in the
cluster.

Preconditions:

- `E1C == 0`
- `E1A != 0`
- at least one nonzero byte across `0xE12..0xE19`

Behavior:

- write `E1C = 2`
- queue `0x2A`

This is the first place where the staged detail work is turned into a clean
handoff token the rest of the scope controller can consume.

## 6. The older broad owner still supplies the coarse handoff

The older owner around `0x08003148` still owns two important pieces that do not
live inside the tighter helper cluster.

### 6.1 Coarse jump helper

At `0x080032E4..0x08003318`:

- require `E1C == 0`
- require `E1D >= 3`
- subtract `3` from `E1D`
- emit `0x27`, then `0x28`

So the right-panel editor has both:

- fine `+/-1` edits from the generic adjust-prev / adjust-next owners
- at least one coarse `-3` jump from the older broad owner

### 6.2 `E1C = 2 -> E1C = 1` bridge

At `0x080032D6..0x0800333E`:

- if `E1C == 2`, the branch falls into `0x08003324`
- `0x08003324..0x08003326` rewrites `E1C = 1`
- then it queues `0x2A`

That makes the older owner a real downstream consumer of the staged detail
commit, not just an unrelated neighboring path.

Best current read:

- `E1C = 2` = armed detailed subview handoff
- `E1C = 1` = coarser right-panel/timebase handoff used by the later controller

## 7. Broad-controller consumption

The broad controller at `0x08004D60` finishes the picture.

From
[scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md):

- state `5` is the stable right-panel editor
- state `6` is the transient handoff above it

Relevant consumption:

- state `5`
  - `E1C == 2` -> direct `0x26 / 0x28` path
- state `6`
  - `E1C == 2` -> clear `E1C`, fall back to state `5`
  - `E1C == 1` -> queue `0x2B`, write state `5`, re-enter the shared bank
    emitter, then stamp state `12`

That means the event cluster does not terminate at `0x2A`. The later controller
still distinguishes between the detailed `E1C = 2` path and the coarser
`E1C = 1` path.

## 8. Updated staged runtime model

The cleanest current model is:

1. enter visible state `5`
   - load `E1B`
   - clear `E1A`
   - clear `E1C`
2. coarse edit phase
   - `0x080041F8` / `0x080047CC` adjust `E1D` by `-1 / +1`
   - `0x080032E4` can adjust `E1D` by `-3`
3. staged detail phase
   - `0x08006120` seeds one active detail bit from `E1D`
   - `0x080062F8` toggles the whole bitmap between all-set and clear
4. commit phase
   - `0x08006418` raises `E1C = 2` and queues `0x2A`
5. post-commit consumption
   - older broad owner can convert `E1C = 2 -> E1C = 1` and queue another
     `0x2A`
   - controller states `5/6` consume `E1C` and decide whether the path stays in
     detailed mode or escalates through `0x2B`

This is the clearest stock event model so far for the right-panel scope family.

## 9. Practical implication

This makes the likely scope reproduction gap more concrete.

The stock right-panel family probably was not waking the FPGA because of one
missing selector byte. It looks more like a staged state machine:

- cursor edit
- detail staging
- explicit commit
- controller-side post-commit handoff

So any future bench reproduction should try to preserve that order rather than
sending isolated `0x26/0x27/0x28/0x2A` fragments.

## 10. Best next move

The next static pass should target the first higher-level owner that chooses
between these cluster actions:

1. what event family routes into `adjust-prev` / `adjust-next`
2. what event family routes into the single-selection stage
3. what event family routes into the mass-toggle stage
4. whether the coarse `E1C = 1` handoff is always derived from a prior
   `E1C = 2` commit, or whether stock can enter it directly

That should be the shortest path from the recovered right-panel cluster to a
bench-reproducible stock control sequence.
