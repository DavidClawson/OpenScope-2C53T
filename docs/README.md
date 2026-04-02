# FNIRSI 2C53T Open-Source Firmware — Documentation Index

Open-source reverse engineering and firmware replacement for the FNIRSI 2C53T handheld 3-in-1 oscilloscope/multimeter/signal generator.

**Start here:** The project README is at the repo root. For RE details, see `CLAUDE.md`. For the full RE archive, see `reverse_engineering/`.

## Hardware & Architecture

- [Peripheral Map](peripheral_map.md) — Memory-mapped peripherals, GPIO assignments, interrupt vectors, ADC/DAC/SPI/USART usage
- [FPGA Future Possibilities](fpga_future.md) — Gowin GW1N-UV2 resources and enhancement opportunities
- [FreeRTOS Tasks](freertos_tasks.md) — Task structure, flash base address offset analysis
- [RTOS Analysis](rtos_analysis.md) — FreeRTOS kernel identification via string signatures
- [ESP32 Coprocessor](esp32_coprocessor.md) — ESP32 UART bridge for WiFi/BLE connectivity (future mod)

> **Hardware pinout, FPGA protocol, and RE coverage** are maintained in `CLAUDE.md` (canonical) and `reverse_engineering/` (detailed artifacts). Previously duplicated docs (`hardware.md`, `fpga_protocol.md`, `meter_data_format.md`) were removed in the April 2026 cleanup — their content was stale and superseded by the RE archive.

## Reverse Engineering

- [Firmware Analysis](firmware_analysis.md) — Binary structure, version history (V1.0.3–V1.2.0), size comparison
- [Function Map](function_map.md) — Named functions, variables, two-region string system
- [RE Guide](re_guide.md) — Tools (Ghidra, binwalk), methodology, how to get started
- [Feature Gap Analysis](feature_gap_analysis.md) — Original vs custom firmware feature comparison
- See also: `reverse_engineering/COVERAGE.md` — 309 functions mapped, full gap analysis
- See also: `reverse_engineering/FPGA_PROTOCOL_COMPLETE.md` — Complete FPGA command reference
- See also: `reverse_engineering/analysis_v120/` — Full decompile, hardware map, xref map, RAM map

## Features & Design

- [FFT / Spectrum Design](fft_design.md) — Windowing, amplitude scaling, spectrum display architecture
- [Feature Brainstorm](feature_brainstorm.md) — Arbitrary waveforms, protocol patterns, organized by subsystem
- [Module API](module_api.md) — Interface spec for self-contained modules (decoders, analysis, test procedures)
- [Industry Modules](industry_modules.md) — Trade-specific UI overlays (automotive, HVAC, ham radio, small engine, welding, more)
- [Meter Ideas](meter_ideas.md) — Multimeter features: complications UI, auto-hold stack, fuse current tester, parasitic drain
- [Fuse Current Tester](fuse_current_tester.md) — Audio-guided automotive fuse probing (brainstorm, canonical in meter_ideas.md)
- [Logic Analyzer Add-On](logic_analyzer_addon.md) — FX2LP-based USB analyzer for 8-channel digital capture
- [Code Organization](code_organization.md) — Proposed codebase restructuring plan

## Testing & Hardware

- [Hardware Test Protocol](hardware_test_protocol.md) — First-flash and subsystem verification checklist
- [Button Manual](button_manual.md) — Physical button layout, navigation, emulator key bindings
- [Custom Test Platform](custom_test_platform.md) — Repurposing hardware as programmable test fixture

## Planning & Community

- [Roadmap](roadmap.md) — Completed features, hardware bring-up status, future milestones
- [Platform Architecture](platform_architecture.md) — HAL layer design, SDK vision
- [Resource Planning](resource_planning.md) — RAM/flash budget, module hot-loading strategy
- [Developer Experience](developer_experience.md) — Multi-tier accessibility (end users → makers → developers)
- [Community Issues](community_issues.md) — 62 documented bugs/requests from EEVblog and forums
- [Reference Projects](reference_projects.md) — pecostm32 FNIRSI hack, EEVblog, open-source tools
- [Landscape](landscape.md) — Budget handheld oscilloscope market and FNIRSI product family

## Applications & Accessories

- [Use Cases](use_cases.md) — Signal path architecture, module roadmap, USB streaming concept
- [Project Ideas](project_ideas.md) — Resource budget analysis, feasibility for new features
- [Accessories](accessories.md) — Community-designed PCBs (RF bridge, SWR analyzer), 3D-printed top panel replacement

## Quick Start

1. Build firmware: `cd firmware && make` (hardware) or `make emu` (emulator)
2. Flash (with bootloader): Settings → Firmware Update → `make flash`
3. Flash (first time / DFU): `make flash-all` (see CLAUDE.md for EOPB0 setup)
4. Emulate: `make renode` (display-only) or `make renode-interactive` (with buttons)
5. Ghidra analysis: `ghidraRun` → Open `ghidra_project/FNIRSI_2C53T`
6. Read [CLAUDE.md](../CLAUDE.md) for the complete hardware reference
