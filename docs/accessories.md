# Accessory Boards

Community-designed add-on PCBs that extend the scope's capabilities. These would be open-source hardware (KiCad files shared, order from JLCPCB/OSHPark for a few dollars each).

## RF Test Bridge (SWR / Antenna Analyzer)

Enables SWR measurement at any frequency, including VHF/UHF beyond the scope's 50MHz bandwidth. Uses diode detectors to convert RF amplitude to DC voltage, which the scope measures.

### How It Works

```
Transmitter/          Directional           Antenna
Signal Gen    ──→     Coupler        ──→    Under Test
(any freq)            (Stockton bridge
                       or tandem match)
                          │       │
                       Forward  Reflected
                       coupler  coupler
                          │       │
                       ┌──┴──┐ ┌──┴──┐
                       │Diode│ │Diode│
                       │Det. │ │Det. │
                       └──┬──┘ └──┬──┘
                          │       │
                        DC V    DC V
                          │       │
                        BNC     BNC
                        CH1     CH2
                     (forward) (reflected)
```

### What the Firmware Does

1. Read DC voltage from CH1 (forward power) and CH2 (reflected power)
2. Calculate: `SWR = (Vfwd + Vref) / (Vfwd - Vref)`
3. Calculate: `Return Loss (dB) = -20 × log10(Vref / Vfwd)`
4. If sweeping: plot SWR vs frequency
5. Display resonant frequency (minimum SWR point)

### Board Design

```
┌──────────────────────────────────────────┐
│                                          │
│  SO-239 IN ──→ Tandem Match ──→ SO-239 OUT
│                Coupler                   │
│                  │    │                  │
│              ┌───┘    └───┐              │
│              │            │              │
│          1N5711       1N5711             │
│          Schottky     Schottky           │
│              │            │              │
│          100pF Cap    100pF Cap          │
│              │            │              │
│           BNC FWD      BNC REF           │
│                                          │
│  Power: None needed (passive circuit)    │
└──────────────────────────────────────────┘
```

### Bill of Materials

| Component | Qty | Approx Cost |
|---|---|---|
| PCB (JLCPCB 5-pack) | 1 | $2 |
| SO-239 UHF connectors | 2 | $3 |
| BNC female connectors | 2 | $2 |
| Ferrite cores (FT50-43 or BN43-202) | 2-3 | $3 |
| 1N5711 Schottky diodes | 2 | $1 |
| 100pF ceramic capacitors | 2 | $0.50 |
| 10K resistors | 2 | $0.25 |
| Magnet wire (for transformer) | — | $2 |
| **Total** | | **~$14** |

### Why This Is Better Than a NanoVNA for Some Uses

| Scenario | NanoVNA | 2C53T + RF Bridge |
|---|---|---|
| Quick SWR check | Good | Good |
| Measure under full TX power | No (milliwatt only) | Yes — diode detectors handle any power with proper coupling |
| See RF waveform + SWR simultaneously | No | Yes |
| Field use with one tool | Need separate device | All-in-one: scope + SWR + multimeter |
| Price if you already have the scope | $50-100 extra | ~$14 in parts |

### Frequency Range

- **HF (1-50 MHz):** Full SWR + waveform analysis (within scope bandwidth)
- **VHF (50-300 MHz):** SWR measurement via diode detectors (DC output is within scope range even though RF isn't)
- **UHF (300-1000 MHz):** SWR measurement works, but diode detector accuracy decreases. Use faster Schottky diodes (BAT54, HSMS-2852) for better UHF performance.

## CAN Bus Adapter

Enables native CAN bus communication using the GD32F307's built-in CAN controller (if MCU pins are accessible).

### Design

```
┌──────────────────────────────────┐
│                                  │
│  OBD-II ──→ CAN         MCU     │
│  Connector   Transceiver  Pins   │
│              (MCP2551    (via    │
│               or         header  │
│               SN65HVD230) cable) │
│                                  │
│  CAN-H ←→ TXD/RXD ←→ PD0/PD1   │
│  CAN-L     (diff)     (CAN0     │
│                        alt pins) │
│                                  │
│  Optional: DB9 output            │
│  for standard CAN tools          │
│                                  │
│  Safety: read-only jumper        │
│  (disconnect TXD to prevent      │
│   accidental transmission)       │
│                                  │
└──────────────────────────────────┘
```

### Bill of Materials

| Component | Qty | Approx Cost |
|---|---|---|
| PCB | 1 | $2 |
| MCP2551 or SN65HVD230 CAN transceiver | 1 | $1.50 |
| OBD-II male connector | 1 | $3 |
| Pin header (connection to scope) | 1 | $0.50 |
| 120Ω termination resistor (switchable) | 1 | $0.10 |
| Bypass capacitors | 2 | $0.20 |
| Read-only jumper | 1 | $0.10 |
| **Total** | | **~$8** |

### Safety Features

- **Read-only jumper** — physically disconnects the TX line. Default: read-only. Must deliberately jumper to enable transmission.
- **No power from OBD-II** — the adapter doesn't draw power from the car's bus. Powered from the scope.
- **ESD protection** on CAN lines

## UART / RS-232 / RS-485 Level Shifter

For decoding serial protocols at different voltage levels.

```
┌─────────────────────────────────┐
│                                 │
│  Screw terminals (signal in)    │
│       │                         │
│  ┌────┴────┐                    │
│  │ Level   │  Selectable:       │
│  │ Shifter │  - RS-232 (±12V)   │
│  │         │  - RS-485 (diff)   │
│  │         │  - 5V logic        │
│  │         │  - 3.3V (direct)   │
│  └────┬────┘                    │
│       │                         │
│     BNC out → Scope CH1         │
│                                 │
└─────────────────────────────────┘
```

Cost: ~$5 in parts. Lets you safely connect to RS-232 ports (which can be ±12V) or RS-485 buses (which are differential) without risking the scope's input.

## Current Sense Adapter

For power analysis and automotive current measurement without an expensive current clamp.

```
┌─────────────────────────────────┐
│                                 │
│  Banana jacks (inline with      │
│  circuit under test)            │
│       │                         │
│  ┌────┴────┐                    │
│  │ Shunt   │  Selectable:       │
│  │ Resistor│  - 1Ω (1A range)   │
│  │         │  - 0.1Ω (10A)      │
│  │         │  - 0.01Ω (100A)    │
│  └────┬────┘                    │
│       │                         │
│  Differential amp               │
│       │                         │
│     BNC out → Scope CH1         │
│                                 │
│  Firmware knows the shunt       │
│  value and displays in Amps     │
│                                 │
└─────────────────────────────────┘
```

Cost: ~$10 in parts. An alternative to a current clamp ($30-100) for lower frequency measurements. The firmware would auto-scale the display to show current instead of voltage.

## Accessory Detection

The firmware could auto-detect which accessory is connected by having each board include a small resistor between a detection pin and ground. Different resistance values identify different accessories:

```
No accessory:  detection pin floating
RF Bridge:     10K to GND
CAN Adapter:   22K to GND
UART Shifter:  47K to GND
Current Sense: 100K to GND
```

The MCU reads the detection pin's ADC value and automatically loads the appropriate mode and settings. Plug in the RF bridge → scope switches to antenna analyzer mode.

## Community Contribution Model

1. Someone designs a board in KiCad
2. Shares the design files on the GitHub repo
3. Others order the PCB from JLCPCB (~$2 for 5 boards)
4. Solder the through-hole components (beginner-friendly)
5. The firmware update includes the accessory support
6. Plug in and it just works
