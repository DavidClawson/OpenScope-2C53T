# Reverse Engineering Reference

This directory contains analysis of the FNIRSI 2C53T stock firmware, performed via Ghidra decompilation for the purpose of hardware interoperability.

## Legal Basis

This reverse engineering was conducted for **interoperability purposes** — to understand the hardware interfaces (FPGA protocol, ADC configuration, peripheral initialization, button matrix, etc.) needed to write compatible replacement firmware.

- **US:** Reverse engineering for interoperability is protected under fair use (*Sega v. Accolade*, 9th Cir. 1992; *Sony v. Connectix*, 9th Cir. 2000)
- **EU:** Software Directive (2009/24/EC), Article 6 explicitly permits decompilation for interoperability

No FNIRSI source code is redistributed in this repository. The raw Ghidra decompilation output is excluded from version control. What is published here is **original analytical work**: hardware documentation, protocol specifications, annotated code fragments, and architectural analysis derived from studying the binary.

The replacement firmware in `firmware/src/` is a clean-room implementation that does not copy FNIRSI code.

## Contents

### Documentation
- `ARCHITECTURE.md` — System architecture overview (start here)
- `HARDWARE_PINOUT.md` — Complete MCU pin assignments
- `FPGA_PROTOCOL_COMPLETE.md` — Full FPGA command/data protocol specification
- `CALIBRATION.md` — ADC calibration data format and pipeline
- `HARDWARE_TESTS.md` — Hardware probing procedures and results
- `COVERAGE.md` — RE coverage tracker (309 functions catalogued)

### Analysis (V1.2.0 firmware)
- `analysis_v120/` — Detailed analysis of the latest firmware version
  - `FPGA_TASK_ANALYSIS.md` — FPGA task state machine and sub-functions
  - `FPGA_BOOT_SEQUENCE.md` — 53-step boot initialization sequence
  - `fpga_task_annotated.c` — Annotated FPGA task with commentary
  - `button_map_confirmed.md` — Hardware-confirmed button matrix mapping
  - `function_names.md` — Complete function naming inventory
  - `function_map_complete.txt` — 309-entry function address map

### Reference Data
- `strings_with_addresses.txt` — 290 firmware strings mapped to addresses
- `vector_table.txt` — ARM interrupt vector table
- `dispatch_table.txt` — FPGA command dispatch table
- `gpio_access_map.txt` — All GPIO register accesses

### Ghidra Scripts
- `ghidra_scripts/` — 14 Java scripts for automated firmware analysis
