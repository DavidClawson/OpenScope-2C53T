# Mode Selector Writer Map

Date: 2026-04-08

Purpose:
- trace concrete write paths for `DAT_20001060`
- separate broad UI / measurement modes from transient command-bank sub-states
- identify the most useful next traces feeding the recovered helper cluster

Primary references:
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3519)
- [usart_protocol_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/usart_protocol_decompile.txt#L421)
- [high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c#L133)
- [normalized_dispatch_entries_2_6_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/normalized_dispatch_entries_2_6_2026_04_08.c#L1533)
- [function_names.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L93)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L3626)

## Executive Summary

`DAT_20001060` is now clearly too overloaded to treat as a simple top-level
"system mode" byte.

The confirmed picture is:

1. boot / config restore seeds it from saved state and defaults it to `8`
2. the USART activation path writes `1` when it enables USART2, resumes the
   `dvom_TX` / `dvom_RX` tasks, and raises `PB11`
3. several visible UI / measurement init helpers consume low-byte values
   `0`, `1`, `2`, and `3`
4. a separate resource / helper path writes `5`
5. a distinct high-flash / normalized-dispatch path writes `10` and immediately
   queues display selectors `0x24` and `0x03`
6. the recovered helper cluster around `0x08006060 / 0x08006120 / 0x080062F8 /
   0x08006418` also rewrites the byte internally, especially through the
   `2 <-> 9` transition path

So the byte behaves more like an overloaded "measurement / display / command-bank
selector" than a single global operating-mode enum.

## 1. Boot and restore path

The init path proves two important things.

First, factory-reset and default paths write `8` directly:

- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3519)
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3825)

Second, later in boot the code reads `base + 0xF64` and copies that byte into
`base + 0xF68` if it is nonzero:

- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5287)

That means `DAT_20001060` is not only assigned ad hoc during runtime. It is also
restored from a saved byte at `+0xF64`.

This matters because the same boot tail immediately branches on the restored
value:

- `1` -> enter the USART2 / task-resume / `PB11 HIGH` path
- `2` -> update `0x20002D50`
- `3` -> branch to a separate init path

So by the end of boot, the byte is already functioning as a mode-dispatch key.

There is also a separate reset/clear path that explicitly zeroes `+0xF64`:

- [fpga_helpers_disasm.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_helpers_disasm.txt#L1889)

So `+0xF64` is behaving like state that is intentionally persisted, restored, and
cleared, not like passive geometry.

## 2. Activation path writes `1`

The clearest direct runtime write to `1` is in the USART activation helper's
common tail:

- [usart_protocol_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/usart_protocol_decompile.txt#L271)

That path:

1. writes `DAT_20001060 = 1`
2. enables USART2
3. resumes `dvom_TX` and `dvom_RX`
4. drives `PB11` high
5. resets measurement-side state

This is not just a passive display label. It is an active transport /
measurement-enable state.

## 3. Visible consumers of `0`, `1`, `2`, and `3`

The exported decompile still shows several named handlers that consume the byte
directly:

- `0` -> [FUN_08009014](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L3626)
  which is currently named
  [meter_mode_init_ac_voltage](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L93)
- `3` -> [FUN_080096E8](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L3717)
  currently named
  [meter_mode_init_dc_voltage](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L94)
- `1` -> [FUN_08009A94](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L3764)
  currently named
  [meter_mode_init_resistance](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L95)
- `2` -> the display task's scope-side wake path
  [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30335)
  and the scope FSM epilog
  [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L8974)

That confirms the low byte is definitely visible at the UI / mode-handler layer.

It also creates an important tension:

- low byte `2` is used as a scope-visible state
- low bytes `0`, `1`, and `3` are consumed by named meter init handlers
- low bytes `5`, `8`, `9`, and `10` also appear elsewhere

So "scope vs meter vs signal-gen" is not the full story. The byte carries finer
sub-states as well.

## 4. High-flash / resource writer to `5`

The forced resource-gap pass at `0x0800AE84` ends with:

- clearing / resetting the `0x20000F12..0x20000F14` display-related family
- formatting / resource lookups around `0x80BC18B`
- then writing `DAT_20001060 = 5`

Reference:
- [high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c#L130)

This does not look like a broad top-level mode switch. It looks more like a
special display / resource / measurement substate transition that later feeds the
cluster's low-byte-`5` bank behavior.

The wider exported context makes that interpretation stronger:

- the same `0x80BC18B` token also appears in
  [fatfs_operation @ 0x0802CFBC](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L20524)
- and in
  [fatfs_init @ 0x0802E7BC](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22735)
  right beside other `fatfs_read_write(...)` path tokens

So the current best read of `= 5` is:

- file / resource browser side state

not:

- primary scope-enter state

## 5. Repeated dispatch writer to `10`

The cleanest non-init write to a value outside the `0..9` bank table is the
`0x08015780..0x08015A14` redraw/overlay ladder:

- [scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)

When `*(base + 0xE10) == 1`, that path:

1. loads `0x080BC18B`
2. calls `FUN_08034878(...)`
3. stores the returned byte to `*(base + 0xE1B)`
4. clears `*(base + 0xE10) = 0xFF`
2. writes `DAT_20001060 = 10`
6. queues display selector `0x24`
7. queues display selector `0x03`

That makes `10` look like a special post-event or overlay state layered on top
of the normal bank table, not part of the simple `0..9` family.

The surrounding context strengthens that too:

- `FUN_08034878` lives in the same FatFs/listing family and walks entries under a
  path token, formatting names with `0x80BCAE5`
  [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28279)
- the hard `= 10` write fires immediately after that result is consumed and
  right before selectors `0x24` and `0x03` are queued
- the same ladder branches on `*(base + 0xE10) == 2 / 3 / 1 / 0xFF`, which fits
  a short-lived overlay selector much better than a top-level scope-mode byte

So the current best read of `= 10` is:

- file-browser or post-selection overlay state

not:

- direct scope configuration state

## 6. Internal cluster rewrites

The helper cluster does not just consume `DAT_20001060`; it also rewrites it.

Most importantly, the recovered `0x08006418` path:

- rewrites low byte `2` into `_DAT_20001060 = 0x050109`
- later can collapse `9` back to `2` depending on `DAT_20001062`

Reference:
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md#L174)

So some observed values are not "stable public modes" at all. They are transient
internal states inside the command-bank emission path.

## 7. Documentation conflict at `+0xF64`

This pass surfaced one important stale mapping.

Boot clearly treats `base + 0xF64` as the saved restore byte that can seed
`DAT_20001060`:

- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5287)

But [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L266)
still labels `+0xF64` as `draw_height` with an inconsistent absolute address.

Current best read:

- the existing `+0xF64` display-task mapping is stale or misbased
- the boot restore path is strong enough that `+0xF64` now needs a focused
  re-audit before it is trusted as display-only state

That re-audit already has one positive clue: `DAT_2000105C` is compared against a
live row/index byte in a menu-style UI path, where equality changes the rendered
label style for the selected item:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4686)

That compare lives inside
[draw_oscilloscope_screen @ 0x08015F50](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4428),
which is also the main remaining named ref in
[ram_map.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ram_map.txt#L302).

So the current best read of `+0xF64` is:

- saved selection / restore seed for a measurement or mode list

not:

- framebuffer height

## 8. Best current interpretation

`DAT_20001060` now looks like a shared selector byte with at least three layers
of meaning:

1. broad mode / handler routing
   - visible at the level of meter init handlers and scope gating
2. transport / measurement activation state
   - visible in the `= 1` enable path
3. finer command-bank / transient substate selection
   - visible in the helper cluster and the `= 5` / `= 10` side paths

So the current name "system_mode" is only partially right. It is accurate at the
top level, but too weak to explain the real runtime behavior.

## 9. Best next move

The next highest-value trace is now:

1. audit writers of `+0xF64`, since boot copies that byte into `DAT_20001060`
2. de-prioritize the `= 5` and `= 10` branches as likely file-browser /
   resource-side states unless a later caller trace ties them back into scope
3. move back to true scope callers that consume low byte `2` and the cluster's
   internal `2 <-> 9` transition path
4. only after that, decide whether the scope-enter gap is still in the downloaded
   app or likely missing from the vendor image

That is a better next branch than more raw command permutations, because it
targets the state transitions that actually feed the recovered helper cluster.

The scope-specific narrowing of that branch is now captured in:
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
