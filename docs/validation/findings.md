# Findings from the inventory (2026-10-05)

Building the [inventory](inventory.md) meant reading every screen's code. That turned up
defects before anyone pressed a button. Each one below is a **prediction from code** until a
checklist run confirms it on a device. Mark it `confirmed`, `not reproduced` or `fixed` as
runs come in.

**Confidence:** **read** = re-checked directly in the code (the line is unambiguous).
**predicted** = follows from rendering or timing order, so it needs eyes on the device.

**Severity:**
- **S1** misleads the user about a measurement.
- **S2** a control or screen doesn't do what it says.
- **S3** cosmetic or legibility.
- **D** documentation only.

## A. Shows something that isn't real

| # | Sev | Finding | Conf | Where |
|---|---|---|---|---|
| F01 | S1 | **Math overlay draws a built-in sine/square, not CH1/CH2.** No DEMO label, and the setting is saved, so it comes back after a reboot. | read | `scope_ui.c:1645-1681` |
| F02 | S1 | **Persistence overlay is fed a synthetic sine**, not captures | read | `scope_ui.c:1602-1642` |
| F03 | S1 | **Component Tester shows fixed demo results** (9.96 Ω PASS, 0.644 V …) with a PASS badge and no DEMO label | read | `component_ui.c:140-172` |
| F04 | S1 | Resistor Calculator "Measured:" is expected × 1.02, labelled as a measurement | read | `resistor_calc_ui.c:148-154, 305-309` |
| F05 | S1 | Top-right trigger badge is fixed text per mode. "Trig'd" in NORMAL even when nothing triggered. | read | `scope_ui.c:249-260` |
| F06 | S2 | **STOP only freezes the display.** Capture keeps running, and any button press while stopped redraws with the newest capture. | read | `fpga.c` (acq loop never reads `running`) |
| F07 | S2 | Startup on Boot does nothing in release (always Scope), shows "Meter" by default, and erases MCU flash on every change | read | `main.c:1032-1033, 126-163` |
| F08 | S2 | Signal generator OK does nothing and stays "OUTPUT OFF" with no explanation | read | `signal_gen.c:269-277` |
| F09 | S2 | SigGen info bar always says "Sine 1.000kHz 3.3Vpp"; "Offset 0.0 V" is fixed text | read | `status_bar.c:174-178`, `siggen_ui.c:126-129` |
| F10 | S1 | Meter "Range:" label and bar-graph full scale are fixed per function, not the live range. Stats mix Ω with kΩ and V with mV. | predicted | `meter_ui.c:66-89, 1163`; `meter_data.c:312-324` |
| F11 | S1 | Fuse tester treats the reading as mV whatever the unit (1000× low on a V reading). Detail view prints the unit twice ("mA mA"). | read | `meter_ui.c:1405-1408`, `fuse_ui.c:178-182` |
| F12 | S2 | Continuity may never beep, flash or show SHORT. Shorted leads arrive as a plain number (~0.17 Ω), which isn't treated as a short. | predicted | `meter_ui.c:741-777`, `meter_data.c:1667-1670` |
| F13 | S2 | Settings → Firmware Update uses the retired HID-bootloader handshake; under the factory bootloader it probably just reboots | predicted | `dfu_boot.c:16-30` |

## B. Works but isn't drawn

| # | Sev | Finding | Conf | Where |
|---|---|---|---|---|
| F14 | S2 | **Trigger Edge row is never drawn** in Oscilloscope Settings: 8 rows, 7 fit, no scrolling. Changeable blind. | read | `settings_ui.c:56, 63, 184-185` |
| F15 | S2 | Popups render only in the scope Time view. Meter AUTO's "AUTO DMM/CANCEL/ERR" and SAVE's message elsewhere are invisible, then appear stale on the next Time view. A pending popup in FFT views forces 20 Hz full redraws. | read / predicted | `scope_ui.c:104-119, 750-777`; `main.c:695` |
| F16 | S2 | Cursor readouts (dt, 1/dt, dV) are probably painted over by the measurement badges | predicted | `scope_ui.c:1517-1593` vs layer 10 at `2242` |
| F17 | S3 | Labels inside the live trace area (`CH1`, `SPI3:`, Auto/Trig'd, trigger arrow, ground markers, MATH) vanish on the next live frame | predicted | `scope_ui.c:2299-2302, 2384-2468` |
| F18 | S3 | Measurement badges update only on full redraws (button press). Known trade-off. | read | `scope_ui.c:2472-2493` |
| F19 | S3 | No STOP indicator in the FFT views; they just freeze | predicted | `main.c:643` |

## C. Text and fonts

| # | Sev | Finding | Conf | Where |
|---|---|---|---|---|
| F20 | S3 | 14 popup strings are wider than the 200 px popup box (e.g. "UP/DN: V/div  LT/RT: Time" 284 px, "SAVE: not in this build" 231 px) | predicted (measured from font widths) | `scope_ui.c:84-87`; strings in `input_handler.c` |
| F21 | S3 | The large-digit font only has `0-9 . - + : V A k m M H z W F O`. Component Tester "2.0uF" draws as "2.0 F"; fuse Scan "NO DRAW"/"DRAW!" lose most letters. | read | `font_xlarge.c` header; `component_ui.c:198`; `fuse_ui.c` |
| F22 | S3 | About says "Custom Firmware v0.2"; no build ID on the device | read | `settings_ui.c:207` |
| F23 | S3 | About's "MENU to go back" overlaps the info bar text | predicted | `settings_ui.c:234-236` |
| F24 | S3 | Meter function index reads "N/10" for 11 functions (two show "10/10") | read | `meter_ui.c:939-955` |
| F25 | S3 | Settings title descenders may be clipped by the selection highlight | predicted | `settings_ui.c:46, 71, 126` |

## D. Themes

| # | Sev | Finding | Conf | Where |
|---|---|---|---|---|
| F26 | S3 | Night Red: secondary text 0x3000 on black is nearly invisible (unselected menu rows, hints, About). Trigger colour = CH1 colour. | predicted | `theme.c:72` |
| F27 | S2 | High Contrast: power-off countdown digit is black on navy (invisible). CH2 colour = fail colour, so mask fails on CH2 can't be seen. | predicted | `input_handler.c:1021, 1056`; `theme.c` |
| F28 | S3 | Classic Green: dim secondary text; CH1 = text colour | predicted | `theme.c` |
| F29 | S3 | FFT, Split and Waterfall ignore the theme (hard-coded black/grey/orange) | read | `scope_ui.c:2637-2880, 2949-3081` |
| F30 | S3 | Changing theme with LEFT/RIGHT leaves the status bar in the old theme until the next full redraw | predicted | `status_bar.c:25-31` |

## E. Behaviour

| # | Sev | Finding | Conf | Where |
|---|---|---|---|---|
| F31 | S2 | Resistor Calculator: the default meant to be 4.7k computes **47k**; **Violet multiplier gives ×0.1**; "Measured" survives a band change | read | `resistor_calc_ui.c:55-60, 90-96, 148-154` |
| F32 | S2 | SINGLE re-arms only on a trigger-mode change; OK doesn't re-arm | read | `fpga.c` (`single_done`, mode change) |
| F33 | S2 | A "NOT SET" timebase or trigger level leaves the UI showing the new value. Likely at codes 19-20, where the acq task sleeps longer than the 1 s park wait. | predicted | `input_handler.c:887-899, 694-703`; `fpga.c:4170-4176` |
| F34 | S2 | Meter stats and chart count OL/ERR/blank as 0, and only sample when the text changes | predicted | `meter_ui.c:612-617, 1310-1312`; `main.c:728-744` |
| F35 | S2 | `-meter` image: each function change blocks the button task 1.4-4.5 s; held arrows queue changes; LEFT/RIGHT during AUTO races it | predicted | `fpga.c:6647-6700`; `meter_autoselect.c:122-133` |
| F36 | S2 | Critical-battery shutdown counts status-bar draws, not seconds (button presses speed it up), never resets, and doesn't save settings | read | `status_bar.c:62-75` |
| F37 | S2 | Cursors left on from the Time view take over the arrows in the FFT views (invisibly) | read | `input_handler.c:676` (cursor branch first) |
| F38 | S2 | X-Y's auto-centring leaves the offset DACs moved afterwards; cycling PRM twice may corrupt the shared buffer pool | predicted | `scope_ui.c:2052, 2122-2133`; `shared_mem.c:46-60` |
| F39 | S2 | Persistence on while in an FFT view → FFT area goes blank | predicted | `fft.c:408-421`; `input_handler.c:569-585` |
| F40 | S3 | Fuse tester: Multi view hides ATO 30/35/40 A; in Scan, UP *lowers* the threshold | read | `fuse_ui.c:337-338, 510-537` |
| F41 | S2 | Sound and Light, Auto Shutdown, FPGA SPI Scanner, Factory Reset: selectable, do nothing, say nothing | read | `input_handler.c:106-148` |
| F42 | S3 | With HOLD or REL on, the meter reading drops to 2 decimals (1.6134 → 1.61) | read | `meter_ui.c:1386-1396` |
| F43 | S2 | **First impression on a fresh unit:** CH1 boots at range 3 and timebase 0x08, so both the V/div and `H=` read `--` until the user finds UP and RIGHT | read | `config.c:51-53`; `fpga.c:4757-4767` |
| F44 | S3 | Ground markers `1`/`2` both sit mid-screen and overlap; under autofit they don't mean 0 V | read | `scope_ui.c:202-237` |
| F45 | S2 | REL (relative) is unreachable; AUTO does function pick instead | read | `meter_ui.c:1213` (no caller) |

## F. `button_manual.md` corrections

- Button list omits POWER, TRIGGER and MENU+POWER.
- Coupling is DC ↔ AC, no GND (lines 29-30, 215, 218).
- V/div and timebase step lists are nominal, not the measured labels (lines 42, 44).
- PRM cycle omits X-Y (line 93); TRIGGER in FFT views is undocumented.
- Meter has 4 layouts (Fuse); AUTO is function pick, not REL (lines 117-118, 133).
- SigGen amplitude steps are 0.1/0.2/0.5/1/2/3.3 Vpp, not ±0.1; duty is ±10 %, not ±5 %; OK is inert in release.
- 20M Limit is `n/a` (lines 217, 220); the math list is wrong (line 230).
- Startup default isn't honoured in release (lines 208-210); Firmware Update isn't the HID bootloader (line 183).
- Quick-reference card: MOVE, AUTO and SAVE rows are stale (lines 280-283).
- Nowhere says which screens are demos.
