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

### Trigger and MOVE

**MOVE** cycles what the arrow keys adjust (a popup names the stage):

| Stage | UP/DOWN | LEFT/RIGHT | SELECT |
|-------|---------|------------|--------|
| V/div (default) | V/div of the active channel | Timebase | Probe 1X/10X |
| Trig level | Trigger level (the dotted line) | Timebase | Probe 1X/10X |
| Position | V/div | Trigger point across the screen | Probe 1X/10X |
| Mask (only while a mask exists) | Mask vertical tolerance ±2 counts | Mask horizontal tolerance ±1 sample | Stop-on-fail on/off |

The trigger edge (Rising/Falling) is in Settings -> Oscilloscope Settings.
(Until 2026-09-22 this table said MOVE cycled the edge; it has not done so
since the trigger-level control landed.)

| Button | Action |
|--------|--------|
| **OK** | Toggle Run / Stop (or, while a mask failure is held, resume testing) |

### Mask pass/fail (time view)

| Button | Action |
|--------|--------|
| **AUTO** | No mask: teach one from the next 8 good captures. Teaching: cancel. Mask ready: turn it off |
| **MOVE** -> *Mask* | Adjust tolerances and stop-on-fail (table above) |
| **OK** | Resume after a held failure |

Every capture is then judged PASS / FAIL, or skipped with a reason (for example,
a setting changed since teaching). Dotted lines show the mask. Columns that
broke it turn red, and a strip along the bottom shows the verdict. The badge
row shows `P<n> F<n>`, `MASK skip: <why>`, or `FAIL ... HOLD-OK` while a
failure is held. Full design: `docs/specs/scope/mask-pass-fail.md`.

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
| **AUTO** | Auto-configure FFT (in FFT views; in the time view AUTO is the mask key) |

Note: Entering an FFT view claims the shared memory pool (88KB). Returning to time view releases it.

### Run Control

| Button | Action |
|--------|--------|
| **OK** | Toggle acquisition Run / Stop (shows popup) |

---

## Multimeter Mode

The four buttons directly under the screen are **softkeys**: the bar along the
bottom edge of the screen says what each one does, right above it, and its
current value. Softkeys arrived with the softkey UI (2026-10-06; spec
`docs/specs/platform/softkey-ui.md`).

### Softkeys

| Button | Big / Graph / Stats | Limits | Fuse |
|--------|---------------------|--------|------|
| **MOVE** | **Function**: next function | Function | **Fuse type**: ATO/ATC, Mini, Micro2, Maxi, J-Case |
| **SELECT** | **Reset**: min/max/avg, chart, histogram | **Low**: arrows move it; press again = set to the reading | **Rating** (next, wrapping; arrows step). In Scan: **Draw if >** presets 0.1 / 0.2 / 0.5 / 1 / 2 mV |
| **TRIGGER** | **Relative** on/off (zeros at the current reading). In Continuity: **Beep** threshold 10 / 30 / 50 / 100 Ohm | **High**, likewise | **Show**: Detail -> Table -> Scan -> Types |
| **PRM** | **View**: Big -> Graph -> Stats -> Limits -> Fuse | View | View |

### Other buttons

| Button | Action |
|--------|--------|
| **OK** | **Hold**: freeze the reading on screen now (HOLD badge); OK again = live |
| **LEFT / RIGHT** | Previous / next function (Fuse view: fuse type) |
| **UP / DOWN** | Limits view: move the highlighted limit. Fuse view: rating up/down (Scan: threshold ±0.1 mV) |
| **AUTO** | Automatic function select (DC V / AC V) |
| **SAVE** | Raw-frame debug overlay |

Changing function is instant: the reading shows dashes while the meter chip
switches in the background (~1.5 s in the `-meter` image). In the plain
`coldtrace` image only DC volts is measured; other functions show `---`.

### 11 Functions

```
0: DC Voltage       6: Resistance
1: AC Voltage       7: Continuity  (short = below the Beep threshold, default 30 Ohm)
2: DC Current (mA)  8: Diode Test
3: DC Current (A)   9: Capacitance
4: AC Current (mA) 10: Temperature
5: AC Current (A)
```

### 5 Views

- **Big** — the reading in 70 px digits, unit underneath, one line of min/max (or the relative reference, or SHORT/OPEN). Readable across the bench.
- **Graph** — compact reading, scrolling strip chart with an auto-scaled Y axis.
- **Stats** — min/max/avg/peak-to-peak and a histogram of the readings.
- **Limits** — pass/fail: a big PASS / FAIL box, the reading's position on a Low–High bar, pass and fail counts, and the last failing value. Entering it with a live reading sets Low/High to the reading ±5 %.
- **Fuse** — fuse current tester: probe the two metal tips on top of a fuse
  left in place, and it estimates the current from the voltage drop and the
  fuse's typical resistance (about ±10 %). Entering it switches to DC V.
  - **Detail**: a picture of the selected fuse, the current, a 0–150 mA bar
    with the 50 mA parasitic-draw mark, the drop and the fuse resistance.
  - **Table**: every rating of the type, largest at the top, with the current
    each would mean.
  - **Scan**: DRAW / NO DRAW against the threshold, for walking a fuse box
    with the engine off, plus the current if the fuse is 10/15/20/30 A
    (20/30/40/60 A for Maxi and J-Case).
  - **Types**: "which fuse is this?" — the five types to scale, front and top,
    and the common rating colours. The number on top of the fuse is what
    counts; colours vary.

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

### Top-Level Menu

```
> Oscilloscope Settings    [sub-menu]
  Sound and Light
  Auto Shutdown
  Display Mode             [cycles theme]
  Math / Persist           [sub-menu]
  Component Tester         [sub-menu]
  Bode Plot                [sub-menu]
  Startup on Boot
  About                    [info screen]
  FPGA SPI Scanner
  Firmware Update          [USB HID bootloader]
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

### Startup on Boot (OK/LEFT/RIGHT to cycle)

```
Scope <-> Meter
```

Fresh or erased settings default to `Meter`: the application starts in
Multimeter mode and configures the DMM frontend immediately after FPGA
initialization. Selecting `Scope` keeps the normal oscilloscope startup path.

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
| LEFT   | Timebase down    | Prev function    | Duty cycle down  |        -         |
| RIGHT  | Timebase up      | Next function    | Duty cycle up    |        -         |
| OK     | Run/Stop         | Hold             | Output on/off    | Enter / Toggle   |
| PRM    | Cycle FFT views  | Softkey: View    | Freq presets     |        -         |
| SELECT | Probe 1X/10X     | Softkey (Reset)  | Cycle waveform   | Cycle comp type  |
| MOVE   | Trigger edge     | Softkey (Func.)  |        -         |        -         |
| TRIGGER| Cursor mode      | Softkey (Rel.)   |        -         |        -         |
| AUTO   | FFT auto-config  | Auto function    |        -         |        -         |
| SAVE   | Screenshot       | Debug overlay    | Screenshot       | Screenshot       |
+--------+------------------+------------------+------------------+------------------+

Note: When cursors are active in Oscilloscope mode, UP/DOWN moves the cursor
and LEFT/RIGHT switches cursor selection (overrides V/div and timebase).
```
