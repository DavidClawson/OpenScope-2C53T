# Dispatcher Table Contradiction

Date: 2026-04-08

Update 2026-04-08 (late):
- This note still preserves the original literal-address contradiction, but it
  predates the normalization fix summarized in
  [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md).
- The literal base `0x0804BE74` is still non-callable on disk, but the
  normalized base `0x08044E74 = 0x0804BE74 - 0x7000` is a valid pointer table.
- That normalized table now looks display-side, not FPGA/UART-side.
- So the remaining contradiction is no longer “the table has no resolution”;
  it is “we were using the display dispatch table to explain FPGA behavior.”

Purpose:
- document the hard contradiction uncovered while trying to map the scope-side
  selector bytes `2`, `3`, `4`, `5`, `6`
- separate what is still solid from what is no longer safe to claim

Primary references:
- [scope_dispatcher_trace_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_dispatcher_trace_2026_04_08.md)
- [scope_only_targets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_only_targets_2026_04_08.md)
- [raw_init_queue_task_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_init_queue_task_audit_2026_04_08.md)
- [master_init_phase3.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/master_init_phase3.c#L466)
- [fpga_task_disasm.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_disasm.txt#L158)
- [dispatch_table_0804BE74_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatch_table_0804BE74_2026_04_08.txt)

## Executive Summary

Three things are solid:

1. The true scope path really does queue selector bytes `2`, `3`, `4`, `5`,
   `6` to `0x20002D6C`.
2. The task at `0x08036A50` really does dequeue one byte and execute:
   - `ldrb r0, [sp, #7]`
   - `ldr.w r0, [r6, r0, lsl #2]`
   - `blx r0`
3. `r6` really is loaded with `0x0804BE74`.

But the raw vendor image at `0x0804BE74` does **not** contain callable Thumb
function pointers. So the current RE has a real contradiction:

- either the dispatch-base claim is incomplete or wrong
- or the downloaded vendor image is not sufficient to reconstruct the live
  dispatch table

That means we cannot yet map selectors `2..6` to downstream handlers from the
vendor app alone.

## 1. What is still solid

### A. Scope selectors are real

The scope-side queue sites are still good anchors:

- selector `2` at
  [full_decompile.c:6992](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L6992)
- selectors `5` and `6` at
  [full_decompile.c:7494](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L7494)
- selector `4` at
  [full_decompile.c:8752](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L8752)
- selector `3` at
  [full_decompile.c:9118](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/full_decompile.c#L9118)

Those selector bytes are still the best upstream scope-control anchors.

### B. `0x08036A50` is a real byte-dispatch task

Independent Thumb disassembly still shows:

```asm
0x08036a52: movw   r5, #0x2d6c
0x08036a56: movw   r6, #0xbe74
0x08036a62: movt   r5, #0x2000   ; r5 = 0x20002d6c
0x08036a6a: movt   r6, #0x0804   ; r6 = 0x0804be74
0x08036a78: ldrb.w r0, [sp, #7]
0x08036a7c: ldr.w  r0, [r6, r0, lsl #2]
0x08036a80: blx    r0
```

So this is not a decompiler hallucination. The `ldrb -> ldr table -> blx`
pattern is real.

### C. `0x20002D6C` still looks like the first byte queue

Queue creation notes still show:

- [master_init_phase3.c:352](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/master_init_phase3.c#L352)
- [init_function_decompile.txt:309](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L309)
- [raw_init_queue_task_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_init_queue_task_audit_2026_04_08.md#L27)

as the first queue created, size `20 x 1 byte`, stored at `0x20002D6C`.

That remains more compatible with a selector/command queue than with a generic
display framebuffer/event pipe.

## 2. The contradiction at `0x0804BE74`

The raw bytes in the V1.2.0 vendor image at `0x0804BE74` are:

```text
slot 0: 0x00000000
slot 1: 0x00400000
slot 2: 0x00000000
slot 3: 0x00000400
slot 4: 0x00000000
slot 5: 0x00008000
slot 6: 0x00000000
slot 7: 0x00000000
```

These are not valid direct Thumb branch targets:

- most are zero
- none of the non-zero values have Thumb bit `1` set
- they do not look like normal `0x080xxxxx` code pointers either

The direct dump is recorded in
[dispatch_table_0804BE74_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dispatch_table_0804BE74_2026_04_08.txt).

## 3. Cross-version sanity check

This is not just a single V1.2.0 oddity.

The same flash neighborhood in older vendor apps also does **not** look like a
normal direct function-pointer table:

- V1.0.3 `0x0804BE74` region: `30180070 18006000 00600000 c00001c0 ...`
- V1.0.7 `0x0804BE74` region: `00800000 80000000 ebb09d00 00000001 ...`
- V1.2.0 `0x0804BE74` region: `00000000 00004000 00000000 00040000 ...`

So the contradiction is deeper than “just one bad vendor release.”

## 4. Related warning signs

### A. The old `display_task @ 0x08036A50` read is not trustworthy

[decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30318)
labels `0x08036A50` as `display_task`, but the task-create notes still place
the named `"display"` task elsewhere.

Also, forcing a decompile at the supposed task entry
[0x0803DA50](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_task_force_0803DA50_2026_04_08.c)
did **not** produce a task loop. It produced a short floating-point helper.

That means the decompiler symbol/handle names around this area are not strong
enough to settle the dispatcher question by themselves.

### B. `0x0804C40C` is also weaker than we thought

Older notes treated `0x0804C40C` as a flash pointer table for unit strings, but
the raw V1.2.0 bytes there are all zero in the downloaded image.

That does not prove the analysis is wrong, but it is another sign that some of
our “hardcoded flash table” claims are overconfident relative to the raw file.

### C. There is no nearby real code-pointer array

A quick scan for contiguous runs of odd in-image code pointers found no obvious
callable table near `0x0804BE74` in V1.2.0.

So this does not currently look like a simple off-by-one-word or small-offset
mistake around the cited base.

## 5. Best current interpretation

The least-overstated summary is:

- selector bytes `2`, `3`, `4`, `5`, `6` are still real upstream scope-side
  dispatcher inputs
- `0x08036A50` is still a real one-byte dispatch loop
- but the vendor image alone does not let us map those selector bytes to final
  handler functions with confidence

This strengthens two hypotheses:

1. one or more current RE claims about queue/task/table identity are wrong
2. the downloaded vendor app is not a sufficient stand-in for the exact
   programmed image on the real board

## 6. Best next steps

1. Re-verify the queue/task creation map directly from raw init disassembly,
   especially the handles at `0x20002D6C` and `0x20002D70`.
2. Revisit the task-entry claims around the named `"display"` task and avoid
   trusting current decompiler labels there without raw-create-site support.
3. Compare the live-board image against the vendor app when a real stock dump is
   available, especially around:
   - `0x0804BE74`
   - `0x0804C40C`
4. Keep using the selector bytes as scope-path anchors, but do not treat them as
   resolved command-handler IDs yet.
