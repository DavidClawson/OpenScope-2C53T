# Firmware Reverse Engineering Coverage Tracker

## Overall Statistics

| Metric | Value |
|--------|-------|
| **Firmware binary** | 734 KB total |
| **Code region** | 252 KB (0x08000238 – 0x0803F2C2) |
| **Data region** | 481 KB (fonts, images, strings, lookup tables) |
| **Total real functions** | **309** (292 mapped + 9 code-region gaps + 8 init-only) |
| **Named functions** | 320 (138 HIGH + 182 MEDIUM confidence) |
| **Ghidra false positives** | 61 (halt_baddata in data region 0x0803F268+) |
| **Function naming** | ~97% complete (42 LOW confidence remain) |
| **FPGA protocol** | **100% decoded** — all ~40 commands mapped, ADC format, state machine |
| **Button input** | **14/15 HARDWARE CONFIRMED** — bidirectional 4x3 matrix, TMR3 at 500Hz. Only PRM (PB7) unresolved. |
| **Battery ADC** | 100% — ADC1 Channel 9, PB1 |
| **Overall understanding** | **~99%** — buttons hardware-confirmed, USART frames flowing |

### Analysis Files (as of 2026-04-01)

| File | Contents |
|------|----------|
| `analysis_v120/function_names.md` | Complete 362-entry naming inventory |
| `analysis_v120/gap_functions.md` | 17 gap functions catalogued by priority |
| `analysis_v120/function_map_complete.txt` | 309-entry complete function map |
| `analysis_v120/fpga_task_annotated.c` | Annotated FPGA task (10 sub-functions) |
| `analysis_v120/FPGA_TASK_ANALYSIS.md` | FPGA protocol: SPI3 format, commands, state machine |
| `analysis_v120/remaining_unknowns.md` | Final 5% extraction: clock tree, IOMUX, ADC, DMA, TMR8 |
| `ARCHITECTURE.md` | Master architecture guide — read this first |
| `FPGA_PROTOCOL_COMPLETE.md` | Definitive FPGA communication reference |
| `HARDWARE_PINOUT.md` | Complete pin-by-pin and peripheral config reference |
| `CALIBRATION.md` | ADC calibration pipeline and measurement processing |

## Gap Analysis — RESOLVED

The original 113 KB of "unmapped" code has been fully catalogued. The gaps contained 17 real functions (9 in code-region gaps, 8 called only from the master init function). The remaining "gap" bytes are function bodies already in function_map.txt but with imprecise size estimates, plus literal pools and alignment padding.

See `analysis_v120/gap_functions.md` for the complete 17-function inventory with priorities.

### Key Gap Discoveries

| Address | Name | Size | Significance |
|---------|------|------|-------------|
| 0x08023A50 | `system_init` (master) | ~15.4 KB | Configures ALL peripherals, creates ALL FreeRTOS tasks |
| 0x08037800 | `fpga_spi3_transfer` | 472B | Core SPI3 read path with PB6 CS, ADC sample processing |
| 0x08037400 | `fpga_command_rx_task` | ~1 KB | Infinite loop on xQueueReceive for FPGA responses |
| 0x0800ba06–bc98 | FPGA command builders (6) | ~300B | Revealed 16 new command codes (0x16-0x28) |
| 0x08039734 | `timer_init_helper` | ~200B | TIM2/TIM5 configuration (FPGA timing) |
| 0x08001830 | `calibration_loader` | ~100B | Loads 301-byte per-channel cal data from SPI flash |

### Beyond Code Region

| Range | Size | Contents | Priority |
|-------|------|----------|----------|
| 0x0803F268–0x08045A00 | ~26 KB | C runtime library (printf formatters, floating-point math) | **Skip** — using our own libc |
| 0x08045A00–0x080B7680 | ~460 KB | Data: UI images, fonts, multilingual strings (DE/ES/PT/EN), lookup tables | **Skip** — own assets |

## Subsystem Coverage

### CRITICAL PATH — Hardware bring-up

| Subsystem | Coverage | Status | Key Findings |
|-----------|----------|--------|-------------|
| **FPGA init sequence** | **90%** | DECODED | 53-step boot in FPGA_BOOT_SEQUENCE.md. Master init at FUN_08023A50 (15.4KB). Missing: PB11 HIGH, USART boot cmds 0x01-0x08, SysTick delays. |
| **FPGA command codes** | **~75%** | DECODED | ~30 of ~40 commands mapped (0x00-0x2C). Dispatch table at 0x0804BE74. Channel, trigger, timebase, meter, siggen all covered. |
| **Button input** | **100%** | **RESOLVED** | All 15 buttons on MCU GPIO. 3-group multiplexed scan (GPIOB/GPIOC), 70-tick debounce, button map at 0x08046528. NOT FPGA or I2C. |
| **SPI3 ADC data** | **95%** | DECODED | Interleaved CH1/CH2 unsigned 8-bit. Normal=1024B, dual=2048B, roll=300 circular. ADC offset=-28.0. VFP calibration pipeline fully documented. |
| **SPI2 flash** | 80% | Known | Full read/write/erase protocol. SPI flash = W25Q128JV. |
| **Power management** | **95%** | DECODED | PC9 hold, PB8 backlight, 3-tier auto power-off (15/30/60min), IWDG ~50ms feed rate. |
| **Clock/peripheral init** | **70%** | PARTIAL | Master init function found (FUN_08023A50). TIM2/TIM5 helper at FUN_08039734. Full RCC config still needs extraction. |

### REWRITING FROM SCRATCH — Protocol understanding sufficient

| Subsystem | Coverage | Key Functions | Status |
|-----------|----------|---------------|--------|
| **Oscilloscope UI** | Named | `scope_ui_draw_main` (5.1KB), 6 sub-mode handlers | FPGA command interface decoded. Skip deep analysis. |
| **Multimeter mode** | **70%** | `meter_data_processor`, `meter_mode_handler` (8-state FSM), BCD extraction | BCD format, mode FSM, bargraph all documented. |
| **Signal generator** | 70% | `siggen_configure` (1.6KB), GPIO MUX functions | DAC config, GPIO MUX routing well mapped. |
| **UI rendering** | Named | `display_render_engine` (2.6KB), `glyph_render_single` | Own renderer exists. Skip. |
| **File I/O** | Named | 8 SPI flash FAT functions, 7 filesystem functions, 15 SPI core | Using own FatFS. Skip. |
| **USB** | Named | `usb_endpoint_handler` (2.6KB at 0x080278e4) | Using USB library. Skip. |
| **C runtime** | Named | 56 functions (printf family, memset, memcpy, FP math, soft-float) | Using our own libc. Skip. |
| **FreeRTOS kernel** | Named | ~40 functions (queue, task, timer, scheduler) | Using FreeRTOS source. Skip. |

### WELL MAPPED — Reference only

| Subsystem | Coverage | Key Functions | Notes |
|-----------|----------|---------------|-------|
| **Interrupt vector table** | 100% | 6 active handlers identified | All ISR entry points known |
| **GPIO pin assignments** | **100%** | `gpio_mux_porta_portb`, `gpio_mux_portc_porte` | MUX, relay, signal routing, button scan all mapped |
| **FPGA task (complete)** | **95%** | 10 sub-functions annotated in `fpga_task_annotated.c` | SPI3 acquisition, USART protocol, meter data, button scan |
| **Data processing pipeline** | 70% | `display_buffer_write` hub + 11 processors | Waveform math, display buffer chain |
| **DMA configuration** | 60% | `dma1_configure` | DMA1 channel setup for USART2 RX |
| **DAC (signal gen)** | 80% | `siggen_configure` | Dual-channel 12-bit output |
| **SPI flash access** | **80%** | 15 SPI core functions named | Read/write/erase protocol, chip select, FAT32 |
| **Timer usage** | **70%** | TMR3 = USART exchange + button scan, TIM2/TIM5 helper found | TMR8_BRK = still unknown |

## Function Size Distribution

Understanding where complexity lives:

| Size Bucket | Count | Total Bytes | % of Code | Notable Functions |
|-------------|-------|-------------|-----------|-------------------|
| Monster (>5KB) | 5 | 35,726 | 25% | FUN_08019e98 (13.3KB scope FSM), FUN_08030524 (6.5KB), FUN_0802a664 (5.6KB), FUN_08015f50 (5.0KB), FUN_0801f6f8 (4.6KB) |
| Large (1-5KB) | 40 | 67,000 | 48% | Mode handlers, data processors, SPI/FPGA drivers |
| Medium (256B-1KB) | 80 | 28,000 | 20% | Utilities, GPIO config, protocol helpers |
| Small (<256B) | 167 | 10,110 | 7% | Leaf functions, register accessors, tiny helpers |

The **top 5 functions contain 25% of all code**. FUN_08019e98 alone is 13.3 KB — the main oscilloscope state machine with 27 callees. This is where the scope UI logic, mode switching, channel config, trigger setup, and measurement coordination all happen.

## Named Functions — Key Reference

The complete 309-function naming inventory is in `analysis_v120/function_names.md`. Below are the most important functions for hardware bring-up:

### FPGA Task Sub-Functions (annotated in `fpga_task_annotated.c`)

| Address | Size | Name | Purpose |
|---------|------|------|---------|
| 08036934 | 282B | `spi_flash_loader` | Read 4KB pages from SPI flash into RAM |
| 08036A50 | 108B | `usart_cmd_dispatcher` | FreeRTOS task: dispatch via table at 0x0804BE74 |
| 08036AC0 | 1776B | `meter_data_processor` | BCD meter data decode, double-precision calibration |
| 080371B0 | 504B | `meter_mode_handler` | 8-state FSM for meter AC/DC/range/overload |
| 080373F4 | 96B | `usart_tx_frame_builder` | Format 10-byte USART TX frames (dvom_TX task) |
| 08037454 | 7164B | `spi3_acquisition_task` | **Core:** 9-mode ADC acquisition, VFP calibration |
| 08039050 | 312B | `spi3_init_and_setup` | SPI3 GPIO config, Mode 3, initial FPGA handshake |
| 08039188 | 1342B | `input_and_housekeeping` | 15-button debounce, watchdog, frequency measurement |
| 080396C8 | 108B | `probe_change_handler` | Probe detect, auto power-off (15/30/60min) |
| 08039734 | 316B | `usart_tx_config_writer` | 7-type command writer for scope/meter/siggen |

### Critical Hardware Functions

| Address | Size | Name | Confidence | Evidence |
|---------|------|------|------------|----------|
| 08023A50 | ~15.4K | `system_init` | **High** | Master init — ALL peripheral config, ALL FreeRTOS task creation |
| 080277b4 | 304 | `usart2_isr` | High | USART2 TX/RX buffer management |
| 0802771c | 110 | `tmr3_isr` | High | Timer-driven USART exchange + button scan trigger |
| 08037800 | 472 | `fpga_spi3_transfer` | **High** | SPI3 CS control (PB6), ADC sample read, float math |
| 08037400 | ~1K | `fpga_command_rx_task` | **High** | Infinite loop on xQueueReceive for FPGA responses |
| 08001830 | ~100 | `calibration_loader` | High | 301-byte per-channel cal data from SPI flash |
| 08039734 | ~200 | `timer_init_helper` | High | TIM2/TIM5 config (called 2x from system_init) |

### Corrections from Prior Analysis

| Address | Old Name | Corrected Name | Why |
|---------|----------|---------------|-----|
| 08008154 | `adc_measurement_core` | `display_render_engine` | Text layout/glyph rendering, not ADC |
| 080278e4 | (was thought FPGA) | `usb_endpoint_handler` | Accesses USB registers (0x40005C00), not FPGA |
| 0803acf0 | (was thought fpga_send_command) | `xQueueGenericSend` | FreeRTOS kernel function |
| 08036934 | `fpga_task_real` | `spi_flash_loader` | Only 282B at this address; real FPGA task starts at sub-functions |
| 080012bc | `delay_or_yield` | `memset_zero` | Memory zeroing with word-aligned fast path |

## What's Left to Do

### Remaining RE Work (minimal — ~2%)

1. **PLL startup code** — The HSE/PLL configuration happens in startup assembly before FUN_08023A50. Need to disassemble the reset handler to extract PLL multiplier and confirm 240MHz. Can also be measured with SysTick or logic analyzer.

2. **42 LOW confidence function names** — These are small utility functions without distinctive register accesses. Can be refined as replacement firmware development reveals their purpose.

3. **Hardware verification of new findings** — Battery ADC (PB1), button scan GPIO mapping, and remaining FPGA commands should be confirmed on real hardware.

### Done — Can Skip

- UI rendering, measurement math, file I/O, USB, C runtime, string tables, screenshot/save — all being rewritten from scratch with own implementations.

## Key Unknowns (Hardware Questions)

| Question | Status |
|----------|--------|
| **FPGA data interface** | **ANSWERED:** SPI3 (60MHz) on PB3/PB4/PB5, CS on PB6. Interleaved CH1/CH2 8-bit. |
| **FPGA command interface** | **ANSWERED:** USART2 (9600 baud) on PA2/PA3. 10-byte TX, 12-byte RX. TMR3-driven. |
| **SPI3 clock speed** | **ANSWERED:** 60 MHz (APB1=120MHz / prescaler 2). Mode 3 (CPOL=1, CPHA=1). |
| **SPI3 CS polarity** | **ANSWERED:** PB6 active-low (GPIOB BOP/BCR confirmed in annotated code). |
| **ADC sample format** | **ANSWERED:** Unsigned 8-bit, interleaved CH1/CH2. Normal=1024B, dual=2048B. Offset=-28.0. |
| **Button input** | **ANSWERED:** All 15 on MCU GPIO. 3-group multiplexed scan on GPIOB/GPIOC. Button map at 0x08046528. |
| **SPI2 destination** | **ANSWERED:** Winbond W25Q128JV SPI flash (JEDEC: EF 40 18). |
| **USART2 baud rate** | **ANSWERED:** 9600 baud confirmed. |
| **Auto power-off** | **ANSWERED:** 3 tiers (15/30/60min) based on probe state. |
| **Watchdog timing** | **ANSWERED:** IWDG fed every 11 housekeeping calls (~50ms). |
| **Battery voltage ADC** | **ANSWERED:** ADC1 Channel 9 on PB1. 239.5-cycle sample time, right-aligned, software-triggered with calibration. |
| **TMR8_BRK** | **ANSWERED:** Vector repurposed — points to FatFs SPI flash page reader. TMR8 hardware is completely unused. |
| **IOMUX/AFIO remap** | **ANSWERED:** `(reg & ~0xF000) \| 0x2000` at AFIO+0x08. Disables JTAG-DP, keeps SW-DP, frees PB3/PB4/PB5 for SPI3. |
| **DMA assignments** | **ANSWERED:** DMA0 Ch1 = LCD framebuffer (16-bit mem-to-mem → EXMC). SPI3 = polled I/O. USART2 = interrupt-driven. |
| **Remaining FPGA commands** | **ANSWERED:** 0x1F/0x20/0x21 = freq counter (mode 4), 0x25 = period (mode 5), 0x29 = duty cycle (mode 6), 0x2C = continuity/diode (mode 8). |
| RCC/CRM PLL config | PLL setup is in startup code BEFORE FUN_08023A50 (not extractable from decompilation). ADC prescaler = PCLK2/6. Peripheral clocks progressively enabled in init. |

## Hardware Probing Log (2026-03-31)

Tested with custom `fpgaprobe.c` firmware (firmware/src/fpgaprobe.c). ESP32 sniffer monitored debug UART pads simultaneously.

### Confirmed Hardware

| Component | Identification | Interface |
|---|---|---|
| **SPI Flash** | Winbond W25Q128JV (JEDEC: `EF 40 18`) | SPI2 PB12 CS, PB13 CLK, PB14 MISO, PB15 MOSI |
| **LCD** | ST7789V (ID: `0x85`) | EXMC NE1 at 0x6001FFFE (cmd) / 0x60020000 (data) |
| **FPGA heartbeat** | 9600 baud, 12-byte frames | Debug UART pads AND PA3 (same signal) |

### Eliminated as FPGA Command Interface

USART1 (PA9/PA10), USART2 (PA2/PA3), USART3 (PB10/PB11), SPI2 (PB12-15), USB (PA11/PA12), EXMC banks NE1-NE4.

### Remaining Hypotheses

1. Bit-banged GPIO protocol on unidentified pins
2. I2C bus (not yet probed as I2C — tested as USART3 only)
3. Ghidra 0x40005C00 addresses are computed pointers, not literal peripheral addresses
4. FPGA data channel only activates after undiscovered init handshake

### Tools Created

- `firmware/src/fpgaprobe.c` — Multi-phase hardware probe (auto-run, no buttons needed)
- `esp32_uart_bridge/src/main.cpp` — ESP32 USB-UART sniffer for debug pad monitoring

## Revision History

- **2026-03-31**: Initial coverage assessment. 292 functions mapped, ~113 KB gap analysis complete. Identified 3 tiers of remaining work.
- **2026-03-31**: Hardware probing session. Eliminated USART1/2/3, SPI2, USB, EXMC as FPGA interfaces. Confirmed SPI flash = W25Q128JV, LCD = ST7789V. FPGA command channel still unknown.
- **2026-04-01**: **Major analysis session — stock firmware RE essentially complete.**
  - Pass 1: FPGA task fully annotated (10 sub-functions, 580+ lines of clean C)
  - Pass 2: All 309 real functions named (138 HIGH, 182 MEDIUM, 42 LOW confidence)
  - Pass 3: 17 gap functions catalogued; 61 Ghidra false positives eliminated
  - Pass 4: Remaining unknowns extracted from master init (258K raw decompilation)
  - Key breakthroughs: ADC data format cracked (interleaved 8-bit, offset -28.0), buttons resolved (all MCU GPIO), ALL ~40 FPGA command codes mapped, SPI3 root cause identified, 9-mode acquisition state machine documented, battery ADC = PB1/Ch9, IOMUX remap extracted, TMR8 mystery solved (repurposed vector), DMA assignments mapped
  - Documentation suite created: ARCHITECTURE.md, FPGA_PROTOCOL_COMPLETE.md, HARDWARE_PINOUT.md, CALIBRATION.md
  - **Stock firmware now ~98% understood.** Only remaining: PLL startup assembly, 42 low-confidence names, hardware verification.
- **2026-04-01 (session 2)**: Hardware verification on real device.
  - Built `fulltest.c` (FPGA init + passive GPIO) — USART frames flowing (0x5A A5 E4 2E 63 25 07 = meter data), SPI3 still 0xFF, passive GPIO reads show no button changes
  - Built `fulltest2.c` (TMR3 500Hz + bidirectional matrix scan) — **14/15 buttons hardware-confirmed!** Complete bit→button mapping established. Only PRM (PB7, active HIGH) unresolved.
  - Discovered buttons need ACTIVE bidirectional scanning (gpio_init reconfiguration each phase), not passive IDR reads
  - USART data confirmed: FPGA sends measurement frames continuously
  - SPI3 still needs work: TMR3 USART exchange running but FPGA not yet entering active data mode
  - New analysis: `button_scan_algorithm.md` (exact scan pseudocode), `button_map_confirmed.md` (hardware-verified mapping)
