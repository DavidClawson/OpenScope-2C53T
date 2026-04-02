# Reverse Engineering Guide

## Tools Installed

- **Ghidra 12.0.4** — `ghidraRun` to launch GUI, `analyzeHeadless` for CLI
- **binwalk** — `binwalk <firmware.bin>` for signature scanning, `binwalk -E` for entropy

## Ghidra Project

Pre-analyzed project at: `ghidra_project/FNIRSI_2C53T`

**To open:** `ghidraRun` → File → Open Project → browse to `ghidra_project/`

**Settings used:**
- Processor: ARM:LE:32:Cortex
- Compiler: default
- Base address: 0x08000000
- Loader: Raw Binary

## Headless Scripts

Located in `ghidra_scripts/`:

**DumpStrings.java** — Extracts all defined strings with addresses
```bash
analyzeHeadless /path/to/project FNIRSI_2C53T -process "APP_2C53T_V1.2.0_251015.bin" \
  -noanalysis -scriptPath ghidra_scripts -postScript DumpStrings.java
```

**DecompileAll.java** — Decompiles all functions to C pseudocode
```bash
analyzeHeadless /path/to/project FNIRSI_2C53T -process "APP_2C53T_V1.2.0_251015.bin" \
  -noanalysis -scriptPath ghidra_scripts -postScript DecompileAll.java /output/path.c
```

**Note:** If the decompiler fails with "os/mac_x86_64/decompile does not exist", create a symlink:
```bash
ln -s mac_arm_64 /opt/homebrew/Cellar/ghidra/*/libexec/Ghidra/Features/Decompiler/os/mac_x86_64
```

## RE Methodology

### How pecostm32 Did It (1013D)

Timeline: ~6 months to working firmware, ~2 years to mature.

1. **Ghidra disassembly** — Load binary, auto-analyze, start naming functions
2. **Custom emulator** — ARM CPU emulator to trace code without hardware
3. **FPGA discovery** — Trace FPGA commands by watching what firmware sends
4. **Font extraction** — Pull bitmap fonts from binary data
5. **Display library** — Recreate the drawing functions
6. **UI framework** — Menus, touch handling
7. **Signal capture** — Hook FPGA readout into display
8. **Feature completion** — SD card, USB, measurements, waveform save/load

### Recommended Approach for 2C53T

#### Phase 1: Anchored RE (start here)

1. Open Ghidra GUI, navigate to known string addresses
2. Right-click string → References → Show References To Address
3. This takes you to the function that uses that string
4. Name the function based on what the string tells you
5. Explore callers and callees, naming as you go

**Best starting points:**

| Address | String | What you'll find |
|---|---|---|
| 080b58ae | "Auto shutdown soon" | Shutdown timer logic, timeout constants |
| 080b5dd0 | "Sound and light" | Buzzer control, display brightness settings |
| 080b5ed6 | "Continuity" | Multimeter continuity mode, buzzer trigger |
| 080b535e | "USB Sharing" | USB initialization, mass storage setup |
| 080b5ce8 | "Oscilloscope Settings" | Main settings menu structure |
| 080b5dab | "Factory Reset" | Configuration save/load, flash storage |

#### Phase 2: Peripheral Mapping

1. Identify the FPGA communication interface (USART2 or GPIO)
2. Trace the display initialization (EXMC/FSMC setup)
3. Map the SPI flash communication (external filesystem)
4. Understand the USB setup (mass storage class)

#### Phase 3: Minimal Firmware

1. Set up GD32F307 toolchain (`arm-none-eabi-gcc`)
2. Write a minimal binary that initializes the LCD and draws text
3. Flash via the existing update mechanism (hold MENU + power)
4. Build up from there: FPGA communication, waveform display, UI

### Tips

- **V1.0.3 is simplest to RE** — smallest binary (452KB), same hardware
- **The vector table is your friend** — active IRQ handlers tell you which peripherals are used
- **Global variables cluster around 0x20008350-0x20008360** — these are the core state variables
- **FUN_08032f6c is the text drawing function** — trace its callers to understand UI flow
- **FUN_08019e98 is the main loop** — 2,530 lines, this is where everything happens
- **Don't overwrite calibration data** — the 1013D stores calibration at 0x001FD000; the 2C53T likely has a similar protected region
- **The firmware update is reversible** — you can always flash back to stock

## Firmware Update Process

1. Download firmware `.bin` file
2. Connect device to PC via USB-C
3. Hold MENU button, then press Power to enter firmware upgrade mode
4. Device appears as USB flash drive
5. Copy `.bin` file to the drive
6. Device auto-updates and reboots
