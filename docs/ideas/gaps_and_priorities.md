# Gaps and Priorities

What the original firmware does that ours doesn't yet, and the priority order for closing those gaps.

*Merged from feature_gap_analysis.md and use_cases.md. Generated 2026-03-29.*

---

## Signal Path Architecture

Understanding which signals go where is key to knowing what's possible:

```
Scope probes -> Analog front end -> External ADC -> FPGA -> MCU (via USART2)
                (amps, attenuators,  (250 MS/s)    (buffers +    (receives
                 protection)                         processes)    digital data)

Multimeter -> Separate analog path -> FPGA or MCU internal ADC (12-bit)

Signal gen -> MCU DAC (12-bit) -> Output amplifier -> BNC output
```

The oscilloscope analog signals never touch the MCU directly — all high-speed sampling is done by the FPGA. The MCU receives digital sample buffers and handles display, UI, and storage.

---

## Oscilloscope Gaps

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

## Multimeter Gaps

The original has **10 multimeter sub-modes** (device_mode 5-14). We have 1 static demo screen. Biggest gap but mostly hardware-dependent on FPGA/ADC routing.

| Feature | Original | Ours | Status |
|---------|----------|------|--------|
| DC Voltage | Yes | Demo screen only | **Gap** |
| AC Voltage | Yes | No | **Gap** |
| DC Current (small/large) | Yes (2 ranges) | No | **Gap** |
| AC Current (small/large) | Yes (2 ranges) | No | **Gap** |
| Resistance | Yes | No | **Gap** |
| Continuity (with buzzer) | Yes | No | **Gap** |
| Diode | Yes | No | **Gap** |
| Capacitance | Yes | No | **Gap** |
| Temperature | Yes | No | **Gap** |
| Min/Max/Avg with reset | No (community complaint) | UI drawn, no reset | **Opportunity** |
| Auto-ranging | Yes | Label only | **Gap** |

## Signal Generator Gaps

| Feature | Original | Ours | Status |
|---------|----------|------|--------|
| Sine | Yes | Yes | Done |
| Square | Yes | Yes | Done |
| Triangle | Sawtooth + Reverse | Triangle + Sawtooth | Close enough |
| Full-wave rectified | Yes | No | **Gap** |
| Half-wave rectified | Yes | No | **Gap** |
| Pulse/Sink | Yes | No | **Gap** |
| Lorentz | Yes | No | **Gap** |
| Amplitude adjust | Yes | Hardcoded 3.3V | **Gap: no UI** |
| Duty cycle adjust | Yes (square) | No | **Gap** |
| Frequency range | 50kHz (was 1MHz) | 25kHz | **Gap** |

## Settings Gaps

| Setting | Original | Ours | Status |
|---------|----------|------|--------|
| Language selection | 4+ languages | "English" only | **Gap** |
| Sound and Light | Buzzer/LED config | Stub | **Gap** |
| Auto Shutdown | Timer config | Stub | **Gap** |
| Display Mode | Unknown | Theme cycling (working!) | We're ahead |
| About | Version/device info | Stub | **Gap** |
| Factory Reset | With confirmation | Stub | **Gap** |
| USB Sharing | Mass storage mode | Not present | **Gap** |
| Osc Settings sub-menu | CH1/CH2/Trigger config | Not present | **Gap** |

## Features We Have That the Original Doesn't

- Working FFT with 5 window functions (original's FFT is broken)
- Waterfall/spectrogram view
- 4 color themes (original has 1 fixed look)
- Protocol decoders (UART/I2C/SPI/CAN/K-Line)
- Bode plot analysis
- Automotive modules (compression test, alternator test)
- Component tester (R/C/diode)
- Mask (pass/fail) testing
- Math channels (A+B, A-B, multiply, invert)
- Watchdog + health monitor
- Soak test infrastructure (2,187 presses / 5 hours, zero faults)

## Unwired Buttons

| Button | Should Do | Current |
|--------|-----------|---------|
| CH1 | Select/configure CH1 | Nothing |
| CH2 | Select/configure CH2 | Nothing |
| MOVE | Move trigger/cursor | Nothing |
| TRIGGER | Cycle trigger mode | Nothing |
| SAVE | Screenshot / save config | Nothing |

---

## Implementation Priority

### Phase 1: Foundation (prove the concept)

| Priority | Feature | Why First |
|---|---|---|
| 1 | UART decode | Simplest protocol, proves overlay concept |
| 2 | I2C decode | Most-requested by electronics hobbyists |
| 3 | SPI decode | Completes the "big three" protocols |

### Phase 2: Automotive (the compelling differentiator)

| Priority | Feature | Why |
|---|---|---|
| 4 | CAN bus waveform analysis | Useful even without protocol decode |
| 5 | CAN bus decode | Message IDs + data overlaid on waveform |
| 6 | OBD-II reader | Standard PIDs to human-readable values |
| 7 | Relative compression test | No cheap tool does this |
| 8 | Injector analysis | Common diagnostic procedure |

### Phase 3: Advanced Features

| Priority | Feature |
|---|---|
| 9 | USB streaming to PC (sigrok/PulseView) |
| 10 | Long-term data logging to flash |
| 11 | Component tester (curve tracer) |
| 12 | Bode plot (signal gen + scope) |
| 13 | Known-good waveform library |
| 14 | Guided diagnostic tests (JSON procedures) |

### Firmware Priority Order

1. **Oscilloscope channel controls** — trigger, coupling, probe, timebase, v/div. Bread-and-butter scope controls.
2. **Settings sub-menus** — nested menus for osc settings, channel config, trigger config.
3. **Multimeter sub-modes** — 10 modes. Mostly hardware-dependent, but UI scaffolding can be built now.
4. **Signal generator missing waveforms** — full-wave, half-wave, pulse, Lorentz.
5. **Wire remaining buttons** — CH1, CH2, MOVE, TRIGGER, SAVE handlers.
6. **Connect backend features to UI** — persistence, roll mode, XY mode, trend plot, cursor, auto-measurement, component tester, Bode, mask test.

---

## Vehicle Definition Files

For automotive modules, vehicle-specific data stored on the filesystem:

```
2:/Vehicles/
  generic_obd2.json          # Standard OBD-II PIDs (universal)
  toyota_camry_2020.json     # Toyota-specific CAN IDs
  honda_civic_2019.json      # Honda-specific CAN IDs
  ford_f150_2021.json        # Ford-specific CAN IDs
```

The DBC file format (industry standard) could also be supported. Open-source DBC databases exist for many vehicles.

## PicoScope Feature Comparison

What PicoScope's $1,000+ automotive software offers that we could replicate:

| PicoScope Feature | Feasibility | Notes |
|---|---|---|
| Guided tests (step-by-step) | High | JSON procedure files |
| Known-good waveform library | High | Reference waveforms as data files |
| Automatic pass/fail analysis | Medium | Pattern matching against references |
| Serial protocol decode | High | Software-only |
| Math channels (A-B, FFT) | Medium | FFT already exists |
| Persistence display | Easy | Don't clear between sweeps |
