# Mixed `sub2 = 4` To Overlay Bridge 2026-04-09

Purpose:
- test whether the recovered overlay-handoff cluster is really downstream of the
  older mixed `sub2 = 4` runtime family
- shrink the remaining “higher owner” gap above the `+0xE10 = 1` writer
- decide whether this branch is still a loose analogy or now a concrete bridge

Key references:
- [overlay_handoff_normalizer_cluster_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/overlay_handoff_normalizer_cluster_2026_04_09.md#L1)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md#L119)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)
- [panel_overlay_state_writer_recovered_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_overlay_state_writer_recovered_2026_04_09.md#L1)

## 1. The mixed family and the overlay-handoff helper are contiguous

Using the corrected raw-app address rule:

- project address + `0x4000` = raw app address

the older mixed family and the newly recovered overlay helper are not separated
by a new owner boundary.

They form one continuous raw block:

- raw `0x0800A6AC..0x0800A94E`
- project `0x080066AC..0x0800694E`

That is a much stronger result than the earlier “they look nearby” impression.

## 2. The upstream half is the known mixed `sub2 = 4` family

The earlier mixed-family note is still structurally right, but we can now pin it
more concretely to the raw block.

### Visible `state 2` side

At raw `0x0800A720` / project `0x08006720`:

```asm
movw r0, #0x0109
movt r0, #0x0004
str.w r0, [r4, #0xF68]
movs r0, #1
strb.w r0, [r4, #0x355]
b.w  0x0800F908
```

Recovered meaning:

- visible `state 2`
- promote to packed preset `9 / 1 / 4 / 0`
- set latch `+0x355 = 1`
- re-enter the shared emitter

### Visible `state 9` side

At raw `0x0800A6F2..0x0800A778` / project `0x080066F2..0x08006778`:

- if `+0x355 == 0`, return
- if `+0xF6A == 4`, clear:
  - `+0x355`
  - `+0xF68 = 2`
  - `+0xF69 = 0`
  - `+0xF6B = 0`
  - then tail-call `FUN_0800B908`
- otherwise:
  - stage `+0xF69 = 0x0401`
  - clear `+0xF6B`
  - emit selector `0x13`, then `0x14`

That is the exact runtime preset bridge we were already using as the best
current mixed `sub2 = 4` family.

## 3. The next block is the known mixed raw-word gate

Immediately after that visible `state 9` side, the same raw block continues into
the already-grounded mixed `F5D` / raw-word gate:

- raw `0x0800A77C..0x0800A7E0`
- project `0x0800677C..0x080067E0`

Important behavior:

- manipulate `+0xF5D`
- emit raw word `0x02A0`
- set `+0xF2D = 8`
- later emit raw word `0x0503`

So the sequence is not merely:

- preset bridge
- then somewhere else a raw-word gate

It is:

- preset bridge
- then immediately the raw-word gate

inside one contiguous owner block.

## 4. The recovered `+0xE10 = 1` helper is downstream in the same block

The next helper in the same raw block is the newly recovered overlay-handoff
writer:

- raw `0x0800A7E4..0x0800A82E`
- project `0x080067E4..0x0800682E`

Its important branch is:

- if `+0xF68 == 2`
  - write `+0xE10 = 1`
  - queue selector `0x03`

That means the mixed-family continuity is now concrete:

1. visible `state 2` -> preset `9 / 1 / 4 / 0`
2. visible `state 9` -> `0x0401`, `0x13`, `0x14`
3. mixed `F5D` side -> `0x02A0`, later `0x0503`
4. visible `state 2` handoff -> `+0xE10 = 1`, queue `0x03`

That is the cleanest bridge we have yet from the mixed runtime family into the
overlay/redraw family.

## 5. The broader normalizer is the trailing third of the same block

The same owner then rolls straight into the broader region we had already
labeled as a reset / entry normalizer:

- raw `0x0800A840..0x0800A94E`
- project `0x08006840..0x0800694E`

This trailing section still looks like:

- visible / packed state normalization
- scope entry/reset posture seeding
- tail-call into `FUN_0800B908`

So the whole block is now best read as one mixed transition cluster, not:

- one mixed preset owner
- one unrelated overlay writer
- one unrelated normalizer

## 6. Updated bridge model

The best current end-to-end read is:

1. the mixed `sub2 = 4` runtime family owns the parent block
2. that family can:
   - promote visible `state 2` to `9 / 1 / 4 / 0`
   - consume `state 9`
   - emit `0x13 / 0x14`
   - enter the `F5D`-gated `0x02A0 -> 0x0503` path
   - then reach the overlay-handoff helper that raises `+0xE10 = 1`
3. the later overlay/resource family can then consume:
   - `+0xE10 = 1`
   - `+0xE10 = 2`
   - `+0xE10 = 3`

This is a real reduction in uncertainty.

We no longer just have:

- one mixed runtime family
- one overlay family
- some guess that they might be related

We now have a concrete raw block that spans both.

## 7. What This Changes

This makes me more confident that we are making real progress.

The remaining gap is now mostly:

- what higher runtime/UI path chooses this mixed `sub2 = 4` owner in the first
  place

not:

- whether the mixed family and overlay family are even connected

That is a much better place to be.

## 8. Best Next Move

The next trace should move one layer higher:

1. find the parent owner above project `0x080066AC..0x0800694E`
2. map which visible runtime states or logical events route into this mixed
   `sub2 = 4` block
3. compare that parent against the already-mapped compact owner at
   `0x08006548` and the broader runtime controller at `0x08004D60`

That is now the shortest path to turning this from a local cluster map into a
stock-faithful transition sequence.
