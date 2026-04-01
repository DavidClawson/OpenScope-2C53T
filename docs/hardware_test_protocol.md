# Hardware Test Protocol

First-flash test checklist for the OpenScope 2C53T custom firmware on real hardware. Designed to verify each major subsystem with minimal equipment.

## Equipment Needed

**You have:**
- OpenScope 2C53T (the device itself)
- Arduino (any model — Uno, Nano, etc.)
- HiLetgo USB logic analyzer (FX2LP-based, 8ch)
- Scope probes (included with 2C53T)
- Multimeter leads (included with 2C53T)
- USB cable for firmware flashing
- A battery or bench power supply (or just the device's own battery)

**Nice to have but not required:**
- Known resistors (e.g., 1k, 4.7k, 10k — a basic resistor kit)
- Known capacitor (e.g., 100nF ceramic)
- A diode (1N4148 or any common silicon diode)
- Jumper wires / breadboard
- SWD debugger (ST-Link, J-Link, or Black Magic Probe)

---

## Phase 0: Pre-Flash (Original Firmware Baseline)

Before flashing custom firmware, capture some reference data with the stock firmware.

- [ ] Power on device, note firmware version on splash/about screen
- [ ] Take photos of each mode screen (scope, meter, siggen, settings)
- [ ] Measure a known voltage (e.g., AA battery ~1.5V) with the multimeter — note reading
- [ ] Run the signal generator at 1kHz sine, probe the output with CH1 — screenshot
- [ ] Note any bugs or quirks you observe (for comparison later)

**Why:** This gives you a baseline. If something doesn't work in custom firmware, you can check whether it worked in stock.

---

## Phase 1: Boot & Display (No Probes)

First power-on with custom firmware. Nothing connected, just verify the basics.

### 1.1 Boot sequence
- [ ] Device powers on without hanging
- [ ] Splash screen appears ("OpenScope 2C53T")
- [ ] Splash clears after ~2 seconds
- [ ] Scope screen draws with grid, status bar, info bar
- [ ] Uptime counter increments in status bar (shows time ticking)

### 1.2 Mode switching (MENU button)
- [ ] MENU cycles: Scope → Meter → SigGen → Settings → Scope
- [ ] Each mode screen draws correctly (no garbage pixels, no freezing)
- [ ] Status bar updates mode name (SCOPE / METER / SIGGEN / SETTINGS)

### 1.3 Theme switching
- [ ] Settings > Display Mode > OK cycles themes
- [ ] Verify at least 2 themes render correctly (Dark Blue, Classic Green)
- [ ] Return to scope mode — theme persists

### 1.4 Settings navigation
- [ ] UP/DOWN navigates settings menu (highlight moves)
- [ ] OK enters sub-menus (Oscilloscope Settings, Math/Persist, Component Tester, About)
- [ ] MENU returns from sub-menus
- [ ] About screen shows device info, heap free, pool status

**Pass criteria:** Device boots, all 4 modes render, navigation works, no crashes.

---

## Phase 2: Multimeter (Battery Test)

Simplest real-world measurement — use the built-in DMM to measure a battery.

### 2.1 DC Voltage
- [ ] Switch to Multimeter mode
- [ ] Plug multimeter leads into COM and V/Ω/C jacks
- [ ] Measure a fresh AA battery (expect ~1.5V)
- [ ] Reading displayed on screen (verify it's in the right ballpark)
- [ ] LEFT/RIGHT cycles through sub-modes (all 10 render without crash)

### 2.2 Meter layouts
- [ ] OK cycles: Full → Chart → Stats → Full
- [ ] Chart layout shows strip chart updating
- [ ] Stats layout shows min/max/avg values

### 2.3 Meter features
- [ ] AUTO toggles REL indicator (shows "REL" badge)
- [ ] TRIGGER toggles HOLD (shows "hold" then "HOLD" when stable)
- [ ] SELECT resets min/max/avg

### 2.4 Resistance
- [ ] Switch to Resistance mode (LEFT/RIGHT to sub-mode 6)
- [ ] Measure a known resistor (e.g., 4.7kΩ) — verify reading is reasonable
- [ ] Touch leads together — should show near 0Ω

### 2.5 Continuity
- [ ] Switch to Continuity mode (sub-mode 7)
- [ ] Touch leads together — buzzer should sound AND screen should flash green
- [ ] Separate leads — "OPEN" displayed, no flash

### 2.6 Diode
- [ ] Switch to Diode mode (sub-mode 8)
- [ ] Measure a silicon diode forward — expect ~0.6V reading
- [ ] Reverse — should show "OL" or very high reading

**Pass criteria:** DMM reads a battery voltage within ±10% of expected. Resistance, continuity, and diode modes respond to real components.

---

## Phase 3: Oscilloscope (Self-Test with Signal Generator)

Use the device's own signal generator to test the scope — no external equipment needed.

### 3.1 Signal generator output
- [ ] Switch to SigGen mode
- [ ] OK to enable output (should show "OUTPUT ON")
- [ ] PRM to set frequency to 1kHz
- [ ] SELECT to cycle waveforms (sine, square, triangle)

### 3.2 Scope self-test (loopback)
- [ ] Connect CH1 probe to signal generator output (BNC or banana depending on hardware)
- [ ] Switch to Scope mode
- [ ] Should see waveform on screen
- [ ] Verify shape matches expected (sine = smooth curve, square = flat tops)
- [ ] Verify frequency readout shows ~1kHz in measurement badges

### 3.3 Scope controls
- [ ] UP/DOWN adjusts V/div (popup shows value, waveform scales)
- [ ] LEFT/RIGHT adjusts timebase (popup shows value, waveform stretches/compresses)
- [ ] CH1 cycles coupling (DC → AC → GND — popup confirms)
- [ ] OK toggles Run/Stop (waveform freezes when stopped)
- [ ] Verify: changing V/div or timebase while stopped redraws the frozen waveform

### 3.4 Dual channel
- [ ] Connect CH2 probe to same signal gen output (or use a jumper wire)
- [ ] Both CH1 and CH2 traces should be visible
- [ ] CH2 button cycles coupling independently

### 3.5 Trigger
- [ ] MOVE cycles trigger edge (Rising/Falling)
- [ ] Verify waveform stabilizes differently with each edge
- [ ] Trigger status badge shows "Auto" / "Trig'd"

### 3.6 Cursors
- [ ] TRIGGER button cycles cursor mode (Off → Vertical → Horizontal → Both)
- [ ] UP/DOWN moves active cursor
- [ ] LEFT/RIGHT switches cursor selection
- [ ] Readout shows delta-t, frequency, delta-V values

### 3.7 FFT (if working)
- [ ] PRM cycles view: Time → FFT → Split → Waterfall
- [ ] FFT view should show a spike at 1kHz (from signal gen)
- [ ] PRM returns to Time view

### 3.8 Screenshot
- [ ] SAVE button shows "SAVED #1" popup (no crash)

**Pass criteria:** Signal generator produces visible waveform on scope. Controls adjust display correctly. FFT shows expected frequency peak.

---

## Phase 4: Arduino Test Signals

Use an Arduino to generate known signals for more precise testing.

### 4.1 Arduino setup
Upload a simple sketch that outputs test signals:

```cpp
// Arduino test signal generator for OpenScope validation
void setup() {
    Serial.begin(9600);       // UART at 9600 baud
    pinMode(3, OUTPUT);        // PWM output (490Hz or 980Hz)
    pinMode(13, OUTPUT);       // 1Hz blink
    analogWrite(3, 128);       // 50% duty cycle PWM
}

void loop() {
    // 1Hz blink on pin 13
    digitalWrite(13, HIGH);
    Serial.println("HELLO");   // UART test data
    delay(500);
    digitalWrite(13, LOW);
    Serial.println("WORLD");
    delay(500);
}
```

### 4.2 PWM measurement
- [ ] Connect CH1 to Arduino pin 3
- [ ] Scope should show ~490Hz square wave (PWM)
- [ ] Adjust timebase until you see clean square wave
- [ ] Frequency badge should read ~490Hz
- [ ] Duty cycle badge should read ~50%

### 4.3 Low-frequency measurement
- [ ] Connect CH1 to Arduino pin 13
- [ ] Set timebase to slow (200ms/div or similar)
- [ ] Should see 1Hz square wave (0.5s high, 0.5s low)

### 4.4 UART decode (future — when protocol decode UI is wired)
- [ ] Connect CH1 to Arduino TX (pin 1)
- [ ] If UART decode is active, should show decoded "HELLO" / "WORLD"
- [ ] (Skip if decode UI not yet wired to display)

### 4.5 I2C test (optional, if you have an I2C sensor)
Connect an I2C device (e.g., MPU6050, BMP280, or any I2C sensor) to the Arduino, then probe SDA/SCL:
- [ ] CH1 on SCL, CH2 on SDA
- [ ] Should see clock and data waveforms
- [ ] If I2C decode is active, should show addresses and data

**Pass criteria:** Arduino-generated signals display correctly with expected frequency and duty cycle.

---

## Phase 5: Logic Analyzer Cross-Check

Use the HiLetgo USB logic analyzer with sigrok/PulseView on your computer to verify the scope's measurements.

### 5.1 Setup
- [ ] Connect logic analyzer to same Arduino signals (pin 3 PWM, pin 1 TX)
- [ ] Open PulseView, set sample rate to 1MHz
- [ ] Capture same signals the scope is displaying

### 5.2 Compare
- [ ] PWM frequency: scope reading vs PulseView measurement (should agree within 1%)
- [ ] PWM duty cycle: scope vs PulseView
- [ ] UART baud rate: scope vs PulseView (both should detect 9600)

**Pass criteria:** Scope measurements agree with logic analyzer within reasonable tolerance.

---

## Phase 6: Stress Test

### 6.1 Rapid button mashing
- [ ] Press buttons rapidly in all modes for 30 seconds
- [ ] No crashes, no freezes, no display corruption
- [ ] Watchdog does not trigger (no red fault screen)

### 6.2 Mode cycling under load
- [ ] With signal gen output active and CH1 connected
- [ ] Rapidly cycle: Scope → FFT → Meter → SigGen → Settings → Scope
- [ ] All mode transitions clean, no stale pixels

### 6.3 Long-duration run
- [ ] Leave scope running with signal gen loopback for 10+ minutes
- [ ] Verify no drift, no crashes, uptime counter keeps incrementing
- [ ] Check About screen: heap free should be stable (not decreasing)

**Pass criteria:** No crashes or watchdog resets during stress testing.

---

## Phase 7: Component Tester Verification

### 7.1 Resistor
- [ ] Settings > Component Tester
- [ ] Connect a known resistor (e.g., 4.7kΩ)
- [ ] Should show measured value with PASS/FAIL
- [ ] OK enters Resistor Calculator — UP/DOWN/LEFT/RIGHT navigate color bands

### 7.2 Capacitor
- [ ] SELECT to switch to Capacitor mode
- [ ] Measure a known capacitor (e.g., 100nF ceramic)
- [ ] Should show measured value and ESR

### 7.3 Diode
- [ ] SELECT to switch to Diode mode
- [ ] Measure forward voltage of a silicon diode
- [ ] Should show ~0.6V with PASS status

**Pass criteria:** Component readings are in the right ballpark for known parts.

---

## Results Log

| Test | Phase | Result | Notes |
|------|-------|--------|-------|
| Boot & splash | 1.1 | | |
| Mode switching | 1.2 | | |
| Theme switching | 1.3 | | |
| Settings navigation | 1.4 | | |
| DC Voltage (battery) | 2.1 | | Expected: ~1.5V |
| Meter layouts | 2.2 | | |
| REL / HOLD | 2.3 | | |
| Resistance | 2.4 | | |
| Continuity | 2.5 | | |
| SigGen output | 3.1 | | |
| Scope loopback | 3.2 | | |
| Scope controls | 3.3 | | |
| Dual channel | 3.4 | | |
| Trigger | 3.5 | | |
| Cursors | 3.6 | | |
| FFT | 3.7 | | |
| Screenshot | 3.8 | | |
| Arduino PWM | 4.2 | | Expected: ~490Hz, 50% |
| Arduino 1Hz | 4.3 | | |
| Logic analyzer compare | 5.2 | | |
| Button stress | 6.1 | | |
| Mode cycling | 6.2 | | |
| Long-duration run | 6.3 | | |
| Component: resistor | 7.1 | | |
| Component: capacitor | 7.2 | | |
| Component: diode | 7.3 | | |

## Notes

- **Phase 0** (stock firmware baseline) is optional but highly recommended — gives you a reference point.
- **Phase 1-3** are the critical tests — if these pass, the firmware is fundamentally working.
- **Phase 4-5** provide precision validation against external instruments.
- **Phase 6-7** test robustness and secondary features.
- If anything fails, note the exact step, take a screenshot (SAVE button or phone camera), and document what you see vs what you expected.
- SWD debugger connection enables live debugging if anything goes wrong — highly recommended to solder the SWD pads before starting.
