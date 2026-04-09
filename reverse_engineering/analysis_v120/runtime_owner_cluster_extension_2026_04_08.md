# Runtime Owner Cluster Extension 2026-04-08

## Summary

This pass extends the newer sibling-owner model around the visible-state
promotion helpers.

The strongest new result is that the raw region from roughly:

- `0x08009FCC`
- through `0x0800A952`

is better read as a **wider runtime-owner cluster** than just three isolated
siblings.

Inside that cluster, the already-grounded `sub2 = 2 / 3 / 4` families now sit
next to:

1. a fourth sibling for packed selector `5`
2. a broader packed-state normalizer at raw `0x0800A834`

That matters because it makes the next trace surface more realistic:

- stock may not jump straight into the `sub2 = 2 / 3 / 4` siblings from some
  hidden external table
- instead, those siblings may be one layer inside a larger contiguous runtime
  cluster that also handles commit, normalize, and exit behavior

Primary references:

- [state2_promotion_owner_family_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state2_promotion_owner_family_map_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
- [scope_owner_start_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_start_map_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. The `sub2 = 5` sibling is real and sits right beside the newer family map

The raw owner at `0x0800A418` is not a random extra helper. It is the cleanest
current sibling for packed selector `5`.

Grounded behavior:

- visible `state 5`
  - gates on `+0xE1C == 0`
  - requires `+0xE1A != 0`
  - ORs the staged bitmap at `+0xE12..+0xE19`
  - when non-empty, raises `+0xE1C = 2`
  - then queues selector `0x2A`
- visible `state 9`
  - if `F6A == 5`, clears the latch and collapses back to visible `2`
  - otherwise stages `0x0501` at `0xF69`, clears `F6B`, and emits
    `0x13`, then `0x14`
- visible `state 2`
  - stamps packed preset `9 / 1 / 5 / 0`
  - sets `+0x355 = 1`
  - re-enters the shared emitter

This matches the older local reading in
[scope_owner_start_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_start_map_2026_04_08.md),
but the new raw pass places it more convincingly beside the `sub2 = 2 / 3 / 4`
siblings.

Best current read:

- `sub2 = 5` is the staged-detail / bitmap-commit sibling
- it is the closest current runtime partner to the `E1C = 2 -> 0x2A` path

## 2. The adjacent `sub2 = 2 / 3 / 4 / 5` siblings now look like a matched family

The current sibling set is:

| Raw owner | Packed selector family | Best current read |
|---|---|---|
| `0x08009FCC` | `sub2 = 3` | acquisition-side |
| `0x0800A418` | `sub2 = 5` | staged-detail / bitmap-commit |
| `0x0800A548` | `sub2 = 2` | timebase / right-panel |
| `0x0800A6AC` | `sub2 = 4` | mixed trigger-like |

Each sibling follows the same broad shape:

1. inspect visible packed/top-level state at `0xF68`
2. handle visible `state 2`, visible `state 9`, and at least one additional
   visible state branch
3. use `+0x355` as the re-entry latch
4. either collapse back to visible `2` or re-enter the shared bank emitter

So the real cluster is now better described as:

- `sub2 = 2 / 3 / 4 / 5` matched runtime siblings

not only the smaller `2 / 3 / 4` set.

## 3. Raw `0x0800A834` is a packed-state normalizer above/beside the siblings

The next useful correction is raw `0x0800A834`.

This body:

- starts with its own normal prologue
- dispatches via `tbb [pc, r0]` after `F68 - 1`
- does **not** look like one more ordinary selector sibling
- ends by clearing scratch at `0x20002D50` and re-entering
  `0x0800F908`

Decoded case targets from the raw `TBB` bytes are:

| `F68` value | Raw target | Best current read |
|---|---:|---|
| `1` | `0x0800A874` | larger teardown / hardware reset path |
| `2` | `0x0800A904` | packed-state normalize to `0x0100` |
| `3` | `0x0800A90E` | packed-state normalize to `0x0200`, using `+0xE59` |
| `4` | `0x0800A926` | packed-state normalize to `0x0300` |
| `5` | `0x0800A858` | clear staged right-panel bytes, normalize to `0x0100` |
| `6` | `0x0800A858` | same as `5` |
| `7` | `0x0800A952` | no-op / return |
| `8` | `0x0800A952` | no-op / return |
| `9` | `0x0800A930` | clear `+0x355`, normalize to `0x0100` |

Shared sink:

- clear halfword at `0x20002D50`
- tail-call `0x0800F908`

That makes `0x0800A834` look much more like a **cluster-level normalizer /
cleanup bridge** than a selector-family owner of its own.

## 4. Why the normalizer matters

The normalizer changes how to read the neighboring siblings.

Before this pass, it was easy to picture:

- a hidden chooser somewhere above
- then one direct jump into one of the `sub2 = 2 / 3 / 4` families

The newer raw picture is more layered:

1. enter one of the matched selector siblings
2. stage or consume packed selector state
3. pass through a cluster-level normalizer / cleanup bridge
4. re-enter the shared emitter

So the next useful question is no longer only:

- "who chooses `sub2 = 2 / 3 / 4`?"

It is now also:

- "which runtime/UI paths land in the `sub2 = 5` staged-detail family, and
  which ones fall through the `0x0800A834` normalizer afterward?"

## 5. Best current working model

The local runtime cluster now looks like this:

1. matched selector siblings:
   - `sub2 = 3` acquisition
   - `sub2 = 5` staged-detail / `0x2A` commit
   - `sub2 = 2` timebase / right-panel
   - `sub2 = 4` mixed trigger-like
2. cluster-level normalizer:
   - raw `0x0800A834`
3. shared bank emitter re-entry:
   - raw `0x0800F908`

This is still a static model, but it is tighter than the earlier one where the
`sub2 = 2 / 3 / 4` families looked more detached.

## 6. What this changes

This shifts the next RE step in a helpful way.

The best next branch is probably **not** another pure caller search for the
`sub2 = 2 / 3 / 4` siblings. A better next target is:

- the higher runtime path that chooses between:
  - `sub2 = 5` staged-detail commit
  - `sub2 = 2` timebase/right-panel
  - `sub2 = 3` acquisition
  - `sub2 = 4` mixed trigger-like
- and the nearby cluster logic that normalizes packed state afterward

That is a more realistic stock-like runtime sequence.

## Best Next Move

1. Trace the higher runtime/UI path that routes into the `sub2 = 5` sibling at
   raw `0x0800A418`, because that now looks like the cleanest bridge from the
   staged right-panel bitmap family into the packed-selector cluster.
2. Compare that path directly against the `sub2 = 2` sibling at
   raw `0x0800A548` to see how stock distinguishes staged-detail commit from
   coarse timebase/right-panel work. The dedicated comparison note is:
   [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
3. Treat raw `0x0800A834` as the likely cluster-level cleanup bridge that runs
   after some of those actions, rather than as one more ordinary selector
   helper.
