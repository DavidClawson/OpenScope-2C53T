# Scope Owner UI Action Map

Date: 2026-04-08

Purpose:
- translate the corrected broad owner at `0x08003148` into likely user-visible
  scope actions
- tie the selector families inside that owner back to nearby draw helpers and
  existing protocol semantics
- separate the parts that now look reasonably grounded from the still-open
  mixed-mode ambiguity

Primary references:
- [scope_owner_start_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_start_map_2026_04_08.md)
- [scope_preset_owner_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_preset_owner_force_2026_04_08.c)
- [FPGA_PROTOCOL_COMPLETE.md](/Users/david/Desktop/osc/reverse_engineering/FPGA_PROTOCOL_COMPLETE.md)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c)

## Executive Summary

`FUN_08003148` now looks like a mixed scope-owner whose low-byte-`2` path spans
at least four distinct UI-action families:

1. a six-choice acquisition submenu
2. a timebase submenu
3. a trigger-like preview submenu with 13 shapes/patterns
4. a packed-state commit/collapse path

There is also a remaining auxiliary branch around `0x2C` that still looks
scope-visible but is not yet semantically pinned down.

The most useful correction from this pass is that the `0x20/0x21` and
`0x27/0x28` families are now much easier to place:

- `0x20/0x21` is the best current candidate for the acquisition submenu
- `0x27/0x28` is the best current candidate for the timebase submenu

The `0x08, 0x17, 0x18, 0x19` family is still more ambiguous. It behaves like a
trigger-style preview branch, but the leading `0x08` still carries a meter-side
meaning in the existing protocol notes, so that branch should stay labeled as
"trigger-like / mixed-mode" for now, not as a solved pure-scope trigger path.

## 1. Working map

| Owner branch | State/data touched | Queued selectors | Best current UI read |
|---|---|---|---|
| `DAT_20001062` decrement path | packed selector byte | `0x20`, `0x21` | acquisition submenu |
| `DAT_20000F14` / `bRam20000F15` path | local mode + small index | `0x27`, `0x28` | timebase submenu |
| `DAT_20000F51` preview path | 13-entry preview bank at `0x20000F5A..` | `0x08`, `0x17`, `0x18`, `0x19` | trigger-like preview submenu |
| packed collapse paths | `DAT_20001062`, `DAT_20001063` | `0x13`, `0x14` | packed-state commit / collapse |
| auxiliary decrement path | `DAT_20001058` | `0x2C` | unresolved scope-visible auxiliary mode/page |

## 2. Acquisition submenu: `0x20 / 0x21`

This is the cleanest mapping from this pass.

Evidence:

- inside `FUN_08003148`, one branch decrements / normalizes `DAT_20001062` and
  immediately queues `0x20`, then `0x21`
- existing protocol work already maps `0x20` / `0x21` to acquisition run mode
  and sample depth in
  [FPGA_PROTOCOL_COMPLETE.md](/Users/david/Desktop/osc/reverse_engineering/FPGA_PROTOCOL_COMPLETE.md)
- the nearby draw helper
  [FUN_08015D58 in full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c)
  renders a six-slot list keyed directly off `DAT_20001062`

Best current read:

- `DAT_20001062` is carrying a six-choice acquisition submenu index here
- the `0x20 / 0x21` pair is very likely the stock FPGA-facing acquisition
  submenu path that we still have not reproduced faithfully

## 3. Timebase submenu: `0x27 / 0x28`

This is the next strongest mapping.

Evidence:

- inside `FUN_08003148`, one branch gates on `DAT_20000F14` and
  `bRam20000F15`, then queues `0x27`, then `0x28`
- the existing protocol notes map `0x27 / 0x28` to timebase period / mode
- the sibling helpers at `0x08006120` and `0x080062F8` also sit in the same
  family and drive `0x26 / 0x28`, which matches the broader timebase sequence in
  the protocol notes
- the nearby draw helper
  [FUN_0801819C in decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c)
  draws a small two-icon selection keyed off `DAT_20000F14`

Best current read:

- `DAT_20000F14` is a timebase-adjacent local submenu state
- `bRam20000F15` is likely the small value/index edited within that submenu
- `0x27 / 0x28` is the best current candidate for the user-visible timebase
  control path inside the broad owner

## 4. Trigger-like preview submenu: `0x08, 0x17, 0x18, 0x19`

This branch is useful, but not fully clean yet.

Evidence:

- inside `FUN_08003148`, one branch decrements `DAT_20000F51`, clamps
  `DAT_20001062`, then queues `0x08`, `0x17`, `0x18`, `0x19`
- the same branch fills a 13-entry preview bank at `0x20000F5A..`
- the nearby draw helper
  [FUN_0800E79C in full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c)
  draws a six-wide preview list keyed by `DAT_20000F51` and the packed selector
  window
- `0x17..0x19` still line up well with the recovered trigger-family meanings

Caveat:

- `0x08` still carries a meter-side meaning in the current protocol notes, so
  this sequence should not yet be described as a solved pure-scope trigger
  sequence

Best current read:

- this branch is probably a trigger-style preview / extended-trigger submenu
- but the leading `0x08` suggests there is still mixed-mode or shared-dispatch
  behavior in the downloaded app image

## 5. Packed-state commit / collapse: `0x13 / 0x14`

This family remains consistent with earlier packed-state work.

Evidence:

- multiple branches inside `FUN_08003148`, `FUN_08006418`, and `FUN_08006548`
  use `0x13`, then `0x14` when `DAT_20001062` has not yet reached the target
  packed selector
- once the target selector is reached, the helper clears the latch and
  collapses back to visible state `2`
- the packed-state notes already showed that selectors `2`, `3`, and `5` are
  special collapse points

Best current read:

- `0x13 / 0x14` is not the owner of the visible submenu by itself
- it is the commit / collapse transport that bridges packed substates back to
  the visible scope-active low-byte-`2` state

## 6. Auxiliary unresolved branch: `0x2C`

One remaining branch inside `FUN_08003148` decrements `DAT_20001058` and queues
`0x2C`.

What we can say:

- it is definitely scope-visible, because `DAT_20001058` participates in
  multiple scope draw helpers and selects display assets / variants
- protocol work still treats `0x2C` as mode-8 config rather than a normal
  oscilloscope family

Best current read:

- this is an auxiliary scope-visible mode/page branch
- it is probably not the main blocker for basic scope acquisition bring-up

## 7. Practical implication

This makes the next static RE pass more concrete.

The highest-value unresolved question is now:

- which user-visible actions or buttons inside the stock scope UI move the
  broad owner into its acquisition path versus its timebase path versus its
  trigger-like preview path

That is a much better target than trying to guess individual FPGA bytes in the
dark.

## 8. Best next move

Next pass:

1. trace who seeds `DAT_20001060 == 2` immediately before `FUN_08003148`
2. trace which UI-side callers / button paths land in the acquisition branch
   (`0x20 / 0x21`)
3. trace which ones land in the timebase branch (`0x27 / 0x28`)
4. keep the `0x08, 0x17, 0x18, 0x19` branch labeled as mixed trigger-like
   preview until the leading `0x08` is explained more cleanly

That should give the shortest path from the broad owner to a stock-faithful
runtime sequence we can try in our firmware.
