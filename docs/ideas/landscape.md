# Budget Handheld Oscilloscope Landscape

*Research compiled March 2026*

## FNIRSI Product Family

FNIRSI (Shenzhen FNIRSI Technology Co., Ltd.) makes a range of oscilloscopes spanning entry-level kits to dual-channel professional handhelds. The devices fall into distinct hardware tiers.

### GD32-Based Handhelds (closest to 2C53T)

| Model | MCU | Bandwidth | Sample Rate | Display | Notes |
|-------|-----|-----------|-------------|---------|-------|
| **2C53T** | GD32F307VET6 (Cortex-M4, 120MHz) | 50 MHz | 250 MS/s | 2.8" 320x240 | Our target. 3-in-1 (scope/DMM/siggen). FPGA: Gowin GW1N-UV2 |
| 2C23T | Likely GD32 | 10 MHz | 50 MS/s | 2.8" 320x240 | Predecessor to 2C53T. 3-in-1. 1 MHz siggen vs 2C53T's 50 kHz |
| 2C53P | Unknown (likely GD32) | 50 MHz | 250 MS/s | 4.3" IPS touch | Tablet form factor of 2C53T. Probably ~90% identical internals |
| 5012H | GD32F407VET6 (Cortex-M4) | 100 MHz | 500 MS/s | 2.4" LCD | Single-channel scope only. MCU is factory read-locked. Has open-source RE by ataradov |
| DPOX180H | Unknown | 180 MHz | 500 MS/s | 2.8" IPS | Digital phosphor, 50k wfm/s, 20 MHz DDS siggen |

### Allwinner Tablet-Style (different architecture)

| Model | MCU/SoC | FPGA | Bandwidth | Display | Notes |
|-------|---------|------|-----------|---------|-------|
| 1013D | Allwinner F1C100s/F1C200s (ARM926, 600MHz) | Anlogic EF2L45LG144B | 100 MHz | 7" 800x480 touch | ADC: 2x AD9288. Battery-powered tablet |
| 1014D | Allwinner F1C200s | Anlogic EF2L45LG144B | 100 MHz | 7" TFT | 2-in-1 with siggen. AC-powered benchtop |

### Entry-Level (STM32, no FPGA)

| Model | MCU | Bandwidth | Notes |
|-------|-----|-----------|-------|
| DSO-150 | STM32F103C8 | 200 kHz | JYE Tech clone/rebrand |
| DSO-152 | Likely STM32 | 200 kHz | Updated DSO-150 |
| DSO-TC3 | Unknown | 500 kHz | 3-in-1: scope + siggen + transistor tester |
| FNIRSI-138 Pro | Unknown | 200 kHz | Pen-style form factor |
| 1C15 | Unknown | 110 MHz | Pen-style probe scope |

### Key Architectural Note

The 1013D/1014D (Allwinner ARM9) are a completely different platform from the 2C53T (GD32 Cortex-M4). No binary compatibility. Different FPGA communication: 1013D uses GPIO bit-banging on Port E, 2C53T uses USART2 serial with string-based commands. The 2C53P is the most likely "free" second target given probable hardware similarity.

---

## Competing Brands

### Hantek

| Model | MCU | FPGA | Bandwidth | Notes |
|-------|-----|------|-----------|-------|
| 2D72 | STM32F103VET6 (Cortex-M3) | Lattice LCMXO2-1200HC | 70 MHz | 3-in-1 handheld. Popular competitor to 2C53T. Different MCU and FPGA vendors |
| DSO5102P | Unknown | Unknown | 100 MHz | Budget bench scope. Bandwidth unlock hack exists |
| 6022BE/BL | Cypress FX2LP | N/A | 20 MHz | USB scope. Best open-source support (OpenHantek6022) |

### OWON

| Model | MCU | FPGA | Bandwidth | Notes |
|-------|-----|------|-----------|-------|
| HDS272S | GD32F303 (Cortex-M4) | Anlogic EG4X20BG256 | 70 MHz | 3-in-1 handheld. **Same MCU vendor as 2C53T** |
| HDS2102S | GD32F303 | Anlogic | 100 MHz | 2-channel, well-regarded. ~$250 |
| HDS2202S | GD32F303 | Anlogic | 200 MHz | Top of HDS line |

The OWON HDS series is interesting — same GD32 MCU family and Anlogic FPGA as FNIRSI. Suggests shared supply chain or reference designs.

### Miniware

| Model | MCU | Bandwidth | Notes |
|-------|-----|-----------|-------|
| DS212 | STM32F103VET6 | 1 MHz | Pocket scope. Open-source, very hackable |
| DS213 | STM32F103VET6 | 15 MHz | Supports 4 firmware slots |

### Others

- **HANMATEK HO52**: Popular with automotive users. Budget handheld.
- **Zeeweii DSO3D12**: W806 MCU (C-SKY CK804 core). Has active RE project (ZeeTweak).
- **JYE Tech DSO-138/150**: Educational kit scopes on STM32F103. The original hackable budget scope.

---

## Shared Hardware Patterns

Budget Chinese test equipment converges on a small set of components:

| Component | Used By | Notes |
|-----------|---------|-------|
| **GD32 MCUs** (Cortex-M4) | FNIRSI 2C53T, 5012H; OWON HDS series | GigaDevice. Pin-compatible STM32 alternatives. Dominant in this segment |
| **Anlogic FPGAs** | FNIRSI 1013D/1014D; OWON HDS series | Shanghai Anlu. Go-to Chinese FPGA for budget test equipment |
| **Gowin FPGAs** | FNIRSI 2C53T (GW1N-UV2) | Another Chinese FPGA vendor. Better documented than Anlogic |
| **Lattice FPGAs** | Hantek 2D72 (LCMXO2) | Western vendor, used by Hantek |
| **AD9288 ADC** (or clones) | FNIRSI 1013D/1014D, 5012H; Hantek 2D72 | Dual 8-bit 100 MSPS. The standard choice |
| **GT911/GT915 touch** | FNIRSI 2C53T, 1013D, 1014D | Goodix. I2C. Same family across all FNIRSI models |
| **Winbond W25Q SPI flash** | All FNIRSI models | Size varies: 2MB to 16MB |
| **ST7789V / ILI9341 LCD** | Various | Parallel RGB565, memory-mapped |
| **STM32F103** | Hantek 2D72; Miniware DS212/213; JYE Tech kits | Older/simpler designs |

No confirmed cases of truly identical PCBs across brands, but component-level convergence is striking. FNIRSI also offers ODM/OEM services, so some unbranded AliExpress scopes may be FNIRSI hardware inside.

---

## Open-Source Firmware Projects

### Active (as of March 2026)

| Project | Target | Last Activity | Stars | Notes |
|---------|--------|---------------|-------|-------|
| [Atlan4/Fnirsi1013D](https://github.com/Atlan4/Fnirsi1013D) | FNIRSI 1013D | Feb 2026 | 74 | Solo developer (471 commits). Most active FNIRSI firmware project |
| [OpenHantek6022](https://github.com/OpenHantek/OpenHantek6022) | Hantek 6022BE/BL | Oct 2025 | 1054 | Maintenance mode. PC software + firmware for USB scopes |
| [mean00/lnDSO150](https://github.com/mean00/lnDSO150) | DSO150/DSO Shell | Mar 2026 | 64 | Active, solo developer |
| [taligentx/ZeeTweak](https://github.com/taligentx/ZeeTweak) | Zeeweii DSO3D12 | Dec 2025 | 25 | RE + binary patching, not a full rewrite. Uses Ghidra for C-SKY arch |
| [EEVengers/ThunderScope](https://github.com/EEVengers/ThunderScope) | Custom hardware | Mar 2026 | 1237 | Open-source from-scratch scope (USB4 streaming). Not replacement firmware |

### Dormant / Dead

| Project | Target | Last Commit | Stars | Notes |
|---------|--------|-------------|-------|-------|
| [pecostm32/FNIRSI_1013D_Firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware) | FNIRSI 1013D | Feb 2025 | 153 | Foundational RE work. Essentially dormant since Jan 2024 |
| [pecostm32/FNIRSI_1014D_Firmware](https://github.com/pecostm32/FNIRSI_1014D_Firmware) | FNIRSI 1014D | Jun 2024 | 28 | Dormant. Explicitly incompatible with 1013D |
| [ataradov/open-5012h](https://github.com/ataradov/open-5012h) | FNIRSI 5012H | Nov 2021 | 142 | Dead. Alpha stage, required MCU swap due to factory lock |
| [ardyesp/DLO-138](https://github.com/ardyesp/DLO-138) | JYE Tech DSO-138 | Sep 2022 | 395 | Dead |
| [michar71/Open-DSO-150](https://github.com/michar71/Open-DSO-150) | JYE Tech DSO-150 | Jul 2020 | 262 | Dead |
| [circuit-specialists/Hantek-2D72](https://github.com/circuit-specialists/Hantek-2D72) | Hantek 2D72 | 2019 | 39 | Dead. Firmware tools and unbrick utilities |

### Community RE Efforts (not full firmware)

| Source | Target | Activity | Notes |
|--------|--------|----------|-------|
| [EEVblog: 2C53T thread](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) | FNIRSI 2C53T | Active discussion | jbtronics identified Gowin GW1N-UV2 FPGA. Dr. Blast patched V1.2.0 to expose hidden filesystem |
| [fuho/owon_hds200](https://github.com/fuho/owon_hds200) | OWON HDS200 series | Oct 2024 | Info archive with firmware versions and hardware docs, not firmware |
| [pecostm32/FNIRSI-1013D-1014D-Hack](https://github.com/pecostm32/FNIRSI-1013D-1014D-Hack) | FNIRSI 1013D/1014D | Archived | Schematics, IC datasheets, FPGA protocol docs. Gold-standard RE reference |
| [Donwulff/FNIRSI_1014D_Firmware](https://github.com/Donwulff/FNIRSI_1014D_Firmware) | FNIRSI 1014D | Aug 2025 | Fork of pecostm32 with experimental changes |

### Fully Open-Source Scope Hardware

| Project | Notes |
|---------|-------|
| [ThunderScope](https://github.com/EEVengers/ThunderScope) | 4-channel 100 MHz USB4 scope. Open hardware + gateware. Most active open-source scope project overall |
| [HaasoscopePro](https://github.com/drandyhaas/HaasoscopePro) | 2 GHz 3.2 GS/s 12-bit USB scope |
| [ScopeFun](https://www.scopefun.com/) | Xilinx Spartan-6 FPGA + Cypress FX2LP |

---

## Why Projects Die

Every dead project above follows the same pattern:

1. **Single maintainer** does heroic RE work
2. Writes functional firmware tightly coupled to one device
3. Burns out or moves on after 1-3 years
4. Project goes dormant; community has no way to continue

pecostm32's 1013D firmware is the clearest example: 153 stars, real users, active forks (Atlan4) — but the original author stopped, and the architecture doesn't support other devices.

The projects that *lasted* (OpenHantek6022 at 1054 stars, ThunderScope at 1237 stars) became platforms with multiple contributors and broader scope than one device.

---

## Opportunity Assessment

**No one is building a multi-device open-source firmware platform for budget handheld oscilloscopes.**

The gap:
- Stock firmware on all brands is universally criticized (bad FFT, no protocol decoding, limited features)
- Hardware is converging (GD32 + FPGA + AD9288 is the standard architecture)
- Users are technical enough to flash firmware (hobbyists, EE students, makers, automotive)
- The 2C53T is currently the hot budget scope — new, popular, $60-80 price point
- No existing project targets it with a replacement firmware

Viable multi-device targets (same GD32 family):
1. **FNIRSI 2C53T** (GD32F307) — primary target
2. **FNIRSI 2C53P** (likely same internals, different LCD) — easiest port
3. **FNIRSI 5012H** (GD32F407) — has existing RE work, single-channel
4. **OWON HDS200 series** (GD32F303) — cross-brand, same MCU vendor + Anlogic FPGA

Code that ports across all of these with zero changes:
- FFT engine (windowing, peak detection, averaging, max hold)
- Signal generator waveform math
- Auto-measurement algorithms (Vpp, Vrms, frequency, duty cycle)
- Protocol decoders (I2C, SPI, UART, CAN)
- Waterfall spectrogram rendering
- Display persistence effects
- Math channels, cursors, trigger logic (software side)
- File export, screenshot capture

Code that requires a per-device HAL (~500-1000 lines each):
- LCD init + pixel write
- FPGA communication protocol
- ADC sample acquisition
- DAC output for signal generator
- Button/input GPIO mapping
- Clock and power initialization

---

## Community Channels

- **EEVblog Forums**: Most active discussion of budget scope internals, firmware patching, hardware teardowns
- **ThunderScope Discord**: Most active open-source oscilloscope community (also covers ngscopeclient)
- **Hackaday**: Occasional coverage of scope hacking projects
- **No dedicated Discord/Matrix** exists for FNIRSI, OWON, or Hantek firmware hacking

---

## Key References

- [FNIRSI 2C53T EEVblog Thread](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/)
- [FNIRSI Official Product Page](https://www.fnirsi.com/collections/oscilloscope)
- [OWON HDS2202S Teardown (Kerry Wong)](http://www.kerrywong.com/2023/03/30/owon-hds2202s-3-in-1-handheld-oscilloscope-dmm-awg-teardown/)
- [Hantek 2D72 Teardown (Kerry Wong)](http://www.kerrywong.com/2021/06/26/teardown-of-an-hantek-2d72-3-in-1-handheld-oscilloscope-dmm-awg/)
- [FNIRSI 1013D Teardown (CNX Software)](https://www.cnx-software.com/2022/11/16/fnirsi-1013d-teardown-and-mini-review-a-portable-oscilloscope-based-on-allwinner-cpu-anlogic-fgpa/)
- [open-5012h Hardware Documentation](https://github.com/ataradov/open-5012h/blob/master/doc/Hardware.md)
