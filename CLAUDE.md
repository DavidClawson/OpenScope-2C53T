# FNIRSI 2C53T Open-Source Firmware Project

## What This Is

Reverse engineering and clean-room rewrite of the firmware for the **FNIRSI 2C53T**, a 3-in-1 handheld oscilloscope/multimeter/signal generator. The original firmware was decompiled from binary using Ghidra and is being refactored into clean, modular C.

## Hardware Target

- **MCU:** GD32F307VET6 — ARM Cortex-M4F @ 120MHz, 1MB flash, 256KB SRAM
- **LCD:** ST7789V 320x240 RGB565 via 16-bit parallel EXMC/FSMC bus
- **FPGA:** Gowin GW1N-UV2 (markings sanded, identified by EEVblog community) — handles 250MS/s ADC sampling. Open-source tooling exists (Yosys/nextpnr/Apicula)
- **SPI Flash:** Winbond W25Q128JVSQ (16MB) — UI assets and system files
- **DAC:** 2-channel 12-bit (built-in GD32) for signal generator output
- **Buttons:** 15 physical buttons (CH1, CH2, MOVE, SELECT, TRIGGER, PRM, AUTO, SAVE, MENU, arrows, OK, POWER)
- **Touch:** I2C interface (likely GT911/GT915)

## Repository Layout

```
firmware/           # Custom replacement firmware (GCC + Make)
  src/main.c        # FreeRTOS tasks, LCD init, UI event loop, mode switching
  src/drivers/      # lcd.c / lcd.h — ST7789V driver via EXMC
  include/          # FreeRTOSConfig.h
  FreeRTOS/         # FreeRTOS kernel (submodule)
  gd32f30x_lib/     # GD32 HAL library (submodule)
  ld/               # Linker script (gd32f307.ld)
  Makefile          # Build: make, make emu, make renode, make disasm

reverse_engineering/  # Ghidra decompilation artifacts
  decompiled_2C53T.c      # V1.2.0 decompilation (~35K lines, 292+ functions)
  decompiled_2C53T_v2.c   # Updated with named functions (~39K lines)
  strings_with_addresses.txt  # 290 strings mapped to firmware addresses
  ghidra_scripts/         # 6 Java automation scripts

emulator/           # Simulation infrastructure
  renode/           # Full-system emulation (GD32F307 platform + peripherals)
  renode_lcd_bridge.py  # WebSocket bridge: Renode framebuffer → React frontend
  emu_2c53t.py      # Unicorn-based emulator (limited — no NVIC/SysTick)

frontend/           # React web UI for LCD display simulation (Vite)
docs/               # 24+ design/analysis documents
ghidra_project/     # Pre-analyzed Ghidra database (V1.2.0)
modules/            # JSON procedure files (automotive, HVAC, ham radio, education)
APP_2C53T_*.bin     # Original firmware binaries (V1.0.3, V1.0.7)

FNIRSI_1013D_Firmware/      # Submodule: pecostm32's 1013D replacement firmware
FNIRSI_1014D_Firmware/      # Submodule: 1014D replacement firmware
FNIRSI-1013D-1014D-Hack/    # Submodule: historical RE work
```

## Build & Run

```bash
# Build firmware (cross-compile for GD32F307)
cd firmware && make

# Build for emulator (skips HAL clock init)
make emu

# Run in Renode (display-only, no input)
make renode

# Run interactive emulator (keyboard → GPIO button input)
# Terminal 1:
make renode-interactive
# Terminal 2:
cd emulator && ./lcd_viewer

# Smoke test (5 seconds, headless)
make renode-test

# Disassembly listing
make disasm
```

**Toolchain:** `arm-none-eabi-gcc` via Homebrew cask `gcc-arm-embedded` (native ARM64)
**Renode:** Expected at `/Applications/Renode.app`
**SDL3 viewer:** `cd emulator && make` (requires `brew install sdl3`)

## Architecture

- **RTOS:** FreeRTOS with Cortex-M4 port (120MHz tick, 1000Hz, 32KB heap)
- **Original firmware tasks:** Display, Input, Acquisition, Measurement, USB, FPGA
- **Current custom firmware:** Display task + Input task, 4 UI modes (scope, meter, signal gen, settings)
- **LCD interface:** Memory-mapped at 0x6001FFFE (command) / 0x60020000 (data), address line A17 selects RS/DCX
- **Font system:** Variable-width bitmap fonts at 4 sizes (12/16/24/48px), generated from TTF via `scripts/generate_font.py`
- **Theme system:** 4 color themes (Dark Blue, Classic Green, High Contrast, Night Red), switchable in Settings > Display Mode
- **Emulator display:** SDL3 native viewer reads `/tmp/openscope_fb.bin` at 30fps; interactive GPIO via `/tmp/openscope_buttons.txt`

## Key Conventions

- Target is GD32F307 (STM32F1-compatible but NOT identical — use GD32 HAL, not STM32 HAL)
- All display rendering is RGB565 (16-bit color)
- Firmware binaries are raw ARM code, not encrypted or compressed
- The decompiled source uses Ghidra naming conventions (FUN_, DAT_, etc.) — rename as functions are understood
- GPL v3 license

## RE Reference

- Largest decompiled functions: UI event loop, waveform processing, signal generator, multimeter mode
- Key data addresses: DAT_20008350/352 (display buffer pointers), various mode/state flags
- Filesystem paths in firmware: `2:/Screenshot file/`, `3:/System file/`
- Firmware versions analyzed: V1.0.3 → V1.0.7 → V1.1.2 → V1.2.0
- V1.2.0 added EXTI3 interrupt (continuity buzzer detection)

## Current State

- Custom firmware boots in Renode through FreeRTOS scheduler (~167KB / 1MB flash)
- LCD driver functional with multi-size font system (4 sizes from SF Pro + Menlo)
- 4 themed UI modes: oscilloscope (with FFT/waterfall), multimeter (large digits), signal generator, navigable settings
- Theme switching (4 themes) wired through all screens
- SDL3 native LCD viewer with interactive button input (replaces React/WebSocket pipeline)
- Soak testing infrastructure (random button fuzzing with fault monitoring)
- Watchdog, health monitoring, task stack checking
- No real hardware flashing yet — development is emulator-first (device arriving ~early April 2026)
