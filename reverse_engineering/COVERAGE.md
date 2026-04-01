# Firmware Reverse Engineering Coverage Tracker

## Overall Statistics

| Metric | Value |
|--------|-------|
| **Firmware binary** | 734 KB total |
| **Code region** | 252 KB (0x08000238 – 0x0803F2C2) |
| **Data region** | 481 KB (fonts, images, strings, lookup tables) |
| **Functions in function_map** | 292 |
| **Additional real functions (in gaps)** | ~9 (probe detection, FPGA helpers) |
| **C runtime / library functions** | ~5 (printf, math — 0x0803F268+) |
| **Ghidra false positives** | ~56 (halt_baddata in data region 0x08040000+) |
| **Mapped function bytes** | 138 KB (54.5% of code region) |
| **Unmapped code gaps** | 113 KB (45.5% of code region) |

## What's Actually In the Gaps

The 113 KB of "unmapped" code is not missing — it's decompiled in `full_decompile.c` but **not catalogued in function_map.txt**, which only tracked functions found via Ghidra's call-graph analysis. The gaps contain functions reached through indirect calls (jump tables, function pointers, ISR dispatch) that Ghidra's cross-reference engine didn't follow.

### Gap Breakdown

| Gap Range | Size | Likely Contents | Priority |
|-----------|------|-----------------|----------|
| 0x0800F376–0x08015CFC | 26.4 KB | Measurement math, waveform processing, format strings (`%.2fmV`) | Medium — rewriting from scratch |
| 0x08002BE0–0x08008154 | 21.4 KB | Init sequences, peripheral setup, calibration, boot code | **High** — need init sequences |
| 0x08023968–0x0802771C | 15.4 KB | UI rendering, menu system, LCD drawing, `bsp_sys.c`, file path `162.jpg` | Low — rewriting UI |
| 0x08036A4E–0x08039874 | 11.5 KB | FPGA command construction, frame builders, protocol helpers | **High** — need FPGA protocol |
| 0x0800BFF2–0x0800E79C | 9.9 KB | Multimeter mode, range switching, DVOM, file refs (`4.jpg`, `2.jpg`) | Medium — need for meter mode |
| 0x08009B7E–0x0800BCD4 | 8.3 KB | Probe detection (FUN_0800ba06–bc98 confirmed), continuity, component test | Medium |
| 0x08028340–0x08028B80 | 2.1 KB | Unknown — between FPGA and USB regions | Low |
| 0x08022E14–0x08023968 | 2.7 KB | UI/display helpers | Low — rewriting |
| Other small gaps | ~16 KB | Scattered utilities, mode handlers | Low |

### Beyond Code Region

| Range | Size | Contents | Priority |
|-------|------|----------|----------|
| 0x0803F268–0x08045A00 | ~26 KB | C runtime library (printf formatters, floating-point math) | **Skip** — using our own libc |
| 0x08045A00–0x080B7680 | ~460 KB | Data: UI images, fonts, multilingual strings (DE/ES/PT/EN), lookup tables | **Skip** — own assets |

## Subsystem Coverage

### CRITICAL PATH — Must understand to bring up hardware

| Subsystem | Coverage | Key Functions | What's Known | What's Missing | Priority |
|-----------|----------|---------------|--------------|----------------|----------|
| **FPGA init sequence** | 20% | FUN_080278e4 (2566B), gap at 0x08036A4E | Frame format, ACK protocol | Boot-time init commands, register setup, baud rate | **P0** |
| **FPGA command codes** | ~25% | FUN_080278e4, FUN_08039eb4, FUN_08039990 | 9 of ~40 commands mapped (1-5, some prefixes) | Remaining 31 command codes, trigger config, channel setup | **P0** |
| **Button input (9 mystery)** | 10% | USART2 ISR (FUN_080277b4), TMR3 ISR | 6 GPIO buttons mapped; 9 come from elsewhere | FPGA button data format, or I2C touch controller | **P0** |
| **SPI2 data transfer** | 80% | FUN_0802f0c4, FUN_0802f048 | Full SPI read/write protocol, chip select, addressing | Sample data format (8 vs 12-bit, signed/unsigned) | **P1** |
| **ADC sample format** | 30% | FUN_08008154 (2612B) | 250MS/s, dual channel, SPI readout | Packing format, calibration offsets, channel interleaving | **P1** |
| **Power management** | 90% | Boot code | PC9 hold, PB8 backlight, soft power | Battery monitoring ADC channel | P2 |
| **Clock/peripheral init** | 40% | Gap at 0x08002BE0 | EXMC timing, GPIO alternate functions | Full RCC config, PLL settings, peripheral clocks | **P1** |

### REWRITING FROM SCRATCH — Only need protocol understanding

| Subsystem | Coverage | Key Functions | What We Need | Priority |
|-----------|----------|---------------|--------------|----------|
| **Oscilloscope UI** | Identified | FUN_08019e98 (13.3 KB — the monster) | Just the FPGA command interface | Skip deep analysis |
| **Multimeter mode** | 30% | Gap at 0x0800BFF2, FUN_0800e79c | Range-switching GPIO, ADC channel for DVOM | P1 |
| **Signal generator** | 70% | FUN_08001c60, FUN_080018a4, FUN_08001a58 | DAC config, GPIO MUX routing — already well mapped | P2 |
| **UI rendering** | Identified | Gap at 0x08023968 | Nothing — own renderer exists | Skip |
| **Menu/settings** | 0% | Unknown location | Nothing — own UI exists | Skip |
| **File I/O** | 10% | Strings show FAT32 at 0x08029C94 | Nothing — using our own FatFS | Skip |
| **USB** | 5% | USB_LP ISR at 0x0802E8E5 | Nothing — using USB library | Skip |
| **Screenshot/save** | 0% | References to `2:/Screenshot file/` | Nothing — own implementation | Skip |

### WELL MAPPED — Reference only

| Subsystem | Coverage | Key Functions | Notes |
|-----------|----------|---------------|-------|
| **Interrupt vector table** | 100% | 6 active handlers identified | All ISR entry points known |
| **GPIO pin assignments** | 90% | FUN_08001a58, FUN_080018a4 | MUX, relay, signal routing mapped |
| **Data processing pipeline** | 70% | FUN_0803e8d0 hub + 11 processors | Waveform math, display buffer chain |
| **DMA configuration** | 60% | FUN_0803bee0 | DMA1 channel setup for USART2 RX |
| **DAC (signal gen)** | 80% | FUN_08001c60 | Dual-channel 12-bit output |
| **SPI flash access** | 70% | FUN_0802f048, FUN_0802f16c | Read/write protocol, chip select |
| **Timer usage** | 50% | TMR3 ISR, TMR8_BRK | TMR3 = periodic (button scan?), TMR8 = unknown |

## Function Size Distribution

Understanding where complexity lives:

| Size Bucket | Count | Total Bytes | % of Code | Notable Functions |
|-------------|-------|-------------|-----------|-------------------|
| Monster (>5KB) | 5 | 35,726 | 25% | FUN_08019e98 (13.3KB scope FSM), FUN_08030524 (6.5KB), FUN_0802a664 (5.6KB), FUN_08015f50 (5.0KB), FUN_0801f6f8 (4.6KB) |
| Large (1-5KB) | 40 | 67,000 | 48% | Mode handlers, data processors, SPI/FPGA drivers |
| Medium (256B-1KB) | 80 | 28,000 | 20% | Utilities, GPIO config, protocol helpers |
| Small (<256B) | 167 | 10,110 | 7% | Leaf functions, register accessors, tiny helpers |

The **top 5 functions contain 25% of all code**. FUN_08019e98 alone is 13.3 KB — the main oscilloscope state machine with 27 callees. This is where the scope UI logic, mode switching, channel config, trigger setup, and measurement coordination all happen.

## Named Functions (Semantic Understanding)

Functions where purpose has been identified through hardware register analysis:

| Address | Size | Proposed Name | Confidence | Evidence |
|---------|------|---------------|------------|----------|
| 080277b4 | 304 | `usart2_isr` | High | USART2 register access, TX/RX buffer management |
| 0802f0c4 | 86 | `spi2_transceive_byte` | High | SPI2_STAT polling, SPI2_DATA read/write |
| 0802f048 | 112 | `spi2_block_read` | High | CS assert, 3-byte address, bulk read loop |
| 08001a58 | 506 | `gpio_mux_porta_portb` | High | GPIOA/B BOP/BCR writes for analog routing |
| 080018a4 | 422 | `gpio_mux_portc_porte` | High | GPIOC/E BOP/BCR writes for analog routing |
| 08001c60 | 1634 | `siggen_configure` | High | DAC register config + GPIO MUX calls |
| 08019e98 | 13276 | `scope_main_fsm` | High | 27 callees, DAC/GPIO/FPGA access, largest function |
| 08008154 | 2612 | `adc_measurement_core` | Medium | Called by 13 mode handlers, accesses ADC-related data |
| 080012bc | 68 | `delay_or_yield` | Medium | Called by 12 functions, no callees (likely timing) |
| 0803bee0 | 358 | `dma1_configure` | High | DMA1 register setup (channel, address, count) |
| 08032f6c | 520 | `measurement_dispatch` | Medium | Called by 11 mode handlers, calls display + data funcs |
| 0803e8d0 | 152 | `display_buffer_write` | Medium | Called by 11 data processors (rendering hub) |
| 08033cfc | 508 | `display_update` | Medium | Called by 16 functions (display refresh) |
| 0803acf0 | 536 | `xQueueGenericSend` | **High** | FreeRTOS kernel — NOT fpga_send_command. Puts items on queue. |
| 0803b09c | 316 | `xQueueGenericSendFromISR` | **High** | FreeRTOS kernel — NOT usart2_rx_process. Called from ISR context. |
| 0803b1d8 | 462 | `xQueueReceive` | **High** | FreeRTOS kernel — blocking queue receive. 6 callers total. |
| 080278e4 | 2566 | `usb_endpoint_handler` | **High** | Accesses 0x40005C00 (USB_CNTR) and 0x40006000 (USB packet buffer). NOT FPGA task. |
| 08036934 | ~11000 | `fpga_task_real` | **High** | **THE REAL FPGA TASK.** Calls xQueueReceive 4x. Accesses SPI3 (0x40003C08), USART2, GPIOB/C. In 11.5KB gap. |
| 08002670 | 1544 | `dma_lcd_transfer` | **High** | DMA0 CH1 setup: blasts framebuffer from 0x2000835C → LCD (0x60020000). NOT FPGA. |
| 08004d60 | 3568 | `meter_state_machine` | **High** | 10-way dispatch (TBH) for meter modes. Base r5=0x200000F8. In unmapped gap. |
| 08039eb4 | 368 | `fpga_frame_builder` | Medium | Called by FPGA task and SPI functions |
| 0802771c | 110 | `tmr3_setup_or_handler` | Medium | TIM3 register access |
| 0800ba06 | 102 | `probe_detect_handler_1` | Medium | GPIOC IDR read (PC0 = probe continuity) |
| 08037800 | 472 | `fpga_spi3_transfer` | Medium | GPIOB writes (SPI CS) — likely SPI3, not SPI2 |
| 08034078 | 626 | `scope_display_refresh` | Medium | Called by scope FSM, calls data processors |
| 080003b4 | ~40 | `sprintf_to_buffer` | **High** | String formatter — confirmed from v2 decompile naming |
| 08008154 | 2612 | `display_render_engine` | **High** | LCD layout/glyph renderer — NOT adc_measurement_core |

## What to Dive Into Next

### Tier 0 — UNBLOCKED by SPI3 discovery

1. **Fully disassemble FUN_08036934** (~11KB, the real FPGA task) — Extract SPI3 configuration (clock speed, CPOL/CPHA, CS polarity), USART2 command format, and the complete command dispatch. This is the single most valuable RE target remaining.

2. **Find JTAG-disable / SPI3 pin setup** in boot init (0x08002BE0 gap) — PB3/PB4 are JTAG pins by default. The IOMUX remap that frees them for SPI3 is in the boot code. Also find SPI3 clock enable (CRM register write) and baud rate divisor.

3. **Write SPI3 test firmware** — Initialize SPI3 on PB3/PB4/PB5 with PB6 CS, try reading from FPGA. Can be done purely in software — no pin probing needed.

### Tier 1 — Blocks hardware bring-up

4. **FPGA command code table** — Trace all items queued to the FPGA task (all 150+ callers of xQueueGenericSend that target the FPGA queue handle). Extract every command code.

5. **Button input path** — GPIOC_IDR (0x40011008) is read 3 times in the FPGA task. May carry button state from FPGA. Also check TMR3 ISR for I2C polling.

6. **ADC sample data format** — Now known to come via SPI3 (not SPI2). Trace the SPI3 read path in FUN_08036934 to determine sample width, packing, channel interleaving.

### Tier 2 — Needed for full functionality

7. **Multimeter DVOM path** — Meter state machine at FUN_08004d60 (3568 bytes). Data arrives via FPGA task, processed by FUN_080028e0, displayed by FUN_0800ec70. Need to trace the float write path.

8. **Peripheral clock init** — Gap at 0x08002BE0. Full CRM configuration including SPI3 clock enable.

9. **USART2 baud rate** — Find BRR register write in init code. May be 9600 (matching heartbeat) or different for TX.

### Tier 3 — Can sidestep entirely

8. **UI rendering functions** (gap at 0x08023968, ~15 KB) — We have our own LCD driver and font renderer. No need to reverse-engineer theirs.

9. **Measurement math** (gap at 0x0800F376, ~26 KB) — Format strings, unit conversion, waveform statistics. Straightforward to reimplement.

10. **File I/O / FatFS** (FAT32 code around 0x08029C94) — Using our own FatFS library. Skip.

11. **USB mass storage** (USB ISR + helpers) — Using a USB library. Skip.

12. **C runtime** (0x0803F268+, ~26 KB) — printf, floating-point. Provided by newlib/picolibc. Skip.

13. **Multilingual string tables** (0x080B+) — We have our own UI strings. Skip.

14. **Screenshot/save** — Own implementation. Skip.

## Key Unknowns (Hardware Questions)

| Question | How to Answer | Status |
|----------|--------------|--------|
| **FPGA data interface** | Binary analysis of FPGA task | **ANSWERED: SPI3 (0x40003C00) on PB3/PB4/PB5. Needs hardware verification.** |
| FPGA command interface | USART2 + SPI3 init sequence | **LIKELY ANSWERED: USART2 for commands, SPI3 for bulk data. Dual-interface.** |
| SPI3 clock speed | Disassemble FUN_08036934 or write test firmware | Unknown — extract from SPI3_CTL0 config in FPGA task |
| SPI3 CS polarity | Trace GPIOB_BOP writes in FUN_08036934 | Likely PB6 active-low (3 GPIOB_BOP refs) |
| USART2 baud rate | Read BRR register from init code | **9600 baud confirmed** on heartbeat output |
| SPI2 destination | Hardware probing | **ANSWERED: SPI2 + PB12 = Winbond W25Q128JV SPI flash** |
| Sample data format | Trace SPI3 read path in FPGA task | Unknown — now tractable via FUN_08036934 disassembly |
| Which 9 buttons come from FPGA | Check GPIOC_IDR reads in FPGA task, or SPI3 data | **LIKELY via FPGA** — GPIOC_IDR read 3x in FPGA task |
| JTAG disable for SPI3 | Find IOMUX remap in boot init (0x08002BE0) | PB3/PB4 are JTAG by default, must be remapped |
| Battery voltage ADC channel | Trace ADC config in init code | Unknown |

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
