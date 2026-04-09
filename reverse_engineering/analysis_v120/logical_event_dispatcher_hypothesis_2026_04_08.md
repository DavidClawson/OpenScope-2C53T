# Logical Event Dispatcher Hypothesis 2026-04-08

## Summary

The strongest current model for the stock event/menu dispatcher above the four
recovered right-panel event-owner peers is now:

1. button scan produces a **logical event ID**, not a raw matrix position
2. `key_task` receives that one-byte logical event from `0x20002D70`
3. `key_task` performs a small amount of modal/input gating
4. `key_task` dispatches through an **opaque event surface** at runtime
   `0x08046544/48`
5. the selected event owner then performs a **second dispatch on visible
   runtime state `F68`**

So the four right-panel peers are best treated as **semantic event owners**
chosen by logical event ID:

- raw `0x080081F8` / decompiled `0x080041F8` = adjust-prev
- raw `0x080087CC` / decompiled `0x080047CC` = adjust-next
- raw `0x0800A120` = staged single-selection owner
- raw `0x0800A2F8` = staged mass-toggle owner

The best current gap is no longer "find another hidden branch inside visible
state `5`." It is:

- recover how logical event IDs map onto those semantic owners through the
  still-unresolved `key_task` dispatch surface

## Concrete Routing Model

### 1. Button scan already normalizes physical buttons into logical events

The stock scan path does not push raw bit positions into `key_task`.

- queue handle `0x20002D70` is the button-event queue
- scan logic uses `btn_map[30]` at runtime `0x08046528`
- the queued byte is the mapped logical event ID, not the physical matrix index

The downloaded vendor app still does not expose a trustworthy plain table at
that location, but the producer side is still best modeled as:

- physical button -> `btn_map` logical event -> queue `0x20002D70`

References:

- [button_scan_algorithm.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_scan_algorithm.md)
- [button_map_confirmed.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_map_confirmed.md)

### 2. `key_task` is the higher dispatcher we were looking for

`key_task` is the first recovered runtime body above the four semantic owner
peers.

Decompiled:

- `key_task` at `0x08039008`

Raw app:

- `key_task` at `0x0803D008`

The important sequence is:

```c
xQueueReceive(_key_queue_handle, &event, portMAX_DELAY);

if ((DAT_20000f08 & 0xfe) == 2) {
    DAT_20000f08 = 0;
    goto dispatch_gate;
}
else if ((DAT_20000f08 != 1) && (DAT_20000f08 != 0xff)) {
    goto dispatch_gate;
}

dispatch_gate:
if (DAT_20000127 == 0 && DAT_20000128 == 0 &&
    DAT_2000012b == 0 && cRam20001024 == 0) {
    (**(code **)(event * 4 + 0x08046544))();
}
```

That gives the strongest current event-routing model:

- **modal/input gate**
  - transient overlay family `DAT_20000f08 = 2/3` collapses to `0`
  - states `1` and `0xff` block dispatch
  - additional blockers: `0x127`, `0x128`, `0x12B`, `0x1024`
- **then**
  - logical event ID indexes a 4-byte-stride dispatch surface
  - selected target is invoked indirectly

This is already enough to say the higher dispatcher is not another menu-local
owner above the four peers. It is `key_task` plus its opaque event surface.

## Why the Four Peers Still Look Like the Right Targets

The recovered peers all have the same broad owner shape:

- real function prologue
- top-level dispatch on visible runtime state `F68`
- context-specific behavior per visible state

That is exactly what we expect from event owners chosen by `key_task`.

### `adjust-prev` and `adjust-next`

- raw `0x080081F8` / decompiled `0x080041F8`
- raw `0x080087CC` / decompiled `0x080047CC`

In the right-panel path:

- visible state `5`:
  - prev -> `E1D--`, emit `0x27`, then `0x28`
  - next -> `E1D++`, emit `0x27`, then `0x28`
- visible state `6`:
  - prev -> `E1D--`, emit `0x29`
  - next -> `E1D++`, emit `0x29`

Across other visible states they continue to behave like generic negative and
positive adjustment owners, not timebase-only helpers.

### Staged single-selection owner

- raw `0x0800A120`

Visible-state map:

- `F68 == 5` -> single-bit stage from `E1D`
- `F68 == 2` -> posture update + selector `0x02`
- `F68 == 1` -> mixed raw-word / selector side path

### Staged mass-toggle owner

- raw `0x0800A2F8`

Visible-state map:

- `F68 == 5` -> full bitmap toggle / broad stage
- `F68 == 2` -> posture update + selector `0x02`
- `F68 == 1` -> mixed gate / posture side path

So the strongest current abstraction is:

- logical event ID selects one semantic owner
- semantic owner branches on visible state
- visible-state-`5` branch is just one context where those owners are reused

## Current Best Dispatcher Hypothesis

The stock event pipeline is most plausibly:

1. physical button is scanned
2. button scan converts it to a logical event ID via `btn_map`
3. logical event ID is queued to `0x20002D70`
4. `key_task` receives the logical event
5. `key_task` applies modal/overlay gating
6. `key_task` dispatches through opaque surface `0x08046544/48`
7. selected semantic owner runs
8. selected owner branches again on visible state `F68`

For the right-panel editor specifically:

- one logical event means "adjust previous"
- one means "adjust next"
- one means "single-selection stage/apply"
- one means "mass-toggle stage/reverse"

The exact physical button labels for those logical events remain unresolved, but
the semantic owner level is now reasonably well grounded.

## Why the Dispatch Surface Is Still the Main Contradiction

The remaining problem is not the owner family. It is the representation of the
dispatch surface itself.

Runtime literal:

- `0x08046544/48`

Correct project location in the current misbased Ghidra import:

- project `0x08042544/48`

The raw and corrected-project bytes there still do not look like a normal flat
Thumb pointer table. They still look exidx-like / unwind-like instead.

That leaves the current strongest interpretation:

- the event-owner family is probably right
- `key_task` really does indirect dispatch
- but the downloaded app image does not expose the dispatch surface in a normal
  plainly-readable handler-table form

## Best Next Unresolved Branch

The best next branch is now:

1. recover the real representation of the runtime dispatch surface at
   `0x08046544/48`
2. identify which logical event IDs map to:
   - adjust-prev
   - adjust-next
   - staged single-selection
   - staged mass-toggle

The most useful follow-up paths are:

- re-audit the corrected project bytes at `0x08042544/48` and neighboring
  structures with the `-0x4000` rule applied consistently
- look for any pre-dispatch translator/normalizer between queued logical event
  IDs and the final `ldr -> blx` index
- if a real stock dump ever becomes available, compare the dispatch surface and
  `btn_map` region first before spending more time on local event-label guesses

## Bottom Line

The strongest current model is a **two-stage dispatcher**:

- `key_task` routes logical events into semantic owners
- semantic owners route again by visible runtime state

That is a tighter and more defensible model than either of these older reads:

- "the right-panel path picks everything internally"
- "we just need to find a plain handler pointer table"
