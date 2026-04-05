# FNIRSI 2C53T Open-Source Firmware Project

## What This Is

Reverse engineering and clean-room rewrite of the firmware for the **FNIRSI 2C53T**, a 3-in-1 handheld oscilloscope/multimeter/signal generator. The original firmware was decompiled from binary using Ghidra and is being refactored into clean, modular C.

## Hardware Target

- **MCU:** Artery AT32F403A — ARM Cortex-M4F @ 240MHz, 1MB flash, 224KB SRAM (with EOPB0=0xFE)
  - Originally identified as GD32F307 from firmware analysis; physical teardown revealed AT32 (markings sanded off)
  - Register-compatible with GD32/STM32F1 at the GPIO/EXMC level
- **LCD:** ST7789V 320x240 RGB565 via 16-bit parallel EXMC/XMC bus
- **FPGA:** Gowin GW1N-UV2 (non-volatile, retains bitstream across power cycles) — handles 250MS/s ADC sampling via **SPI3 data + USART2 commands**
- **SPI Flash:** Winbond W25Q128JVSQ (16MB) — UI assets and system files
- **DAC:** 2-channel 12-bit (built-in) for signal generator output
- **Buttons:** 15 physical buttons — 4x3 bidirectional GPIO matrix (PA7/PB0/PC5/PE2 × PA8/PC10/PE3) + 3 passive (PC8=POWER, PB7=PRM, PC13=UP). 14/15 hardware-confirmed. TMR3 ISR at 500Hz.
- **Touch:** I2C interface (likely GT911/GT915) — not used for button input
- **Board revision:** 2C53T-V1.4

### Confirmed Pin Assignments
| Function | Pin | Notes |
|----------|-----|-------|
| Power hold | PC9 | Must be HIGH immediately at boot or device shuts off |
| LCD backlight | PB8 | HIGH to enable |
| FPGA SPI3 data | PB3 (SCK), PB4 (MISO), PB5 (MOSI) | Bulk ADC data from FPGA (JTAG pins, remapped) |
| FPGA SPI3 CS | PB6 (GPIO) | Software chip select for FPGA SPI3 |
| FPGA SPI enable | PC6 (GPIO) | Must be HIGH for FPGA SPI3 to work |
| FPGA active mode | PB11 (GPIO) | Set HIGH during active measurement mode |
| FPGA USART cmd | PA2 (TX), PA3 (RX) | 9600 baud, 10-byte TX / 12-byte data RX / 10-byte echo RX |
| SWD | PA13 (SWDIO), PA14 (SWCLK) | Debug header near USB-C port |
| Battery sense | PB1 (ADC1 Ch9) | 239.5-cycle sample time, software-triggered |
| UART debug | RX, TX, GND | Through-hole pads (not yet mapped to MCU pins) |
| FPGA programming | M0-M3, GND, VDD, VPP | Header for Gowin programmer |
| Pinhole reset | NRST | Accessible from outside case |

## Repository Layout

```
firmware/           # Custom replacement firmware (GCC + Make)
  src/main.c        # FreeRTOS tasks, LCD init, UI event loop, mode switching
  src/drivers/      # lcd.c / lcd.h — ST7789V driver via EXMC
  include/          # FreeRTOSConfig.h
  FreeRTOS/         # FreeRTOS kernel (submodule)
  at32f403a_lib/    # Artery AT32 HAL library (clone from ArteryTek GitHub)
  gd32f30x_lib/     # GD32 HAL library (legacy, kept for emulator builds)
  bootloader/       # USB HID IAP bootloader (16KB at 0x08000000)
  ld/               # Linker scripts (at32f403a_app.ld for hardware, at32f403a.ld for emu)
  Makefile          # Build: make, make flash, make flash-all, make emu
  Makefile.hwtest   # Minimal hardware test build

reverse_engineering/  # Ghidra decompilation artifacts
  COVERAGE.md             # RE coverage tracker: 309 real functions, fully catalogued
  analysis_v120/          # Latest analysis: full_decompile.c, hardware_map, xref_map, RAM map, FPGA protocol
    fpga_task_annotated.c     # Annotated FPGA task (10 sub-functions, 580+ lines)
    FPGA_TASK_ANALYSIS.md     # FPGA protocol analysis: SPI3 format, command table, state machine
    function_names.md         # Complete function naming inventory (309 real + 61 false positives)
    gap_functions.md          # 17 gap functions catalogued with priorities
    function_map_complete.txt # Complete 309-entry function map
  decompiled_2C53T.c      # V1.2.0 decompilation (~35K lines, 292+ functions)
  decompiled_2C53T_v2.c   # Updated with named functions (~39K lines)
  strings_with_addresses.txt  # 290 strings mapped to firmware addresses
  ghidra_scripts/         # 14 Java automation scripts

emulator/           # Simulation infrastructure
  renode/           # Full-system emulation (GD32F307 platform + peripherals)
  renode_lcd_bridge.py  # WebSocket bridge: Renode framebuffer → React frontend
  emu_2c53t.py      # Unicorn-based emulator (limited — no NVIC/SysTick)

frontend/           # React web UI for LCD display simulation (Vite)
docs/               # 33 design/analysis/planning documents (see docs/README.md for full index)
ghidra_project/     # Pre-analyzed Ghidra database (V1.2.0)
modules/            # JSON procedure files (automotive, HVAC, ham radio, education)
APP_2C53T_*.bin     # Original firmware binaries (V1.0.3, V1.0.7)

FNIRSI_1013D_Firmware/      # Submodule: pecostm32's 1013D replacement firmware
FNIRSI_1014D_Firmware/      # Submodule: 1014D replacement firmware
FNIRSI-1013D-1014D-Hack/    # Submodule: historical RE work
```

## Build & Run

### Hardware (AT32F403A target)
```bash
cd firmware && make              # Build for real hardware (AT32 @ 240MHz)
make flash                       # Flash via USB HID bootloader (case closed)
make flash-all                   # Flash bootloader + app via DFU (first-time setup)
make flash-dfu                   # Flash app via ROM DFU (fallback, BOOT0 + reset)
```

### Normal dev cycle (case closed)
1. On device: Settings → Firmware Update (shows "BOOTLOADER MODE" screen)
2. On host: `cd firmware && make flash`
3. Device auto-reboots into updated firmware

### First-time setup
1. Set EOPB0 for 224KB SRAM (one-time): enter DFU mode (BOOT0 + reset), then:
   `dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin`
2. Flash bootloader + app: `make flash-all`
3. Close the case — all future updates go over USB-C

### Flash layout
```
0x08000000  Bootloader (16KB) — permanent, USB HID IAP
0x08003800  Upgrade flag (2KB sector)
0x08004000  Application (1008KB) — updated via make flash
```

### Emulator
```bash
make emu                         # Build for emulator (skips AT32 clock init)
make renode                      # Run in Renode (display-only)
make renode-interactive          # Run with keyboard input (use with lcd_viewer)
make renode-test                 # 5-second smoke test
```

**Toolchain:** `arm-none-eabi-gcc` via Homebrew cask `gcc-arm-embedded` (native ARM64)
**DFU tool:** `dfu-util` via `brew install dfu-util`
**Renode:** Expected at `/Applications/Renode.app`
**SDL3 viewer:** `cd emulator && make` (requires `brew install sdl3`)
**Logic analyzer:** `sigrok-cli` via `brew install sigrok-cli` — drives HiLetgo 24MHz 8CH USB analyzer (fx2lafw driver)

## Architecture

- **RTOS:** FreeRTOS with Cortex-M4 port (240MHz tick, 1000Hz, 32KB heap)
- **Original firmware tasks:** Display, Input, Acquisition, Measurement, USB, FPGA
- **Current custom firmware:** Display task + Input task, 4 UI modes (scope, meter, signal gen, settings)
- **LCD interface:** Memory-mapped at 0x6001FFFE (command) / 0x60020000 (data), address line A17 selects RS/DCX
- **EXMC config:** SNCTL0=0x5011, SNTCFG0=0x02020424, SNWTCFG0=0x00000202 (works at 240MHz)
- **Font system:** Variable-width bitmap fonts at 4 sizes (12/16/24/48px), generated from TTF via `scripts/generate_font.py`
- **Theme system:** 4 color themes (Dark Blue, Classic Green, High Contrast, Night Red), switchable in Settings > Display Mode
- **Emulator display:** SDL3 native viewer reads `/tmp/openscope_fb.bin` at 30fps; interactive GPIO via `/tmp/openscope_buttons.txt`

## Key Conventions

- Target is Artery AT32F403A (STM32F1-compatible registers for GPIO/EXMC, but uses AT32 HAL for clock/peripheral init)
- IOMUX (AFIO) clock MUST be enabled for EXMC alternate function pins to work
- Power hold (PC9 HIGH) must be the very first operation in main()
- All display rendering is RGB565 (16-bit color)
- Firmware binaries are raw ARM code, not encrypted or compressed
- The decompiled source uses Ghidra naming conventions (FUN_, DAT_, etc.) — rename as functions are understood
- GPL v3 license

## RE Reference

- **Coverage:** 309 real functions fully decompiled and named (138 HIGH, 182 MEDIUM, 42 LOW confidence). 61 Ghidra false positives eliminated.
- **FPGA interface:** Dual-channel — SPI3 (60MHz) for bulk ADC data, USART2 (9600 baud) for commands. Fully annotated.
- **FPGA task:** 10 sub-functions across 11.5KB, annotated in `analysis_v120/fpga_task_annotated.c`
- **SPI3 data format:** Interleaved CH1/CH2 unsigned 8-bit. Normal=1024B (512 pairs), dual=2048B. ADC offset=-28.0.
- **SPI3 config:** Mode 3 (CPOL=1, CPHA=1), Master, /2 clock (60MHz), 8-bit, software CS on PB6
- **USART protocol:** 10-byte TX frames (header + cmd + params + checksum). RX is two distinct frame types on the same stream: **12-byte data frames** (header `0x5A 0xA5`, carry BCD digits + status) and **10-byte echo frames** (header `0xAA 0x55`, FPGA acks every TX command; byte[3] echoes our cmd, byte[7] = fixed `0xAA` integrity marker). Timer-driven via TMR3. See `analysis_v120/usart2_isr_state_machine.md`.
- **FPGA command codes:** ALL ~40 mapped (0x00-0x2C) — scope, trigger, timebase, meter, siggen, freq counter, period, duty cycle, continuity/diode. Dispatch table at 0x0804BE74.
- **FPGA control pins:** PC6 = SPI enable (HIGH), PB11 = active mode (HIGH), PC11 = meter MUX enable (HIGH in meter mode)
- **Analog frontend relays:** PC12 = input routing, PE4/PE5/PE6 = range/attenuation select. DCV pattern: PC12=H, PE4=H, PE5=L, PE6=H
- **Gain resistors:** PA15, PA10 = gain select (HIGH for DCV), PB10 = gain select (LOW for DCV). Controlled by gpio_mux_porta_portb (FUN_08001A58)
- **Additional frontend pins:** PB9, PA6 = analog frontend control (undocumented, configured as outputs in stock init)
- **SPI3 bulk exchange (cmds 0x3B/0x3A) — RESOLVED 2026-04-04:** Stock master init runs a bulk SPI3 transfer during the FPGA init phase, opened with opcode 0x3B, closed with opcode 0x3A. Source table lives at `0x08051D19` in the V1.2.0 binary. **Transfer size is 115,638 bytes** (38,546 × 3-byte records) — the prior "411 bytes" folklore was wrong. Ghidra had "misdecoded" the disasm region 4798–4833 as instructions; hand-decoding showed those 56 bytes are actually ASCII FreeRTOS task name strings (`Timer1`, `Timer2`, `display`, `key`, `osc`, `fpga`, `dvom_TX`, `dvom_RX`) embedded in the code stream with a `b.n` branch jumping over them. There is no early-exit; the loop runs the full 115,638 bytes. The table has a 160-byte block structure with `FF FF FF FF FF FF` sentinels at byte 30 of each block — characteristic of a register-init table, not an FPGA bitstream. Remains the leading hypothesis for missing factory calibration (low-Ω, DCV >10V). **Our firmware does NOT replay this exchange** — the replay experiment is the next big RE move. See `analysis_v120/spi3_bulk_cal_resolved.md` and `analysis_v120/fpga_h2_spi3_bulk.md`.
- **Master init decompilation:** 4 phase files in analysis_v120/master_init_phase[1-4].c (~3500 lines total)
- **Buttons: 14/15 HARDWARE CONFIRMED.** Bidirectional 4x3 matrix + 3 passive. See `analysis_v120/button_map_confirmed.md` for complete mapping. Only PRM (PB7) unresolved.
- **Acquisition:** Double-buffered (2 queue items per trigger), 9-mode state machine (fast TB, roll, normal, dual, extended, meter ADC, siggen, calibration, self-test)
- **Calibration:** ADC offset -28.0, per-channel VFP pipeline. Scope uses a 120-byte gain/offset cal table at RAM 0x20000358 (6 entries × 20 bytes, loaded from SPI flash at boot), indexed by voltage range. FUN_080018A4 (`gpio_mux_portc_porte`) reads this table to compute the scope trigger comparator DAC1 value — it's a SCOPE trigger path, not meter cal. The 301-byte regions at state[0x356]/state[0x483] are scope roll-mode sample buffers, NOT calibration data. **Meter cal source is still unresolved** — the leading hypothesis is the FPGA-side 411-byte cal exchange via cmds 0x3B/0x3A at boot. See analysis_v120/cal_data_myth_busted.md for the full hypothesis history and H1 postmortem.
- **Meter data:** BCD digit extraction from cross-byte nibbles in USART RX frames, 8-state mode FSM. Frame layout: 12-byte data frames (header `0x5A 0xA5`) and 10-byte echo frames (header `0xAA 0x55`, byte[3]=echoed cmd, byte[7]=`0xAA` integrity marker). See `analysis_v120/usart2_isr_state_machine.md`.
- **Meter DAC reference calibration:** Per-range DAC value computed by `FUN_080018A4` using meter cal tables at `0x20000394` (upper bounds) and `0x20000358` (baselines), each 2 bytes × N entries (~40 bytes total). Formula: `DAC = ((*puVar1 - *puVar2) / divisor) * (offset + 100) + *puVar2`, written to `0x40007408`. The `CLAUDE.md` claim about "301-byte per-channel meter cal data loaded from SPI flash" was wrong — that 301-byte region is the scope roll-mode sample buffer, not meter cal. See `analysis_v120/cal_data_myth_busted.md`.
- **Boot sequence:** 53-step init documented in `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md`
- **Master init:** FUN_08023A50 (15.4KB) — configures all peripherals, creates all FreeRTOS tasks
- **8 FreeRTOS tasks:** display, key, osc, fpga, dvom_TX, dvom_RX, Timer1, Timer2
- **7 FreeRTOS queues:** usart_cmd (0x20002D6C), button_event (0x20002D70), usart_tx (0x20002D74), spi3_data (0x20002D78), meter_sem (0x20002D7C), fpga_sem1 (0x20002D80), fpga_sem2 (0x20002D84)
- **Auto power-off:** 3 tiers (15min/30min/1hr) based on probe state
- **Watchdog:** IWDG fed every 11 calls to input_and_housekeeping (~50ms)
- Calibration tables in RAM: 120-byte scope gain/offset cal at 0x20000358 (6 entries × 20 bytes, loaded from SPI flash at boot). Used by scope_main_fsm (indexed by voltage range) and FUN_080018A4 (`gpio_mux_portc_porte`) to compute the scope trigger comparator DAC1 value. Not a meter cal source.
- Filesystem paths in firmware: `2:/Screenshot file/`, `3:/System file/`
- Firmware versions analyzed: V1.0.3 → V1.0.7 → V1.1.2 → V1.2.0
- **IOMUX remap:** `(reg & ~0xF000) | 0x2000` at AFIO+0x08 — disables JTAG-DP, keeps SW-DP, frees PB3/PB4/PB5 for SPI3
- **Battery ADC:** PB1 / ADC1 Channel 9, 239.5-cycle sample, right-aligned, software-triggered
- **TMR8:** Actually IS configured (ARR=99, generates periodic interrupt for FatFs). Not unused as previously thought.
- **DMA:** Ch1 = LCD framebuffer (16-bit mem-to-mem → EXMC). SPI3 = polled. USART2 = interrupt-driven.
- **Key docs:** `reverse_engineering/ARCHITECTURE.md` (start here), `FPGA_PROTOCOL_COMPLETE.md`, `HARDWARE_PINOUT.md`, `CALIBRATION.md`, `analysis_v120/FPGA_TASK_ANALYSIS.md`

## Current State

- **Full oscilloscope UI running on real FNIRSI 2C53T hardware** (AT32F403A @ 240MHz, battery powered)
- LCD driver functional with multi-size font system (4 sizes from SF Pro + Menlo)
- 4 themed UI modes: oscilloscope (with FFT/waterfall), multimeter (large digits), signal generator, navigable settings
- Theme switching (4 themes) wired through all screens
- FreeRTOS scheduler running with display + input tasks
- Power management: PC9 hold, PB8 backlight, POWER button 3-2-1 countdown shutdown, low-battery auto-off at 3.3V
- **USB HID bootloader** — closed-case firmware updates via `make flash`, LCD status screen, POWER button exit, auto-reboot after flash
- Battery monitor: PB1 ADC with 16-sample averaging, percentage display, USB charge detection ("CHG"), LiPo protection shutdown
- SDL3 native LCD viewer with interactive button input for emulator
- Soak testing infrastructure (random button fuzzing with fault monitoring)
- Watchdog, health monitoring, task stack checking
- **Button input: 14/15 HARDWARE CONFIRMED** — bidirectional 4x3 matrix scan at 500Hz via TMR3 ISR. Rows: PA7,PB0,PC5,PE2. Cols: PA8,PC10,PE3. Passive: PC8(POWER),PB7(PRM),PC13(UP). Complete mapping in `analysis_v120/button_map_confirmed.md`. PRM (PB7) detection still needs work.
- **FPGA USART communication working** — bidirectional, frames captured, meter data flowing
- **Meter reading pipeline working** — first live readings 2026-04-03. The original "3.7x high" problem turned out to be a **decimal-point placement bug**, not a gain error; fixed once the frame[6] decoder was wired up. DCV 0–9V and the full resistance range are now accurate within a few percent. See `analysis_v120/meter_math_pipeline_annotated.c` for the full pipeline trace and `src/drivers/meter_data.c` for the current decoder.
- **Meter low-Ω and kΩ band override (2026-04-04):** Before tonight the display flickered between correct and wrong readings on the same resistor (e.g., a 10kΩ probe alternated between `9.821 kOhm` ✓ and `98.24 kOhm` ✗). Root cause: the FPGA meter IC rotates through several `frame[6]` byte variants per measurement (`0x07, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F` for low-Ω; `0x40, 0x4A, 0x4B, 0x4D, 0x4E` for kΩ), each claiming a different dp interpretation of the *same* raw BCD. Fix: `meter_data.c` now computes the resistance from `raw_bcd` directly at the **band** level, ignoring the per-frame dp hint. Upper nibble 0 of frame[6] → low-Ω regime → `ohms = raw_bcd × 0.0304`. Upper nibble 4 → kΩ regime → `kohms = raw_bcd × 0.001`. Result is stable regardless of which frame variant arrives first. Factor 0.0304 is per-device (factory-calibrated); currently hardcoded for bench unit #1. See `analysis_v120/meter_math_pipeline_annotated.c`.
- **Meter DCV >10V is a known limitation (2026-04-04):** Bench captures show 7V reads correctly (f6 stable at `0x07`, raw 7005, dp=1) but 11V reads as `0.997V` (f6 rotates through `0x0A, 0x0F, 0x0B, 0x07`, raw=997, wrong dp latch). Same root cause as low-Ω: the meter IC's auto-range is unstable without factory cal. Candidate fixes filed in `meter_data.c`: (1) systematic bench capture across voltages, (2) firmware-driven range select via FPGA commands, (3) H2 SPI3 bulk cal replay experiment. Deferred to a future session.
- **Boot safety system** — 3-strike crash recovery: app crashes 3x → bootloader enters SAFE MODE automatically. Hold POWER at boot = force bootloader. boot_validate() handshake. Never need to open case again.
- **Fuse current tester UI** — 4th meter layout (OK cycles Full→Chart→Stats→Fuse). 3 views, 5 fuse types, 47 ratings. Uses real meter mV readings for parasitic drain estimation.
- **FPGA SPI3 root cause identified** — was missing: PB11 HIGH (active mode), full USART boot command sequence (0x01-0x08), queue-driven triggering (not polled), SysTick delays between boot phases. See `FPGA_TASK_ANALYSIS.md`
- **Stock firmware ~98% understood** — 309 functions named, ALL FPGA commands mapped, ADC format cracked, button input resolved, battery ADC found, IOMUX remap extracted. Master init fully decompiled in 4 phase files. Remaining: dispatch table mechanism (null entries), 42 low-confidence function names.
- **Meter poll decoupled from UI (2026-04-04):** The FPGA meter IC only emits data frames in response to recent TX commands. This was originally worked around by calling `fpga_send_cmd(0x00, 0x09)` inside `draw_meter_screen()`, which coupled the data cadence to the UI redraw rate. Now handled by a dedicated `fpga_meter_poll_task` in `fpga.c` that polls at ~4 Hz while `current_mode == MODE_MULTIMETER`, regardless of UI activity. Reduces USART traffic ~7.5x and decouples data flow from draw scheduling. See `analysis_v120/usart2_isr_state_machine.md`.
- **Meter redraw throttled to data-change events (2026-04-04):** `main.c` was unconditionally calling `draw_meter_screen()` every 50ms (20 Hz), which caused visible flicker from the full-area `lcd_fill_rect` → redraw sequence. Now gated on `meter_reading.update_count` changing, with a 1-second safety tick. Drops redraws to the FPGA data rate (~4 Hz) and eliminates flicker. Scope and signal gen still have the same structural issue (unconditional animation branches) — filed as a future "rendering pass" TODO that would introduce per-component dirty tracking (retained-mode UI à la React reconciliation). 224KB SRAM is plenty for component-level dirty state; no framebuffer needed.

## Five-Agent RE Session (2026-04-04)

A parallel five-agent deep-dive session produced five substantial analysis files and surfaced several corrections to prior claims. Deliverables in `reverse_engineering/analysis_v120/`:

1. **`meter_math_pipeline_annotated.c`** (1008 lines) — Full annotation of `dvom_rx_task` (0x08036AC0, 1776B) and `fpga_state_update` (0x080028E0, 768B). BCD digit extraction from cross-byte nibbles, 8-state mode FSM, decimal scaling pipeline, frame[6] decoder table. The "3.7x high" bug turned out to be a decimal-point placement issue (already fixed).

2. **`spi3_bulk_cal_resolved.md`** — Hand-decoded the init lines 4798–4833 that Ghidra had mis-labeled as instructions. Definitively resolved the SPI3 bulk exchange size: **115,638 bytes, not 411**. Table structure suggests a register-init blob, not an FPGA bitstream. Remains the leading hypothesis for missing factory cal.

3. **`scope_render_monsters_annotated.c`** (1535 lines) — Annotation of the "waveform render" monsters and `scope_mode_cursor`. **Major correction**: `FUN_08030524` (6632B) and `FUN_08031f20` (4110B), labeled `waveform_render_ch1/ch2` in `function_names.md`, are actually a JFIF/JPEG Huffman decoder (SOI markers, DHT tables, IDCT butterfly constants `0x16A0`, `0x1D90`, `0x29CF`). They decode boot-logo / UI assets from SPI flash. The real scope waveform draw is a **filled-capsule (Bresenham circle end-caps + line segments) in `FUN_08019470` + `FUN_08018DA0` + `FUN_08015F50`**, fixed color `0xFB43` (amber), no persistence. Y-transform formula: `display_y = (scale_a/scale_b) × (raw − 128.0 − baseline) + 128.0 + dc_offset`, clamped to `[28, 228]`, `scale_a/b` from a 16-bit LUT at flash `0x080465CC`. Also: `scope_mode_cursor` is actually a **measurement engine** computing Vpp/Vmax/Vmin/Vavg/Vrms/frequency/period with 64-bit VFP accumulators — should be renamed `scope_measurement_engine`.

4. **`stock_iap_bootloader.md`** — Annotation of USB and FMC functions. **Major correction**: stock firmware has **no in-app programming path at all**. USB is Mass Storage Class (BOT) on EP1 bulk with 12 SCSI commands (screenshot/file browser), not HID IAP. `FUN_0802f3e4` and `FUN_0802f5ec`, labeled `fmc_program_flash` and `fmc_erase_page` in `function_names.md`, are actually **option-byte writers** (they set EOPB0=0xFE for the 224KB SRAM mode). Stock factory upgrades must have used ROM DFU or a separate bootloader binary not present in `APP_2C53T_V1.2.0`. **The "caseless bridge flash" idea is infeasible** — new units need ROM DFU (BOOT0 + reset) or SWD once; after that our HID bootloader handles everything.

5. **`reset_and_clock_init.md`** — Decoded the reset handler and clock init at `0x08000000–0x08000238`. **240MHz confirmed**, but via a different PLL path than ours: stock uses `HEXT ÷ 2 × 60 = 240MHz` (pllhextdiv=1, pllmult_l=11, pllmult_h=3 → 11 + 16×3 + 1 = 60). Our firmware uses `HEXT × 30 = 240MHz`; both produce the same SYSCLK. Stock is built with **Keil MDK (ARMCC)**, not GCC — explains the scatter-load layout, TBB jump tables, and absence of an explicit `SystemInit()`. No firmware changes needed.

**Corrections to `analysis_v120/function_names.md` stemming from this session:**
- `waveform_render_ch1` / `ch2` → `jpeg_huffman_decode_1` / `_2`
- `scope_mode_cursor` → `scope_measurement_engine`
- `fmc_program_flash` → `option_bytes_write_optkeyr`
- `fmc_erase_page` → `option_bytes_erase` (companion)
