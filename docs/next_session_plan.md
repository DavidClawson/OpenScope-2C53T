# Next Session Plan: Real Measurement Data

## Status After Bootloader Session

**Bootloader: DONE** — USB HID IAP bootloader working, closed-case `make flash`, LCD status screen, POWER button exit, auto-reboot after flash.

**Current firmware:** Full UI with 4 modes, theme switching, settings navigation — but **all measurement data is simulated** (demo waveforms, hardcoded meter values).

## What's Needed for Real Measurements

All measurement data (scope AND meter) flows through the **FPGA** (Gowin GW1N-UV2):
- **Oscilloscope:** FPGA samples 250MS/s ADC → sends data via **SPI3** (PB3/4/5/6)
- **Multimeter:** FPGA measures → sends BCD digits via **USART2** (PA2/PA3)
- **MCU ADC** is only used for battery voltage (PB1)

### The FPGA Must Be Booted First

The FPGA has its own bitstream (non-volatile) but needs a ~50-step command sequence from the MCU before it starts producing data. This is fully documented in `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md`.

## Roadmap (ordered by dependency)

### Phase 1: FPGA USART Communication (Meter Data)
**Easiest win — meter readings with no probes needed**

1. Initialize USART2 (PA2 TX, PA3 RX, 9600 baud)
2. Set PC6 HIGH (FPGA SPI enable pin)
3. Send FPGA boot commands 0x01-0x08 via USART2
4. Set PB11 HIGH (FPGA active mode)
5. Parse 12-byte USART RX frames (0x5A 0xA5 header + BCD meter data)
6. Wire parsed meter values to meter_ui.c (replace demo_value)

**Test:** Connect probes to a battery, see real DC voltage on the meter screen.

**Key files:**
- `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md` — init sequence
- `reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md` — USART frame format
- `firmware/src/fpgainit.c` — existing USART2 init code (partially working)
- `firmware/src/fpgatalk.c` — frame TX/RX handling

### Phase 2: FPGA SPI3 Acquisition (Scope Data)
**The big one — real oscilloscope waveforms**

1. Configure SPI3 (PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS as GPIO)
2. IOMUX remap: disable JTAG, keep SWD, free PB3/PB4/PB5
3. SPI3 Mode 3 (CPOL=1, CPHA=1), /2 prescaler (60MHz)
4. Implement SPI3 handshake (command 0x05 sequence)
5. Read 1024-byte blocks: interleaved CH1/CH2, unsigned 8-bit, offset -28.0
6. Double-buffer with FreeRTOS queue (2 items per trigger)
7. Apply VFP calibration pipeline (per-channel gain/offset from flash cal data)
8. Wire to scope_ui.c (replace draw_demo_waveform)

**Test:** Connect probe to signal generator output, see real waveform.

**Key files:**
- `reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md` — SPI3 format, capture modes
- `reverse_engineering/analysis_v120/fpga_task_annotated.c` — 580-line annotated FPGA task

### Phase 3: Timebase and Trigger Control
**Make the scope actually usable**

1. Implement USART2 command sender for timebase changes (cmd 0x04)
2. Trigger level/edge/mode commands (cmd 0x06-0x08)
3. Wire scope_state timebase/trigger changes to FPGA commands
4. Implement auto-trigger and normal trigger modes

### Phase 4: Signal Generator Output
**Already partially working (DAC init exists)**

1. Verify DAC output on probe tip
2. Wire siggen_ui frequency/waveform selection to DAC updates
3. Test with scope in loopback (siggen output → scope input)

## Hardware Needed

- **Phase 1:** Just the device + USB-C cable (meter probes are built in)
- **Phase 2:** Probe + any signal source (even touching a wire to see noise)
- **Phase 3:** Same as Phase 2
- **Phase 4:** Oscilloscope probe or multimeter to verify output

## Quick Wins (No FPGA Needed)

These can be done in parallel with FPGA work:
- **Battery voltage display:** Wire PB1 ADC reading to status bar (already have the ADC code in RE)
- **Power-off from software:** Long-press POWER → drop PC9 LOW
- **Settings persistence:** Save/load theme, coupling, probe settings to SPI flash
- **About screen:** Fix MCU string (says GD32F307 but it's AT32F403A)
