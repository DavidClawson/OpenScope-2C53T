# Scope Composite State Presets

Date: 2026-04-08

Purpose:
- document the newly recovered raw store patterns into `0xF68..0xF6B`
- decode the helper-side composite writes that preset
  `DAT_20001060..DAT_20001063` in one shot
- connect those presets back to the visible `2 / 9` scope transition family

Primary references:
- [mode_scope_state_store_hits_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_store_hits_2026_04_08.txt)
- [mixed_scope_handler_disasm_08002F00_08003160_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_disasm_08002F00_08003160_2026_04_08.txt)
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)
- [mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt)
- [mode_scope_state_cluster_08003210_080034F0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003210_080034F0_2026_04_08.txt)
- [mixed_scope_handler_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_bridge_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)

## Executive Summary

This pass found the strongest state-setup evidence so far for the scope path:
the vendor app does not only tweak `DAT_20001060..63` byte-by-byte. It also
uses helper-side preset writes that stamp the whole `0xF68..0xF6B` bundle into
known layouts.

The most useful recovered presets are:

- `0x00030109` written at `0x0800603C`
- `0x00020109` written at `0x08006580`
- `0x00040109` written at `0x08006728`
- `0x0301` written to `0xF69` at `0x0800607E`
- `0x0501` written to `0xF69` at `0x0800650C`
- `0x0201` written to `0xF69` at `0x0800661E`

Decoded little-endian across `0xF68..0xF6B`, those mean:

- `0x00030109` -> `DAT_20001060 = 9`, `DAT_20001061 = 1`,
  `DAT_20001062 = 3`, `DAT_20001063 = 0`
- `0x00020109` -> `DAT_20001060 = 9`, `DAT_20001061 = 1`,
  `DAT_20001062 = 2`, `DAT_20001063 = 0`
- `0x00040109` -> `DAT_20001060 = 9`, `DAT_20001061 = 1`,
  `DAT_20001062 = 4`, `DAT_20001063 = 0`
- `0x0301` -> `DAT_20001061 = 1`, `DAT_20001062 = 3`
- `0x0501` -> `DAT_20001061 = 1`, `DAT_20001062 = 5`
- `0x0201` -> `DAT_20001061 = 1`, `DAT_20001062 = 2`

That makes the packed selector byte `DAT_20001062` much more important than our
older notes suggested. Values `2`, `3`, `4`, and `5` are now confirmed
transition targets in real helper code.

## 1. Recovered collapse-to-`2` helpers

Three sibling helpers explicitly collapse back to visible scope-active state `2`
when `DAT_20001062` reaches a specific value:

### `0x08006010`

Raw behavior:

- if `DAT_20001062 == 3`
  - clear latch `+0x355`
  - `DAT_20001060 = 2`
  - clear `DAT_20001061`
  - clear `DAT_20001063`
  - tail-call the common scope redraw path

Reference:
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)

### `0x080064E0`

Raw behavior:

- if `DAT_20001062 == 5`
  - clear latch `+0x355`
  - `DAT_20001060 = 2`
  - clear `DAT_20001061`
  - clear `DAT_20001063`
  - tail-call the same redraw path

Reference:
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)

### `0x080065CA`

Raw behavior:

- gated by the same latch family
- if `DAT_20001062 == 2`
  - clear latch `+0x355`
  - `DAT_20001060 = 2`
  - clear `DAT_20001061`
  - clear `DAT_20001063`
  - tail-call the redraw path

Reference:
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)

So `DAT_20001062 = 2 / 3 / 5` are not arbitrary UI list values. They are now
confirmed collapse checkpoints back into visible scope-active `DAT_20001060 = 2`.

## 2. Recovered preset-to-`9` helpers

Three siblings write composite presets that push the state family into the
internal `9` path rather than directly back to visible `2`.

### `0x08006034`

The helper writes:

- `0x00030109` to `0xF68`
- sets latch `+0x355 = 1`
- then redraws

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 3`
- `DAT_20001063 = 0`

Reference:
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)

### `0x08006578`

The helper writes:

- `0x00020109` to `0xF68`
- sets latch `+0x355 = 1`
- then redraws

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 2`
- `DAT_20001063 = 0`

Reference:
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)

### `0x08006720`

The sibling helper at `0x08006720` writes:

- `0x00040109` to `0xF68`
- sets latch `+0x355 = 1`
- then redraws

Decoded:

- `DAT_20001060 = 9`
- `DAT_20001061 = 1`
- `DAT_20001062 = 4`
- `DAT_20001063 = 0`

This extends the known internal preset family from `2 / 3` to at least
`2 / 3 / 4`.

This is a strong confirmation of the older "2 <-> 9 paired family" theory:
the app really does stamp a whole transitional preset around low byte `9`.

## 3. Halfword-only packed presets

The sibling helpers also use narrower presets that leave `DAT_20001060` alone
but set up `DAT_20001061` and `DAT_20001062` together:

- `0x0800607E` -> `strh 0x0301` at `0xF69`
- `0x0800650C` -> `strh 0x0501` at `0xF69`
- `0x0800661E` -> `strh 0x0201` at `0xF69`

Decoded:

- `DAT_20001061 = 1`
- `DAT_20001062 = 3 / 5 / 2`

and each path immediately queues `0x13`, then `0x14`.

That lines up neatly with the raw bridge behavior in
[mixed_scope_handler_disasm_08002F00_08003160_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_disasm_08002F00_08003160_2026_04_08.txt),
where the `9` path emits `0x13`, `0x14` and later collapses back to `2`.

## 4. Direct top-level mode writes beyond `2 / 9`

The same store-hit pass also surfaced direct writes to other low bytes:

- `0x08004F08` writes `DAT_20001060 = 4`
- `0x080065B2` writes `DAT_20001060 = 5`
- `0x08004D96` writes `DAT_20001060 = DAT_20001061 + 1`
- `0x08006592` writes `DAT_20001060 = 2` while clearing `0xE1C / 0xE12 /
  0xE16 / 0xE1A`

References:
- [mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)

So the visible command-bank states `4` and `5` are not just theoretical table
rows. They do have concrete runtime writers in the downloaded app.

## 5. Runtime re-entry correction

The raw call-site audit now shows these preset helpers do not just prepare state
for some unrelated path. They re-enter the shared selector-bank emitter
`FUN_0800B908` directly at runtime.

That correction is documented in
[runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md).

So the better current read is:

- stock stages a preset in `DAT_20001060..63`
- then tail-calls the shared bank emitter
- rather than treating the preset helpers and the bank emitter as isolated layers

## 6. Why this changes the search

This narrows the next RE branch in a useful way.

The highest-value question is no longer:

- "which unknown raw selector byte should we brute force next?"

It is now:

- "which user-visible scope actions drive the app into the `DAT_20001062`
  targets `2`, `3`, and `5`, and when does it choose the `9` preset instead of
  the direct collapse-to-`2` path?"

That is a better fit for the bench behavior too. If our firmware never stages
the right packed preset before replaying the selector bank, it would explain why
we see the right families but still never wake the real scope engine.

## 7. Best next move

The next static pass should trace the owners of these preset helpers in this
order:

1. `0x08006010 / 0x08006034 / 0x08006076`
2. `0x080064E0 / 0x08006504 / 0x08006578 / 0x080065CA / 0x08006616`
3. `0x08004D90 / 0x08004F08`

The goal is to connect each preset to a concrete scope UI action:

- range change
- trigger submenu
- timing submenu
- cursor/marker submenu
- overlay exit/confirm

Once that mapping is in place, we can translate it back into a much more
stock-faithful runtime sequence in our firmware.
