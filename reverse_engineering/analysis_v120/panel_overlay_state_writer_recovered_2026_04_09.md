# Panel Overlay State Writer Recovered 2026-04-09

Purpose:
- correct the older `+0xE10` writer-gap conclusion
- pin down the first direct runtime writer for the redraw-consume state
- shrink the remaining gap from “missing writer” to “missing owner”

Key references:
- [panel_overlay_state_writer_gap_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_overlay_state_writer_gap_2026_04_08.md#L1)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md#L1)
- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md#L135)
- [overlay_artifact_owner_family_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/overlay_artifact_owner_family_2026_04_09.md#L1)

## 1. Direct `+0xE10 = 1` Writer

A raw-store sweep against the archived V1.2.0 app-slot binary recovered a clean
direct writer for `+0xE10 / DAT_20000F08 == 1`.

Correct addresses:

- raw app:
  - `0x0800A7E4..0x0800A82E`
- project / decompile view:
  - `0x080067E4..0x0800682E`

Critical instructions:

```asm
0x0800A7EC: ldrb.w r1, [r0, #0xF68]
0x0800A7F0: cmp     r1, #2
0x0800A7F2: beq.n   0x0800A80E
...
0x0800A80E: movs    r1, #1
0x0800A810: strb.w  r1, [r0, #0xE10]
0x0800A814: movw    r0, #0x2D6C
...
0x0800A826: movs    r3, #3
0x0800A82C: strb    r3, [r1, #0]
0x0800A82E: b.w     0x0803ECF0
```

So the recovered behavior is:

- if `DAT_20001060 / +0xF68 == 2`
  - set `DAT_20000F08 / +0xE10 = 1`
  - queue selector `0x03` to the `0x20002D6C` display-command queue

This is the first grounded direct producer of the redraw-consume state.

## 2. Local Helper Shape

The same helper has a second small branch for `+0xF68 == 1`:

- read `+0xF5D`
- mask to the high nibble
- if that nibble is not `0xB0`, clear `+0xF5D`
- return

So this helper is not a generic overlay writer. It is a very small
state-conditioned handoff point tied to the visible-state family around
`+0xF68`.

## 3. Corrected `+0xE10` Transition Map

The current grounded runtime map is now:

- `+0xE10 = 1`
  - raw `0x0800A810`
  - project `0x08006810`
  - produced when `+0xF68 == 2`
  - immediately queues display selector `0x03`

- `+0xE10 = 2`
  - raw `0x0800E6CC`
  - project `0x0800A6CC`
  - successful append / commit path after
    `FUN_08036084()` + `FUN_08035ED4()`

- `+0xE10 = 3`
  - raw `0x0800E68E`, `0x0800E69A`, `0x0800E6AC`
  - project `0x0800A68E`, `0x0800A69A`, `0x0800A6AC`
  - preview / metadata failure and rebuild-repair path

- `+0xE10 = 0xFF`
  - raw `0x0801983E`
  - project `0x0801583E`
  - redraw-consume cleanup after re-enumeration and label redraw

- `+0xE10 = 0`
  - raw `0x0803D064`
  - project `0x08039064`
  - `key_task` collapse for the transient `2/3` family

That means the old question “does the downloaded app even contain a direct
`+0xE10 = 1` writer?” is now answered: yes, it does.

## 4. What This Changes

This shrinks the remaining gap a lot.

We no longer need to explain the overlay family as:

- consumer for `1`
- writers for `2/3`
- but no visible `1` producer

The better current model is:

1. a small helper near project `0x080067E4`
   produces `+0xE10 = 1` from visible state `+0xF68 == 2`
2. the artifact family around project `0x0800A66A`
   produces `+0xE10 = 2/3`
3. the redraw ladder around project `0x080156D0`
   consumes `1/2/3`
4. cleanup pushes that family back toward `0xFF` or `0`

So the unresolved part is now the higher owner above the recovered `=1` helper,
not the existence of the primitive write itself.

## 5. Likely Family Placement

The recovered `=1` helper sits immediately before the broader region already
described in
[scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md#L135)
as the `0x08006840` reset / entry normalizer family.

That makes the current best read:

- project `0x080067E4..0x0800694E`
- raw `0x0800A7E4..0x0800A94E`

is one broader owner cluster, not disconnected helpers.

I did not find any direct raw `bl` / `b.w` callers to `0x0800A7E4`, which
supports the “interior cluster label” interpretation rather than a clean
standalone callee.

## 6. Best Next Move

The best next step is now very specific:

1. trace the broader owner around project `0x080067E4..0x0800694E`
2. connect the `+0xF68 == 2 -> +0xE10 = 1 -> queue 0x03` helper to the rest of
   the visible-state normalizer
3. stop treating `+0xE10 = 1` as a missing-data mystery
4. keep the descriptor-family work focused on the later overlay/runtime leaves,
   where the remaining uncertainty is still clearly data-shaped
