# FNIRSI 2C53T — Stock USB IAP / FMC Flash Path Analysis

**Source binary:** `APP_2C53T_V1.2.0_251015.bin` (V1.2.0, flash base 0x08004000)
**Analysis date:** 2026-04-04
**Author:** RE subagent (Claude Sonnet 4.6) from Ghidra decompile + binary inspection

---

## 1. Executive Summary

The FNIRSI 2C53T stock application firmware implements a **USB Mass Storage Class (MSC)** interface using Bulk-Only Transport (BOT) with a SCSI transparent command set. There is **no stock in-application programming (IAP) path** through the USB interface in the application firmware — the stock app exposes the SPI flash (W25Q128) as a removable drive for screenshots and system files. The FMC (internal flash memory controller) is accessed in the stock app only for option-byte operations (setting EOPB0=0xFE to enable 224KB SRAM), not for reprogramming the application itself.

The "BOOTLOADER MODE" screen and the HID IAP protocol referenced in the project's CLAUDE.md and existing documentation belong entirely to **our custom bootloader** (`firmware/bootloader/`), which is a clean-room implementation using Artery's HID IAP SDK class. The stock firmware has no concept of a firmware upgrade path through the app — stock upgrades presumably go through the ROM DFU or a dedicated stock bootloader that lives below the app (separate binary, not analyzed here).

This document covers: (a) the stock USB MSC stack in the app firmware, (b) the FMC register operations found in the stock app, (c) our custom HID IAP bootloader in full detail, and (d) caseless-bridge feasibility.

---

## 2. USB Device Identity

### 2A. Stock Application Firmware — USB Mass Storage Class

The stock application exposes the SPI flash filesystem as a removable USB drive. The USB device descriptors are constructed dynamically at runtime and not stored as static byte arrays in the application flash. No VID/PID constant was found in the V1.2.0 binary through a raw binary search. The descriptor layout is inferred from the USB stack callback tables.

**Device class architecture:**
- Interface class: `0x08` (Mass Storage Class)
- Subclass: `0x06` (SCSI transparent command set)
- Protocol: `0x50` (Bulk-Only Transport)
- Endpoints: 2 bulk endpoints (EP1 IN/OUT)
- Max packet size: 64 bytes (bulk, USB Full Speed)

**Packet buffer SRAM layout (0x40006000):**
The endpoint buffer table is managed via `DAT_40005c50` (BTABLE offset register). The USB stack uses 16-bit packed accesses — buffer addresses in the BTABLE are byte-addresses shifted right by 1 (`addr * 2 + 0x40006000`). This is the standard STM32F1-compatible USB peripheral packing behavior shared by the AT32F403A.

**USB state block:**
- `DAT_20002B24`: USB device handle pointer (passed to all USB callbacks)
- `DAT_20002B28`: USB class callback table (10+ function pointers, set up by master_init)
- `DAT_20002B2C`: USB descriptor source table
- `DAT_40005C00–DAT_40005C1C`: USB_EPnR registers (EP0–EP7 endpoint registers)
- `DAT_40005C40`: USB_CNTR (control register)
- `DAT_40005C44`: USB_ISTR (interrupt status register)
- `DAT_40005C48`: USB_FNR (frame number)
- `DAT_40005C4C`: USB_DADDR (device address)
- `DAT_40005C50`: USB_BTABLE (buffer table base address in packet SRAM)

### 2B. Our Custom HID IAP Bootloader (firmware/bootloader/)

This is the relevant IAP interface for the caseless-bridge goal. VID/PID are defined in `hid_iap_desc.h`:

```
VID: 0x2E3C  (Artery Technology)
PID: 0xAF01  (HID IAP device)
bcdDevice: 0x0200
```

**Device Descriptor (18 bytes):**
```
12 01 00 02 00 00 00 40 3C 2E 01 AF 00 02 01 02 03 01
```
Decoded:
```
bLength          = 0x12 (18)
bDescriptorType  = 0x01 (Device)
bcdUSB           = 0x0200 (USB 2.0)
bDeviceClass     = 0x00 (per-interface)
bDeviceSubClass  = 0x00
bDeviceProtocol  = 0x00
bMaxPacketSize   = 0x40 (64)
idVendor         = 0x2E3C
idProduct        = 0xAF01
bcdDevice        = 0x0200
iManufacturer    = 1  -> "Artery"
iProduct         = 2  -> "HID IAP"
iSerialNumber    = 3  -> device-unique hex string
bNumConfigurations = 1
```

**Configuration Descriptor (41 bytes total):**
```
09 02 29 00 01 01 00 C0 32   <- Config desc (9 bytes)
09 04 00 00 02 03 00 00 00   <- Interface desc (9 bytes): class=HID, 2 EPs
09 21 10 01 00 01 22 20 00   <- HID desc (9 bytes): bcdHID=1.10, report=32B
07 05 81 03 40 00 01          <- EP1 IN: INTERRUPT, 64B, 1ms interval
07 05 01 03 40 00 01          <- EP1 OUT: INTERRUPT, 64B, 1ms interval
```

**HID Report Descriptor (32 bytes):**
```
06 FF 00   USAGE_PAGE (Vendor 0xFF00)
09 01      USAGE (Demo Kit)
A1 01      COLLECTION (Application)
  15 00    LOGICAL_MINIMUM (0)
  25 FF    LOGICAL_MAXIMUM (255)
  75 08    REPORT_SIZE (8 bits)
  95 40    REPORT_COUNT (64)
  09 01    USAGE
  81 02    INPUT (Data, Var, Abs)   <- 64-byte IN report (device→host)
  95 40    REPORT_COUNT (64)
  09 01    USAGE
  91 02    OUTPUT (Data, Var, Abs)  <- 64-byte OUT report (host→device)
  95 01    REPORT_COUNT (1)
  09 01    USAGE
  B1 02    FEATURE (Data, Var, Abs) <- 1-byte feature
C0         END_COLLECTION
```

**String Descriptors:**
```
String 0 (LANGID):     04 03 09 04  (US English)
String 1 (Mfr):        0E 03 41 00 72 00 74 00 65 00 72 00 79 00  ("Artery")
String 2 (Product):    10 03 48 00 49 00 44 00 20 00 49 00 41 00 50 00  ("HID IAP")
String 3 (Serial):     26 bytes (0x1A), device-unique hex from UID registers
                       UID1 = 0x1FFFF7E8, UID2 = 0x1FFFF7EC, UID3 = 0x1FFFF7F0
                       serial0 = UID1 + UID3; serial1 = UID2
                       8 hex digits from serial0, 4 from serial1
```

**Endpoints summary:**
| EP | Direction | Type | MaxPkt | Address |
|----|-----------|------|--------|---------|
| EP0 | Control | Control | 64B | 0x00/0x80 |
| EP1 | IN | Interrupt | 64B | 0x81 |
| EP1 | OUT | Interrupt | 64B | 0x01 |

---

## 3. Control Transfer Flow

### 3A. Stock USB MSC — EP0 State Machine (FUN_080278e4 @ 0x080278e4)

This function is the USB interrupt service handler, called from the USB full-speed interrupt vector (USBFS_L_CAN1_RX0_IRQHandler). It processes all USB events by reading `USB_ISTR` (at `DAT_40005C44`) and dispatching based on endpoint and direction flags.

**EP0 SETUP stage:**
When `USB_ISTR` indicates a SETUP transaction on EP0:
1. The 8-byte SETUP packet is read from packet buffer SRAM (via `0x40006000 + btable_ep0_rx_offset * 2`).
2. Stored into `DAT_20002D38–DAT_20002D3F`.
3. `DAT_20002D44` (EP0 state) is set to `0x01` (SETUP received).
4. `bmRequestType` byte dispatches to standard vs. class handlers.

**EP0 state machine values (`DAT_20002D44`):**
```
0x01 = SETUP received — dispatch to request handler
0x02 = DATA IN stage — sending response to host
0x03 = DATA OUT stage — receiving data from host (for SET_DESCRIPTOR etc.)
0x04 = STATUS stage — zero-length packet
0x05 = STALL — send STALL to signal error
```

**Standard request dispatch (bmRequestType bits [6:5] = 0b00, Standard):**

`bmRequestType[4:0]` = recipient + `bRequest` byte:
- `bRequest = 0x05` (SET_ADDRESS): Writes device address to `DAT_20002D4A` / `DAT_40005C4C`.
- `bRequest = 0x06` (GET_DESCRIPTOR): Calls descriptor handler, initiates DATA IN.
- `bRequest = 0x08` (GET_CONFIGURATION): Returns `DAT_20002D4D` (1 byte).
- `bRequest = 0x09` (SET_CONFIGURATION): Sets `DAT_20002D48` to 2 or 3, reconfigures endpoints.
- `bRequest = 0x00` (GET_STATUS): Returns 2-byte status bitfield.
- `bRequest = 0x01` (CLEAR_FEATURE): Clears endpoint halt bit.
- `bRequest = 0x03` (SET_FEATURE): Sets endpoint halt bit.

**Class request dispatch (bmRequestType bits [6:5] = 0b01, Class):**
Calls into the class-specific handler table at `DAT_20002B28[2]` with the SETUP packet pointer.

**Key EP0 register operations:**
```c
// Set EP0 TX to VALID (send data):
*(uint *)(&DAT_40005C00 + ep * 4) = (reg & 0xF3F) ^ 0x80A0;  // STAT_TX = VALID

// Set EP0 RX to VALID (ready to receive):
*(uint *)(&DAT_40005C00 + ep * 4) = (reg & 0x3F0F) ^ 0xB080;  // STAT_RX = VALID

// Set EP0 to STALL:
*(uint *)(&DAT_40005C00 + ep * 4) = (reg & 0x3F0F) ^ 0x9080;  // STAT_TX=STALL, STAT_RX=STALL
```

### 3B. Our HID IAP Bootloader — EP0/EP1 Flow

The Artery USB stack handles standard EP0 requests in `usbd_core.c`. Class requests (HID GET_REPORT, SET_REPORT, SET_IDLE, GET_IDLE, SET_PROTOCOL, GET_PROTOCOL) are handled by `hid_iap_class_handler`. The IAP data exchange uses EP1 OUT/IN interrupt transfers, not EP0 control transfers.

---

## 4. IAP Protocol Command Set

### 4A. Stock USB MSC — Not an IAP Protocol

The stock firmware does **not** implement a firmware IAP protocol. The USB interface provides a standard MSC drive. The "command set" is SCSI over BOT.

**MSC Bulk-Only Transport (BOT) frame format:**

CBW (Command Block Wrapper, 31 bytes, host→device):
```
[0:3]   dCBWSignature  = 0x43425355 ("USBC" little-endian)
[4:7]   dCBWTag        = host-assigned tag (echoed in CSW)
[8:11]  dCBWDataTransferLength = bytes expected in data stage
[12]    bmCBWFlags     = 0x00 (data-out) or 0x80 (data-in)
[13]    bCBWLUN        = 0x00 (single LUN)
[14]    bCBWCBLength   = CDB length (6 or 10)
[15:30] CBWCB          = SCSI CDB (up to 16 bytes)
```

CSW (Command Status Wrapper, 13 bytes, device→host):
```
[0:3]   dCSWSignature  = 0x53425355 ("USBS" little-endian)
[4:7]   dCSWTag        = echoes dCBWTag
[8:11]  dCSWDataResidue = bytes not transferred
[12]    bCSWStatus     = 0x00 (OK), 0x01 (FAILED), 0x02 (Phase Error)
```

**SCSI command dispatch (FUN_0802920c, switch on CBWCB[0]):**

| Opcode | SCSI Command | Response | Side Effects |
|--------|-------------|----------|--------------|
| 0x00 | TEST UNIT READY | No data (6-byte CDB) | Returns GOOD if media present |
| 0x03 | REQUEST SENSE | 18-byte sense data | Returns current sense key/ASC |
| 0x12 | INQUIRY | Up to 36 bytes | Returns device identification string; LUN0: `0x8029838` data, other LUNs: `0x804bce8` data |
| 0x1A | MODE SENSE(6) | 8-byte mode page | Returns write-protect and medium state |
| 0x1B | START/STOP UNIT | No data | Load/eject; no physical effect |
| 0x1E | PREVENT/ALLOW MEDIUM REMOVAL | No data | Sets locked flag in state |
| 0x23 | READ FORMAT CAPACITIES | 12-byte capacity list | Returns geometry (sectors, block size) |
| 0x25 | READ CAPACITY(10) | 8 bytes | Returns last LBA + block size (512) |
| 0x28 | READ(10) | Up to 4096 bytes/iteration | Reads from SPI flash; `FUN_0802f048(buf, addr, len)` |
| 0x2A | WRITE(10) | Host sends data | Writes to SPI flash; `FUN_0802f16c(buf, addr, len)` |
| 0x2F | VERIFY(10) | No data stage | Verifies; implementation may be stub |
| 0x5A | MODE SENSE(10) | Mode page data | 10-byte CDB variant of MODE SENSE |

The driver supports a single LUN (LUN 0). `DAT_20001066` controls SPI flash bank selection: if 0, a `+0x200000` offset is applied to the SPI flash address, otherwise base address is used directly.

**MSC Class-Specific Requests (EP0):**
```
bRequest = 0xFF (Bulk-Only Mass Storage Reset): resets BOT state machine, both endpoints to VALID
bRequest = 0xFE (Get Max LUN): returns 0x00 (one LUN)
```

These are handled in `FUN_0802a078` (labeled `fs_create_file` in the function map, but actually the MSC class request handler).

### 4B. Our HID IAP Bootloader — Command Protocol

Commands are exchanged as 64-byte HID reports. The host sends an OUT report (EP1 OUT), the device responds with an IN report (EP1 IN). Both directions use report size 64 bytes.

**Command frame format (host→device, 64 bytes):**
```
[0:1]  iap_cmd  = command code (big-endian uint16)
[2:N]  payload  = command-specific (see below)
[N+1:63] padding = 0x00
```

**Response frame format (device→host, 64 bytes):**
```
[0:1]  iap_cmd  = echo of command code (big-endian uint16)
[2:3]  result   = 0xFF00 = ACK, 0x00FF = NACK (big-endian uint16)
[4:63] data     = command-specific response data
```

**Command Table:**

| Code | Name | Host Payload | Device Response | Side Effects |
|------|------|-------------|-----------------|--------------|
| `0x5AA0` | `IAP_CMD_IDLE` | none | ACK | Resets IAP state, re-runs iap_init() |
| `0x5AA1` | `IAP_CMD_START` | none | ACK | Resets IAP state, ready to receive address |
| `0x5AA2` | `IAP_CMD_ADDR` | [2:5] = 32-bit address (big-endian) | ACK or NACK | Validates addr ≥ 0x08004000 and ≤ flash end. On first ADDR after START: erases upgrade flag sector. Erases the target sector. Sets `iap_address`. |
| `0x5AA3` | `IAP_CMD_DATA` | [2:3] = data_len, [4:4+len] = raw bytes | ACK when 1024B block full | Accumulates into 1024-byte FIFO. When FIFO full: unlocks flash, programs 256 words (1KB), locks flash. |
| `0x5AA4` | `IAP_CMD_FINISH` | none | ACK | Flushes partial FIFO (word-padded with 0xFF). Sets IAP upgrade flag at `flag_address`. |
| `0x5AA5` | `IAP_CMD_CRC` | [2:5] = start_address, [6:7] = size_in_KB | ACK + [4:7] = CRC32 result | Computes hardware CRC32 over the specified flash region (endian-converted). |
| `0x5AA6` | `IAP_CMD_JMP` | none | ACK, then reboot | Sets state=JMP_WAIT; after IN report is sent, sets state=JMP. iap_loop() then calls NVIC_SystemReset(). |
| `0x5AA7` | `IAP_CMD_GET` | none | ACK + [4:7] = app_address | Returns the configured application base address (0x08004000). |

**Typical firmware update sequence:**
```
1. Host sends IAP_CMD_IDLE       → device ACKs
2. Host sends IAP_CMD_START      → device ACKs, state=START
3. For each 1KB block at address A:
   a. Host sends IAP_CMD_ADDR [A]     → device erases sector if aligned, ACKs
   b. Host sends IAP_CMD_DATA [len][data] (multiple packets to fill 1024B)
      → device ACKs when 1KB FIFO full and programmed to flash
4. Host sends IAP_CMD_FINISH     → device flushes remainder, sets upgrade flag, ACKs
5. Host sends IAP_CMD_CRC [addr][size_kb] → device verifies and responds
6. Host sends IAP_CMD_JMP        → device ACKs, then NVIC_SystemReset()
7. Bootloader reboots, finds upgrade flag, jumps to 0x08004000
```

---

## 5. Flash Programming Sequence

### 5A. Stock Application — FMC Operations (Option Bytes Only)

The two FMC functions found in the stock application (`FUN_0802f3e4` and `FUN_0802f5ec`) are **option byte operations**, not application flash programming. They exist only to set EOPB0=0xFE at first boot (enabling 224KB SRAM from 96KB default).

**AT32F403A FMC Register Map:**
```
0x40022000 = FMC_PSR     (prefetch/wait state control)
0x40022004 = FMC_KEYR    (unlock key register, bank 0)
0x40022008 = FMC_OPTKEYR (unlock key for option bytes)
0x4002200C = FMC_STS0    (status register, bank 0)
0x40022010 = FMC_CTRL0   (control register, bank 0)
0x40022014 = FMC_ADDR0   (address register, bank 0)
0x4002201C = FMC_OBR     (option byte register — read-only reflection)
0x40022020 = FMC_WPR     (write protection register)
0x40022044 = FMC_KEYR2   (unlock key register, bank 1 / extended flash)
0x4002204C = FMC_STS1    (status register, bank 1)
0x40022050 = FMC_CTRL1   (control register, bank 1)
0x40022054 = FMC_ADDR1   (address register, bank 1)
```

**FMC_CTRL0 / FMC_CTRL1 bit definitions:**
```
Bit 0: PG    — program enable (halfword programming)
Bit 1: PER   — page erase enable
Bit 2: MER   — mass erase enable
Bit 4: OBPG  — option byte program enable
Bit 5: OBER  — option byte erase enable
Bit 6: STRT  — start operation (self-clearing)
Bit 7: LOCK  — lock (write 1 to lock; cleared by writing keys to KEYR)
Bit 9: ERRIE — erase error interrupt enable
Bit 10: EOPIE — end-of-program interrupt enable
```

**FMC_STS0 bit definitions:**
```
Bit 0: BSY  — busy (operation in progress)
Bit 2: WRPERR — write protection error
Bit 4: PGERR — programming error (address not erased)
Bit 5: EOP  — end of program (success flag)
```

**FUN_0802f3e4 @ 0x0802f3e4 — `fmc_write_option_bytes` (mislabeled as fmc_program_flash):**

This function unlocks the option byte registers and writes to the option byte area at 0x1FFFF800. It:
1. Writes `0xCDEF89AB` to `FMC_OPTKEYR` (0x40022008) — this is the SECOND key; the first key `0x45670123` must be written by the caller or the AT32 handles it differently.
2. Polls `FMC_CTRL0[9]` (bit 9 of 0x40022010) for busy-wait.
3. Sets `FMC_CTRL0 |= 0x60` — bits 5+6 = OBER+STRT = erase option bytes, start.
4. Checks `FMC_OBR[1]` (0x4002201C bit 1) to see if option bytes are all-0xFF (blank).
5. Handles two paths: (a) blank = set OBER only, (b) non-blank = apply user-data byte `uVar3 = 0xA5`.
6. Then sets `FMC_CTRL0 |= 0x10` (OBPG bit) and writes to `0x1FFFF800` (option byte address).
7. Polls `FMC_STS0[0]` (busy bit) with a 0x100000 retry loop.
8. Clears `FMC_CTRL0 bit 5` (OBER) or `bit 4` (OBPG) on completion.

**Caller context (FUN_0802cf7c @ 0x0802cf7c — `spi_flash_init_controller`):**
```c
// Called at startup if option bytes not already configured
if ((char)_DAT_1ffff810 == -2) return;  // 0xFE = already set, skip
_DAT_40022004 = 0xcdef89ab;   // Write key2 to FMC_KEYR (bank0 unlock)
_DAT_40022044 = 0xcdef89ab;   // Write key2 to FMC_KEYR2 (bank1 unlock)
FUN_0802f3e4();                // Erase+write option bytes area
FUN_0802f5ec();                // Write EOPB0=0xFE at 0x1FFFF810
// ... then initialize SPI flash controller ...
```

The guard `(char)_DAT_1ffff810 == -2` checks if `0x1FFFF810` (EOPB0) is already `0xFE`. After the write succeeds, re-reads will return `0xFE` and this code is skipped on future boots.

**FUN_0802f5ec @ 0x0802f5ec — `fmc_write_eopb0` (mislabeled as fmc_erase_page):**

Writes EOPB0=0xFE to enable 224KB SRAM:
1. Writes `0xCDEF89AB` to `FMC_OPTKEYR` (0x40022008).
2. Polls `FMC_CTRL0[9]` for busy.
3. Writes `0xFE` to `0x1FFFF810` (EOPB0 option byte address).
4. Polls `FMC_STS0[0]` (busy) with 0x100000 retry loop (8 iterations × 8 unrolled checks × ~100K outer loops).
5. Clears `FMC_CTRL0 bit 4` (OBPG) on completion.

### 5B. Our HID IAP Bootloader — Flash Programming

The bootloader uses the AT32F403A HAL for all flash operations (`at32f403a_lib`).

**Complete unlock sequence (two keys required):**
```c
void flash_unlock(void) {
    FMC->KEYR = 0x45670123;   // Key 1: write to FMC_KEYR (0x40022004)
    FMC->KEYR = 0xCDEF89AB;   // Key 2: write to FMC_KEYR (0x40022004)
    // Flash is now unlocked (LOCK bit cleared)
}
```

The stock app only writes key 2 because at startup the flash begins unlocked, or the function is called after key 1 has already been written elsewhere. Our bootloader writes both keys via the HAL.

**Page erase (`flash_sector_erase(address)`):**
```c
void flash_sector_erase(uint32_t address) {
    // Wait for BSY clear
    while (FMC->STS0 & FMC_STS_BSY);
    
    // Set PER bit, write target address, set STRT
    FMC->CTRL0 |= FMC_CTRL_PER;    // bit 1
    FMC->ADDR0  = address;
    FMC->CTRL0 |= FMC_CTRL_STRT;   // bit 6
    
    // Wait for BSY clear + EOP set
    while (FMC->STS0 & FMC_STS_BSY);
    FMC->STS0 = FMC_STS_EOP;       // Clear EOP by writing 1
    FMC->CTRL0 &= ~FMC_CTRL_PER;   // Clear PER
}
```

Page size: 2KB (0x800 bytes) for AT32F403A with 1MB flash. The IAP `iap_erase_sector()` checks alignment before erasing: it only erases if `address & (SECTOR_SIZE_2K - 1) == 0`.

**Word program (`flash_word_program(address, data)`):**
```c
void flash_word_program(uint32_t address, uint32_t data) {
    // Wait for BSY clear
    while (FMC->STS0 & FMC_STS_BSY);
    
    // Set PG bit
    FMC->CTRL0 |= FMC_CTRL_PG;     // bit 0
    
    // Write halfword at address (AT32 programs in halfwords = 2 bytes)
    // The HAL internally does two 16-bit writes for a 32-bit word
    *(volatile uint16_t *)address = (uint16_t)(data & 0xFFFF);
    while (FMC->STS0 & FMC_STS_BSY);
    *(volatile uint16_t *)(address + 2) = (uint16_t)(data >> 16);
    while (FMC->STS0 & FMC_STS_BSY);
    
    FMC->STS0 = FMC_STS_EOP;
    FMC->CTRL0 &= ~FMC_CTRL_PG;
}
```

**Lock sequence:**
```c
void flash_lock(void) {
    FMC->CTRL0 |= FMC_CTRL_LOCK;   // bit 7 — locks flash until next reset or KEYR write
}
```

**Address range accepted by IAP:**
- Minimum: `FLASH_APP_ADDRESS = 0x08004000` (start of application region)
- Maximum: `flash_end_address = 0x08004000 + flash_size` (top of 1MB flash = 0x08104000)
- Addresses below 0x08004000 (bootloader region) are explicitly rejected with NACK.
- The upgrade flag sector is at `0x08004000 - 0x800 = 0x08003800` (written by FINISH, erased by ADDR on first command after START).

**Extended flash (0x08080000):**
The AT32F403A 1MB device has a second bank starting at 0x08080000. Our bootloader's `iap_info.flash_end_address` extends to `0x08104000` based on `FLASH_SIZE_REG()`, so the IAP will accept writes to the extended flash region without modification. The HAL routes the address to FMC_CTRL1/FMC_ADDR1 automatically for addresses in the second bank.

---

## 6. "BOOTLOADER MODE" Entry Path

The "BOOTLOADER MODE" screen is part of **our custom bootloader**, not the stock firmware. It lives at 0x08000000–0x08003FFF. Entry occurs via the following paths (evaluated in order at every boot):

### Path 1: App-Requested Update (Normal Developer Flow)
The application writes RAM magic at `0x20037FE0 = 0xDEADBEEF`, then calls `NVIC_SystemReset()`. On the next boot, the bootloader finds the magic word and enters bootloader mode. In the stock firmware, Settings → Firmware Update would trigger this sequence.

**Call path in our custom app (`src/ui/meter_ui.c` or equivalent):**
```c
// User selects "Firmware Update" from settings menu
*((volatile uint32_t *)0x20037FE0) = 0xDEADBEEF;
NVIC_SystemReset();
```
The bootloader (`firmware/bootloader/src/main.c:check_ram_magic()`) clears the magic word and sets `enter_bootloader = 1`.

### Path 2: Boot Safety System (Crash Recovery)
A boot failure counter lives at `0x20037FDC`. The bootloader increments it before jumping to the app; the app must call `boot_validate()` within 5 seconds to clear it. After 3 consecutive crashes (IWDG fires each time without clearing the counter), the bootloader enters "SAFE MODE" — a variant of bootloader mode with a red warning screen.

### Path 3: Button Combo at Power-On
- POWER + PRM held simultaneously at boot: checked at 200ms after power-on.
- POWER alone held for 800ms: checked at 800ms.
Both cause `enter_bootloader = 1`.

### Path 4: No Valid App
If `flash_sector_erase` at `0x08003800` (the upgrade flag sector) contains anything other than `0x41544B38` (IAP_UPGRADE_COMPLETE_FLAG), the bootloader concludes no valid app is installed and enters bootloader mode automatically.

**In the stock firmware context:** The stock app has no concept of writing `0xDEADBEEF` to `0x20037FE0`. Stock firmware upgrades were presumably delivered via a separate upgrade utility that programs via the ROM DFU (BOOT0 pin) or via the stock's own mechanism.

---

## 7. Memory Layout Implications

**Flash layout:**
```
0x08000000  Bootloader (16KB, 8 × 2KB sectors)     — permanent, NEVER erased by IAP
0x08003800  Upgrade flag sector (2KB)               — erased by IAP on first ADDR command
0x08004000  Application (1008KB)                    — updated by IAP on ADDR+DATA commands
0x08080000  Extended flash bank (0x08080000–0x08100000) — part of app, accessible by IAP
```

**Bootloader protection:**
The IAP explicitly rejects any write to addresses below `0x08004000`:
```c
if (address < iap_info.app_address || address > iap_info.flash_end_address) {
    status = IAP_FAILED;
    result = IAP_NACK;
}
```
This means the bootloader cannot self-erase through the IAP protocol. There is no hardware write-protect (Option Byte WRPRT) set by default — the bootloader relies on the software check.

**Self-erase risk:** If the app somehow calls `flash_sector_erase(0x08000000)`, it would erase the bootloader. We currently do not enable OB write protection, which would be advisable for production. The bootloader does not set WRPRT bits for sectors 0–7.

**Vector table:** The application's vector table at `0x08004000` is protected because the IAP only writes pages, and the upgrade flag sector erase clears `0x08003800`, not `0x08004000`. The app's vector table is only overwritten when a new firmware image is flashed starting at its own base address.

---

## 8. Caseless Bridge Flash Plan

**Feasibility: HIGH** for our own `make flash` tool (already working). **MEDIUM** for impersonating the stock FNIRSI upgrade tool (requires knowing the stock's VID/PID and protocol, which remain unresolved since the stock app uses MSC for file access rather than firmware upgrades).

**Existing capability:** Our HID IAP bootloader is fully functional and `make flash` works today (closed-case updates via Settings → Firmware Update → USB HID IAP). This satisfies the caseless-bridge goal for users of our custom firmware.

**For a Python end-user flasher (no toolchain):** The minimum required implementation is:
1. Enumerate USB HID devices with `VID=0x2E3C, PID=0xAF01`.
2. Open the HID device and send/receive 64-byte reports.
3. Execute the IAP command sequence: IDLE → START → [ADDR + DATA × N] → FINISH → CRC → JMP.
4. The `scripts/` directory already has Python tooling; a `flash.py` using `hidapi` is straightforward.

**For impersonating the stock FNIRSI upgrade tool:** The stock application firmware (V1.2.0) exposes only USB MSC (SPI flash as a removable drive) — it has no firmware IAP capability. The stock firmware upgrade mechanism was probably: a separate stock bootloader binary (not included in the `APP_2C53T_V1.2.0` binary), accessed via DFU mode (BOOT0 pin), not through the Settings → Firmware Update path. Without the stock bootloader binary or the stock upgrade utility, we cannot determine the stock upgrade protocol's VID/PID or command set. The "caseless bridge" concept of booting through stock IAP is therefore not applicable — **there is no stock IAP in the app firmware to bridge through**.

**Minimum descriptors for Python flasher targeting our bootloader:**
```python
VENDOR_ID   = 0x2E3C
PRODUCT_ID  = 0xAF01
EP_IN       = 0x81   # Interrupt IN, 64 bytes
EP_OUT      = 0x01   # Interrupt OUT, 64 bytes
REPORT_SIZE = 64
```

---

## 9. Annotated Pseudocode

### 9A. `usb_endpoint_init_all` @ 0x08039990 (402 bytes)

This function initializes the USB endpoint state structures in RAM. It does NOT write to USB hardware registers directly — it sets the RAM-side shadow of endpoint state that is later used by `usb_endpoint_configure` to program the actual USB peripheral.

```c
// USB state RAM layout at 0x20002B30:
// Each endpoint has two 0x20-byte records: one for OUT direction, one for IN direction
// ep_out[n] starts at 0x20002B30 + n * 0x20
// ep_in[n]  starts at 0x20002C30 + n * 0x20
// Fields within each record:
//   [0]    = endpoint number (hardware EP register index)
//   [1]    = endpoint type (0=ctrl, 1=iso, 2=bulk, 3=interrupt)
//   [2]    = direction flag (1=IN, 0=OUT)
//   [3]    = enabled flag (0=disabled, 1=enabled)
//   [4:5]  = TX buffer offset in packet SRAM
//   [6:7]  = RX buffer offset in packet SRAM
//   [8:9]  = max packet size (default 64 = 0x40)
//   [10]   = double-buffer flag (1=double-buffered, used for isochronous)
//   [11]   = NAK/stall state
//   [12:13]= received byte count (IN direction)

void usb_endpoint_init_all(void) {
    // Initialize EP0 OUT (control)
    DAT_20002B30 = 0;       // ep_num = 0
    DAT_20002B32 = 1;       // ep_type = control? (actually maps to USB_EPT_CONTROL)
    DAT_20002B38 = 0;       // max_packet = 0 (set later by configure)
    DAT_20002B44 = 0;       // rx count = 0
    DAT_20002B3E = 0;       // tx remaining = 0
    DAT_20002B34 = 0;       // tx buffer offset = 0 (allocated later)

    // EP1 OUT
    DAT_20002B50 = 0x0101;  // ep_num=1, ep_type=1 (bulk)
    DAT_20002B52 = 1;       // direction = OUT
    DAT_20002B58 = 0;       // max_packet = 0
    DAT_20002B64 = 0;
    DAT_20002B5E = 0;
    DAT_20002B54 = 0;

    // EP2 OUT
    DAT_20002B70 = 0x0202;  // ep_num=2, ep_type=2 (bulk)
    // ... (pattern repeats for EP3–EP7 OUT)

    // EP0 IN (mirrored at 0x20002C30)
    DAT_20002C30 = 0;       // ep_num = 0
    DAT_20002C32 = 0;       // ep_type = 0 (control)
    DAT_20002C38 = 0;
    // ... etc.

    // EP1 IN through EP7 IN follow same pattern with ep_num/ep_type pairs:
    // 0x0101, 0x0202, 0x0303, 0x0404, 0x0505, 0x0606, 0x0707
    // These map to the physical endpoint numbers and transfer types.
}
```

Key insight: the `ep_type` field maps to USB_EPT bits as: 0=CONTROL (0x200), 1=ISOCHRONOUS (0), 2=BULK (0x600), 3=INTERRUPT (0x400). These are written into `USB_EPnR[9:8]` when `usb_endpoint_configure` is called.

### 9B. `usb_endpoint_configure` @ 0x08039874 (284 bytes)

Configures a specific USB endpoint in the hardware registers. Called by `usb_descriptor_handler` when SET_CONFIGURATION enables an interface.

```c
void usb_endpoint_configure(int usb_handle, uint param_ep_addr) {
    // param_ep_addr: 0x00-0x07 = OUT, 0x80-0x87 = IN
    // usb_handle + (ep_num * 0x20) selects the endpoint state record

    int ep_num = param_ep_addr & 0x7F;
    byte *ep_rec;

    if (param_ep_addr & 0x80) {
        ep_rec = usb_handle + 0x10C;    // IN endpoint record (offset from ep_in base)
    } else {
        ep_rec = usb_handle + 0x0C;     // OUT endpoint record
    }

    // ep_rec[10] = double-buffer flag
    if (ep_rec[10] == 0) {
        // Single-buffer endpoint
        uint ep_hw_num = ep_rec[0];     // hardware EP register index
        uint ep_type   = ep_rec[2];     // transfer type

        if (ep_type == 1) {
            // IN endpoint
            if (USB_EPnR[ep_hw_num] & EP_STAT_TX_STALL) {
                // Toggle TX status to NAK
                USB_EPnR[ep_hw_num] = (USB_EPnR[ep_hw_num] & 0xF0F) | 0x80C0;
            }
            USB_EPnR[ep_hw_num] = (USB_EPnR[ep_hw_num] & 0xF3F) | 0x8080;
            // Set type bits
        } else {
            // OUT endpoint
            if (USB_EPnR[ep_hw_num] & EP_STAT_RX_STALL) {
                USB_EPnR[ep_hw_num] = (USB_EPnR[ep_hw_num] & 0xF0F) | 0xC080;
            }
            USB_EPnR[ep_hw_num] = (USB_EPnR[ep_hw_num] & 0x3F0F) | 0x8080;
        }
        USB_EPnR[ep_hw_num] = (USB_EPnR[ep_hw_num] & 0xF3F) | 0x8080;
    } else {
        // Double-buffer endpoint (isochronous): configure both TX and RX BDT entries
        // ... sets BTABLE entries for both buffers, enables STAT_RX/TX to VALID/DISABLED
    }
}
```

**USB_EPnR toggle-bit protocol:** The AT32F403A USB peripheral (STM32F1-compatible) uses toggle bits for STAT_RX and STAT_TX fields in USB_EPnR. Writing a 1 to a bit that is already 1 clears it; writing 0 leaves it unchanged. The masking patterns observed:
- `0xF3F` = keep EP_TYPE, EP_KIND, DTOG_TX, CTR_TX; zero STAT_TX
- `0x3F0F` = keep EP_TYPE, EP_KIND, DTOG_RX, CTR_RX; zero STAT_RX
- `0x8080` = set CTR_RX=0 (write-0 to clear) and CTR_TX=0
- `0x80A0` = set STAT_TX bits to VALID (10 XOR current)
- `0xB080` = set STAT_RX bits to VALID
- `0x9080` = set STAT_TX to STALL
- `0x8090` = set STAT_TX to NAK

### 9C. `usb_descriptor_handler` @ 0x08039b24 (908 bytes)

Configures a USB endpoint from its descriptor record, called during SET_CONFIGURATION processing. Also allocates packet buffer SRAM for the endpoint.

```c
void usb_descriptor_handler(int usb_handle, uint ep_addr, byte ep_enabled) {
    // ep_addr: full endpoint address (0x00-0x07 OUT, 0x80-0x87 IN)
    // ep_enabled: 0=disable/reset, 1=enable/configure, 2=bulk, 3=interrupt

    int ep_num = ep_addr & 0x7F;
    byte *ep_rec;

    if (ep_addr & 0x80) {
        // IN endpoint
        ep_rec = usb_handle + 0x10C + ep_num * 0x20;  // actually: base + ep_num * 0x20 + 0x10C offset
        *(uint16_t *)(usb_handle + 0x10E) = 0;        // clear IN transfer flag
    } else {
        // OUT endpoint
        ep_rec = usb_handle + 0x0C + ep_num * 0x20;
        *(uint16_t *)(usb_handle + 0x10E) = 1;        // set OUT flag
    }

    // Set defaults
    ep_rec[8] = 0x40;   // max_packet = 64
    ep_rec[9] = 0;
    ep_rec[3] = ep_enabled;

    // Allocate packet buffer SRAM if not already allocated
    if (ep_rec[10] == 0) {
        // Single buffer: allocate TX buffer
        if (ep_rec[4:5] == 0) {  // tx_offset not yet set
            ep_rec[4:5] = DAT_20000010;        // current SRAM allocator pointer
            DAT_20000010 += 0x40;              // advance by 64 bytes (one packet buffer)
        }
    } else {
        // Double buffer: allocate both TX and RX buffers
        if (ep_rec[4:5] == 0) {
            ep_rec[4:5] = DAT_20000010;
            DAT_20000010 += 0x40;
        }
        if (ep_rec[6:7] == 0) {
            ep_rec[6:7] = DAT_20000010;
            DAT_20000010 += 0x40;
        }
    }

    // Program hardware USB_EPnR register
    uint hw_ep = ep_rec[0];
    uint type_bits;
    switch (ep_enabled) {
        case 0:  type_bits = 0x200; break;   // CONTROL
        case 2:  type_bits = 0x600; break;   // BULK
        case 3:  ep_rec[10] = 1;             // double-buffer for interrupt?
                 type_bits = 0x400; break;   // INTERRUPT
        default: type_bits = 0; break;       // ISOCHRONOUS
    }

    // Write endpoint number and type to hardware register
    USB_EPnR[hw_ep] = (USB_EPnR[hw_ep] & 0x8F80) | ep_rec[1];  // ep_num field
    USB_EPnR[hw_ep] = type_bits | (USB_EPnR[hw_ep] & 0x898F);

    if (ep_rec[2] == 1) {   // IN endpoint
        // Write TX buffer address to BTABLE
        BTABLE[hw_ep].TXaddr = ep_rec[4:5];

        if (ep_rec[10] != 0) {
            // Double buffer: also write RX addr, set STAT to VALID/DISABLED
            BTABLE[hw_ep].RXaddr = ep_rec[6:7];
            USB_EPnR[hw_ep] = set_STAT_TX_VALID(USB_EPnR[hw_ep]);
            USB_EPnR[hw_ep] = set_STAT_RX_DISABLED(USB_EPnR[hw_ep]);
            USB_EPnR[hw_ep] = set_EP_KIND(USB_EPnR[hw_ep]);  // double-buffer mode
            USB_EPnR[hw_ep] = set_STAT_TX_to_NAK_then_VALID;
            return;
        }
        // Single buffer IN: set STAT_TX to VALID
        USB_EPnR[hw_ep] = (USB_EPnR[hw_ep] & 0xF3F) ^ 0x80A0;   // STAT_TX=VALID

    } else {   // OUT endpoint
        // Write RX buffer address and compute block size field
        BTABLE[hw_ep].RXaddr = ep_rec[6:7];
        uint blk_size;
        if (ep_rec[8:9] < 0x3F) {
            blk_size = ((~ep_rec[8:9] & 1) + (ep_rec[8:9] >> 1)) * 0x400;
        } else {
            blk_size = (ep_rec[8:9] >> 5) * 0x400 | 0x8000;
        }
        BTABLE[hw_ep].RXcount = blk_size;

        if (ep_rec[10] != 0) {
            // Double buffer OUT...
            BTABLE[hw_ep].TXaddr = ep_rec[4:5];
            // ... similar block size for TX path
        }
        // Set STAT_RX=VALID
        USB_EPnR[hw_ep] = (USB_EPnR[hw_ep] & 0x3F0F) ^ 0xB080;
    }
}
```

**BTABLE access note:** The BTABLE descriptors live in packet buffer SRAM (0x40006000). Because the USB packet SRAM uses 16-bit cells accessed via 32-bit bus, each 16-bit value occupies 4 bytes of address space. To convert a BTABLE offset (from `USB_BTABLE` register) to a real address:
```
real_addr = 0x40006000 + (USB_BTABLE + ep * 8) * 2
```
Writes use: `*(uint *)(btable_base + ep_offset * 2 + field_offset) = value;`

### 9D. `usb_endpoint_handler` @ 0x080278e4 (2566 bytes)

The main USB interrupt handler. Called from the USBFS interrupt vector each time the USB peripheral generates an interrupt.

```c
void usb_endpoint_handler(void) {
    int usb_handle = DAT_20002B24;
    uint istr = USB_ISTR;                // Read interrupt status
    uint ep_active = USB_ISTR >> 16;     // High word: which EPs are active

    // Process all pending endpoint events (may be multiple)
    while (USB_ISTR & USB_ISTR_EP_PENDING) {
        uint ep_flags = USB_ISTR;
        uint ep_num = ep_flags & 0xF;

        uint ep_reg = USB_EPnR[ep_num];

        if (ep_reg & EP_CTR_TX) {
            // IN transfer complete (device→host)
            USB_EPnR[ep_num] &= 0x8F0F;   // clear CTR_TX, keep toggled bits
            uint bytes_sent = BTABLE[ep_num].TXcount & 0x3FF;
            // ...accumulate bytes_sent into ep_state.tx_total...

            if (ep_num == 0) {
                // EP0 IN complete
                if (ep0_state == 0x02) {
                    // More data to send?
                    if (tx_remaining < tx_total) {
                        // Send next chunk via FUN_08039eb4
                    } else if (tx_total == rx_total) {
                        // Transfer complete — trigger SET_CONFIGURATION callback
                        callbacks[3](usb_handle);  // setup_complete callback
                        ep0_state = 0x05;          // go to STATUS stage
                        USB_CNTR &= ~(SRAM_ADDR_BITS);
                        USB_EPnR[0] = set_RX_VALID;
                    } else {
                        // Error
                        callbacks[3](usb_handle);
                        ep0_state = 0x05;
                    }
                }
            } else {
                // Non-zero EP IN complete: notify upper layer if ep_type==3 (interrupt)
                if (ep_remaining_bytes == 0) {
                    callbacks[5](usb_handle, ep_num);  // IN_complete callback for EP
                }
            }

        } else if (ep_reg & EP_CTR_RX) {
            // OUT transfer complete (host→device)
            USB_EPnR[ep_num] &= 0xF8F;   // clear CTR_RX

            uint bytes_rcvd = BTABLE[ep_num].RXcount & 0x3FF;
            // Copy from packet SRAM to RAM buffer:
            // src = 0x40006000 + BTABLE[ep_num].RXaddr * 2
            // dst = ep_state.rx_buf_ptr
            // Copy in 16-bit chunks (note: packet SRAM is 16-bit word at every 4 bytes)
            for (int i = 0; i < (bytes_rcvd+1)/2; i++) {
                rx_buf[i] = *(uint16_t *)(0x40006000 + BTABLE[ep_num].RXaddr * 2 + i * 4);
            }

            if (ep_num == 0) {
                // EP0 OUT — could be SETUP or DATA OUT
                // SETUP: ep_reg & EP_SETUP set
                if (ep0_state == 0x03) {
                    // DATA OUT stage
                    // Check if all expected data received
                    if (rx_total >= data_expected) {
                        callbacks[4](usb_handle);   // data_out_complete callback
                        ep0_state = 0x04;            // STATUS stage
                        FUN_08039eb4(usb_handle, 0, 0, 0);  // send ZLP for STATUS
                    }
                } else {
                    // SETUP packet received on EP0 OUT
                    // Parse bmRequestType, bRequest, wValue, wIndex, wLength
                    ep0_state = 0x01;
                    parse_setup_packet();
                }
            } else {
                // Non-zero EP OUT complete
                callbacks[6](usb_handle, ep_num);  // OUT_complete callback
            }
        }
        // Re-read ISTR for next pending event
    }

    // Check for USB reset
    if (USB_ISTR & USB_ISTR_RESET) {
        USB_ISTR = ~USB_ISTR_RESET;       // clear reset
        _DAT_20000010 = 0x40;             // reset SRAM allocator (EP0 uses first 64 bytes)
        usb_endpoint_init_all();          // reset all EP state
        usb_descriptor_handler(usb_handle, 0, 0);    // configure EP0 OUT
        usb_descriptor_handler(usb_handle, 0x80, 0); // configure EP0 IN
        USB_DADDR = 0x80;                 // enable USB with address 0 (device attached)
        DAT_20002D48 = 1;                 // device_state = DEFAULT
        callbacks[8](usb_handle, 1);     // reset callback
    }

    // Check for USB suspend
    if (USB_ISTR & USB_ISTR_SUSP) {
        USB_ISTR = ~USB_ISTR_SUSP;
        callbacks[7](usb_handle);        // suspend callback
    }

    // Check for USB wakeup
    if (USB_ISTR & USB_ISTR_WKUP) {
        USB_ISTR = ~USB_ISTR_WKUP;
        // Restore CNTR from suspend state
        USB_CNTR &= ~(FSUSP | LPMODE);
        DAT_20002D48 = DAT_20002D49;     // restore saved device state
        callbacks[8](usb_handle, 3);     // wakeup callback
    }
}
```

### 9E. `usb_class_request_handler` (SCSI dispatch) @ 0x0802920c (1396 bytes)

This is the SCSI command dispatch function for the USB MSC driver. Despite the name in the task description, it is actually the SCSI CDB processor, called from the BOT data stage handler after receiving a CBW.

```c
// pcVar16 = pointer to MSC state block (contains CBW parse results)
// pcVar16[0x102f] = SCSI command opcode (CBWCB[0])
// pcVar16[0x1028] = dCBWDataTransferLength (from CBW)
// pcVar16[0x102d] = LUN number (bCBWLUN)

undefined4 scsi_dispatch(int usb_handle) {
    char *msc_state = *(usb_handle + 4)->msc_state;  // MSC context block
    uint lun = msc_state[0x102d];
    uint transfer_len = *(uint *)(msc_state + 0x1028);
    uint scsi_opcode = msc_state[0x102f];

    if (scsi_opcode >= 0x5B) return 1;  // unknown command → error

    switch (scsi_opcode) {
    case 0x00:  // TEST UNIT READY
        if (transfer_len == 0) {
            msc_state[0x18:0x1B] = {0,0,0,0};   // no data
            return 0;   // GOOD
        } else {
            send_sense(ILLEGAL_REQUEST);
            return 1;
        }

    case 0x03:  // REQUEST SENSE
        // Copy REQUEST_SENSE data from current sense state into response buffer
        msc_state[0x2D] = DAT_20000029;  // sense key
        *(uint *)(msc_state + 0x25) = _DAT_20000021;
        *(uint *)(msc_state + 0x29) = _DAT_20000025;
        *(uint *)(msc_state + 0x21) = _DAT_2000001d;
        msc_state[0x1C] = DAT_20000018;
        // Limit response length to 18 (0x12)
        uint len = min(transfer_len, 0x12);
        *(uint *)(msc_state + 0x18) = len;
        return 0;

    case 0x12:  // INQUIRY
        // Source data: LUN0 → flash addr 0x8029838; other → 0x804BCE8
        uint inquiry_src = (msc_state[0x102D] == 0) ? 0x08029838 : 0x0804BCE8;
        uint len = min(transfer_len, 0x24);  // standard 36-byte INQUIRY data
        *(uint *)(msc_state + 0x18) = len;
        // Copy 'len' bytes from inquiry_src to msc_state+0x1C
        memcpy(msc_state + 0x1C, inquiry_src, len);
        return 0;

    case 0x1A:  // MODE SENSE(6)
        msc_state[0x18] = 8;    // response length = 8 bytes
        msc_state[0x19:0x1B] = {0,0,0};
        msc_state[0x1C:0x23] = {0x03,0,0,0, 0,0,0,0};  // mode data header
        return 0;

    case 0x1B:  // START/STOP UNIT
        msc_state[0x18:0x1B] = {0,0,0,0};   // no data stage
        return 0;

    case 0x1E:  // PREVENT/ALLOW MEDIUM REMOVAL
        msc_state[0x18:0x1B] = {0,0,0,0};
        return 0;

    case 0x23:  // READ FORMAT CAPACITIES
        // Build capacity list descriptor
        // Checks LUN, reads geometry from LUN state table at msc_state + lun*4 + 8
        // Response: 12 bytes (4-byte header + 8-byte current max capacity descriptor)
        // Format: {0,0,0,8, sectors_MSB...sectors_LSB, 0x02, block_size[2:0]}
        ...
        return 0;

    case 0x25:  // READ CAPACITY(10)
        // Returns last LBA (big-endian) + block size (512 = 0x200)
        // Last LBA = (total_sectors - 1)
        // Gets sector count from LUN state at msc_state + lun*4 + 8
        ...
        return 0;

    case 0x28:  // READ(10)
        // Extracts LBA from CBWCB[2:5] (big-endian), transfer_len from CBWCB[7:8]
        // Validates: LBA < media_sectors, doesn't exceed media bounds
        // On valid: sets msc_state read pointer, initiates bulk IN transfer
        // Reads 4KB blocks from SPI flash via FUN_0802f048()
        // If LUN==0 and flag not set: adds 0x200000 to SPI flash address
        ...
        return 0;

    case 0x2A:  // WRITE(10)
        // Extracts LBA from CBWCB[2:5], transfer_len from CBWCB[7:8]
        // Validates address range
        // Initiates bulk OUT transfer; data written to SPI flash via FUN_0802f16c()
        ...
        return 0;

    case 0x2F:  // VERIFY(10)
        // Similar to READ but doesn't return data; just validates
        ...
        return 0;

    case 0x5A:  // MODE SENSE(10)
        // 10-byte CDB variant; otherwise same as MODE SENSE(6)
        ...
        return 0;

    default:
        // Unknown SCSI command: set ILLEGAL REQUEST sense
        send_sense(ILLEGAL_REQUEST);
        return 1;
    }
}
```

### 9F. `fmc_write_option_bytes` @ 0x0802f3e4 (518 bytes)

Labeled `fmc_program_flash` in the function map but is actually an option byte writer:

```c
void fmc_write_option_bytes(void) {
    // Write second unlock key to OPTKEYR
    FMC_OPTKEYR = 0xCDEF89AB;  // @0x40022008

    // Wait for flash busy to clear
    while (FMC_CTRL0[9] != 0) {    // bit 9 of 0x40022010
        if (FMC_CTRL0[9] == 0) break;
    }

    undefined2 user_data = 0;

    // Set OBER+STRT bits to erase option bytes
    FMC_CTRL0 |= 0x60;  // bits 5+6: OBER=1, STRT=1

    // Check if option bytes are blank (FMC_OBR bit 1 = 0 = not blank)
    if ((FMC_OBR & 2) == 0) {
        user_data = 0xA5;   // non-blank: use default user data byte
    }

    // Check FMC_STAT for busy (bit 0) = operation completed?
    if ((FMC_STS0 & 1) == 0) {
        // Check error bits
        if (FMC_STS0 & 0x04) {    // bit 2 = WRPERR
            iVar1 = 1;
        } else {
            iVar1 = 2;
            if (FMC_STS0 & 0x10) iVar1 = 3;  // bit 4 = PGERR → iVar1=3 else 2
        }

        // Clear OBER, set OBPG to program
        FMC_CTRL0 &= ~0x20;    // clear OBER
        if (iVar1 == 3 || iVar1 == 2) {
            return;  // error — don't proceed
        }
        FMC_CTRL0 |= 0x10;    // set OBPG (option byte program)
        FMC_OB_DATA0 = user_data;  // write to 0x1FFFF800

        // Busy-wait loop (0x100000 iterations × 8 checks per iteration)
        for (int retry = 0x100000; retry > 0; retry--) {
            if ((FMC_STS0 & 1) == 0) goto done;
            if ((FMC_STS0 & 1) == 0) goto done;
            // ... (8 identical checks per loop iteration — unrolled)
        }
    } else {
        // Long timeout loop for initial erase
        for (int retry = 0x40000000; retry > 0; ) {
            if ((FMC_STS0 & 1) == 0) {
                retry = check_error_bits();
                if (retry != 1) goto FMC_CTRL0_clear;
                retry = 0xFFFFFFDF;   // error indicator
                goto clear_and_return;
            }
            retry -= 16;  // decrement by 16 unrolled iterations
        }
    }

done:
FMC_CTRL0_clear:
    FMC_CTRL0 &= ~0x20;   // clear OBER or OBPG
    return;

clear_and_return:
    FMC_CTRL0 &= 0xFFFFFFDF;   // clear bit 5
    return;
}
```

### 9G. `fmc_write_eopb0` @ 0x0802f5ec (236 bytes)

Labeled `fmc_erase_page` in the function map but writes EOPB0:

```c
void fmc_write_eopb0(void) {
    // Write second unlock key to OPTKEYR
    FMC_OPTKEYR = 0xCDEF89AB;  // @0x40022008

    // Wait for flash controller not busy
    while (FMC_CTRL0[9] != 0) {
        if (FMC_CTRL0[9] == 0) break;
    }

    // Write 0xFE to EOPB0 option byte (enables 224KB SRAM)
    *(uint8_t *)0x1FFFF810 = 0xFE;

    // Busy-wait: up to 0x100000 * 8 = 8M iterations
    if ((FMC_STS0 & 1) != 0) {
        for (int retry = 0x100000; retry < 0; retry += 0x10) {
            if ((FMC_STS0 & 1) == 0) break;
            // 8 unrolled identical checks...
            if ((FMC_STS0 & 1) == 0) break;
        }
    }

    // Clear OBPG bit (bit 4) in FMC_CTRL0
    FMC_CTRL0 &= ~0x10;
    return;
}
```

---

## 10. Open Questions

1. **Stock bootloader binary missing.** The stock firmware upgrade flow — whatever it uses — is implemented in a separate bootloader binary that lives below `0x08004000`. This binary was not provided for analysis. Without it, the stock IAP protocol (if any) remains unknown.

2. **Stock VID/PID unresolved.** USB descriptors in the V1.2.0 stock application are constructed dynamically at runtime. No VID/PID constants are stored as static data in the binary. The VID/PID for the stock MSC drive are not known without USB trace capture or HAL header inspection.

3. **FMC first unlock key.** The stock app only writes `0xCDEF89AB` (second key) to `FMC_KEYR` before calling the option byte functions. The first key `0x45670123` is never written in the decompile. Possibilities: (a) the flash is already unlocked at the point these functions are called; (b) the keys are encoded differently in Thumb2 and the decompile missed the first write; (c) the AT32F403A only requires the second key for option byte operations (unlikely per datasheet). This needs verification on hardware.

4. **SCSI INQUIRY data.** The data at `0x08029838` and `0x0804BCE8` (standard INQUIRY response for LUN 0 and other LUNs) was not extracted. These contain the USB drive's "vendor/product/revision" strings visible when the device is mounted.

5. **LUN geometry.** The total sector count and block size for the SPI flash virtual drive depend on the LUN state table at `msc_state + lun*4 + 8`. This is likely initialized from SPI flash filesystem metadata at startup but was not traced.

6. **`FUN_08028b80` identity.** This function (listed as "USB class request handler" in the task description) is actually an FFT algorithm (radix-8 butterfly operations with floating-point coefficients from `DAT_0806e0d0`). It is called from the scope FFT pipeline, not from USB code. The mislabeling in the task description is an error — there is no function at `0x08028b80` that handles USB class requests.

7. **`DAT_20002b28` callback table population.** The function pointers in the USB callback table are written during `master_init` but not traced in the available decompile slices. The full callback assignments would clarify which functions handle SET_CONFIGURATION, data-out completion, and MSC-specific events.

8. **Option byte unlock key sequence.** The AT32F403A reference manual specifies both keys must be written in sequence to unlock FMC_OPTKEYR. Why the decompile shows only the second key needs hardware verification.

---

## Appendix: Key Addresses Quick Reference

| Address | Component | Description |
|---------|-----------|-------------|
| `0x40005C00` | USB_EPnR[0] | USB endpoint 0 register (EP0) |
| `0x40005C40` | USB_CNTR | USB control register |
| `0x40005C44` | USB_ISTR | USB interrupt status register |
| `0x40005C48` | USB_FNR | USB frame number register |
| `0x40005C4C` | USB_DADDR | USB device address register |
| `0x40005C50` | USB_BTABLE | USB buffer table offset register |
| `0x40006000` | Packet SRAM | USB packet buffer (512 bytes, 16-bit packed) |
| `0x40022004` | FMC_KEYR | Flash key register (write key1 then key2 to unlock) |
| `0x40022008` | FMC_OPTKEYR | Option byte key register |
| `0x4002200C` | FMC_STS0 | Flash status register (bank 0) |
| `0x40022010` | FMC_CTRL0 | Flash control register (bank 0) |
| `0x40022044` | FMC_KEYR2 | Flash key register (bank 1) |
| `0x1FFFF800` | OB.RDP | Option byte: read protection |
| `0x1FFFF810` | OB.EOPB0 | Option byte: EOPB0 (SRAM size) — set to 0xFE for 224KB |
| `0x20002B24` | USB handle | USB device handle pointer (RAM) |
| `0x20002B28` | Callbacks | USB class callback table (RAM, 10+ function pointers) |
| `0x20002D30` | SETUP buffer | EP0 SETUP packet staging area (8 bytes) |
| `0x20002D44` | EP0 state | EP0 state machine variable |
| `0x20002D48` | device_state | USB device state (1=DEFAULT, 2=ADDRESS, 3=CONFIGURED) |
| `0x20037FDC` | boot_counter | Boot failure counter (our bootloader) |
| `0x20037FE0` | dfu_magic | DFU RAM magic trigger (0xDEADBEEF) |
| `0x08000000` | Bootloader | Our custom HID IAP bootloader (16KB) |
| `0x08003800` | Upgrade flag | IAP upgrade flag sector (2KB) |
| `0x08004000` | App base | Application start (our firmware) |
| `0x08029838` | INQUIRY data | SCSI INQUIRY response for LUN 0 (in flash) |
| `0x0804BCE8` | INQUIRY data | SCSI INQUIRY response for non-zero LUNs |

---

*End of analysis. File: `reverse_engineering/analysis_v120/stock_iap_bootloader.md`*
