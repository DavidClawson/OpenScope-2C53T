# Screen pass: Meter (all four layouts)

| | |
|---|---|
| **Image** | Note which: `coldtrace` (DC volts only) or `coldtrace-meter` (all functions) |
| **Tier** | 0; 1 (an AA cell) for §4 |
| **Time** | ~15 min |

Mark each row **✓**, **✗** (say what you saw) or **?** (confusing: say why). F-numbers are
predicted defects ([findings.md](../findings.md)).

## 1. Arriving

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| MT-01 | From Scope, MENU | Meter screen, Full layout, DC V. Status bar says `METER`. | | |
| MT-02 | Leads open | A clear "no reading" state (`---` or `OL`); nothing that looks like a measurement | | |

## 2. Functions

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| MT-10 | RIGHT ×11 (one full loop) | Each function's name and unit in turn: DC V, AC V, DC mA, DC A, AC mA, AC A, Resistance, Continuity, Diode, Capacitance, Temperature. The index reads 1/11 … 11/11 (**F24**). | | |
| MT-11 | `coldtrace` only: a function other than DC V | The screen makes clear this function isn't available in this image, not just `---` | | |
| MT-12 | `coldtrace-meter` only: time each RIGHT press | The change takes ~1.5 s. Do held arrows queue up many changes? (**F35**) | | |
| MT-13 | "Range:" label in Resistance with a 1 kΩ on the leads | Matches the reading's range (**F10**: a fixed label is predicted) | | |

## 3. Buttons and layouts

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| MT-20 | OK ×4 | Full → Chart → Stats → Fuse → Full | | |
| MT-21 | TRIGGER with a steady reading | Grey `hold`, then `HOLD` within ~5 s; the reading freezes. The same number of decimals as live (**F42**). | | |
| MT-22 | SELECT | Min/max/avg and chart reset | | |
| MT-23 | AUTO | A message that auto-select started; it ends on a function and says so (**F15**: popups predicted invisible here) | | |
| MT-24 | SAVE | Toggles a raw-data debug overlay (expected for now), or says what it does | | |
| MT-25 | UP, DOWN, PRM, CH1, CH2, MOVE | Nothing happens. Should anything? Note ideas. | | |

## 4. Reading the screen (an AA cell on DC V, tier 1)

| ID | Look at | Expect | Result | Note |
|---|---|---|---|---|
| MT-30 | Main digits | About 1.6 V, large and steady, unit `V`, tag `DC` | | |
| MT-31 | Reverse the leads | A leading minus | | |
| MT-32 | Bar graph | Proportional to the reading on a sensible scale | | |
| MT-33 | Chart layout, 30 s | The strip chart keeps adding points while the reading is steady (**F34**) | | |
| MT-34 | Stats layout | Sample count climbs; histogram appears; units consistent | | |
| MT-35 | Remove the cell (leads open) in Stats | Min/avg do not drop to 0 (**F34**) | | |
| MT-36 | Fonts and colours | Nothing clipped; every label readable | | |

## 5. Fuse layout

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| MT-40 | View to Fuse; LEFT/RIGHT; Rating; Show ×4 | Type, rating and view (Detail → Table → Scan → Types) change and are labelled; the meter switches to DC V with a popup; the Types page has Fuse type on MOVE | | |
| MT-41 | Detail view | Current shown once with one unit (**F11**, fixed 2026-10-07) | | |
| MT-42 | Scan view | "NO DRAW" / "DRAW" readable as words (**F21**, fixed) | | |
| MT-43 | Table view, ATO | Every rating reachable, including 1, 2, 40 A (**F40**, fixed); UP moves the highlight up | | |
| MT-44 | Tips together, Cal leads; then probes on a fuse carrying no current | Popup "Leads cal: about −1 mV"; the key shows the value; Detail near 0 mA, Scan NO DRAW (**F47**, fixed 2026-10-07) | | |
| MT-47 | Cal leads with the tips apart and moving; then on a fuse carrying 5 mV or more | "Hold the tips together"; "Too large: tips together?"; the stored value is unchanged | | |
| MT-48 | Wave the open leads around | "unsteady: probes on?" (Scan: WAIT), no DRAW verdict | | |
| MT-45 | Types view, beside a real fuse box | You can tell which type you have; the colours match real fuses | | |
| MT-46 | Leave the reading steady for 30 s | Nothing on screen blinks (fuse flicker, 2026-10-06) | | |
