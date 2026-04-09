# Mixed Cluster Parent Boundary 2026-04-09

Purpose:
- test whether the mixed `sub2 = 4` / overlay handoff cluster is directly owned
  by the next larger raw function
- compare that cluster against the compact `0x08006548` sibling and the broader
  `0x08004D60` runtime controller
- decide whether the next best trace is still a local linear caller search or a
  higher indirect chooser search

Key references:
- [mixed_sub2_4_to_overlay_bridge_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_sub2_4_to_overlay_bridge_2026_04_09.md#L1)
- [right_panel_shim_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_shim_case_map_2026_04_08.md#L1)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md#L1)
- [runtime_owner_cluster_extension_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_owner_cluster_extension_2026_04_08.md#L1)

## 1. The mixed cluster still ends cleanly at `0x0800694E`

The raw continuity check around the current mixed/overlay region now looks like
this:

- raw `0x0800A6AC..0x0800A94E`
- project `0x080066AC..0x0800694E`

The cluster ends with the same shared-emitter tail-call we already knew:

```asm
0x0800A94A: ldmia.w sp!, {r4, r5, r7, lr}
0x0800A94E: b.w     0x0800F908
```

Then a new clean prologue starts immediately after:

```asm
0x0800A954: stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
0x0800A958: sub    sp, #4
```

So the current mixed block is a real owner/cluster boundary, not just an
interior label that happens to run into more of the same body.

## 2. Structural comparison: `0x080066AC` still matches the compact sibling class

The compact owner at project `0x08006548` and the mixed owner at
project `0x080066AC` still look like the same class of runtime sibling:

- same small prologue shape (`push {r4, r5, r7, lr}`)
- same base pointer to `0x200000F8`
- same visible-state focus on `+0xF68`
- same top-level handling of:
  - visible `state 2`
  - visible `state 9`
  - one family-specific active side
- same re-entry latch at `+0x355`
- same shared-emitter tail-call to `0x0800F908`

That keeps the current sibling-family model intact:

- `0x08006548` = compact `sub2 = 2` timebase/right-panel sibling
- `0x080066AC` = compact `sub2 = 4` mixed/trigger-like sibling

## 3. Structural comparison: `0x08006954` does **not** match that sibling class

The next prologue at project `0x08006954` / raw `0x0800A954` looks very
different.

Its first visible-state split is:

- `+0xF68 == 3` -> branch to `0x0800A9E2`
- `+0xF68 == 2` -> stay in the main body
- otherwise return near `0x0800AF20`

Useful signs that it is a different kind of owner:

- much larger register save set
- direct MMIO writes to the `0x4000xxxx` / `0x4001xxxx` peripheral space
- direct use of the SPI/acquisition queue at `0x20002D78`
- concrete selector emission on the `0x20002D6C` side with values:
  - `0x08`
  - `0x18`
  - `0x19`
  - later `0x02`
  - later `0x06`
  - later `0x08`

That is not the compact preset-bridge pattern of `0x08006548` or `0x080066AC`.

## 4. No direct raw call/branch bridge from `0x08006954` into the mixed cluster

The direct raw-branch sweep still did **not** surface:

- a plain `bl` / `b.w` into raw `0x0800A6AC`
- a plain `bl` / `b.w` into raw `0x0800A7E4`
- a plain `bl` / `b.w` into raw `0x0800A834`

And the new owner at raw `0x0800A954` starts after a clean epilogue from the
mixed block, not before it.

So the best current read is:

- `0x08006954` is not the missing linear parent wrapper around the mixed block
- it is a neighboring, heavier hardware/acquisition-oriented owner

## 5. Comparison with the broad controller at `0x08004D60`

The broad controller at project `0x08004D60` still sits one level above the
compact siblings conceptually:

- it dispatches directly on visible `+0xF68`
- it materializes stable visible states like `4`, `5`, `6`, and `9`
- it contains the later preview/posture interior branches

But this boundary check is still useful because it says the mixed
`0x080066AC..0x0800694E` block is **not** simply the next linear chunk of that
controller in the raw image.

So the current hierarchy is still best read as:

1. broad runtime controller: `0x08004D60`
2. compact sibling-owner family:
   - `0x08005FCC`
   - `0x08006548`
   - `0x080066AC`
   - `0x0800A418`
3. neighboring heavier hardware/acquisition owners:
   - now including `0x08006954`

## 6. What This Changes

This is a useful boundary result even though it is partly negative.

We now have better grounds for saying:

- the mixed `sub2 = 4` / overlay handoff cluster is a compact sibling-owner
  block in its own right
- the next raw prologue is **not** its direct linear chooser/parent
- the remaining parent gap is therefore more likely to be:
  - indirect dispatch
  - event/menu routing
  - or a wider non-linear owner relationship

That is a better answer than continuing to assume that the next adjacent raw
function must be the missing wrapper.

## 7. Best Next Move

The next trace should shift from local adjacency to indirect selection:

1. keep treating project `0x080066AC..0x0800694E` as the compact mixed sibling
   cluster
2. stop prioritizing the adjacent `0x08006954` prologue as its most likely
   parent
3. focus instead on the unresolved chooser above the compact sibling family:
   - event/menu routing into `0x08005FCC`
   - event/menu routing into `0x08006548`
   - event/menu routing into `0x080066AC`
4. compare that chooser problem directly against the still-unresolved
   `key_task` dispatch surface and logical-event model

That is now the shortest path to turning the compact sibling map into a real
stock transition graph.
