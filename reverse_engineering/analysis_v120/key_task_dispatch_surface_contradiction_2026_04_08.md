# Key-Task Dispatch Surface Contradiction 2026-04-08

## Summary

This pass revisits the `key_task` dispatch surface with the raw-app address
model tightened up.

The strongest result is:

- the contradiction is real, and stronger than before

Raw `key_task` still clearly does:

1. receive one-byte logical event IDs from the button queue
2. compute an indexed address from `0x08046548`
3. `ldr` a 32-bit value
4. `blx` through it

But the bytes at `0x08046544..0x0804658C` do **not** look like a handler table.
They look much more like an `.ARM.exidx`-style unwind surface:

- alternating negative/positive 32-bit words
- several first words decode cleanly as small PREL31-style backreferences into
  nearby code
- second words look like compact unwind payloads, not callable Thumb pointers

That means the problem is no longer just "the table bytes look weird." The
current downloaded V1.2.0 app now contains a direct structural contradiction:

- `key_task` raw assembly expects callable targets
- the corresponding literal region looks exidx-like instead

Primary references:

- [right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)
- [ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md)
- [right_panel_stage_entry_event_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_entry_event_split_2026_04_08.md)

## 1. Raw `key_task` still really dispatches indirectly

The raw app disassembly at `0x0803D008..0x0803D09A` shows:

```asm
0x0803D012: movw  r8, #0x6548
0x0803D028: movt  r8, #0x0804
...
0x0803D08C: ldrb.w r0, [sp, #7]
0x0803D090: add.w  r0, r8, r0, lsl #2
0x0803D094: ldr.w  r0, [r0, #-4]
0x0803D098: blx    r0
```

So the runtime shape is unambiguous:

- one-byte event ID
- 4-byte stride
- indirect branch through a value loaded from the `0x08046544/48` region

This is not a decompiler hallucination.

## 2. The runtime literal region does not look like a function-pointer table

Dumping raw bytes at `0x08046520..0x08046590` gives:

```text
0x08046520: 98ffffff de000000 2ffeffff 72030000
0x08046530: dbf9ffff f60a0000 a1e9ffff af710000
0x08046540: ce270000 8bf0ffff 4d080000 4bfbffff
0x08046550: 95020000 b2feffff 9a000000 aeffffff
0x08046560: 95ffffff e4000000 23feffff 8b030000
0x08046570: aff9ffff 440b0000 17e9ffff 58700000
0x08046580: f4290000 dbefffff a8080000 17fbffff
```

If these were normal handler pointers, we would expect values like:

- `0x080081F9`
- `0x080087CD`
- `0x0800A121`
- `0x0800A2F9`

or at least other obvious Thumb code addresses in the app region.

Instead we get values like:

- `0xFFFFF08B`
- `0x0000084D`
- `0xFFFFFB4B`
- `0x00000295`

Those are not plausible direct handler targets for `blx r0` on Cortex-M.

## 3. The region looks exidx-like

Interpreting the first word of each 8-byte pair as a signed relative offset from
its own location produces a very exidx-looking pattern:

| Pair address | First word | Relative target | Second word |
|---|---:|---:|---:|
| `0x08046544` | `0xFFFFF08B` | `0x080455CF` | `0x0000084D` |
| `0x0804654C` | `0xFFFFFB4B` | `0x08046097` | `0x00000295` |
| `0x08046554` | `0xFFFFFEB2` | `0x08046406` | `0x0000009A` |
| `0x0804655C` | `0xFFFFFFAE` | `0x0804650A` | `0xFFFFFF95` |
| `0x08046564` | `0x000000E4` | `0x08046648` | `0xFFFFFE23` |
| `0x0804656C` | `0x0000038B` | `0x080468F7` | `0xFFFFF9AF` |
| `0x08046574` | `0x00000B44` | `0x080470B8` | `0xFFFFE917` |
| `0x0804657C` | `0x00007058` | `0x0804D5D4` | `0x000029F4` |

That is not a proof by itself, but it is a *much* better fit for unwind-index
data than for button-event handlers.

## 4. The Ghidra project adds a second `-0x4000` wrinkle for data targets

The earlier app-slot correction in
[raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)
still matters for **code locations** exported from the decompile project.

The newer project-side rule in
[ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md)
shows that inside the current project, the bytes matching runtime literal
`0x08046544/48` live at:

- project `0x08042544/48`

not at project `0x08046544/48`.

That fixes the **location** mismatch when comparing project memory to raw app
memory.

But it does **not** eliminate the contradiction, because the corrected bytes at
runtime `0x08046544/48` still look exidx-like rather than handler-like.

## 5. No easy alternate pointer cluster surfaced

I rechecked the raw vendor app for easy routing evidence to the four recovered
event-owner peers:

- `0x080081F8` = adjust-prev
- `0x080087CC` = adjust-next
- `0x0800A120` = staged single-selection owner
- `0x0800A2F8` = staged mass-toggle owner

Results:

- no literal aligned pointers
- no literal Thumb pointers
- no easy PREL31-style references to those owners elsewhere in the image

So the raw app still does not yield a simple replacement "real handler table"
surface yet.

## 6. Best current interpretation

The best current interpretation is:

1. the recovered event owners are still real
2. `key_task` still clearly performs an indirect dispatch
3. but the downloaded V1.2.0 app does **not** expose a normal direct handler
   table at the runtime literal address the raw code appears to use

That leaves a smaller set of explanations than before:

- the downloaded vendor app is not the exact image shape used by the decompile
  project
- the decompile project's `key_task` region is structurally right, but the data
  region it assumes has been misidentified or overlaid incorrectly
- the dispatch surface is more exotic than a flat pointer table, and current
  decompilation is flattening it too aggressively

## What This Changes

The next question is no longer:

- "which handler-table slot points at the four event owners?"

It is now:

- "what is the real representation of the key/event dispatch surface that
  `key_task` is using in the downloaded image?"

That is a better target than continuing to assume a plain pointer table.

## Best Next Move

1. Use the project-side `-0x4000` rule when checking flash-literal data in the
   current misbased Ghidra import.
2. Treat runtime `0x08046544/48` and project `0x08042544/48` as the same
   **unresolved dispatch surface**, not a confirmed handler table.
3. Keep the four event-owner peers as the best recovered semantic family:
   - `adjust-prev`
   - `adjust-next`
   - `single-selection`
   - `mass-toggle`
4. Use that family to keep tracing runtime choreography, while explicitly
   separating it from the still-unresolved physical/logical event-ID table.
