# Scope Selector Bypass

Date: 2026-04-08

Purpose:
- test whether the visible scope handlers in the downloaded vendor app actually
  drive the same selector-byte family as the dynamic `0x0500 | low_byte` path
- decide whether the next RE branch should stay on the selector family or move
  back to the true scope handlers

Primary references:
- [function_names.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L129)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6995)
- [selector_writer_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/selector_writer_audit_2026_04_08.md)
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)

## Executive Summary

The visible scope-side code in the downloaded vendor app appears to interact
primarily with:

- the display selector queue `0x20002D6C`
- the acquisition / SPI queue `0x20002D78`

not with:

- the raw UART word queue `0x20002D74`
- or the `DAT_20001025 / DAT_2000102e` selector family

That does not prove the selector family is irrelevant to scope, but it does make
the next branch much clearer:

- the missing scope activation path likely bypasses the currently recovered
  dynamic selector-byte helpers, or reaches them through an enclosing body that
  is not yet reconstructed cleanly

## 1. What the named scope handlers look like

The current named scope-side handlers are:

- [scope_main_fsm @ 0x08019E98](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L129)
- [scope_mode_timebase @ 0x0801D2EC](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L130)
- [scope_mode_trigger @ 0x0801E1E4](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L131)
- [scope_state_handler @ 0x0802A534](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L156)

In the currently exported `full_decompile.c`, the scope-region queue activity we
can see is overwhelmingly `0x20002D78`, not `0x20002D74`.

Examples inside the visible scope FSM region:

- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6995)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7491)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8098)
- [full_decompile.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8117)

Those are all sends to `_DAT_20002d78`.

## 2. Negative result that matters

In the current exported `full_decompile.c`:

- the search hit set for `0x20002D74` is empty
- the search hit set for `DAT_20001025` and `DAT_2000102e` is confined to the
  meter formatter / display side, not the visible scope FSM body

That matches the writer audit:

- [selector_writer_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/selector_writer_audit_2026_04_08.md#L18)

and weakens the idea that the currently recovered selector-byte family is the
main visible scope-control surface in the downloaded app.

## 3. Best current interpretation

The current evidence fits one of these two explanations better than the old
single-path theory:

1. the real scope-entry / scope-config UART path is hidden in an enclosing body
   around the forced helper slices (`0x08006060`, `0x08006120`, `0x080062F8`,
   `0x08006418`) that Ghidra has not reconstructed cleanly
2. the downloaded vendor app is missing enough code/data that the true scope
   control path is simply not represented in the current export the way the live
   board uses it

The visible scope FSM itself mostly looks like:

- choose display/update selectors
- queue acquisition work to `0x20002D78`
- update analog / trigger / render state

not:

- emit the recovered `0x050C/0x050D/...` raw UART words directly

## 4. Recommended next move

The better next branch is now:

1. trace the larger enclosing helper cluster around `0x08006060` / `0x08006120`
   / `0x080062F8` / `0x08006418`
2. in parallel, identify the live writers of `DAT_20001060`, because that byte
   now looks like the higher-value selector for the cluster
3. keep re-reading the true scope handlers for high-flash refs and
   indirect dispatches rather than for direct `0x20002D74` sends

If the enclosing-body pass still does not reveal a clean scope-side UART
builder, that will strengthen the “downloaded vendor image is incomplete for
scope RE” hypothesis even further.
