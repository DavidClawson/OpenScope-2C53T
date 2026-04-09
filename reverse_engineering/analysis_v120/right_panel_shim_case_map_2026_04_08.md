# Right-Panel Shim Case Map at `0x08006548`

Date: 2026-04-08

Purpose:
- decode the compact runtime owner at `0x08006548`
- map its visible-state dispatch to the already recovered right-panel / packed-
  preset helpers
- identify what this owner implies about stock entry and exit order above the
  right-panel event cluster

Primary references:
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [right_panel_handoff_directness_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_handoff_directness_2026_04_08.md)

## Executive Summary

The compact body at `0x08006548` is more informative than it first looked.

Its jump table dispatches directly on visible `DAT_20001060`, and the raw table
bytes from the stock V1.2.0 image decode to this mapping:

| Visible state (`DAT_20001060`) | Target | Best current meaning |
|---|---|---|
| `1` | `0x0800656A` | mixed/f5d gate |
| `2` | `0x08006578` | preset `9 / 1 / 2 / 0` |
| `3` | `0x08006576` | return |
| `4` | `0x08006576` | return |
| `5` | `0x08006592` | direct return to visible state `2` + clear panel staging |
| `6` | `0x080065B2` | direct entry to visible state `5` |
| `7` | `0x08006576` | return |
| `8` | `0x08006576` | return |
| `9` | `0x080065C0` | consume preset / collapse or emit `0x13`, `0x14` |

That means visible `state 6` is now the strongest immediate parent of the
state-`5` right-panel editor in this compact runtime owner.

## 1. Raw jump-table recovery

At `0x0800655C`, the owner executes:

- `ldrb DAT_20001060`
- subtract `1`
- bounds-check against `8`
- `tbb [pc, r1]`

Reading the raw bytes from the V1.2.0 app image at `0x08006560` gives:

- `05 0c 0b 0b 19 29 0b 0b 30`

Using the standard Thumb `TBB` target rule (`base = 0x08006560`,
`target = base + 2 * table[index]`) yields:

- state `1` -> `0x0800656A`
- state `2` -> `0x08006578`
- state `3` -> `0x08006576`
- state `4` -> `0x08006576`
- state `5` -> `0x08006592`
- state `6` -> `0x080065B2`
- state `7` -> `0x08006576`
- state `8` -> `0x08006576`
- state `9` -> `0x080065C0`

So this mapping is grounded in raw image bytes, not just decompiler control flow.

## 2. State `6` is the direct parent of state `5` here

The `state 6` case at `0x080065B2` is minimal:

- write `DAT_20001060 = 5`
- tail-call `FUN_0800B908`

That is the cleanest direct "enter the right-panel editor" shim we currently
have in the downloaded vendor image.

Best current read:

- visible `state 6` is a transient pre-entry state above the right-panel editor
- `state 5` is the stable editor posture itself

This aligns well with the broader controller map where `state 6` already looked
like a transient handoff above `state 5`.

## 3. State `5` is an exit/reset posture in this owner

The `state 5` case at `0x08006592` does not look like editor entry. It does:

- write `DAT_20001060 = 2`
- clear `E1C`
- clear `0xE12`
- clear `0xE16`
- clear `E1A`
- tail-call `FUN_0800B908`

So in this compact owner, visible `state 5` is specifically the posture from
which stock can directly reset back to base visible `state 2`.

That helps explain why the right-panel family feels staged:

- `state 6` enters the editor
- `state 5` is the stable editor posture
- `state 5` can also directly collapse back to base scope `state 2`

## 4. State `2` is the preset bridge, not direct panel entry

The `state 2` case at `0x08006578` writes:

- `0x00020109` to `0xF68`
- `0x355 = 1`
- tail-call `FUN_0800B908`

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 2`
- `DAT_20001063 = 0`

So this compact owner reinforces the packed-state model:

- visible `state 2` is not going straight to a raw command family here
- it first promotes into the internal preset `9 / 1 / 2 / 0`

## 5. State `9` is the preset-consumption side

The `state 9` case beginning at `0x080065C0` splits in two useful ways:

- if `0x355 == 0`, return
- if `DAT_20001062 == 2`, collapse back to visible `state 2` and clear the
  packed bytes
- otherwise stage `0x0201` at `0xF69`, clear `0xF6B`, and emit `0x13`, then
  `0x14`

This confirms that the compact owner contains both sides of the preset bridge:

- visible `state 2` -> preset `9 / 1 / 2 / 0`
- internal `state 9` -> collapse or selector-family emission

## 6. What This Changes

This owner gives a better immediate control order above the right-panel cluster:

1. a higher-level path can enter visible `state 6`
2. this compact owner converts `6 -> 5`
3. the right-panel editor runs in visible `state 5`
4. from visible `state 5`, stock can either:
   - collapse back to base visible `state 2`, or
   - hand off into the packed preset / commit side through the other helpers

That is a better fit than treating `0x08006548` as just another small preset
shim.

## 7. Best Next Move

The next static search should now target two specific inbound questions:

1. who writes or routes into visible `state 6` before `0x08006548` runs
2. who routes into visible `state 2` before the same owner promotes it into
   preset `9 / 1 / 2 / 0`

Those two paths are now the shortest route from the recovered right-panel
runtime controller to a stock-faithful bench sequence we can try to reproduce.
