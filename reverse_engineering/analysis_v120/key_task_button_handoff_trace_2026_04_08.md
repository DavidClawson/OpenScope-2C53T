# Key Task / Button Handoff Trace (2026-04-08)

This pass traces the two code ranges the earlier re-audit called out:

- decompiled/project `0x08039012..0x08039098`
- decompiled/project `0x08039382..0x080393EE`

For the archived raw V1.2.0 app, those correspond to:

- raw `0x0803D012..0x0803D098` for `key_task`
- raw `0x0803D382..0x0803D3EE` for the button-event producer

The goal here is narrow:

1. identify exactly what byte reaches `key_task`
2. identify any transform before dispatch
3. decide whether there is a second level of indirection before the semantic event owners

## 1. The byte that reaches `key_task` is a single logical event byte

The recovered button producer around raw `0x0803D382..0x0803D504` is the same
scan/debounce path already summarized in
[button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md).

The concrete raw setup is:

- `0x0803D374/378`: load `0x20002D50` = current button bitfield
- `0x0803D37E/38E`: load `0x20002D58` = 15 debounce counters
- `0x0803D382/392`: load `0x08046528` = byte table used as `btn_map`

The loop walks 15 buttons in five groups of three:

- `r6 = 0, 3, 6, 9, 12`
- bit masks per group use shifts `1 << r6`, `2 << r6`, `4 << r6`
- per-button counters live at `0x20002D58 + idx`

The event byte is staged at `sp+0x0A`.

There are only three event-producing cases:

1. Short-press confirm
   - counter reaches `0x46`
   - byte loaded from `btn_map[idx + 0x0F]`
   - raw examples:
     - `0x0803D3D2..0x0803D3DA`
     - `0x0803D432..0x0803D43A`
     - `0x0803D494..0x0803D49C`

2. Held/repeat event
   - previous state bit is also set
   - counter reaches `0x48`
   - byte loaded from `btn_map[idx]`
   - counter is decremented back to `0x47`
   - raw examples:
     - `0x0803D3E8..0x0803D3F6`
     - `0x0803D448..0x0803D458`
     - `0x0803D4AE..0x0803D4BE`

3. Early release event
   - current bit clear
   - counter in `[2, 0x45]`
   - byte loaded from `btn_map[idx]`
   - raw examples:
     - `0x0803D406..0x0803D410`
     - `0x0803D466..0x0803D472`
     - `0x0803D4D2..0x0803D4DE`

So the exact byte entering the button-event queue is:

- one byte loaded from the `0x08046528` table
- either from the lower 15-byte bank (`idx`) or the upper 15-byte bank (`idx+0x0F`)
- staged at `sp+0x0A`

This is not a raw matrix position and not a bitmask.

## 2. The queue send is direct and only happens for exactly one event

The send site is raw `0x0803D4E0..0x0803D504`:

- `r2` is the accumulated `event_count`
- `sp+0x0A` is the selected `button_id`
- `0x20002D70` is the button-event queue handle

The gating is explicit:

- if `event_count > 1`, do not queue
- if `button_id == 0`, do not queue
- only if `event_count == 1 && button_id != 0`, call `xQueueGenericSend`

So the queue payload is a single already-decoded logical event byte, and chorded /
ambiguous multi-event scans are dropped before `key_task` sees anything.

## 3. `key_task` does not transform the event byte before dispatch

Raw `key_task` at `0x0803D008..0x0803D09A` does four things:

1. block on `xQueueReceive(*(0x20002D70), &event, -1)`
2. apply modal gating based on `DAT_20000F08`
3. apply extra blockers from `0x127`, `0x128`, `0x12B`, and `0x1024`
4. dispatch indirectly through the `0x08046544/48` surface

The important dispatch sequence is:

```c
event = *(uint8_t *)(sp + 7);
target = *(uint32_t *)(0x08046544 + event * 4);
blx target;
```

In raw instructions:

- `0x0803D08C`: `ldrb.w r0, [sp, #7]`
- `0x0803D090`: `add.w r0, r8, r0, lsl #2`
- `0x0803D094`: `ldr.w r0, [r0, #-4]`
- `0x0803D098`: `blx r0`

That means there is no arithmetic rewrite, no remap table, and no extra decoding
step between the queued byte and the `key_task` dispatch slot. The one-byte
logical event is used directly as the table index.

## 4. The only transform inside `key_task` is modal gating

The event byte is unchanged, but dispatch is conditionally suppressed.

Recovered gating logic:

- if `(DAT_20000F08 & 0xFE) == 2`, collapse it to `0` and continue
- if `DAT_20000F08 == 1` or `0xFF`, skip dispatch and loop
- if any of these are nonzero, skip dispatch and loop:
  - `DAT_20000127`
  - `DAT_20000128`
  - `DAT_2000012B`
  - `DAT_20001024`

So `key_task` changes whether the event is allowed to dispatch, but it does not
change the event byte itself.

## 5. There is a second level of dispatch, but it is after the semantic owner

The answer to the indirection question is:

- **No** second level before the semantic owner inside `key_task`
- **Yes** a second level once the semantic owner starts running

The current best model is still:

1. physical matrix / passive buttons -> bitfield at `0x20002D50`
2. debounce -> one logical byte from `0x08046528`
3. queue byte to `0x20002D70`
4. `key_task` uses that byte directly as the index into `0x08046544/48`
5. selected semantic owner then dispatches again on visible runtime state `F68`

That last point matches the recovered owners from
[logical_event_dispatcher_hypothesis_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/logical_event_dispatcher_hypothesis_2026_04_08.md):

- `adjust-prev`
- `adjust-next`
- `staged single-selection`
- `staged mass-toggle`

Each of those peers has its own top-level dispatch on visible runtime state.

So the event path is:

- one byte of logical event ID
- one direct `key_task` dispatch
- then one state-driven dispatch inside the chosen owner

## 6. What remains unresolved

The structural trace is now strong, but two numeric/data contradictions remain:

1. `0x08046528` is definitely used as a byte event table by code, but the
   downloaded V1.2.0 bytes at that runtime address still do not look like a
   sane 30-byte logical-ID table.

2. `0x08046544/48` is definitely used as the `key_task` dispatch surface by
   code, but the downloaded V1.2.0 bytes there still look exidx/unwind-like
   rather than a clean handler table.

So the strongest current conclusion is:

- the control-flow shape is now clear
- the unresolved part is the data representation at the two literal surfaces,
  not the queue handoff itself

## 7. Bottom line

The exact byte reaching `key_task` is:

- a one-byte logical event ID selected by the button scan/debounce path from the
  `0x08046528` byte surface

That byte is transformed as follows:

- **before** `key_task`: chosen from `btn_map[idx]` or `btn_map[idx+0x0F]`
- **inside** `key_task`: not transformed, only gated
- **at dispatch**: used directly as the slot index into `0x08046544/48`

And the second level of indirection is:

- not inside `key_task`
- inside the selected semantic owner, which then dispatches on runtime state

That narrows the open problem to the two data surfaces themselves:

- `0x08046528`
- `0x08046544/48`
