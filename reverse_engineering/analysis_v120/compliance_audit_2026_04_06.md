# Compliance Audit: Stock vs Custom Firmware Init Sequence

**Date:** 2026-04-06
**Method:** 4 parallel agents audited master_init phases 1-4 register-by-register against our fpga_init() and main.c

## Background

Despite claiming "FPGA protocol 100% decoded", SPI3 MISO remained dead — the FPGA never responded with scope data. This audit compared every register write in the stock firmware's 15.4KB master init function (FUN_08023A50) against our custom implementation.

## Critical Findings (fixes applied)

### 1. SPI3 Clock Prescaler: /4 instead of /2 (CRITICAL)
- **Stock:** BR[2:0] = 000 → /2 → 60MHz from 120MHz APB1
- **Ours:** BR[2:0] = 001 → /4 → 30MHz (half speed!)
- **Evidence:** fpga_task_annotated.c line 1132, FPGA_TASK_ANALYSIS.md line 89, remaining_unknowns.md line 86 all say "/2 clock (60MHz)"
- **Fix:** Removed `| (1 << 3)` from CTRL1 register write

### 2. SPI3 CTRL2 DMA Enable Bits: 0x00 instead of 0x03 (CRITICAL)
- **Stock:** `SPI3_CTL1 |= 0x03` — sets RXDMAEN + TXDMAEN
- **Ours:** `FPGA_SPI->ctrl2 = 0` — no DMA bits
- **Our comment was WRONG:** "DMA must be DISABLED or the data register won't work for polled access" — this is false on AT32/STM32F1. Setting DMA enable bits without configuring DMA channels just causes ignored DMA requests. Polled DR access still works fine.
- **Fix:** Changed to `FPGA_SPI->ctrl2 = 0x03`

### 3. SPI3 NVIC Interrupt Not Enabled (HIGH)
- **Stock:** Enables SPI3 IRQ #51 (`NVIC_ISER1 bit 19`)
- **Ours:** No SPI3 interrupt handler or NVIC enable
- **Fix:** Added `NVIC_EnableIRQ(SPI3_I2S3EXT_IRQn)` + stub IRQ handler

### 4. PC11 (Meter MUX Enable) Never Configured as Output (CRITICAL for meter)
- **Stock:** Configures PC11 as output push-pull, then sets HIGH
- **Ours:** Wrote `GPIOC->scr = (1U << 11)` without calling `gpio_init()` — pin defaults to floating input on reset, so the write was silently ignored
- **Fix:** Added `gpio_init(GPIOC, {PINS_11, OUTPUT})` before the scr write

### 5. Missing Boot Commands 0x03, 0x04, 0x05 (MEDIUM)
- **Stock:** Sends all 0x01-0x08 during USART boot sequence
- **Ours:** Only sent 0x01, 0x02, 0x06, 0x07, 0x08 (skipped 0x03=trigger, 0x04=vertical scale, 0x05=channel enable)
- **Fix:** Added the missing 3 commands

### 6. Handshake Byte Count Wrong (MEDIUM)
- **Stock:** Sends 4 bytes per CS transaction (0x00, 0x05, 0x00, 0x00) with leading dummy
- **Ours:** Only sent 2 bytes (0x05, 0x00)
- **Fix:** Added leading 0x00 dummy and trailing 0x00 parameter

### 7. SysTick Delays Too Short (LOW-MEDIUM)
- **Stock:** ~20ms multi-phase delay before handshake
- **Ours:** 10ms single delay
- **Fix:** Increased to 20ms (10+5+5)

### 8. PB11 HIGH Set Too Early (LOW)
- **Stock:** Sets PB11 HIGH in step 52, just before vTaskStartScheduler()
- **Ours:** Set PB11 immediately after H2 cal upload, before analog frontend or meter commands
- **Fix:** Moved to after all configuration, just before post-init SPI3 probe

## Other Discrepancies (not yet fixed)

### Structural / Architectural
| # | Item | Stock | Ours | Severity |
|---|------|-------|------|----------|
| 9 | Missing "osc" task | 8 FreeRTOS tasks | 6 tasks (no osc) | MEDIUM |
| 10 | Missing 2 FPGA semaphores | Gates acquisition start + secondary ops | Queue-based | MEDIUM |
| 11 | Missing usart_cmd_queue | 20-deep command routing layer | Direct to TX queue | MEDIUM |
| 12 | Missing TIM5 config | Freq measurement timer | Not configured | MEDIUM |
| 13 | No SPI flash restore | Loads saved state at boot | Always defaults | MEDIUM |
| 14 | Meter cmds during init | Sent at runtime on mode switch | Sent at boot | MEDIUM |
| 15 | EXTI3 on PA3 | Rising edge for fast USART RX | USART RXNE only | MEDIUM |

### LCD / Display
| # | Item | Stock | Ours | Severity |
|---|------|-------|------|----------|
| 16 | PD6 = LCD reset pin | HW reset (toggle HIGH→LOW→HIGH) | Software reset (0x01) | MEDIUM |
| 17 | Full LCD init | All ST7789V registers | Minimal (SLPOUT+MADCTL+COLMOD+DISPON) | LOW |
| 18 | LCD DMA (DMA1 CH4) | Fast pixel blitting | Direct EXMC writes | LOW |
| 19 | IOMUX->REMAP3 bit 27 | Set | Not set | LOW |
| 20 | EXMC reg 0xA0000220 | Configured | Not configured | LOW |

### Timer / DMA
| # | Item | Stock | Ours | Severity |
|---|------|-------|------|----------|
| 21 | No DMA channels for SPI3 | DMA-driven transfers | Polled | MEDIUM |
| 22 | TMR7/TMR8 | FatFs timing | Not configured | LOW |
| 23 | TMR3 start timing | Deferred until mode logic | Started immediately | LOW |

## What Changed in This Session

All changes in `firmware/src/drivers/fpga.c`:
1. SPI3 CTRL1: removed `(1 << 3)` — prescaler now /2 = 60MHz
2. SPI3 CTRL2: changed `= 0` to `= 0x03` (DMA enable bits)
3. Added `NVIC_EnableIRQ(SPI3_I2S3EXT_IRQn)` + stub SPI3 IRQ handler
4. Added `gpio_init()` for PC11 before meter MUX enable write
5. Added boot commands 0x03, 0x04, 0x05 to USART init sequence
6. Fixed handshake to send 4 bytes (leading 0x00 + 0x05 + 0x00 + trailing 0x00)
7. Increased pre-handshake delay from 10ms to 20ms
8. Moved PB11 HIGH to after all analog frontend/meter configuration

## Next Steps

1. **Flash and test** — does MISO respond now with prescaler/DMA/IRQ fixes?
2. **If still dead:** Binary-trace the stock firmware using Capstone disassembly to extract every STR to 0x40000000+ in the init function
3. **If still dead:** Unicorn emulation of stock init, logging all peripheral register writes
4. **If still dead:** Consider FPGA hardware damage (stock firmware also won't boot on this unit)
5. **Regardless:** Implement Tier 3 fixes (osc task, semaphores, TIM5, SPI flash restore)
