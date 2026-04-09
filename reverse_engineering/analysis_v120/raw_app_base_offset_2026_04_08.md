# Raw App Base Offset 2026-04-08

## Summary

The downloaded stock app image in:

- `/Users/david/Desktop/osc/archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`

is a standalone **app-slot** binary. When raw-disassembling that file with
`arm-none-eabi-objdump`, the correct VMA is:

- `0x08004000`

But the current Ghidra/decompile-derived notes are using addresses as if the app
were imported at:

- `0x08000000`

So when comparing a decompiled/project address to the archived raw app image,
the correct conversion is:

- `raw_app_addr = decompiled_addr + 0x4000`

This resolves the apparent contradictions that showed up when checking the
right-panel redraw cluster and the key-loop slice against the raw app bytes.

## Confirmed Examples

### 1. Boot descriptor call

Decompiled note:

- `0x08027294` calls `FUN_08034878(0x080BC18B)`

Correct raw app address:

- `0x0802B294`

Raw app disassembly at `0x0802B28C..0x0802B298` shows:

```asm
0x0802B28C: movw r0, #0xC18B
0x0802B290: movt r0, #0x080B
0x0802B294: bl   0x08038878
0x0802B298: strb.w r0, [sl, #0xE1B]
```

So the earlier boot/resource note was structurally right; the raw check just
needed the `+0x4000` shift.

### 2. Key-loop slice

Decompiled note:

- `0x08039008` = key-loop body
- `0x08039058` = interior overlay-state slice

Correct raw app addresses:

- `0x0803D008`
- `0x0803D058`

Raw app disassembly at `0x0803D008..0x0803D098` matches the known loop shape:

- wait on queue at `0x20002D70`
- clear `+0xE10` when `(state & 0xFE) == 2`
- gate on `+0x2F / +0x30 / +0x33 / +0xF2C`
- dispatch through the `0x08046548` table

So the earlier "hidden state-assignment" slice was not disproven by the raw
binary; it was only checked at the wrong absolute address.

### 3. Right-panel redraw cluster

Decompiled slices:

- `0x080156D0`
- `0x080157B4`
- `0x080157D0`

Correct raw app addresses:

- `0x080196D0`
- `0x080197B4`
- `0x080197D0`

Raw app disassembly around `0x08019640..0x08019840` shows that these are not
isolated top-level functions in the archived app image. They are interior slices
inside a broader owner that starts earlier, around:

- decompiled `0x08015640`
- raw `0x08019640`

This is useful because it gives a cleaner next trace surface than treating
`0x080156D0` or `0x080157D0` as standalone roots.

## What This Changes

1. Raw-vs-decompile contradictions in this region should not be trusted unless
   the `+0x4000` app-slot shift has been applied first.
2. Earlier claims that `0x08039058` was "probably unrelated" are superseded.
   The corrected raw check supports the existing right-panel/key-loop model.
3. The right-panel redraw family should now be traced as a larger owner around:
   - decompiled `0x08015640`
   - raw `0x08019640`

## Practical Rule

For the archived V1.2.0 stock app image:

- **decompiled/Ghidra note address** -> add `0x4000` -> **raw app-slot address**

This rule should be used for future objdump, grep, and callsite tracing against
the archived app file.

Update after the dispatch-surface check in
[ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md):

- this `+0x4000` rule is still the right one for **code-location comparisons**
- but when a decompiled function loads an **absolute flash literal**, the
  matching bytes inside the current misbased Ghidra project live at:
  - `project data address = runtime literal - 0x4000`

So code checks and literal-data checks now have different compensation rules.
