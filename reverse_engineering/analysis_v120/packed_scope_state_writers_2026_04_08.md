# Packed Scope-State Writers

Date: 2026-04-08

Purpose:
- follow up on the packed scope-state readers at `0xF69..0xF6B`
- verify whether the downloaded app really contains runtime writers for
  `DAT_20001061` / `DAT_20001063`
- identify concrete producer clusters for the true scope-side packed state

Primary references:
- [packed_scope_state_disasm_08003010_08003140_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08003010_08003140_2026_04_08.txt)
- [packed_scope_state_disasm_08004220_08004820_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08004220_08004820_2026_04_08.txt)
- [packed_scope_state_force_followup_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_force_followup_2026_04_08.c)
- [scope_state_packed_readers_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_packed_readers_2026_04_08.c)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)

## Executive Summary

This pass found the first solid runtime writers for the packed scope-state bytes
inside the downloaded vendor image.

That matters because the earlier exact-byte xref pass made `0xF69..0xF6B` look
mostly reader-driven. Raw objdump says otherwise.

The strongest concrete producers are:

1. `0x08003028..0x0800313C`
2. `0x08004226..0x080042D2`
3. `0x080047FA..0x0800481C`

Across those clusters:

- `DAT_20001061` / raw `+0xF69` is actively written as a small phase counter /
  state byte
- `DAT_20001063` / raw `+0xF6B` is actively cleared, decremented, masked down to
  its low nibble, and incremented in higher-level paths
- the `0xF69..0xF6B` family is therefore not just a passive render input; it is
  a live scope-side control bundle in the downloaded app

So the next best RE target is no longer "find any writer at all." It is
"reconstruct the enclosing producer path around these writer clusters."

## 1. `0x08003028..0x0800313C`: reset / collapse / queue producer

The slice in
[packed_scope_state_disasm_08003010_08003140_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08003010_08003140_2026_04_08.txt)
contains several real writes:

- `0x08003036`: `DAT_20001061 = 0`
- `0x08003052`: `DAT_20001061 = 0`
- `0x08003056`: `DAT_20001063 = 0`
- `0x080030D8`: `DAT_20001061 = 0`
- `0x080030DC`: `DAT_20001063 = 0`
- `0x080030EE`: `DAT_20001061 = 1`
- `0x080030F6`: `DAT_20001063 = DAT_20001063 & 0x0F`
- `0x08003116`: `DAT_20001061 = 1`
- `0x0800311E`: `DAT_20001063 = DAT_20001063 & 0x0F`

This cluster also queues display selectors through `0x20002D6C`, including:

- `0x20`, then `0x21`
- `0x13`, then `0x14`
- `0x19`, or `0x14`

That is a strong sign that the packed bytes are being updated together with
display-side selector events, not merely read after the fact.

Current best read of this cluster:

- collapse/reset the packed scope substate
- reduce `DAT_20001063` to its low-nibble subview selector
- notify the display queue about the transition

## 2. `0x08004226..0x080042D2`: decrement / wrap producer

The second useful producer lives in
[packed_scope_state_disasm_08004220_08004820_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_disasm_08004220_08004820_2026_04_08.txt).

At `0x08004226..0x08004236`:

- read `DAT_20001061`
- if zero, synthesize `3`
- otherwise decrement by `1`
- write the result back to `DAT_20001061`

Immediately after that, the code queues selector bytes:

- `0x0B`
- `0x0C`
- `0x0F`
- `0x10`
- `0x11`

So `DAT_20001061` is acting like a small cyclic/phase state that directly feeds
display-queue updates.

This is a much better anchor for the packed family than the old "reader only"
model.

## 3. `0x080047FA..0x0800481C`: increment producer

The same `0x08004220..0x08004820` slice also contains a symmetric producer at
`0x080047FA..0x08004806`:

- read `DAT_20001061`
- if `< 3`, increment by `1`
- otherwise clamp/wrap to `0`
- write back to `DAT_20001061`

Then it again queues selector bytes beginning with `0x0B`.

That confirms `DAT_20001061` is not just a boolean gate. It behaves like a
small multi-step phase/substate byte with both decrementing and incrementing
producers in the real vendor image.

## 4. Reader-side meaning is still consistent

The force-decompiled readers still line up with this producer model:

- [packed_scope_state_force_followup_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_force_followup_2026_04_08.c)
- [scope_state_packed_readers_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_packed_readers_2026_04_08.c)

Key points:

- `0x0800D83C` only renders when `DAT_20001061 == 2`, and decodes
  `DAT_20001063` into marker/highlight coordinates
- `0x0800E03C` and `0x0800E042` key off `DAT_20001063 == 3`
- `0x0800C972` treats `DAT_20001063 == 2` specially when drawing a right-side
  panel item

So the data model now looks like:

- `0xF69` = small phase/substate byte, actively incremented/decremented/reset
- `0xF6B` = packed display/marker selector, actively normalized and stepped

That is stronger than the old wording in which `0xF69..0xF6B` were mostly
described from the reader side.

## 5. Why this matters for the scope blocker

This does not solve the FPGA scope-enter problem by itself, but it does narrow
the next static branch.

The better next question is now:

- which enclosing scope-side handler owns the `0x08003028`, `0x08004226`, and
  `0x080047FA` producer clusters, and how does it feed the visible
  `DAT_20001060 == 2 / 9` path?

That is a better target than:

- continuing to treat `0x08015848` as the main missing scope commit
- or assuming the packed bytes lack writers in the vendor app
