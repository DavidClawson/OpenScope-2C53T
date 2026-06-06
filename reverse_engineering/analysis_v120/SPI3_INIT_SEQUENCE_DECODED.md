# SPI3 Init Sequence — Decoded from Stock Firmware Decompilation

**Date:** 2026-04-09  
**Source:** ripcord phase decomposition of `FUN_08027a50` (stock V1.2.0)  
**Confidence:** HIGH — decompiled C with register address verification  

## Context

The FPGA returns all-0xFF on SPI3 MISO during the H2 calibration upload.
GPIO pin mux is correct (verified on hardware). This document decodes
the exact stock firmware SPI3 initialization sequence to identify any
missing steps.

> 2026-06-06 correction: this is the SPI3 init choreography correction. The
> early clock-only synchronization hypothesis was superseded by byte-accurate
> stock disassembly and
> `spi3_bulk_cal_resolved.md`. The stock-visible sequence is explicit SPI3 data
> writes (`00 05 00 00`, delay, `12 00 00`, delay, `15 00 00 3B`), then the
> 115,638-byte H2 table in 3-byte records, then `3A`/flush/CS deassert. MISO is
> drained/discarded; no recovered FPGA ACK/apply status exists.

## Stock Firmware SPI3 Init Sequence

The following sequence occurs in `FUN_08027a50` (the 15KB system init),
decompiled lines 907-1069. Every register address has been verified
against the AT32F403A reference manual.

### Step 1: Clock enables

```c
CRM_APB1EN |= 0x8000;    // 0x4002101C: bit 15 = SPI3 clock enable
CRM_APB2EN |= 0x08;      // 0x40021018: bit 3 = GPIOB clock enable
```

### Step 2: GPIO pin configuration

```c
// PB12 = output push-pull 2MHz (SPI2 flash CS — NOT FPGA related)
GPIOB_CRH = (GPIOB_CRH & 0xFFF8FFFF) | 0x20000;  // 0x40010030

// PB3 (SCK), PB4 (MISO), PB5 (MOSI), PB6 (CS) configured via
// FUN_080342fc(GPIOB_CRL, params) — 4 calls with different pin configs
FUN_080342fc(0x40010C00, ...);  // GPIO pin setup helper
FUN_080342fc(0x40010C00, ...);
FUN_080342fc(0x40010C00, ...);
FUN_080342fc(0x40010C00, ...);
```

### Step 3: CS deassert

```c
GPIOB_BSRR = 0x40;  // 0x40010C10: PB6 HIGH (CS deassert)
```

### Step 4: SPI3 peripheral init

```c
// Init params: 0x100 (prescaler /4?), 0x1010100 (Mode 3 + master + 8bit?)
FUN_0803a848(&SPI3_CTRL1, init_params);  // SPI3 base = 0x40003C00

// Enable DMA request lines
SPI3_CTRL2 |= 3;   // 0x40003C04: bit 0 = RXDMAEN, bit 1 = TXDMAEN

// Enable SPI3
SPI3_CTRL1 |= 0x40; // 0x40003C00: bit 6 = SPIEN
```

### Step 5: GPIOC config + PC6 HIGH

```c
CRM_APB2EN |= 0x10;  // 0x40021018: bit 4 = GPIOC clock enable
FUN_080342fc(0x40011000, ...);  // GPIOC pin config
GPIOC_BSRR = 0x40;  // 0x40011010: PC6 HIGH (FPGA SPI enable)
```

### Step 6: SysTick delay (~100ms)

```c
// Two nested countdown loops using SysTick (0xE000E010-0xE000E018)
// Each loop counts down from 100 in steps of 50 (0x32)
// with SysTick reload values based on DAT_20002b20 (system clock)
// Total delay: approximately 100ms
uVar15 = 100;
do {
    uVar15 -= 0x32;  // subtract 50
    // poll SysTick COUNTFLAG
    while (SYSTICK_CTRL & 0x10000 == 0) {};
} while (uVar15 != 0);
```

### Step 7: SPI3 handshake and H2 start opcode

The later byte-accurate pass recovered the SPI3 data bytes that this older
decompile summary missed. Stock performs explicit SPI3 transfers, not a
mysterious clock-only synchronization phase:

```text
0x0802676E.. phase-7 handshake:
  00 05 00 00          ; sync/query group
  delay
  12 00 00             ; second command group
  delay
  15 00 00 3B          ; third command group plus H2 bulk-start opcode
```

The exact GPIO chip-select edges are still tracked through the stock CS writes,
not inferred from RX contents. `0x3B` is the SPI3 session opcode that enters H2
bulk-write mode; it is not a USART2 command and it is not present inside the H2
table itself.

### Step 8: H2 bulk table upload

```c
iVar18 = 0;
do {
    // Wait TXE
    while (!(SPI3_STS & 0x02)) {};
    // Wait RXNE  
    while (!(SPI3_STS & 0x01)) {};
    
    // Wait TXE again
    while (!(SPI3_STS & 0x02)) {};
    // Wait RXNE again
    while (!(SPI3_STS & 0x01)) {};
    
    // Read source bytes from flash
    puVar3 = &DAT_08051d19 + iVar18;
    
    // Two more TXE+RXNE polls
    while (!(SPI3_STS & 0x02)) {};
    while (!(SPI3_STS & 0x01)) {};
    
    iVar18 += 3;  // advance by 3 bytes per iteration
} while (iVar18 != 0x1c3b6);  // 0x1C3B6 = 115,638 bytes
```

**KEY OBSERVATION:** The loop advances by 3 bytes per iteration
(`iVar18 += 3`) and the total count is 115,638 (`0x1C3B6`). This means
38,546 iterations, each transferring 3 bytes. The source data starts at
`0x08051D19`, and the table SHA/layout are guarded by
`scripts/test_stock_h2_table.py`.

Stock drains or ignores MISO during this loop. The current firmware's
`h2_bytes_sent == 115638` diagnostic is therefore TX-side evidence only: it
proves byte transmission, not FPGA acceptance/apply status and not DMM physical
calibration correctness.

### Step 9: Post-upload handshake

```c
// Stock-resolved post-H2 sequence:
//   CS assert; TX 0x00; CS deassert
//   TX 0x3A; TX 0x00
//   CS assert; TX 0x00; CS deassert
//   TX 0x00
//   CS assert; TX 0x00 final flush
```

The resolved stock note pins these offsets:

```text
0x08026AE6: PB6 CS assert before H2 start
0x08026B06..0x08026B08: send 0x3B
0x08026B7C..0x08026C30: 38,546 x 3-byte H2 records
0x08026C96..0x08026C98: send 0x3A
0x08026CD2: send 0x00 flush
0x08026CF6: PB6 CS deassert
```

### Step 10: USART2 boot commands

```c
// Queue USART2 commands via FUN_0803ecf0 (queue send):
FUN_0803ecf0(queue, 1, ...);  // command 0x01: channel init
FUN_0803ecf0(queue, 2, ...);  // command 0x02: siggen setup  
FUN_0803ecf0(queue, 6, ...);  // command 0x06: siggen setup
FUN_0803ecf0(queue, 7, ...);  // command 0x07: meter probe detect
FUN_0803ecf0(queue, 8, ...);  // command 0x08: meter configure
```

### Step 11: PC4 conditional set

```c
// 0x08026E2E..0x08026E8A, after USART2 commands 1,2,6,7,8
CRM_APB2EN |= 0x10;  // GPIOC clock enable
// Configure PC4 as output
if (DAT_2000010f == 2) {
    GPIOC_BSRR_SET = 0x10;   // PC4 HIGH
} else {
    GPIOC_BSRR_RESET = 0x10; // PC4 LOW
}
```

**PC4 is set conditionally based on `DAT_2000010f` / `ms[0x17]`.**
STATE_STRUCTURE.md identifies this byte as `trigger_run_mode`:
`0=AUTO`, `1=NORMAL`, `2=SINGLE`.  That makes the post-H2 PC4 branch a
scope-state boundary, not a DMM range byte, not an H2 ACK/apply proof, and
not a low-DCV correction.  This branch is left unimplemented until a stock `.data` default,
stock/open trace, or measured multi-mode effect
proves which PC4 level should be driven and what that level changes.

## Current Open-Firmware Boundary

| Step | Stock firmware | Custom firmware | Match? |
|------|---------------|-----------------|--------|
| SPI3 clock enable | ✅ CRM_APB1EN \|= 0x8000 | ✅ Yes | ✅ |
| GPIO PB3/4/5/6 config | ✅ 4 calls to pin config helper | ✅ Yes | ✅ |
| CS deassert (PB6 HIGH) | ✅ Before SPI3 enable | ✅ Yes | ✅ |
| SPI3 CTRL2 \|= 3 (DMA) | ✅ Before enable | ✅ Yes | ✅ |
| SPI3 CTRL1 \|= 0x40 (enable) | ✅ Yes | ✅ Yes | ✅ |
| PC6 HIGH | ✅ After SPI3 enable | ✅ Yes | ✅ |
| 100ms delay after SPI3 enable | ✅ SysTick-based | ✅ `systick_delay_ms(100)` | ✅ |
| Handshake/start bytes | ✅ `00 05 00 00`, `12 00 00`, `15 00 00 3B` | ✅ same byte groups | ✅ |
| H2 bulk upload | ✅ 38,546 x 3-byte records | ✅ same table and 3-byte loop | ✅ TX only |
| Post-upload CS/`3A`/flush sequence | ✅ CS edges plus `3A`/flush | ✅ mirrored in `fpga.c` | ✅ TX only |
| USART2 commands 1,2,6,7,8 | ✅ After SPI3/H2 complete | ✅ deferred until after H2 cleanup | ✅ |
| PC4 conditional set | ✅ `0x08026E2E..0x08026E8A`, based on `DAT_2000010f` / `ms[0x17]` (`trigger_run_mode`) | documented unresolved; no open PC4 write yet | gap |

## Remaining Gap

The current unresolved H2 question is not "add dummy exchanges". It is
acceptance/effect: whether the FPGA accepted and applied the transmitted table,
and whether that state explains low-DCV/current/low-Ohm physical correctness.
Resolving that requires a stock/open-firmware SPI3 trace with MISO/CS timing or
repeatable live validation across multiple DMM ranges after a preflighted
OpenScope image. Until then, H2 byte count remains diagnostic only.  The PC4
post-H2 trigger-run-mode boundary is a separate unresolved GPIO state boundary;
it should not be used as a blind low-DCV or DMM-range fix without stock default
or effect evidence.
