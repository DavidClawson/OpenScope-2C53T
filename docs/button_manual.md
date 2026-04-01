# OpenScope 2C53T - Button & Navigation Manual

Physical buttons: **MENU**, **AUTO**, **SAVE**, **CH1**, **CH2**, **PRM**, **SELECT**, **UP**, **DOWN**, **LEFT**, **RIGHT**, **OK**, **MOVE**

Emulator keys: `M`=MENU, `A`=AUTO, `S`=SAVE, `1`=CH1, `2`=CH2, `P`=PRM, `Space`=SELECT, Arrows, `Enter`=OK

---

## Mode Switching

The device has 4 modes, cycled by pressing **MENU**:

```
Oscilloscope  -->  Multimeter  -->  Signal Generator  -->  Settings
      ^                                                       |
      +-------------------------------------------------------+
```

If you're inside a Settings sub-menu, **MENU** goes back to the top-level Settings list first, then cycles to Oscilloscope on the next press.

---

## Oscilloscope Mode

### Channel Controls

| Button | Action |
|--------|--------|
| **CH1** | Select CH1 as active channel + cycle coupling (DC -> AC -> GND) |
| **CH2** | Select CH2 as active channel + cycle coupling (DC -> AC -> GND) |
| **SELECT** | Cycle probe setting (1X -> 10X) for active channel |

### Vertical / Horizontal

| Button | Action |
|--------|--------|
| **UP** | Increase V/div for active channel (zoom out vertically) |
| **DOWN** | Decrease V/div for active channel (zoom in vertically) |
| **LEFT** | Decrease timebase (faster sweep, zoom in horizontally) |
| **RIGHT** | Increase timebase (slower sweep, zoom out horizontally) |

V/div steps: 5mV, 10mV, 20mV, 50mV, 100mV, 200mV, 500mV, 1V, 2V, 5V

Timebase steps: 5ns, 10ns, 20ns, 50ns, 100ns, 200ns, 500ns, 1us, 2us, 5us, 10us, 20us, 50us, 100us, 200us, 500us, 1ms, 2ms, 5ms, 10ms, 20ms

### Trigger

| Button | Action |
|--------|--------|
| **MOVE** | Cycle trigger edge (Rising -> Falling) |
| **OK** | Toggle Run / Stop |

### Cursors

| Button | Action |
|--------|--------|
| **TRIGGER** | Cycle cursor mode: Off -> Vertical -> Horizontal -> Both -> Off |
| **UP/DOWN** | Move active cursor |
| **LEFT/RIGHT** | Switch between cursor pairs (V1/V2 or H1/H2) |

When cursors are active, UP/DOWN/LEFT/RIGHT control cursors instead of V/div and timebase. The cursor readout shows delta-time (dt), frequency (1/dt), and delta-voltage (dV).

### FFT / Spectrum Views

| Button | Action |
|--------|--------|
| **PRM** | Cycle view: Time -> FFT -> Split -> Waterfall -> Time |
| **SELECT** | Cycle FFT window function (in FFT/Split/Waterfall views) |
| **UP/DOWN** | Adjust reference level (+/- 5dB, in FFT views) |
| **LEFT/RIGHT** | Zoom in/out frequency range (in FFT views) |
| **AUTO** | Auto-configure FFT (generate test signal + optimize) |

Note: Entering an FFT view claims the shared memory pool (88KB). Returning to time view releases it.

### Run Control

| Button | Action |
|--------|--------|
| **OK** | Toggle acquisition Run / Stop (shows popup) |

---

## Multimeter Mode

### Sub-mode Navigation

| Button | Action |
|--------|--------|
| **LEFT** | Previous meter sub-mode |
| **RIGHT** | Next meter sub-mode |
| **OK** | Cycle layout: Full -> Chart -> Stats -> Full |
| **AUTO** | Toggle relative/delta mode (REL) — zeros at current reading, shows deviation |
| **TRIGGER** | Toggle auto-hold (HOLD) — freezes display when reading stabilizes |
| **SELECT** | Reset min/max/avg tracking (also resets chart and histogram) |

### 10 Sub-modes

```
0: DC Voltage       5: AC Current (A)
1: AC Voltage       6: Resistance
2: DC Current (mA)  7: Continuity
3: DC Current (A)   8: Diode Test
4: AC Current (mA)  9: Capacitance
```

### 3 Display Layouts

- **Full** — Large digits, bar graph, min/max/avg, range info. Classic DMM.
- **Chart** — Compact reading on top, scrolling strip chart below with auto-scaling Y axis. Shows measurement trend over time.
- **Stats** — Compact reading, min/max/avg/peak-to-peak statistics, and histogram of reading distribution.

---

## Signal Generator Mode

### Waveform & Output

| Button | Action |
|--------|--------|
| **SELECT** | Cycle waveform type |
| **OK** | Toggle output ON / OFF |

### Waveforms

```
Sine -> Square -> Triangle -> Sawtooth -> Full-Rect Sine -> Half-Rect Sine -> Pulse -> Noise
```

### Parameters

| Button | Action |
|--------|--------|
| **UP** | Increase amplitude (+0.1 Vpp) |
| **DOWN** | Decrease amplitude (-0.1 Vpp) |
| **LEFT** | Decrease duty cycle (-5%) |
| **RIGHT** | Increase duty cycle (+5%) |
| **PRM** | Cycle frequency presets: 1 -> 10 -> 100 -> 1k -> 10k -> 25k -> 1 Hz |

---

## Settings Mode

### Top-Level Menu (9 items)

```
> Oscilloscope Settings    [sub-menu]
  Sound and Light
  Auto Shutdown
  Display Mode             [cycles theme]
  Math / Persist           [sub-menu]
  Component Tester         [sub-menu]
  Startup on Boot
  About                    [info screen]
  Factory Reset
```

### Navigation

| Button | Action |
|--------|--------|
| **UP** | Move selection up |
| **DOWN** | Move selection down |
| **OK** | Enter sub-menu or toggle selected option |
| **MENU** | Back to top-level (from sub-menu) or cycle to Oscilloscope mode |

### Display Mode (OK to cycle)

```
Dark Blue -> Classic Green -> High Contrast -> Night Red -> Dark Blue
```

### Oscilloscope Settings Sub-menu (depth 1)

```
> CH1 Coupling      [DC / AC / GND]
  CH1 Probe         [1X / 10X]
  CH1 20M Limit     [ON / OFF]
  CH2 Coupling      [DC / AC / GND]
  CH2 Probe         [1X / 10X]
  CH2 20M Limit     [ON / OFF]
  Trigger Mode      [Auto / Normal / Single]
  Trigger Edge      [Rising / Falling]
```

Press **OK** on any item to cycle its value. **MENU** to go back.

### Math / Persist Sub-menu (depth 3)

```
> Math Channel      [Off / A+B / A-B / A*B / A/B / Max / Min]
  Persistence       [On / Off]
  Back
```

- **Math Channel**: OK cycles through math operations, then back to Off
- **Persistence**: OK toggles on/off. Enabling persistence claims the shared memory pool (evicts FFT if active)

### Component Tester (depth 4)

| Button | Action |
|--------|--------|
| **SELECT** | Cycle component type |
| **OK** | Enter Resistor Calculator |

### Resistor Color Band Calculator (depth 5)

Interactive graphical resistor with 4 color bands (Digit 1, Digit 2, Multiplier, Tolerance).

| Button | Action |
|--------|--------|
| **LEFT/RIGHT** | Move between bands |
| **UP/DOWN** | Change selected band's color |
| **SELECT** | Simulate measurement and show pass/fail |
| **OK** | Back to Component Tester |

Calculates expected resistance from color bands, then compares against measured value showing deviation percentage and PASS/FAIL against the selected tolerance.

### About Screen (depth 2)

Shows device info, pool status, heap free, license. **MENU** or **OK** to go back.

---

## Quick Reference Card

```
+--------+------------------+------------------+------------------+------------------+
|  Key   |   Oscilloscope   |    Multimeter    |    Signal Gen    |     Settings     |
+--------+------------------+------------------+------------------+------------------+
| MENU   | -> Multimeter    | -> Signal Gen    | -> Settings      | Back / -> Scope  |
| CH1    | Coupling DC/AC   |        -         |        -         |        -         |
| CH2    | Coupling DC/AC   |        -         |        -         |        -         |
| UP     | V/div up         |        -         | Amplitude up     | Selection up     |
| DOWN   | V/div down       |        -         | Amplitude down   | Selection down   |
| LEFT   | Timebase down    | Prev sub-mode    | Duty cycle down  |        -         |
| RIGHT  | Timebase up      | Next sub-mode    | Duty cycle up    |        -         |
| OK     | Run/Stop         | Cycle layout     | Output on/off    | Enter / Toggle   |
| PRM    | Cycle FFT views  |        -         | Freq presets     |        -         |
| SELECT | Probe 1X/10X     | Reset min/max    | Cycle waveform   | Cycle comp type  |
| MOVE   | Trigger edge     |        -         |        -         |        -         |
| TRIGGER| Cursor mode      | Toggle HOLD      |        -         |        -         |
| AUTO   | FFT auto-config  | Toggle REL       |        -         |        -         |
| SAVE   | Screenshot       | Screenshot       | Screenshot       | Screenshot       |
+--------+------------------+------------------+------------------+------------------+

Note: When cursors are active in Oscilloscope mode, UP/DOWN moves the cursor
and LEFT/RIGHT switches cursor selection (overrides V/div and timebase).
```
