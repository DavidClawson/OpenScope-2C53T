# Overlay Artifact Owner Family 2026-04-09

Purpose:
- collapse the now-solved BMP-side and `%d.bin`-side helpers into one clearer
  runtime family
- identify what is already explained by the downloaded app versus what still
  looks like missing high-flash data
- decide what the next highest-value trace should be above the helper layer

Key references:
- [FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)
- [FUN_0802EA08](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22906)
- [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)
- [FUN_08036084](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29714)
- [right_panel_resource_siblings_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_siblings_force_2026_04_08.c#L1)
- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md#L1)
- [bcad2_preview_completion_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/bcad2_preview_completion_2026_04_09.md#L1)
- [bcad2_bc859_coupled_lifecycle_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/bcad2_bc859_coupled_lifecycle_2026_04_09.md#L1)

## 1. The Helper Layer Is No Longer The Main Mystery

The software-only results now pin down both sibling artifact helpers:

- [FUN_08036084](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29714)
  can complete end-to-end and produce a valid `Screenshot file/1.BMP`
- [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)
  can complete end-to-end and produce a valid `Screenshot simple file/1.BIN`
- both can run in one chained stock session, as shown in
  [bcad2_bc859_coupled_lifecycle_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/bcad2_bc859_coupled_lifecycle_2026_04_09.md#L1)

So the current blocker is no longer:

- "what do the BMP and `%d.bin` helpers do?"

It is now:

- "which higher runtime owner chooses when to enumerate, rebuild, preview-build,
  and metadata-commit the same entry family?"

## 2. `FUN_08034878()` Already Shows The Shared Family Structure

[FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)
is the strongest shared bridge above the helpers.

For each discovered entry under `0x080BC18B`, it:

1. extracts an entry id with `FUN_0802912C(...)`
2. formats `0x080BCAE5` with that id
3. calls [FUN_0802EA08](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22906)
4. if that succeeds, records the id into the live overlay state
5. otherwise formats `0x080BC859` and calls `FUN_0802E12C()`

That is already a real family, not a bag of unrelated helpers:

- `0x080BC18B` = entry enumeration root
- `0x080BCAE5` = per-entry formatted name/id family
- `0x080BC859` = per-entry `%d.bin` side path
- `FUN_0802EA08()` = screenshot-family existence/validation check
- `FUN_0802E12C()` = create/repair side path on the formatted target

So the app itself already contains a shared enumerator/repair owner above both
artifact helpers.

## 3. `FUN_0802EA08()` Clarifies The Directory Split

[FUN_0802EA08](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22906)
defaults to `0x080BC1B4`, opens that descriptor family, walks entries, and
compares the discovered names against the currently formatted buffer.

That makes the current best role split:

- `0x080BC18B` = enumerated list/entry-root family
- `0x080BC1B4` = screenshot-file directory checked by `FUN_0802EA08()`
- `0x080BCAD2` = concrete BMP artifact path family
- `0x080BC859` = concrete `%d.bin` metadata path family

This is more specific than the older "overlay resources somewhere under `2:/`"
model. The two concrete artifact families are siblings under a broader
entry-indexed overlay/runtime owner.

## 4. The Runtime Leaves Are Now Cleanly Split

The forced runtime slices in
[right_panel_resource_siblings_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_siblings_force_2026_04_08.c#L1)
show three meaningful leaf behaviors above the same indexed family:

### `0x0800A66A`

Single-entry preview + metadata commit:

1. formats `0x080BCAD2`
2. runs `FUN_08036084()`
3. if that succeeds, runs `FUN_08035ED4()`
4. on success:
   - sets `+0xE10 = 2`
   - appends the current `+0xE11` into `+0xE25[]`
   - increments `+0xE1B`
5. forces visible state `2`

This is the cleanest "build one preview artifact, then commit its matching
metadata" path.

### `0x0800AFBC`

Bitmap / multi-slot rebuild:

1. walks staged bits in `+0xE1A`
2. for each selected slot, formats both:
   - `0x080BCAD2`
   - `0x080BC859`
3. calls `FUN_0802E12C()` after each formatted path
4. rebuilds the live list with `FUN_08034878(0x080BC18B)`
5. clears:
   - `+0xE1A`
   - `+0xE1C`
6. forces visible state `5`

This is not the same as the single-entry commit path. It is a staged rebuild /
repair path over a selected set of slots.

### `0x0800B10A`

Single-slot rebuild:

1. formats one `0x080BCAD2` path
2. calls `FUN_0802E12C()`
3. formats one `0x080BC859` path
4. calls `FUN_0802E12C()`
5. rebuilds the list with `FUN_08034878(0x080BC18B)`
6. clears staging and returns to visible state `5`

This is the single-slot counterpart to `0x0800AFBC`, not a duplicate of
`0x0800A66A`.

## 5. The Redraw Ladder Is The First LCD-Visible Consumer

The first clearly LCD-visible consumer after the list is seeded is still the
right-panel redraw ladder described in
[overlay_seed_first_ui_consumer_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/overlay_seed_first_ui_consumer_2026_04_08.md#L1):

- if `+0xE10 == 1`
- re-run `FUN_08034878(0x080BC18B)`
- format the current label with `0x080BCAE5`
- redraw the panel
- clear `+0xE10` to `0xFF`
- force visible state `10`
- queue display selectors `0x24`, then `0x03`

So the full family now looks like:

1. enumerate / validate / repair entry roots
2. choose one of the runtime rebuild or commit leaves
3. redraw/consume the resulting overlay state

That is a much tighter runtime model than the earlier "filesystem helpers
floating under the right panel" picture.

## 6. What Is Still Missing

The remaining uncertainty is now much narrower:

- we still do not have the clean higher chooser that routes into:
  - single-entry commit (`0x0800A66A`)
  - single-slot rebuild (`0x0800B10A`)
  - multi-slot rebuild (`0x0800AFBC`)
- and some descriptor text/data still lives beyond the end of the downloaded
  vendor app image

But the code-flow gap is smaller than before. The unresolved piece is no
longer the helper semantics; it is the event/state chooser above them.

## 7. Best Next Step

The sharpest next trace is now:

1. identify the runtime chooser that selects between:
   - append/commit (`+0xE10 = 2`)
   - rebuild/error (`+0xE10 = 3`)
   - redraw consume (`+0xE10 = 1`)
2. keep treating descriptor reconstruction as the higher-yield path once that
   chooser stops narrowing cleanly

So the "next best thing" is no longer another filesystem-helper experiment.
It is to trace the event/state owner above the now-solved overlay artifact
family.
