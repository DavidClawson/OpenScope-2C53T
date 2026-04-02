# FNIRSI 2C53T Open-Source Firmware Project

## What This Is

Reverse engineering and clean-room rewrite of the firmware for the **FNIRSI 2C53T**, a 3-in-1 handheld oscilloscope/multimeter/signal generator. The original firmware was decompiled from binary using Ghidra and is being refactored into clean, modular C.

## Hardware Target

- **MCU:** Artery AT32F403A — ARM Cortex-M4F @ 240MHz, 1MB flash, 224KB SRAM (with EOPB0=0xFE)
  - Originally identified as GD32F307 from firmware analysis; physical teardown revealed AT32 (markings sanded off)
  - Register-compatible with GD32/STM32F1 at the GPIO/EXMC level
- **LCD:** ST7789V 320x240 RGB565 via 16-bit parallel EXMC/XMC bus
- **FPGA:** Gowin GW1N-UV2 (non-volatile, retains bitstream across power cycles) — handles 250MS/s ADC sampling via **SPI3 data + USART2 commands**
- **SPI Flash:** Winbond W25Q128JVSQ (16MB) — UI assets and system files
- **DAC:** 2-channel 12-bit (built-in) for signal generator output
- **Buttons:** 15 physical buttons — 4x3 bidirectional GPIO matrix (PA7/PB0/PC5/PE2 × PA8/PC10/PE3) + 3 passive (PC8=POWER, PB7=PRM, PC13=UP). 14/15 hardware-confirmed. TMR3 ISR at 500Hz.
- **Touch:** I2C interface (likely GT911/GT915) — not used for button input
- **Board revision:** 2C53T-V1.4

### Confirmed Pin Assignments
| Function | Pin | Notes |
|----------|-----|-------|
| Power hold | PC9 | Must be HIGH immediately at boot or device shuts off |
| LCD backlight | PB8 | HIGH to enable |
| FPGA SPI3 data | PB3 (SCK), PB4 (MISO), PB5 (MOSI) | Bulk ADC data from FPGA (JTAG pins, remapped) |
| FPGA SPI3 CS | PB6 (GPIO) | Software chip select for FPGA SPI3 |
| FPGA SPI enable | PC6 (GPIO) | Must be HIGH for FPGA SPI3 to work |
| FPGA active mode | PB11 (GPIO) | Set HIGH during active measurement mode |
| FPGA USART cmd | PA2 (TX), PA3 (RX) | 9600 baud, 10-byte TX / 12-byte RX frames |
| SWD | PA13 (SWDIO), PA14 (SWCLK) | Debug header near USB-C port |
| Battery sense | PB1 (ADC1 Ch9) | 239.5-cycle sample time, software-triggered |
| UART debug | RX, TX, GND | Through-hole pads (not yet mapped to MCU pins) |
| FPGA programming | M0-M3, GND, VDD, VPP | Header for Gowin programmer |
| Pinhole reset | NRST | Accessible from outside case |

## Repository Layout

```
firmware/           # Custom replacement firmware (GCC + Make)
  src/main.c        # FreeRTOS tasks, LCD init, UI event loop, mode switching
  src/drivers/      # lcd.c / lcd.h — ST7789V driver via EXMC
  include/          # FreeRTOSConfig.h
  FreeRTOS/         # FreeRTOS kernel (submodule)
  at32f403a_lib/    # Artery AT32 HAL library (clone from ArteryTek GitHub)
  gd32f30x_lib/     # GD32 HAL library (legacy, kept for emulator builds)
  bootloader/       # USB HID IAP bootloader (16KB at 0x08000000)
  ld/               # Linker scripts (at32f403a_app.ld for hardware, at32f403a.ld for emu)
  Makefile          # Build: make, make flash, make flash-all, make emu
  Makefile.hwtest   # Minimal hardware test build

reverse_engineering/  # Ghidra decompilation artifacts
  COVERAGE.md             # RE coverage tracker: 309 real functions, fully catalogued
  analysis_v120/          # Latest analysis: full_decompile.c, hardware_map, xref_map, RAM map, FPGA protocol
    fpga_task_annotated.c     # Annotated FPGA task (10 sub-functions, 580+ lines)
    FPGA_TASK_ANALYSIS.md     # FPGA protocol analysis: SPI3 format, command table, state machine
    function_names.md         # Complete function naming inventory (309 real + 61 false positives)
    gap_functions.md          # 17 gap functions catalogued with priorities
    function_map_complete.txt # Complete 309-entry function map
  decompiled_2C53T.c      # V1.2.0 decompilation (~35K lines, 292+ functions)
  decompiled_2C53T_v2.c   # Updated with named functions (~39K lines)
  strings_with_addresses.txt  # 290 strings mapped to firmware addresses
  ghidra_scripts/         # 14 Java automation scripts

emulator/           # Simulation infrastructure
  renode/           # Full-system emulation (GD32F307 platform + peripherals)
  renode_lcd_bridge.py  # WebSocket bridge: Renode framebuffer → React frontend
  emu_2c53t.py      # Unicorn-based emulator (limited — no NVIC/SysTick)

frontend/           # React web UI for LCD display simulation (Vite)
docs/               # 33 design/analysis/planning documents (see docs/README.md for full index)
ghidra_project/     # Pre-analyzed Ghidra database (V1.2.0)
modules/            # JSON procedure files (automotive, HVAC, ham radio, education)
APP_2C53T_*.bin     # Original firmware binaries (V1.0.3, V1.0.7)

FNIRSI_1013D_Firmware/      # Submodule: pecostm32's 1013D replacement firmware
FNIRSI_1014D_Firmware/      # Submodule: 1014D replacement firmware
FNIRSI-1013D-1014D-Hack/    # Submodule: historical RE work
```

## Build & Run

### Hardware (AT32F403A target)
```bash
cd firmware && make              # Build for real hardware (AT32 @ 240MHz)
make flash                       # Flash via USB HID bootloader (case closed)
make flash-all                   # Flash bootloader + app via DFU (first-time setup)
make flash-dfu                   # Flash app via ROM DFU (fallback, BOOT0 + reset)
```

### Normal dev cycle (case closed)
1. On device: Settings → Firmware Update (shows "BOOTLOADER MODE" screen)
2. On host: `cd firmware && make flash`
3. Device auto-reboots into updated firmware

### First-time setup
1. Set EOPB0 for 224KB SRAM (one-time): enter DFU mode (BOOT0 + reset), then:
   `dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin`
2. Flash bootloader + app: `make flash-all`
3. Close the case — all future updates go over USB-C

### Flash layout
```
0x08000000  Bootloader (16KB) — permanent, USB HID IAP
0x08003800  Upgrade flag (2KB sector)
0x08004000  Application (1008KB) — updated via make flash
```

### Emulator
```bash
make emu                         # Build for emulator (skips AT32 clock init)
make renode                      # Run in Renode (display-only)
make renode-interactive          # Run with keyboard input (use with lcd_viewer)
make renode-test                 # 5-second smoke test
```

**Toolchain:** `arm-none-eabi-gcc` via Homebrew cask `gcc-arm-embedded` (native ARM64)
**DFU tool:** `dfu-util` via `brew install dfu-util`
**Renode:** Expected at `/Applications/Renode.app`
**SDL3 viewer:** `cd emulator && make` (requires `brew install sdl3`)
**Logic analyzer:** `sigrok-cli` via `brew install sigrok-cli` — drives HiLetgo 24MHz 8CH USB analyzer (fx2lafw driver)

## Architecture

- **RTOS:** FreeRTOS with Cortex-M4 port (240MHz tick, 1000Hz, 32KB heap)
- **Original firmware tasks:** Display, Input, Acquisition, Measurement, USB, FPGA
- **Current custom firmware:** Display task + Input task, 4 UI modes (scope, meter, signal gen, settings)
- **LCD interface:** Memory-mapped at 0x6001FFFE (command) / 0x60020000 (data), address line A17 selects RS/DCX
- **EXMC config:** SNCTL0=0x5011, SNTCFG0=0x02020424, SNWTCFG0=0x00000202 (works at 240MHz)
- **Font system:** Variable-width bitmap fonts at 4 sizes (12/16/24/48px), generated from TTF via `scripts/generate_font.py`
- **Theme system:** 4 color themes (Dark Blue, Classic Green, High Contrast, Night Red), switchable in Settings > Display Mode
- **Emulator display:** SDL3 native viewer reads `/tmp/openscope_fb.bin` at 30fps; interactive GPIO via `/tmp/openscope_buttons.txt`

## Key Conventions

- Target is Artery AT32F403A (STM32F1-compatible registers for GPIO/EXMC, but uses AT32 HAL for clock/peripheral init)
- IOMUX (AFIO) clock MUST be enabled for EXMC alternate function pins to work
- Power hold (PC9 HIGH) must be the very first operation in main()
- All display rendering is RGB565 (16-bit color)
- Firmware binaries are raw ARM code, not encrypted or compressed
- The decompiled source uses Ghidra naming conventions (FUN_, DAT_, etc.) — rename as functions are understood
- GPL v3 license

## RE Reference

- **Coverage:** 309 real functions fully decompiled and named (138 HIGH, 182 MEDIUM, 42 LOW confidence). 61 Ghidra false positives eliminated.
- **FPGA interface:** Dual-channel — SPI3 (60MHz) for bulk ADC data, USART2 (9600 baud) for commands. Fully annotated.
- **FPGA task:** 10 sub-functions across 11.5KB, annotated in `analysis_v120/fpga_task_annotated.c`
- **SPI3 data format:** Interleaved CH1/CH2 unsigned 8-bit. Normal=1024B (512 pairs), dual=2048B. ADC offset=-28.0.
- **SPI3 config:** Mode 3 (CPOL=1, CPHA=1), Master, /2 clock (60MHz), 8-bit, software CS on PB6
- **USART protocol:** 10-byte TX frames (header + cmd + params + checksum), 12-byte RX (0x5A 0xA5 data, 0xAA 0x55 echo). Timer-driven via TMR3.
- **FPGA command codes:** ALL ~40 mapped (0x00-0x2C) — scope, trigger, timebase, meter, siggen, freq counter, period, duty cycle, continuity/diode. Dispatch table at 0x0804BE74.
- **FPGA control pins:** PC6 = SPI enable (HIGH), PB11 = active mode (HIGH)
- **Buttons: 14/15 HARDWARE CONFIRMED.** Bidirectional 4x3 matrix + 3 passive. See `analysis_v120/button_map_confirmed.md` for complete mapping. Only PRM (PB7) unresolved.
- **Acquisition:** Double-buffered (2 queue items per trigger), 9-mode state machine (fast TB, roll, normal, dual, extended, meter ADC, siggen, calibration, self-test)
- **Calibration:** ADC offset -28.0, per-channel VFP pipeline, 301-byte cal data loaded from SPI flash per channel
- **Meter data:** BCD digit extraction from cross-byte nibbles in USART RX frames, 8-state mode FSM
- **Boot sequence:** 53-step init documented in `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md`
- **Master init:** FUN_08023A50 (15.4KB) — configures all peripherals, creates all FreeRTOS tasks
- **8 FreeRTOS tasks:** display, key, osc, fpga, dvom_TX, dvom_RX, Timer1, Timer2
- **7 FreeRTOS queues:** usart_cmd (0x20002D6C), button_event (0x20002D70), usart_tx (0x20002D74), spi3_data (0x20002D78), meter_sem (0x20002D7C), fpga_sem1 (0x20002D80), fpga_sem2 (0x20002D84)
- **Auto power-off:** 3 tiers (15min/30min/1hr) based on probe state
- **Watchdog:** IWDG fed every 11 calls to input_and_housekeeping (~50ms)
- Calibration tables in RAM: 6 gain/offset pairs at 0x20000358–0x20000434 (loaded from SPI flash at boot)
- Filesystem paths in firmware: `2:/Screenshot file/`, `3:/System file/`
- Firmware versions analyzed: V1.0.3 → V1.0.7 → V1.1.2 → V1.2.0
- **IOMUX remap:** `(reg & ~0xF000) | 0x2000` at AFIO+0x08 — disables JTAG-DP, keeps SW-DP, frees PB3/PB4/PB5 for SPI3
- **Battery ADC:** PB1 / ADC1 Channel 9, 239.5-cycle sample, right-aligned, software-triggered
- **TMR8:** Vector repurposed for FatFs. TMR8 hardware is unused.
- **DMA:** Ch1 = LCD framebuffer (16-bit mem-to-mem → EXMC). SPI3 = polled. USART2 = interrupt-driven.
- **Key docs:** `reverse_engineering/ARCHITECTURE.md` (start here), `FPGA_PROTOCOL_COMPLETE.md`, `HARDWARE_PINOUT.md`, `CALIBRATION.md`, `analysis_v120/FPGA_TASK_ANALYSIS.md`

## Current State

- **Full oscilloscope UI running on real FNIRSI 2C53T hardware** (AT32F403A @ 240MHz, battery powered)
- LCD driver functional with multi-size font system (4 sizes from SF Pro + Menlo)
- 4 themed UI modes: oscilloscope (with FFT/waterfall), multimeter (large digits), signal generator, navigable settings
- Theme switching (4 themes) wired through all screens
- FreeRTOS scheduler running with display + input tasks
- Power management: PC9 hold, PB8 backlight, POWER button 3-2-1 countdown shutdown, low-battery auto-off at 3.3V
- **USB HID bootloader** — closed-case firmware updates via `make flash`, LCD status screen, POWER button exit, auto-reboot after flash
- Battery monitor: PB1 ADC with 16-sample averaging, percentage display, USB charge detection ("CHG"), LiPo protection shutdown
- SDL3 native LCD viewer with interactive button input for emulator
- Soak testing infrastructure (random button fuzzing with fault monitoring)
- Watchdog, health monitoring, task stack checking
- **Button input: 14/15 HARDWARE CONFIRMED** — bidirectional 4x3 matrix scan at 500Hz via TMR3 ISR. Rows: PA7,PB0,PC5,PE2. Cols: PA8,PC10,PE3. Passive: PC8(POWER),PB7(PRM),PC13(UP). Complete mapping in `analysis_v120/button_map_confirmed.md`. PRM (PB7) detection still needs work.
- **FPGA USART communication working** — bidirectional, frames captured, meter data flowing
- **FPGA SPI3 root cause identified** — was missing: PB11 HIGH (active mode), full USART boot command sequence (0x01-0x08), queue-driven triggering (not polled), SysTick delays between boot phases. See `FPGA_TASK_ANALYSIS.md`
- **Stock firmware ~98% understood** — 309 functions named, ALL FPGA commands mapped, ADC format cracked, button input resolved, battery ADC found, IOMUX remap extracted, TMR8 mystery solved. Remaining: PLL startup assembly, 42 low-confidence function names.
