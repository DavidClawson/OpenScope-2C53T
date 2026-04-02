# OpenScope 2C53T

**Open-source replacement firmware for the FNIRSI 2C53T handheld oscilloscope / multimeter / signal generator.**

<p align="center">
  <img src="scope.jpg" alt="FNIRSI 2C53T" width="300">
</p>

The FNIRSI 2C53T is a capable $75 handheld 3-in-1 instrument held back by buggy stock firmware. This project is a complete clean-room firmware rewrite built from reverse engineering the original binary.

## Current Status

**Custom firmware runs on real hardware.** The UI, button input, battery management, and USB bootloader all work. Active development is focused on getting live oscilloscope data from the FPGA.

### Working on hardware today
- 4 navigable UI modes: oscilloscope, multimeter, signal generator, settings
- 4 color themes (Dark Blue, Classic Green, High Contrast, Night Red)
- Variable-width bitmap fonts at 4 sizes (12/16/24/48px)
- FreeRTOS with display + input tasks
- 14/15 button matrix scanning at 500Hz
- Battery monitor with percentage, USB charge detection, low-battery auto-off
- Soft power management (3-2-1 countdown shutdown)
- Watchdog and health monitoring
- USB HID bootloader for closed-case firmware updates
- FPGA USART communication (bidirectional, meter data flowing)

### Implemented, tested, awaiting real data
These algorithms are written and unit-tested, but currently run on demo waveforms. They'll come alive once FPGA ADC data flows through SPI3.

- FFT spectrum analyzer (4096-point, 5 window functions, averaging, max hold)
- Protocol decoders (UART, SPI, I2C, CAN, K-Line/KWP2000)
- Math channels (CH1+CH2, CH1-CH2, CH1*CH2, invert)
- Auto-measurements (frequency, Vpp, Vrms, duty cycle)
- Persistence display (5 decay modes)
- Bode plot engine (log sweep, gain/phase calculation)
- Signal generator (DDS, 4 waveforms)
- Component tester (resistor, capacitor, ESR, diode, continuity)
- XY mode, roll mode, trend plotting
- Mask/pass-fail testing
- Config save/load with checksum
- Screenshot capture (BMP)

### In progress
- **FPGA SPI3 data acquisition** — root cause identified (needs PB11 HIGH, full boot command sequence, queue-driven triggering). This is the critical path to a working oscilloscope.

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | Artery AT32F403A — ARM Cortex-M4F @ 240MHz, 1MB flash, 224KB SRAM |
| **Display** | ST7789V 320x240 RGB565 via 16-bit parallel bus (EXMC) |
| **FPGA** | Gowin GW1N-UV2 — handles 250MS/s ADC sampling |
| **ADC** | Dual-channel, 8-bit, 250MS/s via FPGA SPI3 |
| **Signal Gen** | 2-channel 12-bit DAC |
| **Flash** | Winbond W25Q128JVSQ (16MB) — UI assets and calibration |
| **Input** | 15 buttons (4x3 scanned matrix + 3 passive) |

> The MCU markings are sanded off. We identified it as AT32F403A through register probing — it's register-compatible with GD32/STM32F1 at the GPIO level.

## Getting Started

### Prerequisites

```bash
# macOS (Homebrew)
brew install --cask gcc-arm-embedded    # ARM toolchain
brew install dfu-util                    # USB DFU flasher
```

### First-Time Hardware Setup

The first flash requires opening the case to access the DFU boot pads. After that, all updates go over USB-C with the case closed.

**What you need:** Phillips screwdriver, a jumper wire or tweezers, the USB-C cable.

1. Remove the 4 Phillips screws on the back and open the case
2. Locate the 3V3 pad on the header below the USB-C port and the BOOT0 test point
3. Bridge BOOT0 to 3V3 while pressing the pinhole reset button, then release — the device enumerates as "AT32 Bootloader DFU"
4. Set EOPB0 for 224KB SRAM (one-time, the AT32 defaults to only 96KB):
   ```bash
   dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin
   ```
5. Flash the bootloader and application:
   ```bash
   cd firmware && make flash-all
   ```
6. Close the case — you won't need to open it again

### Normal Development Cycle (case closed)

Once the USB HID bootloader is installed, updates are simple:

1. On the device: **Settings > Firmware Update** (shows "BOOTLOADER MODE" screen)
2. On your computer:
   ```bash
   cd firmware && make flash
   ```
3. The device auto-reboots into the updated firmware

### Build

```bash
cd firmware
make              # Build for hardware (AT32 @ 240MHz)
make emu          # Build for emulator (skips hardware init)
```

### Emulator (no hardware required)

```bash
make renode              # Run in Renode with LCD display
make renode-interactive  # Run with keyboard input
```

Requires [Renode](https://renode.io/) at `/Applications/Renode.app`. An SDL3 native LCD viewer is also available (`brew install sdl3 && cd emulator && make`).

## Project Structure

```
firmware/               Custom replacement firmware (C + FreeRTOS + Make)
  src/main.c            Entry point, FreeRTOS tasks, mode switching
  src/drivers/          LCD, buttons, battery, watchdog, DFU boot
  src/ui/               Scope, meter, siggen, settings, themes
  src/dsp/              FFT, math channels, signal gen, Bode
  src/decode/           Protocol decoders (UART, SPI, I2C, CAN, K-Line)
  src/tasks/            Measurement engine, component tester, mask test
  bootloader/           USB HID IAP bootloader (16KB)

reverse_engineering/    Hardware analysis and protocol documentation
  ARCHITECTURE.md       System overview (start here for RE)
  HARDWARE_PINOUT.md    Complete MCU pin assignments
  FPGA_PROTOCOL_COMPLETE.md   Full FPGA command/data specification
  COVERAGE.md           309 functions mapped from stock firmware
  analysis_v120/        Detailed V1.2.0 analysis artifacts

emulator/               Renode platform + SDL3 LCD viewer
docs/                   Design docs, analysis, planning (see docs/README.md)
modules/                JSON procedure files (automotive, HVAC, ham radio)
scripts/                Font generation, flash tools, soak testing
```

## Documentation

Start with the [Documentation Index](docs/README.md). Key documents:

- [Architecture Overview](reverse_engineering/ARCHITECTURE.md) — How the hardware works
- [FPGA Protocol](reverse_engineering/FPGA_PROTOCOL_COMPLETE.md) — ADC sampling and command interface
- [Hardware Pinout](reverse_engineering/HARDWARE_PINOUT.md) — Every MCU pin mapped
- [Roadmap](docs/roadmap.md) — What's done, what's next, future plans

## Reverse Engineering

The stock firmware was reverse-engineered using [Ghidra](https://ghidra-sre.org/). We've identified and named 309 functions, mapped all ~40 FPGA commands, fully documented the ADC data format, and traced every hardware pin. About 98% of the stock firmware is now understood.

No FNIRSI source code is distributed in this repository. See [reverse_engineering/README.md](reverse_engineering/README.md) for methodology and legal basis.

## Contributing

This is a solo project in active development. The core architecture is still taking shape, so I'm not accepting code contributions right now.

**If you own a 2C53T and want to help:**
- Open an issue with hardware observations, bug reports, or feature requests
- Test firmware builds and report what works / what doesn't
- Share logic analyzer captures of FPGA communication

Code contributions may be accepted once the data acquisition pipeline stabilizes.

## Related Projects

- [pecostm32/FNIRSI-1013D-1014D-Hack](https://github.com/pecostm32/FNIRSI-1013D-1014D-Hack) — Schematics, datasheets, and FPGA docs for the 1013D/1014D
- [pecostm32/FNIRSI_1013D_Firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware) — Replacement firmware for the 1013D
- [Atlan4/Fnirsi1013D](https://github.com/Atlan4/Fnirsi1013D) — Most active FNIRSI firmware fork (471 commits)
- [Gissio/radpro](https://github.com/Gissio/radpro) — Custom firmware for FNIRSI Geiger counters

## License

[GNU General Public License v3.0](LICENSE)
