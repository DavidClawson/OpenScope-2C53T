# Panel Subview Action Meaning

Date: 2026-04-08

Purpose:
- turn the recovered `E1C = 1` and `E1C = 2` writer bridges into likely
  user-visible scope actions
- distinguish the coarse right-panel/timebase handoff from the staged
  bitmap/toggle handoff
- record the current confidence limits caused by missing high-flash UI/resource
  data

Primary references:
- [panel_subview_writer_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_writer_bridge_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c)

## Executive Summary

The recovered `E1C` writers now look like two different user-visible phases of
the same right-panel scope family:

1. `E1C = 1` is best read as the **coarse right-panel/timebase handoff**
2. `E1C = 2` is best read as the **staged bitmap/toggle handoff**

Both converge on selector `0x2A` and then feed the broad runtime controller, but
their preconditions are too different to describe them as the same action.

## 1. Best read for `E1C = 1`

The `E1C = 1` writer lives in the older broad owner around `0x080032D6`:

- if `E1C == 0`, it first decrements `E1D` by `3`
- it then emits `0x27`, `0x28`
- once the path reaches `E1C == 2`, it rewrites `E1C = 1`
- then emits `0x2A`

That lines up best with the existing timebase interpretation:

- `0x27 / 0x28` is already the strongest timebase/right-panel selector family
- `E1D` behaves like the small editable index inside that family
- `E1C = 1` happens after that coarse adjustment, not before it

Best current read:

- `E1C = 1` is the parent or coarse right-panel/timebase selection state that
  hands off into the later `state 5 / state 6` controller

## 2. Best read for `E1C = 2`

The `E1C = 2` writer lives in the helper-cluster path around `0x08006468`:

- require `E1C == 0`
- require `E1A != 0`
- require one or more active bits in `0xE12..0xE19`
- then write `E1C = 2`
- emit `0x2A`

That path is much more "armed" than the `E1C = 1` writer.

It depends on:

- a live toggle latch in `E1A`
- an already-populated bitmap family in `0xE12..0xE19`

And the earlier helper at `0x0800619E` shows how that state is staged:

- if `E1C == 0` and `E1B != 0`
- and `E1A == 0`
- set `E1A = 1`
- rebuild the bitmap from `E1D`
- emit `0x28`, then `0x26`

Best current read:

- `E1C = 2` is not the parent timebase menu
- it is a second-phase detailed/toggled subview that becomes valid only after
  the bitmap/toggle state is staged

## 3. How the broad controller treats them

The broad controller at `0x08004D60` reinforces that distinction:

- when `E1C == 1`, state `6` takes the `0x2B -> state 5 -> state 12` path
- when `E1C == 2`, state `6` just clears `E1C` and falls back to state `5`
- when `E1C == 2`, state `5` also has the direct `0x26 / 0x28` path

That makes `E1C = 1` look more like a deliberate commit/handoff state, while
`E1C = 2` looks more like an already-armed detail state that can be consumed
directly.

## 4. Confidence limit

One caveat remains important: the exact on-screen labels for this right-panel
family are still partly hidden by missing high-flash resource data.

We can see the draw helper at
[FUN_0801819c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L5227)
rendering the right-panel family from `DAT_20000F14`, but the associated string
or asset data around `0x080BC681`, `0x080BC5CF`, `0x080BC54A`, and
`0x080BC4F2` is not materially recoverable from the downloaded vendor image
alone.

So the semantic labels here are still an inference from control flow, not from
fully recovered UI text.

## 5. Best current practical model

The strongest working model now is:

1. coarse timebase/right-panel selection:
   - `E1D`
   - `0x27 / 0x28`
   - handoff via `E1C = 1`, then `0x2A`
2. staged detailed/toggled subview:
   - `E1A`
   - `0xE12..0xE19`
   - `0x28`, `0x26`
   - handoff via `E1C = 2`, then `0x2A`

That is the clearest action-level split we have had so far for the right-panel
scope path.

## 6. Best Next Move

The next high-value pass is to trace:

1. what exact event or key path increments/decrements `E1D` before the
   `0x27 / 0x28 -> E1C = 1 -> 0x2A` sequence
2. what exact event or key path sets `E1A` and repopulates the bitmap before the
   `E1C = 2 -> 0x2A` sequence

If those paths are recoverable in the downloaded image, we should be able to
turn this into a bench-reproducible stock order for the timebase/right-panel
scope handoff.
