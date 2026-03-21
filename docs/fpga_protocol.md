# FPGA Communication Protocol

## Correction: FUN_0802e7bc is FatFS Init, Not USART Handler

Our earlier analysis incorrectly identified FUN_0802e7bc as the USART2 handler. It's actually a **FatFS filesystem initialization function** that mounts volumes and checks file availability. The "string compare" calls we saw are filesystem operations (f_mount, f_open, etc.), and the string addresses (0x80bba1f, etc.) are filesystem paths in persistent flash.

## Dual Communication Paths to FPGA

The 2C53T uses **two separate interfaces** to communicate with the FPGA:

### 1. USART2 (0x40004400) — Command/Control Channel

**Real ISR:** FUN_080277b4 (not the vector table entry, which lands on a function epilogue)

**Protocol: Binary fixed-frame** (not text-based as initially assumed)

**TX Frame (MCU → FPGA): 10 bytes**
```
Byte[0-1]: Header (likely 0x5A/0xA5 or similar magic)
Byte[2]:   Command parameter high byte
Byte[3]:   Command parameter low byte (echoed back by FPGA for verification)
Byte[4-8]: Command data
Byte[9]:   Checksum: (param + (param >> 8)) & 0xFF
```

**RX Frame Type 1 — Acknowledgment: 2 bytes**
```
[0x5A, 0xA5]  — Sync/ACK from FPGA
```

**RX Frame Type 2 — Status Response: 10 bytes**
```
Byte[0]: 0xAA (header)
Byte[1]: 0x55 (header)
Byte[3]: Echo of TX Byte[3] (command verification)
Byte[7]: 0xAA (frame integrity marker)
```

**RX Frame Type 3 — Acquisition Complete: 12 bytes**
```
12-byte extended response indicating acquisition is done.
Triggers PendSV (0xE000ED04 = 0x10000000) for RTOS context switch
→ wakes the osc task to process captured data
```

**Validation:** The ISR checks that RX_buf[3] matches TX_buf[3] (echo verification) and that RX_buf[7] == 0xAA (integrity).

**Command codes observed** in DAT_20000025: 0x20 (standard), 0x21 (error/retry), 0x24 (special mode)

### 2. SPI2 (0x40003C00) — High-Speed Data Transfer

**Function:** 0x08037454 performs SPI2 transfers

**Registers accessed:**
- SPI2_STAT: 0x40003C08 (status register)
- SPI2_DATA: 0x40003C0C (data register)
- GPIO chip-select: 0x40010C10/0x40010C14 (GPIOB set/reset)

**Purpose:** Bulk ADC sample data acquisition from FPGA. After the FPGA is configured via USART2, the actual waveform sample data is transferred over SPI at high speed.

## Acquisition Pipeline

```
1. MCU sends 10-byte command via USART2
   (configure timebase, trigger level, channel settings)
        │
        ▼
2. FPGA acknowledges with [0x5A, 0xA5]
   or returns 10-byte status with [0xAA, 0x55, ...]
        │
        ▼
3. FPGA captures ADC samples at configured rate
        │
        ▼
4. MCU reads sample data via SPI2
   (bulk transfer, GPIO chip-select controlled)
        │
        ▼
5. FPGA sends 12-byte completion frame via USART2
   → triggers PendSV → wakes osc task
        │
        ▼
6. osc task processes waveform data
   → sends display commands to display task queue
```

## Memory Map for FPGA Communication

| Address | Size | Purpose |
|---|---|---|
| 0x20000005 | 10 bytes | USART2 TX buffer (command frame) |
| 0x2000000F | 1 byte | USART2 TX byte index |
| 0x20000018-0x2000002F | 24 bytes | FPGA command/status structure |
| 0x20000025 | 1 byte | Current FPGA command code |
| 0x20004E10 | 1 byte | USART2 RX byte index |
| 0x20004E11+ | 12 bytes | USART2 RX buffer |
| 0x20004E14 | 1 byte | RX echo verification byte |
| 0x20004E18 | 1 byte | RX integrity marker (must be 0xAA) |

## Updated Function Identifications

| Ghidra Address | Corrected Name | Description |
|---|---|---|
| FUN_0802e7bc | `fatfs_init` | FatFS filesystem mount and file check (NOT USART handler) |
| FUN_080277b4 | `usart2_isr` | Real USART2 interrupt service routine |
| FUN_0802d534 | `fatfs_read_write` | FatFS file read/write (NOT uart_print_string) |
| FUN_0802dc40 | `fatfs_open` | FatFS file open (NOT string_compare) |
| FUN_0802d8b8 | `fatfs_open_read` | FatFS open for reading (NOT string_copy_or_parse) |
| FUN_0802d80c | `fatfs_mount` | FatFS volume mount |
| FUN_0802cfbc | `fatfs_operation` | FatFS filesystem operation |

## Baud Rate

Not determined from the binary — the USART2 baud rate register (BRR at 0x40004408) is configured during boot initialization in a region not covered by the decompiled output. Would need to either:
- Probe the UART lines with a logic analyzer on the real device
- Decompile more functions from the 0x08004D60-0x08007000 region
- Check if the GD32 HAL library defaults are used (common: 115200, 921600, or 1000000 baud)

## Implications

1. **The FPGA protocol is binary, not text** — corrects our earlier assumption
2. **Dual interface** (UART for commands, SPI for data) is a clean design
3. **10-byte fixed frames** with checksums means the protocol is relatively simple to replicate
4. **The echo verification** (TX byte echoed in RX) provides built-in error detection
5. **For the emulator**: need to simulate both USART2 responses (ACK frames) and SPI2 data (sample buffers)
6. **For protocol decode modules**: the SPI2 path is where captured waveform data flows — protocol decoders would process this data
