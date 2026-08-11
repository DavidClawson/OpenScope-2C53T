# OpenScope 2C53T

**Open-source replacement firmware for the FNIRSI 2C53T handheld oscilloscope / multimeter / signal generator.**

<p align="center">
  <img src="scope.jpg" alt="FNIRSI 2C53T" width="300">
</p>

The FNIRSI 2C53T is a capable $75 handheld 3-in-1 instrument held back by buggy stock firmware. This project is a complete clean-room firmware rewrite built from reverse engineering the original binary.

> **Looking for contributors with test equipment.** The firmware runs on real hardware — UI, meter, bootloader, and button input all work. The next milestones (live oscilloscope waveforms, factory calibration) need hardware captures we can't do with a single bench unit. If you have a 2C53T and a logic analyzer, or experience with Gowin FPGAs, [see how you can help](#help-wanted).

## Current Status

**Custom firmware runs on real hardware.** The UI, button input, battery management, and USB bootloader all work. Active development is focused on getting live oscilloscope data from the FPGA.

### Working on hardware today
- 4 navigable UI modes: oscilloscope, multimeter, signal generator, settings
- 4 color themes (Dark Blue, Classic Green, High Contrast, Night Red)
- Variable-width bitmap fonts at 4 sizes (12/16/24/48px)
- FreeRTOS with display + input tasks
- 15/15 button matrix scanning at 500Hz
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
- **FPGA SPI3 data acquisition** — **major breakthrough (Jun 2026).** A community logic-analyzer capture of a stock boot ([issue #18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18), thanks to @maksidze) revealed that our FPGA configuration bitstream had been extracted from the wrong flash offset — we'd been uploading garbage. With the corrected 115,638-byte bitstream, the FPGA now accepts the config and the PC0 data-ready line arms for the first time. Remaining work: implement the per-channel `0x04`/`0x05` sample-read protocol the capture revealed and wire it to the waveform renderer. This is the critical path to a working oscilloscope.

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

**Toolchain:**

```bash
# macOS (Homebrew)
brew install --cask gcc-arm-embedded    # ARM toolchain
brew install dfu-util                    # USB DFU flasher

# Linux (Debian/Ubuntu)
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
sudo apt install dfu-util make

# Windows
# Install ARM GNU Toolchain from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# Install dfu-util from https://dfu-util.sourceforge.net/
# Build with Make (via MSYS2, WSL, or similar)
```

**Dependencies (all platforms):**

The firmware depends on two libraries that aren't bundled in the repo. Clone them into the `firmware/` directory:

```bash
cd firmware
git clone https://github.com/ArteryTek/AT32F403A_407_Firmware_Library.git at32f403a_lib
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS
```

**Build once before flashing:**

```bash
cd firmware && make
```

This populates `firmware/build/` with `firmware.bin` (the application) and `option_bytes48.bin` (a 48-byte blob used by the one-time option-byte DFU write below).

> **`bootloader.bin` is built separately.** Plain `make` only builds the application. The USB HID bootloader is a separate target — run `make bootloader` to build it (output lands in `bootloader/build/bootloader.bin`). The first-time `make flash-all` step below builds it for you automatically, so you normally don't need to run `make bootloader` by hand.

### First-Time Hardware Setup

The first flash requires opening the case to enter the AT32's **ROM DFU mode** — this is the only mode that can write option bytes. After the initial flash installs the USB HID bootloader, all future updates go over USB-C with the case closed.

> **Two bootloaders — don't confuse them.** *ROM DFU* (entered via BOOT0 + pinhole reset, LCD dark, `2e3c:df11`) is required for the one-time EOPB0 setup. The *USB HID bootloader* (Settings → Firmware Update, "BOOTLOADER MODE" on the LCD) handles every update after that but cannot write option bytes.

**See the full walkthrough with photos: [DFU Mode Guide](docs/dfu_mode_guide.md)**

> **Windows users:** community member [@baraa1936](https://github.com/baraa1936) wrote a screenshot-by-screenshot walkthrough for flashing from Windows using the Artery ISP GUI tool (including the EOPB0 / 224KB SRAM option-byte step) — see [issue #20](https://github.com/DavidClawson/OpenScope-2C53T/issues/20).

The short version:

1. Open the case (6 Phillips screws on back)
2. Use a jumper wire to bridge 3.3V (from the SWD header near USB-C) to the BOOT0 pull-down resistor (MCU side, near the main chip)
3. While holding 3.3V on BOOT0, press the pinhole reset button, then release both
4. Verify ROM DFU: `dfu-util -l` should list `2e3c:df11` with alt interfaces 0 (Internal Flash) and 1 (Option Byte)
5. Set EOPB0 = 0xFE → 224KB SRAM mode (one-time):
   ```bash
   cd firmware
   dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D build/option_bytes48.bin
   ```
   Expect `Download done. / File downloaded successfully`. The `Invalid DFU suffix signature` and `Error sending dfu abort request` warnings are cosmetic.
6. Pinhole reset to stay in DFU, then flash the bootloader and application:
   ```bash
   make flash-all
   ```
   If the application write finishes but the device does not come up running the
   app, do not assume it booted. Remove the BOOT0 jumper, reset into the USB HID
   bootloader, and run `make flash` to install the application through the
   bootloader.
7. Remove the BOOT0 jumper, pinhole reset, close the case — you won't need to open it again once the application boots

### Normal Development Cycle (case closed)

Once the USB HID bootloader is installed, updates are simple:

1. On the device: **Settings > Firmware Update** (shows "BOOTLOADER MODE" screen)
2. On your computer:
   ```bash
   cd firmware && make flash
   ```
3. The device auto-reboots into the updated firmware

### Restoring Stock or Flashing via USB-C (macOS & Linux)

The device's **stock bootloader** also accepts firmware over USB-C — handy for restoring the original FNIRSI firmware or flashing without the HID bootloader. Hold **MENU + tap Power** to enter upgrade mode (the LCD shows "firmware upgrade"); the device mounts a FAT12 volume named `IAP`.

> **macOS users:** do **not** drag-drop the `.bin` in Finder — macOS's FAT driver corrupts the write (the volume uses 2048-byte sectors and Finder adds AppleDouble `._` junk the bootloader misreads as firmware). Use the bundled flasher, which writes the device correctly:

```bash
brew install mtools                  # one-time (Linux: sudo apt install mtools)
python3 scripts/iap_flash.py         # detect device → pick firmware → flash
```

It auto-detects the device and available images, verifies the stock firmware by SHA-256, and shows a progress bar. Subcommands: `status`, `list`, `flash <path>`, `doctor` (prerequisite check), `guide` (full walkthrough). A bad flash is never a brick — re-enter upgrade mode and reflash any image.

**Windows** users can skip the tool — drag-drop the `.bin` onto the `IAP` drive (the official FNIRSI method; Windows' FAT driver handles the volume cleanly).

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

## Help Wanted

The firmware is functionally complete for the UI layer, but the next milestones are blocked on hardware captures and experimentation that a single bench unit can't provide. **You don't need to write code to make a big contribution here.**

### 1. ~~Logic analyzer captures of the stock firmware boot sequence~~ ✅ DONE
**Solved June 2026** thanks to @maksidze ([issue #18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18)), who patched the stock firmware's SPI prescaler to /64 and captured a full stock boot on a Saleae. That capture revealed our FPGA bitstream was extracted from the wrong file offset (we'd treated the flash address as a file offset, ignoring the `0x08007000` link base — the real bitstream is at file offset `0x4AD19`). The corrected bitstream is byte-exact against the capture and the FPGA now accepts it. Full decode: [`reverse_engineering/captures/`](reverse_engineering/captures/). Still useful: captures from **other board revisions** to confirm the bitstream and sequence generalize.

### 2. Oscilloscope sample-read implementation
The capture revealed the post-config read protocol: per-channel 1026-byte reads with opcodes `0x04` (CH1) / `0x05` (CH2), gated on the PC0 data-ready line. We're implementing this now. If you have a 2C53T + logic analyzer and want to help characterize the runtime trigger/timebase command sequence (the part that rides on USART, which the SPI capture couldn't see), open an issue. See [FPGA Protocol](reverse_engineering/FPGA_PROTOCOL_COMPLETE.md) for the full command table.

### 3. Board variant documentation
We've confirmed one board revision (V1.4) and one user has reported a different layout with no version marking. If your 2C53T looks different from [our photos](docs/images/), photos of your PCB (top and bottom) are extremely valuable — especially near the FPGA, SPI flash, and analog frontend.

### 4. Everything else
- **Test on your hardware** — different units reveal things a single bench unit can't
- **Document what worked** — first-flash walkthroughs for Linux or Windows are always welcome
- **Contribute modules** (`modules/*.json`) for your domain (automotive, HVAC, ham radio, etc.)
- **Translate** — we have users in Korea and Russia already; localization help is welcome

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for the full guide. Bug reports and feature requests are always welcome via the [issue tracker](https://github.com/DavidClawson/OpenScope-2C53T/issues).

## Related Projects

- [pecostm32/FNIRSI-1013D-1014D-Hack](https://github.com/pecostm32/FNIRSI-1013D-1014D-Hack) — Schematics, datasheets, and FPGA docs for the 1013D/1014D
- [pecostm32/FNIRSI_1013D_Firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware) — Replacement firmware for the 1013D
- [Atlan4/Fnirsi1013D](https://github.com/Atlan4/Fnirsi1013D) — Most active FNIRSI firmware fork (471 commits)
- [Gissio/radpro](https://github.com/Gissio/radpro) — Custom firmware for FNIRSI Geiger counters

## License

[GNU General Public License v3.0](LICENSE)
