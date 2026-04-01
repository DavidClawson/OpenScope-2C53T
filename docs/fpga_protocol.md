# FPGA Communication Protocol — Reverse Engineering Analysis

*Decoded from decompiled V1.2.0 firmware (decompiled_2C53T_v2.c, ~39K lines)*
*Initial analysis: 2026-03-20. Updated: 2026-03-31 with SPI3 discovery, corrected function IDs.*

## BREAKTHROUGH: FPGA Data Interface is SPI3 (0x40003C00) on PB3/PB4/PB5

**Discovery date:** 2026-03-31

The FPGA bulk data channel is **SPI3** (AT32 peripheral at `0x40003C00`), NOT SPI2 (`0x40003800`). Previous hardware probing tested SPI2 and found it connected to the Winbond SPI flash. SPI3 was never tested because it wasn't known to be in use.

### How This Was Found

Binary analysis of the V1.2.0 firmware, working backward from FreeRTOS kernel functions:

1. `FUN_0803acf0` was labeled "fpga_send_command" in earlier analysis — **actually `xQueueGenericSend`** (FreeRTOS kernel). It puts items on a queue, doesn't touch hardware.
2. `FUN_0803b09c` was labeled "usart2_rx_process" — **actually `xQueueGenericSendFromISR`**. Called from USART2 ISR and DMA-to-LCD function.
3. `FUN_0803b1d8` is the real **`xQueueReceive`** — found by identifying FreeRTOS kernel patterns (ISB/DSB barriers, BASEPRI manipulation, PendSV triggers).
4. Tracing all callers of `xQueueReceive` found **FUN_08036934** in the 11.5KB gap at 0x08036A4E–0x08039874.
5. Disassembling FUN_08036934 revealed MOVW/MOVT pairs constructing peripheral address **0x40003C08** — that's **SPI3_STAT**, not SPI2_STAT (0x40003808).

### Corrected Peripheral Map

| Peripheral | Address | Pins | Function |
|---|---|---|---|
| **SPI3** | **0x40003C00** | **PB3 (SCK), PB4 (MISO), PB5 (MOSI)** | **FPGA bulk data channel (ADC samples)** |
| USART2 | 0x40004400 | PA2 (TX), PA3 (RX) | FPGA command/status (heartbeat + commands) |
| SPI2 | 0x40003800 | PB13 (SCK), PB14 (MISO), PB15 (MOSI), PB12 (CS) | SPI flash only (Winbond W25Q128JV) |
| DMA0 CH1 | 0x4002001C | N/A | LCD framebuffer DMA (RAM → EXMC 0x60020000) |

### Peripheral Addresses in FPGA Task (FUN_08036934)

| Address | Register | Usage in FPGA Task |
|---|---|---|
| 0x40003C08 | SPI3_STAT | Polled for TX/RX ready during data transfer |
| 0x4000440C | USART2_CTL0 | Command framing control |
| 0x40010C10 | GPIOB_BOP | Chip select assertion (3 references — PB6 likely) |
| 0x40011008 | GPIOC_IDR | Input read (3 references — handshake or status) |
| 0x40010800 | GPIOA_CTL0 | Pin configuration (SPI3 alternate function setup) |
| 0x40011800 | GPIOE_CTL0 | Pin configuration |
| 0x40000400 | TMR2 | Timing/delay |

### SPI3 Pin Mapping (AT32F403A)

Default SPI3 pins (no remap):
| Signal | Pin | Notes |
|---|---|---|
| SPI3_SCK | **PB3** | Shared with JTDO/TRACESWO — requires JTAG disable (IOMUX remap) |
| SPI3_MISO | **PB4** | Shared with JNTRST |
| SPI3_MOSI | **PB5** | |
| SPI3_CS | PA15 or GPIO | GPIOB_BOP suggests PB6 used as software CS |

**Important:** PB3 and PB4 are JTAG pins by default. The firmware must disable JTAG via IOMUX/AFIO remap to use them as SPI3. This is likely done in the boot init code (gap at 0x08002BE0).

### What This Explains

1. **Why SPI2 probing found only SPI flash** — SPI2 IS connected to flash. The FPGA uses a completely separate SPI peripheral.
2. **Why USART2 probing showed only heartbeat** — USART2 handles command framing, but the data path is SPI3. Without sending valid commands first, the FPGA has nothing to send on SPI3.
3. **Why the 11.5KB gap was labeled "FPGA protocol"** — it IS the FPGA task. FUN_08036934 receives commands from FreeRTOS queues and executes them via SPI3 + USART2.
4. **Why PB3/PB4/PB5 weren't in the pin table** — they're JTAG pins by default, so they weren't suspected as SPI.

### Hardware Verification (2026-03-31 evening)

Custom `spi3test.c` firmware flashed via DFU and tested on real hardware:

**SPI3 peripheral: CONFIRMED OPERATIONAL**
- JTAG disabled via `gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE)` — SWD preserved
- SPI3 remapped via `gpio_pin_remap_config(SPI3_GMUX_0010, TRUE)` — PB3/PB4/PB5
- SPI3_CTL0 = `0x0000035C` (master, CPOL=0, CPHA=0, /16 clock, software CS, enabled)
- SPI3_STS = `0x00000002` (TXE set — peripheral alive and ready)
- All SPI3 reads return `0xFF` — FPGA not driving MISO yet

**USART2 heartbeat: CONFIRMED ON HARDWARE**
- USART2 configured at 9600 baud on PA2 (TX) / PA3 (RX)
- Captured heartbeat bytes: `5A A5 E4 2E 63 25 07 00 00 00 ...`
- Matches exactly what was seen on debug UART pads
- 11 of 12 bytes captured per frame (started mid-frame)
- Heartbeat continuously received in monitor loop (bottom of LCD)

**USART2 command test: NO RESPONSE**
- Sent 3 command frames: `AA 55 00 00...`, `AA 55 00 01...`, `AA 55 00 1C...`
- All responses were `00` — FPGA did not reply
- Command frame format may be wrong, or FPGA needs different init handshake

**SPI3 after USART2 commands: STILL ALL FF**
- SPI3 reads unchanged after sending USART2 commands
- FPGA likely needs correct init sequence before activating SPI3 data channel

**Test firmware:** `firmware/src/spi3test.c` — builds with `make -f Makefile.hwtest TEST=spi3test`

### Next Steps

1. **Disassemble FUN_08036934 fully** (11KB) to extract the exact FPGA init sequence, SPI3 clock config, and command frame format
2. **Find USART2 BRR (baud rate)** in the boot init gap — TX baud might differ from 9600 RX heartbeat
3. **Extract the boot init code** at 0x08002BE0 to find the complete peripheral setup sequence
4. **Try different command frame formats** — the `AA 55` prefix may be wrong; the decompiled TX buffer starts at 0x20000005 and the ISR sends 10 bytes
5. **Try SPI3 with FPGA command bytes** — send known FPGA commands (0x01-0x05) directly on SPI3 instead of USART2

---

## Hardware Probing Results (2026-03-31)

Previous probing eliminated these interfaces (all still valid — SPI3 was simply not tested):

| Interface | Pins | Result |
|---|---|---|
| USART2 | PA2 TX, PA3 RX | PA3 receives 9600 baud heartbeat only. Commands need proper init sequence. |
| USART1 | PA9 TX, PA10 RX | Dead — 0 bytes received |
| USART3 | PB10 TX, PB11 RX | Dead — 0 bytes received |
| SPI2 + PB12 CS | PB13/14/15 | Winbond W25Q128JV SPI flash (JEDEC: `EF 40 18`). FAT filesystem. |
| SPI2 + PB6 CS | PB13/14/15 | Same SPI flash responds |
| **SPI3** | **PB3/4/5, PB6 CS** | **TESTED: Peripheral works (STS=0x02), reads all 0xFF. FPGA not responding yet — needs init sequence.** |
| USB | PA11/PA12 | Default reset state |
| EXMC NE1 | PD/PE data bus | LCD only (ST7789V) |

### Debug UART Heartbeat

The FPGA continuously outputs 12-byte frames at 9600 baud on both the debug UART pads AND PA3 (USART2 RX):
```
5A A5 E4 2E 63 25 07 00 00 00 00 Fx   (x = rolling counter 0-3)
```
- Bytes 2-10 (`E4 2E 63 25 07 00 00 00 00`) are constant regardless of button presses or commands sent
- Byte 11 cycles F0→F1→F2→F3→F0... (counter changed to EE/EF range when custom firmware runs)
- This is a passive heartbeat — likely FPGA idle output before command init sequence

### SPI Flash Contents

SPI2 with PB12 chip select reads from Winbond W25Q128JV (16MB):
- Address 0x000000: `EB 3C 90 4D 53 44 4F 53` = FAT filesystem boot sector ("MSDOS")
- Contains UI asset images, system files, and unit format strings (paths like `3:/System file/`)

---

## Architecture Overview

The FNIRSI 2C53T uses a **dual-interface** model between the AT32F403A MCU and the Gowin GW1N-UV2 FPGA:

```
MCU (AT32F403A)                          FPGA (Gowin GW1N-UV2)
┌─────────────┐                          ┌──────────────────┐
│             │── USART2 PA2/PA3 ──────>│                  │
│ fpga_task   │   (cmd/ctrl 9600bd)     │  ADC controller  │
│ FUN_08036934│<── USART2 (response) ───│  Trigger engine   │
│             │                          │  Sample memory    │
│             │<── SPI3 PB3/4/5 ───────│  DVOM mux         │
│             │   (bulk data, high-spd) │                  │
│             │── GPIOB (CS, PB6?) ────>│                  │
└─────────────┘                          └──────────────────┘
```

**USART2** (0x40004400, PA2 TX / PA3 RX): Command/control channel. 10-byte frames MCU->FPGA, 2/10/12-byte responses FPGA->MCU.

**SPI3** (0x40003C00, PB3 SCK / PB4 MISO / PB5 MOSI): High-speed bulk data channel. MCU reads ADC sample buffers from FPGA. Chip select via GPIOB (likely PB6).

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

## Multimeter FPGA Commands (from P1 analysis)

The multimeter modes send FPGA commands via `fpga_send_command` (FUN_0803acf0). Each meter sub-mode has its own initialization sequence:

| Command | Purpose | Source Function |
|---------|---------|-----------------|
| 0x07 | Meter mode select (positive range) | FUN_0800ba06, bb10, bc98 |
| 0x0A | Meter mode select (negative range) | FUN_0800ba06, bb10, bc98 |
| 0x16 | DC meter config 1 | FUN_0800bb10 |
| 0x17 | DC meter config 2 | FUN_0800bb10 |
| 0x18 | DC meter config 3 | FUN_0800bb10 |
| 0x19 | DC meter config 4 (final, triggers measurement) | FUN_0800bb10 |
| 0x1A | AC meter config 1 | FUN_0800ba06 |
| 0x1B | AC meter config 2 | FUN_0800ba06 |
| 0x1C | AC meter config 3 | FUN_0800ba06 |
| 0x1D | AC meter config 4 | FUN_0800ba06 |
| 0x1E | AC meter config 5 (final, triggers measurement) | FUN_0800ba06 |
| 0x20 | Component tester config 1 | FUN_0800bba6 |
| 0x21 | Component tester config 2 (final) | FUN_0800bba6 |
| 0x26 | Unknown meter sub-mode config 1 | FUN_0800bc00 |
| 0x27 | Unknown meter sub-mode config 2 | FUN_0800bc00 |
| 0x28 | Unknown meter sub-mode config 3 (final) | FUN_0800bc00 |

Command 0x07 vs 0x0A is selected based on a sign bit: `if (*param_1 << 24 >= 0)` selects 0x0A, else 0x07.

The meter mode dispatch table lives at **0x0804C0CC** in flash — function pointers indexed by sub-mode number.

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

## Logic Analyzer Setup

**Hardware:** HiLetgo 24MHz 8CH USB Logic Analyzer (Saleae Logic clone, fx2lafw driver)
**Software:** `sigrok-cli` (installed via Homebrew)

### Wiring Plan

| Analyzer Ch | Target Signal | MCU Pin | Notes |
|-------------|---------------|---------|-------|
| D0 | USART2 TX | PB10 (AF) | MCU → FPGA commands |
| D1 | USART2 RX | PB11 (AF) | FPGA → MCU responses |
| D2 | SPI2 CLK | PB13 | Bulk data clock |
| D3 | SPI2 MISO | PB14 | FPGA → MCU sample data |
| D4 | SPI2 MOSI | PB15 | MCU → FPGA SPI commands |
| D5 | SPI2 CS | PB6 (GPIO) | Active polarity TBD |
| D6 | (spare) | | |
| D7 | (spare) | | |

### Capture Commands

```bash
# Step 1: Determine USART2 baud rate — capture idle traffic at max rate
sigrok-cli -d fx2lafw --config samplerate=24M --samples 2000000 \
  -o captures/fpga_idle.sr

# Step 2: Decode UART at candidate baud rates
sigrok-cli -i captures/fpga_idle.sr \
  -P uart:baudrate=115200:rx=D0:tx=D1 -A uart

sigrok-cli -i captures/fpga_idle.sr \
  -P uart:baudrate=921600:rx=D0:tx=D1 -A uart

sigrok-cli -i captures/fpga_idle.sr \
  -P uart:baudrate=1000000:rx=D0:tx=D1 -A uart

# Step 3: Capture mode transitions (trigger manually while recording)
sigrok-cli -d fx2lafw --config samplerate=12M --time 10s \
  -o captures/fpga_mode_switch.sr

# Step 4: Decode SPI2 bulk data
sigrok-cli -i captures/fpga_mode_switch.sr \
  -P spi:clk=D2:miso=D3:mosi=D4:cs=D5 -A spi

# Step 5: Stacked decode — UART frames + SPI together
sigrok-cli -d fx2lafw --config samplerate=12M --time 5s \
  -P uart:baudrate=115200:rx=D0:tx=D1,spi:clk=D2:miso=D3:mosi=D4:cs=D5 \
  -o captures/fpga_full_trace.sr
```

### Probing Procedure

1. Connect logic analyzer to USART2 TX/RX pins + SPI2 MOSI/MISO/CLK/CS
2. Boot original firmware, capture idle traffic to determine baud rate
3. Trigger various modes (scope, meter, siggen) and capture command sequences
4. Map command bytes to mode transitions
5. Capture SPI2 bulk data during scope acquisition to determine sample format
6. Feed captured data into our emulator simulation for validation
