# OpenScope as a Custom Test Platform

Brainstormed 2026-03-29. The idea: strip or augment the oscilloscope firmware and use the hardware as a **programmable test fixture** for manufacturing, automotive service, field repair, and education.

## The Concept

An $80 handheld device with analog inputs, signal output, protocol comms, a screen, and buttons is essentially a cheap LabVIEW-in-a-box. Instead of general-purpose oscilloscope firmware, load a **test procedure module** that guides the user through a specific sequence:

```
┌─────────────────────────────────┐
│  BMW F30 EPB Service Mode        │
│                                  │
│  Step 3 of 5: Retract calipers   │
│                                  │
│  Sending CAN: 0x7E0 [02 10 03]  │
│  Motor current: 2.1A ✓           │
│  ████████████████░░░░ Progress    │
│                                  │
│  [OK] Continue    [MENU] Abort   │
└─────────────────────────────────┘
```

## Why This Hardware Works

| Capability | Hardware | Test Use |
|-----------|----------|----------|
| 2 analog inputs | Scope probes + ADC via FPGA | Measure voltage, current, waveforms |
| Signal output | DAC (2ch 12-bit) | Drive signals, actuate relays, simulate sensors |
| Serial comms | USART, SPI, I2C | Talk to ECUs, sensors, microcontrollers |
| CAN bus | Via external transceiver (MCP2551) | Automotive diagnostics, industrial |
| K-Line | Via L-line driver | OBD-I, older vehicle protocols |
| Screen | 320x240 color LCD | Step-by-step instructions, PASS/FAIL |
| Buttons | 15 physical buttons | Navigate, confirm, abort |
| USB | OTG port | Load procedures, export results |
| Portable | Battery powered, handheld | Field use, production floor |

## Target Users

### Manufacturing / QC
- Small electronics shops testing PCBs off a production line
- "Plug board into jig, press OK, get PASS/FAIL"
- Test sequences: check voltage rails, send test patterns, verify responses
- Log results with serial number for traceability

### Automotive Service
- Independent shops that can't afford $5000 dealer scan tools
- Actuator tests: retract EPB, bleed ABS, cycle injectors, test fuel pump
- Sensor validation: verify TPS range (0.5-4.5V), check MAF frequency response
- Custom pigtail harnesses for specific connectors (OBD-II, manufacturer-specific)

### Field Service / HVAC
- Furnace control board testing: flame sensor current, ignitor resistance, 24VAC timing
- Refrigeration: compressor current draw, defrost cycle verification
- Motor testing: start/run capacitor health, winding resistance

### Education
- Lab exercises with on-screen prompts and automatic grading
- "Connect probe to TP3. Press OK. Expected: 2.5V ± 0.1V"
- Students follow guided procedures, results are logged

## Architecture: Test Procedure Modules

This maps directly to our existing module API (`module_api.h`). A test procedure is a module with a state machine:

```c
typedef enum {
    STEP_INSTRUCTION,    /* Show text, wait for user OK */
    STEP_MEASURE,        /* Take a measurement, compare to spec */
    STEP_ACTUATE,        /* Drive an output signal */
    STEP_COMMUNICATE,    /* Send/receive serial protocol */
    STEP_WAIT,           /* Delay or wait for condition */
    STEP_RESULT,         /* Display pass/fail */
} step_type_t;

typedef struct {
    step_type_t type;
    const char *description;     /* "Check 5V rail" */

    /* For MEASURE steps */
    uint8_t channel;             /* 0=CH1, 1=CH2 */
    float expected_value;        /* 5.0 */
    float tolerance_pct;         /* 5.0 (= ±5%) */
    const char *unit;            /* "V" */

    /* For ACTUATE steps */
    float output_voltage;
    float output_frequency;
    uint32_t duration_ms;

    /* For COMMUNICATE steps */
    const uint8_t *tx_data;
    uint8_t tx_len;
    uint8_t *rx_buf;
    uint8_t rx_expected_len;
    uint32_t timeout_ms;
} test_step_t;

typedef struct {
    const char *name;            /* "BMW F30 EPB Service" */
    const char *version;         /* "1.0.0" */
    const test_step_t *steps;
    uint8_t num_steps;
} test_procedure_t;
```

## Procedure Distribution

### Option A: Compiled-in modules
- Write procedure in C, compile into firmware
- Most performant, full access to platform API
- Requires rebuilding firmware for each new procedure
- Good for: manufacturers with fixed test sequences

### Option B: Interpreted procedure files (JSON/YAML)
- Store procedure definitions on SPI flash or SD card
- A built-in interpreter reads and executes steps
- Users can create/edit procedures on a PC and transfer via USB
- Good for: shops and hobbyists creating custom tests
- Limited: can't do complex logic without scripting

### Option C: Lua/MicroPython scripting (ESP32 path)
- ESP32 daughter card runs a scripting engine
- Full programming capability without recompiling firmware
- Scripts loaded from SD card or downloaded via WiFi
- ESP32 has the RAM/CPU for an interpreter
- Good for: maximum flexibility, community sharing

### Recommended: Start with Option A, evolve to B

Build a few example compiled modules (automotive EPB, PCB voltage check, resistance verification). This proves the concept with zero new infrastructure — just use the existing module API.

Then build the JSON interpreter for Option B, which opens it up to non-programmers. The JSON format would be something like:

```yaml
name: "5V Power Supply Test"
version: "1.0"
steps:
  - type: instruction
    text: "Connect CH1 probe to 5V output"
  - type: measure
    channel: 1
    expect: 5.0
    tolerance: 5%
    unit: V
    fail_message: "5V rail out of spec"
  - type: instruction
    text: "Connect CH1 probe to 3.3V output"
  - type: measure
    channel: 1
    expect: 3.3
    tolerance: 3%
    unit: V
  - type: result
    text: "Power supply PASSED"
```

## Custom Hardware Accessories

For specific applications, users would build **pigtail harnesses**:

- **OBD-II pigtail**: OBD connector → banana jacks → scope probes + CAN transceiver
- **PCB test jig**: Pogo pin bed-of-nails → banana jacks → scope probes
- **HVAC harness**: Molex connector → banana jacks for flame sensor, ignitor, 24VAC
- **Automotive sensor**: Weatherpack connector → banana jacks for specific sensor pinout

These are cheap to make ($5-20 in parts) and could be shared/sold as a community accessory.

## Business Model Ideas

- **Open source firmware** (GPL v3) — the core platform is free
- **Community module repository** — like Arduino libraries, anyone can contribute
- **Premium modules** — complex automotive procedures could be commercial (like Autel/Launch scan tools charge for manufacturer coverage)
- **Custom module development** — consulting service for manufacturers who want a specific test procedure built

## CAN Bus Integration

The GD32F307 has **two CAN controllers** built in (CAN0 and CAN1) with hardware arbitration, error detection, and filtering. The MCU handles the protocol natively — you just need a **physical transceiver** to convert the logic-level TX/RX to the differential CAN-H/CAN-L bus.

**Do NOT use the DAC/signal generator for CAN.** CAN is a differential protocol with precise electrical requirements (dominant/recessive states, 120 ohm termination). The 12-bit DAC outputs single-ended analog — fundamentally wrong signal type.

### Recommended Transceiver Options

| Chip | Cost | Notes |
|------|------|-------|
| MCP2551 | $1 | Classic, 5V, widely available |
| SN65HVD230 | $1 | 3.3V compatible, better for GD32 |
| TJA1050 | $0.80 | NXP, automotive grade |

### Wiring

```
GD32 CAN0_TX (PA12) --> MCP2551 TXD --> CAN-H ──┐
GD32 CAN0_RX (PA11) <-- MCP2551 RXD <-- CAN-L ──┤── OBD-II pins 6,14
                                         120Ω ───┘   (or DB9 connector)
```

### What This Enables

- **OBD-II diagnostics** — read DTCs, clear codes, live data (PIDs)
- **Actuator commands** — retract EPB, bleed ABS, activate fuel pump
- **ECU programming** — reflash modules (with appropriate security access)
- **CAN bus sniffing** — capture all traffic, filter by ID, decode protocols
- **UDS/KWP2000** — standardized diagnostic protocols over CAN

A complete CAN interface adds ~$4 in parts (transceiver + OBD connector).

## Expanding I/O with Interface Boards

Two analog channels and one signal output is sufficient for simple tests, but complex fixtures need more I/O. The solution is a small **interface board** between the scope and the device under test, controlled via I2C or SPI from the GD32.

### Analog Multiplexing

An analog mux like the **CD4051** ($0.50) provides 8:1 switching — one chip gives 8 test points through a single scope channel. The firmware switches channels between measurements.

```
                    CD4051 (8:1 analog mux)
                   ┌────────────────────────┐
CH1 ◄──────────────┤ COM              Y0-Y7 ├──── 8 test points
GD32 GPIO (3 pins) ┤ A, B, C (select)       │
                   └────────────────────────┘
```

Two mux chips = 16 test points through 2 scope channels. The module firmware handles switching transparently: "measure test point 5" = set mux to channel 5, read CH1.

### Digital I/O Expansion

| Chip | Cost | Channels | Use |
|------|------|----------|-----|
| MCP23017 (I2C) | $1 | 16 GPIO | Relay control, switch reading, LED indicators |
| 74HC595 (SPI) | $0.20 | 8 output | Relay banks, unlimited when chained |
| PCF8574 (I2C) | $0.50 | 8 GPIO | Simple digital I/O |

### Additional Analog Outputs

| Chip | Cost | Channels | Use |
|------|------|----------|-----|
| MCP4728 (I2C) | $2 | 4 x 12-bit DAC | Simulate sensor outputs, drive test signals |
| MCP4725 (I2C) | $1 | 1 x 12-bit DAC | Single additional output |

### Complete Interface Board Example

For a PCB manufacturing test fixture:

```
OpenScope                    Interface Board                    DUT (Device Under Test)
┌──────────┐                ┌─────────────────────┐            ┌──────────────┐
│ CH1  ────┼────────────────┤ CD4051 mux (8:1)    ├──── TP1-8 ─┤ Test points  │
│ CH2  ────┼────────────────┤ CD4051 mux (8:1)    ├──── TP9-16 ┤              │
│ SigGen ──┼────────────────┤ MCP4728 (4ch DAC)   ├──── Drive  ┤ Power/signal │
│ GND  ────┼────────────────┤                     ├──── GND ───┤              │
│ I2C SDA ─┼────────────────┤ MCP23017 (16 GPIO)  ├──── Relays ┤ Relay board  │
│ I2C SCL ─┼────────────────┤                     │            │              │
│ CAN TX ──┼────────────────┤ MCP2551 transceiver ├──── CAN ───┤ CAN port     │
│ CAN RX ──┼────────────────┤                     │            │              │
└──────────┘                └─────────────────────┘            └──────────────┘

Total interface board BOM: ~$8
```

### Interface Board as a Product

The interface board could be a standardized open-source design:

- **Basic** ($5): 1x analog mux + CAN transceiver
- **Standard** ($12): 2x analog mux + CAN + 16 GPIO + 4 DAC outputs
- **Pro** ($20): Standard + relay driver stage + screw terminals + case

Users would buy or build the board, then create test procedures specific to their application. The board is generic — the test module makes it specific.

## Comparison to Alternatives

| Solution | Cost | Flexibility | Portability | Analog In | Standalone |
|----------|------|------------|-------------|-----------|------------|
| National Instruments + LabVIEW | $5,000+ | Very high | Desktop only | Yes | No |
| Digilent Analog Discovery 3 | $300-400 | High | PC-tethered | Yes | No |
| PicoScope | $200-2,000 | High | PC-tethered | Yes | No |
| Autel/Launch scan tool | $500-5,000 | Fixed | Handheld | No | Yes |
| Raspberry Pi + HATs | $50-100 | High | Needs monitor | With HAT | Sort of |
| Arduino + shields | $30 | High | Needs PC | Limited | No |
| Tablet + USB scope | $200-500 | Medium | Awkward | Yes | Sort of |
| **OpenScope + module** | **$80** | **High** | **Handheld, battery** | **Yes** | **Yes** |

The gap: **standalone, handheld, battery-powered, with real analog I/O, programmable, at $80.** Nobody else occupies that spot.

## Interface Board Manufacturing

**PCB fabrication:** JLCPCB or PCBWay, ~$2-5 for 5 boards. With SMD assembly service, fully populated Standard boards would be ~$12-15 each in quantity 10.

**Connectors:**
- BNC for analog channels (matches standard scope probes)
- Screw terminals for expanded I/O (relay connections, sensor wires — field-serviceable)
- DB9 for CAN (industry standard) or 2-pin screw terminal for raw CAN-H/CAN-L
- Pin headers or ribbon cable for I2C/SPI back to scope

**Enclosure:** 3D printed two-piece case, ~80x60x25mm. STL files shared on Thingiverse/Printables. ~$1 in filament.

**Total cost for complete custom test platform:**

| Item | Cost |
|------|------|
| OpenScope 2C53T | $80 (already own) |
| Interface board (assembled) | $12-15 |
| 3D printed case | $1 |
| Application-specific harness | $5-10 |
| Test module firmware | Free (open source) |
| **Total** | **~$100** |

**Future:** Design a KiCad schematic for the Standard interface board (2x CD4051 mux, MCP2551 CAN, MCP23017 GPIO, MCP4728 DAC). Open source the design files alongside the firmware.

## Next Steps

1. Build 2-3 example test procedure modules using existing module API
2. Test on real hardware once device arrives
3. Design JSON procedure interpreter
4. Build a "procedure editor" PC tool (could be web-based)
5. Create community module repository structure

## Example Modules to Build First

1. **PCB Voltage Rail Checker** — measure N test points against expected values
2. **Resistor Verification** — already started (color band calculator)
3. **Automotive Relay Test** — actuate via signal gen, measure response current
4. **Continuity Matrix** — guided point-to-point continuity test for cable harnesses
5. **Sensor Curve Tracer** — sweep input voltage, record output, compare to expected curve
