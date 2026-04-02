# Documentation Index

Open-source replacement firmware for the FNIRSI 2C53T handheld oscilloscope / multimeter / signal generator.

**Start here:** The project [README](../README.md) is at the repo root. For the full RE archive, see [`reverse_engineering/`](../reverse_engineering/).

---

## Top-Level

- [Roadmap](roadmap.md) — What works, what's in progress, what's planned
- [Button Manual](button_manual.md) — Physical button layout, navigation, emulator key bindings
- [Hardware Test Protocol](hardware_test_protocol.md) — First-flash and subsystem verification checklist

## Design

Architecture, technical specs, and system design documents.

- [Peripheral Map](design/peripheral_map.md) — Memory-mapped peripherals, GPIO assignments, interrupt vectors
- [FFT Design](design/fft_design.md) — Windowing, amplitude scaling, spectrum display architecture
- [FPGA Future Possibilities](design/fpga_future.md) — Gowin GW1N-UV2 resources and enhancement opportunities
- [ESP32 Coprocessor](design/esp32_coprocessor.md) — ESP32 UART bridge for WiFi/BLE connectivity (future mod)
- [Platform Architecture](design/platform_architecture.md) — HAL layer design, SDK vision
- [Module API](design/module_api.md) — Interface spec for self-contained modules
- [Code Organization](design/code_organization.md) — Proposed codebase restructuring plan
- [Resource Planning](design/resource_planning.md) — RAM/flash budget, module hot-loading strategy
- [Developer Experience](design/developer_experience.md) — Multi-tier accessibility (end users, makers, developers)
- [Logic Analyzer Add-On](design/logic_analyzer_addon.md) — FX2LP-based USB analyzer for 8-channel digital capture
- [Custom Test Platform](design/custom_test_platform.md) — Repurposing hardware as programmable test fixture

## Ideas

Feature brainstorms, market research, and community feedback. These capture the vision for what the device could become.

- [Feature Catalog and Industry Modules](ideas/feature_catalog.md) — Comprehensive feature wishlist organized by subsystem, plus trade-specific applications (automotive, HVAC, audio, ham radio, industrial, marine, and more)
- [Gaps and Priorities](ideas/gaps_and_priorities.md) — Original vs custom firmware comparison, implementation priority order
- [Meter Ideas](ideas/meter_ideas.md) — Multimeter enhancements: calibration, measurement stacks, parasitic drain testing, fuse voltage drop method
- [Project Ideas](ideas/project_ideas.md) — Feasibility analysis for modifications and new features
- [Accessories](ideas/accessories.md) — Community-designed PCBs (RF bridge, SWR analyzer), 3D-printed panel replacement
- [Landscape](ideas/landscape.md) — Budget handheld oscilloscope market and FNIRSI product family
- [Community Issues](ideas/community_issues.md) — 62 documented bugs/requests from EEVblog and forums

## Reverse Engineering

RE methodology and analysis docs. The primary RE artifacts live in [`reverse_engineering/`](../reverse_engineering/) — these are supplementary.

- [Firmware Analysis](re/firmware_analysis.md) — Binary structure, version history (V1.0.3-V1.2.0), size comparison
- [Function Map](re/function_map.md) — Named functions, variables, two-region string system
- [RE Guide](re/re_guide.md) — Tools (Ghidra, binwalk), methodology, how to get started
- [FreeRTOS Tasks](re/freertos_tasks.md) — Task structure, flash base address offset analysis
- [RTOS Analysis](re/rtos_analysis.md) — FreeRTOS kernel identification via string signatures
- [Reference Projects](re/reference_projects.md) — pecostm32 FNIRSI hack, EEVblog, open-source tools

## Quick Start

1. Build firmware: `cd firmware && make` (hardware) or `make emu` (emulator)
2. Flash (with bootloader): Settings > Firmware Update > `make flash`
3. Flash (first time / DFU): `make flash-all` (see [README](../README.md) for EOPB0 setup)
4. Emulate: `make renode` (display-only) or `make renode-interactive` (with buttons)
5. Ghidra analysis: `ghidraRun` > Open `ghidra_project/FNIRSI_2C53T`
6. Read [reverse_engineering/ARCHITECTURE.md](../reverse_engineering/ARCHITECTURE.md) for the hardware reference
