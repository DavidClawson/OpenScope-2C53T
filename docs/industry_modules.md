# Industry-Specific Modules

The same core scope/multimeter/signal generator hardware serves many different trades and industries. Each "module" is primarily a **context layer** — trade-specific UI labels, measurement interpretations, pass/fail thresholds, and guided test procedures — on top of the same underlying scope functions.

## Core Insight

Most industry tests are variations of the same basic measurements:

```
┌───────────────────────────────────────────────────────┐
│  Context Layers (JSON procedure files per trade)       │
│                                                        │
│  HVAC:          Auto:           Industrial:            │
│  "Compressor    "Compression    "Motor Current         │
│   Current Test"  Test"           Signature Analysis"   │
│                                                        │
├───────────────────────────────────────────────────────┤
│  Core Functions (same firmware)                        │
│                                                        │
│  Current waveform capture → peak detection → FFT       │
│  Voltage measurement → logging → statistics            │
│  Protocol decode → display overlay                     │
│  Signal gen sweep → response measurement → Bode plot   │
│  Component test → I-V curve → parameter extraction     │
└───────────────────────────────────────────────────────┘
```

## HVAC / Refrigeration

~400,000 technicians in the US. Moving rapidly from analog to digital/electronic systems. Currently buy Fieldpiece, Testo, and Fluke meters ($200-600).

### Tests

| Test | How it works | Why it matters |
|---|---|---|
| **Compressor current analysis** | Current clamp on common wire, capture startup surge + running current | Current signature reveals bad valves, slugging, liquid flooding, bearing wear. Same concept as automotive compression test. |
| **Capacitor tester** | Signal gen excites capacitor, scope measures response. Calculate capacitance, ESR, leakage under voltage | HVAC capacitors fail constantly. ESR and leakage are better indicators than capacitance alone. |
| **ECM/X13 motor analysis** | Capture PWM waveform from variable-speed blower motor control board | Modern furnaces use ECM motors. Verify control board output, diagnose motor speed issues. |
| **VFD output quality** | Scope the output of mini-split or variable-speed compressor inverter | Bad VFD output = motor overheating, short life. Check for voltage imbalance, carrier frequency issues. |
| **Flame rectification** | Measure µA DC signal from flame sensor to ground | Flame rods produce 1-6µA DC when flame is present. Below 1µA = dirty sensor or cracked insulator. Pass/fail with thresholds. |
| **Contactor analysis** | Current waveform through contactor coil during pull-in | Detects weak coils, contact bounce, voltage drop issues |
| **Thermostat wire decode** | Decode serial communication on communicating thermostat wires | Some brands (Carrier Infinity, Trane ComfortLink) use proprietary serial protocols between thermostat and equipment |
| **Superheat/subcooling calc** | Thermocouple temperature + pressure transducer → calculate superheat or subcooling | The #1 refrigeration measurement. Built-in calculator with target ranges per refrigerant type. |

### Example Guided Test

```json
{
  "module": "HVAC - Compressor Current",
  "setup": {
    "ch1": {"probe": "Current clamp on compressor COMMON wire", "scale": "10A/div"},
    "timebase": "200ms/div",
    "trigger": {"source": "ch1", "level": "5A", "edge": "rising"}
  },
  "measurements": ["inrush_peak", "running_rms", "lra_ratio", "start_time_ms"],
  "pass_fail": {
    "lra_ratio": {"max": 6.0, "label": "LRA/RLA ratio should be < 6:1"},
    "running_rms": {"max_pct_of_nameplate": 110, "label": "Running amps < 110% nameplate RLA"},
    "start_time_ms": {"max": 500, "label": "Start time should be < 500ms"}
  },
  "instructions": [
    "Clamp current probe around compressor COMMON wire",
    "Ensure compressor is OFF and thermostat is calling",
    "Press RUN, then enable compressor contactor",
    "Wait for 10 seconds of stable running data",
    "Results show inrush peak, running current, and start characteristics"
  ]
}
```

## Solar / Renewable Energy

Growing rapidly. Installers and maintenance techs need tools for commissioning and troubleshooting.

| Test | How it works | Why it matters |
|---|---|---|
| **Inverter output THD** | FFT analysis of grid-tie inverter AC output | High THD means inverter failure or grid code violation. Utility may disconnect. |
| **MPPT efficiency** | CH1 = panel voltage, CH2 = current (clamp). Calculate and graph power over time. | Verify the MPPT controller is finding the maximum power point. |
| **I-V curve tracer** | Signal gen as variable electronic load + scope measures panel V and I at each load point | Maps the panel's full I-V curve. Detect cell degradation, cracking, shading issues. Worth $1000+ as a dedicated tool. |
| **Battery charge profiling** | Long-term log of voltage and current over a full charge cycle | Detect bad cells, charger malfunction, incorrect charge profile. |
| **String voltage verification** | Multimeter mode with logging. Measure each string voltage over a day. | Find underperforming strings (shading, bypass diode failures). |
| **Ground fault detection** | Measure leakage current on PV conductors to ground | Ground faults are a fire risk. Code requires testing. |

## Audio / Music

Guitar players, audio engineers, hi-fi hobbyists, speaker builders. Large community that already buys specialized audio test tools.

| Test | How it works | Why it matters |
|---|---|---|
| **Frequency response (Bode plot)** | Signal gen sweeps through audio band, scope measures output of amp/preamp/pedal | See the exact frequency response curve. Find roll-off points, resonances. |
| **Speaker impedance curve** | Sweep frequency through speaker, measure impedance at each point | Find Fs (resonant frequency), Qts, and other Thiele-Small parameters for cabinet design. A dedicated tool costs $200+. |
| **THD+N measurement** | Pure sine from signal gen → device under test → scope measures distortion + noise | The standard audio quality metric. Compare amps, DACs, preamps objectively. |
| **Tube amp bias meter** | Measure cathode/plate current of each output tube | Display current with target range for the tube type. Under/over-biased tubes sound bad and die early. |
| **Pickup winding analyzer** | Signal gen excites guitar pickup, scope measures response | Characterize resonant frequency, inductance, Q factor, output level. Compare pickups objectively. |
| **Cable tester** | Continuity + capacitance per foot | High-capacitance cables roll off treble. Quantify cable quality. |
| **Crossover network analysis** | Sweep frequency through speaker crossover, measure output at each driver | Verify crossover points, slopes, driver phasing |

## 3D Printing / CNC / Maker

Large maker community that's increasingly technical about motion systems and electronics.

| Test | How it works | Why it matters |
|---|---|---|
| **Stepper current analysis** | Current clamp on stepper motor phase wire, capture waveform | Diagnose skipped steps, over-current, resonance. Verify microstepping is working. |
| **TMC driver UART decode** | Decode UART traffic to Trinamic stepper drivers (TMC2209, TMC5160) | Verify driver configuration (current, microstepping, stealthChop vs spreadCycle). |
| **Input shaper resonance** | Accelerometer on print head → scope captures vibration during a tap test | Find resonant frequencies for Klipper's input shaper. Currently needs ADXL345 + Raspberry Pi. |
| **Heated bed MOSFET check** | Scope the MOSFET gate and drain during PWM switching | Diagnose temperature oscillation, slow switching, MOSFET degradation. |
| **End stop signal quality** | Capture end stop trigger signal during homing | Detect bounce, noise, incorrect voltage levels causing homing failures. |
| **Servo/ESC tester** | Signal gen outputs standard PWM at 50Hz, adjustable 1-2ms pulse width | Test RC servos and ESCs directly without an Arduino or flight controller |

## Marine Electronics

Niche but boat owners spend heavily on electronics. Marine electrical problems are common and expensive.

| Test | How it works | Why it matters |
|---|---|---|
| **NMEA 0183 decode** | UART decode of RS-422 serial protocol (4800/38400 baud) | Standard marine data protocol. GPS, depth, wind, speed — all communicate via NMEA 0183. |
| **NMEA 2000 decode** | CAN bus decode with marine PGN lookup tables | Modern marine network standard. Same as automotive CAN but different message definitions. |
| **Alternator ripple** | Scope the charging system output, measure AC ripple on DC | Ripple > 0.5V = bad diode in alternator rectifier. #1 charging system problem on boats. |
| **Stray current detection** | Measure AC and DC voltage between hull/drive and water ground | Stray current causes galvanic corrosion — destroys props, shafts, through-hulls. A stray current meter costs $300+. |
| **Battery bank logging** | Log voltage and current over charge/discharge cycles | Detect weak cells, compare batteries, verify charger operation. |
| **Bonding system test** | Measure resistance between bonded underwater metals | Verify the bonding system is intact (corrosion protection). |

## Electric Vehicle

Growing fast. Independent shops need affordable EV diagnostic tools.

| Test | How it works | Why it matters |
|---|---|---|
| **J1772 pilot signal** | Decode the 1kHz PWM signal on the EV charging plug (pin 1) | PWM duty cycle encodes available current (6A-80A). Diagnose charging failures, verify EVSE operation. Simple and within scope bandwidth. |
| **CCS/CHAdeMO CAN decode** | CAN bus decode of DC fast charge communication | Debug fast charging negotiation failures. See the voltage/current requests between car and charger. |
| **BMS CAN decode** | Decode Battery Management System CAN traffic | Read individual cell voltages, temperatures, state of charge, error codes. |
| **12V system analysis** | Same as automotive — the 12V auxiliary system still has all the same components | EVs still have 12V batteries, alternators (DC-DC converters), and CAN buses. |
| **Charging efficiency** | Measure input power (grid side) vs battery power simultaneously | Calculate charging losses. Compare Level 1 vs Level 2 vs DC fast charge efficiency. |

## Industrial Maintenance

| Test | How it works | Why it matters |
|---|---|---|
| **Motor current signature analysis (MCSA)** | FFT of motor current reveals mechanical problems | Bearing wear, eccentricity, broken rotor bars, air gap issues — all visible in the current spectrum WITHOUT stopping the motor. Dedicated MCSA tools cost $2,000-10,000. |
| **Vibration analysis** | Accelerometer input → FFT of vibration data | Imbalance, misalignment, looseness, bearing defects. Each shows up at characteristic frequencies. |
| **4-20mA loop test** | Signal gen sources a 4-20mA current to test sensors and transmitters | Verify that a pressure/temperature/level transmitter responds correctly across its range. |
| **PLC I/O verification** | Check PLC output voltage, timing, and input threshold levels | Diagnose "it works on the bench but not in the field" problems. |
| **VFD diagnostics** | Capture 3-phase output waveform, measure carrier frequency | Check for voltage imbalance, dead-time issues, switching noise. |
| **Power quality** | Mains voltage FFT — show harmonics, calculate THD | LED drivers, VFDs, and switch-mode supplies inject harmonics. Excessive harmonics damage equipment and trip breakers. |

## Drone / RC

| Test | How it works | Why it matters |
|---|---|---|
| **ESC protocol decode** | Decode DShot150/300/600, OneShot125, standard PWM throttle signals | Verify the flight controller is sending correct throttle commands. Debug ESC desync. |
| **Motor KV measurement** | Drive motor at known voltage, measure back-EMF to calculate KV | Verify motor specs match listing. Compare motors from different manufacturers. |
| **Battery discharge profiler** | Log voltage under constant load over time. Graph the discharge curve. | Compare cell quality, detect puffing/degradation, verify mAh rating. |
| **Servo tester** | Generate adjustable PWM from signal gen (1-2ms at 50Hz) | Test servos directly without a receiver or flight controller. |
| **FPV video signal** | Analog FPV video is composite NTSC/PAL — within scope bandwidth | Diagnose video noise, check signal levels at camera/VTX/receiver. |

## Education

| Feature | How it works |
|---|---|
| **Physics lab mode** | Guided experiments: sound waves via microphone, pendulum timing, spring resonance, RC circuit transients |
| **Component identifier** | Probe unknown component, signal gen tests it, scope analyzes response: "4.7µF capacitor, 0.3Ω ESR, 25V rated" |
| **Waveform math tutorial** | Interactive: "This is a sine wave. Add the 3rd harmonic. See how it starts looking like a square wave? This is Fourier synthesis." |
| **Circuit simulator companion** | Match real scope measurements with textbook predictions. "The theory says the cutoff frequency is 1.59kHz. Let's measure it." |
| **Measurement techniques** | Step-by-step guide to proper scope technique: probe compensation, ground clip placement, bandwidth limitations |

## Module System Architecture

### JSON Procedure Files

Each module is a JSON file stored on the device filesystem:

```
2:/Modules/
├── hvac/
│   ├── compressor_current.json
│   ├── capacitor_test.json
│   ├── flame_rectification.json
│   └── ecm_motor.json
├── auto/
│   ├── compression_test.json
│   ├── injector_analysis.json
│   ├── can_decode.json
│   └── obd2_reader.json
├── audio/
│   ├── frequency_response.json
│   ├── speaker_impedance.json
│   └── thd_measurement.json
├── solar/
│   ├── iv_curve.json
│   ├── inverter_thd.json
│   └── mppt_efficiency.json
├── marine/
│   ├── nmea0183_decode.json
│   ├── nmea2000_decode.json
│   └── alternator_ripple.json
├── ham/
│   ├── antenna_analyzer.json
│   ├── harmonic_check.json
│   └── cw_decoder.json
└── education/
    ├── rc_circuit.json
    ├── sound_waves.json
    └── component_id.json
```

### Module JSON Structure

```json
{
  "name": "Compressor Current Analysis",
  "category": "HVAC",
  "version": "1.0",
  "description": "Analyze compressor startup and running current",
  "icon": "compressor.bmp",

  "setup": {
    "mode": "oscilloscope",
    "ch1": {
      "enabled": true,
      "label": "Compressor Current",
      "probe": "current_clamp",
      "probe_ratio": "10A/V",
      "coupling": "DC",
      "volts_div": "2V",
      "position": 50
    },
    "ch2": {
      "enabled": false
    },
    "timebase": "200ms",
    "trigger": {
      "source": "ch1",
      "mode": "single",
      "edge": "rising",
      "level": "5A"
    }
  },

  "measurements": [
    {
      "id": "inrush_peak",
      "type": "peak_max",
      "channel": "ch1",
      "window_ms": [0, 500],
      "unit": "A",
      "label": "Locked Rotor Amps"
    },
    {
      "id": "running_rms",
      "type": "rms",
      "channel": "ch1",
      "window_ms": [2000, 8000],
      "unit": "A",
      "label": "Running Load Amps"
    },
    {
      "id": "lra_ratio",
      "type": "ratio",
      "numerator": "inrush_peak",
      "denominator": "running_rms",
      "label": "LRA/RLA Ratio"
    },
    {
      "id": "start_time",
      "type": "time_to_settle",
      "channel": "ch1",
      "settle_pct": 120,
      "of": "running_rms",
      "unit": "ms",
      "label": "Start Time"
    }
  ],

  "pass_fail": [
    {
      "measurement": "lra_ratio",
      "max": 6.0,
      "fail_message": "LRA/RLA ratio too high — possible mechanical binding or low voltage"
    },
    {
      "measurement": "start_time",
      "max": 500,
      "fail_message": "Start time too long — check start capacitor and relay"
    }
  ],

  "reference_waveform": "compressor_scroll_good.dat",

  "instructions": [
    {"step": 1, "text": "Clamp current probe around compressor COMMON wire", "image": "clamp_common.bmp"},
    {"step": 2, "text": "Ensure compressor is OFF", "image": null},
    {"step": 3, "text": "Set thermostat to call for cooling", "image": null},
    {"step": 4, "text": "Press RUN — compressor will start", "image": null},
    {"step": 5, "text": "Wait 10 seconds for stable reading", "image": null}
  ],

  "knowledge_base": {
    "normal": "Scroll compressors: LRA 4-5x RLA, start time < 200ms. Reciprocating: LRA 5-6x RLA, start time < 400ms.",
    "high_lra": "Excessive LRA suggests mechanical binding, low voltage, or failed start components.",
    "long_start": "Long start time with normal LRA suggests weak start capacitor or potential relay issues.",
    "high_running": "Running amps > 110% of nameplate RLA suggests high head pressure, low suction, or mechanical wear."
  }
}
```

### Why This Architecture Wins

1. **One firmware, infinite modules** — adding a new test is just adding a JSON file, no recompilation
2. **Community contributed** — an HVAC tech writes a module for their specific diagnostic, shares it
3. **Trade-specific language** — the screen says "Locked Rotor Amps" not "Peak Current", which matters to the tech in the field
4. **Guided procedures** — step-by-step instructions with images, like having a mentor
5. **Pass/fail** — green/red indicators against industry standards, no interpretation needed
6. **Reference waveforms** — "this is what good looks like" — learn by comparison
7. **Knowledge base** — built-in troubleshooting guidance based on the results

## Small Engine / Outdoor Power Equipment

Lawnmowers, chainsaws, generators, pressure washers, snowblowers. Every small engine shop in the country, plus millions of DIYers. The ignition and charging systems are simpler than automotive but the diagnostic approach is the same — and these shops have almost no electronic test equipment.

| Test | How it works | Why it matters |
|---|---|---|
| **Ignition coil output** | Capture primary coil waveform during cranking or running | No-spark conditions, weak spark, intermittent misfire. Small engine coils are simple 2-wire — easy to probe. |
| **Charging system (stator)** | Scope the AC output from the magneto/alternator stator | Bad stator = no charge = dead battery. Measure AC voltage and waveform shape. Missing phases = shorted windings. |
| **Kill switch / safety interlock** | Verify the kill circuit opens and closes cleanly | Blade-brake-clutch, seat switch, handlebar deadman — these fail constantly. Scope shows intermittent contacts that a meter misses. |
| **Governor hunting** | Log engine RPM (from ignition pulse frequency) over time | Governor surge/hunt is a common complaint. The scope can show the RPM oscillation pattern and help dial in governor adjustment. |
| **Electric starter current** | Current clamp on starter circuit | Same as automotive starter profiling but for smaller motors. Diagnose dragging starter, low battery, high compression. |

**Why this market matters:** There are ~100,000+ small engine repair shops in the US, and most of them work with a multimeter and "experience." A $70 scope that speaks their language (ignition coil, stator, kill switch) would be immediately useful. The Briggs & Stratton / Kohler / Honda small engine community is huge.

## Motorcycle / Powersports

Different community from automotive, different systems, different culture. Motorcycle mechanics are often owner-mechanics who wrench on their own bikes.

| Test | How it works | Why it matters |
|---|---|---|
| **Stator AC output** | Scope all 3 phases of the stator output (one at a time with CH1) | The #1 charging failure on motorcycles. A shorted stator winding shows as one phase with lower amplitude. Dedicated stator testers cost $150+. |
| **Regulator/rectifier output** | Scope the DC output ripple from the reg/rec | Bad regulator = overcharging (kills battery) or undercharging. The ripple pattern shows which diodes are working. Same as alternator ripple but different physical setup. |
| **CDI ignition timing** | Capture trigger coil signal and CDI output simultaneously | Verify advance curve, diagnose no-start conditions. CDI boxes fail silently — the scope is the only way to see inside them. |
| **Throttle Position Sensor** | Monitor TPS voltage through full throttle range | Fuel-injected bikes rely on TPS for fueling. A dead spot or noisy TPS causes hesitation. The scope shows what a multimeter can't — the transition quality. |
| **Fuel injector waveform** | Capture injector driver signal at idle and WOT | Modern sport bikes run 4 injectors. Compare pulse widths across cylinders to find lean/rich cylinders. |
| **ABS wheel speed** | Same as automotive but relevant for newer bikes | ABS module failures are common and expensive. Verify the sensor signal before replacing the module. |

**Why this market matters:** The motorcycle aftermarket is massive. Forums like ADVrider, SportBikes.net, and the various brand-specific communities have hundreds of thousands of members who do their own maintenance. A YouTube video titled "diagnose your motorcycle charging system with a $70 scope" would go viral in that community.

## Appliance Repair / White Goods

Washing machines, dryers, dishwashers, refrigerators, ovens. ~150,000 appliance repair techs in the US, and the industry is moving rapidly from simple electromechanical to inverter-driven, microcontroller-controlled appliances that require electronic diagnosis.

| Test | How it works | Why it matters |
|---|---|---|
| **Inverter motor diagnostics** | Current clamp on motor phases, capture startup and running waveform | Samsung/LG/GE front-load washers use inverter-driven motors. Bad inverter = no spin/agitate. The current waveform shows whether the inverter or the motor is the problem. |
| **Control board relay outputs** | Scope the relay driver signals on the main control board | Modern appliances have 5-15 relays controlled by a microcontroller. If the board commands the relay but nothing happens, the relay or the load is bad. If the board doesn't command it, the board is bad. The scope answers the question. |
| **NTC thermistor validation** | Measure thermistor voltage at known temperature, compare to expected curve | Thermistors drift over time. A dryer thermistor that reads 10% high runs 10% too cold. The meter can measure resistance, but a reference curve comparison tells you if it's in spec. |
| **Door lock motor current** | Capture current draw of the door lock actuator | Front-load washer door locks are electromechanical and fail frequently. The current waveform shows: lock engaging, latch engagement, microswitch closure. A partial failure (lock engages but latch doesn't catch) is visible in the waveform but invisible on a multimeter. |
| **Water level pressure switch** | Monitor the pressure switch signal during fill | Electronic pressure sensors output a frequency proportional to water level. If the frequency is wrong, the machine overfills or underfills. The scope measures frequency directly. |

**Why this market matters:** Appliance repair techs currently use basic multimeters. The industry is shifting to inverter-driven, electronically-controlled appliances that need scope-level diagnostics. Samsung and LG publish waveform-based diagnostic procedures that most techs can't perform because they don't own a scope. A $70 scope with appliance-specific guided tests fills a real gap.

## Generator / Backup Power

Standby generators, portable generators, UPS systems. Installers, electricians, and facility maintenance techs who commission and maintain backup power systems.

| Test | How it works | Why it matters |
|---|---|---|
| **Output voltage quality** | Scope the AC output, measure THD via FFT | A generator with high THD damages sensitive electronics. Inverter generators claim "clean power" — verify the claim. |
| **Frequency stability** | Track output frequency over time under varying load | Frequency should be 60.0 Hz ±0.5 Hz. Governors that hunt show up immediately. |
| **Load step response** | Capture voltage/frequency transient when a large load turns on | How quickly does the generator recover? Long sag = undersized generator or slow governor. |
| **Transfer switch timing** | Scope the utility and generator feeds simultaneously | Measure the blackout window during transfer. Critical for UPS-backed loads. Should be <100ms for most automatic transfer switches. |
| **Battery charger output** | Monitor the float/bulk/equalize charge profile on the starter battery | Standby generators sit for months. If the battery charger fails, the generator won't start when needed. The scope catches charger failure before the outage does. |

**Why this market matters:** Backup power is critical infrastructure. Every hospital, data center, cell tower, and grocery store has generators that require periodic testing. The techs who maintain them need to verify output quality, and most carry only a basic multimeter.

## Welding

Welders are deeply technical, tool-obsessed, and already comfortable with electrical concepts. The welding community is large and active online.

| Test | How it works | Why it matters |
|---|---|---|
| **Arc voltage/current waveform** | Capture welding arc voltage and current (with appropriate isolation) | The waveform reveals arc stability, spatter tendency, heat input. Professional welding engineers use oscilloscopes for WPS (Welding Procedure Specification) qualification. |
| **Pulse waveform verification** | Scope the pulse output of a pulse MIG or pulse TIG welder | Verify that the welder's pulse settings (peak current, background current, frequency, pulse width) match what's programmed. Cheap welders often lie about their pulse parameters. |
| **Wire feed speed** | Tach signal from wire feeder motor | Verify WFS matches the setting. Inconsistent wire feed = inconsistent weld quality. |
| **Welder output ripple** | Scope the DC output of a welder at no-load and under arc | Excessive ripple = bad diodes in the rectifier. Same concept as alternator ripple test. |

**Why this market matters:** The American Welding Society has ~70,000 members. Welding schools graduate thousands of students per year. The intersection of "welder who owns a scope" is currently small, but that's because no $70 scope has ever spoken their language.

### Target Audiences by Market Size

| Industry | US Workforce | Current Tool Cost | Our Advantage |
|---|---|---|---|
| HVAC/R | ~400,000 | $200-600 meters | Scope + motor analysis on a meter budget |
| Automotive | ~750,000 | $200-2,000 scopes | PicoScope features at 1/10 the price |
| Solar | ~250,000 (growing) | $300-1,000 analyzers | I-V curve + inverter analysis |
| Audio | ~100,000 pro + millions hobbyist | $200-500 audio analyzers | Bode plot + THD + speaker impedance |
| Marine | ~50,000 + millions boat owners | $300+ marine meters | NMEA decode + stray current |
| 3D printing | Millions of hobbyists | $50-200 in various tools | Stepper + resonance in one tool |
| Ham radio | ~750,000 US licensees | $50-300 analyzers | Antenna + harmonic + receiver |
| Industrial | ~500,000 maintenance techs | $2,000-10,000 MCSA tools | Motor analysis at 1/100 the price |
| Education | Millions of students | $200-500 lab scopes | Guided labs + component ID |
| EV service | ~50,000 (growing fast) | $500-2,000 EV tools | J1772 + BMS + CAN decode |
| Small engine | ~100,000 shops + millions DIY | $100-300 ignition testers | Coil + stator + governor in one tool |
| Motorcycle | ~50,000 shops + millions riders | $150-500 stator testers | Charging + CDI + injector analysis |
| Appliance repair | ~150,000 | $200-400 meters | Inverter motor + control board diagnostics |
| Generator/backup | ~30,000 + electricians | $300-1,000 power analyzers | THD + frequency + transfer switch timing |
| Welding | ~70,000 AWS members + schools | $500-2,000 weld monitors | Arc waveform + pulse verification |
