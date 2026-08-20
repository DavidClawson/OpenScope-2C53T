# Documentation Index

Open-source replacement firmware for the FNIRSI 2C53T handheld oscilloscope / multimeter / signal generator.

**Start here:** The project [README](../README.md) is at the repo root. For the full RE archive, see [`reverse_engineering/`](../reverse_engineering/).

---

## Top-Level

- [Feature specs](specs/) — the plan: one promotion-ladder spec per feature, plus the catalog of what's missing and the community-demand audit
- [Structural audit 2026-08-20](structural_audit_2026-08-20.md) — RTOS/reliability pass: bus ownership, torn buffers, silent-success error paths, decorative controls, dead weight — prioritized P0–P3
- [Roadmap](roadmap.md) — design sections only (region layer, user cal, stretch tracks); status moved to the README maturity matrix + specs
- [Button Manual](button_manual.md) — Physical button layout, navigation, emulator key bindings
- [Hardware Test Protocol](hardware_test_protocol.md) — First-flash and subsystem verification checklist

## Design

Architecture, technical specs, and system design documents.

- [Peripheral Map](design/peripheral_map.md) — Memory-mapped peripherals, GPIO assignments, interrupt vectors
- [FFT Design](design/fft_design.md) — Windowing, amplitude scaling, spectrum display architecture
- [DMM Voltage Waveform](design/dmm_voltage_waveform.md) — Multimeter voltage-mode waveform overlay using the DMM probe path
- [FPGA Future Possibilities](design/fpga_future.md) — Gowin GW1N-UV2 resources and enhancement opportunities
- [FPGA Gateware Plan](design/fpga_gateware_plan.md) — Custom bitstream track: trigger engine design, Tang Nano 20K testbench discipline, three-leg verification, runtime protocol
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

## Community Tools & Sibling Projects

Independent firmware and tooling for the same hardware, by contributors. Not maintained here — see each repo.

- [maksidze/FNIRSI-2C53T-flash-dump](https://github.com/maksidze/FNIRSI-2C53T-flash-dump) — GUEST firmware (`0x08007000`, flashed with `scripts/iap_flash.py`) exposing the W25Q128 over USB MSC: `FAT0` (2 MB system assets), `FAT1` (14 MB user data) and a raw 64 MiB `FLASH WRITE` volume with byte-exact `FLASH.BIN` dump/restore. **The W25Q recovery path** — and the tool for cross-unit / factory-cal dumps (issue #18, 2026-08-15).
- [maksidze/DOOM-2C53T](https://github.com/maksidze/DOOM-2C53T) — DOOM on the 2C53T; its `pwm_audio.c` is the reference for the PB9 buzzer (TMR4_CH4 PWM, issue #25).
- [Stlkv/OpenScope-2C23T-2C53T-port](https://github.com/Stlkv/OpenScope-2C23T-2C53T-port) (branch `2c53t-port`) — the bit-bang SSPI loader transplant that first cold-configured the 2C53T FPGA (issue #18); our `fpga_bitbang_config_sequence()` is ported from it.
- [rosenrot00/OpenScope-2C23T](https://github.com/rosenrot00/OpenScope-2C23T) — independent 2C23T firmware with a working scope; source of the PC8/PC9 config-control observations.
- [DavidClawson/gw1n2-apicula](https://github.com/DavidClawson/gw1n2-apicula) — sibling repo: GW1N-2 bitstream RE / Apicula support, netlist and capture-path sim harness for this FPGA.

## Quick Start

1. Build firmware: `cd firmware && make` (hardware) or `make emu` (emulator)
2. Flash (with bootloader): Settings > Firmware Update > `make flash`
3. Flash (first time / DFU): `make flash-all` (see [README](../README.md) for EOPB0 setup)
4. Emulate: `make renode` (display-only) or `make renode-interactive` (with buttons)
5. Ghidra analysis: `ghidraRun` > Open `ghidra_project/FNIRSI_2C53T`
6. Read [reverse_engineering/ARCHITECTURE.md](../reverse_engineering/ARCHITECTURE.md) for the hardware reference
