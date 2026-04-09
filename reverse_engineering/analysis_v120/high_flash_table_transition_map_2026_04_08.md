# High-Flash Table Transition Map

Date: 2026-04-08

Primary question:
- What user-visible state transition changes `DAT_20001025` before the missing
  `0x080BB3FC` lookup fires?

Short answer:
- The best current evidence says this is **not a scope transition**.
- It is a **multimeter mode / meter-display transition** driven by
  `dvom_rx_task` frame classification and a paired increment/decrement UI path.

Key references:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30348)
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md#L151)
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L69)

## 1. `0x080BB3FC` matches the meter formatter path

The strongest correction is that the forced `0x080BB3FC` access pattern is
isomorphic to the already-identified `FUN_080028E0` meter formatter.

`FUN_080028E0` is already classified as:
- [function_names.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L90):
  `meter_data_process`

And the meter deep-dive already concluded:
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md#L253):
  `FUN_080028e0` is the display-side formatter and meter-mode dispatcher, not a
  range-feedback controller.

That matters because the forced gap functions around `0x080041F8` and
`0x080047CC` use the same state variables:
- `DAT_20001025`
- `DAT_20001027`
- `DAT_2000102E`
- `DAT_20001030`
- `DAT_20001060`

Those are the same meter-FSM variables documented in:
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md#L755)

## 2. Where `DAT_20001025` changes before the lookup

The live state changes happen in `dvom_rx_task`, not in a scope boot path.

### A. Frame-driven forced mode change to `1`

In `dvom_rx_task`, one decoded frame pattern forces:

```c
if ((iVar10 == 0x12 && iVar11 == 10) && (iVar12 == 5)) {
  DAT_2000102d = '\x05';
  if (cRam20001055 == -0x50) {
    cRam20001055 = -0x4f;
    DAT_20001025 = 1;
  }
}
```

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30570)

The existing meter note already interprets this as:
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md#L530)
  force mode change to ACV (`1`)

### B. Generic mode/format change to `8`

Another frame classification path forces:

```c
DAT_2000102d = '\x04';
DAT_20001025 = 8;
```

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30643)

The existing meter note interprets this as a mode/format change bucket:
- [meter_fsm_deep_dive.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/meter_fsm_deep_dive.md#L572)

### C. Immediate formatter dispatch after those updates

Right after the frame-classification block, stock calls:

```c
FUN_080028e0();
```

Reference:
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L30651)

So the sequence is:

1. meter frame arrives
2. `dvom_rx_task` classifies it
3. `DAT_20001025` may be rewritten
4. `FUN_080028E0` formats/dispatches the display-side result

That is a meter FSM pipeline, not a scope-entry pipeline.

## 3. What the forced high-flash functions are doing

The forced functions at `0x080041F8` and `0x080047CC` still matter, but they
look like paired decrement/increment handlers over the same meter-mode state.

### Decrement-with-wrap variant

```c
bVar14 = DAT_20001025 == '\0';
DAT_20001025 = DAT_20001025 - 1;
if (bVar14) {
  DAT_20001025 = 7;
}
sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
```

Reference:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L90)

### Increment-with-wrap variant

```c
bVar11 = 0;
if (DAT_20001025 < 7) {
  bVar11 = DAT_20001025 + 1;
}
DAT_20001025 = bVar11;
sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
```

Reference:
- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L884)

These do not look like “scope boot” routines anymore. They look like:
- a left/right mode-cycle helper
- over the same `meter_mode` family already used by `dvom_rx_task`
- with a missing 8-entry byte table that likely selects a meter-side raw UART
  command low byte

## 4. Practical interpretation of the transition map

The user-visible transitions affecting `0x080BB3FC` are most likely:

1. Meter frame classification inside `dvom_rx_task`
   - specific patterns force `DAT_20001025 = 1`
   - generic mode-change patterns force `DAT_20001025 = 8`

2. Meter mode stepping in a paired increment/decrement UI path
   - `0x080041F8` decrements `DAT_20001025` with wrap
   - `0x080047CC` increments `DAT_20001025` with wrap

So the missing-table question has changed:
- `0x080BB3FC` is probably a **meter mode command table**
- not the best remaining candidate for missing **scope** activation bytes

## 5. What this means for scope RE

This shifts the priority:

1. De-prioritize `0x080BB3FC` as a scope blocker
2. Keep `0x080BB40C` as a mode/config table of interest, but not a direct UART
   proof point
3. Re-focus true scope RE on callers inside:
   - [scope_main_fsm](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L128)
   - [scope_mode_timebase](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L129)
   - [scope_mode_trigger](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L130)
   - [scope_state_handler](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L160)

## 6. Recommended next step

The highest-value next RE move is:

- search the true scope handlers for high-flash references and raw UART-word
  builders that do **not** share the `DAT_20001025 / DAT_20001027 / DAT_2000102E`
  meter-state family

That will separate “missing stock scope config” from “missing stock meter mode
tables,” which are now clearly being conflated by the incomplete vendor image.
