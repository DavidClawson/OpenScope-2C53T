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

## Hardware Notes

- **MCU:** GD32F307 Cortex-M4 @ 120MHz with FPU and DSP — plenty of power for DSP, FFT, protocol decode
- **RAM:** 256KB — room for large capture buffers, multiple framebuffers, decode buffers
- **Flash:** 1MB with ~267KB free — space for new code and data
- **DAC:** 12-bit, 2-channel — signal generator output (quantization visible on smooth waveforms)
- **CAN:** 2x controllers built into MCU (currently unused, pin availability TBD)
- **USB:** Full-Speed — streaming mode feasible with decimation or burst capture
- **SPI flash:** External filesystem — store presets, waveform references, vehicle definitions, test procedures
