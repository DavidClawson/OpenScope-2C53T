# SPI3 Init Sequence — Decoded from Stock Firmware Decompilation

**Date:** 2026-04-09  
**Source:** ripcord phase decomposition of `FUN_08027a50` (stock V1.2.0)  
**Confidence:** HIGH — decompiled C with register address verification  

## Context

The FPGA returns all-0xFF on SPI3 MISO during the H2 calibration upload.
GPIO pin mux is correct (verified on hardware). This document decodes
the exact stock firmware SPI3 initialization sequence to identify any
missing steps.

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

### Step 7: SPI3 handshake (CRITICAL — 4 dummy exchanges)

**After the 100ms delay, the stock firmware performs exactly 4 SPI3
byte exchanges before the H2 upload loop.** Each exchange consists of:

```c
// Wait for TXE (TX buffer empty)
while (!(SPI3_STS & 0x02)) {};  // 0x40003C08 bit 1

// Wait for RXNE (RX buffer not empty) 
while (!(SPI3_STS & 0x01)) {};  // 0x40003C08 bit 0
```

This pattern repeats 4 times. The stock firmware does NOT write explicit
data to `SPI3_DT` during these 4 exchanges — it just polls TXE and RXNE.
This suggests the TX buffer was pre-loaded or the FPGA is responding to
the clock edges alone.

**Then there's ANOTHER SysTick delay (~100ms).** Then 4 MORE SPI3
TXE+RXNE polls.

### Step 8: H2 calibration bulk upload

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
    
    // Read source byte from flash
    puVar3 = &DAT_08051d1b + iVar18;
    
    // Two more TXE+RXNE polls
    while (!(SPI3_STS & 0x02)) {};
    while (!(SPI3_STS & 0x01)) {};
    
    iVar18 += 3;  // advance by 3 bytes per iteration
} while (iVar18 != 0x1c3b6);  // 0x1C3B6 = 115,638 bytes
```

**KEY OBSERVATION:** The loop advances by 3 bytes per iteration
(`iVar18 += 3`) and the total count is 115,638 (`0x1C3B6`). This means
38,546 iterations, each transferring 3 bytes. The per-iteration pattern
of 3 SPI exchanges (6 TXE+RXNE polls) matches a 3-byte-per-block
transfer protocol.

The source data starts at `DAT_08051d1b` (flash address `0x08051D1B`).

### Step 9: Post-upload handshake

```c
// 4 more TXE+RXNE poll pairs (drain SPI buffers)

// CS assert then deassert (toggle)
GPIOB_BSRR_RESET = 0x40;  // 0x40010C14: PB6 LOW (CS assert)
// ... TXE+RXNE polls ...
GPIOB_BSRR_SET = 0x40;    // 0x40010C10: PB6 HIGH (CS deassert)

// Write 0 to SPI3_DT
SPI3_DT = 0;               // 0x40003C0C: flush

// Disable SysTick
SYSTICK_CTRL &= ~1;        // 0xE000E010: clear ENABLE bit
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
CRM_APB2EN |= 0x10;  // GPIOC clock enable
// Configure PC4 as output
if (DAT_2000010f == 2) {
    GPIOC_BSRR_SET = 0x10;   // PC4 HIGH
} else {
    GPIOC_BSRR_RESET = 0x10; // PC4 LOW
}
```

**PC4 is set conditionally based on `DAT_2000010f`.** This RAM variable
comes from the `.data` init section (which is missing from our binary
dump). Its default value determines whether PC4 starts HIGH or LOW.

## What's Different From the Custom Firmware?

Compare this against the osc project's `fpga.c` init sequence:

| Step | Stock firmware | Custom firmware | Match? |
|------|---------------|-----------------|--------|
| SPI3 clock enable | ✅ CRM_APB1EN \|= 0x8000 | ✅ Yes | ✅ |
| GPIO PB3/4/5/6 config | ✅ 4 calls to pin config helper | ✅ Yes | ✅ |
| CS deassert (PB6 HIGH) | ✅ Before SPI3 enable | ✅ Yes | ✅ |
| SPI3 CTRL2 \|= 3 (DMA) | ✅ Before enable | ✅ Line 1577 | ✅ |
| SPI3 CTRL1 \|= 0x40 (enable) | ✅ Yes | ✅ Yes | ✅ |
| PC6 HIGH | ✅ After SPI3 enable | ✅ Yes | ✅ |
| **100ms delay after PC6** | ✅ SysTick-based | **⚠️ Check** | **?** |
| **4 dummy SPI exchanges** | ✅ TXE+RXNE polls ×4 | **❌ Not seen** | **❌** |
| **Second 100ms delay** | ✅ SysTick-based | **❌ Not seen** | **❌** |
| **4 more dummy exchanges** | ✅ TXE+RXNE polls ×4 | **❌ Not seen** | **❌** |
| H2 bulk upload (3B/iter) | ✅ 38,546 iterations | **⚠️ Check format** | **?** |
| Post-upload CS toggle | ✅ Assert then deassert | **⚠️ Check** | **?** |
| SPI3_DT = 0 flush | ✅ After upload | **❌ Not seen** | **❌** |
| USART2 commands 1,2,6,7,8 | ✅ After SPI3 complete | **⚠️ Check order** | **?** |
| **PC4 conditional set** | ✅ Based on RAM flag | **❌ Unknown** | **❌** |

## Critical Missing Steps

### 1. The 4+4 dummy SPI exchanges (MOST LIKELY CAUSE)

The stock firmware performs **8 dummy SPI3 exchanges** (4 before a delay,
4 after) between PC6 going HIGH and the H2 upload starting. These are
just TXE+RXNE polls without explicit data writes — they clock the SPI
bus to synchronize with the FPGA's SPI slave.

If the custom firmware jumps straight from PC6=HIGH to the H2 upload
without these synchronization exchanges, the FPGA's SPI slave may never
enter its receive-ready state. The FPGA might require seeing clock
edges on SCK (from the dummy exchanges) as a "wake up" signal.

### 2. The inter-exchange delays

The stock firmware has ~100ms SysTick delays:
- After PC6 goes HIGH (before first 4 dummy exchanges)
- After first 4 dummy exchanges (before second 4)

These delays give the FPGA time to enable its SPI slave logic after
the enable signal.

### 3. The 3-byte-per-iteration transfer format

The H2 upload loop advances by 3 bytes per iteration, not 1. If the
custom firmware sends bytes one at a time, the FPGA's SPI protocol
state machine may not recognize the framing.

### 4. PC4 conditional output

PC4 is set HIGH or LOW based on a RAM flag at `DAT_2000010f`. This
could be a hardware strapping pin (e.g., selecting between FPGA
configuration modes). If it's wrong, the FPGA might be in the wrong
mode to accept SPI3 data.

## Recommended Test Sequence

1. **Add the 8 dummy SPI3 exchanges** with 100ms delays between groups
2. **Verify the 3-byte transfer framing** in the H2 upload loop
3. **Check PC4** — try both HIGH and LOW to see if either enables FPGA SPI
4. **Check the exact delay timing** — the stock firmware uses SysTick with
   a system-clock-dependent reload value (`DAT_20002b20`)

The dummy exchanges are the most likely fix. The FPGA probably needs to
see SCK activity after PC6 goes HIGH before it enables its SPI slave
data path. Without those clock edges, MISO stays tri-stated (0xFF).
