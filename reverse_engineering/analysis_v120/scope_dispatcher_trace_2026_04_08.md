# Scope Dispatcher Trace

Date: 2026-04-08

Update 2026-04-08 (late):
- This note predates the corrected queue split in
  [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md).
- The key correction is that `0x20002D6C` is now confirmed as the
  display-command queue, not the direct FPGA/UART selector queue.
- The normalized table `0x08044E74 = 0x0804BE74 - 0x7000` is callable, but its
  handlers look display-side and write into the LCD framebuffer region.
- So the scope-side bytes `2..6` are still real upstream selectors, but they
  are now best read as display/update selectors associated with scope state
  changes.
- The real wire-level anchors are the `0x20002D74` raw-word builders and the
  `0x20002D78` acquisition-trigger cases.

Purpose:
- trace what we can currently say about the internal dispatcher behind the
  scope-side bytes `2`, `3`, `4`, `5`, `6`
- separate those internal selector bytes from the real wire-level TX words
- record the current ambiguity in the queue/task naming

Primary references:
- [scope_only_targets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_only_targets_2026_04_08.md)
- [scope_raw_cmd_recovery_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_raw_cmd_recovery_2026_04_08.md)
- [dispatcher_table_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatcher_table_contradiction_2026_04_08.md)
- [raw_init_queue_task_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_init_queue_task_audit_2026_04_08.md)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30318)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6992)

## Executive Summary

The current best read is:

1. The scope path really does queue internal selector bytes `2`, `3`, `4`, `5`,
   `6` to `0x20002D6C`.
2. Those bytes are **not the final wire-level UART low bytes**.
3. The old "this is the display task" correction was too strong. The raw init
   audit now makes `0x20002D6C` look even more like the first byte-oriented
   selector/command queue, while `0x20002D70` is a better fit for button/input
   traffic.
4. The real contradiction is now the dispatch base itself: the code at
   `0x08036A50` really does `ldr [0x0804BE74 + idx*4]; blx`, but the raw vendor
   image does **not** contain callable Thumb pointers at
   `0x0804BE74`. See
   [dispatcher_table_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatcher_table_contradiction_2026_04_08.md).
5. So for now, bytes `2..6` should be treated as **real upstream scope-side
   selector bytes** whose exact downstream handler mapping is still unresolved.

That makes the real wire-level anchors:
- the `0x20002D54` raw TX-word writers
- the `spi3_data_queue` trigger-byte cases

not the selector bytes themselves.

## 1. Confirmed scope-side selector-byte sites

All of these are in the true scope path inside
[scope_main_fsm](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L128).

### Selector `2`

Queued at:
- [full_decompile.c:6992](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6992)
- [full_decompile.c:7985](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7985)
- [full_decompile.c:8134](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8134)

Observed contexts:
- scope bring-up / reconfigure path
- post-frontend state change
- single-shot capture/freeze path

Existing interpretation:
- most likely “reconfigure acquisition mode” or “commit scope state”

### Selector `3`

Queued at:
- [full_decompile.c:9118](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L9118)

Observed context:
- scope exit epilog when `DAT_20001060 == 2`

Existing interpretation:
- stock “heartbeat” / loop-closure selector

### Selector `4`

Queued at:
- [full_decompile.c:8752](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8752)

Observed context:
- immediately after scope range stepping
- immediately after relay/gain helper call

Existing interpretation:
- stock “range changed, push new scope config” selector

### Selectors `5` and `6`

Queued at:
- [full_decompile.c:7494](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7494)
- [full_decompile.c:7496](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7496)

Observed context:
- immediately after DAC/comparator-related write
- immediately after `FUN_0801efc0()`
- paired with SPI3 trigger bytes `8` and `6`

Existing interpretation:
- advanced scope config selector family, likely tied to the raw word families
  discussed in [scope_raw_cmd_recovery_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_raw_cmd_recovery_2026_04_08.md)

## 2. Confirmed SPI3 trigger-byte sites

The same scope paths also queue bytes to `0x20002D78`:

- trigger `1` at [full_decompile.c:6994](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6994)
- trigger `8` then `6` at [full_decompile.c:7490](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7490)
- trigger `1` at [full_decompile.c:7981](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7981)
- triggers `4`, `5`, `9` at [full_decompile.c:8097](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8097)

Those are stronger than the selector bytes because the SPI3 acquisition task has
a real switch on them in the decompile.

## 3. The important queue-label ambiguity

There is a conflict in the existing notes:

- [FPGA_TASK_ANALYSIS.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md#L11)
  labels `0x08036A50-0x08036ABC` as `usart_cmd_dispatcher`
- but [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30318)
  clearly decompiles that task as `display_task`, receiving from
  `_display_queue_handle` and dispatching via `0x0804BE74`

Observed code:

```c
iVar1 = xQueueReceive(_display_queue_handle,&bStack_1,0xffffffff);
if (iVar1 == 1) {
  (**(code **)((uint)bStack_1 * 4 + 0x804be74))();
}
```

What changed in this pass is:

1. the `"display_task @ 0x08036A50"` read is no longer trustworthy by itself,
   because raw task-create auditing shows several named task entries in the
   downloaded vendor app do not resolve cleanly to plausible task code
2. `0x20002D6C` now looks more like the first byte-oriented command queue than a
   pure display queue, and `0x20002D70` has better evidence as the button-event
   queue
3. the flash contents at `0x0804BE74` are not a valid direct function-pointer
   array in the vendor image, so the dispatch base itself is unresolved

Right now, the evidence is not strong enough to claim the selector bytes `2..6`
map one-to-one onto UART command handlers without qualification.

## 4. What remains solid despite that ambiguity

Two things are still solid:

### A. Selector bytes are upstream of raw TX words

[scope_raw_cmd_recovery_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_raw_cmd_recovery_2026_04_08.md)
already established the three-layer model:

1. internal selector byte queued to `0x20002D6C`
2. runtime helper builds a 16-bit word at `0x20002D54`
3. `dvom_tx_task` sends that word as:
   - TX `[2]` = high byte
   - TX `[3]` = low byte

So even if the queue/task naming is fuzzy, the selector byte is still not the
wire byte.

### B. Raw words remain the best wire-level anchors

Recovered raw TX words:
- `0x02A0`
- `0x0501`
- `0x0503`
- `0x0508`
- `0x0509`
- `0x0514`

Recovered low-byte families:
- CH1-flavored: `0x0C`, `0x0E`, `0x10`, `0x11`
- CH2-flavored: `0x0D`, `0x15`, `0x16`, `0x17`

Those are better bench anchors than the selector bytes `2..6`.

## 5. Best current interpretation of `2..6`

Given the scope-side call sites, the least-overstated summary is:

- `2`: scope reconfigure / commit after state change
- `3`: steady-state scope loop closure
- `4`: range/frontend change propagation
- `5`, `6`: advanced scope config selectors associated with the path that also
  writes DAC/comparator state and special SPI3 trigger bytes

## 6. Best next step

The highest-value next RE move is:

1. resolve why the literal dispatch site at `0x08036A50` points at a
   non-callable on-disk region in the vendor image
2. re-verify the queue/task creation map directly from raw init disassembly
3. compare the `0x0804BE74` region against a real on-device dump when one is
   available
4. only then try to map selectors `2`, `3`, `4`, `5`, `6` to concrete helper
   functions

That will tell us whether the missing scope link is:
- a bad table/base-address assumption in the current RE
- a vendor-download vs programmed-image mismatch
- or a deeper two-stage dispatch mechanism we have not modeled yet
