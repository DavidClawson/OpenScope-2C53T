# FNIRSI 2C53T Open-Source Firmware — Documentation Index

Open-source reverse engineering and firmware replacement for the FNIRSI 2C53T handheld 3-in-1 oscilloscope/multimeter/signal generator.

## Hardware & Architecture

- [Hardware Identification](hardware.md) — AT32F403A MCU (not GD32), full pinout, peripheral mapping, EOPB0 config
- [Peripheral Map](peripheral_map.md) — Memory-mapped peripherals, GPIO assignments, interrupt vectors, ADC/DAC/SPI/USART usage
- [FPGA Protocol](fpga_protocol.md) — USART2 command frames, SPI2 bulk data, acquisition pipeline, RTOS task architecture
- [FPGA Future Possibilities](fpga_future.md) — Gowin GW1N-UV2 resources and enhancement opportunities
- [FreeRTOS Tasks](freertos_tasks.md) — Task structure, flash base address offset analysis
- [RTOS Analysis](rtos_analysis.md) — FreeRTOS kernel identification via string signatures
- [ESP32 Coprocessor](esp32_coprocessor.md) — ESP32 UART bridge for WiFi/BLE connectivity

## Reverse Engineering

- [Firmware Analysis](firmware_analysis.md) — Binary structure, version history (V1.0.3–V1.2.0), size comparison
- [Function Map](function_map.md) — Named functions, variables, two-region string system
- [RE Guide](re_guide.md) — Tools (Ghidra, binwalk), methodology, how to get started
- [Feature Gap Analysis](feature_gap_analysis.md) — Original vs custom firmware feature comparison
- See also: `reverse_engineering/COVERAGE.md` — 362 functions mapped, 45.5% gap analysis, priority tiers
- See also: `reverse_engineering/analysis_v120/` — Full decompile, hardware map, xref map, RAM map, FPGA protocol

## Features & Design

- [FFT / Spectrum Design](fft_design.md) — Windowing, amplitude scaling, spectrum display architecture
- [Feature Brainstorm](feature_brainstorm.md) — Arbitrary waveforms, protocol patterns, organized by subsystem
- [Module API](module_api.md) — Interface spec for self-contained modules (decoders, analysis, test procedures)
- [Industry Modules](industry_modules.md) — Trade-specific UI overlays (automotive, HVAC, ham radio, education)
- [Meter Data Format](meter_data_format.md) — DVOM data pipeline: raw FPGA floats → auto-range → display formatting
- [Meter Ideas](meter_ideas.md) — Multimeter display concepts inspired by Apple Watch complications
- [Logic Analyzer Add-On](logic_analyzer_addon.md) — FX2LP-based USB analyzer for 8-channel digital capture
- [Code Organization](code_organization.md) — Proposed codebase restructuring plan

## Testing & Hardware Bring-Up

- [Device Testing Plan](device_testing_plan.md) — First-flash checklist with equipment list
- [Hardware Test Protocol](hardware_test_protocol.md) — Subsystem verification with minimal equipment
- [Button Manual](button_manual.md) — Physical button layout, navigation, emulator key bindings
- [Custom Test Platform](custom_test_platform.md) — Repurposing hardware as programmable test fixture

## Planning & Community

- [Roadmap](roadmap.md) — Completed features, in-progress work, future milestones
- [Platform Architecture](platform_architecture.md) — HAL layer design, SDK vision, making the codebase a reusable developer platform
- [Resource Planning](resource_planning.md) — RAM/flash budget, module hot-loading strategy
- [Developer Experience](developer_experience.md) — Multi-tier accessibility (end users → vendors)
- [Community Issues](community_issues.md) — 62 documented bugs/requests from EEVblog and forums
- [Reference Projects](reference_projects.md) — pecostm32 FNIRSI hack, EEVblog, open-source tools
- [Landscape](landscape.md) — Budget handheld oscilloscope market and FNIRSI product family

## Applications & Accessories

- [Use Cases](use_cases.md) — Signal path architecture, module roadmap
- [Project Ideas](project_ideas.md) — Resource budget analysis, feasibility for new features
- [Accessories](accessories.md) — Community-designed open-source PCBs (RF bridge, SWR analyzer)

## Quick Start

1. Install tools: `brew install ghidra gcc-arm-embedded dfu-util`
2. Build firmware: `cd firmware && make` (hardware) or `make renode` (emulator)
3. Flash via DFU: `make flash-dfu` (see [Hardware Test Protocol](hardware_test_protocol.md))
4. Ghidra analysis: `ghidraRun` → Open `ghidra_project/FNIRSI_2C53T`
5. Read the [RE Guide](re_guide.md) for methodology
