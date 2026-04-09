# Button/Event Dispatch Re-Audit 2026-04-08

## Scope

Re-audit the disputed button/event dispatch data surfaces using the corrected
project-data rule from
[ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md):

- for the current Ghidra V1.2.0 project,
  `project_addr = runtime_literal - 0x4000`

Focus window:

- runtime `0x08046520..0x08046590`
- corrected project `0x08042520..0x08042590`

Primary references:

- [ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md)
- [key_task_dispatch_surface_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/key_task_dispatch_surface_contradiction_2026_04_08.md)
- [right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md)
- [button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md)

## Executive Summary

Three conclusions survive the address-model correction:

1. The old Ghidra dump at project `0x08046520..` was looking at the wrong
   bytes for the runtime literals used by `key_task` and the button scan path.
2. After relocating to corrected project `0x08042520..`, the disputed surface
   still does **not** become a plausible `btn_map` or handler table.
3. The corrected neighborhood now looks even more strongly like one contiguous
   exidx/unwind-style metadata run, not two separate button/event tables.

No newly plausible nearby button-map or handler-table surface appeared in the
corrected neighborhood.

## Corrected Surfaces

The addresses that matter in the current project are:

| Runtime literal | Corrected project address | Old interpretation |
|---|---|---|
| `0x08046528` | `0x08042528` | `btn_map[30]` |
| `0x08046544` | `0x08042544` | `key_task` slot 0 |
| `0x08046548` | `0x08042548` | `key_task` base for indexed dispatch |

The old project-side dump at `0x08046520..0x08046590` should now be treated as
stale for this question. It corresponds to raw file offset `0x46520`, not to
runtime literal `0x08046520`.

## What Survives

### 1. The `key_task` indirect dispatch contradiction survives

Raw `key_task` still clearly does:

- load base literal `0x08046548`
- compute `event_id * 4 + base`
- load from `[addr - 4]`
- `blx` through the loaded value

So the runtime dispatch shape is still real.

### 2. The corrected `btn_map` surface is still not a usable 30-byte ID table

Corrected project bytes at runtime `0x08046528` / project `0x08042528` are:

```text
0x08046528: 2F FE FF FF 72 03 00 00 DB F9 FF FF F6 0A 00 00
0x08046538: A1 E9 FF FF AF 71 00 00 ...
```

That is not a plausible 30-byte logical key-ID map.

### 3. The corrected dispatch slots are still not callable Thumb targets

Corrected project words at runtime `0x08046544/48` / project `0x08042544/48`
are:

```text
0x08046544 -> 0xFFFFF08B
0x08046548 -> 0x0000084D
```

Those are not plausible direct `blx` targets for Cortex-M code.

## What Needs Correction

### 1. The earlier project dump at `0x08046520..` must be retired for this question

The earlier contradiction note was directionally right but mixed two address
spaces:

- runtime-linked literal addresses
- project storage addresses in a flat import at `0x08000000`

The corrected project bytes live at `0x08042520..`, not `0x08046520..`.

### 2. The corrected region is better read as one contiguous metadata run

The stronger interpretation after relocation is:

- runtime `0x080464E0..0x08046698`
- corrected project `0x080424E0..0x08042698`

This wider span decodes cleanly as repeating 8-byte pairs with PREL31-like
first words that target nearby code addresses. Representative rows:

```text
0x080464E8: first=FFFFFE3C -> 0x08046324 second=00000358
0x08046528: first=FFFFFE2F -> 0x08046357 second=00000372
0x08046570: first=FFFFF9AF -> 0x08045F1F second=00000B44
0x08046628: first=FFFFFE03 -> 0x0804642B second=000003C9
0x08046680: first=000032A7 -> 0x08049927 second=FFFFED3D
```

That pattern is much closer to exidx/unwind metadata than to either:

- a byte-oriented `btn_map`
- a flat 4-byte Thumb handler table

### 3. The `key_task` base is misaligned relative to the 8-byte pair cadence

If the corrected region is treated as an 8-byte metadata table, `key_task`
dispatch starting at runtime `0x08046544` walks through the middle of those
pairs:

- slot 0 -> `0x08046544` = second word of pair at `0x08046540`
- slot 1 -> `0x08046548` = first word of next pair
- slot 2 -> `0x0804654C` = second word of that pair

That makes the contradiction stronger, not weaker:

- the loaded values are not just "weird pointers"
- the dispatch stride is stepping through alternating columns of a structure
  that already looks non-handler-like

## Nearby Surfaces Checked

I checked the broader corrected neighborhood:

- runtime `0x08046300..0x08046900`
- project `0x08042300..0x08042900`

Results:

- no plausible 30-byte small-byte candidate table surfaced nearby
- no 4-byte-stride Thumb-pointer cluster surfaced nearby

So no nearby surface became newly plausible as a replacement `btn_map` or
`key_task` handler table after the correction.

## Best Current Interpretation

The best current interpretation is now:

1. the raw code literals `0x08046528` and `0x08046548` are still real
2. the corrected data bytes at those literals still do not support the old
   `btn_map` / flat handler-table model
3. the whole corrected neighborhood is more plausibly unwind/metadata data in
   this vendor image

So the surviving conclusion is:

- the button/event semantic owners recovered elsewhere still look useful
- the specific `btn_map` and `key_task` data surfaces in the downloaded V1.2.0
  app remain unresolved even after the address fix

## Concrete Next Addresses To Inspect

1. Pin the exact bounds of the metadata run:
   - runtime `0x080464E0..0x080466A0`
   - project `0x080424E0..0x080426A0`

2. Re-verify the literal producers in raw code:
   - raw `0x08039012..0x08039098` for `key_task` base `0x08046548`
   - raw `0x08039382..0x080393EE` for button-scan base `0x08046528`

3. Re-audit any earlier "bad table" claims that used absolute flash literals in
   this same area with the corrected project addresses:
   - runtime `0x08046520..0x08046590`
   - project `0x08042520..0x08042590`

4. If another event surface is still suspected nearby, inspect outside the
   metadata span first, not inside it:
   - runtime `0x08046300..0x080464DF`
   - runtime `0x080466A0..0x08046900`

## Practical Impact

This closes one ambiguity cleanly:

- the old project dump was at the wrong address

But it does **not** rescue the old button/event table model. After correction,
the same region still fails as a `btn_map` and still fails as a flat
dispatch-table surface.
