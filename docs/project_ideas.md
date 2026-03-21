# Project Ideas and Feasibility

## Resource Budget

The GD32F307 has significant headroom beyond what the stock firmware uses:

- **RAM:** ~36KB free (256KB total, ~220KB used)
- **Flash:** ~267KB free (1MB total, 733KB used)
- **CPU:** 120MHz Cortex-M4 with DSP and FPU — plenty for protocol decoding
- **Unused peripherals:** 2x CAN, extra timers, extra UARTs, internal ADC

## Modification Difficulty Levels

### Binary Patch (No recompilation)

These can be done by finding values in the hex dump and changing them:

| Modification | Approach | Difficulty |
|---|---|---|
| Change UI colors | Find RGB565 color constants, replace values | Easy |
| Change auto-shutdown timeout | Find timer comparison constant, change value | Easy |
| Change menu strings | Replace strings with same-length or shorter text | Easy |
| Change boot logo | Replace LOGO2C53T.jpg on internal filesystem | Easy |

### Code Injection (Small additions)

Write small ARM Thumb routines, place in free flash space, redirect existing code:

| Modification | Approach | Difficulty |
|---|---|---|
| Screen flash on continuity | Add display call at EXTI3 handler (0x08009C11) | Medium |
| Custom beep patterns | Modify TIM8 PWM parameters at buzzer trigger points | Medium |
| Additional auto-off intervals | Add menu options to existing auto-shutdown UI | Medium |

### New Features (Requires RE + development)

| Feature | What's Needed | Feasibility |
|---|---|---|
| Protocol decode (CAN) | Understand FPGA sample readout, add decode logic + overlay | High — MCU has built-in CAN controller |
| Protocol decode (I2C) | Same as CAN but simpler protocol | High |
| Protocol decode (UART) | Simplest protocol to decode | High |
| USB streaming mode | Add USB CDC or bulk mode alongside mass storage | Medium-High |
| OBD-II decode | CAN decode + PID lookup tables | High (after CAN works) |
| Compression test | Current waveform analysis + cylinder counting | Medium |

## Automotive Diagnostic Module

### Architecture

```
┌─────────────────────────────────┐
│  Module: CAN Bus Decode         │
│  - Trigger: edge detect         │
│  - Decode: bit stuffing, CRC    │
│  - Display: ID + data overlay   │
├─────────────────────────────────┤
│  Module: Compression Test       │
│  - Trigger: off ignition signal │
│  - Analyze: current peaks       │
│  - Display: cylinder bar graph  │
├─────────────────────────────────┤
│  Module: Injector Analysis      │
│  - Trigger: injector pulse      │
│  - Measure: pulse width, duty   │
│  - Display: timing diagram      │
├─────────────────────────────────┤
│  Core: Waveform capture + UI    │
│  (shared display, FPGA, etc.)   │
└─────────────────────────────────┘
```

### CAN Bus — Native Hardware Support

The GD32F307 has **two built-in CAN controllers** (CAN0 at 0x40006400, CAN1 at 0x40006800) that are completely unused in the stock firmware. This means:

- No need to decode CAN from analog waveform samples
- The MCU can receive CAN frames directly in hardware
- Only needs a simple external CAN transceiver chip (MCP2551 or SN65HVD230, ~$1)
- Could simultaneously show the analog waveform (via scope) AND the decoded protocol data (via CAN controller)

### Vehicle Definition Files

Protocol-specific data can be stored as files on the filesystem:

```
2:/Vehicles/
  ├── generic_obd2.dbc      (standard OBD-II PID definitions)
  ├── toyota_camry_2020.dbc  (manufacturer-specific CAN IDs)
  └── honda_civic_2019.dbc   (manufacturer-specific CAN IDs)
```

DBC files are the industry standard for CAN database definitions. Open-source DBC databases exist for many vehicles.

### Safety Considerations

- **Read-only by default** — passive CAN monitoring only
- **OBD-II queries** — opt-in, standard PIDs only (same as any $20 scan tool)
- **Raw CAN transmit** — expert mode only, requires deliberate enable with warnings
- **Physical adapter** — read-only adapters simply don't wire up the TX line

## USB Streaming / PC Oscilloscope Mode

### Concept

Add a firmware mode that streams raw ADC samples over USB to a PC, enabling use with open-source oscilloscope software.

### Software Options on PC Side

| Software | Description |
|---|---|
| sigrok/PulseView | Universal signal analysis framework with 100+ protocol decoders |
| OpenHantek | Open-source DSO software (designed for Hantek USB scopes) |
| ngscopeclient | GPU-accelerated oscilloscope frontend (used by ThunderScope) |

### Implementation Path

1. Add "USB Streaming" menu option alongside existing "USB Sharing"
2. When selected: stop LCD rendering, pipe FPGA samples to USB bulk endpoint
3. Write a sigrok hardware driver that knows the 2C53T's USB protocol
4. PulseView handles display, measurements, protocol decoding, export

### Bandwidth Considerations

- 250 MS/s × 8 bits = 250 MB/s raw (too fast for USB Full-Speed)
- USB Full-Speed = ~1 MB/s → need decimation or burst capture
- Options: send every Nth sample, or capture a buffer then transfer

## Open-Source Project Structure

### Realistic Distribution

```
GitHub releases:
  firmware_v2.0_base.bin          (scope + multimeter + siggen)
  firmware_v2.0_auto.bin          (+ CAN decode + compression test)
  firmware_v2.0_electronics.bin   (+ I2C + SPI + UART decode)
  firmware_v2.0_full.bin          (everything)
```

### Build System

```bash
make MODULES="can i2c uart obd2"    # builds with selected modules
make all                             # builds everything
```

### Development Workflow

1. Core firmware on GitHub with clean hardware abstraction
2. Compile-time module selection via flags
3. Vehicle definition files (DBC) as separate data contributions
4. Community forks for specialized use cases
