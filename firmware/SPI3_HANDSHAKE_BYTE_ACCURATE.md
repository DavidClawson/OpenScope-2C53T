# SPI3 Handshake & H2 Upload — Byte-Accurate Disassembly Decode

**Date:** 2026-04-09
**Source:** ripcord raw disassembly of `FUN_08027a50` in stock V1.2.0
**Method:** `arm-none-eabi-objdump` on raw binary, cross-referenced with
ripcord's `peripheral_xrefs` table (206 SPI3 xrefs in this function)
**Supersedes:** `SPI3_INIT_SEQUENCE_DECODED.md` — that document was based
on Ghidra decompiler output which **lost all SPI3_DT write values**.

## Critical Correction

The earlier analysis (from decompiled pseudo-C) stated:

> "The stock firmware does NOT write explicit data to SPI3_DT during
> these 4 exchanges — it just polls TXE and RXNE."

**This is wrong.** The Ghidra decompiler optimized away the volatile
register writes. The raw Thumb-2 instructions prove the firmware writes
specific command bytes to SPI3_DT at every handshake step. The exact
byte sequence is documented below.

## Address Correction

The address range `0x08026774–0x08026B30` (cited in the original request)
is **not the SPI3 handshake**. That range falls inside `FUN_080263bc`
(0x080263BC–0x08026D40), which is a **flash settings save** function
(USB disconnect → serialize device settings → flash erase/program →
cleanup). It touches USBFS, FLASH, TMR10/11, and GPIOC — zero SPI3.

The actual SPI3 code lives in `FUN_08027a50` (0x08027A50–0x0802B842),
a 15,346-byte / 452-basic-block system init function. SPI3 register
accesses span **0x0802A5E8–0x0802ADC6**.

## Register Setup

```
r6 = 0x40003C08  (SPI3_STS)
  [r6, #0] = SPI3_STS  (0x40003C08) — status register
  [r6, #4] = SPI3_DT   (0x40003C0C) — data register (TX write / RX read)

r4 = 0x40010C00  (GPIOB base)
  [r4, lr] = GPIOB_BRR  (0x40010C14) — bit reset → PB6 LOW  = CS ASSERT
  [r4, ip] = GPIOB_BSRR (0x40010C10) — bit set   → PB6 HIGH = CS DEASSERT
```

## SPI3 Exchange Pattern (every transfer)

Each SPI3 byte exchange follows this exact instruction sequence:

```arm
    ; --- Poll TXE (TX buffer empty, STS bit 1) ---
    ldr   r0, [r6, #0]         ; read SPI3_STS
    lsls  r0, r0, #30          ; shift bit 1 to sign position
    ; (IT block: up to 3 more speculative reads if still not ready)
    bmi.n <ready>              ; branch if TXE set
    ldr   r0, [r6, #0]         ; retry
    lsls  r0, r0, #30
    bpl.n <poll_start>         ; loop if still not ready

    ; --- Write TX byte ---
    movs  r0, #<value>         ; load immediate command byte
    str   r0, [r6, #4]         ; write to SPI3_DT
    nop                        ; pipeline barrier

    ; --- Poll RXNE (RX buffer not empty, STS bit 0) ---
    ldr   r0, [r6, #0]         ; read SPI3_STS
    lsls  r0, r0, #31          ; shift bit 0 to sign position
    ; (IT block: speculative reads)
    bne.n <rx_ready>
    ldr   r0, [r6, #0]
    lsls  r0, r0, #31
    beq.n <poll_rxne>          ; loop if RXNE not set

    ; --- Read RX byte ---
    ldr   r0, [r6, #4]         ; read SPI3_DT (clears RXNE)
```

## Complete Handshake Byte Sequence

### Pre-upload handshake: 11 exchanges in 3 groups

#### Group 1 — immediately after SPI3 enable + PC6 HIGH + ~100ms delay

| # | Insn addr  | `movs` value | TX byte | Notes |
|---|-----------|-------------|---------|-------|
| 1 | 0x0802A790 | `movs r0, #0`  | **0x00** | Sync/reset |
| 2 | 0x0802A7D4 | `movs r0, #5`  | **0x05** | Command: unknown (mode select?) |
| 3 | 0x0802A810 | `movs r0, #0`  | **0x00** | Padding/ack |
| 4 | 0x0802A854 | `movs r0, #0`  | **0x00** | Padding/ack |

#### ~100ms SysTick delay (countdown from 100 in steps of 50)

#### Group 2

| # | Insn addr  | `movs` value | TX byte | Notes |
|---|-----------|-------------|---------|-------|
| 5 | 0x0802A90C | `movs r0, #18` | **0x12** | Command: 18 decimal |
| 6 | 0x0802A948 | `movs r0, #0`  | **0x00** | Padding |
| 7 | 0x0802A98C | `movs r0, #0`  | **0x00** | Padding |

#### Calibrated SysTick delay (uses `DAT_20002b20` system clock divisor)

#### Group 3

| # | Insn addr  | `movs` value | TX byte | Notes |
|---|-----------|-------------|---------|-------|
|  8 | 0x0802AA44 | `movs r0, #21` | **0x15** | Command: 21 decimal |
|  9 | 0x0802AA80 | `movs r0, #0`  | **0x00** | Padding |
| 10 | 0x0802AAC4 | `movs r0, #0`  | **0x00** | Padding |
| 11 | 0x0802AB08 | `movs r0, #59` | **0x3B** | Command: 59 decimal (= length marker?) |

### Summary: handshake TX bytes

```
Group 1:  00  05  00  00
          ~~delay~~
Group 2:  12  00  00
          ~~delay~~
Group 3:  15  00  00  3B
```

**Non-zero command bytes: 0x05, 0x12, 0x15, 0x3B** — these are likely
FPGA SPI slave commands. The zeros are either padding, acknowledgement
slots, or clock-only exchanges where the RX response is what matters.

## Bulk Upload Loop (38,546 iterations)

```arm
; Loop entry at 0x0802AB74
; r0 = byte offset (0, 3, 6, ..., 0x1C3B3)
; r1 = flash base (&DAT_08051d1b)
; r2 = terminal count (0x1C3B6 = 115,638)
; r6 = &SPI3_STS (0x40003C08)

0x0802AB74:  adds  r0, #3            ; offset += 3
0x0802AB76:  ldr   r3, [r6, #4]      ; DT READ: response from prev iteration byte 2
0x0802AB78:  cmp   r0, r2            ; if offset == 115638: exit
0x0802AB7A:  beq.n exit_loop
0x0802AB7C:  ldrb  r3, [r1, r0]      ; r3 = flash[offset+0]  (byte 0)

             ; ... TXE poll ...
0x0802AB9A:  str   r3, [r6, #4]      ; DT WRITE: send byte 0

             ; ... RXNE poll ...
0x0802ABB6:  ldr   r3, [r6, #4]      ; DT READ: response to byte 0

0x0802ABB8:  adds  r3, r0, r1        ; r3 = &flash[offset]
0x0802ABBA:  ldrb  r7, [r3, #1]      ; r7 = flash[offset+1]  (byte 1)

             ; ... TXE poll ...
0x0802ABD8:  str   r7, [r6, #4]      ; DT WRITE: send byte 1

             ; ... RXNE poll ...
0x0802ABF6:  ldr   r7, [r6, #4]      ; DT READ: response to byte 1

0x0802ABF4:  ldrb  r3, [r3, #2]      ; r3 = flash[offset+2]  (byte 2)

             ; ... TXE poll ...
0x0802AC14:  str   r3, [r6, #4]      ; DT WRITE: send byte 2

             ; ... RXNE poll → branch to 0x0802AB74 ...
```

**Each iteration sends 3 consecutive bytes from flash.** Full-duplex:
after each TX, the firmware reads back the RX byte (even if it discards
it). Total data: 38,546 iterations x 3 bytes = **115,638 bytes** from
flash address `0x08051D1B`.

## Post-Upload Cleanup (6 exchanges + CS toggles)

After the loop exits, the firmware interleaves CS pin toggles with
SPI3 transfers:

| Step | GPIO | SPI3 TX | Insn addr |
|------|------|---------|-----------|
| 1 | **CS assert** (PB6 LOW) | — | 0x0802AC34 |
| 2 | — | TX **0x00** → RX | 0x0802AC54 |
| 3 | **CS deassert** (PB6 HIGH) | — | 0x0802AC76 |
| 4 | — | TX **0x3A** → RX | 0x0802AC98 |
| 5 | — | TX **0x00** → RX | 0x0802ACD4 |
| 6 | **CS assert** (PB6 LOW) | — | 0x0802ACF6 |
| 7 | — | TX **0x00** → RX | 0x0802AD18 |
| 8 | **CS deassert** (PB6 HIGH) | — | 0x0802AD3A |
| 9 | — | TX **0x00** → RX | 0x0802AD5C |
| 10 | **CS assert** (PB6 LOW) | — | 0x0802AD7E |
| 11 | — | TX **0x00** (flush) → RX | 0x0802ADA8 |

Post-upload TX bytes: `CS_LO → 0x00 → CS_HI → 0x3A, 0x00 → CS_LO → 0x00 → CS_HI → 0x00 → CS_LO → 0x00`

**Non-zero post-upload byte: 0x3A** — likely a "transfer complete"
command to the FPGA. The CS toggles suggest framing: each CS
assert/deassert pair delimits a command word.

## Then: USART2 Boot Commands

Immediately after the SPI3 cleanup, the firmware queues USART2
commands via `FUN_0803ecf0` (xQueueSend):

```
command 1  → FPGA_CMD_INIT_01 (channel init)
command 2  → FPGA_CMD_INIT_02 (siggen setup)
command 6  → FPGA_CMD_INIT_06 (siggen setup)
command 7  → FPGA_CMD_INIT_07 (meter probe detect)
command 8  → FPGA_CMD_INIT_08 (meter configure)
```

These match the definitions in `fpga.h`.

## What the Custom Firmware Should Do Differently

### 1. Send the handshake command bytes (CRITICAL)

The current `fpga.c` likely sends all-zeros or skips the handshake
entirely. The stock firmware sends a specific 11-byte command sequence
with non-zero bytes at positions 2, 5, 8, and 11:

```c
// Group 1
spi3_exchange(0x00);  // sync
spi3_exchange(0x05);  // command
spi3_exchange(0x00);
spi3_exchange(0x00);
delay_ms(100);

// Group 2
spi3_exchange(0x12);  // command
spi3_exchange(0x00);
spi3_exchange(0x00);
delay_ms(100);        // calibrated delay

// Group 3
spi3_exchange(0x15);  // command
spi3_exchange(0x00);
spi3_exchange(0x00);
spi3_exchange(0x3B);  // terminal command
```

### 2. Upload in 3-byte frames (IMPORTANT)

Each loop iteration sends exactly 3 consecutive bytes, with full
TXE/RXNE polling between each byte. If the custom firmware sends
bytes with different framing or timing, the FPGA SPI slave state
machine may reject the data.

### 3. Post-upload CS toggling (IMPORTANT)

The stock firmware toggles CS (PB6) during post-upload cleanup in a
specific pattern. This isn't a simple "deassert at end" — it's a
3-phase CS toggle with SPI transfers in between, suggesting the FPGA
uses CS edges as command delimiters.

### 4. The 0x3A completion byte (CHECK)

The post-upload sends 0x3A between the first CS deassert and the
second CS assert. This may be a "verify" or "commit" command. If it's
missing, the FPGA might not latch the uploaded data.

## Raw Data: All 20 DT WRITE Instructions

For verification, every SPI3_DT write in the function:

```
ADDR        INSTRUCTION          VALUE  PHASE
0x0802A790  movs r0, #0;  str   0x00   handshake group 1
0x0802A7D4  movs r0, #5;  str   0x05   handshake group 1
0x0802A810  movs r0, #0;  str   0x00   handshake group 1
0x0802A854  movs r0, #0;  str   0x00   handshake group 1
0x0802A90C  movs r0, #18; str   0x12   handshake group 2
0x0802A948  movs r0, #0;  str   0x00   handshake group 2
0x0802A98C  movs r0, #0;  str   0x00   handshake group 2
0x0802AA44  movs r0, #21; str   0x15   handshake group 3
0x0802AA80  movs r0, #0;  str   0x00   handshake group 3
0x0802AAC4  movs r0, #0;  str   0x00   handshake group 3
0x0802AB08  movs r0, #59; str   0x3B   handshake group 3
0x0802AB9A  str r3 (ldrb [r1,r0])      bulk loop: flash[offset+0]
0x0802ABD8  str r7 (ldrb [r3,#1])      bulk loop: flash[offset+1]
0x0802AC14  str r3 (ldrb [r3,#2])      bulk loop: flash[offset+2]
0x0802AC54  movs r0, #0;  str   0x00   post-upload (after CS assert)
0x0802AC98  movs r0, #58; str   0x3A   post-upload (after CS deassert)
0x0802ACD4  movs r0, #0;  str   0x00   post-upload
0x0802AD18  movs r0, #0;  str   0x00   post-upload (after CS assert)
0x0802AD5C  movs r0, #0;  str   0x00   post-upload (after CS deassert)
0x0802ADA8  movs r0, #0;  str   0x00   post-upload flush (CS assert)
```

## Method Notes

This decode was produced by:
1. ripcord `peripheral_xrefs` table identified all 206 SPI3 register
   accesses with instruction addresses and read/write types
2. `arm-none-eabi-objdump` on the raw binary extracted the Thumb-2
   instructions at each DT WRITE address
3. The `movs rN, #imm` instruction immediately before each
   `str rN, [r6, #4]` gives the exact byte value written to SPI3_DT
4. The bulk loop body loads bytes from flash via `ldrb` with
   register-offset addressing

The Ghidra decompiler dropped these values because it treats
peripheral register writes as side-effect-free stores to "dead"
addresses when it can't prove the store is read back. This is a
known Ghidra limitation with memory-mapped I/O.
