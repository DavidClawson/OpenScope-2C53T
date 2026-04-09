# Overlay Handoff Normalizer Cluster 2026-04-09

Purpose:
- connect the recovered direct `+0xE10 = 1` writer to the neighboring mixed
  raw-word gate and the broader `0x08006840` normalizer family
- decide whether these are separate helpers or one tighter transition cluster
- narrow the next trace target above the overlay/resource family

Key references:
- [panel_overlay_state_writer_recovered_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_overlay_state_writer_recovered_2026_04_09.md#L1)
- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md#L135)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md#L119)
- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md#L232)
- [overlay_artifact_owner_family_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/overlay_artifact_owner_family_2026_04_09.md#L1)

## 1. Cluster layout

Using the corrected app-slot address rule:

- raw = project/decompiled `+ 0x4000`

the relevant cluster is:

- project `0x0800677C..0x0800694E`
- raw `0x0800A77C..0x0800A94E`

This is tighter than the older “normalizer at `0x08006840` plus unrelated small
helpers” model.

The cluster naturally splits into three adjacent parts:

1. a small mixed raw-word gate
   - project `0x0800677C..0x080067E0`
   - raw `0x0800A77C..0x0800A7E0`
2. the recovered overlay-handoff helper
   - project `0x080067E4..0x0800682E`
   - raw `0x0800A7E4..0x0800A82E`
3. the broader reset / entry normalizer
   - project `0x08006840..0x0800694E`
   - raw `0x0800A840..0x0800A94E`

## 2. The mixed raw-word gate is immediately adjacent

The bytes just before the recovered `+0xE10 = 1` writer are not generic setup.
They are a real mixed command gate:

```asm
0x0800A784:  movs    r0, #0xB0
0x0800A786:  strb.w  r0, [r4, #0xF5D]
...
0x0800A79A:  mov.w   r2, #0x02A0
0x0800A7A0:  strh    r2, [r1]
...
0x0800A7AA:  movs    r0, #8
0x0800A7AC:  strb.w  r0, [r4, #0xF2D]
...
0x0800A7BC:  if (high_nibble(F5D) == 0xB0) send 0x0503
```

So this region still matches the earlier “mixed trigger-like / preview family”
reading:

- it manipulates `+0xF5D`
- emits raw word `0x02A0`
- later emits raw word `0x0503`

That means the overlay-handoff helper did not appear in isolation. It is
touching exactly the same local mixed-state family we already knew from the
`sub2 = 4` branch.

## 3. The recovered overlay-handoff helper sits between them

The next small helper at raw `0x0800A7E4` now has a clean two-way split:

### Branch A: `+0xF68 == 2`

```asm
0x0800A80E:  movs    r1, #1
0x0800A810:  strb.w  r1, [r0, #0xE10]
...
0x0800A826:  movs    r3, #3
0x0800A82C:  strb    r3, [r1]
0x0800A82E:  b.w     xQueueSend(...)
```

Recovered meaning:

- visible state `+0xF68 == 2`
- produce overlay state `+0xE10 = 1`
- queue display selector `0x03`

### Branch B: `+0xF68 == 1`

```asm
0x0800A7FA:  ldrb.w  r1, [r0, #0xF5D]
0x0800A7FE:  and.w   r1, r1, #0xF0
0x0800A802:  cmp     r1, #0xB0
0x0800A806:  movne   r1, #0
0x0800A808:  strbne.w r1, [r0, #0xF5D]
```

Recovered meaning:

- visible state `+0xF68 == 1`
- preserve `+0xF5D` only when its high nibble is `0xB0`
- otherwise clear it

So this helper is best read as a small handoff/sanitizer bridge between the
mixed `F5D` family and the later overlay consume path.

## 4. The broader `0x08006840` owner still looks like a normalizer

The following broad owner still behaves like the reset / entry normalizer we
already documented:

- clear `+0xE1C`, `+0xE12`, `+0xE16`, `+0xE1A`
- write `+0xF68 = 0x0100 / 0x0200 / 0x0300`
- clear `+0x355` on one path
- in another path, when `+0xF68 == 3`, queue `0x08`, `0x18`, `0x19`
- clear scratch at `0x20002D50`
- tail-call `FUN_0800B908`

That still supports the older interpretation that `0x08006840` is not the main
live runtime controller, but a staging/normalization body.

## 5. Updated cluster interpretation

The best current read is now:

1. the mixed local family can stage or emit `0x02A0 -> ... -> 0x0503`
2. the next helper can convert visible state `2` into overlay state `+0xE10 = 1`
   and queue display selector `0x03`
3. the broader `0x08006840` family normalizes or re-seeds packed-visible state
   before tail-calling the shared bank emitter

So this is no longer best modeled as:

- one unrelated mixed TX helper
- one unrelated missing overlay writer
- one unrelated entry normalizer

It now looks like one mixed transition cluster that spans:

- raw-word side
- overlay-handoff side
- visible-state normalization side

## 6. What This Changes

This makes the remaining unresolved question smaller and better shaped.

The main open issue is not:

- “does the downloaded app ever generate `+0xE10 = 1`?”

That is now answered.

The better next question is:

- which higher owner routes runtime posture into this
  `0x0800677C..0x0800694E` mixed handoff cluster?

That is now the shortest code-flow gap between:

- the mixed raw-word family (`0x02A0`, `0x0503`)
- the overlay consume path (`+0xE10 = 1`)
- the later redraw/resource consumers (`+0xE10 = 1/2/3`)

## 7. Best Next Move

The next trace should focus on the parent owner above this cluster:

1. find which runtime/UI branch reaches project `0x0800677C..0x0800694E`
2. compare that parent against the earlier mixed `sub2 = 4` family notes
3. check whether that same parent can also land in the already known
   `state 2 -> 9/1/4/0` bridge

That is now higher-yield than more isolated xref hunting on `+0xE10`.
