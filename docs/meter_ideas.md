# Multimeter Feature Ideas

Brainstormed 2026-03-29. These are future feature candidates for the multimeter mode.

## Meter View Layouts ("Complications" concept)

Inspired by Apple Watch complications — let users choose what to display in different screen zones. Cycle between layouts with a button press.

1. **Full** — Large digits filling the screen. Classic DMM look. Current default.
2. **Chart** — Large reading on top, scrolling strip chart below (configurable sample rate: 0.25s / 0.5s / 1s). Great for watching voltage trends over time.
3. **Dual** — Split screen with two measurements (e.g., voltage top, current bottom).
4. **Stats** — Current value + live min/max/avg + reading histogram over time.

### Use Cases for Strip Chart
- Watching battery voltage during engine cranking
- Monitoring alternator output at idle vs load
- Tracking power supply stability under varying load
- Finding intermittent connections (wiggle test while watching voltage)
- Temperature sensor drift over time

## Calibration Mode

User-accessible calibration against known references:
- Connect precision voltage source (e.g., 5.000V) and store offset/gain correction
- Connect precision resistor (e.g., 1k 0.1%) for resistance calibration
- Store cal constants in SPI flash alongside factory trim
- Show "CAL" indicator when user calibration is active
- Not essential for $80 device, but differentiating for power users

## Measurement Enhancements

- **Relative / Delta mode** — Zero out current reading, show deviation from that point. Essential for finding small changes.
- **Hold / Auto-hold with measurement stack** — See "Auto-Hold Workflow" section below for full design.
- **Min/Max with timestamps** — Record not just the extreme values but when they occurred, for event correlation.
- **dB mode** — Express voltage as dBV or dBm relative to configurable reference. Useful for audio/RF.
- **Duty cycle / frequency counter** — Big stable readout in meter mode (scope hardware can measure this, just present it in meter UI).
- **Temperature** — K-type thermocouple input (if hardware supports it — needs investigation on real device).
- **True RMS indication** — Show whether AC measurement is true RMS or mean-responding.

## Auto-Hold Workflow

A hands-free, eyes-free measurement workflow using audio feedback, auto-hold, short-to-reset, and a measurement stack. Designed for situations where you can't see the device — arms in an engine bay, probing inside a panel, etc.

### Core Concept

1. Touch probes to test point
2. Reading stabilizes → **beep** (confirms hold)
3. Remove probes, touch them together (short) → **double beep** (pushes value to stack, resets for next measurement)
4. Repeat for multiple test points
5. Walk back to device, review all captured measurements

### Stability Detection

Declare "stable" when N consecutive readings (e.g., 8-10) fall within a tolerance band (~1% of each other). The band prevents jitter from causing false holds, while still capturing quickly when the reading settles.

### Short-to-Reset Gesture

Touching probes together produces an unambiguous signal:
- In voltage mode: ~0V (below a low threshold, e.g., <10mV)
- In resistance mode: near 0Ω (below ~1Ω)

This is distinct from any real measurement, so it's a safe reset gesture. The meter detects the short, pushes the current held value onto the stack, plays a double-beep, and re-arms for the next reading.

### Measurement Stack

Instead of discarding the held value on reset, push it onto a stack of up to 10 entries:

```
┌─────────────────────────────────────────┐
│ AUTO-HOLD          Stack: 3/10          │
├─────────────────────────────────────────┤
│                                         │
│          ► 4.98 V                       │
│            (measuring...)               │
│                                         │
├─────────────────────────────────────────┤
│  #3   12.02 V    0:42 ago              │
│  #2    5.01 V    1:15 ago              │
│  #1    3.31 V    1:48 ago              │
├─────────────────────────────────────────┤
│ Short probes to capture & advance       │
└─────────────────────────────────────────┘
```

Each entry stores: value, unit/mode, range, and timestamp. ~8 bytes per entry, 80 bytes total for 10 entries.

**Use cases:**
- Checking a voltage regulator circuit: input, output, reference, enable — 4 measurements together
- Verifying a wiring harness: check each pin at a connector (8-12 points)
- Battery cells in series: 4-6 cells in a pack
- Fuse box parasitic drain survey: measure voltage drop across 10 fuses, review which ones have current

### Audio Feedback (requires TIM8 buzzer)

| Event | Sound | Purpose |
|-------|-------|---------|
| Reading stabilizing | Quiet rising tone | "Almost there" |
| Hold captured | Firm single beep | "Got it, remove probes" |
| Short detected (push to stack) | Quick double beep | "Saved, ready for next" |
| Stack full (10/10) | Three descending tones | "Come back and review" |
| Scrolling stack entries | Soft click per entry | Navigation feedback |

The entire workflow is operable without ever looking at the screen.

## Component Verification Modes

### Resistor Color Band Calculator
- Show graphical resistor with color bands on screen
- User dials in expected value with UP/DOWN (or picks bands)
- Measure actual resistance
- Show PASS/FAIL with tolerance percentage
- Educational: teaches color code while verifying components

### Capacitor Verification
- Enter expected value from markings
- Measure actual capacitance
- Show deviation and ESR
- PASS/FAIL against standard tolerances (10%, 20%)

### Diode / LED Tester
- Forward voltage display with diode symbol
- Auto-detect silicon vs germanium vs LED vs Zener
- For LEDs: estimate color from Vf (red ~1.8V, green ~2.2V, blue ~3.0V)

### Continuity Improvements
- Visual indicator (screen flash or color bar) for noisy environments where buzzer can't be heard
- Faster response by using EXTI3 hardware interrupt rather than polling
- Adjustable threshold (default 50 ohm, configurable)
- Audible tone pitch proportional to resistance (lower R = higher pitch) — this is how Fluke meters work; your ears tell you "getting closer" without looking

### Parasitic Drain via Fuse Voltage Drop

Use automotive blade fuses as shunt resistors. Measure the millivolt drop across a fuse and calculate current from a built-in resistance lookup table.

**Fuse resistance table (typical values):**

| Rating | Typical Resistance | At 500mA draw |
|--------|-------------------|---------------|
| 5A | ~8 mΩ | 4.0 mV |
| 10A | ~5 mΩ | 2.5 mV |
| 15A | ~3 mΩ | 1.5 mV |
| 20A | ~2.5 mΩ | 1.25 mV |
| 30A | ~1.5 mΩ | 0.75 mV |

**UI:** User selects fuse type (Mini, ATC/ATO, Maxi, Low-profile Mini, Micro2) and rating (1A-40A) with arrow buttons. Firmware looks up typical resistance and displays calculated current.

```
┌─────────────────────────────────────────┐
│ PARASITIC DRAIN TEST          Fuse: 15A │
├─────────────────────────────────────────┤
│         Current:  340 mA                │
│    Voltage drop:  1.02 mV               │
│   Fuse resistance: 3.0 mΩ              │
├─────────────────────────────────────────┤
│ ▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░ │
│ 340mA                          [< 50mA] │
├─────────────────────────────────────────┤
│ Fuse: ◄ 15A ►    Type: ◄ Mini ATC ►    │
│ ⚠ DRAW EXCEEDS 50mA THRESHOLD          │
└─────────────────────────────────────────┘
```

**Resolution concern:** The voltage drops are in the microvolt-to-low-millivolt range. The oscilloscope ADC (8-bit, fast) won't resolve this. The multimeter ADC path (slower, higher resolution) might work, especially with heavy averaging — parasitic drain is DC, so we have unlimited averaging time. Needs validation on hardware.

**Combines with SPI flash data logging:** Start the test, close the car door, walk away for 30 minutes. The time-series log shows module sleep behavior, wake events, and final steady-state draw.

## Data Logging

- Record measurements to SPI flash at configurable intervals
- Export as CSV via USB
- Useful for long-term monitoring (battery discharge curves, temperature logging)
- Ring buffer with configurable depth

## Automotive-Specific

- Cranking voltage test (auto-trigger on voltage dip below threshold)
- Alternator ripple test (AC component of DC voltage)
- Injector pulse width measurement
- Sensor range validation (e.g., TPS should be 0.5-4.5V)
