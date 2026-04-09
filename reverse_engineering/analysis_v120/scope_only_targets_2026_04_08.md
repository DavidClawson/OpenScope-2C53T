# Scope-Only RE Targets

Date: 2026-04-08

Purpose:
- separate true scope communication/control paths from the already-corrected
  meter-side `0x080BB3FC` lead
- rank the next scope RE targets by how directly they touch FPGA command flow

Primary references:
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c)
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c)
- [FPGA_PROTOCOL.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/FPGA_PROTOCOL.md)
- [high_flash_table_transition_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_table_transition_map_2026_04_08.md)

## Executive Summary

The real scope control nexus is [scope_main_fsm](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L128), not the smaller scope sub-handlers.

The most useful correction from this pass is:
- [scope_mode_timebase](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L129)
- [scope_mode_trigger](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L130)
- [scope_state_handler](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L160)

do **not** show direct `usart_cmd_queue` / `usart_tx_queue` activity or a
`0x080BB3FC`-style missing high-flash command table in the current decompile
range checks.

Instead, `scope_main_fsm` queues:
- internal command bytes to `usart_cmd_queue` (`0x20002D6C`)
- acquisition trigger bytes to `spi3_data_queue` (`0x20002D78`)

That makes the internal dispatcher behind commands `2, 3, 4, 5, 6` the best
next scope target.

## 1. Negative result: the smaller scope handlers are not the transport choke point

Range scans of the true scope sub-handlers found no direct hits for:
- `_DAT_20002d6c`
- `uRam20002d74`
- `0x080BB3FC`
- other raw UART-word builder patterns

Checked ranges:
- [scope_mode_timebase body](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L9127)
- [scope_mode_trigger body](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L10101)
- [scope_state_handler body](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L15971)

This does not mean they are unimportant. It means they mainly mutate state, and
the actual FPGA-facing queue traffic is centralized higher up.

## 2. Scope queue map inside `scope_main_fsm`

### A. `cmd 2` + trigger byte `1`

At:
- [full_decompile.c:6992](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6992)

Observed:

```c
local_6b = 2;
FUN_0803acf0(_DAT_20002d6c,&local_6b,0xffffffff);
local_6c = 1;
FUN_0803acf0(_DAT_20002d78,&local_6c,0xffffffff);
```

This matches the documented scope bring-up / reconfigure path in:
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c#L651)

Interpretation:
- internal `cmd 2` = acquisition-mode reconfigure
- SPI3 trigger byte `1` = wake the standard acquisition path

### B. trigger byte `8`, then `cmd 5`, `cmd 6`, then trigger byte `6`

At:
- [full_decompile.c:7490](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7490)

Observed:

```c
local_6a = 8;
FUN_0803acf0(_DAT_20002d78,&local_6a,0xffffffff);
FUN_0801efc0();
local_69 = 5;
FUN_0803acf0(_DAT_20002d6c,&local_69,0xffffffff);
local_69 = 6;
FUN_0803acf0(_DAT_20002d6c,&local_69,0xffffffff);
local_6a = 6;
FUN_0803acf0(_DAT_20002d78,&local_6a,0xffffffff);
```

This is one of the strongest true-scope control sites because it:
- updates DAC/comparator-related state just beforehand
- immediately queues two internal FPGA command bytes
- then queues another SPI3 acquisition mode

The existing protocol notes make the likely interpretation:
- internal `cmd 5` / `cmd 6` likely fan out into the scope-side raw command
  families documented as mode 4 / mode 5 in
  [FPGA_PROTOCOL.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/FPGA_PROTOCOL.md#L38)
- SPI3 trigger bytes `8` and `6` are special acquisition modes rather than the
  basic steady-state trigger

Important caution:
- the byte placed on `usart_cmd_queue` is a dispatcher selector, not
  necessarily a single final wire-level `cmd_lo` byte. Parameter encoding and
  some command expansion happen downstream.

### C. trigger byte `1`, then `cmd 2` during scope state transition

At:
- [full_decompile.c:7981](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7981)

Observed:

```c
local_6a = 1;
FUN_0803acf0(_DAT_20002d78,&local_6a,0xffffffff);
local_69 = 2;
FUN_0803acf0(_DAT_20002d6c,&local_69,0xffffffff);
```

This appears in the scope reconfiguration path after channel/range state
adjustment and frontend mux changes. It is another good anchor for “stock
re-arm after state change.”

### D. trigger bytes `4`, `5`, `9` in acquisition-phase progression

At:
- [full_decompile.c:8097](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8097)
- [full_decompile.c:8116](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8116)
- [full_decompile.c:8123](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8123)

Observed:

```c
local_6d = 4;  // CH1-enabled case
local_6d = 5;  // CH2-enabled case
...
local_6e = 4;
local_6e = 5;
local_6e = 9;
```

These line up well with the acquisition-phase notes:
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c#L636)
- [FPGA_PROTOCOL.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/FPGA_PROTOCOL.md#L38)

Interpretation:
- trigger bytes `4` and `5` are the paired dual/bulk acquisition legs
- trigger byte `9` is part of the later phase/freeze tail

### E. `cmd 4` after scope range stepping

At:
- [full_decompile.c:8752](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8752)

Observed:

```c
(&DAT_200000fa)[uVar70] = bVar37 + 1;
if (uVar70 == 0) {
  FUN_080018a4(DAT_200000fa);
}
else {
  FUN_08001a58(DAT_200000fb);
}
local_6b = 4;
FUN_0803acf0(_DAT_20002d6c,&local_6b,0xffffffff);
```

This is the clearest auto-range / frontend-change queue site in the true scope
path.

It matches the existing analysis:
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c#L666)

Interpretation:
- internal `cmd 4` is the best current handle for the stock “range changed,
  push scope config to FPGA” path

### F. `cmd 3` heartbeat at scope exit

At:
- [full_decompile.c:9118](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L9118)

Observed:

```c
if (DAT_20001060 == '\x02') {
  local_70 = 3;
  FUN_0803acf0(_DAT_20002d6c,&local_70,0xffffffff);
}
```

This is the steady-state loop closure already highlighted in:
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c#L584)

Interpretation:
- internal `cmd 3` = scope heartbeat / re-arm

## 3. Scope-specific high-flash refs still present

There are still real scope-side references above the vendor image end, but they
look display/config-like rather than transport-like.

### `0x080BB40C` / `0x080BB40E`

Used in:
- [full_decompile.c:12846](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L12846)
- [full_decompile.c:12902](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L12902)

This is the same halfword table already discussed in:
- [high_flash_scope_indexing_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_scope_indexing_2026_04_08.md)

It is used by trigger-overlay drawing and channel-aware visual/config logic, not
as a raw UART byte source.

### `0x080BBC37` / `0x080BBC55`

Used in:
- [full_decompile.c:4919](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L4919)
- [full_decompile.c:5191](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L5191)

Context:
- they are passed into `FUN_08008154` / common draw paths with explicit
  coordinates, dimensions, and color arguments

That makes them likely:
- missing scope UI resources
- or scope-specific style/config data

They are worth tracking, but they do not currently look like the communication
blocker.

## 4. Ranked next targets

### Priority 1: internal dispatcher for `cmd 2`, `3`, `4`, `5`, `6`

Why:
- these are the actual scope-facing internal command bytes queued from
  `scope_main_fsm`
- they sit exactly on the boundary between scope state and raw FPGA traffic

Best anchors:
- [full_decompile.c:6992](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6992)
- [full_decompile.c:7494](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7494)
- [full_decompile.c:8752](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8752)
- [full_decompile.c:9118](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L9118)

### Priority 2: SPI3 trigger-byte cases `1`, `4`, `5`, `6`, `8`, `9`

Why:
- these are the concrete acquisition modes the scope path is actually arming
- they already line up with the annotated acquisition-mode docs

Good references:
- [remaining_unknowns.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/remaining_unknowns.md#L373)
- [fpga_comms_deep_dive.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_comms_deep_dive.c#L440)

### Priority 3: scope-specific high-flash resources/configs

Targets:
- `0x080BB40C`
- `0x080BB40E`
- `0x080BBC37`
- `0x080BBC55`

Why:
- they may still matter for exact stock scope presentation/config semantics
- but they are no longer the leading transport explanation

## 5. Recommendation

The next RE pass should not start from `0x080BB3FC`.

It should start from the queue sites in `scope_main_fsm` and answer:

1. What does internal `cmd 4` dispatch to?
2. What exact raw command families do internal `cmd 5` and `cmd 6` emit?
3. Which SPI3 trigger-byte case is the first one that should make `PC0` go low
   in normal scope operation?

That is now the shortest path from the stock RE to a concrete bench experiment.
