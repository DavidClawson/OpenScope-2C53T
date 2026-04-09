# Acquisition Task Deep Dive — FUN_0801de98 + SPI3 Acquisition Task

## Two-Layer Architecture

The stock firmware implements acquisition through **two separate functions** that
communicate via the SPI3 data queue (`0x20002D78`):

1. **FUN_0801de98** (0x0801de98, 13,276 bytes) — the **acquisition orchestrator**.
   Called from the input/housekeeping ISR path. Manages the state machine, decides
   which acquisition mode to trigger, handles auto-ranging, calibration correction,
   and trigger detection. Sends trigger bytes (1-9) to the SPI3 queue.

2. **spi3_acquisition_task** (0x08037428, 7,208 bytes) — the **SPI3 transfer worker**.
   FreeRTOS task that blocks on the SPI3 data queue. Executes the actual SPI3
   bus transactions. Documented in `fpga_task_annotated.c` function 6.

The osc firmware's `fpga_acquisition_task()` in `firmware/src/drivers/fpga.c`
implements the SPI3 transfer worker but **does not have an equivalent of the
acquisition orchestrator**. This is a significant structural gap.

---

## Trigger Queue Mechanism (TMR3 ISR → Acquisition Task)

```
TMR3 ISR
  → input_and_housekeeping (FUN_08039188)
    → Increments acquisition_timer (ms[0x32])
    → When timer == 0xFF: sends command to USART cmd queue
    → USART cmd dispatcher calls FUN_0801de98
      → FUN_0801de98 evaluates state machine
      → Sends trigger byte to SPI3 data queue (0x20002D78)
        → spi3_acquisition_task wakes and executes SPI3 transfer
```

Key state variables checked by the orchestrator before any SPI3 trigger:

| Address       | Field                    | Gate condition                              |
|---------------|--------------------------|---------------------------------------------|
| `0x20001060`  | system_mode              | Must be `0x02` (scope mode) or return early |
| `0x20000126`  | scope_active_flag        | Nonzero = scope path; zero = alternate path |
| `0x2000010f`  | hold_mode (ms+0x17)      | `0x02` = held; affects which path is taken  |
| `0x20000ea8`  | acquisition_state        | State machine: 0→1→2→3→4→6                 |
| `0x20000125`  | timebase_index (ms+0x2D) | Selects sweep speed (0x00-0x13)             |
| `0x20000eb2`  | busy_flag_1              | Must be 0 for acquisition to proceed        |
| `0x20000eb3`  | busy_flag_2              | Must be 0 for acquisition to proceed        |
| `0x2000012a`  | frame_counter            | Threshold-gated state transitions            |

### Acquisition State Machine

```
State 0: Idle / just configured
State 1: Armed — FUN_080212ec() called, setup callees invoked
State 2: Active acquisition — SPI3 triggers being sent
State 3: Waiting for frame count threshold
State 4: Post-trigger processing
State 6: Complete — TMR3 disabled, results processed
```

---

## Per-Mode SPI3 Protocol Specification

The SPI3 acquisition task switches on `(trigger_byte - 1)`. All modes share
a **pre-acquisition CS transaction** before the mode-specific transfer:

### Pre-Acquisition Command (all modes)

```
CS_ASSERT  (PB6 LOW)
spi3_xfer(command_code)     → response discarded
CS_DEASSERT (PB6 HIGH)
```

**command_code calculation:**
```c
int16_t voltage_range = *(int16_t *)(ms + 0x1C);  // ms[0x1C] signed 16-bit
int16_t command_code = ~0x7F ^ voltage_range;      // = 0xFFFFFF80 ^ voltage_range
// Effectively: 0x80 | (voltage_range & 0x7F) for small positive ranges
```

The probe calibration path (ms[0x352] and ms[0x353] both valid, ms[0x33]==0)
modifies this with VFP-computed values before the CS transaction.

---

### Mode 0 (trigger_byte=1): SCOPE_FAST_TIMEBASE — Config Only

**Trigger condition:** `acquisition_state == 1`, fast timebase selected.
Queue send: `local_6c = 1; xQueueSend(SPI3_DATA_QUEUE, &local_6c, ...)`

**Pre-acquisition:** Standard command_code CS transaction (shared).

**Mode-specific transfer:**
```
(no additional CS_ASSERT — reuses shared deasserted state)
spi3_xfer(timebase_index)     → sends ms[0x2D] value (0x00-0x13)
```

**Byte count:** 1 byte (timebase config)
**Data format:** No ADC data read. Configures FPGA timebase only.
**Buffer destination:** None. Checks `acq_sample_count >= threshold + 0x32`.
**Post-acquisition:** If threshold met, exits. Otherwise, continues polling.

---

### Mode 1 (trigger_byte=2): SCOPE_ROLL_MODE — Circular Buffer

**Trigger condition:** `acquisition_state == 2`, roll mode active.
Queue send: `local_6f = 2; xQueueSend(SPI3_DATA_QUEUE, &local_6f, ...)`

**Pre-acquisition:** Standard command_code CS transaction.

**CS sequence:**
```
CS_ASSERT  (PB6 LOW)
spi3_xfer(0xFF)   → byte 0: CH1 reference (raw_ch1_hi)
spi3_xfer(0xFF)   → byte 1: CH1 data (raw_ch1_lo)
spi3_xfer(0xFF)   → byte 2: CH2 reference (raw_ch2_hi)
spi3_xfer(0xFF)   → byte 3: CH2 data (raw_ch2_lo)
spi3_xfer(0xFF)   → byte 4: status/last byte
CS_DEASSERT (PB6 HIGH)
```

**Byte count:** 5 bytes per trigger event.
**Data format:** 5 interleaved bytes. Byte 4 stored to ms[0x5AF].
**Buffer destination:** Circular buffers:
  - CH1: ms+0x356 (300 bytes = 0x12C entries)
  - CH2: ms+0x483 (300 bytes = 0x12C entries)

**Buffer management:**
1. Decrement `roll_read_ptr` (ms+0xDB4) if > 0
2. Increment `roll_sample_count` (ms+0xDB6), cap at 300
3. Shift existing buffer contents down by 1 (expensive memmove)
4. Store new calibrated sample at position [0]
5. When count reaches 0x12D: trigger display update, disable TMR3,
   send cmd 2 to USART queue, call measurement_calc(), copy circular
   buffer to main ADC buffers

**Calibration (roll-specific):**
```c
if (ch1_probe > 0xDC) {
    float cal = ((float)ch1_probe + (-28.0)) / s18_gain;
    float result = ((float)raw_byte + s20_offset - (float)dc_offset)
                 / cal + s22_bias + (float)dc_offset;
    clamp(result, 0.0, 255.0);
}
```

---

### Mode 2 (trigger_byte=3): SCOPE_NORMAL — 1024 Bytes Interleaved

**Trigger condition:** `acquisition_state == 4`, normal timebase, CH1 or both.
Queue send: `local_6e = 4; xQueueSend(SPI3_DATA_QUEUE, &local_6e, ...)` (but
the trigger_byte received by the task is adjusted — see mapping note below).

Wait — let me re-examine. The queue sends `4` for `case 4` in the task, which is
`trigger_byte - 1 == 3`, meaning trigger_byte=4 maps to **case 3** (dual channel).

**Re-mapping (critical):**

| Queue value | trigger_byte | switch case (trigger_byte-1) | Mode                |
|-------------|--------------|------------------------------|---------------------|
| 1           | 1            | case 0                       | Fast timebase       |
| 2           | 2            | case 1                       | Roll mode           |
| 3           | 3            | case 2                       | Normal (1024B)      |
| 4           | 4            | case 3                       | Dual channel (2048B)|
| 5           | 5            | case 4                       | Extended command    |
| 6           | 6            | case 5                       | Meter ADC read      |
| 7           | 7            | case 6                       | Siggen feedback     |
| 8           | 8            | case 7                       | Calibration         |
| 9           | 9            | case 8                       | Self test           |

Queue sends observed in the orchestrator:
- **1** → Roll mode (case 0 = fast TB)
- **4** → Normal scope CH1 path (case 3 = dual channel)
- **5** → Normal scope CH2 path (case 4 = extended)
- **6** → Meter ADC (case 5)
- **8** → Calibration (case 7)
- **9** → Self test (case 8)

**The orchestrator sends 4 and 5 back-to-back for dual-channel acquisition.**
This is the double-buffer ping-pong: trigger_byte=4 reads CH1+CH2 interleaved
into the first buffer, trigger_byte=5 reads the second half.

**CS sequence (case 2, trigger_byte=3, normal):**
```
(CS already asserted from pre-acquisition? No — pre-acquisition deasserts first)
spi3_xfer(0xFF)              → discard (command/echo byte)
loop 512 times (i=0 to 0x3FF step 2):
    spi3_xfer(0xFF)          → CH1 byte → ms[0x5B0 + i]
    spi3_xfer(0xFF)          → CH2 byte → ms[0x5B0 + i + 1]
```

**Important: The CS assertion for the bulk read is a SINGLE long assertion.**
Looking at the annotated code (lines 900-924), there is one CS_ASSERT at the
start and one CS_DEASSERT at the end. The entire 1024-byte bulk read happens
within one CS window.

**Byte count:** 1 (echo) + 1024 (data) = 1025 SPI bytes.
**Data format:** Interleaved in a single buffer: even bytes = CH1, odd bytes = CH2.
**Buffer destination:** ms+0x5B0 (1024 bytes, interleaved CH1/CH2).
**Post-acquisition:** Falls through to VFP calibration at 0x08037D9C.

---

### Mode 3 (trigger_byte=4): SCOPE_DUAL_CHANNEL — 2048 Bytes Split

**Trigger condition:** Dual-channel active. Sent as first of back-to-back pair (4, then 5).

**CS sequence (case 3, trigger_byte=4):**
```
spi3_xfer(0xFF)              → discard (command/echo byte)
loop 1024 times (i=0x400 to 0x7FF step 2):
    spi3_xfer(0xFF)          → byte → ms[0x5B0 + i]       (= ms[0x9B0 + j])
    spi3_xfer(0xFF)          → byte → ms[0x5B0 + i + 1]
```

**Byte count:** 1 (echo) + 2048 (data) = 2049 SPI bytes.
**Data format:** Interleaved bytes, but offset starts at 0x400 into the buffer.
  - Even bytes (relative) → CH1 buffer at ms+0x9B0
  - Odd bytes (relative) → CH2 continuation
**Buffer destination:** ms+0x9B0 (upper 1024 bytes of the 2KB acquisition area).
**Post-acquisition:** Falls through to calibration at 0x0803806C.

**CRITICAL DISCREPANCY WITH OSC FIRMWARE:**
The stock firmware case 3 reads into ms[0x5B0 + 0x400] through ms[0x5B0 + 0x7FF],
which is ms[0x9B0] through ms[0xDB0]. The data is still interleaved (even=CH1,
odd=CH2) in the raw buffer, with de-interleaving happening during calibration.

The osc firmware (fpga.c lines 911-933) reads case 4 (trigger_byte=4, which maps
to stock case 3) as:
```c
// WRONG: reads 1024 sequential bytes into ch1_buf, then 1024 into ch2_buf
for (i = 0; i < 1024; i++) { fpga.ch1_buf[i] = spi3_xfer(0xFF); }
for (i = 0; i < 1024; i++) { fpga.ch2_buf[i] = spi3_xfer(0xFF); }
```

**This is incorrect.** The stock firmware reads interleaved data (ABABABAB...)
and de-interleaves during calibration, not sequential CH1-then-CH2.

---

### Mode 4 (trigger_byte=5): SCOPE_EXTENDED — Single Command Exchange

**Trigger condition:** Extended mode / second half of dual-channel pair.

**CS sequence (case 4):**
```
spi3_xfer(command_code)      → read response (discarded)
```

**Byte count:** 1 SPI byte.
**Data format:** N/A — command/response only.
**Buffer destination:** None directly (status byte from pre-acquisition stored).
**Post-acquisition:** CS_DEASSERT, done.

---

### Mode 5 (trigger_byte=6): METER_ADC_READ

**Trigger condition:** Meter mode active, CH1 or CH2 selected.
Queue send: `local_6a = 6; xQueueSend(SPI3_DATA_QUEUE, &local_6a, ...)`

**CS sequence (case 5):**
```
spi3_xfer(active_channel)    → sends ms[0x16] (0 or 1)
```

**Byte count:** 1 SPI byte.
**Data format:** Single byte response = meter ADC value.
**Buffer destination:** Response used by meter processing pipeline.

---

### Mode 6 (trigger_byte=7): SIGGEN_FEEDBACK

**Trigger condition:** Signal generator mode active.

**CS sequence (case 6):**
```
spi3_xfer(trigger_mode)      → sends ms[0x18]
```

**Byte count:** 1 SPI byte.
**Data format:** Single byte feedback from signal generator output.

---

### Mode 7 (trigger_byte=8): CALIBRATION — 16-bit Readback

**Trigger condition:** Calibration mode, or auto-calibration cycle.
Queue send: `local_6a = 8; xQueueSend(SPI3_DATA_QUEUE, &local_6a, ...)`

**CS sequence (case 7, two phases):**
```
Phase 1:
    spi3_xfer(command_code)  → high_byte = SPI3_DATA
    ms[0x46] = high_byte
    CS_DEASSERT
    vTaskDelay(1)            → 1 tick delay between phases
    CS_ASSERT

Phase 2:
    spi3_xfer(0x0A)          → sub-command for low byte read
    spi3_xfer(0xFF)          → clock out (response discarded?)
    spi3_xfer(0xFF)          → low_byte = SPI3_DATA

Result: ms[0x46] = (high_byte << 8) | low_byte
ms[0xDB0] frame counter incremented
```

**Byte count:** 4 SPI bytes (1+3) across 2 CS windows.
**Data format:** 16-bit assembled value (FPGA version/status).
**Buffer destination:** ms+0x46 (spi3_status_word).

---

### Mode 8 (trigger_byte=9): SELF_TEST

**Trigger condition:** Boot validation or diagnostic.
Queue send: `local_6e = 9; xQueueSend(SPI3_DATA_QUEUE, &local_6e, ...)`

**CS sequence (case 8):**
```
spi3_xfer(command_code)      → send command
spi3_xfer(0xFF)              → read response byte 1
spi3_xfer(0xFF)              → read response byte 2
```

**Byte count:** 3 SPI bytes, single CS window.
**Data format:** 2 response bytes for communication integrity check.

---

## Trigger Sources and Double-Buffering

### Source 1: input_and_housekeeping (ISR-driven, trigger_byte=1)

The `input_and_housekeeping` function (0x08039188) monitors PC0 (FPGA data-ready,
active LOW). When the acquisition counter reaches the timebase threshold:

```c
// input_and_housekeeping, lines 1322-1327 of annotated file:
if (threshold == count) {
    uint8_t trigger = 1;                              // trigger_byte=1 → case 0
    xQueueSend(SPI3_DATA_QUEUE, &trigger, FOREVER);   // first
    xQueueSend(SPI3_DATA_QUEUE, &trigger, FOREVER);   // second (back-to-back)
}
```

This sends trigger_byte=1 **twice**, triggering two consecutive case 0 (fast
timebase config) SPI3 transfers. These configure the FPGA timebase but do NOT
read bulk ADC data.

### Source 2: Acquisition Orchestrator (FUN_0801de98, trigger_bytes 4 and 5)

The orchestrator sends bulk-read triggers conditionally:

```c
// FUN_0801de98, lines 1502-1511 in decompiled output:
if ((channel_config & 1) != 0 || timebase < 5) {    // CH1 enabled or fast TB
    acquisition_state = 0;
    local_6d = 4;                                    // trigger_byte=4 → case 3 (dual)
    xQueueSend(SPI3_DATA_QUEUE, &local_6d, FOREVER);
}
if (timebase < 5 || (channel_config & 2) != 0) {    // fast TB or CH2 enabled
    acquisition_state = 0;
    local_6d = 5;                                    // trigger_byte=5 → case 4 (extended)
    xQueueSend(SPI3_DATA_QUEUE, &local_6d, FOREVER);
}
```

When both channels are enabled and timebase < 5:
- trigger_byte=4 → case 3: reads 1024 bytes into ms+0x9B0 (the "upper" buffer)
- trigger_byte=5 → case 4: sends single command_code byte (re-arm/status)

**This is NOT a "read CH1 then read CH2" double buffer.** Case 3 reads
interleaved CH1+CH2 data into the second 1KB buffer. Case 4 is a re-arm command.

### Source 3: Orchestrator path for scope_active (trigger_byte=1)

In a different code path (line 1389):
```c
local_6a = 1;                                        // trigger_byte=1 → case 0
xQueueSend(SPI3_DATA_QUEUE, &local_6a, FOREVER);
local_69 = 2;
xQueueSend(USART_CMD_QUEUE, &local_69, FOREVER);    // USART command, NOT SPI3
```

### Key Finding: trigger_byte=3 (case 2, normal 1KB) is Never Sent

**The orchestrator never sends trigger_byte=3.** The "normal scope" acquisition
path (case 2, 1024 bytes into ms+0x5B0) appears to be dead code in the main
acquisition flow, or is triggered by a code path not in FUN_0801de98.

The primary scope data path is:
1. `input_and_housekeeping` sends trigger_byte=1 twice (fast TB config)
2. Orchestrator state machine advances
3. Orchestrator sends trigger_byte=4 (case 3: read 1KB interleaved into upper buf)
4. Orchestrator sends trigger_byte=5 (case 4: single command re-arm)

The osc firmware's `fpga_trigger_scope_read()` implementation:
```c
if (fpga.acq_mode == (FPGA_ACQ_DUAL + 1)) {
    xQueueSend(queue, FPGA_ACQ_NORMAL + 1, 0);   // trigger_byte = 3
    xQueueSend(queue, FPGA_ACQ_DUAL + 1, 0);     // trigger_byte = 4
}
```

This sends 3 then 4 — **but the stock sends 4 then 5**. The osc firmware
triggers case 2 (normal 1KB) which may be the correct data path for single-
channel, but the stock firmware's dual-channel path uses case 3 + case 4.

---

## Command Code Calculation Detail

```c
// From FUN_0801de98 around line 1563:
uint8_t timebase_idx = DAT_20000125;        // ms[0x2D]
uint8_t div3 = timebase_idx / 3;            // integer division
uint8_t mod3_lookup = DAT_080465c8[timebase_idx + div3 * (-3)];  // = DAT_080465c8[timebase_idx % 3]
// This is a lookup table at 0x080465c8 indexed by (timebase_idx % 3)

// The command involves:
// 1. Load calibration constants from ms+0xEC8..0xED4
// 2. FUN_080425da(div3) — likely a power/scaling function
// 3. FUN_080408e0 — multiply with DAT_0801e210 (literal pool constant)
// 4. FUN_08042538 — combine with calibration values
// 5. FUN_08005732 — final division producing trigger position
// 6. Add 0x96 (150) offset
```

The command_code sent in the pre-acquisition CS transaction:
```c
int16_t voltage_range = *(int16_t *)(ms + 0x1C);
command_code = ~0x7F ^ voltage_range;
// = 0xFFFFFF80 ^ voltage_range
// For 8-bit SPI: bottom 8 bits = 0x80 ^ (voltage_range & 0xFF)
// Effectively sets bit 7 and XORs with voltage range
```

---

## Discrepancies with OSC Firmware

### 1. Dual-Channel Interleaving (CRITICAL BUG)

**Stock firmware (case 3):** Reads 2048 interleaved bytes (ABABAB...) into a
contiguous buffer at ms+0x9B0, then de-interleaves during VFP calibration.

**Osc firmware (case 4):** Reads 1024 bytes sequentially into ch1_buf, then
1024 bytes into ch2_buf. This assumes the FPGA sends CH1 data first, then CH2
data — but the FPGA sends interleaved data.

**Fix:** Change case 4 to read interleaved:
```c
case 4: {
    for (int i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
        uint8_t ch1_raw = spi3_xfer(0xFF);
        uint8_t ch2_raw = spi3_xfer(0xFF);
        fpga.ch1_buf[i] = calibrate(ch1_raw);
        fpga.ch2_buf[i] = calibrate(ch2_raw);
    }
    break;
}
```

### 2. Trigger Byte Mapping and Acquisition Flow Mismatch

**Stock firmware primary scope path:**
1. `input_and_housekeeping` sends trigger_byte=1 twice (case 0: fast TB config)
2. Orchestrator sends trigger_byte=4 (case 3: read 1024B interleaved into upper buf)
3. Orchestrator sends trigger_byte=5 (case 4: re-arm command)
4. trigger_byte=3 (case 2: normal 1KB) is NEVER sent by the main acquisition path

**Osc firmware scope path:**
- Sends trigger_byte=3 (case 2: normal 1KB) for single channel
- Sends trigger_byte=3 then 4 for dual channel
- No equivalent of the fast TB config (trigger_byte=1) precursor
- No re-arm command (trigger_byte=5) afterward

The osc firmware bypasses the fast-timebase configuration step and the re-arm
step entirely. The FPGA may need the trigger_byte=1 (fast TB config) as a
precursor to arm the ADC sample buffer before the bulk read.

### 3. Missing Acquisition Orchestrator

The osc firmware has no equivalent of FUN_0801de98. The stock firmware's
orchestrator handles:
- Auto-ranging (the massive nested if-else for timebase selection, lines 298-394)
- Acquisition state machine (states 0-6)
- Frame counter gating (wait for N frames before transitioning)
- Trigger detection and sorting (the trigger search loop at ~lines 2050-2140)
- Roll mode sample decimation and buffer management
- Calibration auto-correction (DC offset drift compensation, lines 470-600)

The osc firmware calls `fpga_trigger_scope_read()` directly from a timer or UI
loop, skipping all of this state management.

### 4. Roll Mode Not Implemented

The osc firmware's case 2 (roll mode) reads 4 bytes and discards them:
```c
uint8_t roll_b1 = spi3_xfer(0xFF);  // ... all discarded
```

The stock firmware reads 5 bytes (not 4), stores the 5th byte at ms[0x5AF],
applies probe-specific VFP calibration, manages a 300-entry circular buffer
with shift operations, and triggers display updates when the buffer fills.

### 5. Missing Echo Byte in Normal Mode

The stock firmware's case 2 (normal scope) sends an initial `spi3_xfer(0xFF)`
whose return value is discarded (echo byte), THEN reads 1024 data bytes.

The osc firmware (case 3) correctly includes this echo byte:
```c
uint8_t echo = spi3_xfer(0xFF);  // ✓ correct
```
This one is actually correct in the osc firmware.

### 6. CS Assertion Around Bulk Read

The stock firmware does NOT re-assert CS between the pre-acquisition command
and the bulk read. The sequence is:

```
CS_ASSERT → command_code → CS_DEASSERT     (pre-acquisition)
[brief instruction-level gap, no explicit delay]
CS_ASSERT → 0xFF echo → 1024 data bytes → CS_DEASSERT  (bulk read)
```

The osc firmware inserts a `volatile int d` delay loop (100 iterations ≈ few μs),
which should be fine but is not present in the stock firmware.

### 7. Calibration Formula Simplification

The osc firmware applies a simple offset:
```c
int16_t ch1_cal = (int16_t)ch1_raw + (int16_t)FPGA_ADC_OFFSET;  // +(-28)
```

The stock firmware applies a full VFP calibration:
```c
float normalized = ((float)raw + (-28.0)) / s28_divisor;
float range = (float)(voltage_range - dc_offset);
float result = normalized * range + (float)dc_offset + s22_bias;
clamp(result, 0.0, 255.0);
```

The simple offset is adequate for getting data on screen but produces uncalibrated
amplitude readings. The full formula accounts for probe gain, display range,
per-channel DC offset, and bias.

---

## Channel Config Byte (DAT_2000010c / ms+0x14)

The `spi3_transfer_mode` byte at ms+0x14 (`DAT_2000010c` in the decompile) is
used as a bitfield that controls which SPI3 triggers are sent:

| Value | Bit 0 | Bit 1 | Meaning                           | Triggers sent |
|-------|-------|-------|-----------------------------------|---------------|
| 0x00  |   0   |   0   | Neither channel active            | None          |
| 0x01  |   1   |   0   | CH1 only                          | 4             |
| 0x02  |   0   |   1   | CH2 only                          | 5             |
| 0x03  |   1   |   1   | Both channels (dual)              | 4, 5          |

When timebase_index < 5 (fast timebases), both triggers are always sent
regardless of channel config. This suggests the FPGA requires both the
bulk-read and the re-arm at fast sweep speeds.

The special value `0x03` also appears in several data indexing decisions within
the orchestrator (e.g., choosing which buffer offset to read from, whether to
use 0x400 or 0x800 wrap points). The decompiled code shows:
```c
if (DAT_2000010c == '\x03' || timebase > 3) {
    // Use 0x400-wrap indexing (1KB buffer per channel)
} else {
    // Use 0x800-wrap indexing (2KB interleaved buffer)
}
```

---

## Summary: Priority Fixes for OSC Firmware

1. **Fix dual-channel interleaving** — case 4 must read interleaved ABAB, not
   sequential AAAA BBBB. This is the most likely cause of broken dual-channel.

2. **Align trigger byte values** — either adjust the enum to match stock (add 1
   to all queue values) or ensure the switch cases handle the osc firmware's
   encoding correctly. Current code works but the back-to-back ping-pong sends
   the wrong pair of values.

3. **Implement roll mode** — read 5 bytes (not 4), store to circular buffers,
   apply calibration.

4. **Add acquisition state machine** — even a minimal version that gates
   trigger sends on acquisition_state would prevent overwhelming the FPGA.
