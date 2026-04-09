# Dynamic Scope Word Builder

Date: 2026-04-08

Purpose:
- pin down the helper that owns the dynamic `0x0500 | low_byte` UART words
- map its runtime selector inputs onto the already-known FPGA RX state family
- separate the real raw-word path from the surrounding display/update side work

Primary references:
- [dynamic_scope_word_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_force_2026_04_08.c)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- [selector_writer_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/selector_writer_audit_2026_04_08.md)
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)
- [fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt)
- [usart_protocol_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/usart_protocol_decompile.txt)

## Executive Summary

This pass confirms that the dynamic CH1/CH2-flavored raw UART word family is
owned by the helper at `0x08006120`, not by the normalized display table and
not by the older high-flash `0x080BB3FC` meter-table path.

The important facts are:

1. `FUN_08006120` really does build a staged halfword at `0x20002D54`,
   OR in `0x0500`, and enqueue the result to the real TX queue at
   `0x20002D74`.
2. The chosen low byte is not arbitrary. It is selected from four CH1/CH2-like
   pairs:
   - `0x0C / 0x0D`
   - `0x0E / 0x17`
   - `0x10 / 0x15`
   - `0x11 / 0x16`
3. The selector inputs are the same state family already visible in the FPGA RX
   path:
   - raw `+0xF2D` = `DAT_20001025`
   - raw `+0xF36` = `DAT_2000102e`
   - raw `+0xF39` = `cRam20001031`
4. A companion helper at `0x080062F8` shares the same outer mode gate but does
   not emit the dynamic `0x0500 | low_byte` word. It looks more like the
   reverse/cleanup partner for the same state machine family.
5. The surrounding helper cluster now also includes:
   - `0x08006060`, which seeds selector bytes before emitting `0x0501`
   - `0x08006418`, which emits the familiar byte-command banks to `0x20002D6C`

So the strongest static anchor for the missing scope-side semantics is now:

- the caller chain feeding `FUN_08006120`
- especially the writers of `DAT_20001025` and `DAT_2000102e`

## 1. What Ghidra now confirms at `0x08006120`

The forced decompile in
[dynamic_scope_word_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_force_2026_04_08.c)
matches the raw objdump closely enough to trust the high-level shape.

The function begins by branching on:

- `DAT_20001060`
- raw offset `+0xF68`
- previously identified in
  [usart_protocol_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/usart_protocol_decompile.txt)
  as the stock `mode_state` family

It has three meaningful outer cases:

### `DAT_20001060 == 1`

This is the branch that matters most for the raw-word path.

The function:

1. clears `bRam20001055` / raw `+0xF5D`
2. reads `DAT_20001025` / raw `+0xF2D`
3. requires the selector to satisfy mask `0xC6`
4. reads `DAT_2000102e` / raw `+0xF36`
5. chooses one low byte from the four paired families
6. writes NaN sentinels to:
   - `uRam20001040`
   - `uRam20001044`
   - `uRam20001048`
7. calls `FUN_080028e0()`
8. ORs the staged halfword with `0x0500`
9. enqueues that word to `0x20002D74`
10. enqueues display selector `0x1B` to `0x20002D6C`

The low-byte selection table is:

- selector `1` -> `0x0C` or `0x0D`
- selector `2` -> `0x0E` or `0x17`
- selector `6` -> `0x11` or `0x16`
- selector `7` -> `0x10` or `0x15`

The choice within each pair depends on `DAT_2000102e`:

- default side keeps `0x0C / 0x0E / 0x11 / 0x10`
- `DAT_2000102e == 1` flips to `0x0D / 0x17 / 0x16 / 0x15`

This is the cleanest static confirmation yet that the recovered low-byte family
is real firmware behavior, not an artifact of older partial notes.

### `DAT_20001060 == 2`

This branch does not enqueue a raw UART word.

Instead it:

- toggles bit `0x80` in `bRam2000044c` / raw `+0x354`
- enqueues display selector `2` to `0x20002D6C`

This is useful context because it shows `FUN_08006120` is not a scope-only
micro-helper. It is a broader state/update helper with one very important
raw-word subpath.

### `DAT_20001060 == 5`

This branch also looks display/state oriented rather than raw-word oriented.

It operates on:

- `cRam20000f12` / raw `+0xE1A`
- `DAT_20000f13` / raw `+0xE1B`
- `DAT_20000f14` / raw `+0xE1C`

and then queues display selectors:

- `0x28`
- `0x26`

So the function mixes display-side mode work with the important dynamic UART
builder. That is why it was easy to misread the whole block before the queue map
was repaired.

## 2. Relationship to the FPGA RX / update path

The strongest reason this helper still looks scope-relevant is not just the
word family. It is that the selector bytes line up with the already-known FPGA
state machine in
[fpga_task_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_decompile.txt).

In the RX/update path around `0x08037178-0x080373A0`:

- raw `+0xF2D` is repeatedly read and dispatched on
- raw `+0xF36` and `+0xF37` are updated from incoming frame bits
- raw `+0xF39` is consulted in case `5`
- `fpga_state_update()` is called at `0x08037386`
- the result is then tested against mask `0x105`

`FUN_08006120` touches the same family:

- `DAT_20001025` = raw `+0xF2D`
- `DAT_2000102e` = raw `+0xF36`
- `cRam20001031` = raw `+0xF39`

That makes this much stronger than a loose word match. The dynamic low-byte
builder sits inside the same runtime state machinery that stock already uses
while processing FPGA responses.

One useful correction falls out of this:

- raw `+0xF36` should no longer be read only as a meter-side scratch byte
- in this path it acts like a two-sided selector that decides which member of a
  CH1/CH2-flavored word pair is transmitted

## 3. The companion helper at `0x080062F8`

The forced function at `0x080062F8` shares the same outer mode gate on
`DAT_20001060`, but its behavior is different enough that it should be treated
as the partner, not the duplicate, of `FUN_08006120`.

Its notable behaviors are:

### `DAT_20001060 == 1`

It only clears `bRam20001055` / raw `+0xF5D` when the same `0xB0` high-nibble
blocker is not set. It does not emit the dynamic UART word.

### `DAT_20001060 == 2`

It increments the low nibble of `bRam2000044c` / raw `+0x354`, updates the
staged halfword at `0x20002D50`, and then queues display selector `2`.

### `DAT_20001060 == 5`

It toggles `cRam20000f12` between `1` and `2`, updates the bitmap-like bytes at
raw `+0xE12..+0xE19`, and queues display selectors in the opposite order:

- `0x26`
- `0x28`

So the best current read is:

- `0x08006120` = forward/select/apply side, including the dynamic `0x0500` word
- `0x080062F8` = reverse/step/cleanup side for the same outer mode family

That is still an inference, but it matches both the decompile and the raw
display-selector ordering.

## 4. What this changes for the scope investigation

This pass narrows the next RE target in a useful way.

The next high-value question is no longer:

- "What does the old `0x0804BE74` table really dispatch?"

That part is now settled as display-side.

The better question is:

- "Who writes `DAT_20001025` and `DAT_2000102e` immediately before
  `FUN_08006120` fires?"

That is the shortest static path to the missing semantics behind:

- `0x050C / 0x050D`
- `0x050E / 0x0517`
- `0x0510 / 0x0515`
- `0x0511 / 0x0516`

Those words may still not be the final scope-enter blocker, but they are now
grounded in a real state machine rather than a guessed raw-frame experiment.

## 5. First writer map for the selector bytes

The first useful correction after tracing the surrounding code is that the
builder is not primarily fed by UI state. It is fed by the UART RX / FPGA-state
path.

### `DAT_20001025`

The strongest currently confirmed writers are:

1. stock init / reset paths
   - reset to `0` in the init block at raw `0x08026FEA`

2. `fpga_task` special-pattern branches
   - raw `0x08036D26` writes `8`
   - raw `0x08036D58` writes `1`

3. `FUN_080028e0()`
   - still acts as the broader state machine owner in the decompiled V1.2.0 app
   - consumes `DAT_20001025` as the main switch state
   - earlier notes already show it can rewrite the state on specific frame
     patterns

The practical meaning is:

- `DAT_20001025` is not just a static display-mode label
- it is actively driven by the RX-processing path before the dynamic raw-word
  helper reads it

### `DAT_2000102e`

This byte is even more tightly coupled to the RX-processing path.

The currently confirmed writers are:

1. `fpga_task` preconditioning and decode branches
   - raw `0x08036CC4` clears it when the current selector is `0`
   - raw `0x08037222` writes `0`
   - raw `0x080372EA` writes `0`
   - raw `0x0803732A` writes `1`
   - raw `0x0803733A` writes `2`
   - raw `0x080373B4` writes `2`

2. `FUN_080028e0()`
   - in the recovered decompile, the main case-`0` branch still writes
     `DAT_2000102e = 0/1/2` from incoming frame bits

So the best current read is:

- `DAT_2000102e` is not a stale formatter-only field
- it is refreshed from live RX conditions and then reused by the dynamic
  `0x0500 | low_byte` builder as the pair-side selector

### Immediate flow of control

The shortest currently understood path is:

1. `fpga_task` waits on the RX semaphore and decodes the incoming frame
2. it updates the `+0xF2D / +0xF36 / +0xF37 / +0xF39` family
3. it calls `FUN_080028e0()`
4. later helper logic such as `FUN_08006120` consumes the same selector family
   to emit the paired `0x0500 | low_byte` words

That makes the state family feel much less like "meter UI mode" and much more
like a live FPGA-response-driven selector bank that both display code and UART
TX helpers reuse.

## 6. Practical next targets

The next passes should focus on:

1. the larger enclosing helper cluster around `0x08006060` / `0x08006120` /
   `0x080062F8` / `0x08006418`, because that cluster now clearly mixes direct
   raw-word TX with byte-command bank emission
2. the live writers of `DAT_20001060`, because that byte now looks like the
   higher-value selector for the whole cluster
3. any scope-side path that bypasses this selector family entirely
4. any matching runtime behavior in the main firmware when meter replies stop
   and scope posture begins
5. the broader writer/read audit captured in
   [selector_writer_audit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/selector_writer_audit_2026_04_08.md)

The immediate takeaway is simple:

- `0x08006120` is now the best static owner for the CH1/CH2 dynamic raw-word
  family
- the surrounding cluster is now captured in
  [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- the missing scope semantics are more likely in the wider mode / caller chain
  feeding that cluster than in the old display-dispatch table or the already-
  deprioritized `0x080BB3FC` meter table
