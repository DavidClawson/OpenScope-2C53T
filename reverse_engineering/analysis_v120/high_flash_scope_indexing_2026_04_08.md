# High-Flash Scope Table Indexing

Date: 2026-04-08

Primary artifact:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c)

## Executive Summary

Two missing high-flash tables remain the best scope-path leads, but they do not play the same role:

- `0x080BB3FC` is the stronger protocol blocker. In the forced scope-gap functions, it behaves like an **8-entry byte table** whose selected byte is immediately turned into a raw UART word of the form `0x0500 | value` and queued to the FPGA.
- `0x080BB40C` looks more like a **16-bit mode/render/config table**. Its entries are read as halfwords via a 4-byte row stride and then used in display/config calculations. It may still matter for scope correctness, but it does **not** look like the direct missing UART byte source in the same way `0x080BB3FC` does.

## Correction After Caller Tracing

A later pass traced the same logic back into `dvom_rx_task` and the known
`FUN_080028E0` meter formatter/dispatcher path. That means the direct
`0x080BB3FC` access pattern documented below is **very likely meter-side, not
scope-side**.

See:
- [high_flash_table_transition_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_table_transition_map_2026_04_08.md)
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md)

So this note is still useful for understanding the table shape, but it should
no longer be read as evidence that `0x080BB3FC` is the primary remaining scope
bring-up blocker.

## Additional Correction After Producer Tracing

A newer pass also showed that `FUN_080041F8` and `FUN_080047CC` are not narrow
"missing table helpers." They are much larger mixed-purpose handlers that switch
on `DAT_20001060`.

In particular:

- `case 0` is now clearly scope/display-side packed-state work that increments
  or decrements `DAT_20001061` and queues selector bytes `0x0B/0x0C/0x0F/0x10/0x11`
- `case 1` is the branch that touches `0x080BB3FC` and emits the
  `0x0500 | value` raw word

See:
- [packed_scope_state_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_writers_2026_04_08.md)

So the most accurate reading is now:

- `0x080BB3FC` is still real and still directly drives raw UART words
- but it lives inside a broader mixed handler, not a clean scope-only entry path
- and the same handlers also own the packed `0xF69..0xF6B` scope-side producers

## 1. `0x080BB3FC`: direct raw-command lookup

The best evidence comes from the forced decompile of `FUN_080041f8` and `FUN_080047cc`.

### `FUN_080041f8` case 1: decrement with wrap

Relevant lines:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L85)

Observed logic:

```c
if ((bRam20001055 & 0xf0) == 0xb0) {
  return 0xb0;
}
bVar14 = DAT_20001025 == '\0';
DAT_20001025 = DAT_20001025 - 1;
if (bVar14) {
  DAT_20001025 = 7;
}
sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
FUN_0803acf0(uRam20002d74,&sRam20002d54,0xffffffff);
uRam20002d53 = 0x1d;
FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
uRam20002d53 = 0x1b;
FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
DAT_2000102e = DAT_20001025;
if (DAT_20001025 != 2) {
  DAT_2000102e = 1;
}
```

### `FUN_080047cc` case 1: increment with wrap

Relevant lines:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L880)

Observed logic:

```c
bVar11 = 0;
if (DAT_20001025 < 7) {
  bVar11 = DAT_20001025 + 1;
}
DAT_20001025 = bVar11;
sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
FUN_0803acf0(uRam20002d74,&sRam20002d54,0xffffffff);
...
DAT_2000102e = DAT_20001025;
if (DAT_20001025 != 2) {
  DAT_2000102e = 1;
}
```

### What this implies

The consistent pattern is:

1. Update `DAT_20001025` as a cyclic selector in the range `0..7`
2. Read one byte from `0x080BB3FC + DAT_20001025`
3. Promote it to a raw word `0x0500 | byte`
4. Queue it directly to the raw UART-word queue at `uRam20002d74`
5. Queue internal commands `0x1D` and `0x1B` immediately afterward

That is still useful for understanding the raw-word builder path, but the
surrounding handler is now known to be mixed-purpose. So this branch should be
read as a concrete meter/raw-word subcase inside a larger `DAT_20001060`
dispatcher, not as a clean scope-only entry helper.

## 2. `DAT_20001025` and `DAT_2000102E`: current RAM labels are probably misleading here

Current state docs label:
- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L253): `DAT_20001025` = `state[0xF2D]` = `meter_mode`
- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L259): `DAT_2000102E` = `state[0xF36]` = `meter_overload`

That label set is probably too meter-centric for these scope-gap functions.

Inside the missing-table callers:
- `DAT_20001025` behaves like a scope-side cyclic selector over 8 entries
- `DAT_2000102E` shadows that selector, except stock collapses every value except `2` down to `1`

So for now the safest wording is:
- `DAT_20001025` = `state[0xF2D]`, currently labeled `meter_mode`, but clearly reused or misnamed in the scope-gap path
- `DAT_2000102E` = `state[0xF36]`, currently labeled `meter_overload`, but also participating in the same scope-side selection logic

## 3. `0x080BB40C`: 16-bit table with row/column selection

This table shows up in several forced scope-gap callers, but the access pattern is different.

### UI-side halfword selection (`FUN_0800BFF4`)

Relevant lines:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L1605)

Observed access:

```c
*(undefined2 *)
 ((uint)DAT_2000105b * 4 + 0x80bb40c +
  ((int)((uint)bRam2000044c << 0x18) >> 0x1f) * -2)
```

Interpretation:
- base row = `DAT_2000105B * 4`
- element width = 16 bits
- extra selector = `0` or `-2`, effectively choosing one of two halfwords in the row

### Channel-aware halfword selection (`FUN_0800F5A8` forced body)

Relevant lines:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L3906)
- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L22)

Observed access:

```c
uVar8 = (uint)DAT_2000010d;
if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
  uVar17 = (uint)(DAT_2000010d >> 4);
  uVar23 = (uint)DAT_2000105b;
  ...
  uVar1 = *(ushort *)(uVar23 * 4 + 0x80bb40c + uVar17 * 2);
}
```

Interpretation:
- `DAT_2000010D` is `channel_config`
  - high nibble = active channel index
  - low nibble = enabled channel count
- row = `DAT_2000105B`
- column = `active_channel * 2`
- loaded value = 16-bit halfword

So `0x080BB40C` is best modeled as a compact halfword table indexed by:
- a scope trigger-overlay row selector at `DAT_2000105B`
- a secondary 2-way or per-channel selector

## 4. What `DAT_2000105B` likely means

`DAT_2000105B` is now guarded as a scope trigger-overlay row selector, not a
DMM range source.  The current stock/decompile evidence is:

```text
function_names.md:
  08021b40 scope_draw_trigger_overlay
  08019af8 scope_draw_trigger_marker

ram_map.txt:
  0x2000105B DAT_2000105b (12 refs): FUN_08021b40@08021b40, ...

full_decompile.c:
  12839  uVar10 = (uint)DAT_2000105b;
  12846  *(undefined2 *)(uVar10 * 4 + 0x80bb40c + uVar9 * 2));
  12850  uVar2 = *(undefined2 *)(uVar10 * 4 + 0x80bb40c + uVar9 * 2);
  12899  *(undefined2 *)((uint)DAT_2000105b * 4 + 0x80bb40c));
  12902  *(undefined2 *)((uint)DAT_2000105b * 4 + 0x80bb40e));
```

The same inspected block reads scope/channel drawing state such as
`DAT_2000010d`, `DAT_2000012c`, and the signed channel offset bytes
`DAT_200000fc`/`DAT_200000fd`. It does not call `FUN_080018a4` or
`FUN_08001a58`, does not touch `0x20002D74`, and does not form a raw `0x05xx`
DMM command word. The downloaded V1.2.0 APP image also has a zero-filled
app-slot shadow at `0x080B740C` for the runtime-looking `0x080BB40C` address.

This is the **Scope Trigger Overlay 105B Boundary**: useful scope/UI evidence,
not DMM range proof, not a factory calibration source, and not an explanation
for the low-DCV mismatch.

References:
- [ram_map.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ram_map.txt#L301)

What matters for triage is simpler: `DAT_2000105B` drives a scope
trigger-overlay halfword/style lookup, while `DAT_20001025` drives the direct
DMM raw-command byte from `0x080BB3FC`.

## 5. Practical conclusion

For bring-up, the priority order changes slightly:

1. the packed-state producers in `FUN_080041F8` / `FUN_080047CC`
   - now known to own real runtime writes to `DAT_20001061 / DAT_20001063`
   - likely closer to the true scope-visible `2 / 9` path than the old table-first view

2. `0x080BB3FC`
   - still a real raw-word table
   - but now best treated as the meter/raw-word subcase inside those broader handlers

3. `0x080BB40C`
   - likely still relevant, but more consistent with secondary mode/config/render behavior than the direct protocol blocker

## 6. Suggested next RE step

Use the full mixed handlers around:
- `0x08002FE8`
- `0x080041F8`
- `0x080047CC`

to answer the narrower question:

Which enclosing scope-side path drives the packed `0xF69..0xF6B` producers and
then reaches the visible `DAT_20001060 == 2 / 9` states?

That is now a better next cut than chasing `DAT_20001025` alone.
