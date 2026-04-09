# Selector Writer Audit

Date: 2026-04-08

Purpose:
- separate active writers of `DAT_20001025` / `DAT_2000102e` from passive readers
- decide whether the dynamic `0x0500 | low_byte` path still looks scope-specific
- record the current boundary of what the downloaded vendor image can prove

Primary references:
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- [dynamic_scope_word_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_force_2026_04_08.c)
- [dynamic_scope_word_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_xrefs_2026_04_08.txt)
- [scope_selector_bypass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_selector_bypass_2026_04_08.md)
- [fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30390)
- [high_flash_table_transition_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_table_transition_map_2026_04_08.md)

## Executive Summary

This audit did not surface a new standalone scope-only writer for
`DAT_20001025` or `DAT_2000102e`.

The active drivers are still:

1. init / reset
2. `fpga_task` RX classification
3. `FUN_080028E0` (`meter_data_process`)
4. the paired increment/decrement high-flash helpers around `0x080041F8` and
   `0x080047CC`

The strongest passive consumer found in the downloaded vendor app is a display
formatter / draw path around `0x0800F1C6`, which reads the selector family but
does not drive it.

That means the current selector-byte story is still dominated by the meter-side
state machine, even though the same bytes are later reused by the dynamic raw
word builder at `0x08006120`.

## 1. Active writers: `DAT_20001025`

### Init / reset

The clearest non-runtime writer remains the init block:

- raw `0x08026FEA` resets `DAT_20001025` to `0`

Reference:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5343)

### `fpga_task`

The RX-processing task has two direct writes that matter:

- raw `0x08036D26` writes `DAT_20001025 = 8`
- raw `0x08036D58` writes `DAT_20001025 = 1`

Reference:
- [fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt#L1298)

These are the same special-pattern branches already visible in the recovered
`dvom_rx_task` decompile.

### `FUN_080028E0`

The recovered V1.2.0 decompile still shows `FUN_080028E0` as the broader state
owner that interprets frame patterns and then switches on `DAT_20001025`.

The strongest confirmed rewrite sites are:

- `DAT_20001025 = 1`
- `DAT_20001025 = 8`

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30575)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30644)

### High-flash increment / decrement helpers

The forced functions around `0x080041F8` and `0x080047CC` are still active
writers too:

- decrement-with-wrap over `DAT_20001025`
- increment-with-wrap over `DAT_20001025`

References:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L83)
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L879)

But the earlier transition-map pass still makes the best interpretation clear:

- these look meter-mode stepping related, not true scope-entry control

Reference:
- [high_flash_table_transition_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_table_transition_map_2026_04_08.md#L92)

## 2. Active writers: `DAT_2000102e`

`DAT_2000102e` is more tightly coupled to live RX decode than `DAT_20001025`.

### `fpga_task`

Confirmed direct writes in the RX path are:

- raw `0x08036CC4` -> `0`
- raw `0x08037222` -> `0`
- raw `0x080372EA` -> `0`
- raw `0x0803732A` -> `1`
- raw `0x0803733A` -> `2`
- raw `0x080373B4` -> `2`

Reference:
- [fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt#L1267)
- [fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt#L1782)

### `FUN_080028E0`

The main recovered write cluster is still inside the case-`0` formatter path:

- `DAT_2000102e = 0`
- `DAT_2000102e = 1`
- `DAT_2000102e = 2`

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30424)

### High-flash increment / decrement helpers

Both forced helpers also rewrite `DAT_2000102e` immediately after stepping
`DAT_20001025`:

- `DAT_2000102e = DAT_20001025`
- if `DAT_20001025 != 2`, force `DAT_2000102e = 1`

Reference:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L100)
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L894)

Again, that still looks more like meter-side mode stepping than scope arming.

## 3. Passive readers / formatters

The cleanest passive consumer in the downloaded app is the UI path around
`0x0800F1C6`.

That code:

- reads `DAT_20001025`
- reads `DAT_2000102e`
- reads `DAT_20001027`
- reads `DAT_2000102d`
- conditionally formats a value using `_DAT_20001032`

but it does not write the selector bytes.

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4322)

So this path is display fallout, not a driver.

## 4. Boundary note: `0x08006120` and `0x080062F8`

The new Ghidra xref helper was useful mainly as a boundary check.

For the force-created dynamic helpers:

- `0x08006120`
- `0x080062F8`

Ghidra reports:

- function size `1 byte`
- `0` callers
- `0` callees

Reference:
- [dynamic_scope_word_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_xrefs_2026_04_08.txt#L3)

That strongly suggests these are not trustworthy standalone function boundaries.
They are better treated as useful entry slices inside a larger unnamed body.

The same boundary problem appears for `FUN_080028E0` too:

- Ghidra sees its callees cleanly
- but still reports `0` callers in the current project state

Reference:
- [dynamic_scope_word_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_xrefs_2026_04_08.txt#L15)

So the call graph in the downloaded project is still incomplete around this
family.

## 5. Practical conclusion

The selector-byte family is real and it does feed the dynamic `0x0500` raw-word
builder, but the active writers we can currently prove are still meter-dominated:

- RX classification
- meter-data formatting
- meter-mode stepping

No new scope-only writer emerged from the downloaded vendor image.

That points to two likely next branches:

1. trace the larger enclosing helper cluster around `0x08006060 / 0x08006120 /
   0x080062F8 / 0x08006418`, since the standalone function boundaries are not
   trustworthy and the cluster now clearly mixes raw-word TX with command-bank
   emission
2. move up one level and identify the live writers of `DAT_20001060`, because
   that byte now looks like the wider cluster selector
3. move back to the true scope handlers and ask whether the missing scope path
   bypasses this selector family entirely

The second and third branches are both now stronger than "keep tracing only
`DAT_20001025`."

That branch is now captured in:
- [scope_selector_bypass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_selector_bypass_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
