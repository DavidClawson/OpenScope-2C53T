# Enclosing Helper Cluster

Date: 2026-04-08

Purpose:
- consolidate the adjacent helper slices at `0x08006060`, `0x08006120`,
  `0x080062F8`, and `0x08006418` into one coherent cluster
- separate direct raw-word TX builders from the command-byte bank emitter
- identify the shortest next trace after the selector-writer and bypass passes

Primary references:
- [enclosing_helper_cluster_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_force_2026_04_08.c)
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [selector_writer_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/selector_writer_audit_2026_04_08.md)
- [mode_selector_writer_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
- [scope_selector_bypass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_selector_bypass_2026_04_08.md)
- [interrupt_handlers.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/interrupt_handlers.c#L454)
- [remaining_unknowns.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/remaining_unknowns.md#L295)

## Executive Summary

The latest force-created pass turns the old `0x08006120` "orphan helper" story
into a cleaner cluster:

1. `0x08006060` is a small selector-seeding front end that writes
   `DAT_20001025` / `DAT_2000102E`, emits raw TX word `0x0501`, then queues
   display selectors `0x1D` and `0x1B`.
2. `0x08006120` is still the dynamic `0x0500 | low_byte` builder, using the
   selector-byte family to choose the CH1/CH2-flavored pairs
   `0x0C/0x0D`, `0x0E/0x17`, `0x10/0x15`, and `0x11/0x16`.
3. `0x080062F8` is the reverse/cleanup partner for the same outer mode family.
4. `0x08006418` is the important new sibling: it emits the full byte-command
   banks to `0x20002D6C` for the familiar `0x00/0x01/0x0B..0x11`,
   `0x1A..0x1E`, `0x16..0x19`, `0x1F..0x21`, `0x25..0x29`, and `0x2C`
   families.

That last point matters most. The visible command-bank table we have been using
for bench experiments is no longer just an isolated note from
[remaining_unknowns.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/remaining_unknowns.md#L295).
It now has a concrete owner in the downloaded app's helper cluster.

So the best next trace is no longer:

- "who writes `DAT_20001025` next?"

It is now:

- "who writes `DAT_20001060` immediately before this cluster runs?"

## 1. Raw shape of the cluster

The adjacent prologues and shared state usage make these addresses look like a
real helper family, not four unrelated one-byte artifacts:

- `0x08006060`
- `0x08006120`
- `0x080062F8`
- `0x08006418`

The forced decompile still reports size `1 byte` for each created function, but
the raw body shape is coherent:

- all four have normal Thumb prologues / epilogues
- all four read state from the same `0x200000F8 + offset` region
- all four interact with queue handles in `0x20002D6C`, `0x20002D74`,
  `0x20002D50`, `0x20002D53`, and `0x20002D54`

That makes them better read as useful entry slices into one adjacent state
machine cluster than as trustworthy final function boundaries.

## 2. `0x08006060`: selector seeder + `0x0501`

The first helper is small but useful.

The raw body shows a TBB-based front end that seeds:

- `DAT_20001025` / raw `+0xF2D`
- `DAT_2000102E` / raw `+0xF2E`

before falling into a fixed raw-word / display pattern:

- write `0x0501` to `0x20002D54`
- enqueue that halfword to `0x20002D74`
- enqueue display selector `0x1D`
- enqueue display selector `0x1B`

The explicit seeded pairs visible in the raw body are:

- default `1 / 1`
- `3 / 5`
- `5 / 7`
- `6 / 8`
- `7 / 10`

That makes this helper look like a small selector-preload or preset-entry
routine rather than a scope-only bring-up block.

## 3. `0x08006120`: dynamic `0x0500 | low_byte`

This remains the cluster's most important direct raw-word builder.

In its `DAT_20001060 == 1` branch it:

1. clears the `0xB0` blocker in `bRam20001055`
2. reads `DAT_20001025`
3. reads `DAT_2000102E`
4. chooses one low byte from:
   - `0x0C / 0x0D`
   - `0x0E / 0x17`
   - `0x10 / 0x15`
   - `0x11 / 0x16`
5. stages the value at `0x20002D54`
6. ORs in `0x0500`
7. enqueues the word to `0x20002D74`
8. queues display selector `0x1B`

That part is unchanged from
[dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md),
but it now sits in a clearer surrounding context.

This helper is not the owner of the larger byte-command banks. It owns the
dynamic raw-word side channel.

## 4. `0x080062F8`: reverse / cleanup partner

The next sibling shares the same outer gate on `DAT_20001060`, but it behaves
more like the reverse or cleanup partner than a duplicate builder.

The visible behaviors are:

- `DAT_20001060 == 1`
  - clears the same `0xB0`-gated `bRam20001055` path
  - does not emit the dynamic `0x0500 | low_byte` word
- `DAT_20001060 == 2`
  - increments / normalizes the low nibble at `+0x354`
  - stages a word at `0x20002D50`
  - queues display selector `2`
- `DAT_20001060 == 5`
  - toggles the bitmap-like bytes at `+0xE12..+0xE19`
  - queues display selectors `0x26`, then `0x28`

This still looks like the "step back / reverse / state cleanup" side of the
same family identified in the earlier dynamic-word pass.

## 5. `0x08006418`: command-bank emitter

This is the most useful new result.

The helper branches on the low byte of `DAT_20001060` and then enqueues bytes to
`0x20002D6C`. Those bytes reproduce the command-bank table already documented in
[remaining_unknowns.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/remaining_unknowns.md#L300)
and visible in [interrupt_handlers.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/interrupt_handlers.c#L454).

The common bank emission is:

| Low byte of `DAT_20001060` | Emitted queue bytes |
|---|---|
| `0` | `0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11` |
| `1` | `0x00, 0x09, 0x07/0x0A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E` |
| `2` | `0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x07/0x0A` |
| `3` | `0x00, 0x08, 0x09, 0x07/0x0A, 0x16, 0x17, 0x18, 0x19` |
| `4` | `0x00, 0x1F, 0x09, 0x20, 0x21` |
| `5` | `0x00, 0x25, 0x09, 0x26, 0x27, 0x28` |
| `6` | `0x29` |
| `7` | `0x15` |
| `8` | `0x00, 0x2C` |
| `9` | `0x00, 0x12, 0x13, 0x14, 0x09, 0x07/0x0A` |

The `0x07` vs `0x0A` split still depends on GPIOC bit-7 state, matching the
earlier note.

This is the first time that table has been tied cleanly to a concrete helper in
the adjacent cluster rather than just a standalone decompile fragment.

## 6. Special prologue behavior around `0x08006418`

The bank emission is wrapped in useful state logic, not just a plain switch.

### Low byte `2`

The helper first rewrites:

- `_DAT_20001060 = 0x050109`
- `cRam2000044d = 1`

before it reaches the common bank logic.

That is a strong hint that low byte `2` is a transition / arming state, not just
"send bank 2 and return."

### Low bytes `5` and `6`

Both have prechecks around the `+0xE1A..+0xE1C` family and can raise:

- `DAT_20000F14 = 2`
- queue selector `0x2A`

before returning.

That looks like a display / capture-ready handshake rather than a pure bank
selection.

### Low byte `9`

When `cRam2000044d != 0` and `DAT_20001062 != 5`, the helper sends:

- `0x13`
- `0x14`

and preserves the `0x0501xx` high bytes in `_DAT_20001060`.

If `DAT_20001062 == 5`, it clears the latch, collapses the low byte back to `2`,
and then emits the bank-`2` sequence.

That makes modes `2` and `9` look like a paired transition family, not two fully
independent static modes.

## 7. What changed

This cluster pass sharpens the current scope hypothesis in two ways.

First, the command-bank table we have been replaying in bench firmware is now
anchored to a real helper cluster in the downloaded app.

Second, the missing scope behavior now looks less like:

- "we guessed the wrong `0x1A..0x1E` or `0x25..0x29` bytes"

and more like:

- "we still have not reproduced the state transition that sets up the right
  `DAT_20001060` path and its preconditions before the bank emitter fires"

That is a better next target than more blind command-byte permutations.

## 8. First confirmed `DAT_20001060` write seeds

The first pass over explicit assignments already gives a better next trace list
than "find all mode writers from scratch."

Confirmed write seeds worth chasing are:

1. init / config restore writes `8`
   - [master_init_phase3.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/master_init_phase3.c#L633)
   - the same init family is also visible in
     [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3519)
     and
     [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L3825)
2. a high-flash resource/helper path writes `5`
   - [high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c#L133)
3. repeated normalized dispatch slices write `10`, then queue `0x24` and `0x03`
   - [normalized_dispatch_entries_2_6_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/normalized_dispatch_entries_2_6_2026_04_08.c#L1533)

That is enough to prove the byte is already doing more work than a simple
"0=boot, 1=meter, 2=scope, 3=siggen" top-level mode enum.

## 9. Best next move

The next high-value trace is:

1. identify the live writers of `DAT_20001060`
2. identify which caller path reaches this helper cluster in scope posture
3. separate broad UI mode uses of `DAT_20001060 == 2` from the finer command-bank
   submodes encoded in the same low byte

That should answer whether the missing scope-enter path is:

- still present in the downloaded app but only visible through a larger state
  transition body, or
- partly absent from the vendor image in the same way other high-flash-dependent
  paths already appear incomplete

The first concrete writer map for that next branch is now captured in:
- [mode_selector_writer_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md)

The scope-specific narrowing of low byte `2` is now captured in:
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
