# FPGA Communication Protocol — Reverse Engineering Analysis

*Decoded from decompiled V1.2.0 firmware (decompiled_2C53T_v2.c, ~39K lines)*
*Initial analysis: 2026-03-20. Updated: 2026-03-29 with command codes, task architecture, and memory map.*

## Architecture Overview

The FNIRSI 2C53T uses a **dual-interface** model between the GD32F307 MCU and the Gowin GW1N-UV2 FPGA:

```
MCU (GD32F307)                           FPGA (Gowin GW1N-UV2)
┌─────────────┐                          ┌──────────────────┐
│             │── USART2 (cmd/ctrl) ────>│                  │
│ fpga_task   │<── USART2 (response) ───│  ADC controller  │
│             │                          │  Trigger engine   │
│ osc_task    │<── SPI2 (bulk data) ────│  Sample memory    │
│             │                          │  DVOM mux         │
│ dvom_tx     │── USART2 (config) ─────>│                  │
└─────────────┘                          └──────────────────┘
```

**USART2** (0x40004400): Command/control channel. 10-byte frames MCU->FPGA, 2/10/12-byte responses FPGA->MCU.

**SPI2** (0x40003C00): High-speed bulk data channel. MCU reads ADC sample buffers from FPGA.

## USART2 Protocol

### TX Frame (MCU -> FPGA): 10 bytes

```
Byte[0-1]: Header / sync markers
Byte[2]:   Command parameter high byte
Byte[3]:   Command parameter low byte (ECHOED back in RX for verification)
Byte[4-8]: Command data fields (5 bytes, meaning varies by command)
Byte[9]:   Checksum: (param + (param >> 8)) & 0xFF
```

**TX buffer**: RAM 0x20000005 (10 bytes)
**TX index**: RAM 0x2000000F
**ISR**: usart2_isr_real @ 0x080277B4 (byte-by-byte TX pump)

### RX Frame Types

**Type 1 -- ACK (2 bytes)**:
```
[0x5A, 0xA5]  -- Synchronization acknowledgment from FPGA
```

**Type 2 -- Status Response (10 bytes)**:
```
Byte[0]: 0xAA (frame header)
Byte[1]: 0x55 (frame header)
Byte[2]: Status/data byte
Byte[3]: ECHO of TX Byte[3]    <-- verified by ISR, discarded on mismatch
Byte[4-6]: Status fields
Byte[7]: 0xAA (integrity marker) <-- MUST be 0xAA or frame is discarded
Byte[8-9]: Checksum/reserved
```

**Type 3 -- Acquisition Complete (12 bytes)**:
```
Extended frame signaling end of sample capture.
Triggers PendSV (writes 0x10000000 to 0xE000ED04) for context switch.
Wakes osc_task to process captured waveform data.
```

### Frame Validation (usart2_isr @ 0x080277B4)

```c
if (rx_buffer[3] != tx_buffer[3])   // Echo mismatch -> discard
    return;
if (rx_buffer[7] != 0xAA)           // Integrity marker wrong -> discard
    return;
// Frame accepted -- process response
```

Two-layer validation: echo verification + integrity marker. Robust against line noise.

## FPGA Command Codes

Commands are dispatched via FreeRTOS queue (`_fpga_queue_handle`). Command code is byte[0] of the queue message.

| Code | Purpose | Details |
|------|---------|---------|
| 1 | Device mode setup | Writes device_mode (0x00-0x12) to SPI2. Configures scope/meter/siggen. |
| 2 | Sample rate / config | Writes acquisition parameters (timebase, sample depth) to SPI2. |
| 3 | Waveform data process | Handles display list updates, frame buffers at 0x200006A8. Processes calibration. |
| 4 | Buffer fill/clear | Fills display buffers with 0xFF, processes 0x400 (1024) samples. |
| 5+ | Channel configuration | Trigger level, coupling, probe settings, channel enable/disable. |

### Command Prefix Encoding

Commands use a prefix byte ORed into the upper bits of `_fpga_command_code`:

```c
_fpga_command_code = CONCAT13(0x20, _fpga_command_code);  // Normal execution
_fpga_command_code = CONCAT13(0x21, _fpga_command_code);  // Error recovery / retry
_fpga_command_code = CONCAT13(0x24, _fpga_command_code);  // Special waveform mode
_fpga_command_code = CONCAT13(0x25, _fpga_command_code);  // Alt mode
```

This suggests a command structure of: `[prefix_byte][3 command bytes]` where the prefix indicates the execution context (normal, retry, special).

## SPI2 Protocol (Bulk Data)

### Registers

| Address | Name | Purpose |
|---------|------|---------|
| 0x40003C08 | SPI2_STAT | Status (bit 0 = ready, bit 1 = busy) |
| 0x40003C0C | SPI2_DATA | Data TX/RX register |
| 0x40010C14 | GPIOB_BOP | Chip-select control (bit 6 = PB6) |

### Access Pattern (from fpga_task @ 0x08037454)

```c
// 1. Assert chip-select
GPIOB_BOP = 0x40;              // Set PB6

// 2. Wait for SPI ready
while (!(SPI2_STAT & 0x01));   // Poll ready bit

// 3. Write command / read data
SPI2_DATA = command_byte;

// 4. Wait for completion
while (SPI2_STAT & 0x02);      // Poll busy bit
```

Chip-select polarity (PB6 active-high vs active-low) needs verification on hardware.

## Acquisition Pipeline

```
1. MCU sends 10-byte command via USART2
   (configure timebase, trigger, channel settings)
        |
        v
2. FPGA acknowledges with [0x5A, 0xA5]
   or returns 10-byte status [0xAA, 0x55, ...]
        |
        v
3. FPGA captures ADC samples at configured rate (up to 250 MS/s)
        |
        v
4. MCU reads sample data via SPI2 (bulk transfer, GPIO chip-select)
        |
        v
5. FPGA sends 12-byte completion frame via USART2
   -> triggers PendSV -> context switch
        |
        v
6. osc_task wakes, processes waveform from shared buffers
   -> sends display commands to display task queue
```

## RTOS Task Architecture (Original Firmware)

```
dvom_tx_task (priority ~2)
  |-- Receives TX frames from _dvom_tx_queue_handle
  |-- Builds 10-byte USART2 frame
  |-- Enables USART2 TX interrupt for byte-by-byte pump
  |-- vTaskDelay(10) between frames (~100 frames/sec max)

fpga_task (priority ~3)
  |-- Receives command codes from _fpga_queue_handle (blocking wait)
  |-- Switch on command code:
  |     Case 1: Configure device mode via SPI2
  |     Case 2: Set sample rate/config via SPI2
  |     Case 3: Process waveform data, manage frame buffers
  |     Case 4: Fill/clear frame buffers (1024 samples)
  |     Case 5+: Channel/trigger configuration
  |-- Manages calibration data

osc_task (priority ~2)
  |-- Awakened by PendSV from USART2 RX completion ISR
  |-- Reads waveform data from shared buffers (0x200006A8)
  |-- Processes for display (scaling, measurements)
  |-- Sends display commands to display task

dvom_rx_task (priority ~2)
  |-- Processes multimeter readings received from FPGA
  |-- Parses DVOM response frames
```

## Memory Map

### FPGA Communication Buffers

| Address | Size | Description |
|---------|------|-------------|
| 0x20000005 | 10 | USART2 TX buffer (command frame) |
| 0x2000000F | 1 | TX byte index (ISR byte pump counter) |
| 0x20000008 | 1 | TX[3] copy for echo verification |
| 0x20000019 | 1 | USART status/command state |
| 0x20000025 | 1 | Current FPGA command code (0x20/0x21/0x24/0x25) |
| 0x20004E10 | 1 | USART2 RX byte index |
| 0x20004E11 | 12 | USART2 RX buffer |
| 0x20004E12 | 1 | RX[3] echo byte (verified against TX[3]) |
| 0x20004E18 | 1 | RX[7] integrity marker (must be 0xAA) |

### ADC / Waveform Buffers

| Address | Size | Description |
|---------|------|-------------|
| 0x2000044A | 1 | CH1 ADC raw value |
| 0x2000044B | 1 | CH2 ADC raw value |
| 0x2000057A | 1 | Processed CH1 display data |
| 0x200006A8 | 8 | CH1/CH2 waveform frame buffer (active) |

## Key Functions

| Address | Name | Purpose |
|---------|------|---------|
| 0x080277B4 | usart2_isr_real | USART2 ISR: TX byte pump + RX frame parser + validation |
| 0x08037454 | fpga_task | FPGA command dispatcher + SPI2 driver |
| 0x08034078 | display_buf_init | Display buffer initialization |
| 0x08039EB4 | frame_builder | TX frame construction |
| 0x0802e7bc | fatfs_init | FatFS filesystem mount (NOT USART handler -- corrected) |

## Correction Log

- **FUN_0802e7bc** was initially identified as USART handler. It is actually **FatFS filesystem init** (f_mount, f_open, etc.). The "string compare" calls are filesystem path operations.

## What We Still Need to Determine (On Hardware)

1. **USART2 baud rate** -- likely 115200, 921600, or 1M. Set in boot init region not covered by decompilation. Probe with logic analyzer.
2. **SPI2 clock speed** -- determines max sample transfer rate.
3. **Chip-select polarity** -- is PB6 active-high or active-low?
4. **Full command byte mapping** -- we have 5 command codes identified. There are likely more for calibration, DVOM mode switching, etc.
5. **Sample data format** -- 8-bit or 12-bit ADC values? Signed or unsigned? Packed or one-per-byte?
6. **Trigger register layout** -- which command data bytes configure trigger level/edge/mode on the FPGA side.
7. **Calibration data format** -- references to calibration values in Case 3 processing.
8. **DVOM response format** -- multimeter readings come through USART2 but in a different frame format.

## Emulator Simulation

`emulator/renode/fpga_dvom_sim.py` implements basic response simulation:

```python
# ACK: [0x5A, 0xA5]
# Status: [0xAA, 0x55, 0x00, echo_byte, 0x00, 0x00, 0x00, 0xAA, 0x00, 0x00]
```

Enough for firmware to boot. Does not simulate real ADC data or SPI2 bulk transfers.

## Hardware Day Plan

1. Connect logic analyzer to USART2 TX/RX pins + SPI2 MOSI/MISO/CLK/CS
2. Boot original firmware, capture idle traffic to determine baud rate
3. Trigger various modes (scope, meter, siggen) and capture command sequences
4. Map command bytes to mode transitions
5. Capture SPI2 bulk data during scope acquisition to determine sample format
6. Feed captured data into our emulator simulation for validation
