# Project Roadmap

## Completed (This Session)

| Feature | Status | Tests |
|---------|--------|-------|
| Custom firmware boots in Renode through FreeRTOS | Done | Verified |
| LCD driver (ST7789V via EXMC) | Done | Renders in emulator |
| FFT spectrum analyzer (4096-pt, 5 windows) | Done | 19 tests |
| CMSIS-DSP integration (arm_rfft_fast_f32) | Done | Auto-detected |
| FFT averaging, max hold, harmonic labeling | Done | Part of FFT tests |
| Waterfall/spectrogram display | Done | Visual |
| Split view (time + frequency) | Done | Visual |
| DDS signal generator (4 waveforms, sub-Hz) | Done | 25 tests |
| Protocol decoders: UART, I2C, SPI, CAN | Done | 38 + 85 noisy tests |
| K-Line/KWP2000 automotive decoder | Done | 9 tests |
| Math channels (A+B, A-B, A×B, invert) | Done | 5 tests |
| Auto-measurements (freq, Vpp, Vrms, duty) | Done | 18 tests |
| Persistence display (5 decay modes) | Done | 8 tests |
| Config save/load with checksum | Done | 10 tests |
| Screenshot capture (BMP) | Done | 6 tests |
| Shared memory pool (saves 152KB RAM) | Done | Integrated |
| Renode ST7789V peripheral (captures real LCD) | Done | Verified |
| Browser-based emulator UI (no React/npm) | Done | Functional |
| UI refactor (main.c → modular src/ui/) | Done | Builds clean |
| Safety infrastructure (timeout, mutex fs) | Done | Builds clean |
| **Total** | **223 native tests** | **All passing** |

---

## Phase 1: Pre-Device (Firmware Features)

### Priority 1: Quick Wins

**Color Themes / Skins**
- 4 built-in themes: Dark Blue (current), Classic Green (Tektronix), High Contrast White, Night Red
- Per-channel color customization (accessibility for colorblind users)
- Theme stored in config, applied at boot
- Effort: Small — just color constant swaps + config field

**Component Tester Mode**
- Resistor pass/fail: set nominal value + tolerance (e.g., 4.7K ±5%), measure, display PASS/FAIL with big green/red indicator
- Capacitor test: charge/discharge curve → calculate capacitance and ESR
- Diode test: forward voltage measurement with polarity detection
- Continuity: audible buzzer threshold + visual indicator
- Effort: Medium — new measurement mode, UI screen, basic algorithms

**XY Mode / Lissajous**
- Plot CH1 (X) vs CH2 (Y) instead of time-domain
- Phase relationship visualization
- Component curve tracer (apply signal gen to component, measure V-I)
- Effort: Small — just a different rendering mode for existing sample data

### Priority 2: Advanced Scope Features

**Bode Plot**
- Signal gen sweeps frequency logarithmically
- Scope measures amplitude + phase at each frequency
- Automatic gain (dB) and phase (°) plot
- Save/export frequency response data
- Killer feature for EE students and filter designers
- Effort: Medium — needs signal gen + scope coordination, new UI

**Mask / Pass-Fail Testing**
- Record a "known good" waveform as a mask template
- Set upper/lower tolerance bounds
- Real-time comparison: PASS if signal stays within mask, FAIL if it exits
- Production testing use case
- Effort: Medium — mask storage, real-time comparison, UI

**Roll Mode**
- For slow signals (temperature, battery drain, parasitic draw)
- Continuous scrolling display, no trigger needed
- Long capture duration (minutes to hours)
- Effort: Small — different display rendering, no trigger logic

**Trend Plot**
- Graph a measurement (frequency, Vpp, Vrms) over time
- Catch slow drift, intermittent faults
- "Frequency was stable at 1.000kHz for 45 minutes, then jumped to 1.002kHz"
- Effort: Small — measurement + time-series storage

**Segmented Memory**
- Only capture when triggered, skip dead time between events
- Capture rare glitches without filling memory with idle signal
- Effort: Medium — memory management, UI for segment navigation

### Priority 3: Automotive Suite

**Relative Compression Test**
- Current clamp on starter lead captures cranking current
- Input: cylinder count (4/6/8), firing order
- Auto-detect TDC from current waveform peaks
- Label each cylinder, calculate relative compression %
- Bar chart display with PASS/FAIL threshold
- Overlay/average multiple crank cycles (uses persistence engine)
- Heat map mode showing cycle-to-cycle variation
- THIS IS THE VIRAL FEATURE for automotive YouTube
- Effort: Large — needs cylinder detection algorithm, custom UI, persistence integration

**Injector Pulse Width Analysis**
- Capture injector drive waveform
- Measure pulse width per cylinder, display as overlay
- Compare duty cycle across cylinders
- Detect stuck/lazy injectors
- Effort: Medium

**Ignition Coil Analysis**
- Primary coil dwell time measurement
- Spark duration and burn voltage (needs HV probe)
- Cylinder-to-cylinder comparison
- Effort: Medium

**Alternator Ripple Test**
- Measure AC ripple on battery with engine running
- Normal: ~50mV ripple. Bad diode: >300mV with missing phase
- Auto-detect "X of 6 diodes working"
- Effort: Small — FFT of battery voltage, pattern matching

**Battery Cranking Analysis**
- Cranking voltage drop profile
- Estimate CCA from voltage sag curve
- Compare to battery rating
- Effort: Small — voltage measurement + analysis

**Parasitic Draw Hunt**
- Roll mode with current clamp on battery negative
- Long-duration recording (30+ minutes) to catch modules waking up
- Mark events when current spikes
- "Module wake at T+12:34, drew 2.3A for 150ms"
- Effort: Medium — roll mode + event detection

### Priority 4: Specialized Applications

**Audio Analysis**
- THD+N measurement (Total Harmonic Distortion + Noise)
- Speaker impedance curve (frequency sweep + impedance calc)
- Audio frequency response (Bode plot in audio range)
- Effort: Medium — builds on FFT + signal gen

**Ham Radio**
- Harmonic analysis for FCC compliance (already done via FFT!)
- SWR measurement (with appropriate coupler)
- CW decoder (already have UART decoder as base)
- Splatter analysis for transmitter testing
- Effort: Small — mostly UI for existing FFT capabilities

**HVAC/Solar**
- Motor start capacitor test (ESR + capacitance from discharge curve)
- Compressor current signature analysis (uses motor current analysis)
- Solar inverter THD measurement
- Effort: Small-Medium

**3D Printing / CNC**
- Stepper motor resonance detection (FFT of motor current)
- TMC driver SPI decode (already have SPI decoder)
- PWM duty cycle measurement for heaters
- Endstop signal verification
- Effort: Small — existing decoders + measurement tools

**Industrial**
- Motor Current Signature Analysis (MCSA) — FFT of motor current for bearing/rotor fault detection
- 4-20mA loop testing
- Relay contact bounce measurement
- PLC signal verification
- Effort: Medium

### Priority 5: Connectivity & Data

**USB Streaming to PC**
- CDC virtual serial port mode
- Stream raw samples to PC for sigrok/PulseView
- Turns $70 device into PC-connected instrument
- Effort: Large — USB CDC driver, protocol design

**CSV/Data Export**
- Save waveform data to SPI flash as CSV
- Export via USB mass storage mode
- Effort: Small

**Waveform Reference Library**
- Store "known good" reference waveforms on SPI flash
- Overlay for comparison: "show me what a good ignition coil looks like"
- Community-contributed waveform database
- Effort: Medium — storage format, UI for browse/overlay

---

## Phase 2: With Device (When It Arrives ~Early April 2026)

### Step 1: Hardware Teardown + SWD Connection
- Open device, photograph PCB
- Confirm GD32F307 + Gowin GW1N-UV2 FPGA
- Solder Dupont wires to SWD port (SWDIO + SWCLK + GND)
- Connect ST-Link V2 or J-Link via USB
- Optionally connect UART TX/RX for serial debug
- Full flash dump via SWD (1MB MCU flash + 16MB SPI flash)
- **Success: can read/write flash, single-step firmware**

### Step 2: FPGA Protocol Capture
- Logic analyzer or second scope on USART2 TX/RX
- Capture full boot sequence + mode changes
- Decode using known 10-byte frame format
- Cross-reference with decompiled fpga task code
- **Output: complete FPGA command reference**

### Step 3: Flash Custom Firmware
- Backup original flash via SWD
- Flash our firmware (with all 223-test-verified features)
- Verify LCD displays our UI
- Test buttons, mode switching, FFT, signal gen
- Flash back to stock to confirm reversibility
- **This is the "it actually works" moment**

### Step 4: Real Signal Acquisition
- Understand FPGA sample data format from Step 2
- Implement: configure timebase → FPGA command → read samples → display
- Real waveform on real LCD from real probe
- **The scope becomes functional**

### Step 5: Hardware-in-the-Loop Testing
- SWD flash + UART serial = fast iteration loop
- Claude writes code → flash → run → serial output → iterate
- Test all protocol decoders with real signals
- K-Line test on the 4Runner!

---

## Top 10 Priority Features (Next to Implement)

1. **Color themes** — quick win, everyone notices
2. **Component tester** (resistor pass/fail) — unique differentiator
3. **XY mode / Lissajous** — expected scope feature, easy to add
4. **Roll mode** — essential for slow signals
5. **Bode plot** — killer feature for EE students
6. **Relative compression test** — viral automotive feature
7. **Alternator ripple test** — easy automotive win
8. **Mask testing** — production/QA use case
9. **Trend plot** — catch intermittent faults
10. **USB streaming** — turns scope into PC instrument

---

## Phase 3: ESP32 WiFi/Co-Processor Mod ("$3 Upgrade")

A hardware mod that adds WiFi, BLE, and a co-processor to the scope.
Total cost: ~$4 in parts. Difficulty: 4 solder joints, 20 minutes.

### The Mod

**Part:** ESP32-S3 Mini module (~$4) or ESP32-C3 Super Mini (~$3)
- 12mm × 18mm — fits inside the case near battery compartment
- Dual-core 240MHz (S3) or single-core 160MHz (C3)
- WiFi 802.11 b/g/n + BLE 5.0
- 512KB SRAM (S3) or 400KB (C3)
- Onboard PCB antenna (works through plastic case)

**Solder Points (4 wires):**
1. 3.3V — tap from existing voltage regulator
2. GND — any ground pad
3. UART TX — unused GPIO on GD32F307 (identified during teardown)
4. UART RX — corresponding UART pin

**Power:** ESP32 draws ~130mA when WiFi active, ~5µA in sleep.
Menu toggle enables/disables WiFi to preserve battery life.

### What It Enables

**Phone as Display (biggest win)**
- ESP32 serves a web page over WiFi
- Phone/tablet connects — instant 1080p scope display
- Pinch-to-zoom on waveforms, multi-pane layout
- No app install — works in any browser on any device
- See scope screen while working under the car hood

**Remote Control**
- Change scope settings from phone touchscreen
- Useful when probes are in tight spots
- Start/stop capture, change timebase, trigger level

**Data Logging + Cloud**
- Stream measurements to InfluxDB/Grafana
- Long-term monitoring (battery drain, temperature, solar output)
- "Log alternator voltage every 5 seconds for the next hour"

**Waveform Sharing**
- Capture a waveform, upload to community database
- "Here's what a good Toyota 5VZ-FE injector looks like"
- Compare your capture against known-good references

**OTA Firmware Updates**
- Update scope firmware over WiFi instead of USB cable
- Download and flash from GitHub releases automatically

**Advanced Co-Processing (ESP32-S3)**
- Offload heavy analysis to the ESP32's faster CPU
- Real-time machine learning (anomaly detection)
- Continuous recording to microSD card (via ESP32 SPI)
- Deep protocol analysis that would choke the main MCU

### Architecture

```
┌──────────────────┐         ┌──────────────────┐
│ GD32F307 (Main)  │  UART   │ ESP32-S3 (WiFi)  │
│ • Scope control  │ or SPI  │ • Web server     │
│ • FPGA interface │◄───────►│ • Phone display  │
│ • LCD rendering  │ 4 wires │ • Data logging   │
│ • Button input   │         │ • BLE beacon     │
│ • Basic DSP      │         │ • OTA updates    │
│ 120MHz, 256KB    │         │ 240MHz, 512KB    │
└────────┬─────────┘         └────────┬─────────┘
         │                            │ WiFi/BLE
    ┌────▼────┐                  ┌────▼────────┐
    │  FPGA   │                  │ Phone/tablet │
    │  ADC    │                  │ 1080p touch  │
    │ 250MS/s │                  │ any browser  │
    └─────────┘                  └──────────────┘
```

### Implementation Steps

1. **During teardown:** Identify available UART pins on GD32 PCB
2. **ESP32 firmware:** Arduino/ESP-IDF project — serial bridge + web server
3. **Scope firmware:** UART driver module for ESP32 communication protocol
4. **Web UI:** Single-page HTML with WebSocket (reuse emulator UI pattern)
5. **Physical mod:** Solder ESP32 module inside case, route antenna toward plastic panel
6. **Mod guide:** Step-by-step photos + video for community

### Why ESP32 and Not a Teensy/Other

| | ESP32-S3 | Teensy 4.1 | RPi Pico W |
|---|---------|-----------|-----------|
| Cost | $4 | $32 | $8 |
| WiFi/BLE | Built-in | No | WiFi only |
| Size | 12×18mm | 18×61mm | 21×51mm |
| Clock | 240MHz | 600MHz | 133MHz |
| RAM | 512KB+8MB | 1MB | 264KB |
| Power | 130mA | 100mA | 40mA |

ESP32-S3 is the sweet spot: WiFi+BLE built-in, small enough to fit, powerful enough to serve as co-processor, cheap enough that the mod is accessible.

---

## FPGA Notes

The FPGA is now identified as **Gowin GW1N-UV2** (confirmed by EEVblog community). Open-source toolchain exists:
- **Yosys** for synthesis
- **nextpnr-gowin** for place-and-route
- **Apicula** for bitstream generation

This opens the possibility of custom FPGA firmware — higher sample rates, custom trigger logic, protocol-aware capture, hardware acceleration. This is a Phase 3+ goal but dramatically expands what the device can do.
