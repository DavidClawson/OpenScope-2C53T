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
- **Hold / Auto-hold** — Freeze display on stable reading. Auto-hold captures first stable value and beeps.
- **Min/Max with timestamps** — Record not just the extreme values but when they occurred, for event correlation.
- **dB mode** — Express voltage as dBV or dBm relative to configurable reference. Useful for audio/RF.
- **Duty cycle / frequency counter** — Big stable readout in meter mode (scope hardware can measure this, just present it in meter UI).
- **Temperature** — K-type thermocouple input (if hardware supports it — needs investigation on real device).
- **True RMS indication** — Show whether AC measurement is true RMS or mean-responding.

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
- Audible tone pitch proportional to resistance (lower R = higher pitch)

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
