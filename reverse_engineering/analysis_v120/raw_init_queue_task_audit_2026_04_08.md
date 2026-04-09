# Raw Init Queue/Task Audit

Date: 2026-04-08

Update 2026-04-08 (late):
- This note predates the `+0x7001` / `-0x7000` normalization breakthrough.
- The corrected queue split is summarized in
  [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md).
- The most important correction is:
  - `0x20002D6C` is the display-command queue consumed by `display_task`
  - `0x20002D70` is the button-event queue consumed by `key_task`
  - `0x20002D74` is the 16-bit UART TX queue consumed by `dvom_TX`
  - `0x20002D78` is the FPGA/acquisition queue consumed by `fpga_task`
- The earlier “display/key/osc task entries do not match the file” issue is
  now explained by a stored entry-PC bias of `+0x7001`.

Purpose:
- re-derive the queue map and task-create map directly from the raw V1.2.0
  vendor image
- separate what is now solid from the older handwritten assumptions
- explain why the downloaded vendor app still does not line up with the live
  system in a few critical places

Primary references:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3385)
- [master_init_phase3.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/master_init_phase3.c#L340)
- [button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md#L408)
- [dispatcher_table_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatcher_table_contradiction_2026_04_08.md)
- [display_task_force_0803DA50_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_task_force_0803DA50_2026_04_08.c#L3)

## Executive Summary

This pass makes four things much clearer:

1. The queue identities are more trustworthy than the old task labels.
2. `0x20002D6C` is still the strongest candidate for the byte-oriented FPGA
   selector/command queue.
3. `0x20002D70` is probably the button-event queue, not the FPGA selector queue.
4. The task-create sites for `display`, `key`, and `osc` now look actively
   inconsistent with the downloaded vendor app, because their recorded entry
   addresses do not resolve to plausible task code in the raw image.

That means the current blocker has shifted:
- queue mapping is getting firmer
- task-entry / dispatch-table reconstruction from the vendor app is getting less
  trustworthy

## 1. Raw queue map from `0x08025BFA`

The queue-creation block is solid in raw disassembly.

### Queue 1: `0x20002D6C`

At
[init_function_decompile.txt:3385](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3385):

```asm
08025BFA: movs r0, #0x14
08025BFC: bl   xQueueCreate
08025C00: movw r1, #0x2d6c
08025C04: movt r1, #0x2000
08025C08: str  r0, [r1]
```

This is:
- queue length = 20
- item size = 1 byte

So `0x20002D6C` is definitely the first 20x1-byte queue created in init.

### Queue 2: `0x20002D70`

At
[init_function_decompile.txt:3392](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3392):

```asm
08025C0A: movs r0, #0x0f
08025C0C: movs r1, #1
08025C10: bl   xQueueCreate
08025C14: movw r1, #0x2d70
08025C18: movt r1, #0x2000
08025C1C: str  r0, [r1]
```

This is:
- queue length = 15
- item size = 1 byte

The strongest current functional clue is still in
[button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md#L408),
which identifies `0x20002D70` as the button-event queue.

So `0x20002D70` is now better treated as:
- `button_event_queue` or another UI/input event queue

not as the main FPGA selector queue.

### Queue 3: `0x20002D74`

At
[init_function_decompile.txt:3400](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3400):

```asm
08025C1E: movs r0, #0x0a
08025C20: movs r1, #2
08025C24: bl   xQueueCreate
08025C28: movw r1, #0x2d74
```

This is:
- queue length = 10
- item size = 2 bytes

That still matches the existing `usart_tx_queue` interpretation very well,
because the TX-side path uses 16-bit raw command words.

### Queue 4: `0x20002D78`

At
[init_function_decompile.txt:3408](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3408):

```asm
08025C32: movs r0, #0x0f
08025C34: movs r1, #1
08025C38: bl   xQueueCreate
08025C3C: movw r1, #0x2d78
```

This is:
- queue length = 15
- item size = 1 byte

That still lines up well with the `spi3_data_queue` / acquisition-trigger-byte
interpretation.

### Queues 5-7: `0x20002D7C`, `0x20002D80`, `0x20002D84`

These are each created with:
- length = 1
- item size = 0
- queue type = semaphore

So the old “three binary semaphores” reading remains solid.

## 2. Raw task-create map from `0x08025CBE`

The task-name pointers are also solid.

The PC-relative `addw r1, pc, #imm` values land exactly on:
- `display`
- `key`
- `osc`
- `fpga`
- `dvom_TX`
- `dvom_RX`

The strings live at:
- `0x08026B4C` = `display`
- `0x08026B54` = `key`
- `0x08026B58` = `osc`
- `0x08026B5C` = `fpga`
- `0x08026B64` = `dvom_TX`
- `0x08026B6C` = `dvom_RX`

So the names are not guesswork.

## 3. `xTaskCreate` wrapper semantics are now solid

The wrapper at
[decompiled_2C53T_v2.c:34455](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L34455)
is a 5-argument helper:

```c
xTaskCreate(task_code, task_name, stack_words, priority, created_handle)
```

It hardcodes `pvParameters = 0` when it calls the deeper TCB-init helper.

Then the stack-init helper at
[decompiled_2C53T_v2.c:29605](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29605)
writes the task code directly into the initial PC slot:

```c
*(uint *)(param_1 + -8) = param_2 & 0xfffffffe;
```

So the first task-create argument is the real entry PC. This is not a name,
priority, or descriptor pointer mix-up.

## 4. The important inconsistency: three named task entries do not match the file

The six named tasks created in init are:

- `display` -> `0x0803DA51`
- `key` -> `0x08040009`
- `osc` -> `0x0804009D`
- `fpga` -> `0x0803E455`
- `dvom_TX` -> `0x0803E3F5`
- `dvom_RX` -> `0x0803DAC1`

After clearing the Thumb bit, these become:

- `display` -> `0x0803DA50`
- `key` -> `0x08040008`
- `osc` -> `0x0804009C`
- `fpga` -> `0x0803E454`
- `dvom_TX` -> `0x0803E3F4`
- `dvom_RX` -> `0x0803DAC0`

### Valid-looking code entries

These still look plausible in the raw image:
- `0x0803E454` (`fpga`)
- `0x0803E3F4` (`dvom_TX`)
- `0x0803DAC0` (`dvom_RX`)

### Invalid or suspicious entries

These do **not** currently look like valid task entry code in the downloaded
vendor app:

- `0x0803DA50` (`display`) decompiles as a short floating-point helper, not a
  task loop:
  [display_task_force_0803DA50_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_task_force_0803DA50_2026_04_08.c#L6)
- `0x08040008` (`key`) lands in a data/literal-pool-looking region
- `0x0804009C` (`osc`) also lands in a data/literal-pool-looking region

That is too strong to ignore. Since `xTaskCreate` really does treat those values
as entry PCs, at least one of these must be true:

1. the downloaded vendor app is not the same image shape as the live board
2. the current Ghidra/raw-file interpretation is missing a loader/relocation
   layer
3. the task-create region we are reading belongs to a build variant that does
   not match the decompiled task names we have been using elsewhere

## 5. What this means for the scope RE

This pass strengthens two conclusions:

### A. `0x20002D6C` is still the best candidate for the scope selector queue

Because:
- it is the first byte queue created
- `0x20002D70` has separate evidence as button/input traffic
- `0x20002D74` is a much better fit for raw 16-bit TX words
- `0x20002D78` is still the best fit for SPI acquisition trigger bytes

So the scope-side selector sites using `0x20002D6C` remain good anchors.

### B. Task-entry-based reconstruction is now lower-confidence than queue-based reconstruction

The queue map is getting firmer.

The task-entry map for the downloaded vendor app is getting less trustworthy,
especially for the display/key/osc side.

That means future RE should prefer:
- raw queue use sites
- raw stack-init / TX-word builders
- live-board comparisons

over assumptions derived from current task labels alone.

## 6. Best next steps

1. Revisit the three suspicious task entries:
   - `0x0803DA50`
   - `0x08040008`
   - `0x0804009C`
2. Check whether those addresses behave differently in older vendor builds.
3. Compare these exact regions against a real stock dump when possible.
4. Keep treating the scope selector bytes on `0x20002D6C` as real upstream
   control signals, but avoid overcommitting to the current task labels while
   this mismatch is unresolved.
