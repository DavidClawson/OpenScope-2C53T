# Queue / Dispatch Split

Date: 2026-04-08

Purpose:
- repair the queue/task interpretation after the `+0x7001` / `-0x7000`
  normalization breakthrough
- separate the display-side dispatch table from the real FPGA/UART command path
- record the first confirmed `0x20002D74` raw-word builder sites

Primary references:
- [raw_init_queue_task_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_init_queue_task_audit_2026_04_08.md)
- [scope_dispatcher_trace_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_dispatcher_trace_2026_04_08.md)
- [dispatcher_table_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatcher_table_contradiction_2026_04_08.md)
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [dispatch_table_08044E74_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatch_table_08044E74_2026_04_08.txt)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30318)

## Executive Summary

This pass resolves the most important queue-label confusion:

1. `0x20002D6C` is the byte queue consumed by
   [display_task @ 0x08036A50](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30318),
   not the UART/FPGA command queue.
2. `0x20002D70` is the byte queue consumed by
   [key_task @ 0x08039008](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L32347).
3. `0x20002D74` is the 16-bit queue consumed by `dvom_TX` at `0x080373F4`.
4. `0x20002D78` is the byte queue consumed by `fpga_task` at `0x08037454`.
5. The literal display-table base `0x0804BE74` normalizes to
   `0x08044E74`, which contains a valid callable pointer table and is therefore
   display-side dispatch, not the missing FPGA transport key.

The practical implication is:
- scope-side bytes `2`, `3`, `4`, `5`, `6` queued to `0x20002D6C` are still
  real and useful, but they are now best read as display/update selectors,
  not as the direct missing FPGA command dispatcher.
- the real wire-level anchors are the `0x20002D74` raw-word builder sites and
  the `0x20002D78` acquisition-trigger sites.

## 1. The normalization breakthrough

The task-create block stores biased entry PCs:

- created `display` PC: `0x0803DA51`
- actual display loop: `0x08036A50`
- created `dvom_RX` PC: `0x0803DAC1`
- actual dvom_RX loop: `0x08036AC0`
- created `dvom_TX` PC: `0x0803E3F5`
- actual dvom_TX loop: `0x080373F4`
- created `fpga` PC: `0x0803E455`
- actual fpga loop: `0x08037454`
- created `key` PC: `0x08040009`
- actual key loop: `0x08039008`
- created `osc` PC: `0x0804009D`
- actual osc loop: `0x0803909C`

For all of these, the stored entry is:
- `actual_task_entry + 0x7001`

The same normalization fixes the table bases:

- literal table base in display task: `0x0804BE74`
- normalized code-pointer table: `0x08044E74`

- literal resource-table base in old notes: `0x0804C40C`
- normalized pointer table: `0x0804540C`

## 2. Corrected queue / task map

The raw task loops now line up cleanly with the queue-creation block:

### `0x20002D6C`

Consumed at raw `0x08036A50`:

```asm
0x08036a52: movw   r5, #0x2d6c
0x08036a56: movw   r6, #0xbe74
...
0x08036a78: ldrb.w r0, [sp, #7]
0x08036a7c: ldr.w  r0, [r6, r0, lsl #2]
0x08036a80: blx    r0
```

After normalization, that table base is `0x08044E74`, and the forced entries
write into the LCD framebuffer region around `_DAT_20008358`. So this queue is
best treated as:

- `display_command_queue`

### `0x20002D70`

Consumed at raw `0x08039008`:

```asm
0x0803900a: movw r6, #0x2d70
...
0x08039048: ldr  r0, [r6, #0]
0x08039050: bl   xQueueReceive
...
0x08039090: add.w r0, r8, r0, lsl #2
0x08039098: blx  r0
```

This remains the best fit for:

- `button_event_queue`

### `0x20002D74`

Consumed at raw `0x080373F4`:

```asm
0x080373fa: movw r6, #0x2d74
...
0x08037420: ldr  r0, [r6, #0]
0x08037428: bl   xQueueReceive
...
0x0803743a: strb r1, [r7, #2]
0x08037440: strb r0, [r7, #9]
```

This is the actual 16-bit raw-word queue for UART TX.

### `0x20002D78`

Consumed at raw `0x08037454`:

```asm
0x0803745a: movw r4, #0x2d78
...
0x080374b2: ldr  r0, [r4, #0]
0x080374ba: bl   xQueueReceive
```

This is still the best fit for:

- `fpga_trigger_queue`
- or more generally the acquisition / SPI3 work queue

## 3. What the normalized `0x08044E74` table really is

The normalized table dump in
[dispatch_table_08044E74_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatch_table_08044E74_2026_04_08.txt)
contains a clean pointer run:

- idx `0` -> `0x0800FD39`
- idx `1` -> `0x0800FE9D`
- idx `2` -> `0x080104ED`
- idx `3` -> `0x08010D71`
- idx `4` -> `0x08011B75`
- idx `5` -> `0x08012245`
- idx `6` -> `0x08012345`

The forced decompiles for indices `2..6` all have the same shape:

- they write halfwords into `_DAT_20008358`
- they use `_DAT_20008350/_52/_54/_56` as drawing-window bounds
- they stamp `0x3A29` into repeated grid-like regions

That is display work, not UART command assembly.

## 4. First confirmed `0x20002D74` raw-word builder sites

The first clean raw-word enqueue sites now line up with the recovered bench
word families:

### `0x080033CA`

Writes `0x0508` to `0x20002D74`:

```asm
0x080033dc: mov.w r3, #0x508
0x080033e4: strh  r3, [r1]
```

### `0x08003BA4`

Writes `0x0509` to `0x20002D74`:

```asm
0x08003bb6: movw  r3, #0x509
0x08003bbe: strh  r3, [r1]
```

### `0x080048C0`

Loads a byte from missing high-flash table `0x080BB3FC[index]`, then sends
`0x0500 | value`:

```asm
0x080048d0: ldrb  r0, [r2, r0]
0x080048da: add.w r0, r0, #0x500
0x080048e2: strh  r0, [r1]
```

### `0x08005B7A`

Writes `0x0514` to `0x20002D74`:

```asm
0x08005b8a: movw  r2, #0x514
0x08005b92: strh  r2, [r1]
```

### `0x080060CA`

Writes `0x0501` to `0x20002D74`:

```asm
0x080060da: movw  r2, #0x501
0x080060e0: strh  r2, [r1]
```

### `0x080062A2`

Takes a dynamic halfword already staged in RAM and ORs in `0x0500`, then sends
it to `0x20002D74`:

```asm
0x08006298: ldrh  r0, [r4]
0x0800629e: orr.w r1, r0, #0x500
0x080062ac: strh  r1, [r4]
```

This is stronger than a generic candidate. The branch-table immediately above it
appears to choose the low byte from paired values:

- `0x0C` / `0x0D`
- `0x0E` / `0x17`
- `0x10` / `0x15`
- `0x11` / `0x16`

Those are exactly the CH1/CH2-flavored families recovered earlier. The local
state bytes at `+0xF2D`, `+0xF36`, and `+0xF39` now look like the runtime
selector state feeding that pair choice.

### `0x0800678A`

Writes `0x02A0` to `0x20002D74`:

```asm
0x0800679a: mov.w r2, #0x2a0
0x080067a0: strh  r2, [r1]
```

### `0x080067BC`

Writes `0x0503` to `0x20002D74`:

```asm
0x080067ce: movw  r3, #0x503
0x080067d6: strh  r3, [r1]
```

These sites line up with the previously recovered raw-word set:

- `0x02A0`
- `0x0501`
- `0x0503`
- `0x0508`
- `0x0509`
- `0x0514`

## 5. What changes for the scope RE

The corrected picture is:

1. `0x20002D6C`
   - display/update selectors
   - useful for UI/state sequencing
   - not the direct missing FPGA UART dispatcher

2. `0x20002D74`
   - actual wire-level UART raw words
   - strongest current anchor for the missing scope command semantics

3. `0x20002D78`
   - acquisition trigger / SPI3 work items
   - still important for scope capture timing

So the next high-value static RE target is:

- functions that write to `0x20002D54` and enqueue to `0x20002D74`,
  especially the scope-side dynamic builder around `0x080062A2`
- specifically the caller/state-writer chain documented in
  [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)

not the normalized display table at `0x08044E74`.
