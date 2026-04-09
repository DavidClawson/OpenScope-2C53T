# Runtime Scope Bank Re-entry

Date: 2026-04-08

Purpose:
- document the newly confirmed runtime tail-calls into `FUN_0800B908`
- identify which helpers re-seed visible scope state `2` versus the internal
  `9` preset path before re-entering the shared selector-bank emitter
- capture the strongest corrected interpretation of the stock scope transition
  surface after the raw call-site audit

Primary references:
- [bank_emitter_disasm_0800B908_0800BCE0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/bank_emitter_disasm_0800B908_0800BCE0_2026_04_08.txt)
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c)

## Executive Summary

The raw call-site audit changes one important assumption.

`FUN_0800B908` is not just a one-shot boot helper in practice. The downloaded
vendor app re-enters the same bank-emitter body from multiple runtime helpers
after staging `DAT_20001060..DAT_20001063`.

That means the scope-side choreography is not only:

- "set one mode byte once at boot"

It is also:

- "preset the packed scope-state bundle"
- "optionally collapse back to visible scope state `2`"
- "then tail-call the shared selector-bank emitter again"

This is a much better fit for the bench results than the older model where we
treated the runtime scope problem mostly as a missing raw selector family.

## 1. Concrete runtime re-entry sites

The following sites tail-call or branch to `FUN_0800B908` in the stock app:

- `0x080030E4`
- `0x080051D6`
- `0x0800533A`
- `0x08005398`
- `0x08006030`
- `0x0800604A`
- `0x08006464`
- `0x08006500`
- `0x0800658E`
- `0x080065AE`
- `0x080065BC`
- `0x080065EA`
- `0x0800671C`
- `0x08006736`
- `0x0800694E`

Not every caller is equally well understood yet, but the important correction
is already firm: the bank emitter is shared by runtime helper paths, not only
boot.

## 2. Runtime helpers that re-seed visible scope state `2`

Several helpers explicitly restore the externally visible scope-active state
before re-entering `FUN_0800B908`.

### `0x080030CC`

This small helper does:

- clear latch `+0x355`
- `DAT_20001060 = 2`
- clear `DAT_20001061`
- clear `DAT_20001063`
- tail-call `FUN_0800B908`

This now looks like a general "collapse back to base scope state and redraw"
entry point.

### `0x08006018 .. 0x08006030`

This helper collapses when `DAT_20001062 == 3`:

- clear latch `+0x355`
- `DAT_20001060 = 2`
- clear `DAT_20001061`
- clear `DAT_20001063`
- tail-call `FUN_0800B908`

### `0x080064E8 .. 0x08006500`

This is the corresponding collapse helper for `DAT_20001062 == 5`:

- clear latch `+0x355`
- `DAT_20001060 = 2`
- clear `DAT_20001061`
- clear `DAT_20001063`
- tail-call `FUN_0800B908`

### `0x080065D2 .. 0x080065EA`

This is the corresponding collapse helper for `DAT_20001062 == 2`:

- clear latch `+0x355`
- `DAT_20001060 = 2`
- clear `DAT_20001061`
- clear `DAT_20001063`
- tail-call `FUN_0800B908`

### `0x08006704 .. 0x0800671C`

This sibling performs the same visible-scope collapse for a fourth runtime
target:

- compare local selector/state against `4`
- on match, clear latch `+0x355`
- `DAT_20001060 = 2`
- clear `DAT_20001061`
- clear `DAT_20001063`
- tail-call `FUN_0800B908`

So the "collapse checkpoints" are broader than our earlier `2 / 3 / 5` model.
There is now concrete evidence for a fourth runtime target flowing back into
visible scope state `2`.

### `0x08006592 .. 0x080065AE`

This helper is especially interesting because it restores visible scope state
without going through a packed-target compare first.

It does:

- `DAT_20001060 = 2`
- clear `DAT_20000E1C`
- clear `DAT_20000E12`
- clear `DAT_20000E16`
- clear `DAT_20000E1A`
- tail-call `FUN_0800B908`

Best current read:

- this is a direct "return to base scope panel/subview" helper, not just a
  packed-collapse shim

That makes it one of the best current anchors for reconstructing the visible
scope UI transition order.

## 3. Runtime helpers that preset the internal `9` path

Several siblings stage a full or partial internal `9` preset, then re-enter the
shared emitter.

### `0x08006034 .. 0x0800604A`

Writes:

- `0x00030109` to `0xF68`
- latch `+0x355 = 1`
- tail-call `FUN_0800B908`

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 3`
- `DAT_20001063 = 0`

### `0x08006578 .. 0x0800658E`

Writes:

- `0x00020109` to `0xF68`
- latch `+0x355 = 1`
- tail-call `FUN_0800B908`

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 2`
- `DAT_20001063 = 0`

### `0x08006720 .. 0x08006736`

This pass surfaced a new sibling preset that was not captured in the earlier
preset note:

- `0x00040109` to `0xF68`
- latch `+0x355 = 1`
- tail-call `FUN_0800B908`

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 4`
- `DAT_20001063 = 0`

That extends the known internal preset family from selector targets `2 / 3 / 5`
to at least `2 / 3 / 4`.

## 4. Direct runtime top-level writes that also re-enter the emitter

Two other helpers are worth keeping on the map even though their full UI meaning
is not pinned down yet.

### `0x080065B2 .. 0x080065BC`

This helper does:

- `DAT_20001060 = 5`
- tail-call `FUN_0800B908`

That makes visible state `5` a real runtime re-entry state, not just a table
row.

### `0x0800694E`

This broader helper also tail-calls `FUN_0800B908` after resetting parts of the
state bundle and `0x20002D50`.

Best current read:

- a more global scope-side reset/entry helper
- still worth tracing, but lower confidence than the smaller `0x060xx /
  0x065xx / 0x067xx` helpers

## 5. Why this matters for the scope blocker

This is the strongest structural explanation yet for why our live command sweeps
have stayed silent in scope mode.

Stock appears to do more than emit selector bytes. It repeatedly:

1. seeds visible or internal scope mode (`2`, `5`, or internal `9`)
2. stages packed selector bytes in `DAT_20001061..DAT_20001063`
3. re-enters the shared bank emitter at `FUN_0800B908`

So a stock-faithful runtime reproduction probably needs the preset step and the
re-entry step together, not just a flat replay of selector-bank bytes.

## 6. Practical implication for the current branch map

This sharpens the previous owner/action notes in two ways.

First:

- the broad owner at `0x08003148` is still the best place to map acquisition,
  timebase, and trigger-like branches

Second:

- the narrower preset helpers are now clearly runtime bridges into the shared
  emitter, not detached curiosities

So the current best working model is:

- `0x08003148` owns the mixed scope submenu logic
- `0x08006010 / 0x080064E0 / 0x08006548 / 0x08006700` provide narrower preset
  and collapse shims
- all of them can re-enter `FUN_0800B908` to emit the actual selector bank

## 7. Best next move

The next highest-value trace is now:

1. map which user-visible UI actions land in the direct visible-scope reset
   helper at `0x08006592`
2. map which UI actions land in the `9/1/3/0`, `9/1/2/0`, and `9/1/4/0` preset
   helpers
3. compare those preset targets against the broad-owner branches:
   - `0x20 / 0x21`
   - `0x27 / 0x28`
   - `0x08, 0x17, 0x18, 0x19`

That is the shortest remaining static path from scope UI gestures to the exact
shared emitter entry order stock uses.
