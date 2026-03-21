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

## HF Receiver / Mixer Board

Turns the scope into a basic HF direct-conversion receiver. The signal generator acts as the local oscillator, an external mixer converts RF to audio baseband, and the scope captures and displays (or decodes) the audio.

### How It Works

```
Antenna ──→ Bandpass ──→ NE602 ──→ Audio ──→ Scope CH1
             Filter      Mixer     Filter    (baseband
             (optional)    ↑       (LPF)      0-20kHz)
                           │
                     Signal Generator
                     output (Local
                     Oscillator at
                     target frequency)
```

The NE602/SA612 is a double-balanced mixer that multiplies the antenna signal with the LO. The difference frequency comes out as audio. Tune the signal generator to 7.074 MHz → you hear/see FT8 signals. Tune to 14.025 MHz → you hear/see CW.

### Bill of Materials

| Component | Qty | Cost |
|---|---|---|
| NE602/SA612 mixer IC | 1 | $2 |
| Crystal or LC bandpass filter (optional) | 1 | $3 |
| Audio LPF components (RC) | — | $0.50 |
| BNC/SMA antenna connector | 1 | $1 |
| BNC output to scope | 1 | $1 |
| Bypass capacitors | 3 | $0.30 |
| PCB | 1 | $2 |
| **Total** | | **~$10** |

### Firmware Features for Receiver Mode

| Feature | Description | Complexity |
|---|---|---|
| **LO tuning** | Use buttons to tune the signal generator frequency in ham band steps | Easy |
| **Band presets** | One-button selection: 40m, 20m, 15m, 10m, etc. | Easy |
| **Audio waveform display** | Show the demodulated audio on screen | Easy (already works) |
| **CW decoder** | Goertzel algorithm detects CW tone, Morse lookup table decodes to text | Easy-Medium |
| **FT8 waterfall** | FFT spectrogram showing FT8 signal traces (visual only, no decode) | Medium |
| **FT8 decode** | Full decode on-device using ft8_lib — possible but uses most CPU/RAM | Hard |
| **FT8 via USB** | Stream audio to PC, let WSJT-X decode — much more practical | Medium |
| **Frequency display** | Show the tuned frequency, band name, mode | Easy |

### CW Decoder Detail

```
Scope display in CW decode mode:

┌─────────────────────────────────────────┐
│ CW Decode  14.025 MHz   WPM: 15        │
├─────────────────────────────────────────┤
│                                          │
│ ~~Audio waveform with tone detection~~   │
│                                          │
│ ▓▓ ▓ ▓▓▓ ▓ ▓▓▓ ▓▓▓  ▓▓ ▓ ▓▓▓          │
│ (envelope visualization)                 │
│                                          │
├─────────────────────────────────────────┤
│ CQ CQ CQ DE W1AW W1AW K                 │
└─────────────────────────────────────────┘
```

Implementation:
1. Goertzel algorithm (~100 lines of C) detects CW tone (600-800 Hz)
2. Threshold → is tone present or absent?
3. Measure on/off timing → classify as dot, dash, character gap, word gap
4. Morse lookup table (~50 lines) → decoded text
5. Scroll decoded text across bottom of display

## Automatic Antenna Tuner Board

Relay-switched L-network that the scope controls via SPI/GPIO. The firmware measures SWR (via RF bridge) and switches components to find the best match.

### Design

```
┌──────────────────────────────────────────────────┐
│                                                  │
│  RF IN ──→ SWR Bridge ──→ L-Network ──→ RF OUT   │
│              │    │          │                    │
│           Fwd   Ref      Capacitor bank          │
│           det   det      (7 relays, binary       │
│            │     │        weighted:               │
│           CH1   CH2       10/20/40/80/            │
│                           160/320/640 pF)         │
│                              │                    │
│                           Inductor bank           │
│                           (7 relays, binary       │
│                            weighted toroids)      │
│                              │                    │
│                          Shift registers           │
│                          (2x 74HC595)             │
│                              │                    │
│                          SPI from scope            │
│                          (via signal gen BNC       │
│                           repurposed as data)     │
│                                                  │
│  SO-239 connectors for radio and antenna         │
└──────────────────────────────────────────────────┘
```

### Bill of Materials

| Component | Qty | Cost |
|---|---|---|
| Signal relays (HFD4 or similar) | 14 | $14 |
| Capacitors (binary-weighted, HV rated) | 7 | $3 |
| Toroid cores (T50-2 or T50-6) | 7 | $5 |
| Magnet wire | — | $2 |
| 74HC595 shift registers | 2 | $1 |
| SO-239 UHF connectors | 2 | $3 |
| PCB (larger board) | 1 | $3 |
| **Total** | | **~$31** |

### Tuning Algorithm

```python
# Pseudocode for auto-tune
best_swr = 99.0
best_config = 0

# Coarse search: try each inductor value with mid capacitance
for L in range(128):  # 7 bits = 128 inductor combinations
    set_relays(L << 7 | 64)  # mid-cap
    swr = measure_swr()
    if swr < best_swr:
        best_swr = swr
        best_L = L

# Fine search: sweep capacitors with best inductor
for C in range(128):  # 7 bits = 128 capacitor combinations
    set_relays(best_L << 7 | C)
    swr = measure_swr()
    if swr < best_swr:
        best_swr = swr
        best_config = best_L << 7 | C

# Apply best match
set_relays(best_config)
# Display: "SWR: 1.2:1  L=3.2uH  C=180pF"
```

Converges in ~256 measurements (vs 16,384 brute force). At 10ms per measurement, tuning takes ~2.5 seconds.

## Harmonic Analyzer (Firmware Only — No Accessory Needed)

Uses the scope's existing input with FFT to analyze transmitter harmonics. Just connect the scope to your transmitter's output (through an attenuator or via the RF bridge's coupled port).

### Features

| Feature | Description |
|---|---|
| FFT display | Frequency-domain view with logarithmic amplitude axis (dBm or dBc) |
| Harmonic markers | Auto-detect fundamental, label H2, H3, H4... with relative amplitude |
| FCC limit overlay | Show the -43dBc line (Part 97 requirement) |
| Pass/fail | Green/red indicator for each harmonic vs FCC limits |
| Band-aware | Select operating band, markers adjust to expected harmonic frequencies |
| Spurious search | Scan for non-harmonic spurs (intermod products, parasitic oscillations) |

```
Display in Harmonic Analyzer mode:

    0 ┤ ▓
  -10 ┤ ▓
  -20 ┤ ▓            ▓
  -30 ┤ ▓            ▓         ▓
  -40 ┤ ▓            ▓         ▓                    ← -43dBc FCC limit
  -50 ┤ ▓            ▓         ▓         ▓
  -60 ┤ ▓            ▓         ▓         ▓
      └─┴────────────┴─────────┴─────────┴────
        7MHz        14MHz     21MHz     28MHz
        FUND ✓      H2 ✓     H3 ✓      H4 ✓
       0 dBc     -22 dBc   -38 dBc   -51 dBc
```

## Complete Ham Radio Kit

```
Kit contents (~$55 total):
├── RF Bridge Board ($14)           → SWR + power measurement
├── HF Receiver Board ($10)         → Direct-conversion receiver
├── Auto-Tuner Board ($31)          → Automatic antenna matching
└── BNC-to-SO239 adapter ($2)       → Connect antenna

Firmware modules (no extra hardware):
├── Antenna Analyzer (SWR sweep, impedance, Smith chart)
├── Harmonic Analyzer (FFT with markers + FCC limits)
├── CW Decoder (Goertzel + Morse table)
├── FT8 Waterfall (FFT spectrogram)
└── Band-plan reference (frequency allocations)

Total: $70 scope + $55 kit = $125 for:
  ✓ Oscilloscope (50MHz, 2ch)
  ✓ Multimeter
  ✓ Signal generator
  ✓ Antenna analyzer
  ✓ Automatic antenna tuner
  ✓ Harmonic analyzer
  ✓ HF receiver
  ✓ CW decoder
  ✓ FT8 waterfall display

Buying these separately: $500-1000+
```

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
