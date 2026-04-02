# Reference Projects and Communities

## Direct References (FNIRSI-specific)

### pecostm32's FNIRSI 1013D/1014D Hack
- **RE repo:** https://github.com/pecostm32/FNIRSI-1013D-1014D-Hack
  - Ghidra analysis, FPGA command documentation, C decompilation
  - Custom ARM emulator for testing without hardware
  - Complete IC datasheets (24 PDFs)
  - Schematics for 1013D and 1014D
- **1013D firmware:** https://github.com/pecostm32/FNIRSI_1013D_Firmware
  - Complete open-source replacement firmware in C
  - FatFS filesystem, display library, FPGA interface, touch panel
- **1014D firmware:** https://github.com/pecostm32/FNIRSI_1014D_Firmware
  - Same approach for the 1014D variant
- **Atlan's fork:** https://github.com/Atlan4/Fnirsi1013D
  - Community improvements to pecostm32's work

**Cloned locally at:**
- `FNIRSI-1013D-1014D-Hack/`
- `FNIRSI_1013D_Firmware/`
- `FNIRSI_1014D_Firmware/`

### Key Files in Reference Repos

| File | What it contains |
|---|---|
| `FNIRSI-1013D-1014D-Hack/Software reverse engineering/C analysis/FPGA analysis/FPGA explained.txt` | Complete FPGA command reference |
| `FNIRSI-1013D-1014D-Hack/Test code/Scope_emulator/` | Custom ARM emulator source code |
| `FNIRSI_1014D_Firmware/fnirsi_1014d_scope/fnirsi_1014d_scope.h` | Color definitions (ARGB format) |
| `FNIRSI_1013D_Firmware/fnirsi_1013d_scope/scope_functions.c` | Main scope logic (231KB) |

### Color System (from 1013D/1014D — likely similar)

Colors are 32-bit ARGB, converted to 16-bit RGB565 for display:
```c
CHANNEL1_COLOR      = 0x00FFFF00  // Yellow
CHANNEL2_COLOR      = 0x0000FFFF  // Cyan
TRIGGER_COLOR       = 0x0000FF00  // Green
ITEM_ACTIVE_COLOR   = 0x00EF9311  // Orange
```

## Open-Source Oscilloscope Software

### sigrok / PulseView
- **Website:** https://sigrok.org
- **PulseView GUI:** https://sigrok.org/wiki/PulseView
- Universal signal analysis framework
- 100+ protocol decoders (CAN, I2C, SPI, UART, JTAG, etc.)
- Cross-platform (macOS, Linux, Windows)
- Would be the target for USB streaming mode

### OpenHantek6022
- **GitHub:** https://github.com/OpenHantek/OpenHantek6022
- Open-source firmware + software for Hantek 6022BE USB oscilloscope
- Closest model to what a 2C53T streaming mode would look like
- The Hantek is a "dumb ADC on USB" — no screen, all processing on PC

### ThunderScope
- **Crowd Supply:** https://www.crowdsupply.com/eevengers/thunderscope
- Open-source PCIe/Thunderbolt oscilloscope, 1 GS/s streaming
- Uses ngscopeclient as frontend

### Haasoscope Pro
- Fully open-source 2GHz USB oscilloscope
- Python/Qt analysis software
- https://www.cnx-software.com/2025/02/27/haasoscope-pro-open-source-real-time-sampling-usb-oscilloscope-with-2ghz-bandwidth/

## CAN Bus / Automotive Tools

### CANable
- **Website:** https://canable.io
- Open-source USB to CAN adapter (~$25)
- STM32-based with open firmware
- Good reference for CAN transceiver circuit design

### can2 Logic Analyzer Decoder
- **Website:** https://kentindell.github.io/can2
- Open-source CAN protocol decoder for logic analyzers
- Detects protocol attacks
- Exports to pcapng for Wireshark analysis

### Awesome CAN Bus
- **GitHub:** https://github.com/iDoka/awesome-canbus
- Curated list of CAN bus tools, hardware, and resources

## Communities

### EEVblog Forum
- **2C53T thread:** https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/
- **1013D thread:** https://www.eevblog.com/forum/testgear/fnirsi-1013d-100mhz-tablet-oscilloscope/
- Main hub for FNIRSI reverse engineering discussion

### Reddit
- r/oscilloscopes
- r/ReverseEngineering
- r/embedded

### FNIRSI Official
- **Firmware downloads:** https://www.fnirsi.com/pages/manuals-firmwares
- **Product page:** https://www.fnirsi.com/products/2c53t

## GD32F307 Resources

- **GD32F307 Datasheet:** Search "GD32F307xx Datasheet" (GigaDevice official)
- **GD32F30x User Manual:** Comprehensive peripheral register documentation
- **GD32F30x Firmware Library:** GigaDevice provides an official HAL/LL library
- The GD32F307 is largely STM32F103 compatible, so STM32 HAL code often works with minor modifications
