# FPGA Communication Protocol -- Complete Reference

## FNIRSI 2C53T Open-Source Firmware Project

**FPGA:** Gowin GW1N-UV2 (non-volatile)
**MCU:** Artery AT32F403A (ARM Cortex-M4F @ 240MHz)
**Firmware analyzed:** V1.2.0 binary, Ghidra decompilation
**Last updated:** 2026-04-01

---

## 1. Overview

The FPGA uses a **dual-interface architecture** with two completely independent communication channels running simultaneously:

| Interface | Purpose | Speed | Pins |
|-----------|---------|-------|------|
| **USART2** | Command/control | 9600 baud | PA2 (TX), PA3 (RX) |
| **SPI3** | Bulk ADC data | 60 MHz | PB3 (SCK), PB4 (MISO), PB5 (MOSI), PB6 (CS) |

**Why two interfaces?** The USART2 channel is a low-bandwidth, reliable command channel for configuration, mode switching, and meter data. It runs at 9600 baud because reliability matters more than speed for control messages. The SPI3 channel is a high-bandwidth data pipe for streaming raw ADC samples at 60 MHz -- fast enough to move 250MS/s waveform data in real time. Separating control from data means the MCU can reconfigure the FPGA (voltage range, trigger level, timebase) without interrupting the ADC data stream.

**Additional control signals:**

| Pin | Function | Active State |
|-----|----------|-------------|
| PC6 | SPI3 enable gate | HIGH to enable FPGA SPI3 |
| PB11 | FPGA active mode | HIGH during active measurement |

Both pins must be HIGH before SPI3 will respond with real data. Missing either one is the most common reason SPI3 returns 0xFF.

---

## 2. Physical Layer -- USART2 (Command/Control)

### Electrical

- **Pins:** PA2 (MCU TX to FPGA), PA3 (MCU RX from FPGA)
- **Baud rate:** 9600
- **Format:** 8N1 (8 data bits, no parity, 1 stop bit)
- **Peripheral base:** `0x40004400`

### Timing Model

USART2 is **not free-running**. It is driven by a **TMR3 interrupt** (IRQ 29, timer base `0x40000400`). The TMR3 ISR fires periodically and calls the `usart2_irq_handler` at `0x080277B4`, which pumps TX bytes out one at a time and collects RX bytes into a frame buffer. This timer-paced approach is critical -- polling or interrupt-driven USART without the timer cadence will not work correctly because the FPGA expects specific inter-byte timing.

The TMR3 ISR also triggers button scanning via `input_and_housekeeping`, so stopping TMR3 halts both USART communication and button input.

### TX Frame Format (MCU to FPGA, 10 bytes)

The TX buffer lives at `0x20000005` in RAM. The TX byte index at `0x2000000F` tracks which byte the ISR should send next.

```
Byte [0]: Header byte 0 (0x00 from BSS init)
Byte [1]: Header byte 1 (0x00 from BSS init)
Byte [2]: Command high byte (cmd_hi)
Byte [3]: Command low byte (cmd_lo)
Byte [4]: Device ID / fixed value (from DAT_20000008)
Byte [5]: Parameter 1 (context-dependent)
Byte [6]: Parameter 2 (context-dependent)
Byte [7]: Parameter 3 (context-dependent)
Byte [8]: Trailer (0xAA)
Byte [9]: Checksum = (cmd_item + cmd_hi) & 0xFF
```

The `usart_tx_frame_builder` task (`0x080373F4`, dvom_TX FreeRTOS task) receives 2-byte command items from `USART_TX_QUEUE` (`0x20002D74`, 10-deep, 2-byte items), fills the TX buffer, and enables the TX interrupt (`TDBEIEN` bit in `USART2_CTRL1`). After each frame, it calls `vTaskDelay(10)` (10ms) before accepting the next command. This enforces a minimum 10ms gap between consecutive TX frames.

### RX Frame Format (FPGA to MCU)

The RX buffer lives at `0x20004E11` (12 bytes). The RX byte index at `0x20004E10` tracks incoming bytes.

**Two frame types:**

| Type | Header | Total Length | Purpose |
|------|--------|-------------|---------|
| Data frame | `0x5A 0xA5` | 12 bytes | Meter data, ADC status, button state |
| Echo frame | `0xAA 0x55` | 10 bytes | Command acknowledgment |

**Data frame (0x5A 0xA5):**
```
Byte [0]:  0x5A (header)
Byte [1]:  0xA5 (header)
Bytes [2-6]: BCD-encoded meter digits (cross-byte nibble pairs)
Byte [7]:  Status flags (AC, auto-range, overload, polarity)
Byte [8]:  Measurement flags
Byte [9]:  Additional status
Bytes [10-11]: Extra data (big-endian 16-bit, used for range info)
```

On receiving a complete 12-byte data frame, the USART ISR:
1. Sends to queue `0x20002D7C` (meter semaphore)
2. Triggers PendSV (`SCB_ICSR |= (1 << 28)`) for immediate context switch

**Echo frame (0xAA 0x55):**
```
Byte [0]:  0xAA (header)
Byte [1]:  0x55 (header)
Bytes [2-9]: Echo data
```
Validation: `rx[3]` must match `tx[3]` (command echo) and `rx[7]` must be `0xAA`. Echo frames do not trigger queue sends -- they are for validation only.

**Captured idle heartbeat example:** `5A A5 E4 2E 63 25 07 00 00 00 00 xx`

### Checksum Calculation

```c
uint8_t checksum = (cmd_item + cmd_high_byte) & 0xFF;
```

Where `cmd_item` is the full 16-bit queue item (high byte = command, low byte = parameter) and `cmd_high_byte` is byte [2] of the TX frame.

---

## 3. Physical Layer -- SPI3 (Bulk Data)

### Electrical

- **SCK:** PB3 (Alternate function push-pull, 50MHz)
- **MISO:** PB4 (Input floating)
- **MOSI:** PB5 (Alternate function push-pull, 50MHz)
- **CS:** PB6 (General purpose output push-pull, software controlled, **active LOW**)
- **Peripheral base:** `0x40003C00`

**Important:** PB3, PB4, and PB5 are JTAG pins by default. The AFIO remap register must be set to disable JTAG-DP while keeping SW-DP:
```c
AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000;  // JTAG-DP disabled, SW-DP enabled
```
This frees PB3/PB4/PB5 for SPI3 use. The IOMUX/AFIO clock must be enabled first.

### SPI Configuration

| Parameter | Value | Register Bits |
|-----------|-------|---------------|
| Mode | 3 (CPOL=1, CPHA=1) | Clock idle HIGH, sample on falling edge |
| Role | Master | MSTR=1 |
| Data frame | 8-bit | DFF=0 |
| Bit order | MSB first | LSBFIRST=0 |
| NSS management | Software | SSM=1, SSI=1 |
| Baud rate | /2 prescaler | BR[2:0]=000 |
| Clock speed | 60 MHz | APB1 (120MHz) / 2 |
| Duplex | Full duplex | BIDIMODE=0 |

Post-init register writes:
```c
SPI3_CTL1 |= 0x02;   // Bit 1: TXDMAEN or RXNEIE
SPI3_CTL1 |= 0x01;   // Bit 0: RXDMAEN or TXEIE
SPI3_CTL0 |= 0x40;   // Bit 6: SPE (SPI enable)
```

### Control Pins

| Pin | Register | Assert | Deassert | Purpose |
|-----|----------|--------|----------|---------|
| PB6 (CS) | GPIOB BOP/BCR (`0x40010C10`/`0x40010C14`) | `BCR = 0x40` (LOW) | `BOP = 0x40` (HIGH) | Chip select |
| PC6 (Enable) | GPIOC BOP | `BOP = (1<<6)` (HIGH) | -- | Must be HIGH for FPGA SPI to function |
| PB11 (Active) | GPIOB BOP | `BOP = 0x800` (HIGH) | -- | Must be HIGH during measurement |

### Transfer Protocol

Every SPI3 exchange follows this sequence:

```
1. CS ASSERT:   GPIOB_BCR = 0x40    (PB6 LOW)
2. Wait TXE:    poll SPI3_STAT bit 1  (TX buffer empty)
3. Write:       SPI3_DATA = command_byte
4. Wait RXNE:   poll SPI3_STAT bit 0  (RX buffer not empty)
5. Read:        response = SPI3_DATA
6. [Repeat steps 2-5 for additional data bytes, sending 0xFF to clock out]
7. CS DEASSERT: GPIOB_BOP = 0x40    (PB6 HIGH)
```

The TXE/RXNE polling in the stock firmware uses an optimized pattern: it checks the status register 3 times in conditional IT blocks before falling into a polling loop, reducing latency for the common fast-response case.

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

---

## 4. Initialization Sequence

Complete power-on to first SPI3 data, based on decompilation of `system_init` (`FUN_08023A50`, ~15.4KB) and `spi3_init_and_setup` (`0x08039050`, 312 bytes).

### Phase 1: Clock and GPIO Init

```
Step 1.  Enable peripheral clocks: GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, AFIO
Step 2.  Configure PC9 as output, set LOW (power hold -- main() sets HIGH)
Step 3.  Configure 47 GPIO pins total:
           - Analog front-end MUX pins
           - LCD parallel bus (EXMC)
           - Button matrix inputs
           - SPI flash (SPI2) pins
           - USB pins
           - Debug UART pins
Step 4.  AFIO remap: AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000
           -> JTAG-DP disabled, SW-DP enabled
           -> Frees PB3/PB4/PB5 from JTAG for SPI3 use
```

### Phase 2: Peripheral Init

```
Step 5.  Enable NVIC IRQ9 (EXTI9_5 or DMA)
Step 6.  Enable USART clock: RCU_APB1EN |= 0x20000 (bit 17 = USART2)
Step 7.  Configure TIM5 (0x40000C00): timer_init() with APB1-derived prescaler
Step 8.  Configure DMA0: clear channel 0 flags
Step 9.  Configure USART2 (0x40004400): 9600 baud, 8N1, TX+RX enabled
Step 10. Initialize meter_state (0x200000F8): load defaults from data block
           - Validates data block header (0x55 or 0xAA signature)
           - Copies calibration values, range settings, mode defaults
```

### Phase 3: FreeRTOS Setup

```
Step 11. Create 7 queues:
           0x20002D6C: xQueueCreate(20, 1)  -- USART command queue
           0x20002D70: xQueueCreate(15, 1)  -- button event queue
           0x20002D74: xQueueCreate(10, 2)  -- USART TX queue (2-byte items)
           0x20002D78: xQueueCreate(15, 1)  -- SPI3 data trigger queue
           0x20002D7C: xQueueCreate(1, 0)   -- binary semaphore (meter RX complete)
           0x20002D80: xQueueCreate(1, 0)   -- binary semaphore (SPI3 init sync)
           0x20002D84: xQueueCreate(1, 0)   -- binary semaphore (EXTI notification)

Step 12. Create 8 FreeRTOS tasks:
           "Timer1"  @ 0x080400B9  prio=10    stack=10w
           "Timer2"  @ 0x080406C9  prio=1000  stack=1000w
           "display" @ 0x0803DA51  prio=1     stack=384w  (1536B)
           "key"     @ 0x08040009  prio=4     stack=128w  (512B)
           "osc"     @ 0x0804009D  prio=2     stack=256w  (1024B)
           "fpga"    @ 0x0803E455  prio=3     stack=128w  (512B)
           "dvom_TX" @ 0x0803E3F5  prio=2     stack=64w   (256B)
           "dvom_RX" @ 0x0803DAC1  prio=3     stack=128w  (512B)

Step 13. Signal semaphore at 0x20002D80
Step 14. Suspend tasks at 0x20002D7C and 0x20002D84
```

### Phase 4: USART Boot Commands

```
Step 15. Send initialization commands to FPGA via USART2 (inline, NOT via queues)
         Format: 10-byte frames transmitted byte-by-byte with TX polling
         Commands sent in sequence: 0x01, 0x02, 0x06, 0x07, 0x08
         These configure the FPGA for initial oscilloscope mode before the
         scheduler starts.
```

**Critical note:** These boot commands are sent by direct register polling, not through the FreeRTOS queue/task infrastructure, because the scheduler has not started yet.

### Phase 5: SPI3 Configuration

```
Step 16. Enable SPI3 clock: RCU_APB1EN |= 0x8000 (bit 15)
Step 17. Enable RCU_APB2EN bits for AFIO (bit 0 = AFEN)

Step 18. Configure SPI3 GPIO:
           PB3 = SPI3_SCK  (AF push-pull, 50MHz)
           PB4 = SPI3_MISO (input floating)
           PB5 = SPI3_MOSI (AF push-pull, 50MHz)
           PB6 = SPI3_CS   (GPIO output push-pull)

Step 19. *** PC6 = FPGA SPI enable (output push-pull, set HIGH) ***
           GPIOC_BOP = (1 << 6)

Step 20. Configure GPIOC pin for SPI3 (via gpio_init at 0x08026634)

Step 21. SPI3 peripheral init:
           Full duplex, Master, /2 clock, MSB first, 8-bit
           CPOL=1, CPHA=1 (MODE 3)
           Software NSS (SSM=1, SSI=1)

Step 22. Post-init:
           SPI3_CTL1 |= 0x02  (TXDMAEN or RXNEIE)
           SPI3_CTL1 |= 0x01  (RXDMAEN or TXEIE)
           SPI3_CTL0 |= 0x40  (SPE = SPI enable)

Step 23. Enable more APB2 clocks: RCU_APB2EN |= 0x10
```

### Phase 6: SysTick Delays

```
Step 24. SysTick delay #1: LOAD = *(0x20002B1C) * 10, wait for COUNTFLAG
Step 25. SysTick delay #2: LOAD = *(0x20002B20), wait for COUNTFLAG
Step 26. Timed loop (2 iterations):
           Counter starts at 100 (0x64)
           First: if count >= 51 -> LOAD = reload * 50, count -= 50
           Second: if count < 51 -> LOAD = reload * count, count = 0
           Each iteration: start SysTick, wait for COUNTFLAG, stop
```

These delays are timing-critical. The FPGA needs specific setup time after SPI3 is enabled before the handshake can proceed.

### Phase 7: SPI3 FPGA Handshake

```
Step 27. CS DEASSERT: GPIOB_BOP = 0x40 (PB6 HIGH)
Step 28. SPI3 send 0x00 (dummy, CS high) -- flush bus
Step 29. Read SPI3 response (discard)

Step 30. CS ASSERT: GPIOB_BCR = 0x40 (PB6 LOW)
Step 31. SPI3 send 0x05 -- FPGA COMMAND (status/ID query)
Step 32. Read SPI3 response (FPGA status/ID)

Step 33. SPI3 send 0x00 (parameter, still CS low)
Step 34. Read SPI3 response

Step 35. CS DEASSERT: GPIOB_BOP = 0x40 (PB6 HIGH)

Step 36. SPI3 send 0x00 (dummy, CS high) -- flush
Step 37. Read response (discard)

Step 38. SPI3 send 0x00 (dummy)
Step 39. Read response (discard)

Step 40. Another SysTick delay loop (100 -> 50 -> 0, same as step 26)
```

### Phase 8: Post-Handshake

```
Step 41. CS ASSERT again
Step 42. More SPI3 exchanges (command 0x12 and others)
Step 43. Configure ADC sampling parameters
Step 44. Set up trigger levels and timebase
Step 45. Initialize display buffer pointers

Step 46. DMA configuration for SPI3 bulk transfers
Step 47. Timer6/Timer7 configuration
Step 48. Meter measurement function init
Step 49. Watchdog timer init (~5 sec timeout, IWDG)
Step 50. Enable TMR3 (IRQ 29) -- THIS DRIVES THE USART EXCHANGE
Step 51. Enable TMR6/TMR7

Step 52. *** PB11 set HIGH *** (FPGA active mode signal)
           GPIOB_BOP = 0x800
           (Set in mode_switch_reset_handler, late in boot)

Step 53. Start FreeRTOS scheduler (tail-call to 0x0803A6D8)
```

### Phase 9: Runtime

```
Step 54. TMR3 ISR fires periodically -> calls usart2_irq_handler()
Step 55. USART handler pumps TX bytes / receives RX bytes
Step 56. On valid 12-byte RX frame (0x5A 0xA5 header):
           -> sends to queue 0x20002D7C
           -> triggers PendSV for context switch

Step 57. FPGA task processes queue items:
           -> usart_cmd_dispatcher processes commands
           -> spi3_acquisition_task waits on queue 0x20002D78
           -> When triggered, does SPI3 transfers (9 command types)

Step 58. SPI3 data stored at meter_state+0x5B0 (CH1) and +0x9B0 (CH2)
```

---

## 5. Complete Command Code Table

### Command Dispatch Architecture

USART commands flow through a multi-level system:

1. **Application layer** calls mode-specific functions that queue 1-byte command codes to `usart_cmd_queue` (`0x20002D6C`)
2. **`usart_cmd_dispatcher`** (`0x08036A50`) receives these and dispatches through a function pointer table at `0x0804BE74`
3. Dispatch handlers call **`usart_tx_config_writer`** (`0x08039734`) which formats 2-byte items and queues them to `usart_tx_queue` (`0x20002D74`)
4. **`usart_tx_frame_builder`** (`0x080373F4`, dvom_TX task) dequeues items, fills the 10-byte TX frame, and triggers transmission

### USART Command Codes by Subsystem

#### Reset/Init

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x00` | TX | Reset/Init | No | Sent at boot and on mode transitions |

#### Oscilloscope -- Channel Configuration

| Code | Direction | Purpose | Config Writer Type | Blocking |
|------|-----------|---------|-------------------|----------|
| `0x01` | TX | Configure channel (generic) | Type 0 (CH1) or Type 1 (CH2) | No |
| `0x0B` | TX | Scope channel config | Types 0-3 | No |
| `0x0C` | TX | Scope channel config | Types 0-3 | No |
| `0x0D` | TX | Scope channel config | Types 0-3 | No |
| `0x0E` | TX | Scope channel config | Types 0-3 | No |
| `0x0F` | TX | Scope channel config | Types 0-3 | No |
| `0x10` | TX | Scope channel config | Types 0-3 | No |
| `0x11` | TX | Scope channel config | Types 0-3 | No |

#### Oscilloscope -- Trigger Configuration (from gap function `FUN_0800bb10`)

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x16` | TX | Trigger: threshold LSB | No | Sent as part of 5-command sequence |
| `0x17` | TX | Trigger: threshold MSB | No | |
| `0x18` | TX | Trigger: mode/edge select | No | |
| `0x19` | TX | Trigger: holdoff | **Yes** (portMAX_DELAY) | Final in sequence, blocks until accepted |

#### Oscilloscope -- Channel Ranges (from gap function `FUN_0800ba06`)

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x1A` | TX | Channel: CH1 gain | No | Part of 6-command sequence |
| `0x1B` | TX | Channel: CH1 offset | No | |
| `0x1C` | TX | Channel: CH2 gain | No | |
| `0x1D` | TX | Channel: CH2 offset | No | |
| `0x1E` | TX | Channel: coupling/BW limit | **Yes** (portMAX_DELAY) | Final in sequence |

#### Oscilloscope -- Timebase (from gap function `FUN_0800bc00`)

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x26` | TX | Timebase: prescaler | No | Part of 3-command sequence |
| `0x27` | TX | Timebase: period | No | |
| `0x28` | TX | Timebase: mode | **Yes** (portMAX_DELAY) | Final in sequence |

#### Oscilloscope -- Acquisition Mode (from gap function `FUN_0800bba6`)

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x20` | TX | Acquisition: run mode | No | Part of 2-command sequence |
| `0x21` | TX | Acquisition: sample depth | **Yes** (portMAX_DELAY) | Final in sequence |

#### Multimeter

| Code | Direction | Purpose | Config Writer Type | Blocking |
|------|-----------|---------|-------------------|----------|
| `0x07` | TX | Probe detected (PC7 HIGH) | Type 4 (range) | No |
| `0x08` | TX | Meter: configure range | Type 4 (range) | No |
| `0x09` | TX | Meter: start measurement | -- | No |
| `0x0A` | TX | No probe detected (PC7 LOW) | -- | No |
| `0x12` | TX | Meter variant setup | Type 4 | No |
| `0x13` | TX | Meter variant setup | Type 4 | No |
| `0x14` | TX | Meter variant setup | Type 4 | No |
| `0x16` | TX | Meter range config | Type 4 | No |
| `0x17` | TX | Meter range config | Type 4 | No |
| `0x18` | TX | Meter range config | Type 4 | No |
| `0x19` | TX | Meter range config | Type 4 | No |
| `0x1A` | TX | Meter range config | Type 4 | No |
| `0x1B` | TX | Meter range config | Type 4 | No |
| `0x1C` | TX | Meter range config | Type 4 | No |
| `0x1D` | TX | Meter range config | Type 4 | No |
| `0x1E` | TX | Meter range config | Type 4 | No |

**Note:** Command codes `0x16`-`0x1E` are shared between oscilloscope trigger/channel configuration and meter range configuration. The interpretation depends on the current operating mode. In scope mode they configure trigger and channel ranges; in meter mode they configure measurement ranges.

#### Signal Generator

| Code | Direction | Purpose | Config Writer Type | Blocking |
|------|-----------|---------|-------------------|----------|
| `0x02` | TX | Siggen setup | Type 5 (freq) | No |
| `0x03` | TX | Siggen setup | Type 6 (wave) | No |
| `0x04` | TX | Siggen setup | -- | No |
| `0x05` | TX | Siggen setup | -- | No |
| `0x06` | TX | Siggen setup | -- | No |

#### Probe Attenuation (from gap function `FUN_0800bc98`)

| Code | Direction | Purpose | Blocking | Notes |
|------|-----------|---------|----------|-------|
| `0x07` | TX | Probe attenuation (param < 0) | No | Conditional on parameter sign |
| `0x0A` | TX | Probe attenuation (param >= 0) | No | Conditional on parameter sign |

#### Other Modes

| Code | Direction | Purpose | Mode | Notes |
|------|-----------|---------|------|-------|
| `0x15` | TX | Standalone mode 7 | 7 | Single command |
| `0x1F` | TX | Mode 4 config | 4 | Unknown payload |
| `0x20` | TX | Mode 4 config | 4 | -- |
| `0x21` | TX | Mode 4 config | 4 | -- |
| `0x25` | TX | Mode 5 config | 5 | Unknown payload |
| `0x26` | TX | Mode 5 config | 5 | -- |
| `0x27` | TX | Mode 5 config | 5 | -- |
| `0x28` | TX | Mode 5 config | 5 | -- |
| `0x29` | TX | Standalone mode 6 | 6 | Single command |
| `0x2C` | TX | Mode 8 config | 8 | -- |

### Mode Dispatcher Map

The mode dispatcher at `FUN_0800735C` selects which command sequences to send based on the active mode:

| Mode | Name | Command Sequence |
|------|------|-----------------|
| 0 | Oscilloscope | 0x00, 0x01, 0x0B-0x11 |
| 1 | Multimeter (basic) | 0x00, 0x09, 0x07/0x0A, 0x1A-0x1E |
| 2 | Signal Generator | 0x02-0x06, 0x08, then mode 9 tail |
| 3 | Multimeter (extended) | 0x00, 0x08, 0x09, 0x07/0x0A, 0x16-0x19 |
| 4 | Unknown | 0x00, 0x1F, 0x09, 0x20, 0x21 |
| 5 | Unknown | 0x00, 0x25, 0x09, 0x26, 0x27, 0x28 |
| 6 | Standalone | 0x29 |
| 7 | Standalone | 0x15 |
| 8 | Unknown | 0x00, 0x2C |
| 9 | Multimeter (variant) | 0x00, 0x12, 0x13, 0x14, then 0x09, 0x07/0x0A |

For modes 1, 3, and 9: command `0x07` is sent if PC7 is HIGH (probe detected), otherwise `0x0A` is sent.

### Command Sequencing Pattern

Multi-command sequences use a specific blocking pattern from the gap functions:

```c
// Example: set trigger config (FUN_0800bb10)
xQueueGenericSend(queue, &cmd_0x07_or_0x0A, 0, 0);  // Non-blocking
xQueueGenericSend(queue, &cmd_0x16, 0, 0);            // Non-blocking
xQueueGenericSend(queue, &cmd_0x17, 0, 0);            // Non-blocking
xQueueGenericSend(queue, &cmd_0x18, 0, 0);            // Non-blocking
xQueueGenericSend(queue, &cmd_0x19, portMAX_DELAY, 0); // BLOCKS until accepted
```

The final command in every sequence uses `portMAX_DELAY` (0xFFFFFFFF) to block the calling task until the queue accepts it. This ensures all commands in the batch are enqueued before the caller proceeds.

---

## 6. SPI3 Acquisition Modes

The `spi3_acquisition_task` (`0x08037454`, 7208 bytes) is the heart of the data pipeline. It waits on `spi3_data_queue` (`0x20002D78`) for trigger events, then executes one of 9 acquisition modes selected by `(trigger_byte - 1)`.

**Important:** The trigger is **double-buffered**. When the acquisition counter in `input_and_housekeeping` reaches the timebase threshold, TWO items are sent to the SPI3 queue back-to-back. This triggers two consecutive read cycles, implementing a ping-pong scheme that prevents display tearing.

```
                    trigger_byte - 1
                         |
         +---------------+-------------------------------+
         |               |                               |
    +----v----+   +------v------+   +----------+  +------v--+
    | Case 0  |   |   Case 1   |   |  Case 2  |  | Case 3  |
    | FAST TB |   | ROLL MODE  |   |  NORMAL  |  |  DUAL   |
    | config  |   | circ buf   |   | 1024 B   |  | 2048 B  |
    | only    |   | 300 samp   |   | interl.  |  | CH1+CH2 |
    +---------+   +------------+   +----------+  +---------+

    +---------+  +---------+  +----------+  +----------+  +----------+
    | Case 4  |  | Case 5  |  |  Case 6  |  |  Case 7  |  |  Case 8  |
    |EXTENDED |  |METER ADC|  | SIGGEN   |  |CALIBRATE |  |SELF TEST |
    | cmd only|  | ch read |  | feedback |  | 16-bit   |  | verify   |
    +---------+  +---------+  +----------+  +----------+  +----------+
```

### Case 0: Fast Timebase Config

**Purpose:** Configures FPGA for fast sweep speeds (timebase index < 4). No ADC data is read.

**Trigger condition:** `trigger_byte == 1`

**Protocol:**
```
CS_ASSERT
spi3_xfer(timebase_index)    // ms[0x2D], range 0x00-0x13
CS_DEASSERT
```

**Logic:**
```c
uint8_t tb_idx = ms[0x2D];
uint8_t sample_threshold = TIMEBASE_SAMPLE_TABLE[tb_idx] + 0x32;
uint16_t acq_count = *(uint16_t *)(ms + 0xDB8);
if (sample_threshold <= acq_count) {
    // Acquisition complete, skip SPI transfer
    return;
}
spi3_xfer(tb_idx);  // Configure FPGA timebase
```

The timebase-to-sample-count lookup table lives at `0x0804D833` in flash.

### Case 1: Roll Mode (Streaming)

**Purpose:** Continuous scrolling oscilloscope display. Reads one sample pair per trigger and appends to a 300-sample circular buffer.

**Trigger condition:** `trigger_byte == 2`

**Byte count:** 5 bytes per trigger (2 per channel + 1 extra)

**Protocol:**
```
CS_ASSERT
spi3_xfer(0xFF) -> raw_ch1_hi (reference)
spi3_xfer(0xFF) -> raw_ch1_lo (data)
spi3_xfer(0xFF) -> raw_ch2_hi (data)
spi3_xfer(0xFF) -> raw_ch2_lo (extra)
spi3_xfer(0xFF) -> last_byte  -> stored at ms[0x5AF]
CS_DEASSERT
```

**Buffer management:**
- CH1 circular buffer: `ms + 0x356` (301 bytes, `0x12D`)
- CH2 circular buffer: `ms + 0x483` (301 bytes, `0x12D`)
- Read pointer: `ms + 0xDB4` (16-bit)
- Sample count: `ms + 0xDB6` (16-bit, max 300 = `0x12C`)
- Roll calibrated CH1: `ms + 0x482`
- Roll calibrated CH2: `ms + 0x5AF`

**Circular buffer algorithm:**
1. If `read_ptr > 0`, decrement it
2. If `sample_count <= 300`, increment it
3. Shift all existing samples down by 1 position (expensive memmove)
4. Read new samples via SPI3
5. Apply VFP calibration
6. Store calibrated values at the head of the buffer

**Display trigger:** When `sample_count == 0x12D` (301):
- Check `hold_mode` and `trigger_mode`
- Disable TMR3 (temporarily stop USART polling)
- Send command 2 to USART cmd queue
- Call `measurement_calc()` at `0x08034078`
- Copy circular buffer to main ADC buffers

### Case 2: Normal Scope Acquisition

**Purpose:** Standard oscilloscope capture. Reads 1024 bytes of interleaved CH1/CH2 data.

**Trigger condition:** `trigger_byte == 3`

**Byte count:** 1024 bytes (`0x400`)

**Protocol:**
```
CS_ASSERT
spi3_xfer(0xFF)    // Initial command byte (read all)
// Tight loop, 1024 bytes:
for i = 0 to 0x3FF step 2:
    spi3_xfer(0xFF)  -> CH1 byte -> ms[0x5B0 + i]
    spi3_xfer(0xFF)  -> CH2 byte -> ms[0x5B0 + i + 1]
CS_DEASSERT
```

**Data format:** Interleaved unsigned 8-bit samples:
```
Byte 0: CH1 sample [0]
Byte 1: CH2 sample [0]
Byte 2: CH1 sample [1]
Byte 3: CH2 sample [1]
...
Byte 1022: CH1 sample [511]
Byte 1023: CH2 sample [511]
```

**Buffer:** All 1024 bytes stored contiguously at `ms + 0x5B0`.

After the transfer completes, falls through to VFP calibration post-processing (section 7).

### Case 3: Dual Channel Acquisition

**Purpose:** Extended dual-channel capture. Reads 2048 bytes, split into separate CH1 and CH2 buffers.

**Trigger condition:** `trigger_byte == 4`

**Byte count:** 2048 bytes (`0x800`)

**Protocol:** Same as case 2 but double the data:
```
CS_ASSERT
spi3_xfer(0xFF)    // Initial command
// 2048 bytes interleaved:
for i = 0x400 to 0x7FF step 2:
    spi3_xfer(0xFF)  -> ms[0x5B0 + i]
    spi3_xfer(0xFF)  -> ms[0x5B0 + i + 1]
CS_DEASSERT
```

**Buffers:**
- CH1: `ms + 0x5B0` (1024 bytes)
- CH2: `ms + 0x9B0` (1024 bytes)

After transfer, falls through to calibration at `0x0803806C`.

### Case 4: Extended Command

**Purpose:** Sends a specific command byte to the FPGA without reading bulk data. Used for XY mode, deep memory, or configuration commands.

**Trigger condition:** `trigger_byte == 5`

**Byte count:** 1 (command only)

**Protocol:**
```
CS_ASSERT
spi3_xfer(command_code)    // Transformed voltage range
CS_DEASSERT
```

Stores response to `ms + 0x46` (trigger status word).

### Case 5: Meter ADC Read

**Purpose:** Reads ADC value for multimeter mode from the specified channel.

**Trigger condition:** `trigger_byte == 6`

**Byte count:** 1

**Protocol:**
```
CS_ASSERT
spi3_xfer(active_channel)    // ms[0x16], value 0 or 1
CS_DEASSERT
```

### Case 6: Signal Generator Feedback

**Purpose:** Monitors signal generator output by reading feedback from the FPGA.

**Trigger condition:** `trigger_byte == 7`

**Byte count:** 1

**Protocol:**
```
CS_ASSERT
spi3_xfer(trigger_mode)    // ms[0x18]
CS_DEASSERT
```

### Case 7: Calibration Readback (16-bit)

**Purpose:** Multi-phase SPI3 exchange for reading FPGA internal status, calibration data, or version info. Returns a 16-bit assembled value.

**Trigger condition:** `trigger_byte == 8`

**Byte count:** 5 (across two CS cycles)

**Protocol:**
```
Phase 1:
  CS_ASSERT
  spi3_xfer(command_code)  -> high_byte
  CS_DEASSERT
  vTaskDelay(1)            // 1ms pause between phases

Phase 2:
  CS_ASSERT
  spi3_xfer(0x0A)         -> status (discarded)
  spi3_xfer(0xFF)         -> low_byte_1
  spi3_xfer(0xFF)         -> low_byte_2
  CS_DEASSERT
```

**Result assembly:**
```c
ms[0x46] = (high_byte << 8) | low_byte;  // 16-bit status word
ms[0xDB0]++;                               // Increment frame counter
```

### Case 8: Self-Test

**Purpose:** FPGA communication integrity test. Used during boot validation.

**Trigger condition:** `trigger_byte == 9`

**Byte count:** 3

**Protocol:**
```
CS_ASSERT
spi3_xfer(command_code)
spi3_xfer(0xFF)
spi3_xfer(0xFF)
CS_DEASSERT
```

---

## 7. ADC Data Format and Calibration

### Raw Data Format

- **Encoding:** Unsigned 8-bit (0-255)
- **Interleaving:** CH1 and CH2 alternate byte-by-byte in normal mode
- **DC offset:** The FPGA ADC has a hardware DC offset of approximately **28 LSBs**

### VFP Register Assignment

These registers are loaded once when `spi3_acquisition_task` enters its main loop and remain valid for the lifetime of the task:

| VFP Register | Value | Purpose |
|-------------|-------|---------|
| `s16` | `-28.0` | ADC zero offset (hardware DC offset correction) |
| `s18` | *(literal pool)* | Calibration gain CH1 |
| `s20` | *(literal pool)* | Calibration offset CH2 |
| `s22` | *(literal pool)* | DC bias correction |
| `s24` | *(literal pool)* | Maximum clamp value (255.0) |
| `s26` | *(literal pool)* | Minimum clamp value (0.0) |
| `s28` | *(literal pool)* | Range divisor |

Literal pool location: `0x08037674+` in flash.

### Per-Sample Calibration Formula (Normal/Dual Mode)

Applied to every raw 8-bit ADC sample after bulk SPI3 read (cases 2 and 3):

```c
float raw_f    = (float)(uint8_t)raw_sample;
float norm     = (raw_f + s16_offset) / s28_divisor;      // Normalize to 0..1
int8_t dc_off  = meter_state[0x04];                        // Per-channel DC offset (signed)
float range    = (float)((int16_t)voltage_range - dc_off); // Display range
float result   = norm * range + (float)dc_off + s22_bias;  // Scale to display coords

// Clamp to valid pixel range
if (result > s24_max) result = s24_max;   // 255.0
if (result < 0.0f)    result = s26_min;   // 0.0

calibrated = (uint8_t)(int)result;
```

**Conditions for calibration to run:**
- `spi3_transfer_mode` (`ms[0x14]`) must not be 3
- `timebase_index` (`ms[0x2D]`) must be < 4
- `calibration_mode` (`ms[0x33]`) must be 0 (not calibrating)
- Both probe types (`ms[0x352]`, `ms[0x353]`) must be valid (not 0 or 0xFF)

### Roll Mode Probe Compensation Formula

Applied in case 1 when probe type value exceeds `0xDC` (220):

```c
float probe_f    = (float)probe_type;
float probe_gain = (probe_f + s16_offset) / s18_gain;
float raw_f      = (float)raw_byte;
float adjusted   = (raw_f + s20_offset - (float)dc_offset) / probe_gain;
float result     = adjusted + s22_bias + (float)dc_offset;

// Clamp
if (result > s24_max) result = s24_max;
if (result < s26_min) result = s26_min;

calibrated = (uint8_t)(int)result;
```

### Buffer Locations (offsets from `meter_state` at `0x200000F8`)

| Offset | Size | Purpose |
|--------|------|---------|
| `+0x356` | 301 bytes | Roll mode circular buffer CH1 |
| `+0x482` | 1 byte | Roll mode calibrated sample CH1 |
| `+0x483` | 301 bytes | Roll mode circular buffer CH2 |
| `+0x5AF` | 1 byte | Roll mode last byte / calibrated CH2 |
| `+0x5B0` | 1024 bytes | ADC sample buffer CH1 |
| `+0x9B0` | 1024 bytes | ADC sample buffer CH2 |
| `+0xDB0` | 1 byte | SPI3 frame counter |
| `+0xDB4` | 2 bytes | Roll mode read pointer |
| `+0xDB6` | 2 bytes | Roll mode sample count (max 300) |
| `+0xDB8` | 2 bytes | Acquisition sample counter |

### Calibration Data Source

Per-channel calibration data (301 bytes each) is loaded from SPI flash at boot by `calibration_loader` (`FUN_08001830`):
- CH1: loaded to `ms + 0x356` buffer with XOR 0x80 transform
- CH2: loaded to `ms + 0x483` buffer with XOR 0x80 transform
- 6 gain/offset calibration pairs stored at RAM `0x20000358`-`0x20000434`

---

## 8. Meter Data Format

### Processing Pipeline

The `meter_data_processor` (`0x08036AC0`, 1776 bytes) runs as a FreeRTOS task that:
1. Blocks on `METER_SEMAPHORE` (`0x20002D7C`) until USART ISR signals a complete RX frame
2. Extracts BCD digits from RX frame bytes [2..6]
3. Applies calibration (double-precision floating point)
4. Classifies the result and formats for display

### BCD Digit Extraction

USART RX frame bytes [2..6] contain cross-byte nibble pairs. Each pair is looked up in a command lookup table to produce a decimal digit (0-9) or special code:

```c
uint8_t b2 = rx[2], b3 = rx[3], b4 = rx[4], b5 = rx[5], b6 = rx[6];

digit0 = lookup_table[(b2 & 0xF0) | (b3 & 0x0F)];
digit1 = lookup_table[(b3 & 0xF0) | (b4 & 0x0F)];
digit2 = lookup_table[(b4 & 0xF0) | (b5 & 0x0F)];
digit3 = lookup_table[(b5 & 0xF0) | (b6 & 0x0F)];
```

### Special Digit Codes

| Pattern | Meaning | Action |
|---------|---------|--------|
| `digit0=0x0A, digit1=0x0B` | "OL" -- Overload | Display "OL" |
| `digit0=0x10, digit1=0x10` | Blank display | No measurement active |
| `digit0=0x10, digit1=0x11` | Partial blank | Transitional state |
| `digit1=0x12, digit2=0x0A, digit3=5` | Continuity detected | Sets buzzer state `ms[0xF5D] = 0xB1` |
| `digit1=0x13, digit2=0x14` | Mode change indicator | Mode transition in progress |
| Any digit = `0xFF` | Invalid/unrecognized | Skip measurement |

### BCD Assembly

For valid digits, values >= 0x10 have 0x10 subtracted:

```c
int d0 = (digit0 >= 0x10) ? (digit0 - 0x10) : digit0;
int d1 = (digit1 >= 0x10) ? (digit1 - 0x10) : digit1;
int d2 = (digit2 >= 0x10) ? (digit2 - 0x10) : digit2;
int d3 = (digit3 >= 0x10) ? (digit3 - 0x10) : digit3;
int raw_value = d0 * 1000 + d1 * 100 + d2 * 10 + d3;
```

### Calibration (Double Precision)

The meter uses ARM EABI soft-float double-precision calls for calibration:

```c
// For each measurement channel (up to 4 iterations):
double divisor  = (double)cal_coeff;               // __aeabi_ui2d
double quotient = ref_value / divisor;              // __aeabi_ddiv
double result   = quotient * accumulated_scale;     // __aeabi_dmul
int    final    = (int)result;                      // __aeabi_d2iz
```

VFP double constants from literal pool at `0x080373CC`:
- `0.0` (zero reference)
- `494.0` (calibration reference value)
- `4503599627370496.0` (2^52, used for integer conversion)

### Result Classification

Stored at `ms[0xF34]`:

| Code | Meaning |
|------|---------|
| 1 | Normal reading |
| 2 | Underrange (value too small for current range) |
| 3 | Overrange (exceeds display but not OL) |
| 4 | Invalid measurement |

### Result Formatting by Sub-Mode

The `meter_sub_mode` field at `ms[0x10]` selects the display format:

| Sub-Mode | Function | Decimal Offset |
|----------|----------|---------------|
| 0 | Voltage DC | digit_count + 0x0A |
| 1 | Voltage AC | Check polarity, +2 |
| 2 | Current | Check flags, +2 |
| 3 | (Unused) | 0xFF (invalid) |
| 4 | Resistance | Dual-channel conditional offset |
| 5 | Capacitance | digit_count + 2 |
| 6 | Frequency | digit_count + 0x0A |
| 7 | Diode/Continuity | Special handling |

### Continuity Buzzer

The continuity buzzer is driven by EXTI3 interrupt handler at `0x08009C10`. When the meter data processor detects the continuity BCD pattern:

```c
ms[0xF5D] = 0xB1;    // Activate buzzer
ms[0xF2D] = 1;        // Update meter mode state
```

The EXTI3 ISR reads `ms[0xF5D]` to drive the piezo buzzer. Values: `0xB0` = buzzer off, `0xB1` = buzzer on.

### 8-State Meter Mode FSM

The `meter_mode_handler` (`0x080371B0`, 504 bytes) processes RX frame status bits to track the meter operating state. State variable: `ms[0xF2D]`.

**Input bits from RX frame:**
- `rx[7]` (status byte): bit 0 = polarity, bit 1 = overload type 1, bit 2 = AC flag, bit 3 = auto-range, bit 5 = AC mode path
- `rx[6]` (flags byte): bit 4 = range indicator 2 / standby, bit 5 = range indicator 1, bit 6 = hold flag / cal coefficient

**State transitions:**

| State | Name | Entry Condition | Key Checks | Outputs |
|-------|------|-----------------|------------|---------|
| 0 | IDLE/INIT | Default | rx[7] bits 5,1,0,3 | Transitions to 1-5 |
| 1 | POLARITY | Overload detected | rx[7] bit 0 | Sets cal_coeff 0/1 |
| 2 | OVERLOAD/RANGE | Range change | ms[0xF36], rx[7] bits 0,3 | Updates range state |
| 3 | AC/DC | AC mode detected | rx[7] bit 2, rx[6] bit 6 | Sets cal_coeff 2 |
| 4 | RANGE INDICATOR | Range readback | rx[6] bits 4,5, rx[7] bit 0 | Sets cal_coeff 0-3 |
| 5 | AUTO-RANGE | Auto-range active | ms[0xF39] flag | Sets cal_coeff 0/4 |
| 6 | STANDBY | Idle/wait | rx[6] bit 4 | Sets cal_coeff 0-2 |
| 7 | STANDBY | Idle/wait | rx[6] bit 4 | Sets cal_coeff 0-2 |

Post-processing: if `overload_flag == 2`, extracts big-endian 16-bit value from `rx[10..11]` into `ms[0xF3A]`.

---

## 9. Config Writer Bit Mappings

The `usart_tx_config_writer` (`0x08039734`, 316 bytes) supports 7 command types that encode configuration parameters into the USART TX frame payload bytes.

### Type 0: Scope CH1 Config

Encodes CH1 voltage range, coupling, and bandwidth limit into TX frame.

### Type 1: Scope CH2 Config

Same format as Type 0 but for CH2.

### Scope Channel Config Bit Layout

`config[0x20]` bit assignments:
```
Bit 1:  CH1 AC/DC coupling (0=DC, 1=AC)
Bit 3:  CH1 bandwidth limit (0=full, 1=20MHz)
Bit 5:  CH2 AC/DC coupling (0=DC, 1=AC)
Bit 7:  CH2 bandwidth limit (0=full, 1=20MHz)
Bit 9:  Trigger edge select (0=rising, 1=falling)
Bit 11: Trigger source select (0=CH1, 1=CH2)
```

`config[0x18]` voltage range encoding:
```
Bits [1:0]:   Range low (2 bits)
Bits [15:12]: Range high (4 bits)
```

### Type 2: Trigger Config

Formats trigger threshold, mode, edge, and holdoff values.

### Type 3: Timebase Config

Formats timebase prescaler, period, and mode values.

### Type 4: Meter Range Config

Used for all meter commands (0x07, 0x08, 0x12-0x14, 0x16-0x1E). Encodes measurement range selection.

### Type 5: Signal Generator Frequency

Encodes frequency value for signal generator output.

### Type 6: Signal Generator Waveform

Encodes waveform type selection (sine, square, triangle, sawtooth, etc.).

### SPI3 Command Byte Transformation

Before sending to SPI3, the voltage range value is transformed:

```c
int16_t command_code = ~0x7F ^ voltage_range;  // Bitwise transform for FPGA
```

This produces the actual byte sent as the first SPI3 command after CS assert.

---

## 10. Troubleshooting

### What We Got Wrong (and How We Fixed It)

This section documents the investigation path. Every one of these was a real mistake that cost hours of debugging.

#### "SPI3 returns 0xFF on every read"

**Root causes (all four must be addressed):**

1. **PB11 not set HIGH.** The FPGA has an "active mode" signal on PB11. Without `GPIOB_BOP = 0x800`, the FPGA keeps MISO pulled HIGH and ignores all SPI3 commands. PB11 is set late in the boot sequence (step 52) inside the mode switch handler, making it easy to miss.

2. **USART boot commands not sent.** Before SPI3 will return real data, the FPGA must receive its initialization sequence over USART2: commands `0x01, 0x02, 0x06, 0x07, 0x08`. These are sent inline (byte-by-byte TX polling) before the FreeRTOS scheduler starts. If you skip them, the FPGA is not configured and SPI3 data is meaningless.

3. **Trigger mechanism missing.** SPI3 data reads are not initiated by polling. They are driven by queue events from `input_and_housekeeping`, which sends items to `spi3_data_queue` (`0x20002D78`) when the acquisition counter reaches the timebase threshold. Without this timer-driven trigger, the acquisition task blocks forever on `xQueueReceive`.

4. **SysTick delays between boot phases.** The FPGA needs specific timing between SPI3 enable (step 22) and the handshake (step 30). The stock firmware uses SysTick-based delays with values loaded from RAM. Skipping these delays can cause the handshake to fail silently.

#### "Buttons are not working -- is the FPGA involved?"

**No.** All 15 buttons are on MCU GPIO pins (GPIOB and GPIOC). They use a 3-group multiplexed scanning scheme in `input_and_housekeeping` (`0x08039188`). The scanning is driven by TMR3. If TMR3 is not running, buttons will not be read.

Button debounce uses 70-tick (short press) and 72-tick (long press) thresholds with the button map table at `0x08046528`.

#### "USART communication is unreliable"

**It must be TMR3-driven.** The stock firmware does not use a free-running USART with interrupt-driven TX/RX. Instead, TMR3 fires periodically and the ISR calls the USART handler, which pumps one TX byte per invocation and collects RX bytes. Attempting to use standard interrupt-driven USART or polled USART will fail because the FPGA expects specific inter-byte timing.

Also: after each TX frame, there must be a 10ms delay (`vTaskDelay(10)`) before the next frame.

#### "Watchdog resets during development"

The IWDG has a ~5 second timeout. It is fed by `input_and_housekeeping` every 11 calls (approximately every 50ms). If you disable TMR3 for debugging or enter a long loop, the watchdog will fire.

```c
// Feed the watchdog
IWDG_KR = 0xAAAA;  // IWDG_RELOAD_KEY at 0x40003000
```

#### "Meter readings are garbage"

Check that:
- The meter semaphore (`0x20002D7C`) is being posted by the USART RX handler when a complete 0x5A 0xA5 frame arrives
- BCD extraction uses cross-byte nibble pairs, not byte-aligned nibbles
- The double-precision calibration path is using the correct EABI soft-float functions
- `ms[0xF35]` (meter data valid flag) is set

#### "Calibrated samples look wrong"

The ADC zero offset of **-28.0** is critical and hardware-specific to this FPGA. Without it, all waveform positions will be shifted by ~28 LSBs (~11% of full scale). This constant is hardcoded in VFP register s16 and must be applied to every raw sample before any other calibration.

### Essential Checklist for Replacement Firmware

Before expecting SPI3 data:

- [ ] AFIO remap done: `AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000`
- [ ] SPI3 clock enabled: `RCU_APB1EN |= 0x8000`
- [ ] SPI3 configured: Mode 3, Master, /2, 8-bit, MSB first, SW NSS
- [ ] SPI3 enabled: `SPI3_CTL0 |= 0x40`
- [ ] PC6 set HIGH: `GPIOC_BOP = (1 << 6)`
- [ ] PB11 set HIGH: `GPIOB_BOP = 0x800`
- [ ] USART2 configured: 9600 baud, 8N1
- [ ] Boot commands sent via USART2: 0x01, 0x02, 0x06, 0x07, 0x08
- [ ] SysTick delays executed between boot phases
- [ ] SPI3 handshake (command 0x05) completed
- [ ] TMR3 enabled and driving USART exchange
- [ ] IWDG initialized and being fed every ~50ms
- [ ] Acquisition trigger: send items to `spi3_data_queue` from timer callback
- [ ] Double-buffer: send TWO queue items per acquisition trigger
- [ ] ADC offset: subtract 28.0 from every raw sample

---

## Appendix A: FreeRTOS Queue Map

| Address | Name | Item Size | Depth | Purpose |
|---------|------|-----------|-------|---------|
| `0x20002D6C` | `usart_cmd_queue` | 1 byte | 20 | USART command dispatch |
| `0x20002D70` | `button_event_queue` | 1 byte | 15 | Button events from input scanning |
| `0x20002D74` | `usart_tx_queue` | 2 bytes | 10 | Commands to send to FPGA via USART |
| `0x20002D78` | `spi3_data_queue` | 1 byte | 15 | Triggers for SPI3 acquisition |
| `0x20002D7C` | `meter_semaphore` | 0 (binary) | 1 | Signals meter RX frame complete |
| `0x20002D80` | `fpga_semaphore_1` | 0 (binary) | 1 | SPI3 init synchronization |
| `0x20002D84` | `fpga_semaphore_2` | 0 (binary) | 1 | EXTI notification |

## Appendix B: Meter State Field Map

Key fields at offsets from `meter_state` base (`0x200000F8`):

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x04` | int8 | `ch1_dc_offset` | Channel 1 DC offset (signed) |
| `+0x05` | int8 | `ch2_dc_offset` | Channel 2 DC offset (signed) |
| `+0x10` | uint8 | `meter_sub_mode` | Meter sub-mode (0-7) for display format |
| `+0x14` | uint8 | `spi3_transfer_mode` | 0=idle/dummy, nonzero=active |
| `+0x16` | uint8 | `active_channel` | Active channel (0 or 1) |
| `+0x17` | uint8 | `hold_mode` | 0=running, 1=single, 2=hold |
| `+0x18` | uint8 | `trigger_mode` | Trigger configuration |
| `+0x1A` | int16 | `trigger_offset` | Signed trigger position offset |
| `+0x1C` | int16 | `voltage_range` | Voltage range encoding (signed) |
| `+0x2D` | uint8 | `timebase_index` | Timebase setting (0x00-0x13) |
| `+0x32` | uint8 | `acquisition_timer` | Acquisition counter (0xFF=disabled) |
| `+0x33` | uint8 | `calibration_mode` | 0=normal, nonzero=calibrating |
| `+0x46` | uint16 | `spi3_status_word` | SPI3 16-bit status readback |
| `+0x352` | uint8 | `ch1_probe_type` | CH1 probe (0/0xFF=none) |
| `+0x353` | uint8 | `ch2_probe_type` | CH2 probe (0/0xFF=none) |
| `+0x356` | uint8[301] | `roll_buf_ch1` | Roll mode circular buffer CH1 |
| `+0x483` | uint8[301] | `roll_buf_ch2` | Roll mode circular buffer CH2 |
| `+0x5B0` | uint8[1024] | `adc_buf_ch1` | ADC sample buffer CH1 |
| `+0x9B0` | uint8[1024] | `adc_buf_ch2` | ADC sample buffer CH2 |
| `+0xDB0` | uint8 | `spi3_frame_count` | SPI3 frame counter |
| `+0xDB4` | uint16 | `roll_read_ptr` | Roll mode read pointer |
| `+0xDB6` | uint16 | `roll_sample_count` | Roll mode count (max 300) |
| `+0xDB8` | uint16 | `acq_sample_count` | Acquisition sample counter |
| `+0xF2D` | uint8 | `meter_mode_state` | Meter mode (0-8) |
| `+0xF30` | uint32 | `meter_raw_value` | Raw meter measurement |
| `+0xF34` | uint8 | `meter_result_class` | 1=normal, 2=under, 3=over, 4=invalid |
| `+0xF35` | uint8 | `meter_data_valid` | Meter data validity flag |
| `+0xF36` | uint8 | `meter_overload_flag` | Overload state |
| `+0xF37` | uint8 | `meter_cal_coeff` | Calibration coefficient selector |
| `+0xF39` | uint8 | `auto_range_flag` | Auto-range enable |
| `+0xF3A` | uint16 | `meter_extra_data` | Extra from RX bytes [10:11] |
| `+0xF3C` | uint16 | `exchange_lock` | USART exchange lock (0=unlocked) |
| `+0xF5D` | uint8 | `continuity_buzzer` | 0xB0=off, 0xB1=on |
| `+0xF68` | uint8 | `mode_state` | System mode (1=active, 2-9=specific) |

## Appendix C: GPIO Pin Map

| Pin | Function | Direction | Config | Set During |
|-----|----------|-----------|--------|-----------|
| PA2 | USART2 TX | Output | AF push-pull | Phase 2 (step 9) |
| PA3 | USART2 RX | Input | Floating | Phase 2 (step 9) |
| PB3 | SPI3 SCK | Output | AF push-pull 50MHz | Phase 5 (step 18) |
| PB4 | SPI3 MISO | Input | Floating | Phase 5 (step 18) |
| PB5 | SPI3 MOSI | Output | AF push-pull 50MHz | Phase 5 (step 18) |
| PB6 | SPI3 CS | Output | GPIO push-pull | Phase 5 (step 18) |
| PB11 | FPGA active | Output | GPIO push-pull | Phase 8 (step 52) |
| PC6 | FPGA SPI enable | Output | GPIO push-pull | Phase 5 (step 19) |
| PC7 | Probe detect | Input | -- | Phase 1 |
| PC9 | Power hold | Output | GPIO push-pull | Phase 1 (step 2) |

## Appendix D: Source Files

This document was synthesized from the following reverse engineering artifacts:

| File | Contents |
|------|----------|
| `analysis_v120/FPGA_TASK_ANALYSIS.md` | SPI3 format, command table, 9-mode state machine, VFP calibration |
| `analysis_v120/fpga_task_annotated.c` | 580+ lines annotated C code, 10 sub-functions |
| `analysis_v120/FPGA_PROTOCOL.md` | USART frame format, mode dispatcher, command map |
| `analysis_v120/FPGA_BOOT_SEQUENCE.md` | 53-step boot timeline, GPIO pin map |
| `analysis_v120/gap_functions.md` | 16 new FPGA command codes from gap function analysis |
| `COVERAGE.md` | Subsystem coverage tracker, function inventory |
