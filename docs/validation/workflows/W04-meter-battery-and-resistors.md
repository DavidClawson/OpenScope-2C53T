# W04: Measure a battery and some resistors

**Job:** "Is this battery good?" and "what value is this resistor?"

| | |
|---|---|
| **Tier** | 1: an AA cell, resistors (e.g. 220 Ω, 1 kΩ, 10 kΩ, 100 kΩ). Tier 3 if you also have a reference meter: record its reading beside ours. |
| **Image** | `coldtrace-meter` for resistors. `coldtrace` does DC volts only: record what it shows. |
| **Time** | ~10 min |

From 100 kΩ up, lay the resistor on the table, not between your fingers (EXP-71: hands read low).

| Step | Do | Correct result | Ref meter | Ours | Result |
|---|---|---|---|---|---|
| 1 | Scope → MENU → Meter (DC V); AA cell on the leads | ~1.5–1.6 V, steady, unit `V` | | | |
| 2 | Reverse the leads | Same magnitude, minus sign | | | |
| 3 | TRIGGER (hold), remove the cell | The held value stays, marked `HOLD` | | | |
| 4 | RIGHT to Resistance (count presses: is it obvious which function you're in?) | Leads open: `OL` | | | |
| 5 | 220 Ω | ~0.22 kΩ; within 2 % of a reference meter | | | |
| 6 | 1 kΩ | ~1.0 kΩ | | | |
| 7 | 10 kΩ | ~10 kΩ | | | |
| 8 | 100 kΩ (on the table) | ~100 kΩ | | | |
| 9 | Short the leads | A few tenths of an ohm | | | |
| 10 | Was the reading's range and unit always obvious? | Yes, without thinking | | | |

**Known:**
- DC V, resistance (100 Ω–1 MΩ), capacitance and diode agreed with a reference within 1.5 % on unit #1 (EXP-71).
- Continuity, AC volts, currents and temperature have never been checked against a reference. Results for those are especially welcome.
