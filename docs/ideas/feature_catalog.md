# Feature Catalog and Industry Modules

Ideas for custom firmware features and trade-specific applications. The hardware supports most of these as pure software — one firmware, many uses.

*Merged from feature_brainstorm.md and industry_modules.md.*

---

## Core Insight

Most industry-specific tests are variations of the same basic measurements. The firmware provides core functions; trade-specific "modules" are context layers (UI labels, thresholds, guided procedures) on top.

```
Context Layers (JSON procedure files per trade)

  HVAC:          Auto:           Industrial:
  "Compressor    "Compression    "Motor Current
   Current Test"  Test"           Signature Analysis"

Core Functions (same firmware)

  Current waveform capture -> peak detection -> FFT
  Voltage measurement -> logging -> statistics
  Protocol decode -> display overlay
  Signal gen sweep -> response measurement -> Bode plot
  Component test -> I-V curve -> parameter extraction
```

---

## Signal Generator

| Feature | Description | Complexity |
|---|---|---|
| Arbitrary waveform | Load custom waveform shapes from filesystem files | Medium |
| Protocol test patterns | Generate valid I2C, SPI, UART, CAN bitstreams from signal output | Medium-Hard |
| Frequency sweep | Ramp from F1 to F2 over time (for Bode plots) | Easy |
| Modulated signals | AM, FM, PWM modulation on carrier | Medium |
| Noise injection | White/pink noise, or noise added to a signal | Easy |
| Chirp / burst | Time-limited signals: single pulse, N cycles, rising frequency chirp | Easy-Medium |
| Servo tester | Generate standard RC servo PWM (50Hz, 1-2ms adjustable pulse) | Easy |
| S-Bus generation | Inverted UART at 100kbaud, 8E2 — test RC receivers and ESCs | Easy-Medium |

### DAC Architecture Note

The signal generator uses the MCU's built-in 12-bit DAC (2 channels) driven by DMA from a hardware timer. Waveform tables are typically 256-512 points. For digital protocols (PWM, S-Bus, I2C test traffic), the MCU's hardware timer outputs and UART/SPI peripherals are more appropriate than the DAC.

## Oscilloscope Display Modes

| Feature | Description | Complexity |
|---|---|---|
| Persistence / phosphor | Old traces fade instead of clearing. Shows jitter and glitches. | Easy |
| Color-graded persistence | Hot pixels glow bright, rare hits dim. Probability distribution. | Medium |
| XY mode | CH1 as X, CH2 as Y. Lissajous figures, I-V curves, phase measurement. | Easy-Medium |
| Roll mode | Continuous scrolling for slow signals (temperature, battery voltage) | Medium |
| Waveform comparison overlay | Load a reference waveform, overlay on live capture | Easy |

## Oscilloscope Analysis

| Feature | Description | Complexity |
|---|---|---|
| Math channels | A+B, A-B, A*B, FFT. Cortex-M4 DSP makes FFT fast. | Medium |
| Mask / limit testing | Define upper/lower boundary, alert if signal exits. Manufacturing QA. | Medium |
| Rise/fall time | Automatic edge speed measurement | Easy |
| Duty cycle history | Graph duty cycle over time for PWM debugging | Easy-Medium |
| Segmented capture | Capture N separate trigger events, flip through like pages | Medium-Hard |
| Measurement statistics | Std dev, sample count, confidence interval alongside measurements | Easy |

## Oscilloscope Triggering

| Feature | Description | Complexity |
|---|---|---|
| Pulse width trigger | Trigger only on pulses wider/narrower than threshold | Medium (may need FPGA) |
| Runt trigger | Trigger on pulses that don't reach full amplitude | Medium (may need FPGA) |
| Pattern trigger | Trigger on specific combination of CH1 and CH2 states | Medium |
| Protocol trigger | Trigger on specific I2C address, UART byte, or CAN message ID | Hard |

## Cross-Functional (3-in-1 Features)

These leverage the oscilloscope + signal generator + multimeter together:

| Feature | Description | Complexity |
|---|---|---|
| **Component tester** | Signal gen sweeps, scope captures response through component. Plot V vs I. Identifies diodes, transistors, caps, inductors, zener voltages. | Medium |
| **Bode plot** | Signal gen sweeps frequency, scope measures amplitude + phase. Essential for filter design. | Medium-Hard |
| **Impedance measurement** | At a specific frequency, measure V and I through component to calculate Z. | Medium-Hard |
| **Cable length / TDR** | Send pulse, measure reflection time. Speed of light * time / 2 = cable length. | Medium |
| **Power analysis** | Current clamp on CH1, voltage on CH2. Real-time power (V*I), energy, power factor. | Medium |

## Protocol Decoding

| Feature | Description | Complexity |
|---|---|---|
| UART decode | Auto-detect baud, extract bytes, hex/ASCII overlay | Easy-Medium |
| I2C decode | Start/stop detection, address + R/W + data on 2 channels | Medium |
| SPI decode | MOSI/MISO/CLK/CS with byte values | Medium |
| CAN bus decode | From analog waveform: bit-level decode with stuffing, ID + data | Medium-Hard |
| 1-Wire | Dallas temperature sensors, iButton | Easy-Medium |
| Modbus RTU | RS-485 industrial protocol (UART-based) | Easy |
| DMX-512 | Lighting control (UART at 250kbaud) | Easy |
| LIN bus | Automotive (UART-based, 19.2kbaud) | Easy-Medium |
| K-Line (ISO 9141) | Older OBD protocol | Easy |

## Multimeter Upgrades

| Feature | Description | Complexity |
|---|---|---|
| Data logging | Record measurements over time (hours). Graph or export CSV. | Medium |
| Min/Max/Average | Continuously track and display | Easy |
| Relative mode | Zero the display, show deviation | Easy |
| Analog bar graph | Visual bar alongside digital reading | Easy |
| Thermocouple input | K-type temperature via voltage + lookup table | Easy-Medium |
| dB measurement | Decibels relative to reference | Easy |

## Buzzer / Audio Feedback

The buzzer is a passive piezo driven by TIM8 PWM. Full control over frequency and duty cycle.

### Synthesized Tones

| Sound | Use Case |
|---|---|
| Single beep | Auto-hold captured |
| Double beep | Stack push / short-to-reset |
| Rising sweep | Power-on, success |
| Descending tones | Stack full, warning |
| Pitch-proportional tone | Continuity (R to pitch), nulling/tuning |
| Trigger tick | Signal presence confirmation |
| Threshold alarm | Voltage/current limit crossed |

### Audio as Measurement Tool

| Mode | Audio Behavior | Use Case |
|---|---|---|
| Continuity (pitch ~ resistance) | Lower R = higher pitch | Trace wires through harness by ear |
| Null/peak tuning | Pitch rises approaching target | Trim pots, antenna tuning, bridge balancing |
| Threshold alarm | Silent until limit crossed | Monitor a rail while probing elsewhere |
| Auto-hold feedback | Rising tone, firm beep on lock | Eyes-free measurement |

## UX / Software

| Feature | Description | Complexity |
|---|---|---|
| Quick presets | Save/recall complete scope configurations | Easy |
| Annotated screenshots | Add text notes before saving | Medium |
| Undo | History of setting changes, step back | Easy-Medium |
| Auto-label | Recognize signal type, suggest settings | Medium-Hard |
| QR code on screenshots | Embed QR linking to downloadable waveform data | Medium |
| Built-in reference library | Annotated reference images for common signals | Easy |
| Teach mode | Real-time explanations of what the scope is doing | Medium |

---

# Industry Modules

## Module System Architecture

Each module is a JSON file stored on the device filesystem. One firmware, infinite modules — adding a new test is just adding a JSON file.

```
2:/Modules/
  hvac/         compressor_current.json, capacitor_test.json, ...
  auto/         compression_test.json, injector_analysis.json, ...
  audio/        frequency_response.json, speaker_impedance.json, ...
  solar/        iv_curve.json, inverter_thd.json, ...
  marine/       nmea0183_decode.json, nmea2000_decode.json, ...
  ham/          antenna_analyzer.json, harmonic_check.json, ...
  education/    rc_circuit.json, sound_waves.json, ...
```

### JSON Procedure Format

```json
{
  "name": "Compressor Current Analysis",
  "category": "HVAC",
  "version": "1.0",
  "setup": {
    "mode": "oscilloscope",
    "ch1": {"label": "Compressor Current", "probe": "current_clamp", "probe_ratio": "10A/V", "volts_div": "2V"},
    "timebase": "200ms",
    "trigger": {"source": "ch1", "mode": "single", "edge": "rising", "level": "5A"}
  },
  "measurements": [
    {"id": "inrush_peak", "type": "peak_max", "window_ms": [0, 500], "unit": "A", "label": "Locked Rotor Amps"},
    {"id": "running_rms", "type": "rms", "window_ms": [2000, 8000], "unit": "A", "label": "Running Load Amps"}
  ],
  "pass_fail": [
    {"measurement": "lra_ratio", "max": 6.0, "fail_message": "LRA/RLA ratio too high — possible mechanical binding"}
  ],
  "instructions": [
    {"step": 1, "text": "Clamp current probe around compressor COMMON wire"},
    {"step": 2, "text": "Ensure compressor is OFF and thermostat is calling"},
    {"step": 3, "text": "Press RUN, then enable compressor contactor"}
  ],
  "knowledge_base": {
    "normal": "Scroll compressors: LRA 4-5x RLA, start time < 200ms.",
    "high_lra": "Excessive LRA suggests mechanical binding or low voltage."
  }
}
```

### Why This Architecture Wins

1. **One firmware, infinite modules** — no recompilation to add a test
2. **Community contributed** — an HVAC tech writes a module, shares it
3. **Trade-specific language** — "Locked Rotor Amps" not "Peak Current"
4. **Guided procedures** — step-by-step with images, like having a mentor
5. **Pass/fail** — green/red against industry standards
6. **Reference waveforms** — "this is what good looks like"

---

## Automotive

| Test | How it works | Why it matters |
|---|---|---|
| CAN bus waveform analysis | Signal quality, termination, dominant/recessive levels | Easy first step into auto diagnostics |
| OBD-II PID decode | Translate CAN PIDs to human-readable (RPM, temp, speed) | Universal vehicle data |
| Relative compression test | Current clamp starter analysis, cylinder bar graph | Normally a $500+ tool |
| Injector analysis | Pulse width, duty cycle, timing diagram | Common diagnostic procedure |
| Ignition waveform | Primary/secondary analysis, burn time | Classic scope diagnostic |
| Guided diagnostic tests | JSON procedure files with auto-setup and pass/fail | PicoScope-style workflow |

## HVAC / Refrigeration

~400,000 US technicians. Currently buy Fieldpiece, Testo, and Fluke meters ($200-600).

| Test | How it works | Why it matters |
|---|---|---|
| Compressor current analysis | Current clamp, capture startup surge + running current | Reveals bad valves, slugging, bearing wear |
| Capacitor tester | Signal gen excites cap, scope measures response. ESR + leakage. | HVAC capacitors fail constantly |
| ECM/X13 motor analysis | Capture PWM from variable-speed blower motor | Diagnose ECM motor speed issues |
| VFD output quality | Scope the output of mini-split inverter | Bad VFD = motor overheating |
| Flame rectification | Measure uA DC from flame sensor | 1-6uA = flame present, below 1uA = dirty sensor |
| Superheat/subcooling calc | Thermocouple + pressure transducer | The #1 refrigeration measurement |

## Solar / Renewable Energy

| Test | How it works | Why it matters |
|---|---|---|
| Inverter output THD | FFT analysis of grid-tie inverter AC output | High THD = grid code violation |
| MPPT efficiency | CH1 = panel voltage, CH2 = current clamp. Graph power. | Verify MPPT is finding max power |
| I-V curve tracer | Variable electronic load + V/I measurement at each point | Detect cell degradation. Worth $1000+ dedicated. |
| Battery charge profiling | Long-term log of voltage and current over charge cycle | Detect bad cells, charger malfunction |

## Audio / Music

| Test | How it works | Why it matters |
|---|---|---|
| Frequency response (Bode plot) | Signal gen sweeps audio band, scope measures output | See exact frequency response curve |
| Speaker impedance curve | Sweep frequency, measure impedance at each point | Find Fs, Qts, Thiele-Small parameters |
| THD+N measurement | Pure sine through device under test, measure distortion | Standard audio quality metric |
| Tube amp bias meter | Measure cathode/plate current of each output tube | Under/over-biased tubes sound bad and die early |
| Pickup winding analyzer | Signal gen excites guitar pickup, scope measures response | Characterize resonant frequency, Q factor |

## Ham Radio

The 50MHz bandwidth covers all of HF (160m through 10m) and the 6m band — where most homebrew and antenna work happens.

| Feature | Description | Complexity |
|---|---|---|
| Antenna analyzer | SWR sweep with resistive bridge ($2 in parts) | Medium |
| SWR plot | Graph SWR vs frequency to find resonant point | Medium |
| Smith chart display | Plot impedance (R + jX vs frequency) | Medium-Hard |
| Harmonic analysis (FFT) | Check for harmonics violating FCC regs | Medium |
| Two-tone IMD test | Two audio tones into SSB transmitter, view IMD | Medium |
| Crystal characterization | Exact frequency, motional parameters, Q factor | Medium-Hard |
| Filter alignment (Bode plot) | Sweep through bandpass filter, see response in real-time | Medium |

**Why hams would love this:** NanoVNA ($50-100), antenna analyzer ($200+), spectrum analyzer ($300+) — if a $70 scope does all three, that's compelling. Open-source aligns perfectly with ham radio's DIY culture.

## 3D Printing / CNC / Maker

| Test | How it works | Why it matters |
|---|---|---|
| Stepper current analysis | Current clamp on phase wire | Diagnose skipped steps, resonance |
| TMC driver UART decode | Decode traffic to Trinamic drivers | Verify driver configuration |
| Input shaper resonance | Accelerometer tap test | Find resonant frequencies for Klipper |
| Servo/ESC tester | PWM at 50Hz, adjustable 1-2ms pulse | Test servos without Arduino |

## Marine Electronics

| Test | How it works | Why it matters |
|---|---|---|
| NMEA 0183 decode | UART decode of RS-422 (4800/38400 baud) | Standard marine GPS/depth/wind protocol |
| NMEA 2000 decode | CAN bus with marine PGN lookup tables | Modern marine network |
| Stray current detection | AC/DC voltage between hull and water ground | Stray current destroys props and shafts |

## Electric Vehicle

| Test | How it works | Why it matters |
|---|---|---|
| J1772 pilot signal | Decode 1kHz PWM on EV charging plug | Diagnose charging failures |
| CCS/CHAdeMO CAN decode | CAN bus of DC fast charge communication | Debug fast charging negotiation |
| BMS CAN decode | Individual cell voltages, temperatures, SOC | Battery health monitoring |

## Industrial Maintenance

| Test | How it works | Why it matters |
|---|---|---|
| Motor current signature analysis | FFT of motor current reveals mechanical problems | Bearing wear, rotor bars, air gap — without stopping motor. Dedicated MCSA tools cost $2,000-10,000. |
| 4-20mA loop test | Signal gen sources 4-20mA current | Verify pressure/temperature/level transmitters |
| VFD diagnostics | 3-phase output waveform, carrier frequency | Check voltage imbalance, switching noise |
| Power quality | Mains voltage FFT, calculate THD | LED drivers and VFDs inject harmonics |

## Small Engine / Outdoor Power

~100,000+ US repair shops. Most work with just a multimeter.

| Test | How it works | Why it matters |
|---|---|---|
| Ignition coil output | Primary coil waveform during cranking | No-spark, weak spark, intermittent misfire |
| Charging system (stator) | AC output from magneto/alternator stator | Missing phases = shorted windings |
| Kill switch / safety interlock | Verify kill circuit opens/closes cleanly | Blade-brake-clutch, seat switch fail constantly |
| Governor hunting | Log RPM from ignition pulse frequency over time | Diagnose governor surge/hunt |

## Motorcycle / Powersports

| Test | How it works | Why it matters |
|---|---|---|
| Stator AC output | Scope all 3 phases | #1 charging failure on motorcycles |
| CDI ignition timing | Trigger coil + CDI output simultaneously | CDI boxes fail silently |
| TPS sweep | Monitor TPS voltage through full throttle range | Dead spots cause hesitation |

## Appliance Repair / White Goods

~150,000 US techs. Industry shifting to inverter-driven, electronically-controlled appliances that need scope-level diagnostics.

| Test | How it works | Why it matters |
|---|---|---|
| Inverter motor diagnostics | Current clamp on motor phases | Samsung/LG front-loaders use inverter motors |
| Control board relay outputs | Scope relay driver signals | Determine if board or load is bad |
| Door lock motor current | Capture lock actuator current waveform | Partial failures visible only on scope |

## Generator / Backup Power

| Test | How it works | Why it matters |
|---|---|---|
| Output voltage quality | Scope AC output, measure THD via FFT | High THD damages sensitive electronics |
| Frequency stability | Track output frequency under varying load | Governors that hunt show up immediately |
| Transfer switch timing | Scope utility and generator feeds simultaneously | Measure blackout window during transfer |

## Welding

| Test | How it works | Why it matters |
|---|---|---|
| Arc voltage/current waveform | Capture welding arc (with isolation) | Reveals arc stability, spatter tendency |
| Pulse waveform verification | Scope pulse MIG/TIG output | Verify pulse parameters match programmed settings |

## Education

| Feature | How it works |
|---|---|
| Physics lab mode | Guided experiments: sound waves, pendulum timing, RC transients |
| Component identifier | Probe unknown component, auto-identify type and value |
| Waveform math tutorial | Interactive Fourier synthesis demonstration |
| Circuit simulator companion | Match real measurements with textbook predictions |

---

## Target Audiences by Market Size

| Industry | US Workforce | Current Tool Cost | Our Advantage |
|---|---|---|---|
| Automotive | ~750,000 | $200-2,000 scopes | PicoScope features at 1/10 the price |
| HVAC/R | ~400,000 | $200-600 meters | Scope + motor analysis on a meter budget |
| Ham radio | ~750,000 licensees | $50-300 analyzers | Antenna + harmonic + receiver in one |
| Industrial | ~500,000 maintenance techs | $2,000-10,000 MCSA tools | Motor analysis at 1/100 the price |
| Solar | ~250,000 (growing) | $300-1,000 analyzers | I-V curve + inverter analysis |
| Appliance repair | ~150,000 | $200-400 meters | Inverter motor + control board diagnostics |
| Small engine | ~100,000 shops + millions DIY | $100-300 ignition testers | Coil + stator + governor in one tool |
| Audio | ~100,000 pro + millions hobbyist | $200-500 audio analyzers | Bode plot + THD + speaker impedance |
| Education | Millions of students | $200-500 lab scopes | Guided labs + component ID |
