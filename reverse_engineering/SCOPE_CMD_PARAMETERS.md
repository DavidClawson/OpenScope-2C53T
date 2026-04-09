# Scope Command Parameters -- Stock V1.2.0 Firmware Analysis

Date: 2026-04-08

## Critical Finding: The 0x0B-0x11 Wire-Level Assumption Is Wrong

The osc firmware currently sends commands 0x0B through 0x11 as direct
UART cmd_lo bytes with guessed cmd_hi parameters. **The stock firmware
does not do this.** Internal selector bytes 0x0B-0x11 are display-queue
selectors, not wire-level UART command codes.

### Stock firmware data flow (three queues, not one)

```
Application code
    |
    | queues 1-byte INTERNAL SELECTOR (e.g. 0x0B)
    v
usart_cmd_queue (0x20002D6C, 20 x 1-byte items)
    |
    | display_task @ 0x08036A50 dequeues, dispatches via table at 0x08044E74
    v
dispatch handler (reads current scope state, formats parameters)
    |
    | produces 16-bit TX WORD (cmd_hi << 8 | cmd_lo) and queues it
    v
usart_tx_queue (0x20002D74, 10 x 2-byte items)
    |
    | dvom_TX task @ 0x080373F4 dequeues, fills 10-byte frame
    v
USART2 TX to FPGA: [0][0][cmd_hi][cmd_lo][...][checksum]
```

The internal selectors 0x0B-0x11 are consumed by the normalized dispatch
table at `0x08044E74`. Those handlers update the LCD and *also* invoke
the `usart_tx_config_writer` which builds the actual wire-level 16-bit
TX words. **The wire-level cmd_lo values are different from the internal
selectors.**

### What the osc firmware currently sends (wrong)

From `fpga.c:fpga_send_scope_sequence()`:

| Wire cmd_hi | Wire cmd_lo | Meaning assumed |
|-------------|-------------|-----------------|
| 0x01        | 0x0B        | CH1 coupling/range |
| ch2_en      | 0x0C        | CH2 coupling/range |
| 0x03        | 0x0D        | Trigger threshold |
| 0x80        | 0x0E        | Trigger mode/edge |
| 0x04        | 0x0F        | Timebase prescaler |
| 0x02        | 0x10        | Timebase period |
| 0x01        | 0x11        | Timebase mode |

These cmd_hi values are guesses. The cmd_lo values 0x0B-0x11 are
internal selector bytes, not the wire-level codes the FPGA expects.

---

## Recovered Wire-Level Scope Commands

### Static raw TX words (confirmed from stock binary writes to 0x20002D54)

These are direct `strh` writes to the staging variable at `0x20002D54`,
followed by enqueue to `0x20002D74`. Each 16-bit value splits as
`cmd_hi = high byte`, `cmd_lo = low byte`.

| Raw word | cmd_hi | cmd_lo | Source address | Context |
|----------|--------|--------|---------------|---------|
| `0x02A0` | 0x02   | 0xA0   | 0x0800679A    | Mode transition / entry |
| `0x0501` | 0x05   | 0x01   | 0x080060DA    | Scope selector seed + display selectors 0x1D, 0x1B |
| `0x0503` | 0x05   | 0x03   | 0x080067CE    | Scope steady-state |
| `0x0508` | 0x05   | 0x08   | 0x080033DC    | Scope / meter config |
| `0x0509` | 0x05   | 0x09   | 0x08003BB6    | Meter start |
| `0x0514` | 0x05   | 0x14   | 0x08005B8A    | Meter variant |

### Dynamic raw TX words (from FUN_08006120 at 0x08006120)

The dynamic scope word builder runs when `DAT_20001060 == 1` (meter
active mode). It selects cmd_lo from a CH1/CH2-flavored pair based on
`DAT_20001025` (meter_mode, offset +0xF2D) and `DAT_2000102E`
(meter_overload, offset +0xF36), then ORs in `0x0500` for cmd_hi=0x05.

| DAT_20001025 | DAT_2000102E == 0 | DAT_2000102E == 1 | Full TX word |
|---|---|---|---|
| 1 | cmd_lo = 0x0C | cmd_lo = 0x0D | 0x050C / 0x050D |
| 2 | cmd_lo = 0x0E | cmd_lo = 0x17 | 0x050E / 0x0517 |
| 6 | cmd_lo = 0x11 | cmd_lo = 0x16 | 0x0511 / 0x0516 |
| 7 | cmd_lo = 0x10 | cmd_lo = 0x15 | 0x0510 / 0x0515 |

The mask test `(1 << DAT_20001025) & 0xC6` limits valid selector values
to {1, 2, 6, 7} (binary: 0b11000110).

### Runtime scope gap functions (cmd_hi set by caller/config_writer)

These gap functions queue 2-byte items to `usart_tx_queue` (0x20002D74).
The cmd_hi byte is set on the stack by the caller before the gap
function writes cmd_lo.

**Channel ranges** (FUN_0800BA06, 102 bytes):
```
Prefix: cmd_lo = 0x07 (CH1) or 0x0A (CH2), based on sign of param[0]
Then:   cmd_lo = 0x1A, 0x1B, 0x1C, 0x1D, 0x1E (final is blocking)
```

**Trigger** (FUN_0800BB10, 84 bytes):
```
Prefix: cmd_lo = 0x07 or 0x0A
Then:   cmd_lo = 0x16, 0x17, 0x18, 0x19 (final is blocking)
```

**Acquisition mode** (FUN_0800BBA6, 24 bytes):
```
cmd_lo = 0x20, 0x21 (final is blocking)
```

**Timebase** (FUN_0800BC00, 42 bytes):
```
cmd_lo = 0x26, 0x27, 0x28 (final is blocking)
```

For these gap functions, the cmd_hi parameter is computed by
`usart_tx_config_writer` from the state structure before the gap
function is called. The exact cmd_hi for each cmd_lo depends on
current scope state (voltage range, coupling, trigger level, etc.).

---

## Mode Entry Sequence

### Boot init (case 0 of FUN_0800B908, scope mode)

The boot dispatcher queues these **internal selector bytes** to
`usart_cmd_queue` (0x20002D6C):

```
0x00  ->  Reset/init
0x01  ->  Configure channel
0x0B  ->  [dispatch handler reads state, builds TX word]
0x0C  ->  [dispatch handler reads state, builds TX word]
0x0D  ->  [dispatch handler reads state, builds TX word]
0x0E  ->  [dispatch handler reads state, builds TX word]
0x0F  ->  [dispatch handler reads state, builds TX word]
0x10  ->  [dispatch handler reads state, builds TX word]
0x11  ->  [dispatch handler reads state, builds TX word]
```

The dispatch table at `0x08044E74` (normalized from `0x0804BE74`)
translates these into wire-level TX words. **The downloaded vendor
firmware image does not contain enough of this dispatch path to fully
reconstruct the exact wire-level encoding for selectors 0x0B-0x11.**

### Runtime scope entry (from helper cluster at 0x08006418)

The helper at `0x08006418` is the confirmed command-bank emitter. For
`DAT_20001060 == 0` (scope mode), it queues this display-selector bank
to `0x20002D6C`:

```
0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
```

This is the same as boot. The translation to wire-level TX words
happens downstream in the dispatch handlers.

### Scope steady-state

The `osc` task (at normalized `0x0803909C`) runs when
`DAT_20001060 == 2`. It sends display selector `3` at the end of each
visible scope pass. The display task kicks the `_osc_semaphore` to
keep the loop self-arming.

---

## What the FPGA Actually Receives (Best Current Hypothesis)

Based on the recovered raw TX words and the wire-level data flow:

### Scope entry / configuration sequence

| Step | Wire cmd_hi | Wire cmd_lo | Evidence | Meaning |
|------|-------------|-------------|----------|---------|
| 1 | 0x02 | 0xA0 | Static write at 0x0800679A | Mode entry / scope arm |
| 2 | 0x05 | 0x01 | Static write at 0x080060DA + FUN_08006060 | Scope selector seed |
| 3 | 0x05 | 0x0C | Dynamic builder, selector=1, side=0 | CH1 config (pair A) |
| 4 | 0x05 | 0x0E | Dynamic builder, selector=2, side=0 | CH1 config (pair B) |
| 5 | 0x05 | 0x10 | Dynamic builder, selector=7, side=0 | CH1 config (pair C) |
| 6 | 0x05 | 0x11 | Dynamic builder, selector=6, side=0 | CH1 config (pair D) |
| 7 | 0x05 | 0x03 | Static write at 0x080067CE | Scope commit / activate |

For CH2-side (when `DAT_2000102E == 1`):

| Step | Wire cmd_hi | Wire cmd_lo | Evidence |
|------|-------------|-------------|----------|
| 3 | 0x05 | 0x0D | Dynamic builder, selector=1, side=1 |
| 4 | 0x05 | 0x17 | Dynamic builder, selector=2, side=1 |
| 5 | 0x05 | 0x15 | Dynamic builder, selector=7, side=1 |
| 6 | 0x05 | 0x16 | Dynamic builder, selector=6, side=1 |

### Channel range update

| Step | Wire cmd_hi | Wire cmd_lo | Evidence |
|------|-------------|-------------|----------|
| 1 | (state-derived) | 0x07 or 0x0A | Prefix: CH1 or CH2 bank |
| 2 | (state-derived) | 0x1A | CH1 gain |
| 3 | (state-derived) | 0x1B | CH1 offset |
| 4 | (state-derived) | 0x1C | CH2 gain |
| 5 | (state-derived) | 0x1D | CH2 offset |
| 6 | (state-derived) | 0x1E | Coupling/BW limit |

### Trigger update

| Step | Wire cmd_hi | Wire cmd_lo | Evidence |
|------|-------------|-------------|----------|
| 1 | (state-derived) | 0x07 or 0x0A | Prefix: trigger source |
| 2 | (state-derived) | 0x16 | Trigger threshold LSB |
| 3 | (state-derived) | 0x17 | Trigger threshold MSB |
| 4 | (state-derived) | 0x18 | Trigger mode/edge |
| 5 | (state-derived) | 0x19 | Trigger holdoff |

---

## State Structure Offsets Feeding the Scope Commands

The `usart_tx_config_writer` at `0x08039734` reads these state offsets
(base `0x200000F8`) to compute cmd_hi for each command type:

### Type 0: CH1 Configuration (selectors 0x0B, 0x0C)

| Offset | Abs Address | Name | How used for cmd_hi |
|--------|-------------|------|---------------------|
| +0x20  | 0x20000118 | `config_bitfield_a` | bit 1 = AC/DC coupling, bit 3 = BW limit |
| +0x18  | 0x20000110 | `trigger_edge` (reused) | bits [1:0] = range low, bits [15:12] = range high |

### Type 1: CH2 Configuration (selectors 0x0B, 0x0C)

| Offset | Abs Address | Name | How used for cmd_hi |
|--------|-------------|------|---------------------|
| +0x20  | 0x20000118 | `config_bitfield_a` | bit 5 = AC/DC coupling, bit 7 = BW limit |
| +0x18  | 0x20000110 | | bits [9:8] = range low, bits [15:12] = range high |

### Type 2: Trigger Configuration (selectors 0x0D, 0x0E)

| Offset | Abs Address | Name | How used for cmd_hi |
|--------|-------------|------|---------------------|
| +0x20  | 0x20000118 | `config_bitfield_a` | bit 9 = trigger edge, bit 11 = trigger source |
| +0x1C  | 0x20000114 | `trigger_level` | Trigger voltage level encoding |

### Type 3: Timebase Configuration (selectors 0x0F, 0x10, 0x11)

Direct register writes from state timebase parameters. The exact
mapping is not fully recovered.

### Dynamic builder inputs (FUN_08006120, cmd_hi always = 0x05)

| Offset | Abs Address | Name | How used |
|--------|-------------|------|----------|
| +0xF68 | 0x20001060 | `system_mode` | Must be `1` for dynamic builder path |
| +0xF2D | 0x20001025 | `meter_mode` | Selects low-byte pair: {1->0x0C/D, 2->0x0E/17, 6->0x11/16, 7->0x10/15} |
| +0xF36 | 0x2000102E | `meter_overload` | Selects CH1 side (0) vs CH2 side (1) within pair |
| +0xF5D | 0x20001055 | `buzzer_state` | Gate: high nibble must not be 0xB0 |

---

## Default Values at Boot (CH1 enabled, 1V/div, 1ms/div, auto trigger)

From `STATE_STRUCTURE.md` and the init path in `master_init_phase2.c`:

| Offset | Default | Name |
|--------|---------|------|
| +0x00  | 0x01    | `ch1_enable` |
| +0x01  | 0x00    | `ch2_enable` |
| +0x14  | 0x00    | `voltage_range` (index 0 = max sensitivity) |
| +0x15  | 0x01    | `channel_config` (1 channel enabled) |
| +0x17  | 0x00    | `trigger_run_mode` (AUTO) |
| +0x18  | 0x00    | `trigger_edge` (rising) |
| +0x1C  | 0x0080  | `trigger_level` (midpoint) |
| +0x20  | 0x0000  | `config_bitfield_a` (DC coupling, no BW limit) |
| +0x2D  | 0x09    | `timebase_index` (approximately 1ms/div) |
| +0xF68 | 0x00    | `system_mode` (scope mode = 0) |

---

## Blocking Issue: Incomplete Vendor Image

The dispatch table mapping internal selectors 0x0B-0x11 to wire-level
TX words passes through code at `0x08044E74` (normalized). The stock
binary also references addresses above `0x080B7680` (the end of the
downloaded app image):

- `0x080BB3EC`, `0x080BB3FC`, `0x080BC18B`, `0x080BC1A5`, etc.

At least 63 distinct absolute flash references in the stock code point
past the end of the vendor-supplied image. This means:

1. The downloadable app image is truncated relative to what ships on
   the device, OR
2. The shipping device contains additional programmed flash regions
   beyond the website app blob, OR
3. The stock firmware depends on data written separately during factory
   programming

The specific lookup at `0x080BB3FC` is used by the scope helper at
`0x080048C0` to index into a table that produces the dynamic
`0x0500 | low_byte` words. **Some scope configuration data may only
be recoverable from a live on-device flash dump.**

---

## Recommended Changes to osc Firmware

### Immediate: Replace 0x0B-0x11 with recovered raw words

The `fpga_send_scope_sequence()` in `fpga.c` should be rewritten to
send the recovered raw TX words instead of guessed 0x0B-0x11 frames:

```c
/* Scope entry: mode transition */
fpga_timed_send_cmd(0x02, 0xA0, 20);   /* Mode entry / scope arm */
fpga_timed_send_cmd(0x05, 0x01, 15);   /* Scope selector seed */

/* CH1 scope config (use CH2 variants 0x0D/0x17/0x15/0x16 for CH2) */
fpga_timed_send_cmd(0x05, 0x0C, 15);   /* CH1 config pair A */
fpga_timed_send_cmd(0x05, 0x0E, 15);   /* CH1 config pair B */
fpga_timed_send_cmd(0x05, 0x10, 15);   /* CH1 config pair C */
fpga_timed_send_cmd(0x05, 0x11, 15);   /* CH1 config pair D */

/* Scope commit */
fpga_timed_send_cmd(0x05, 0x03, 20);   /* Scope activate */
```

### Longer term: Recover missing lookup data

Dump the on-device flash (not just the downloaded app image) to recover
the table at `0x080BB3FC` and the full dispatch handler code. This will
provide the definitive cmd_hi encoding for each scope parameter.

### Keep existing range/trigger/timebase sequences

The channel range (0x1A-0x1E), trigger (0x16-0x19), timebase
(0x26-0x28), and acquisition (0x20-0x21) command families are
confirmed as real wire-level cmd_lo values. Only their cmd_hi
parameters need validation against the config_writer output.

---

## Source Files Referenced

- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_raw_cmd_recovery_2026_04_08.md` -- Recovered raw TX words
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md` -- Queue/task identity correction
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md` -- FUN_08006120 analysis
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md` -- Command-bank emitter
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_dispatcher_trace_2026_04_08.md` -- Display selector trace
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fpga_task_annotated.c` -- dvom_TX frame builder, config_writer
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md` -- State structure offsets
- `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/gap_functions_annotated.c` -- Gap function analysis
- `/Users/david/Desktop/osc/firmware/src/drivers/fpga.c` -- Current osc firmware FPGA driver
