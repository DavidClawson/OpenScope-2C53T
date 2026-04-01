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
- **Buttons:** 15 physical buttons (CH1, CH2, MOVE, SELECT, TRIGGER, PRM, AUTO, SAVE, MENU, arrows, OK, POWER)
- **Touch:** I2C interface (likely GT911/GT915)
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
  ld/               # Linker scripts (at32f403a.ld for hardware, gd32f307.ld for emulator)
  Makefile          # Build: make, make emu, make renode, make flash-dfu
  Makefile.hwtest   # Minimal hardware test build

reverse_engineering/  # Ghidra decompilation artifacts
  COVERAGE.md             # RE coverage tracker: 362 functions, 45.5% gap analysis, priority tiers
  analysis_v120/          # Latest analysis: full_decompile.c, hardware_map, xref_map, RAM map, FPGA protocol
  decompiled_2C53T.c      # V1.2.0 decompilation (~35K lines, 292+ functions)
  decompiled_2C53T_v2.c   # Updated with named functions (~39K lines)
  strings_with_addresses.txt  # 290 strings mapped to firmware addresses
  ghidra_scripts/         # 6 Java automation scripts

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
make flash-dfu                   # Flash via USB DFU (device must be in DFU mode)
```

### Flashing procedure
1. Open case, hold BOOT0 resistor pad to 3V3
2. Press pinhole reset button on side of case
3. Release BOOT0 — device enumerates as "AT32 Bootloader DFU"
4. `dfu-util -a 0 -d 2e3c:df11 -s 0x08000000:leave -D firmware/build/firmware.bin`

### First-time setup (EOPB0 for 224KB SRAM)
The AT32 defaults to 96KB SRAM. The firmware requires 224KB. Set EOPB0 once:
```bash
# Enter DFU mode, then:
dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin
# Then flash firmware normally
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

- **Coverage:** 362 functions decompiled, 138KB mapped (54.5%), 113KB in gaps (see `reverse_engineering/COVERAGE.md`)
- **FPGA interface:** SPI3 (0x40003C00) on PB3/PB4/PB5 for bulk data, USART (0x40004400) on PA2/PA3 for commands
- **FPGA task:** FUN_08036934 is actually 10 sub-functions (~12KB total), comprehensively decompiled
- **SPI3 config:** Mode 3 (CPOL=1, CPHA=1), Master, /2 clock (60MHz), 8-bit, software CS on PB6
- **USART protocol:** 10-byte TX frames, 12-byte RX frames (0x5A 0xA5 header), 9600 baud, timer-driven (not USART IRQ)
- **FPGA control pins:** PC6 = SPI enable (HIGH), PB11 = active mode (HIGH)
- **Hardware verified:** PB4 (MISO) physically connected to FPGA (GPIO test confirmed)
- **Boot sequence:** 53-step init documented in `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md`
- **8 FreeRTOS tasks:** display, key, osc, fpga, dvom_TX, dvom_RX, Timer1, Timer2
- **Comprehensive decompilation:** 12,700 lines across 3 files (init function, FPGA task, USART protocol)
- Calibration tables in RAM: 6 gain/offset pairs at 0x20000358–0x20000434 (loaded from SPI flash at boot)
- Filesystem paths in firmware: `2:/Screenshot file/`, `3:/System file/`
- Firmware versions analyzed: V1.0.3 → V1.0.7 → V1.1.2 → V1.2.0
- **Key docs:** `docs/fpga_protocol.md`, `docs/peripheral_map.md`, `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md`

## Current State

- **Full oscilloscope UI running on real FNIRSI 2C53T hardware** (AT32F403A @ 240MHz, battery powered)
- LCD driver functional with multi-size font system (4 sizes from SF Pro + Menlo)
- 4 themed UI modes: oscilloscope (with FFT/waterfall), multimeter (large digits), signal generator, navigable settings
- Theme switching (4 themes) wired through all screens
- FreeRTOS scheduler running with display + input tasks
- Power management: PC9 hold, PB8 backlight, soft power button
- DFU flashing via USB (BOOT0 + pinhole reset)
- SDL3 native LCD viewer with interactive button input for emulator
- Soak testing infrastructure (random button fuzzing with fault monitoring)
- Watchdog, health monitoring, task stack checking
- Button input on hardware not yet mapped (pin assignments need verification)
- **FPGA USART communication working** — bidirectional, frames captured, meter data flowing
- **FPGA SPI3 not yet responding** — hardware connected (verified), correct mode/pins/control signals set, but FPGA holds MISO HIGH. Likely needs timer-generated hardware trigger or specific init sequence timing. See `FPGA_BOOT_SEQUENCE.md`
