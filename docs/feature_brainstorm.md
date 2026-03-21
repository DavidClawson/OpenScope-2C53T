# Feature Brainstorm

Ideas for custom firmware features, organized by subsystem. The hardware already supports most of these — they're pure software.

## Signal Generator

| Feature | Description | Complexity |
|---|---|---|
| Arbitrary waveform | Load custom waveform shapes from filesystem files | Medium |
| Protocol test patterns | Generate valid I2C, SPI, UART, CAN bitstreams from the signal output | Medium-Hard |
| Frequency sweep | Ramp from F1 to F2 over time (for Bode plots) | Easy |
| Modulated signals | AM, FM, PWM modulation on carrier | Medium |
| Noise injection | White/pink noise, or noise added to a signal | Easy |
| Chirp / burst | Time-limited signals: single pulse, N cycles, rising frequency chirp | Easy-Medium |
| Servo tester | Generate standard RC servo PWM (50Hz, 1-2ms adjustable pulse) | Easy |
| DDS improvements | Better output quality if FPGA has a DDS path (vs MCU DAC) | Unknown (needs FPGA RE) |

## Oscilloscope Display Modes

| Feature | Description | Complexity |
|---|---|---|
| Persistence / phosphor | Don't clear between sweeps, old traces fade. Shows jitter and glitches. $5000+ scope feature. | Easy — just dim framebuffer pixels instead of clearing |
| Color-graded persistence | Hot pixels (frequently hit) glow bright, rare hits dim. Probability distribution display. | Medium |
| XY mode | CH1 as X, CH2 as Y. Lissajous figures, I-V curves, phase measurement. | Easy-Medium |
| Roll mode | Continuous scrolling display for slow signals (temperature, battery voltage) | Medium |
| Waveform comparison overlay | Load a reference waveform, overlay on live capture. Differences jump out. | Easy |

## Oscilloscope Analysis

| Feature | Description | Complexity |
|---|---|---|
| Math channels | A+B, A-B, A×B, FFT. Cortex-M4 DSP instructions make FFT fast. | Medium |
| Mask / limit testing | Define upper/lower boundary, alert if signal goes outside. Manufacturing QA. | Medium |
| Rise/fall time | Automatic edge speed measurement | Easy |
| Duty cycle history | Graph duty cycle over time for PWM debugging | Easy-Medium |
| Segmented capture | Capture N separate trigger events, flip through like pages. Catches intermittent problems. | Medium-Hard |
| Measurement statistics | Standard deviation, sample count, confidence interval alongside measurements | Easy |

## Oscilloscope Triggering

| Feature | Description | Complexity |
|---|---|---|
| Pulse width trigger | Trigger only on pulses wider/narrower than threshold. Catches glitches. | Medium (may need FPGA support) |
| Runt trigger | Trigger on pulses that don't reach full amplitude. Finds logic level issues. | Medium (may need FPGA support) |
| Pattern trigger | Trigger on specific combination of CH1 and CH2 states | Medium |
| Protocol trigger | Trigger on specific I2C address, UART byte, or CAN message ID | Hard (software decode + trigger) |

## Multimeter Upgrades

| Feature | Description | Complexity |
|---|---|---|
| Data logging | Record measurements over time (hours). Graph on screen or export CSV. | Medium |
| Min/Max/Average | Continuously track and display min, max, average | Easy |
| Relative mode | Zero the display, show deviation from that point (±12mV) | Easy |
| Analog bar graph | Visual bar alongside digital reading, shows trends at a glance | Easy |
| Thermocouple input | Read K-type thermocouple temperature. Voltage + lookup table. | Easy-Medium |
| dB measurement | Show measurement in decibels relative to reference. Audio work. | Easy |

## Cross-Functional (Using Multiple Subsystems)

These leverage the 3-in-1 design — features no single-function instrument can do:

| Feature | Description | Complexity |
|---|---|---|
| **Component tester** | Signal gen outputs sweep, scope captures response through component. Plot V vs I. Identifies diodes, transistors, caps, inductors, zener voltages. Dedicated testers cost $200+. | Medium |
| **Bode plot** | Signal gen sweeps frequency, scope measures amplitude + phase. Plot magnitude/phase vs frequency. Essential for filter design. PicoScope charges extra for this. | Medium-Hard |
| **Impedance measurement** | At a specific frequency, measure V and I through component to calculate Z. Sweep for Z vs frequency (antenna tuning, crystal characterization). | Medium-Hard |
| **Cable length / TDR** | Send pulse from signal gen, measure reflection time on scope. Speed of light × time / 2 = cable length. Finds opens, shorts, impedance mismatches. | Medium |
| **Power analysis** | Current clamp on CH1, voltage on CH2. Calculate real-time power (V×I), energy over time, power factor. | Medium |
| **Servo tester** | PWM generation at servo frequencies with adjustable pulse width via buttons | Easy |

## Protocol Decoding

| Feature | Description | Complexity |
|---|---|---|
| UART decode | Auto-detect baud, extract bytes, show hex/ASCII overlay | Easy-Medium |
| I2C decode | Start/stop detection, address + R/W + data overlay on 2 channels | Medium |
| SPI decode | MOSI/MISO/CLK/CS with byte values on 2 channels | Medium |
| CAN bus decode | From analog waveform: bit-level decode with stuffing, ID + data | Medium-Hard |
| CAN native | Using MCU's built-in CAN controller (if pins accessible) | Easy (if wired) |
| 1-Wire | Dallas temperature sensors, iButton | Easy-Medium |
| Modbus RTU | RS-485 industrial protocol (UART-based) | Easy |
| DMX-512 | Lighting control (UART at 250kbaud) | Easy |
| LIN bus | Automotive (UART-based, 19.2kbaud) | Easy-Medium |
| K-Line (ISO 9141) | Older OBD protocol | Easy |

## Automotive

| Feature | Description | Complexity |
|---|---|---|
| CAN bus waveform analysis | Signal quality, termination, dominant/recessive levels | Easy |
| OBD-II PID decode | Translate CAN PIDs to human-readable (RPM, temp, speed) | Medium |
| Relative compression test | Current clamp starter analysis, cylinder bar graph | Medium |
| Injector analysis | Pulse width, duty cycle, timing diagram | Medium |
| Ignition waveform | Primary/secondary analysis, burn time | Medium |
| Guided diagnostic tests | JSON procedure files with auto-setup and pass/fail | Medium |
| Known-good waveform library | Reference waveforms for comparison | Easy (data contribution) |
| Vehicle definition files | DBC/JSON files mapping CAN IDs to signal names per vehicle | Easy (data contribution) |

## UX / Software

| Feature | Description | Complexity |
|---|---|---|
| Quick presets | Save/recall complete scope configurations with one button | Easy |
| Custom color themes | Dark, light (printouts), high-contrast (outdoor), colorblind-friendly | Easy |
| Annotated screenshots | Add text notes to screenshots before saving | Medium |
| Undo | Keep history of setting changes, let user step back | Easy-Medium |
| Auto-label | Recognize signal type and suggest appropriate settings | Medium-Hard |

## "Why Doesn't Every Scope Do This?"

| Feature | Description | Why it's cool |
|---|---|---|
| QR code on screenshots | Embed QR linking to downloadable waveform data | Share screenshot on forum, anyone can get the actual data |
| Built-in reference library | Annotated reference images: "what should a healthy CAN bus look like?" | Like a textbook built into the instrument |
| Teach mode | Annotate what the scope is doing in real-time with explanations | Perfect for students and beginners |

## Viral Potential (Features That Would Get Attention)

1. **Persistence display** — looks incredible, normally a $5000+ scope feature
2. **Component tester** — everyone wants one, dedicated units cost $200+
3. **CAN bus decode** — automotive community is huge and passionate
4. **Bode plot** — EE students and filter designers would love this
5. **Protocol test pattern generator** — "generate I2C traffic from a $70 scope"

## Ham Radio

The 50MHz bandwidth covers all of HF (160m through 10m) and the 6m band — where most homebrew, kit building, and antenna work happens. VHF/UHF (2m, 70cm) is out of range.

### Antenna & Feedline

| Feature | Description | Complexity |
|---|---|---|
| **Antenna analyzer** | Sweep frequency across a band, measure SWR and impedance. Needs a simple resistive bridge ($2 in parts) between signal gen and scope input. | Medium |
| **SWR plot** | Graph SWR vs frequency to find antenna resonant point | Medium |
| **Smith chart display** | Plot impedance on a Smith chart (R + jX vs frequency) | Medium-Hard |
| **Cable TDR** | Find opens, shorts, impedance discontinuities in coax | Medium |
| **Coax loss measurement** | Measure feedline attenuation at a specific frequency | Easy |
| **Velocity factor** | Measure actual velocity factor of coax with known length | Easy-Medium |

### Transmitter Testing

| Feature | Description | Complexity |
|---|---|---|
| **Harmonic analysis (FFT)** | Check for harmonics violating FCC regs. Fundamental at 14MHz, spurs at 28MHz, 42MHz? | Medium (FFT already partially exists) |
| **Two-tone IMD test** | Two audio tones into SSB transmitter, view IMD on scope. Key quality metric. | Medium |
| **AM modulation depth** | View AM envelope, calculate modulation percentage | Easy-Medium |
| **Transmit power** | Dummy load + voltage measurement = power calculation | Easy |
| **Key click analysis** | View CW keying waveform edges — clean or splattering? | Easy |
| **Splatter analysis** | FFT of transmitted signal to check spurious emissions | Medium |

### Filter & Crystal Work

| Feature | Description | Complexity |
|---|---|---|
| **Crystal characterization** | Measure crystal's exact frequency, motional parameters, Q factor | Medium-Hard |
| **Filter alignment (Bode plot)** | Sweep through bandpass filter, see passband, rolloff, rejection in real-time | Medium |
| **Crystal matching** | Compare crystals to find matched sets for ladder filters. Auto-sweep each one. | Medium |
| **IF strip alignment** | Sweep IF passband of a receiver, see the response curve | Medium |

### Receiver Testing

| Feature | Description | Complexity |
|---|---|---|
| **Sensitivity measurement** | Calibrated signal from gen, measure receiver's MDS | Medium |
| **Selectivity** | Sweep across receiver passband, plot response curve | Medium |
| **AGC behavior** | Step signal level, watch AGC response over time | Easy-Medium |
| **Oscillator stability** | Monitor VFO frequency over time, measure drift | Medium |

### "Ham Shack Essential" Module

A packaged ham radio module would include:
```
Ham Radio Module
├── Antenna Analyzer
│   ├── SWR sweep (1-50MHz, any band)
│   ├── Impedance plot (R + jX)
│   ├── Smith chart display
│   └── Resonant frequency finder
├── Transmitter Test
│   ├── Harmonic analysis (FFT)
│   ├── Two-tone IMD test
│   ├── Power measurement
│   └── Modulation analysis (AM/SSB)
├── Filter / Crystal
│   ├── Bode plot (auto-sweep)
│   ├── Crystal parameter measurement
│   └── Crystal matching for ladder filters
└── Cable / Feedline
    ├── TDR (length + fault finding)
    ├── Loss measurement
    └── Velocity factor
```

**Why hams would love this:**
- NanoVNA ($50-100), antenna analyzer ($200+), spectrum analyzer ($300+) — if a $70 scope does all three, that's compelling
- HF homebrew community is huge and loves affordable tools
- Open-source aligns perfectly with ham radio's DIY culture
- QRP (low power) community is very active and technical
- Built-in distribution via ARRL, ham forums, YouTube ham channels

**Limitation:** 50MHz bandwidth means no VHF/UHF (2m, 70cm). But HF is where most homebrewing happens.

## ESP32 WiFi Mod + Module Platform

### The $4 Hardware Mod

Solder an ESP32-S3 mini module (12×18mm, ~$4) inside the case with 4-5 wires:
- 3.3V, GND, UART TX, UART RX (4 wires = basic WiFi/BLE)
- BOOT pin (5th wire = enables OTA firmware updates)

This single mod enables everything below.

### OTA Firmware Updates

The GD32F307 has a built-in UART bootloader. The ESP32 can act as a wireless flash programmer:

1. Phone browser shows "Firmware v2.3 available" notification
2. User taps "Install"
3. ESP32 downloads binary from GitHub over WiFi
4. ESP32 asserts GD32 BOOT pin, pulses RESET
5. ESP32 streams firmware over UART using STM32/GD32 bootloader protocol
6. ESP32 releases BOOT, pulses RESET → scope boots with new firmware

No USB cable, no computer. Update your scope from under the car.

### Module / Plugin System

The 16MB SPI flash is partitioned into module slots. Modules are downloadable packages containing any combination of:

**Module contents:**
- **Procedure scripts (JSON):** Step-by-step guided tests with prompts, measurements, pass/fail criteria
- **DTC databases:** Diagnostic trouble code → description → common causes → fix procedures
- **Reference waveforms:** "Known good" patterns for overlay comparison
- **Measurement profiles:** What to measure, thresholds, display layout
- **Code overlays (advanced):** Compiled ARM code loaded into RAM overlay region and executed

**SPI Flash layout:**
```
0x000000  System UI assets       (1MB)
0x100000  Module slot 1          (1MB)  ← "Toyota Diagnostics"
0x200000  Module slot 2          (1MB)  ← "HVAC Analyzer"
0x300000  Module slot 3          (1MB)  ← "Audio Test Suite"
0x400000  Module slot 4          (1MB)  ← "Ham Radio Tools"
0x500000  Waveform library       (2MB)  ← community reference waveforms
0x700000  DTC databases          (2MB)  ← Toyota, Honda, GM, Ford...
0x900000  Screenshots            (1MB)
0xA00000  User data              (6MB)
```

### Module Store (via ESP32 WiFi)

The ESP32 serves a web page with a module browser:

```
Phone browser → "OpenScope Module Store"

  📦 Toyota Diagnostic Suite v2.1        [Install]
     K-Line decoder, 847 DTCs, 23 sensor PIDs,
     reference waveforms for 3.4L 5VZ-FE
     By: community/toyota-osc

  📦 Fuel System Analyzer v1.3           [Install]
     Fuel pump current profile, injector
     balance test, fuel pressure trending
     By: community/fuel-diag

  📦 HVAC Technician Pack v1.0           [Installed ✓]
     Capacitor test, compressor current,
     refrigerant pressure curves
     By: community/hvac-tools

  📦 Community: "4Runner Ignition Pack"  [Install]
     Uploaded by @david_clawson
     Reference coil waveforms, timing specs
```

### JSON Procedure Format (concept)

```json
{
  "module": "fuel_pump_analyzer",
  "version": "1.3",
  "name": "Fuel Pump Current Test",
  "description": "Analyze fuel pump health via starter current profile",
  "author": "openscope-community",
  "procedures": [
    {
      "id": "fuel_pump_prime",
      "name": "Fuel Pump Prime Test",
      "steps": [
        {
          "type": "prompt",
          "text": "Connect current clamp around fuel pump feed wire (B+ to pump)",
          "image": "fuel_pump_clamp.ref"
        },
        {
          "type": "configure",
          "timebase": "500ms",
          "ch1_vdiv": "5A",
          "trigger": "rising",
          "trigger_level": "0.5A"
        },
        {
          "type": "prompt",
          "text": "Turn key to ON (do not start). Pump should run for ~2 seconds."
        },
        {
          "type": "capture",
          "duration_ms": 5000,
          "channel": "ch1"
        },
        {
          "type": "measure",
          "measurements": ["vpp", "frequency", "vrms"],
          "reference": "fuel_pump_prime_good.ref"
        },
        {
          "type": "evaluate",
          "rules": [
            {"measurement": "vrms", "min": 3.0, "max": 8.0, "unit": "A", "label": "Pump current"},
            {"measurement": "vpp", "max": 2.0, "unit": "A", "label": "Current ripple"}
          ],
          "pass_message": "Fuel pump prime current normal",
          "fail_message": "Abnormal pump current — check pump, relay, or wiring"
        }
      ]
    },
    {
      "id": "injector_balance",
      "name": "Injector Balance Test",
      "steps": [
        {
          "type": "prompt",
          "text": "Move current clamp to injector harness common feed"
        },
        {
          "type": "configure",
          "timebase": "5ms",
          "ch1_vdiv": "1A",
          "trigger": "rising",
          "trigger_level": "0.2A"
        },
        {
          "type": "capture",
          "duration_ms": 200,
          "repeat": 10,
          "average": true
        },
        {
          "type": "analyze",
          "algorithm": "peak_detect",
          "params": {"min_peaks": 4, "max_peaks": 8},
          "display": "bar_chart",
          "label_format": "Cyl %d: %.1f%%"
        }
      ]
    }
  ],
  "references": {
    "fuel_pump_prime_good.ref": "waveforms/fuel_pump_prime_good.bin",
    "fuel_pump_clamp.ref": "images/fuel_pump_clamp.bin"
  },
  "dtc_database": "dtc/generic_fuel_system.json"
}
```

### Automotive Sensor Add-Ons

Community-designed sensor modules that connect to the scope's probe inputs:

**Fuel pressure transducer** — 0-100 PSI, 0.5-4.5V output. Connect to CH1, module provides pressure-to-voltage calibration and reference curves.

**Current clamp adapter** — Hall-effect current sensor (e.g., ACS712). 0-30A range for starter motor, fuel pump, alternator. Module provides amp-per-volt scaling.

**Exhaust gas temperature** — K-type thermocouple with MAX6675 module. Connect via the signal gen output (repurposed as SPI CS). Module reads temperature, overlays on scope display.

**MAP sensor adapter** — Manifold absolute pressure from a standalone MAP sensor. Module provides kPa-to-voltage calibration and altitude correction.

### Phone as Primary Display

With ESP32 WiFi, the phone becomes the better display:
- 1080p+ resolution vs 320x240
- Multi-touch pinch-to-zoom on waveforms
- Multi-pane layout: scope + FFT + measurements + decoded protocol all visible
- Screenshot and share directly from phone
- Voice notes attached to captures
- No app needed — pure browser-based via WebSocket

### Community Platform Vision

```
GitHub: openscope-modules/
├── modules/
│   ├── toyota-diagnostics/     ← maintained by community
│   ├── honda-diagnostics/
│   ├── hvac-tools/
│   ├── audio-analyzer/
│   ├── ham-radio/
│   └── education-kit/
├── waveforms/                   ← reference waveform library
│   ├── automotive/
│   ├── power-supplies/
│   └── audio/
├── dtc-databases/               ← diagnostic trouble codes
│   ├── obd2-generic.json
│   ├── toyota-specific.json
│   └── honda-specific.json
└── module-spec.md               ← module format specification
```

Anyone can contribute a module. The ESP32 module store pulls from this repo. Scope downloads what it needs. Community grows the platform.

## Hardware Notes

- **MCU:** GD32F307 Cortex-M4 @ 120MHz with FPU and DSP — plenty of power for DSP, FFT, protocol decode
- **RAM:** 256KB — room for large capture buffers, multiple framebuffers, decode buffers
- **Flash:** 1MB with ~267KB free — space for new code and data
- **DAC:** 12-bit, 2-channel — signal generator output (quantization visible on smooth waveforms)
- **CAN:** 2x controllers built into MCU (currently unused, pin availability TBD)
- **USB:** Full-Speed — streaming mode feasible with decimation or burst capture
- **SPI flash:** External filesystem — store presets, waveform references, vehicle definitions, test procedures
