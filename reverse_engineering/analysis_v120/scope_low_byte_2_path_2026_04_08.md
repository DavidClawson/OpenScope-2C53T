# Scope Low-Byte 2 Path

Date: 2026-04-08

Purpose:
- isolate the concrete meaning of low byte `2` in `DAT_20001060`
- connect boot restore, scope-visible consumers, and the helper-cluster `2 <-> 9`
  transition
- narrow the next trace to the few remaining upstream setters that could still
  hide the missing scope-enter semantics

Primary references:
- [mode_selector_writer_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md#L174)
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5287)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L6588)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L8974)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30335)

## Executive Summary

Low byte `2` is now the clearest scope-visible state in the overloaded
`DAT_20001060` family.

The currently provable path is:

1. boot can restore low byte `2` from saved byte `+0xF64`
2. boot immediately performs a special `0x20002D50` setup when the restored
   value is `2`
3. the main scope FSM returns early unless `DAT_20001060 == 2`
4. the scope FSM epilog queues display selector `3` only when the byte is still
   `2`
5. the display / osc task only kicks `_osc_semaphore` automatically when the byte
   is `2`
6. the adjacent helper cluster treats `2` as a transition state that can expand
   into `0x050109` and later collapse back from `9` to `2`
7. the `9 -> 2` collapse keys off `DAT_20001062 == 5`, and that byte now looks
   like a packed scope UI / subview selector rather than a generic refresh flag

That makes low byte `2` the best remaining static anchor for the missing
scope-enter path.

## 1. Boot restore into `2`

Boot reads `+0xF64` and, if nonzero, copies its low byte into `DAT_20001060`:

- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5287)

It then branches immediately on the restored value.

When the restored byte is `2`, boot does not enter the USART-enable path first.
Instead it:

- reads `+0x354`
- maps that to either `0x0000` or `0x3C00`
- stores the result to `0x20002D50`

Reference:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5293)

Even without fully decoding `0x20002D50`, this is already a strong hint that
restored `2` is more than just a UI label. It causes immediate scope-side setup
work during boot.

## 2. Scope FSM gate

The main oscilloscope state machine begins with a hard guard:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L6588)

If `DAT_20001060 != 2`, it returns immediately.

That is the cleanest exported proof so far that low byte `2` is the visible
scope-active state at the top level.

## 3. Scope epilog display handoff

At the end of the visible scope path, the same function still checks for `2`:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L8974)

If the byte is still `2`, it queues display selector `3`.

So low byte `2` is not just an entry guard. It persists through the visible
scope pass and controls the display-task handoff at the end too.

## 4. Display / osc task semaphore kick

The display-side task loop also treats low byte `2` specially:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30335)

If `_osc_semaphore` is empty and `DAT_20001060 == 2`, it enqueues a wake item.

That means low byte `2` is the state that keeps the oscilloscope work loop
self-arming in the visible task structure.

## 5. Cluster-side `2 <-> 9` transition

The helper cluster adds a second layer on top of the visible scope-active role.

From
[enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md#L174):

- entering low byte `2` rewrites `_DAT_20001060 = 0x050109`
- sets `cRam2000044d = 1`
- low byte `9` can later send `0x13` / `0x14`
- if `DAT_20001062 == 5`, low byte `9` collapses back to `2`

So low byte `2` looks like the externally visible steady state, while `9` looks
more like an internal follow-on or transitional substate inside the same scope /
command-bank family.

## 6. Why `DAT_20001062 == 5` matters

The helper-cluster collapse condition appears to key off a real scope UI state,
not an unrelated housekeeping byte.

Evidence:

- [scope_ui_draw_main](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L109)
  explicitly notes a switch on `DAT_20001062 & 0xF`
- [draw_channel_info @ 0x0800BDE0](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L3880)
  splits the byte into low and high nibbles, using equality and the high nibble
  as display-list indexes
- [scope_ui_draw_range_list_ch1 @ 0x08015D58](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4356)
  repeats the same packed-nibble list logic
- [draw_oscilloscope_screen @ 0x08015F50](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L4455)
  switches on `ui_state_flags & 0xF` and uses nibble-derived comparisons to
  control highlighted rows and subviews

So the best current read is:

- `DAT_20001062` = packed scope UI / subview selector byte

That makes the cluster behavior easier to interpret:

- low byte `9` may be a transitional scope / command-bank state
- it collapses back to visible scope-active `2` once scope subview `5` is active

## 7. What this rules out

This path makes a few older ideas much weaker.

It weakens:

- "low byte `2` is just one arbitrary member of a flat 0..9 bank table"
- "the missing scope path is most likely hiding in the `=5` or `=10` writers"

because:

- `2` is the only value with a clear boot restore path into scope-visible work
- `2` is the only value with a hard scope-FSM guard in exported code
- `5` and `10` now look more like file/resource browser side states

## 8. Best next move

The next highest-value trace is now very focused:

1. find who saves `+0xF64 = 2`
2. find who sets up the cluster's internal `9` state from visible scope posture
3. find who drives `DAT_20001062` to `5`, since that is the known collapse point
   from `9` back to `2`

If those traces stay inside the downloaded app, we still have a decent shot at
recovering the scope-enter path statically.

If they vanish into the same missing / ambiguous regions that have already shown
up elsewhere, that will strengthen the "vendor image is incomplete for scope RE"
hypothesis further.
