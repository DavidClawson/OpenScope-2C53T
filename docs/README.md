# FNIRSI 2C53T Reverse Engineering Project

Open-source reverse engineering effort for the FNIRSI 2C53T handheld 3-in-1 oscilloscope/multimeter/signal generator.

## Goal

Understand the firmware well enough to:
- Modify existing behavior (auto-shutdown timing, colors, buzzer behavior)
- Add protocol decoding (CAN bus, I2C, UART) with on-screen overlay
- Enable USB streaming mode for use as a PC-connected instrument
- Build automotive diagnostic modules (compression test, injector analysis, CAN/OBD-II decode)

## Documentation

- [Hardware Identification](hardware.md) — MCU, peripherals, memory map
- [Firmware Analysis](firmware_analysis.md) — Binary structure, strings, version comparison
- [Peripheral Map](peripheral_map.md) — Active interrupts, GPIO usage, communication buses
- [RTOS Analysis](rtos_analysis.md) — FreeRTOS identification, task architecture, kernel functions
- [Function Map](function_map.md) — Named functions and variables from RE analysis
- [Reverse Engineering Guide](re_guide.md) — Tools, methodology, how to get started
- [Project Ideas](project_ideas.md) — Feature ideas and feasibility analysis
- [Reference Projects](reference_projects.md) — Related open-source projects and communities

## Quick Start

1. Install tools: `brew install binwalk ghidra`
2. Open the Ghidra project: `ghidraRun` → Open `/ghidra_project/FNIRSI_2C53T`
3. Navigate to key addresses (see [Firmware Analysis](firmware_analysis.md))
4. Read the [RE Guide](re_guide.md) for methodology

## Repository Contents

```
osc/
├── docs/                          # This documentation
├── 2C53T Firmware V1.2.0/         # Latest firmware (V1.2.0)
├── APP_2C53T_V1.0.3_240814.bin    # Earliest firmware (smallest, best for RE)
├── APP_2C53T_V1.0.7_241205.bin    # Intermediate firmware
├── 2C53T_Firmware_V1.1.2/         # V1.1.2 (anti-aliased fonts added)
├── ghidra_project/                # Pre-analyzed Ghidra project (V1.2.0)
├── ghidra_scripts/                # Custom Ghidra scripts
├── decompiled_2C53T.c             # 34,618 lines of decompiled C (292 functions)
├── strings_with_addresses.txt     # 290 identified strings with flash addresses
├── FNIRSI-1013D-1014D-Hack/       # pecostm32's RE work on sibling models
├── FNIRSI_1013D_Firmware/         # Open-source replacement firmware (1013D)
└── FNIRSI_1014D_Firmware/         # Open-source replacement firmware (1014D)
```
