# Inventory: every screen, control and readout

*Built 2026-10-05 from the code of the v0.4.1 release builds (main `d46c6e5`) plus the
EXP-72 switches. Code-read, not yet walked on the device. This is the map
the validation checklists are written from. Where it disagrees with
[`button_manual.md`](../button_manual.md), the code is what's described here. The
disagreements are listed in [findings.md](findings.md).*

## How to read this

**Builds.** Two release images, and the difference matters:

| Image | `make` target | What changes for a tester |
|---|---|---|
| `coldtrace` | `guest-coldtrace` | Meter reads **DC volts only**. LEFT/RIGHT in the meter relabels the function but doesn't switch the meter chip. |
| `coldtrace-meter` | `guest-coldtrace-meter` (adds `FPGA_METER_SUBMODES=1`) | Every meter function is commanded. Each function change takes ~1.4 s (up to ~4.5 s). |

Both images: boot always goes to Scope; the signal generator can't output; FFT is compiled in;
the FPGA scanner is compiled out.

**Status words** used in every table:

| Status | Meaning |
|---|---|
| **LIVE** | Does what it says, on real data |
| **MEASURED** | LIVE and checked against an independent reference (an EXP write-up) |
| **DEMO** | Draws canned or synthetic data, not a measurement |
| **INERT** | Can be selected or pressed, does nothing, and says nothing |
| **REFUSED** | Does nothing and says so (`n/a`, `not in this build`) |
| **HIDDEN** | Works, but isn't drawn |
| **DEAD** | Compiled in, but no screen reaches it |

## 1. Getting around

| Button | Everywhere |
|---|---|
| MENU | Next mode: **Scope → Meter → SigGen → Settings → Scope**. Inside a Settings sub-menu it goes back to the top level first. Saves pending settings. |
| POWER (hold) | "Hold to power off" with a 3-2-1 countdown; release to cancel; "Goodbye!" then off. |
| MENU + POWER | Reboots into the factory IAP drive, for firmware updates. Not in the manual. |
| UP/DOWN/LEFT/RIGHT (hold) | Auto-repeat after 500 ms, every 150 ms. No other button has a hold action. |

The status bar shows `SCOPE` / `METER` / `SIGGEN` / `SETUP`, battery %, `CHG` while charging,
and a flashing `LOW` below 3.3 V.

## 2. Scope

### 2.1 Views

| View | How to get there | Status |
|---|---|---|
| **Time** (split screen: CH1 on top, CH2 below; both always on) | Boot, or MENU from Settings | LIVE |
| Pre-capture demo (animated sine/square, "DEMO") | Until the first real capture, then never again | DEMO by design |
| **FFT** | PRM from Time | LIVE (CH1) |
| **Split** (CH1 trace above, FFT below) | PRM ×2 | LIVE |
| **Waterfall** | PRM ×3 | LIVE |
| **X-Y** | PRM ×4 (PRM again returns to Time) | LIVE. Not in the manual. |
| STOP | OK | Display-only (see 2.2) |
| Cursors | TRIGGER in Time | LIVE |
| Mask | AUTO in Time | MEASURED (EXP-69) |
| Math overlay | Settings → Math / Persist → Math Channel | **DEMO.** Synthetic data, not CH1/CH2, with no DEMO label. |
| Persistence overlay | Settings → Math / Persist → Persistence | **DEMO**, same |

### 2.2 Controls in the Time view

| Button | Does | Status |
|---|---|---|
| CH1 / CH2 | Makes that channel active and toggles **DC ↔ AC**. The relay is read back; a popup shows "CH1 AC" or "CH1 AC NOT SET". No GND. | MEASURED (EXP-70) |
| SELECT | Probe 1X ↔ 10X on the active channel; every volts readout ×10 | MEASURED (EXP-70) |
| UP / DOWN | Active channel's range 0..9 (popup "CH1 643mV/div") | LIVE. Ranges 5-7 measured; 4, 8, 9 marked `~`; 0-3 `--` |
| LEFT / RIGHT | Timebase code 0..20 (popup "H=2.56ms/div") | LIVE. Codes 14-20 measured, 13 `~`, 0-12 `--` |
| MOVE | Focus cycle: V/div+time → trigger level → position → mask tolerance (if a mask exists) | LIVE |
| …trigger-level stage, UP/DOWN | Level ±5 steps; popup "Trig +5 code 134"; dotted line moves | MEASURED (EXP-55/56) |
| …position stage, LEFT/RIGHT | Trigger x position ±16 px (8..312); ▼ marker | LIVE |
| …mask stage | UP/DOWN tolerance V ±2, LEFT/RIGHT tolerance H ±1, SELECT stop-on-fail | MEASURED (EXP-69) |
| AUTO | Mask: teach from 8 captures → "MASK ready" → AUTO again turns it off | MEASURED (EXP-69) |
| OK | RUN ↔ STOP. Releases a held mask fail ("MASK: RESUME"). **STOP only freezes the screen**: capture keeps running, and any button press redraws with the newest capture. | LIVE, partly (see findings) |
| TRIGGER | Cursors: Off → V → H → Both → Off | LIVE |
| PRM | Next view | LIVE |
| SAVE | Popup "SAVE: not in this build" | REFUSED |

**With cursors on**, UP/DOWN move the active cursor ±4 px (on a vertical cursor, UP moves it
*left*). LEFT and RIGHT both step to the next cursor. V/div, timebase and the MOVE stages
are unreachable until the cursors are off.

**SINGLE** captures once and holds. It re-arms only when the trigger mode changes; OK
doesn't re-arm it.

### 2.3 Controls in FFT / Split / Waterfall / X-Y

| Button | Does |
|---|---|
| SELECT | FFT window: Rect → Hann → Hamm → BHar → Flat |
| UP / DOWN | Reference level ±5 dB |
| LEFT / RIGHT | Zoom in / out (span halves or doubles) |
| AUTO | Zoom to ~12× the CH1 fundamental. Does nothing on a `--` timebase. |
| TRIGGER | Trigger mode Auto → Normal → Single (not in the manual) |
| PRM | Next view |

V/div and timebase can't be changed in these views. In X-Y, SELECT/arrows/AUTO act on the
hidden FFT state.

### 2.4 What's on the Time screen

| Element | Where | Notes |
|---|---|---|
| Info bar (bottom) | `CH1:<V/div> DC` · trigger mode · `H=<time/div>` · `CH2:<V/div> AC` | `~` = provisional, `--` = not calibrated |
| Badges, row 1 | Freq · Vpp · Vrms · Duty (CH1) | Volts on ranges 4-9, else `cnt`. `--` when unavailable. Refreshed on full redraws only. |
| Badges, row 2 | Per · CH2pp · legend/mask slot | |
| Trigger level | Dotted line on CH1, right-edge arrow + edge glyph | |
| Trigger position | ▼ at the top, where the trigger actually landed; hidden when free-running | |
| Top-left | `CH1`/`CH2` (active), `SPI3:OK n` | |
| Top-right | `Auto` / `Trig'd` / `Ready` / `STOP` | **Fixed text per mode**, not "did it trigger" |
| Ground markers `1` / `2` | Left edge | Both sit mid-screen; under autofit they don't mark 0 V |
| Mask strip | 3 px above row 2 | green pass, red fail column, amber skip/teach |

**Vertical scale** is autofit by default: each frame is fitted to its own min/max, but never
zoomed closer than 8 ADC counts. True scale is shell-only (`fpga scope graticule true`).

**Expected labels** (1X; ×10 at 10X), worked out from the calibration tables:

| Range | 0-3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|
| CH1 | `--` | `~415mV` | `643mV` | `1.26V` | `2.60V` | `~8.22V` | `~10.37V` |
| CH2 | `--` | `~279mV` | `617mV` | `1.23V` | `2.47V` | `~6.59V` | `~12.51V` |

| Timebase code | 0-12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|---|
| `H=` | `--` | `~259us` | `641us` | `1.28ms` | `2.56ms` | `6.41ms` | `12.83ms` | `25.59ms` | `63.97ms` |

A **fresh or erased unit** boots with CH1 at range 3 and timebase 0x08, so both CH1's V/div and
`H=` read `--` until changed.

**Refresh rate (EXP-72).** A triggered input refreshes once per fill + 202 ms (3.4/s at code
16). That's the FPGA design's hold, and no register changes it. A quiet input refreshes 33/s
(`fpga autolive`, default on after v0.4.1). v0.4.1 itself, and `fpga autolive off`, give 2.1/s.

## 3. Meter

### 3.1 Functions (LEFT/RIGHT, wraps around)

| # | Function | `coldtrace` | `coldtrace-meter` |
|---|---|---|---|
| 0 | DC V | MEASURED (EXP-23, EXP-71: ±1.615 V cell) | MEASURED (EXP-71) |
| 1 | AC V | label only | LIVE, untested. Shows `---` unless the input is 45-65 Hz. |
| 2-5 | DC mA, DC A, AC mA, AC A | label only | LIVE, untested |
| 6 | Resistance | label only | MEASURED (EXP-71: 100 Ω-1 MΩ within 1.5 %) |
| 7 | Continuity | label only | LIVE, untested. Beep/flash may never fire (findings). |
| 8 | Diode | label only | MEASURED (EXP-71: 1N4007, red LED) |
| 9 | Capacitance | label only | MEASURED (EXP-71: 61 nF, 228 µF) |
| 10 | Temperature | label only | LIVE, untested |

"label only" means the screen changes but the chip stays on DC V; non-DCV frames are refused
and the screen shows `---`.

### 3.2 Controls

| Button | Does | Status |
|---|---|---|
| LEFT / RIGHT | Function (see above); clears min/max/chart | LIVE |
| OK | Layout: **Full → Chart → Stats → Fuse** → Full | LIVE (manual lists 3) |
| TRIGGER | Auto-hold: grey `hold` → `HOLD` after 5 readings within 0.5 % | LIVE |
| SELECT | Reset min/max/avg, chart, histogram, hold | LIVE |
| AUTO | Automatic function pick between DC V and AC V (≥2.5 s per try). Manual says REL. | LIVE. REL is unreachable. |
| SAVE | Raw-frame debug overlay on/off | LIVE (debug) |
| UP / DOWN / PRM / CH1 / CH2 / MOVE | Nothing | — |

### 3.3 What's on screen

- **Main reading:** big digits; letters (`OL`, `ERR`, `CONT`) fall back to a smaller font. Then the unit (live), and the AC/DC tag (`AC ~` when mains is detected). `---` means no data or the wrong function. A leading `-` shows negative.
- **Full layout:** `<`/`>` arrows, a bar graph with a fixed full-scale label, and min/max/avg. A fixed "Range:" label, which comes from the function table, not the live range. A function index `N/10` (there are 11). For DC/AC V, the lower panel says "DMM waveform unavailable / Not in this firmware build". Continuity shows `SHORT`/`OPEN` and flashes green; diode shows `|>|`.
- **Chart layout:** strip chart 300×120, min/max, sample counter.
- **Stats layout:** MIN/MAX/AVG/P-P, sample count, a 20-bin histogram after 5 samples.
- **With HOLD or REL,** the reading shows 2 decimals only.

### 3.4 Fuse current tester (meter layout 4)

| Button | Does |
|---|---|
| MOVE (Fuse type) / LEFT / RIGHT | Fuse type: ATO/ATC, Mini, Micro2, Maxi, J-Case |
| SELECT (Rating) / UP / DOWN | Rating; in Scan the Draw-if threshold (UP = larger) |
| TRIGGER (Show) | Detail → Table → Scan → Types |
| PRM (View) | Next meter view |

Redesigned 2026-10-07 (softkey spec): converts the reading's own unit, switches the
meter to DC V on entry, draws in place. LIVE; seen on unit #1 by screenshot, current
not yet measured against a known load. Open: F47 (open-lead offset), F48 (resolution).

## 4. Signal generator

| Button | Does | Status |
|---|---|---|
| SELECT | Waveform: Sine, Square, Triangle, Sawtooth, FullRect, HalfRect, Pulse, Noise | preview only |
| UP / DOWN | Amplitude 0.1 / 0.2 / 0.5 / 1.0 / 2.0 / 3.3 Vpp | preview only |
| LEFT / RIGHT | Duty ±10 % (10-90 %) | preview only |
| PRM | Frequency preset 1 Hz → 10 → 100 → 1k → 10k → 25k | preview only |
| OK | Output on/off | **INERT in both images.** It stays "OUTPUT OFF" with no explanation. The output pin is the CH1 offset DAC, and releasing it would kill capture. |

The info bar always says "Sine 1.000kHz 3.3Vpp", whatever is selected. The "Offset 0.0 V"
row is fixed text.

## 5. Settings

UP/DOWN move (no wrap, no scroll indicator), OK selects, MENU goes back. The top level scrolls;
only 7 rows show at a time.

| Item | Values / action | Saved? | Status |
|---|---|---|---|
| Oscilloscope Settings › | sub-menu (below) | | |
| Sound and Light | — | stored, never used | **INERT**, no value shown |
| Auto Shutdown | — | stored, never used | **INERT** |
| Display Mode | Dark Blue / Classic Green / High Contrast / Night Red (OK/RIGHT forward, LEFT back) | yes | LIVE |
| Math / Persist › | sub-menu (below) | | |
| Component Tester › | screen (section 6) | | DEMO |
| Bode Plot › | screen (section 6) | | DEMO |
| Startup on Boot | Scope / Meter | yes (MCU flash, written on every change) | **INERT in release**: always boots Scope. The default shows "Meter". |
| About › | static text | | LIVE. Says "Custom Firmware v0.2". |
| FPGA SPI Scanner | — | | **INERT** (compiled out) |
| Firmware Update | DFU | | Resets via the old HID-bootloader handshake; under the factory bootloader probably just reboots (**confirm**). The working path is MENU+POWER. |
| Factory Reset | — | | **INERT**, no reset exists |

**Oscilloscope Settings** (OK cycles, LEFT/RIGHT ±1):

| Row | Values | Status |
|---|---|---|
| CH1 Coupling | DC / AC | MEASURED (EXP-70) |
| CH1 Probe | 1X / 10X | MEASURED (EXP-70) |
| CH1 20M Limit | `n/a` | REFUSED |
| CH2 Coupling / Probe / 20M | same | same |
| Trigger Mode | Auto / Normal / Single | LIVE |
| Trigger Edge | Rising / Falling | **HIDDEN**: 8 rows, 7 fit, no scrolling. Selectable blind. |

**Math / Persist** (OK only): Math Channel Off → A+B → A−B → A×B → −A → −B; Persistence
On/Off; Back. Both overlays are DEMO (section 2.1). Math is saved across power cycles;
persistence isn't.

**Saved across a power cycle:** V/div, coupling, probe, timebase, trigger
mode/edge/level, theme, math, meter function and layout. **Not saved:** RUN/STOP, cursors,
trigger position, persistence, FFT view, siggen, fuse settings, mask. Writes happen 2 s
after a change, on the next button press, and on MENU, power-off or MENU+POWER.

## 6. Tool screens (from Settings)

| Screen | Controls | Status |
|---|---|---|
| **Component Tester** | SELECT: Resistor → Capacitor → Diode → Continuity. OK opens the Resistor Calculator (hint says "OK:Back"). | **DEMO.** Fixed values: 9.96 Ω PASS, "2.0 F" (font has no `u`), 0.644 V, 0.01 Ω SHORT. |
| **Resistor Calculator** | LEFT/RIGHT band, UP/DOWN colour, SELECT "measure" (expected × 1.02), OK back | LIVE as a calculator. "Measured" is simulated but labelled as measured. Default reads 47k, not 4.7k; Violet multiplier ×0.1 (findings). |
| **Bode Plot** | OK/MENU back | **DEMO** ("Demo: RC LP 1kHz") |

## 7. System behaviour

| Behaviour | How to observe | Status |
|---|---|---|
| Boot | Splash ~750 ms → Scope with a brief DEMO trace → live | LIVE |
| Power off | Hold POWER 3 s | LIVE. On USB power it may hang on "Goodbye!" (Exp R: USB keeps the rail up). |
| Charging | Plug USB: `CHG` after ~5 s | LIVE |
| Low battery | `LOW` below 3.3 V for 10 s; critical shutdown after 30 status-bar draws | LIVE. Counts draws, not seconds; doesn't save settings (findings). |
| Auto power-off on idle | — | doesn't exist |
| Backlight | on/off only | no brightness control |
| Buzzer | continuity only | no key clicks, no mute setting |
| Screenshot | USB only: `scripts/screenshot.py` (needs a held screen; 16-colour palette, so don't judge theme contrast from it) | LIVE |
| Firmware update | MENU+POWER → IAP drive; or USB `fwload` + `fwapply` (from v0.4.1) | LIVE |
| Fault screen | "SYSTEM FAULT". In release the watchdog isn't started, so most faults freeze rather than restart. | — |

## 8. Compiled in but unreachable

Trend plot, roll mode, the standalone X-Y module (`xy_mode.c`; the X-Y *view* in 2.1 is
separate), the UART / I2C / SPI / CAN / K-line decoders, the alternator and compression tests,
and the repo's `modules/*.json` procedures (no loader). The FFT AVG/MH indicators can never
turn on. There are no controls for: trigger source CH2, channel on/off, vertical position. The
checklist records these as absent, not as failures.

## 9. Shell-only features (USB, `scripts/bench.py` or a terminal)

`fpga scope graticule true|auto` (true scale), `fpga scope center`, `fpga scope softtrig`,
`mask …` (numeric tolerances, per-channel, stats), `trig2` (CH2 offset), `screen dumpbin`
(screenshot), `btn <name>` (inject a press), `settings`, `mode startup`, `version`.
EXP-72: `fpga autolive on|off` (default on), `fpga holdlog` (per-handover interval log).
