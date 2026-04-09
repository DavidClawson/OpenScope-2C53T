# Scope State Commit Bridge

Date: 2026-04-08

Purpose:
- capture the first exact-byte xref pass over the packed scope-state family
- record the recovered `0x08015848` slice without over-reading it as a generic
  scope-mode commit
- separate the redraw/overlay ladder from the already-known command-bank emitter

Primary references:
- [data_xrefs_scope_state_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_scope_state_2026_04_08.txt)
- [data_xrefs_scope_state_adjacent_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_scope_state_adjacent_2026_04_08.txt)
- [scope_state_force_sites_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_force_sites_2026_04_08.c)
- [scope_state_force_sites_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_force_sites_xrefs_2026_04_08.txt)
- [scope_state_packed_readers_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_packed_readers_2026_04_08.c)
- [scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)

## Executive Summary

This pass is still useful, but the raw-objdump follow-up changed the meaning of
the `0x08015848` discovery.

The negative result remains:

- exact-byte xrefs still show no plain runtime byte writer for `DAT_20001062`
- visible scope-side code keeps leaning on the packed `0xF69..0xF6B` family
  more than on direct writes to `0xF6A`

The correction is:

1. `0x08015848` is **not** a generic caller-supplied scope-mode commit helper
2. it is an interior slice inside the larger `0x08015780..0x08015A14`
   redraw/overlay ladder
3. the enclosing body hard-sets `DAT_20001060 = 10` after a
   `FUN_08034878(0x080BC18B)` file/resource path
4. that makes the whole ladder fit a file/resource overlay much better than the
   missing FPGA scope-enter transition

So the practical outcome is:

- keep the packed `0xF69..0xF6B` family prioritized
- de-prioritize `0x08015848` as the main scope-enter lead

## 1. Exact-byte xref results

The first headless Ghidra pass over the packed scope-state bytes still gives a
useful baseline:

- [data_xrefs_scope_state_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_scope_state_2026_04_08.txt)
- [data_xrefs_scope_state_adjacent_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_scope_state_adjacent_2026_04_08.txt)

### `DAT_20001060`

Exact-byte refs include:

- reads from the visible scope/meter handlers
- one direct write slice at `0x08015848`

That recovered slice still matters, but the later raw disassembly proved it is
an interior hard-set to `10`, not a free-standing caller-fed mode commit.

### `DAT_20001062`

Exact-byte refs still show:

- reads from `FUN_08015D58`, `FUN_0800BDE0`, `FUN_0800E79C`, and
  `FUN_08015F50`
- no runtime exact-byte writer

The only explicit write visible in the current exported analysis is still the
boot/config restore path at
[init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5350).

### Adjacent bytes `0x20001061` and `0x20001063`

The adjacent-byte pass also remained read-only:

- `DAT_20001061` reads at `0x080090B2`, `0x0800E03C`, `0x0800D83C`
- `DAT_20001063` reads at `0x0800C972`, `0x0800E042`, `0x0800E550`,
  `0x0800D846`

So the packed `0xF69..0xF6B` family still looks real, but its runtime writes are
not cleanly exposed as simple byte stores in the downloaded image.

## 2. `0x08015780..0x08015A14`: redraw ladder, not generic scope commit

The neighboring forced slices plus the raw objdump are the decisive correction:

- [scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)

The enclosing body reads `*(base + 0xE10)` and branches:

- `== 3` -> `0x08015876`
- `== 2` -> `0x08015884`
- `== 1` -> special path beginning at `0x080157C4`
- else -> common redraw tail at `0x080158BA`

The special `== 1` path then:

1. loads `0x080BC18B`
2. calls `FUN_08034878(...)`
3. stores the returned byte to `*(base + 0xE1B)`
4. clears `*(base + 0xE10) = 0xFF`
5. hard-sets `DAT_20001060 = 10`
6. queues display selectors `0x24`, then `0x03`

The sibling slices at `0x08015876`, `0x08015884`, `0x080158C8`, `0x08015904`,
`0x08015940`, `0x08015990`, and `0x080159CC` redraw progressively smaller
subsets of the same right-panel family.

So this cluster is still real, but it is now much better described as:

- right-panel redraw / overlay ladder

than:

- missing scope-enter / FPGA-arm bridge

## 3. `0x08015848`: interior hard-set to `DAT_20001060 = 10`

Force-decompiling the exact writer still recovered useful local behavior:

- [scope_state_force_sites_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_force_sites_2026_04_08.c#L10)

Local slice behavior:

1. write `*(base + 0xF68) = 10`
2. queue display selector `0x24`
3. queue display selector `0x03`
4. conditionally redraw the right-panel rectangles
5. increment `*(base + 0xF70)`

The important correction is not inside this slice; it is in the enclosing-body
context:

- the real input selector is `*(base + 0xE10)`
- the special path is tied to `FUN_08034878(0x080BC18B)`
- the final `= 10` commit is fixed, not caller-supplied

So `0x08015848` is still worth keeping in the notes, but only as a local anchor
inside the redraw ladder.

## 4. `0x0800B914`: normalized bank emitter, still separate

The forced site at `0x0800B914` still re-confirms the already recovered
command-bank table:

- [scope_state_force_sites_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_force_sites_2026_04_08.c#L53)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md#L144)

So `0x0800B914` remains useful as a normalized bank-emission anchor, but it is
still a separate problem from the redraw/overlay ladder.

## 5. `0x08009314`: low byte `9` is still special

The forced read site at `0x08009314` is still one of the best concrete anchors
for the true scope-side special state:

- [scope_state_force_sites_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_force_sites_2026_04_08.c#L200)

It gates on:

- `DAT_20001060 == 9`

Then it:

1. seeds a display/layout block
2. draws a title/header
3. waits on `_DAT_20002D84`
4. dispatches through the viewport/function-pointer family

That still strengthens the interpretation that low byte `9` is a real runtime
substate layered on top of the visible scope-active `2`, not just a temporary
queue byte.

## 6. What changed

The nearby packed-state readers still make one part of the picture clearer:

- [scope_state_packed_readers_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_packed_readers_2026_04_08.c)

The visible runtime UI helpers in this neighborhood lean much more heavily on
`0xF69` and `0xF6B` than on direct reads or writes of `0xF6A`:

- `0x0800D83C` first requires `*(base + 0xF69) == 2`, then decodes
  `*(base + 0xF6B)` into marker/highlight positions
- `0x0800E03C` and `0x0800E042` split on `*(base + 0xF6B) == 3`
- `0x0800C972` also keys off `*(base + 0xF6B)` and `*(base + 0xF60)`

So the current downloaded image still exposes a packed runtime family centered
on:

- `0xF69` = transition / phase-like gate
- `0xF6B` = display / marker mode byte

while `0xF6A` still looks more like a rendered selector input than a cleanly
written runtime control byte.

## 7. Best next move

The next highest-value branch is now:

1. keep the corrected redraw/overlay interpretation for the
   `0x08015780..0x08015A14` ladder
2. reconnect the packed `0xF69..0xF6B` readers to the true scope-active
   `DAT_20001060 == 2 / 9` path
3. keep looking for wider writers rather than expecting a simple
   `strb ... #0xF6A = 5`

That is a better next cut than more raw command guessing, because the overlay
ladder is now corrected and the remaining unknowns have moved back to the
packed scope-state family.
