# Hardware Identification

## MCU: GD32F307 (High Confidence)

Identified from firmware analysis without opening the device. The chip markings were reportedly sanded off.

**Evidence:**
1. F1-style GPIO configuration (CRL/CRH 4-bit nibble registers at 0x40010C00)
2. Interrupt vector table matches GD32F30x / STM32F103 layout exactly
3. Stack pointer at 0x20036F90 → 220KB RAM used → 256KB chip (GD32F307 has 256KB)
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
| MCU | GD32F307 (high confidence) | Identified from firmware analysis |
| FPGA | Unknown model | Markings sanded off. Communicates via USART2 or GPIO |
| ADC | Unknown model | Likely connected to FPGA, 250MS/s per specs |
| LCD | Unknown controller | 2.8" display via EXMC/FSMC parallel bus. Likely ILI9341 or ST7789 |
| Touch | Unknown controller | Via I2C. Likely GT911/GT915 (same family as 1013D/1014D) |
| SPI Flash | Unknown model | Likely W25Q series. Holds system files filesystem |
| DAC | Built-in GD32F307 DAC | 2-channel 12-bit, likely drives signal generator |

## What Physical Teardown Would Confirm

- Exact MCU part number suffix (VGT6 vs VET6 etc.)
- FPGA manufacturer and model (Lattice? Gowin? custom?)
- ADC chip model and configuration
- LCD controller IC
- SPI flash chip model and capacity
- PCB layout and pin routing
