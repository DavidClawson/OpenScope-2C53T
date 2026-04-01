# Hardware Identification

## MCU: Artery AT32 (Confirmed via DFU bootloader)

**Update (2026-03-30):** Physical teardown and ROM bootloader access confirmed the MCU is an **Artery AT32** (likely AT32F403A or AT32F407), NOT a GD32F307. The chip markings are fully sanded off but the built-in bootloader identifies as "AT32 Bootloader DFU" by "Artery Technology" (VID:PID 2e3c:df11). SWD reports IDCODE 0x2ba01477 (Cortex-M4) and device ID 0x70050344.

The original firmware analysis below is still valid — AT32 is register-compatible with GD32/STM32 at the GPIO/EXMC level, which is why the firmware analysis correctly identified the peripheral layout.

**Original identification (from firmware analysis, pre-teardown):**
1. F1-style GPIO configuration (CRL/CRH 4-bit nibble registers at 0x40010C00)
2. Interrupt vector table matches GD32F30x / STM32F103 layout exactly
3. Stack pointer at 0x20036F90 → 220KB RAM used → 256KB chip
4. EXMC/FSMC at 0xA0000000 (present on GD32F307)
5. USB interrupt at vector position 36 (USB_LP_CAN_RX0)
6. All peripheral register addresses match GD32F307 memory map

**GD32F307 Specifications:**
- ARM Cortex-M4 @ 120MHz
- 256KB SRAM
- 1MB Flash (firmware uses 733KB = 71%)
- EXMC/FSMC for parallel LCD interface
- USB OTG Full-Speed
- 3x SPI, 2x I2C, 5x USART
- **2x CAN controllers** (currently unused in firmware!)
- 2-channel 12-bit DAC (likely used for signal generator)
- 12-bit ADC
- LQFP100 or LQFP144 package

## Memory Layout

```
FLASH (1MB):
┌──────────────────────────┐ 0x08000000
│ Vector table (320 bytes) │
├──────────────────────────┤ 0x08000140
│ Code region (~252 KB)    │
│ (292+ functions)         │
├──────────────────────────┤ ~0x0803F1C8
│ Data section (~481 KB)   │
│ Fonts, strings, images,  │
│ lookup tables            │
├──────────────────────────┤ 0x080B7680 (end of update binary)
│ Additional data          │
│ (referenced by firmware  │
│  but not in update file) │
├──────────────────────────┤
│ Calibration / settings   │
├──────────────────────────┤
│ Free space (~267 KB)     │
└──────────────────────────┘ 0x08100000

RAM (256KB):
┌──────────────────────────┐ 0x20000000
│ Global variables         │
│ (heavily used:           │
│  DAT_20008350-20008360)  │
├──────────────────────────┤
│ Heap / buffers           │
├──────────────────────────┤
│ Stack (grows down)       │
└──────────────────────────┘ 0x20036F90 (initial SP)
```

## Storage

Two FatFS filesystem volumes:

**Drive 2:/ — Internal flash filesystem (user data)**
- `/LOGO/LOGO2C53T.jpg` — boot logo
- `/Screenshot file/*.bmp` — user screenshots
- `/Screenshot simple file/*.bin` — waveform data

**Drive 3:/ — External SPI flash (system assets)**
- `/System file/*.jpg` — 80+ UI asset images (buttons, icons, mode screens)
- `/System file/9999.bin` — possibly configuration data

The filesystem uses FatFS (embedded FAT32 library). The device exposes these via USB mass storage ("USB Sharing" mode).

## Peripheral Identification

| Peripheral | Status | Notes |
|---|---|---|
| MCU | **Artery AT32F403A** (confirmed via DFU) | Markings sanded off. Bootloader identifies as "AT32 Bootloader DFU" (VID:PID 2e3c:df11). Register-compatible with GD32/STM32F1. |
| FPGA | **Gowin GW1N-UV2** (community-identified) | Markings sanded off but identified by EEVblog user jbtronics. Gowin has public docs and open-source tooling (Yosys/nextpnr). **Command interface unknown** — not on any USART, SPI2, USB, or EXMC (see fpga_protocol.md). |
| ADC | Unknown model | Likely connected to FPGA, 250MS/s per specs. Possibly AD9288 or clone (10-bit based on `& 0x3ff` mask in decompiled code) |
| LCD | **ST7789V** (confirmed via hardware probe) | 2.8" 320x240 display via EXMC/FSMC parallel bus. Read ID returns 0x85. |
| Touch | Unknown / possibly none | No I2C references found in decompiled firmware. Touch may not exist on this model. |
| SPI Flash | **Winbond W25Q128JV** (confirmed via JEDEC ID `EF 40 18`) | 128Mbit (16MB) on SPI2 (PB12 CS, PB13-15 CLK/MISO/MOSI). Contains FAT filesystem with UI assets. |
| DAC | Built-in AT32F403A DAC | 2-channel 12-bit, drives signal generator output |

## Physical Teardown (2026-03-30)

PCB photographed. All major IC markings are sanded/lasered off as expected.

**SWD Debug Header** — 4 through-hole pads near USB-C port (top-to-bottom):
- SWCLK
- GND
- SWDIO
- 3V3

**FPGA Programming Header** — 7 through-hole pads:
- M3, M2, M1, M0 (mode select)
- GND
- VDD
- VPP (programming voltage)

**Range-switching relay:** HFD4/3-LS (signal relay near multimeter banana jacks)

**Analog front-end:** Shielded under metal can (left side of PCB), likely contains ADC

## What Remains Unconfirmed

- Exact MCU part number suffix (VGT6 vs VET6 etc.) — markings sanded
- ~~FPGA manufacturer and model~~ → **Gowin GW1N-UV2** (identified by jbtronics on EEVblog, March 2025)
- ADC chip model and configuration (likely AD9288 or pin-compatible clone) — under metal shield
- LCD controller IC
- ~~SPI flash chip model and capacity~~ → **Winbond W25Q128JVSQ** (confirmed from firmware analysis)

## FPGA Notes (Gowin GW1N-UV2)

The Gowin GW1N-UV2 is a small non-volatile FPGA from Gowin Semiconductor (Guangzhou, China):

- **Architecture:** GW1N family, low-power variant
- **LUTs:** ~2,000-4,600 (depending on exact sub-model)
- **Built-in flash:** Non-volatile, no external configuration memory needed
- **Package:** Likely QFN or LQFP
- **Documentation:** Public datasheets available from Gowin
- **Open-source tooling:** Supported by [Yosys](https://github.com/YosysHQ/yosys) + [nextpnr-gowin](https://github.com/YosysHQ/nextpnr) + [Apicula](https://github.com/YosysHQ/apicula) (Gowin bitstream documentation project)
- **Implications:** Unlike Anlogic or fully-custom FPGAs, the Gowin ecosystem means it may eventually be possible to reverse-engineer *and replace* the FPGA bitstream, not just talk to it

The FPGA communicates with the GD32F307 via USART2 using a string-based command protocol. It handles high-speed ADC sampling (250 MS/s) and likely manages trigger detection, timebase control, and sample buffer management.

**Source:** EEVblog user jbtronics, [2C53T thread](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/)

## Debug & Analysis Tools

| Tool | Purpose | Install |
|------|---------|---------|
| SWD (OpenOCD/pyOCD) | Live debug, flash, breakpoints | Via SWD header near USB-C |
| `dfu-util` | Flash firmware via USB DFU mode | `brew install dfu-util` |
| `sigrok-cli` | Logic analyzer capture & protocol decode | `brew install sigrok-cli` |

**Logic Analyzer:** HiLetgo 24MHz 8CH USB (Saleae Logic clone). Uses `fx2lafw` driver — firmware auto-loaded by sigrok. 8 channels at up to 24MHz sample rate, sufficient for USART2 (likely ≤1Mbaud) and SPI2 clock sniffing.

See [fpga_protocol.md](fpga_protocol.md#logic-analyzer-setup) for wiring plan and capture commands.

## Related Device Hardware Comparison

For context on shared components across the budget scope ecosystem, see [docs/landscape.md](landscape.md).

| Component | 2C53T | FNIRSI 5012H | FNIRSI 1013D | OWON HDS272S | Hantek 2D72 |
|-----------|-------|-------------|-------------|-------------|-------------|
| MCU | GD32F307 (CM4) | GD32F407 (CM4) | Allwinner F1C100s (ARM9) | GD32F303 (CM4) | STM32F103 (CM3) |
| FPGA | Gowin GW1N-UV2 | Unknown | Anlogic EF2L45 | Anlogic EG4X20 | Lattice LCMXO2 |
| ADC | Unknown (250MS/s) | AD9288-class | 2x AD9288 | Unknown | AD9288BSTZ |
| Flash | W25Q128 (16MB) | W25Q64 (8MB) | W25Q16/32 | Unknown | Unknown |
| Touch | GT911/915 (I2C) | N/A | GT911/915 (I2C) | Unknown | N/A |
