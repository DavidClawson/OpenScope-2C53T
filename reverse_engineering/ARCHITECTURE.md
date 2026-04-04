# FNIRSI 2C53T Firmware Architecture Guide

*Read this first. This document explains how the entire device works -- hardware and firmware together -- based on comprehensive reverse engineering of the V1.2.0 stock firmware.*

---

## 1. Device Overview

The FNIRSI 2C53T is a handheld 3-in-1 test instrument: a 2-channel 250MS/s digital oscilloscope, a multimeter (AC/DC voltage, resistance, continuity, capacitance, diode), and a signal generator. It has a 320x240 color LCD, 15 physical buttons, a rechargeable battery, and USB-C for charging and firmware updates.

The device runs on an ARM Cortex-M4F MCU (Artery AT32F403A, markings sanded off and originally identified as a GigaDevice GD32F307). A Gowin GW1N-UV2 FPGA handles high-speed ADC sampling and analog front-end control. The MCU and FPGA communicate over two independent channels: a slow USART link for commands and a fast SPI link for bulk ADC data.

The stock firmware is a single monolithic binary (~734 KB) running FreeRTOS with 8 tasks. It was decompiled from the raw flash image using Ghidra. Of the 309 real functions in the binary, ~95% have been named and categorized.

**Board revision:** 2C53T-V1.4

---

## 2. Hardware Block Diagram

```
                              +------------------+
                              |   Gowin GW1N-UV2 |
                  +-----------+      FPGA        +-----------+
                  |           | (non-volatile,    |           |
                  |           |  retains config)  |           |
                  |           +--------+---------+           |
                  |                    |                      |
                  |              250 MS/s ADC                 |
                  |              (2 channels)                 |
                  |                                           |
          SPI3 (60 MHz)                            USART2 (9600 baud)
          PB3/4/5 + PB6 CS                         PA2 TX, PA3 RX
          Bulk ADC data                            10B cmd / 12B resp
                  |                                           |
                  |           +------------------+            |
                  +-----------+  AT32F403A MCU   +------------+
                              |  Cortex-M4F      |
                              |  240 MHz, 1MB    |
                              |  flash, 224KB    |
                              |  SRAM (EOPB0)    |
                              +--+--+--+--+--+---+
                                 |  |  |  |  |
              +------------------+  |  |  |  +------------------+
              |                     |  |  |                     |
     EXMC 16-bit parallel     SPI2  | USB |  GPIO              DAC
     0x6001FFFE / 0x60020000  PB12-15  PA11/12  (buttons,     (2-ch 12-bit)
              |                |    |     |   MUX, relays)      |
              v                v    |     v                     v
        +----------+   +----------+|  +-----+          +------------+
        | ST7789V  |   | W25Q128  || | USB  |          | Signal Gen |
        | 320x240  |   | 16MB SPI || | DFU  |          | Output     |
        | LCD      |   | Flash    || +------+          +------------+
        +----------+   +----------+|
                                   |
                              +---------+
                              | Battery |
                              | Monitor |
                              | (ADC)   |
                              +---------+
```

### Bus Summary

| Bus | Pins | Speed | Purpose |
|-----|------|-------|---------|
| SPI3 | PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS | 60 MHz (Mode 3) | FPGA ADC sample data |
| USART2 | PA2 TX, PA3 RX | 9600 baud, 8N1 | FPGA command/response |
| EXMC | PD0-15 (data), A17 = RS | ~30 MHz | LCD parallel interface |
| SPI2 | PB12 CS, PB13 CLK, PB14 MISO, PB15 MOSI | ~30 MHz | SPI flash (W25Q128JV) |
| USB | PA11 D-, PA12 D+ | Full-speed | DFU firmware update |
| DAC | Internal | -- | 2-channel 12-bit signal generator output |
| GPIO | Various | -- | Buttons, analog MUX, relays, power, backlight |

### Key Control Signals

| Pin | Function | Notes |
|-----|----------|-------|
| PC6 | FPGA SPI enable | Must be HIGH for SPI3 to work |
| PB11 | FPGA active mode | Must be HIGH during measurement |
| PC9 | Power hold | Must be HIGH immediately at boot or device dies |
| PB8 | LCD backlight | HIGH to enable |

---

## 3. Pin Map

Organized by function. Every known pin assignment from decompilation and hardware probing.

### FPGA Interface

| Pin | Function | Direction | Config |
|-----|----------|-----------|--------|
| PB3 | SPI3_SCK | MCU out | AF push-pull 50MHz |
| PB4 | SPI3_MISO | MCU in | Input floating |
| PB5 | SPI3_MOSI | MCU out | AF push-pull 50MHz |
| PB6 | SPI3_CS | MCU out | GPIO push-pull, active LOW |
| PC6 | SPI enable | MCU out | GPIO push-pull, HIGH = enabled |
| PB11 | Active mode | MCU out | GPIO push-pull, HIGH = active |
| PA2 | USART2_TX | MCU out | FPGA command channel |
| PA3 | USART2_RX | MCU in | FPGA response channel |

### LCD (EXMC Parallel)

| Pin | Function | Notes |
|-----|----------|-------|
| PD0-PD15 | 16-bit data bus | EXMC bank 0 (NE1) |
| PD? (A17) | RS / DCX select | Address bit selects cmd vs data |
| PB8 | Backlight enable | GPIO, HIGH = on |

LCD registers: command at `0x6001FFFE`, data at `0x60020000`. EXMC config: SNCTL0=0x5011, SNTCFG0=0x02020424.

### SPI Flash

| Pin | Function |
|-----|----------|
| PB12 | SPI2_CS (active LOW) |
| PB13 | SPI2_CLK |
| PB14 | SPI2_MISO |
| PB15 | SPI2_MOSI |

Chip: Winbond W25Q128JV (JEDEC: EF 40 18), 16MB.

### Analog Front End

| Pin | Function | Notes |
|-----|----------|-------|
| PA15 | Analog MUX select A | gpio_mux_porta_portb |
| PB10 | Analog MUX select B | gpio_mux_porta_portb |
| PC12 | Relay control A | gpio_mux_portc_porte |
| PE4 | Relay control B | gpio_mux_portc_porte |
| PE5 | Relay control C | gpio_mux_portc_porte |
| PE6 | Relay control D | gpio_mux_portc_porte |
| PE12 | Relay control E | gpio_mux_portc_porte |
| PD13 | Signal routing | siggen_configure |

### Button Input (3-Group Multiplexed Scan)

| Group | Source | Buttons |
|-------|--------|---------|
| 1 (GPIOC) | PC7, PC8, other | CH1/CH2 probe change, probe type A/B |
| 2 (PB8-derived) | PB-relative | Button groups 1-3, special button |
| 3 (GPIOC IDR bit 10) | PC10, etc. | Trigger, Select, Menu, OK |

Additional confirmed direct buttons: PRM (PB7), CH2 (PA7+PE3 matrix), Down (PC5+PE3), Right (PA8+PE2), Auto (PC10+PE2), Save (PB0+PE3).

### Power & System

| Pin | Function |
|-----|----------|
| PC9 | Power hold (must be HIGH) |
| PA13 | SWDIO (debug) |
| PA14 | SWCLK (debug) |
| PA11 | USB D- |
| PA12 | USB D+ |

### JTAG Remap

PB3, PB4, PB5 are JTAG pins by default. The firmware sets `AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000` to disable JTAG-DP (keeping SWD) and free these pins for SPI3.

---

## 4. Firmware Task Architecture

The stock firmware runs FreeRTOS with 8 tasks and 7 queues/semaphores.

### Task Table

| Task Name | Address | Priority | Stack | Purpose |
|-----------|---------|----------|-------|---------|
| `display` | 0x0803DA51 | 1 (lowest) | 1536B | LCD rendering, waveform drawing, UI refresh |
| `key` | 0x08040009 | 4 | 512B | Button event processing, mode dispatch |
| `osc` | 0x0804009D | 2 | 1024B | Oscilloscope state machine (13.3KB FSM) |
| `fpga` | 0x0803E455 | 3 | 512B | SPI3 acquisition engine (9 modes) |
| `dvom_TX` | 0x0803E3F5 | 2 | 256B | USART TX frame builder (to FPGA) |
| `dvom_RX` | 0x0803DAC1 | 3 | 512B | USART RX frame processing |
| `Timer1` | 0x080400B9 | 10 | 40B | FreeRTOS software timer |
| `Timer2` | 0x080406C9 | 1000 (highest) | 4000B | FreeRTOS software timer (high-priority) |

### Queue and Semaphore Table

| RAM Address | Type | Item Size | Depth | Purpose |
|-------------|------|-----------|-------|---------|
| 0x20002D6C | Queue | 1 byte | 20 | USART command dispatch (mode commands) |
| 0x20002D70 | Queue | 1 byte | 15 | Button events from input scanning |
| 0x20002D74 | Queue | 2 bytes | 10 | Commands to send to FPGA via USART |
| 0x20002D78 | Queue | 1 byte | 15 | Triggers for SPI3 acquisition |
| 0x20002D7C | Semaphore | binary | 1 | Meter USART RX frame complete |
| 0x20002D80 | Semaphore | binary | 1 | SPI3 init synchronization |
| 0x20002D84 | Semaphore | binary | 1 | EXTI notification |

### Task Interaction Diagram

```
                                                 TMR3 ISR (periodic)
                                                       |
                      +--------------------------------+------------------------------+
                      |                                |                              |
                      v                                v                              v
              usart2_irq_handler              input_and_housekeeping          button debounce
                      |                                |                              |
           +----------+----------+                     v                              v
           |                     |              spi3_data_queue              button_event_queue
           v                     v              (0x20002D78)                (0x20002D70)
    usart_cmd_queue       meter_semaphore              |                              |
    (0x20002D6C)          (0x20002D7C)                 v                              v
           |                     |            spi3_acquisition_task              key task
           v                     v            (fpga task, 9 modes)         (mode dispatch)
    usart_cmd_dispatcher   meter_data_processor        |                              |
           |                     |                     v                              v
           v                     v              ADC sample buffers          usart_cmd_queue
    USART_CMD_DISPATCH     meter_mode_handler   ms+0x5B0 (CH1)            (0x20002D6C)
    TABLE[n]()             (8-state FSM)        ms+0x9B0 (CH2)
           |                                           |
           v                                           v
    usart_tx_config_writer                      display task
           |                                    (LCD rendering)
           v
    usart_tx_queue (0x20002D74)
           |
           v
    usart_tx_frame_builder (dvom_TX task)
           |
           v
    USART2 TX --> FPGA
```

---

## 5. Data Flow: ADC Sample to Screen Pixel

This is the core data path of the oscilloscope. From analog input to pixel on the LCD.

### Step 1: FPGA Samples the ADC

The Gowin FPGA controls the 250MS/s ADC. It continuously digitizes both channels into 8-bit unsigned values (0-255). The FPGA stores samples in its internal buffer and waits for the MCU to read them.

### Step 2: TMR3 ISR Triggers Acquisition

TMR3 fires periodically. The ISR (`tmr3_isr` at 0x0802771C) calls `input_and_housekeeping`. When the acquisition counter reaches the timebase threshold, TWO items are sent to the SPI3 queue (double-buffered ping-pong to prevent tearing).

### Step 3: SPI3 Transfer

The `spi3_acquisition_task` (7164 bytes, the largest sub-function) wakes on the queue event. It selects one of 9 acquisition modes based on the current `spi3_transfer_mode`:

```
Mode 0: FAST TIMEBASE  -- Send timebase config byte only, no data read
Mode 1: ROLL MODE      -- 5-byte transfer, circular buffer (300 samples)
Mode 2: NORMAL SCOPE   -- 1024 bytes, interleaved CH1/CH2 (512 pairs)
Mode 3: DUAL CHANNEL   -- 2048 bytes, split into CH1 and CH2 buffers
Mode 4: EXTENDED CMD   -- Command-only, no data
Mode 5: METER ADC      -- Read active meter channel
Mode 6: SIGGEN FEEDBACK-- Signal generator readback
Mode 7: CALIBRATION    -- 16-bit calibration value readback
Mode 8: SELF TEST      -- Verification mode
```

The SPI3 transfer protocol:

```
MCU                              FPGA
 |  CS ASSERT (PB6 LOW)          |
 |  --> command byte              |
 |  <-- status byte (full duplex) |
 |  --> 0xFF (clock out)          |
 |  <-- ADC data byte 1           |
 |  --> 0xFF                      |
 |  <-- ADC data byte 2           |
 |     ... (1024 or 2048 bytes)   |
 |  CS DEASSERT (PB6 HIGH)       |
```

ADC data is interleaved: `[CH1[0], CH2[0], CH1[1], CH2[1], ...]`

### Step 4: VFP Calibration Pipeline

Every raw sample passes through an ARM VFP (hardware floating-point) calibration pipeline. The VFP registers are loaded once at task entry and stay resident:

```c
// VFP registers (persistent across samples)
s16 = -28.0f     // ADC zero offset (hardware-specific)
s18 = gain_ch1   // Calibration gain
s20 = offset_ch2 // Calibration offset
s22 = dc_bias    // DC bias correction
s24 = 255.0f     // Maximum clamp
s26 = 0.0f       // Minimum clamp
s28 = divisor    // Range divisor

// Per-sample calibration
float raw_f   = (float)(uint8_t)raw_sample;
float norm    = (raw_f + s16) / s28;                          // Normalize
int8_t dc_off = meter_state[0x04];                            // Per-channel DC offset
float range   = (float)((int16_t)voltage_range - dc_off);
float result  = norm * range + (float)dc_off + s22;           // Scale to display
result = clamp(result, 0.0f, 255.0f);
calibrated = (uint8_t)(int)result;
```

The critical constant is the ADC offset of -28.0. The FPGA ADC has ~28 LSBs of DC offset that must be subtracted from every sample.

### Step 5: Buffer Storage

Calibrated samples are written to the measurement state structure in SRAM:

| Buffer | Address | Size | Use |
|--------|---------|------|-----|
| CH1 normal | ms + 0x5B0 | 512 bytes | Normal/dual mode CH1 |
| CH2 normal | ms + 0x9B0 | 512 bytes | Normal/dual mode CH2 |
| CH1 roll | ms + 0x356 | 300 bytes | Roll mode circular buffer |
| CH2 roll | ms + 0x483 | 300 bytes | Roll mode circular buffer |

Roll mode uses a shift-and-append algorithm: all existing samples move down by 1 position before the new sample is added. Expensive but maintains display continuity.

### Step 6: Display Rendering

The display task reads the sample buffers and renders waveforms to the LCD via the EXMC parallel interface. The rendering engine (`display_render_engine`, 2.6KB) handles text layout, glyph rendering, and waveform drawing with Bresenham line algorithms.

LCD writes go to memory-mapped addresses:
- Command: `*(volatile uint16_t*)0x6001FFFE = cmd;`
- Data: `*(volatile uint16_t*)0x60020000 = pixel_rgb565;`

---

## 6. Data Flow: Button Press to Action

### Step 1: GPIO Scanning

The `input_and_housekeeping` function (1342 bytes) is called from the TMR3 ISR. It reads three groups of GPIO pins:

```
Group 1: GPIOC IDR -- probe changes, probe type (4 signals)
Group 2: GPIOB IDR -- button matrix groups (4 signals)
Group 3: GPIOC IDR bit 10 -- trigger, select, menu, OK (4 signals)
```

The 15 buttons are encoded as a 16-bit bitmask with multiplexed scanning.

### Step 2: Debounce (70/72-tick)

Each button has an 8-bit counter:

```
PRESSED:
  counter++ (saturate at 0xFF)
  if counter == 0x46 (70):  emit SHORT PRESS event
  if counter == 0x48 (72):  emit LONG PRESS event, hold at 0x47

RELEASED:
  if 2 <= counter <= 0x45:  emit RELEASE event
  counter = 0
```

The button map lookup table at `0x08046528` translates physical bit positions to logical button IDs. Short press uses `button_map[btn + 0x0F]`, long press uses `button_map[btn]`.

### Step 3: Queue to Key Task

Debounced button events (1 byte each) are sent to `button_event_queue` (0x20002D70, depth 15).

### Step 4: Key Task Dispatch

The `key` task (priority 4, highest non-timer) receives button events and dispatches them to the current mode handler. The mode dispatch table at `0x0804C0CC` selects the handler based on the active mode (oscilloscope, multimeter, signal generator, etc.).

The oscilloscope mode handler alone (`scope_main_fsm` at 0x08019E98) is 13.3 KB with 27 callees -- it is the single largest function in the firmware.

---

## 7. Data Flow: USART Command to FPGA Response

The USART2 link is the command/control channel between the MCU and FPGA. It runs at 9600 baud -- slow by design, as it only carries configuration commands and meter readings, not bulk ADC data.

### Step 1: Command Enqueue

When a mode handler needs to configure the FPGA (change timebase, voltage range, trigger level, etc.), it calls `usart_tx_config_writer` (316 bytes). This function encodes the command into a 2-byte item and sends it to `usart_tx_queue` (0x20002D74, 2-byte items, depth 10).

There are 7 config writer types:

| Type | Purpose | Parameters |
|------|---------|------------|
| 0 | Scope CH1 config | Coupling, bandwidth, voltage range |
| 1 | Scope CH2 config | Coupling, bandwidth, voltage range |
| 2 | Trigger config | Edge, source, threshold |
| 3 | Timebase config | Prescaler, period |
| 4 | Meter range | Range code |
| 5 | Siggen frequency | Frequency value |
| 6 | Siggen waveform | Waveform type |

### Step 2: TX Frame Builder

The `dvom_TX` task (priority 2, 256B stack) runs `usart_tx_frame_builder` (96 bytes). It blocks on `usart_tx_queue`, receives the 2-byte command, and formats a 10-byte USART frame:

```
Byte  [0]: Header byte 1
Byte  [1]: Header byte 2
Byte  [2]: Command high byte
Byte  [3]: Command low byte
Bytes [4-8]: Parameters (typically 0x00)
Byte  [9]: Checksum = (byte[2] + byte[3]) & 0xFF
```

The frame is written to the TX buffer at `0x20000005` and the USART TX interrupt is enabled.

### Step 3: USART2 Transmission

The USART2 ISR (`usart2_isr` at 0x080277B4) transmits the frame byte-by-byte using TX-empty interrupts. After the last byte, it disables the TX interrupt.

### Step 4: FPGA Response

The FPGA responds with one of two frame types:

**Data frame (12 bytes):** `5A A5` header + 10 data bytes. Contains meter readings, ADC status, or button state. Triggers a queue send + PendSV for immediate context switch.

**Echo frame (10 bytes):** `AA 55` header + 8 bytes. Command acknowledgment. Validated by checking `rx[3] == tx[3]` and `rx[7] == 0xAA`.

### Step 5: RX Processing

On receiving a valid data frame, the ISR signals `meter_semaphore` (0x20002D7C) and sends the command byte to `usart_cmd_queue` (0x20002D6C).

The `usart_cmd_dispatcher` task (108 bytes) blocks on the command queue and dispatches through the function pointer table at `0x0804BE74`.

### Step 6: Timer-Driven Exchange

The entire USART exchange is driven by TMR3, NOT by USART interrupts alone. TMR3 fires periodically and its ISR orchestrates: (1) pump TX bytes, (2) check for complete RX frames, (3) trigger button scanning, (4) manage acquisition timing.

---

## 8. Mode Switching

The device has three primary modes plus sub-modes:

### Mode Dispatch Architecture

```
                    mode_dispatch_indirect (0x0800BCD4)
                              |
                    Jump table at 0x0804C0CC
                              |
         +--------------------+--------------------+
         |                    |                    |
    Mode 0: SCOPE       Mode 1: METER       Mode 2: SIGGEN
    scope_main_fsm       meter handlers      siggen_configure
    (13.3 KB, 27 calls)  (8-state FSM)      (1.6 KB)
         |
         +-- Normal (2-ch waveform)
         +-- FFT
         +-- XY mode
         +-- Roll/streaming
         +-- Single shot
```

### FPGA Command Mode Map

Each device mode sends a specific sequence of FPGA commands via the dispatch table at `0x0804BE74`:

| Mode ID | Device Mode | FPGA Commands |
|---------|-------------|---------------|
| 0 | Oscilloscope | 0x00, 0x01, 0x0B-0x11 |
| 1 | Multimeter (basic) | 0x00, 0x09, 0x07/0x0A, 0x1A-0x1E |
| 2 | Signal generator | 0x02-0x06, 0x08 |
| 3 | Multimeter (extended) | 0x00, 0x08-0x0A, 0x16-0x19 |
| 4-8 | Internal modes | Various specialized commands |
| 9 | Multimeter variant | 0x00, 0x12-0x14, 0x09, 0x07/0x0A |

### Mode Transition

When the user selects a new mode, the key task:
1. Sends the mode-specific FPGA init sequence via `usart_cmd_queue`
2. Sets the `spi3_transfer_mode` variable to select the appropriate acquisition mode
3. Updates the display task's render mode
4. Reconfigures analog MUX relays via `gpio_mux_porta_portb` and `gpio_mux_portc_porte`

---

## 9. Meter Data Pipeline

The multimeter data path is distinct from the oscilloscope path. Meter readings come through USART, not SPI3.

### Step 1: BCD Extraction

USART RX frame bytes [2..6] contain packed BCD digits in a cross-byte nibble format:

```
digit0 = lookup( (rx[2] & 0xF0) | (rx[3] & 0x0F) )
digit1 = lookup( (rx[3] & 0xF0) | (rx[4] & 0x0F) )
digit2 = lookup( (rx[4] & 0xF0) | (rx[5] & 0x0F) )
digit3 = lookup( (rx[5] & 0xF0) | (rx[6] & 0x0F) )
```

### Step 2: Special Code Detection

| Pattern | Meaning |
|---------|---------|
| digit0=0x0A, digit1=0x0B | "OL" -- Overload |
| digit0=0x10, digit1=0x10 | Blank display |
| digit1=0x12, digit2=0x0A, digit3=5 | Continuity (buzzer) |
| digit1=0x13, digit2=0x14 | Mode change indicator |
| Any digit = 0xFF | Invalid/unrecognized |

### Step 3: Double-Precision Calibration

The `meter_data_processor` (1776 bytes) applies double-precision floating-point calibration to the BCD value. This is notably more precise than the scope's single-precision VFP pipeline.

### Step 4: 8-State Mode FSM

The `meter_mode_handler` (504 bytes) runs an 8-state finite state machine:

```
State 0 (IDLE)       -- Check rx[7] bits for AC, auto-range, overload
State 1 (POLARITY)   -- rx[7] bit 0 sets calibration coefficient
State 2 (OVERLOAD)   -- Overload flag or range change detection
State 3 (AC/DC)      -- rx[7] bit 2 = AC flag, rx[6] bit 6 = hold
State 4 (RANGE)      -- rx[6] bits 4-5 = range, rx[7] bit 0 = polarity
State 5 (AUTO-RANGE) -- Auto-range flag sets cal coefficient 0 or 4
State 6 (STANDBY A)  -- rx[6] bit 4 selects cal coefficient
State 7 (STANDBY B)  -- Same
```

### Step 5: Display

Meter UI rendering uses large-digit fonts (`meter_ui_draw_value`, 530 bytes) with a bargraph visualization (`meter_ui_draw_bargraph`, 1798 bytes with Bresenham line drawing). Unit suffixes and format strings are stored in SPI flash, not MCU flash.

### Continuity Buzzer

The EXTI3 ISR (0x08009C10) reads the buzzer state variable at `ms+0xF5D` (values 0xB0/0xB1) to drive the piezo buzzer when continuity is detected.

---

## 10. Power Management

### Boot Sequence (Critical)

The very first operation in main() must be:

```c
GPIOC_BOP = (1 << 9);   // PC9 HIGH = power hold
```

If this is not done within a few milliseconds of reset, the hardware power latch releases and the device shuts off. The stock firmware sets this in the master init function at 0x08023A50.

### Backlight

PB8 controls the LCD backlight. Set HIGH to enable. The display task manages backlight dimming as part of auto power-off.

### Auto Power-Off (3 Tiers)

The `probe_change_handler` (108 bytes) implements tiered auto-shutdown based on probe state:

| Condition | Timeout |
|-----------|---------|
| Probe disconnected | 15 minutes (900 ticks) |
| Probe type change | 30 minutes (1800 ticks) |
| Full probe swap | 60 minutes (3600 ticks) |

### Watchdog

The IWDG (Independent Watchdog) is initialized with approximately a 5-second timeout. The `input_and_housekeeping` function feeds the watchdog every 11 calls, which at the TMR3 call rate gives approximately 50ms between feeds. If the firmware hangs, the watchdog resets the MCU within 5 seconds.

### Battery Monitoring

Battery voltage is read via an ADC channel. The specific ADC channel has not yet been identified in the decompilation (it is configured somewhere in the 15.4KB master init function).

---

## 11. Memory Map

### Global State Structure (0x200000F8, ~4 KB)

The firmware's entire runtime state lives in a single ~4KB structure at RAM address 0x200000F8. Every scope, meter, and siggen function accesses it — 71+ functions total. Register `r9` or `sl` typically holds the base pointer.

**See `analysis_v120/STATE_STRUCTURE.md` for the complete field-by-field decode.**

Key regions within the structure:
- **+0x00 – 0x3F**: Core config (channels, trigger, voltage range, timebase, mode)
- **+0x50 – 0x230**: Cursor data array (120 entries, scope_mode_cursor exclusive)
- **+0x260 – 0x33C**: Calibration tables (12 gain/offset pairs from SPI flash)
- **+0x356 – 0x5AF**: Roll mode circular buffers (301 bytes × 2 channels)
- **+0x5B0 – 0xDAF**: ADC sample buffers (1024 bytes × 2 channels)
- **+0xDB0 – 0xE0F**: Acquisition runtime (counters, busy flags, roll pointers)
- **+0xF2D – 0xF40**: Meter operating state (independent from scope)
- **+0xF78 – 0xF88**: Waveform viewport rectangle (X, Y, W, H — 40+ refs each)

### Flash Layout (0x08000000, 734 KB total)

```
0x08000000 +----------------------------+
           | Vector table (256 bytes)   |
0x08000100 +----------------------------+
           | Code region                |
           | 309 functions              |
           | 252 KB                     |
           |                            |
           | Key sections:              |
           |   0x08000238  C runtime    |
           |   0x080018A4  Analog MUX   |
           |   0x08008154  Display eng  |
           |   0x08015CFC  Scope UI     |
           |   0x08019E98  Scope FSM    |  <-- 13.3 KB, largest function
           |   0x08023A50  system_init  |  <-- 15.4 KB, master init
           |   0x080277B4  USART2 ISR   |
           |   0x08028314  File I/O     |
           |   0x0802CE94  SPI flash    |
           |   0x08030524  Waveform     |
           |   0x08034828  FreeRTOS     |
           |   0x08036934  FPGA task    |  <-- 10 sub-functions, 11.6 KB
           |   0x0803C046  Soft-float   |
0x0803F268 +----------------------------+
           | C runtime library          |
           | (printf, FP math)          |
           | ~26 KB                     |
0x08045A00 +----------------------------+
           | Data region                |
           | ~460 KB                    |
           |                            |
           |   UI images, icons         |
           |   Bitmap fonts             |
           |   Multilingual strings     |
           |   (DE, ES, PT, EN)         |
           |   Lookup tables            |
           |   Calibration defaults     |
           |                            |
           | Key tables:                |
           |   0x08046528  Button map   |
           |   0x0804BE74  Cmd dispatch |
           |   0x0804C0CC  Mode dispatch|
           |   0x0804C3B4  CH1 strings  |
           |   0x0804CA24  Font data ptr|
0x080B7680 +----------------------------+
```

### SRAM Layout (0x20000000, 224 KB with EOPB0=0xFE)

```
0x20000000 +----------------------------+
           | USART buffers              |
           |   0x20000005  TX buf (10B) |
           |   0x2000000F  RX state     |
0x20000018 +----------------------------+
           | SPI flash I/O buffers      |
0x200000F8 +----------------------------+
           | meter_state (ms) structure |  <-- THE central data structure
           |   ms+0x00   Mode/status    |
           |   ms+0x04   DC offset      |
           |   ms+0x0E   Voltage range  |
           |   ms+0x10   Timebase       |
           |   ms+0x14   Trigger config |
           |   ms+0x18   CH1 config     |
           |   ms+0x20   Config bits    |
           |   ms+0x25   Channel select |
           |   ms+0x46   Cal readback   |
           |   ms+0x356  CH1 roll buf   |  (300 bytes)
           |   ms+0x483  CH2 roll buf   |  (300 bytes)
           |   ms+0x5B0  CH1 sample buf |  (512+ bytes)
           |   ms+0x9B0  CH2 sample buf |  (512+ bytes)
           |   ms+0xF39  Auto-range flag|
           |   ms+0xF5D  Buzzer state   |
0x20001100 +----------------------------+ (approximate)
           | Calibration tables         |
           |   0x20000358-0x20000434    |
           |   6 gain/offset pairs      |
           |   Loaded from SPI flash    |
0x20002B00 +----------------------------+
           | Timer/SysTick reload vals  |
           |   0x20002B1C, 0x20002B20   |
0x20002D6C +----------------------------+
           | FreeRTOS queue handles     |
           |   7 queue/semaphore ptrs   |
           |   (0x20002D6C-0x20002D84)  |
0x20002D88 +----------------------------+
           | FreeRTOS heap              |
           | Task stacks               |
           | (managed by heap_4.c)     |
           |                            |
0x20006000 +----------------------------+
           | FreeRTOS kernel state      |
           |   0x200062F0  pxCurrentTCB |
           |                            |
0x20038000 +----------------------------+ (approximate end, 224KB)
```

### Key RAM Addresses (Quick Reference)

| Address | Size | Name | Purpose |
|---------|------|------|---------|
| 0x200000F8 | ~4 KB | `meter_state` | Central measurement/config structure |
| 0x20000005 | 10B | USART TX buffer | Outgoing FPGA command frame |
| 0x20000358 | 20B | Cal table CH1 | Gain/offset pairs |
| 0x2000036C | 20B | Cal table CH2 | Gain/offset pairs |
| 0x20002D6C | 4B | usart_cmd_queue handle | FreeRTOS queue pointer |
| 0x20002D70 | 4B | button_event_queue handle | FreeRTOS queue pointer |
| 0x20002D74 | 4B | usart_tx_queue handle | FreeRTOS queue pointer |
| 0x20002D78 | 4B | spi3_data_queue handle | FreeRTOS queue pointer |
| 0x200062F0 | 4B | pxCurrentTCB | FreeRTOS current task pointer |

---

## 12. File List

Everything in the `reverse_engineering/` directory.

### Top-Level Files

| File | Description |
|------|-------------|
| `ARCHITECTURE.md` | This document |
| `COVERAGE.md` | RE coverage tracker: 309 functions, subsystem status, what is left |
| `decompiled_2C53T.c` | V1.2.0 decompilation (~35K lines, 292 functions) |
| `decompiled_2C53T_v2.c` | Updated with named functions (~39K lines) |
| `strings_with_addresses.txt` | 290 strings mapped to firmware addresses |

### analysis_v120/ Directory (Latest Analysis)

| File | Size | Description |
|------|------|-------------|
| `ANALYSIS_SUMMARY.md` | -- | Key findings: button input, GPIO map, interrupt table |
| `FPGA_BOOT_SEQUENCE.md` | -- | 53-step boot timeline from power-on to first SPI3 data |
| `FPGA_PROTOCOL.md` | -- | USART2 frame format, ~30 command codes by mode |
| `FPGA_TASK_ANALYSIS.md` | -- | Complete FPGA task: 10 sub-functions, queues, SPI3 format, ADC calibration, 9-mode state machine, button input, meter data |
| `fpga_task_annotated.c` | 580+ lines | Annotated C for all 10 FPGA sub-functions |
| `fpga_task_decompile.txt` | 5701 lines | Raw Ghidra output for FPGA task |
| `init_function_decompile.txt` | 6391 lines | Full master init pseudocode + disassembly |
| `usart_protocol_decompile.txt` | 787 lines | USART2 ISR and protocol specification |
| `full_decompile.c` | 37909 lines | Complete 362-function decompilation |
| `function_names.md` | -- | Complete 362-entry naming inventory with confidence levels |
| `function_map_complete.txt` | 309 entries | Verified function map with sizes |
| `gap_functions.md` | -- | 17 gap functions catalogued by priority (P0-P3) |
| `hardware_map.txt` | -- | Every peripheral register access (GPIO, timer, USART, SPI, DMA) |
| `ram_map.txt` | -- | All labeled SRAM addresses with function cross-references |
| `xref_map.txt` | -- | Function cross-reference graph (caller/callee) |
| `button_candidates.c` | -- | Button input investigation code |
| `fpga_data_processors.c` | -- | Data processing pipeline functions |
| `interrupt_handlers.c` | -- | All 6 active interrupt handlers |

### ghidra_scripts/ Directory

| Script | Purpose |
|--------|---------|
| 6 Java scripts | Ghidra automation for bulk decompilation, cross-reference extraction, register access mapping |

### Other Directories

| Path | Contents |
|------|----------|
| `ghidra_project/` | Pre-analyzed Ghidra database (open in Ghidra to browse interactively) |
| `APP_2C53T_*.bin` | Original firmware binaries (V1.0.3, V1.0.7) at repo root |

---

## Appendix: 53-Step Boot Sequence Summary

For the complete annotated sequence, see `analysis_v120/FPGA_BOOT_SEQUENCE.md`. The abbreviated timeline:

```
Phase 1 (Clock/GPIO):  Enable all GPIO clocks + AFIO. Configure 47 pins.
                        AFIO remap to free PB3/4/5 from JTAG.

Phase 2 (Peripherals):  USART2 @ 9600 baud. TIM5 config. DMA0 clear.
                         Load meter_state defaults from data block.

Phase 3 (RTOS):         Create 7 queues + 8 tasks. Signal semaphore.

Phase 4 (FPGA init):    Send USART commands 0x01, 0x02, 0x06, 0x07, 0x08
                         (inline TX, not via queues).

Phase 5 (SPI3 config):  Enable SPI3 clock. Configure PB3/4/5/6.
                         PC6 HIGH (FPGA SPI enable).
                         SPI3: Mode 3, Master, /2, 8-bit, SW NSS.

Phase 6 (Delays):       Two SysTick-based delays with computed reload values.

Phase 7 (Handshake):    CS assert, send 0x05, read response, CS deassert.
                         Another SysTick delay.

Phase 8 (Post-init):    More SPI3 exchanges (0x12, etc). ADC params.
                         DMA config. Timer6/7. Watchdog (~5s).
                         TMR3 enable (drives USART + buttons).
                         PB11 HIGH (FPGA active mode).

Phase 9 (Run):          Start FreeRTOS scheduler. Tasks begin executing.
```

---

## Appendix: Remaining Unknowns

| Item | Status | Impact |
|------|--------|--------|
| Full RCC/CRM clock tree | Partially known | Need to verify 240MHz PLL matches stock |
| AFIO remap register value | Known pattern, exact bits need extraction | Required for SPI3 pin remap |
| ~10 FPGA command payloads | Command codes known, payload format unknown | Commands 0x1F-0x21, 0x25, 0x29, 0x2C |
| Battery voltage ADC channel | Unknown | Trace ADC config in master init |
| TMR8_BRK handler purpose | Unknown | Low priority |
