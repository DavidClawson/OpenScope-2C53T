# Right-Panel Event Owner Map

Date: 2026-04-08

Purpose:
- trace the higher-level event-owner family that feeds the right-panel `E1D`
  edit path
- determine whether the paired `E1D` handlers are narrow timebase helpers or
  broader stock adjustment actions
- capture the new contradiction around `key_task` dispatch data in the downloaded
  vendor app

Primary references:
- [right_panel_event_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_path_2026_04_08.md)
- [mixed_scope_handlers_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handlers_force_2026_04_08.c)
- [packed_scope_state_disasm_08004220_08004820_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08004220_08004820_2026_04_08.txt)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md)
- [button_map_confirmed.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_map_confirmed.md)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L32347)

## Executive Summary

This pass makes the event-family picture cleaner even though it does not fully
recover the physical key mapping.

1. `FUN_080041F8` and `FUN_080047CC` are best read as the stock
   **adjust-previous** and **adjust-next** owners for the mixed scope/UI family,
   not as narrow one-off helpers.
2. In the right-panel path, those owners feed the `E1D` cursor directly:
   - decrement owner -> `E1D--`
   - increment owner -> `E1D++`
3. The exact physical button mapping is still blocked by the downloaded vendor
   app: the expected `key_task` dispatch surface at `0x08046544/48` now looks
   exidx-like rather than handler-like, and the nearby `btn_map` byte table at
   `0x08046528` still does not look usable either.

So the best current event model is:

- generic adjust-prev owner: `0x080041F8`
- generic adjust-next owner: `0x080047CC`
- staged select/toggle owners: `0x08006120` and `0x080062F8`

Update after the raw owner pass in
[right_panel_stage_entry_event_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_entry_event_split_2026_04_08.md):

- the stage-side pair is now stronger than "staged select/toggle owners"
- raw `0x0800A120` and `0x0800A2F8` look like direct sibling event owners,
  parallel to `0x080081F8 / 0x080087CC`

That is enough to keep moving on the scope choreography even though the exact
physical button labels remain unresolved in the vendor download.

## 1. `key_task` dispatch is real, but the downloaded surface is not a plain handler table

The runtime key path in
[decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L32352)
is straightforward:

1. `key_task` receives a one-byte button/event ID from `_key_queue_handle`
2. if the transient overlay states are not blocking input, it dispatches through
   `(**(code **)((uint)bStack_1 * 4 + 0x8046544))()`

The raw dispatch contradiction is now documented more cleanly in
[key_task_dispatch_surface_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/key_task_dispatch_surface_contradiction_2026_04_08.md).

The short version is:

- raw `key_task` really does `ldr -> blx` through `0x08046544/48`
- but the bytes there do **not** look like handler pointers
- they look much more like an exidx/unwind surface

The same general problem appears one page earlier at the documented `btn_map`
table address `0x08046528`: it still does not decode into a plausible logical
button-ID map in this vendor image.

So the current best read is now stronger:

- `key_task` really does dispatch through a table
- but the downloaded vendor app does not expose that dispatch surface as a
  normal handler table at the literal location the code appears to use

This matches the other table contradictions we already saw in the same vendor
image.

## 2. The paired handlers are broader than the right-panel path

The strongest evidence that `0x080041F8` and `0x080047CC` are generic adjustment
owners is their symmetry across **multiple** `DAT_20001060` top-level cases, not
just state `5`.

### `FUN_080041F8` (`adjust-prev`)

Representative behavior:

- `state 0`: decrement `DAT_20001061`, emit `0x0B, 0x0C, 0x0F, 0x10, 0x11`
- `state 1`: decrement `DAT_20001025` with wrap, emit raw `0x0500 | byte`,
  then `0x1D`, `0x1B`
- `state 4`: decrement the mixed `0x20 / 0x21` family
- `state 5`: decrement `E1D`, emit `0x27`, then `0x28`
- `state 6`: decrement `E1D`, emit `0x29`
- `state 9`: decrement packed state and emit `0x14`

### `FUN_080047CC` (`adjust-next`)

Representative behavior:

- `state 0`: increment `DAT_20001061`, emit the same `0x0B..0x11` bank
- `state 1`: increment `DAT_20001025` with wrap, emit the same raw-word family
- `state 4`: increment the mixed `0x20 / 0x21` family
- `state 5`: increment `E1D`, emit `0x27`, then `0x28`
- `state 6`: increment `E1D`, emit `0x29`
- `state 9`: increment packed state and emit `0x14`

That is much too regular to be "two unrelated timebase helpers." The best fit is
that these are the stock negative/positive adjustment owners for the whole mixed
scope/UI family.

## 3. What this means for the right-panel `E1D` path

Inside the right-panel family, the event interpretation is now stronger:

- `state 5`:
  - adjust-prev -> `E1D--`, then `0x27`, `0x28`
  - adjust-next -> `E1D++`, then `0x27`, `0x28`
- `state 6`:
  - adjust-prev -> `E1D--`, then `0x29`
  - adjust-next -> `E1D++`, then `0x29`

So the right-panel path is not owning its own unique button handlers. It is
reusing the same generic adjust-prev / adjust-next family that other scope/mixed
states also use.

That also explains why the path around `0x08006120` / `0x080062F8` felt
different: those two do not behave like pure increment/decrement owners. They
toggle latches, rebuild bitmaps, and emit `0x26 / 0x28` in different orders.

Best current event-family split:

- `0x080041F8` = adjust-prev
- `0x080047CC` = adjust-next
- `0x08006120` / raw `0x0800A120` = staged single-selection / apply-side owner
- `0x080062F8` / raw `0x0800A2F8` = staged mass-toggle / reverse-side owner

## 4. Why the physical button names are still blocked

We now have the physical button matrix from
[button_map_confirmed.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_map_confirmed.md),
including real `LEFT`, `RIGHT`, `UP`, `DOWN`, `SELECT`, `MENU`, `TRIGGER`, and
others.

We also know from
[button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md)
that the scan logic does **not** push raw matrix positions to `key_task`. It
pushes logical IDs through `btn_map[30]`.

Normally we would finish the mapping like this:

1. matrix position -> `btn_map` logical ID
2. logical ID -> `key_task` handler-table slot
3. handler slot -> `adjust-prev`, `adjust-next`, etc.

But step `1` and step `2` are both currently blocked in the vendor download:

- `btn_map` at `0x08046528` still looks invalid
- the `key_task` dispatch surface at `0x08046544/48` looks exidx-like rather
  than handler-like

So the current safest wording is:

- we have event-family semantics
- we do **not** yet have a trustworthy physical-key-to-event-ID mapping from the
  downloaded app alone

## 5. Practical implication for scope-mode reproduction

This is still useful for firmware reconstruction.

We no longer have to think of the right-panel sequence as a mystery list of raw
selector bytes. We can now model it as:

1. enter state `5`
2. use generic adjust-prev / adjust-next to move the coarse cursor (`E1D`)
3. use the staged select/toggle family to arm `E1A` and populate the bitmap
4. commit through `E1C = 2 -> 0x2A`

That is a much more stock-shaped control model than brute-forcing isolated
selector families.

## 6. Best next move

The next best pass is now narrower:

1. trace the staged select/toggle owners (`0x08006120 / 0x080062F8`) as event
   peers to the new adjust-prev / adjust-next pair
2. look for the first higher-level scope owner that enters state `5` and then
   immediately routes into these four event families
3. treat exact physical button labels as blocked until we have either:
   - a real stock dump of the `0x08046528 / 0x08046544` region, or
   - bench/runtime evidence from a true stock unit
