# Feature Gap Analysis: Original Firmware vs Custom Firmware

*Generated 2026-03-29. Cross-referenced from decompiled V1.2.0 (290 strings, 292 functions), community issues (62 issues), and current custom firmware source.*

## Oscilloscope — Mostly Good, Key Gaps

| Feature | Original | Ours | Status |
|---------|----------|------|--------|
| Time-domain waveform | Yes | Yes (demo data) | UI done, needs real ADC |
| Dual-channel (CH1+CH2) | 5 sub-modes | 1 mode | **Gap: no channel sub-modes** |
| FFT/Split/Waterfall | Broken in original | Working | We're ahead |
| Trigger (Normal/Single/Auto) | Yes | No | **Gap: no trigger UI/logic** |
| Trigger level adjust | Yes | No | **Gap** |
| Coupling (AC/DC/GND) | Yes, per-channel | No | **Gap** |
| Probe setting (1x/10x) | Yes, per-channel | No | **Gap** |
| 20MHz bandwidth limit | Yes | No | **Gap** |
| Timebase selection | Yes | Static | **Gap: no user-adjustable timebase** |
| V/div selection | Yes | Static | **Gap: no user-adjustable v/div** |
| Cursor measurement | Yes | No | **Gap: no cursor UI** |
| Parameter measurement | Yes (auto-measure) | Backend only | **Gap: no auto-measure UI** |
| Persistence display | Yes | Backend only | **Gap: needs UI hookup** |
| Roll mode | Unknown | Backend only | **Gap: needs UI** |
| XY mode | Unknown | Backend only | **Gap: needs UI** |

## Multimeter — Major Gap

| Feature | Original | Ours | Status |
|---------|----------|------|--------|
| DC Voltage | Yes | Demo screen only | **Gap: static demo** |
| AC Voltage | Yes | No sub-mode | **Gap** |
| DC Current (small/large) | Yes (2 ranges) | No | **Gap** |
| AC Current (small/large) | Yes (2 ranges) | No | **Gap** |
| Resistance | Yes | No | **Gap** |
| Continuity (with buzzer) | Yes | No | **Gap** |
| Diode | Yes | No | **Gap** |
| Capacitance | Yes | No | **Gap** |
| Temperature | Yes | No | **Gap** |
| Min/Max/Avg with reset | No (community complaint) | UI drawn, no reset | **Opportunity** |
| Auto-ranging | Yes | Label only | **Gap** |

The original has **10 multimeter sub-modes** (device_mode 5-14). We have 1 static demo screen. Biggest gap but mostly hardware-dependent (FPGA/ADC routing).

## Signal Generator — Close

| Feature | Original | Ours | Status |
|---------|----------|------|--------|
| Sine | Yes | Yes | Done |
| Square | Yes | Yes | Done |
| Triangle | Sawtooth + Reverse | Triangle + Sawtooth | Close enough |
| Full-wave rectified | Yes | No | **Gap** |
| Half-wave rectified | Yes | No | **Gap** |
| Pulse/Sink | Yes | No | **Gap** |
| Lorentz | Yes | No | **Gap** |
| Frequency adjust | Yes | Yes | Done |
| Amplitude adjust | Yes | Hardcoded 3.3V | **Gap: no UI** |
| Duty cycle adjust | Yes (square) | No | **Gap** |
| Output toggle | Yes | Yes | Done |
| Frequency range | 50kHz (was 1MHz) | 25kHz | **Gap** |

## Settings — Mostly Stubs

| Setting | Original | Ours | Status |
|---------|----------|------|--------|
| Language selection | 4+ languages | "English" label only | **Gap** |
| Sound and Light | Buzzer/LED config | Stub | **Gap** |
| Auto Shutdown | Timer config | Stub | **Gap** |
| Display Mode | Unknown | Theme cycling (working!) | We're ahead |
| Startup on Boot | Power-on mode select | Stub | **Gap** |
| About | Version/device info | Stub | **Gap** |
| Factory Reset | With confirmation dialog | Stub | **Gap** |
| USB Sharing | Mass storage mode | Not present | **Gap** |
| Osc Settings sub-menu | CH1/CH2/Trigger config | Not present | **Gap** |

## Features We Have That the Original Doesn't

- Working FFT with 5 window functions
- Waterfall/spectrogram view
- 4 color themes (original has 1 fixed look)
- Protocol decoders (UART/I2C/SPI/CAN/K-Line)
- Bode plot analysis
- Automotive modules (compression test, alternator test)
- Component tester (R/L/C/diode)
- Mask (pass/fail) testing
- Math channel (A+B, A-B, multiply, invert)
- Watchdog + cooperative health monitor
- Soak test infrastructure (2,187 presses / 5 hours, zero faults)

## Buttons Not Yet Wired

| Button | Should Do | Current |
|--------|-----------|---------|
| CH1 | Select/configure CH1 | Nothing |
| CH2 | Select/configure CH2 | Nothing |
| MOVE | Move trigger/cursor | Nothing |
| TRIGGER | Cycle trigger mode | Nothing |
| SAVE | Screenshot / save config | Nothing |

## Priority Order

1. **Oscilloscope channel controls** — trigger, coupling, probe, timebase, v/div UI. Bread-and-butter scope controls. Actionable without hardware.
2. **Settings sub-menus** — nested menus for osc settings, channel config, trigger config. Currently stubs.
3. **Multimeter sub-modes** — 10 modes. Mostly hardware-dependent (FPGA/ADC routing), but UI scaffolding can be built now.
4. **Signal generator missing waveforms** — full-wave, half-wave, pulse, Lorentz. Backend + UI.
5. **Wire remaining buttons** — CH1, CH2, MOVE, TRIGGER, SAVE handlers.
6. **Connect backend features to UI** — persistence, roll mode, XY mode, trend plot, cursor measurement, auto-measurement, component tester, Bode, mask test.
