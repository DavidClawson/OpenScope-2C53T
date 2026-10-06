# Screen pass: Settings, About and the tool screens

| | |
|---|---|
| **Image** | either |
| **Tier** | 0 |
| **Time** | ~15 min |

Mark each row **✓**, **✗** (say what you saw) or **?** (confusing: say why). F-numbers are
predicted defects ([findings.md](../findings.md)).

## 1. The menu

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| SE-01 | From Meter, MENU twice (Siggen, then Settings) | A list titled "Settings", nothing clipped (**F25**) | | |
| SE-02 | DOWN to the end | The list scrolls and shows there's more below; every item is reachable | | |
| SE-03 | OK on each of: Sound and Light, Auto Shutdown, FPGA SPI Scanner, Factory Reset | Each does something, or says it isn't available (**F41**) | | |
| SE-04 | Display Mode: OK, then LEFT, then RIGHT | Theme changes each time; the **whole** screen, status bar included, repaints in the new theme (**F30**) | | |
| SE-05 | Startup on Boot → Scope/Meter, then power-cycle | Boots into the chosen mode (**F07**) | | |
| SE-06 | Firmware Update → OK | Something clear happens: an update mode, or instructions (**F13**) | | |
| SE-07 | About | The release version (e.g. v0.4.1) and a build ID (**F22**). Text doesn't overlap the info bar (**F23**). | | |

## 2. Oscilloscope Settings

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| SE-10 | OK on Oscilloscope Settings | Rows: CH1 Coupling, CH1 Probe, CH1 20M Limit, CH2 ×3, Trigger Mode, **Trigger Edge** (**F14** predicts the last row is not drawn) | | |
| SE-11 | Change CH1 Coupling and Probe, then MENU, MENU to Scope | The Scope info bar and popups agree with what you set | | |
| SE-12 | 20M Limit | Shows `n/a`; pressing OK changes nothing. Is "n/a" clear enough? | | |
| SE-13 | Trigger Edge: DOWN to it, OK | You can see the row and its new value (**F14**) | | |

## 3. Math / Persist

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| SE-20 | Math Channel OK → A+B; back to Scope; change CH1's signal or V/div | The math trace follows the inputs (**F01** predicts a fixed synthetic trace) | | |
| SE-21 | Persistence On; back to Scope | Persistence of the real trace (**F02**) | | |
| SE-22 | Persistence On while the FFT view is selected; back to Scope | FFT still works (**F39**) | | |

## 4. Tool screens

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| SE-30 | Component Tester; SELECT ×4 | Each type; it's clear whether results are real or a demo (**F03**) | | |
| SE-31 | Component Tester: the hint vs what OK does | The hint is accurate (it says "OK:Back"; OK opens the calculator) | | |
| SE-32 | Resistor Calculator on entry | Shows the default it claims (4.7k?) (**F31** predicts 47k) | | |
| SE-33 | Set Brn-Blk-Org-Gold | 10 kΩ 5 % | | |
| SE-34 | Multiplier Violet | ×10 M (**F31** predicts ×0.1) | | |
| SE-35 | SELECT | It's clear the "measured" value is simulated (**F04**) | | |
| SE-36 | Bode Plot | Clearly labelled a demo | | |

## 5. Signal generator (MENU from Meter)

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| SE-40 | SELECT, UP/DOWN, LEFT/RIGHT, PRM | Waveform, amplitude, duty and frequency change; the preview follows | | |
| SE-41 | Info bar | Matches the selected waveform and amplitude (**F09**) | | |
| SE-42 | OK | Turns output on, or says why it can't in this build (**F08**) | | |
