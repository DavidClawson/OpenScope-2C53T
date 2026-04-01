# FPGA Task Comprehensive Analysis

## FNIRSI 2C53T V1.2.0 — FUN_08036934

Address range: `0x08036934` - `0x08039870` (11,580 bytes)

---

## Sub-Function Map

| # | Address Range | Size | Name | Purpose |
|---|---------------|------|------|---------|
| 1 | `0x08036934-0x08036A4E` | 282B | `spi_flash_loader` | Read 4KB pages from SPI flash (W25Q128JV) into RAM; supports regions 2 (2MB offset) and 3 (raw); FAT32 signature write-back |
| 2 | `0x08036A50-0x08036ABC` | 108B | `usart_cmd_dispatcher` | FreeRTOS task loop: receives command bytes from USART queue, dispatches via function pointer table at `0x0804BE74` |
| 3 | `0x08036AC0-0x080371B0` | 1776B | `meter_data_processor` | Decodes BCD meter readings from USART RX frames; double-precision calibration; classifies readings (normal/under/over/invalid) |
| 4 | `0x080371B0-0x080373A8` | 504B | `meter_mode_handler` | 8-state FSM for meter mode: AC/DC coupling, auto-range, overload, probe detection |
| 4b | `0x080373A8-0x080373F4` | 76B | *(literal pool)* | VFP double constants for meter calibration math |
| 5 | `0x080373F4-0x08037454` | 96B | `usart_tx_frame_builder` | FreeRTOS task (dvom_TX): receives 2-byte commands from TX queue, formats 10-byte USART frame, enables TX interrupt |
| 6 | `0x08037454-0x08039050` | 7164B | `spi3_acquisition_task` | **THE BIG ONE** — SPI3 ADC acquisition engine with 9 capture modes, VFP calibration, probe compensation |
| 7 | `0x08039050-0x08039188` | 312B | `spi3_init_and_setup` | SPI3 GPIO config (PB3/4/5/6), Mode 3 (CPOL=1 CPHA=1), /2 prescaler, initial FPGA handshake |
| 8 | `0x08039188-0x080396C6` | 1342B | `input_and_housekeeping` | 15-button debounce, watchdog, timers, frequency measurement, acquisition trigger monitoring |
| 9 | `0x080396C8-0x08039734` | 108B | `probe_change_handler` | Probe connect/disconnect detection; auto power-off (15min/30min/1hr thresholds) |
| 10 | `0x08039734-0x08039870` | 316B | `usart_tx_config_writer` | 7-type command writer: scope CH1/CH2 config, trigger, timebase, meter range, siggen freq/wave |

---

## FreeRTOS Architecture

### Queue Handles (RAM addresses)

| Address | Queue | Item Size | Depth | Purpose |
|---------|-------|-----------|-------|---------|
| `0x20002D6C` | `usart_cmd_queue` | 1 byte | 20 | USART command dispatch (mode commands) |
| `0x20002D70` | `button_event_queue` | 1 byte | 15 | Button events from input scanning |
| `0x20002D74` | `usart_tx_queue` | 2 bytes | 10 | Commands to send to FPGA via USART |
| `0x20002D78` | `spi3_data_queue` | 1 byte | 15 | Triggers for SPI3 acquisition |
| `0x20002D7C` | `meter_semaphore` | 0 (binary) | 1 | Signals meter RX frame complete |
| `0x20002D80` | `fpga_semaphore_1` | 0 (binary) | 1 | SPI3 init synchronization |
| `0x20002D84` | `fpga_semaphore_2` | 0 (binary) | 1 | EXTI notification |

### Task Interactions

```
                     usart_cmd_queue
TMR3 ISR -----> usart2_irq_handler ---------> usart_cmd_dispatcher
     |               |                              |
     |               v                              v
     |         meter_semaphore               USART_CMD_DISPATCH_TABLE[n]()
     |               |                              |
     |               v                              |
     |         meter_data_processor                  |
     |               |                              |
     |               v                              v
     |         meter_mode_handler            usart_tx_config_writer
     |                                              |
     |                                              v
     |                                       usart_tx_queue
     |                                              |
     |                                              v
     |                                       usart_tx_frame_builder
     |                                              |
     |                                              v
     |                                       USART2 TX (10 bytes) --> FPGA
     |
     +------> input_and_housekeeping
     |               |
     |               v
     |         spi3_data_queue
     |               |
     |               v
     |         spi3_acquisition_task
     |               |
     |               v
     |         SPI3 <---> FPGA (ADC data)
     |               |
     |               v
     |         ms[0x5B0] (CH1 buf)
     |         ms[0x9B0] (CH2 buf)
     |
     +------> button_event_queue --> key task
```

---

## SPI3 Data Format

### Physical Layer

- **Clock**: 60 MHz (APB1=120MHz / prescaler 2)
- **Mode**: 3 (CPOL=1, CPHA=1) — clock idle HIGH, sample on falling edge
- **Frame**: 8-bit, MSB first
- **CS**: PB6, software-controlled, active LOW
- **Control pins**: PC6 = SPI enable (HIGH), PB11 = active mode (HIGH)

### Transfer Protocol

Every SPI3 exchange follows this pattern:

```
MCU                          FPGA
 |                             |
 | CS_ASSERT (PB6 LOW)        |
 |                             |
 | --> command byte            |
 | <-- status/data byte        |  (simultaneous full-duplex)
 |                             |
 | --> 0xFF (clock out)        |
 | <-- data byte 1             |
 |                             |
 | --> 0xFF (clock out)        |
 | <-- data byte 2             |
 |    ...                      |
 |                             |
 | CS_DEASSERT (PB6 HIGH)     |
```

### SPI3 Command Bytes (sent as first byte after CS assert)

| Byte | Mode | Description |
|------|------|-------------|
| `timebase_idx` | Case 0 | Fast timebase configuration (0x00-0x13) |
| `0x03` | Idle | Dummy/keepalive when `spi3_transfer_mode == 0` |
| `0xFF` | Cases 2,3 | Bulk read command (returns ADC data stream) |
| `active_channel` | Case 5 | Meter ADC read (channel 0 or 1) |
| `trigger_mode` | Case 6 | Signal generator feedback |
| `0x0A` | Case 7 sub | Calibration sub-command |
| `voltage_range XOR` | Various | Transformed voltage range for FPGA |

### ADC Data Packing

**Normal scope mode (cases 2,3):** Interleaved 8-bit samples

```
Byte 0: CH1 sample [0]
Byte 1: CH2 sample [0]
Byte 2: CH1 sample [1]
Byte 3: CH2 sample [1]
...
```

- Case 2: 1024 bytes total (512 sample pairs), stored at `ms+0x5B0`
- Case 3: 2048 bytes total (1024 sample pairs), split to `ms+0x5B0` (CH1) and `ms+0x9B0` (CH2)
- Raw values: unsigned 8-bit (0-255)

**Roll mode (case 1):** Single interleaved pair per trigger

```
Byte 0: CH1 high (reference)
Byte 1: CH1 data
Byte 2: CH2 data
Byte 3: CH2 extra
Byte 4: Last byte (stored at ms+0x5AF)
```

- Circular buffer: 300 samples max (`0x12C`)
- Buffer locations: `ms+0x356` (CH1), `ms+0x483` (CH2)

**Calibration readback (case 7):** 16-bit assembled value

```
Phase 1: command → high byte
Phase 2: 0x0A → status, 0xFF → low byte 1, 0xFF → low byte 2
Result: (high << 8) | low → ms[0x46]
```

---

## ADC Calibration Pipeline

### VFP Register Assignment (loaded once at task entry)

| Register | Value | Purpose |
|----------|-------|---------|
| `s16` | `-28.0` | ADC zero offset (hardware-specific) |
| `s18` | *(literal pool)* | Calibration gain CH1 |
| `s20` | *(literal pool)* | Calibration offset CH2 |
| `s22` | *(literal pool)* | DC bias correction |
| `s24` | *(literal pool)* | Maximum clamp value (likely 255.0) |
| `s26` | *(literal pool)* | Minimum clamp value (likely 0.0) |
| `s28` | *(literal pool)* | Range divisor |

### Per-Sample Calibration Formula

```c
// Applied to each raw 8-bit ADC sample
float raw_f    = (float)(uint8_t)raw_sample;
float norm     = (raw_f + s16_offset) / s28_divisor;      // Normalize to 0..1
int8_t dc_off  = meter_state[0x04];                        // Per-channel DC offset
float range    = (float)((int16_t)voltage_range - dc_off); // Display range
float result   = norm * range + (float)dc_off + s22_bias;  // Scale to display

// Clamp
if (result > s24_max) result = s24_max;   // 255.0
if (result < 0.0f)    result = s26_min;   // 0.0

calibrated = (uint8_t)(int)result;
```

### Roll Mode Probe Compensation

For probe types with value > `0xDC` (220):

```c
float probe_f = (float)probe_type;
float probe_gain = (probe_f + s16_offset) / s18_gain;
float raw_f   = (float)raw_byte;
float adjusted = (raw_f + s20_offset - (float)dc_offset) / probe_gain;
float result   = adjusted + s22_bias + (float)dc_offset;
// Clamp as above
```

---

## 9-Mode Acquisition State Machine

```
                    trigger_byte - 1
                         |
         ┌───────────────┼───────────────────────────────┐
         |               |                               |
    ┌────v────┐   ┌──────v──────┐   ┌────────────┐  ┌───v───┐
    │ Case 0  │   │   Case 1   │   │  Case 2    │  │Case 3 │
    │ FAST TB │   │ ROLL MODE  │   │  NORMAL    │  │ DUAL  │
    │ config  │   │ circ buf   │   │ 1024 bytes │  │ 2048B │
    │ only    │   │ 300 samp   │   │ interleave │  │CH1+CH2│
    └────┬────┘   └──────┬─────┘   └─────┬──────┘  └───┬───┘
         |               |               |             |
         └───────────────┼───────────────┴─────────────┘
                         |
                    ┌────v─────┐
                    │ Calibrate│
                    │  (VFP)   │
                    └──────────┘

    ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
    │ Case 4  │  │ Case 5  │  │  Case 6  │  │  Case 7  │  │  Case 8  │
    │EXTENDED │  │METER ADC│  │ SIGGEN   │  │CALIBRATE │  │SELF TEST │
    │ cmd only│  │ ch read │  │ feedback │  │ 16-bit   │  │ verify   │
    └─────────┘  └─────────┘  └──────────┘  └──────────┘  └──────────┘
```

---

## USART2 Command Code Table

### TX Frame Format (MCU to FPGA, 10 bytes)

```
[0] [1]  [2]     [3]    [4]-[8]  [9]
HDR HDR  CMD_HI  CMD_LO  PARAMS  CHECKSUM

Checksum = (cmd_item + CMD_HI) & 0xFF
```

### RX Frame Types (FPGA to MCU)

| Type | Header | Length | Trigger | Content |
|------|--------|--------|---------|---------|
| Data | `5A A5` | 12 bytes | Queue send + PendSV | Meter data, ADC status, button state |
| Echo | `AA 55` | 10 bytes | None (validation only) | Command acknowledgment |

### Known USART Command Codes

From `FPGA_PROTOCOL.md` and `usart_tx_config_writer` analysis:

| Code | Mode | Purpose | Config Writer Type |
|------|------|---------|-------------------|
| `0x00` | All | Reset/Init | — |
| `0x01` | Scope | Configure channel | Type 0 (CH1) or Type 1 (CH2) |
| `0x02` | Siggen | Setup | Type 5 (freq) |
| `0x03` | Siggen | Setup | Type 6 (wave) |
| `0x04` | Siggen | Setup | — |
| `0x05` | Siggen | Setup | — |
| `0x06` | Siggen | Setup | — |
| `0x07` | Meter | Probe detected (PC7 HIGH) | Type 4 (range) |
| `0x08` | Meter | Configure | Type 4 (range) |
| `0x09` | Meter | Start measurement | — |
| `0x0A` | Meter | No probe detected | — |
| `0x0B-0x11` | Scope | Channel/trigger/timebase | Types 0-3 |
| `0x12-0x14` | Meter | Variant setup | Type 4 |
| `0x15` | Standalone | Mode 7 | — |
| `0x16-0x1E` | Meter | Range configurations | Type 4 |
| `0x1F-0x21` | Mode 4 | Unknown configs | — |
| `0x25-0x28` | Mode 5 | Unknown configs | — |
| `0x29` | Mode 6 | Standalone | — |
| `0x2C` | Mode 8 | Config | — |

### Config Writer Bit Mappings

**Scope Channel Config (Types 0,1):**

```
config[0x20] bit layout:
  CH1: bit 1 = AC/DC coupling, bit 3 = bandwidth limit
  CH2: bit 5 = AC/DC coupling, bit 7 = bandwidth limit
  Trigger: bit 9 = edge select, bit 11 = source select

config[0x18] voltage range:
  bits [1:0] = range low (2 bits)
  bits [15:12] = range high (4 bits)
```

---

## Meter Data Processing

### BCD Digit Extraction

USART RX frame bytes [2..6] are cross-byte nibble pairs:

```
digit0 = lookup((rx[2] & 0xF0) | (rx[3] & 0x0F))
digit1 = lookup((rx[3] & 0xF0) | (rx[4] & 0x0F))
digit2 = lookup((rx[4] & 0xF0) | (rx[5] & 0x0F))
digit3 = lookup((rx[5] & 0xF0) | (rx[6] & 0x0F))
```

### Special Digit Codes

| Pattern | Meaning |
|---------|---------|
| `digit0=0x0A, digit1=0x0B` | "OL" — Overload |
| `digit0=0x10, digit1=0x10` | Blank display |
| `digit0=0x10, digit1=0x11` | Partial blank |
| `digit1=0x12, digit2=0x0A, digit3=5` | Continuity detected (triggers buzzer) |
| `digit1=0x13, digit2=0x14` | Mode change indicator |
| `any digit = 0xFF` | Invalid/unrecognized |

### Meter Mode State Machine (8 states)

```
State 0 (IDLE):
    Check rx[7] bits for AC, auto-range, overload
    → transitions to states 1-5 based on detected condition

State 1 (POLARITY):
    rx[7] bit 0 → set cal_coeff

State 2 (OVERLOAD/RANGE):
    If overload_flag == 1: check rx[7] bit 0
    Else: check bit 3 for range change

State 3 (AC/DC):
    rx[7] bit 2 = AC flag
    rx[6] bit 6 = hold flag

State 4 (RANGE INDICATOR):
    rx[6] bit 5 = range 1
    rx[6] bit 4 = range 2
    rx[7] bit 0 = polarity select

State 5 (AUTO-RANGE):
    ms[0xF39] flag → set cal_coeff to 0 or 4

States 6,7 (STANDBY):
    rx[6] bit 4 → cal_coeff selection
```

---

## Button Input System

### GPIO to Button Mapping

Three groups of inputs are scanned from GPIO:

**Group 1 (GPIOC-derived):**
- `0x0080`: CH1 probe change
- `0x0800`: CH2 probe change
- `0x0100`: Probe type A
- `0x0200`: Probe type B

**Group 2 (PB8-derived):**
- `0x0020`: Button group 1
- `0x0010`: Button group 2
- `0x0008`: Button group 3
- `0x2000`: Special button

**Group 3 (GPIOC IDR bit 10):**
- `0x1000`: Trigger button
- `0x0004`: Select button
- `0x4000`: Menu button
- `0x0002`: OK button

### Debounce Algorithm

```
For each of 15 buttons:
  if (pressed):
    if (counter != 0xFF) counter++
    if (counter == 0x46):     // 70 ticks = SHORT PRESS confirmed
      emit button_map[btn + 0x0F]
    if (counter == 0x48):     // 72 ticks = LONG PRESS
      emit button_map[btn]
      counter--               // Hold at 0x47
  else (released):
    if (2 <= counter <= 0x45):
      emit button_map[btn]    // Release event
    counter = 0
```

Button map table at `0x08046528` translates physical positions to logical IDs.

---

## Key Discoveries

### 1. ADC Offset of -28.0

The hardcoded VFP constant `s16 = -28.0` reveals the FPGA ADC has a DC offset of approximately 28 LSBs. All raw samples are corrected by subtracting this offset before calibration. This is critical for our replacement firmware.

### 2. Dual-Interface FPGA Communication

The FPGA uses TWO independent interfaces simultaneously:
- **USART2** (9600 baud): Low-bandwidth command/control channel. 10-byte commands, 12-byte responses. Timer-driven (TMR3), not interrupt-driven.
- **SPI3** (60 MHz): High-bandwidth data channel. Raw ADC samples at full speed. Software CS on PB6.

### 3. Button Input via GPIO Scanning (NOT FPGA)

Contrary to earlier hypothesis, buttons are **not** delivered via FPGA/I2C. The `input_and_housekeeping` function directly scans GPIOB and GPIOC pins, debounces in software, and queues events. The 9 "mystery" buttons are on MCU GPIO pins, just read through a complex multiplexed scanning scheme with 3 groups.

### 4. Roll Mode Circular Buffer Architecture

Roll/streaming mode uses a separate 300-sample circular buffer (`ms+0x356` for CH1, `ms+0x483` for CH2), completely independent from the main 1024-byte ADC buffers. The shift-and-append algorithm moves all existing samples down by 1 position before adding the new sample — this is expensive but ensures display continuity.

### 5. Acquisition Trigger is Double-Buffered

When the acquisition counter reaches the timebase threshold, TWO items are sent to the SPI3 queue (back-to-back). This triggers two consecutive SPI3 read cycles, implementing a ping-pong double-buffer scheme that prevents display tearing.

### 6. Auto Power-Off Has Three Tiers

The probe change handler implements tiered auto-shutdown:
- Probe disconnect: 15 minutes (900 ticks)
- Probe type change: 30 minutes (1800 ticks)
- Full probe swap: 1 hour (3600 ticks)

### 7. Watchdog Feed Rate

The IWDG is fed every 11 calls to `input_and_housekeeping`. Given the typical call rate from TMR3, this gives approximately 50ms between feeds with a ~5 second timeout.

### 8. Continuity Buzzer State Machine

The meter data processor detects continuity via specific BCD digit patterns (`digit1=0x12, digit2=0x0A, digit3=5`) and sets a buzzer state variable at `ms+0xF5D` (values `0xB0`/`0xB1`). The EXTI3 interrupt handler at `0x08009C10` reads this to drive the piezo buzzer.

---

## What This Means For Our Firmware

### Immediate Action Items

1. **Set ADC offset to -28.0** in SPI3 calibration code
2. **Send TWO queue items** per acquisition trigger (double-buffer)
3. **Implement button scanning** on GPIOB/GPIOC — no need for FPGA button protocol
4. **Keep TMR3 driving USART exchange** — the ISR-based approach is required
5. **Feed IWDG every ~50ms** to prevent watchdog reset

### SPI3 Initialization Sequence (from `spi3_init_and_setup`)

```c
// 1. Configure GPIO (after AFIO remap frees PB3/4/5)
//    PB3 = AF_PP 50MHz (SCK)
//    PB4 = Floating input (MISO)
//    PB5 = AF_PP 50MHz (MOSI)
//    PB6 = GPIO output PP (CS)

// 2. CS HIGH (deassert)
GPIOB_BOP = 0x40;

// 3. SPI3 config: Mode 3, Master, /2, 8-bit, MSB first, SW NSS
// 4. Enable DMA/interrupt bits in CTL1
SPI3_CTL1 |= 0x03;
// 5. Enable SPI
SPI3_CTL0 |= 0x40;

// 6. Initial handshake
CS_ASSERT;
spi3_xfer(0x00);  // dummy
CS_DEASSERT;
vTaskDelay(10);
```

### Why SPI3 Wasn't Responding

Our test firmware was missing:
1. The **PB11 HIGH** signal (FPGA active mode — set in mode_switch handler)
2. The **full USART boot command sequence** (commands 0x01-0x08 sent before SPI3)
3. The **correct trigger mechanism**: data reads are initiated by queue events from `input_and_housekeeping`, not by polling
4. The **SysTick delay timing** between boot phases

---

## Files

- `fpga_task_annotated.c` — This annotated C file (all 10 functions)
- `fpga_task_decompile.txt` — Raw Ghidra output with disassembly
- `usart_protocol_decompile.txt` — USART2 ISR and protocol documentation
- `FPGA_BOOT_SEQUENCE.md` — 53-step boot timeline
- `FPGA_PROTOCOL.md` — Command code table
