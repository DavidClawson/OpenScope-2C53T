# OpenScope 2C53T

**Open-source replacement firmware for the FNIRSI 2C53T handheld oscilloscope / multimeter / signal generator.**

<p align="center">
  <img src="scope.jpg" alt="FNIRSI 2C53T" width="300">
</p>

The FNIRSI 2C53T is a capable $75 handheld instrument held back by buggy stock firmware — UI freezes, unreliable triggering, measurement inaccuracies, and limited features. This project is a complete clean-room firmware rewrite built from reverse engineering the original binary.

**Status:** Running on real hardware. Oscilloscope UI, multimeter, signal generator, and settings screens all functional. FPGA communication working. Active development.

## What's Different from Stock

- Rewritten UI with 4 color themes (no more freezing)
- FreeRTOS-based architecture with proper task isolation
- Variable-width bitmap fonts at 4 sizes
- FFT / spectrum analysis view
- Math channels (CH1+CH2, CH1-CH2, CH1*CH2)
- XY mode, roll mode, trend plotting
- Protocol decoding (UART, SPI, I2C, CAN, K-Line)
- Component tester (ESR, capacitance, inductance)
- Bode plot analysis
- Persistence display mode
- Soft power management with watchdog
- 4 button-navigable UI modes with settings persistence

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | Artery AT32F403A — ARM Cortex-M4F @ 240MHz, 1MB flash, 224KB SRAM |
| **Display** | ST7789V 320x240 RGB565 via 16-bit parallel bus |
| **FPGA** | Gowin GW1N-UV2 — handles 250MS/s ADC sampling |
| **ADC** | Dual-channel, 8-bit, 250MS/s via FPGA SPI3 |
| **Signal Gen** | 2-channel 12-bit DAC |
| **Flash** | Winbond W25Q128JVSQ (16MB) — UI assets and calibration |
| **Input** | 15 buttons (4x3 scanned matrix + 3 passive) |

> The MCU markings are sanded off. We identified it as AT32F403A through register probing and firmware analysis — it's register-compatible with GD32/STM32F1 at the GPIO level.

## Quick Start

### Prerequisites

```bash
# macOS (Homebrew)
brew install --cask gcc-arm-embedded    # ARM toolchain
brew install dfu-util                    # USB DFU flasher

# Clone with submodules
git clone --recursive https://github.com/DavidClawson/open-scope-2c53t.git
cd open-scope-2c53t

# Clone required HAL library
cd firmware
git clone https://github.com/ArteryTek/AT32F403A_407_Firmware_Library.git at32f403a_lib
```

### Build

```bash
cd firmware
make            # Build for hardware (AT32 @ 240MHz)
```

### Flash

1. Open the case (4 Phillips screws on back)
2. Hold the BOOT0 pad to 3V3 while pressing the pinhole reset button
3. Release BOOT0 — device enumerates as "AT32 Bootloader DFU"
4. Flash:

```bash
make flash-dfu
# Or manually:
# dfu-util -a 0 -d 2e3c:df11 -s 0x08000000:leave -D build/firmware.bin
```

> **First time only:** The AT32 defaults to 96KB SRAM. This firmware needs 224KB. You must set EOPB0 once — see [EOPB0 Setup](#eopb0-setup) below.

### Emulator (no hardware required)

```bash
make emu         # Build for emulator
make renode      # Run in Renode with LCD display
```

Requires [Renode](https://renode.io/) installed at `/Applications/Renode.app`.

## EOPB0 Setup (First Time)

The AT32F403A defaults to 96KB SRAM. This firmware requires the full 224KB, enabled by setting EOPB0 to 0xFE. This only needs to be done once per device:

```bash
# Enter DFU mode (BOOT0 + reset), then:
dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin
# Then flash firmware normally with make flash-dfu
```

## Project Structure

```
firmware/               Custom replacement firmware (C + FreeRTOS)
  src/main.c            FreeRTOS tasks, LCD init, mode switching
  src/drivers/          LCD (EXMC), buttons (matrix scan), watchdog, DFU
  src/ui/               Scope, meter, siggen, settings, themes, FFT, XY, persistence
  src/dsp/              FFT, math channels, signal generator, Bode plot
  src/decode/           Protocol decoders (UART, SPI, I2C, CAN, K-Line)
  src/tasks/            Input handler, measurement engine, component test, mask test
  src/modules/          Application modules (alternator test, compression test)
  Makefile              Build: make, make emu, make renode, make flash-dfu

reverse_engineering/    Hardware analysis and protocol documentation
  ARCHITECTURE.md       System overview (start here)
  HARDWARE_PINOUT.md    Complete pin assignments
  FPGA_PROTOCOL_COMPLETE.md   Full FPGA command/data specification
  analysis_v120/        Detailed V1.2.0 firmware analysis

emulator/               Renode platform definition + SDL3 LCD viewer
docs/                   33 design and analysis documents (see docs/README.md)
modules/                JSON procedure files (automotive, HVAC, ham radio, education)
```

## Documentation

The `docs/` directory has comprehensive documentation covering hardware architecture, RE methodology, feature design, and testing procedures. Start with the [Documentation Index](docs/README.md).

Key documents:
- [Architecture Overview](reverse_engineering/ARCHITECTURE.md) — How the hardware works
- [FPGA Protocol](reverse_engineering/FPGA_PROTOCOL_COMPLETE.md) — ADC sampling and command interface
- [Hardware Pinout](reverse_engineering/HARDWARE_PINOUT.md) — Every MCU pin mapped
- [Roadmap](docs/roadmap.md) — What's done, what's next

## Reverse Engineering

This firmware was built by reverse engineering the stock FNIRSI binary using [Ghidra](https://ghidra-sre.org/). The `reverse_engineering/` directory contains our analysis — 309 functions identified and named, complete FPGA protocol documentation, and full hardware mapping.

No FNIRSI source code is distributed in this repository. See [reverse_engineering/README.md](reverse_engineering/README.md) for details on methodology and legal basis.

## Contributing

Contributions are welcome! Areas where help is especially needed:

- **FPGA SPI3 data acquisition** — Getting live ADC samples flowing (protocol is documented, implementation in progress)
- **Protocol decoders** — Testing and improving UART/SPI/I2C/CAN decode
- **Calibration** — Implementing the ADC calibration pipeline
- **Testing on hardware** — If you own a 2C53T, we need testers
- **UI/UX** — Layout improvements for the 320x240 display

### Development Setup

1. Install `arm-none-eabi-gcc` (see Quick Start)
2. For emulation: install [Renode](https://renode.io/) and optionally build the [SDL3 LCD viewer](emulator/) (`brew install sdl3 && cd emulator && make`)
3. For RE work: install [Ghidra](https://ghidra-sre.org/) and open `ghidra_project/FNIRSI_2C53T`

### Submitting Changes

1. Fork the repo and create a feature branch
2. Test with `make` (must compile cleanly)
3. Test in Renode if possible (`make renode`)
4. Open a PR with a description of what changed and why

## Related Projects

This project builds on the shoulders of prior FNIRSI reverse engineering work:

- [pecostm32/FNIRSI-1013D-1014D-Hack](https://github.com/pecostm32/FNIRSI-1013D-1014D-Hack) — Schematics, datasheets, and FPGA documentation for the 1013D/1014D
- [pecostm32/FNIRSI_1013D_Firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware) — Replacement firmware for the 1013D
- [Atlan4/Fnirsi1013D](https://github.com/Atlan4/Fnirsi1013D) — Most active FNIRSI firmware fork (471 commits)
- [Gissio/radpro](https://github.com/Gissio/radpro) — Custom firmware for FNIRSI Geiger counters

## Support This Project

If this firmware is useful to you, consider supporting development:

- [GitHub Sponsors](https://github.com/sponsors/DavidClawson)

## License

[GNU General Public License v3.0](LICENSE)
