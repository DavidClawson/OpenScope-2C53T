# Fuse Current Tester — Audio-Guided Mode

> **Canonical location:** This feature is now documented in `docs/meter_ideas.md` under
> "Parasitic Drain via Fuse Voltage Drop". This file contains the original brainstorm notes.

## Concept

Measure current flow through automotive fuses **without removing them** by reading the millivolt-level voltage drop across the fuse's internal resistance. Use audio feedback so the user can probe an entire fuse box by ear, without looking at the screen.

## How It Works

Automotive blade fuses (mini, ATO, maxi) have a small but measurable internal resistance, typically 1-10 milliohms. When current flows through the fuse, Ohm's law creates a voltage drop:

```
V_drop = I_load * R_fuse

Example: 10A through a 5 mOhm fuse = 50 mV
Example: 1A through a 5 mOhm fuse = 5 mV
Example: 0A (no load) = 0 mV
```

The oscilloscope's meter mode can resolve millivolt-level signals. By touching probes to both legs of a fuse (exposed at the top of the fuse box), you can determine:
1. Whether the fuse is intact (continuity)
2. Whether current is flowing (non-zero voltage drop)
3. Approximate current magnitude (if fuse resistance is known)

## Audio Feedback Pattern

The key insight: when probing a fuse box, you're often reaching into a tight space and can't easily look at the display. Audio feedback lets you probe fuse-by-fuse without shifting your eyes.

| Condition | Audio Pattern | Meaning |
|-----------|--------------|---------|
| No contact / open fuse | Silence | Probes not connected, or fuse blown |
| Contact, no current | One long beep | Fuse is good but circuit is inactive |
| Contact, current detected | Rapid triple beep | Current is flowing — this circuit is live |
| High current (> threshold) | Continuous fast beeping | Significant load on this circuit |

### Workflow

1. Set scope to Fuse Test mode
2. Touch probes across a fuse (both legs exposed at top of fuse box)
3. Listen:
   - **Silence** = no connection or blown fuse (check fuse)
   - **One beep** = good fuse, no load (move on)
   - **Triple beep** = current flowing (this circuit is drawing power)
4. Move to next fuse without looking at the screen
5. Glance at display only when you find something interesting

## Technical Considerations

### Signal levels
- Fuse resistance: ~1-10 mOhm (varies by rating and manufacturer)
- Expected voltage drops: 1 mV to 200 mV for typical automotive loads
- Need high-gain, low-noise measurement (meter mode's mV range)
- DC measurement — no AC coupling

### Audio output
- The device has a DAC (signal generator output) — could drive a small speaker/piezo
- Alternatively, PWM on an unused GPIO pin to a piezo buzzer
- Need to check if the signal generator output is accessible while in meter mode
- Stock firmware may have a beep capability (continuity mode beeps)

### Fuse resistance lookup table

**Measured data now available:** `docs/fuse_model.json` has per-type, per-rating resistances for 5 fuse types (ATO/ATC, Mini, Micro, Maxi, JCase). Firmware C header at `firmware/src/ui/fuse_table.h`.

Standard automotive fuse ratings and typical resistances could be stored for current estimation:

| Fuse Rating | Typical R_fuse | Full-load V_drop |
|-------------|---------------|-----------------|
| 5A          | ~8 mOhm      | 40 mV           |
| 10A         | ~5 mOhm      | 50 mV           |
| 15A         | ~3.5 mOhm    | 52 mV           |
| 20A         | ~3 mOhm      | 60 mV           |
| 25A         | ~2.5 mOhm    | 62 mV           |
| 30A         | ~2 mOhm      | 60 mV           |

(Exact values vary by manufacturer — could calibrate with a known load)

### Thresholds
- **No contact**: reading unstable or out of range
- **No current**: stable reading < 0.5 mV (just contact resistance)
- **Current detected**: stable reading > 1 mV
- **High current**: reading > 50 mV (configurable threshold)

## Future Extensions

- **Current estimation display**: If fuse rating is selected, calculate and show estimated amps
- **Fuse box mapping**: Save readings per-fuse-slot to build a current map of the whole box
- **Parasitic draw hunting**: The killer app — find which circuit is draining the battery overnight by probing each fuse with the key off
- **Blown fuse detection**: No continuity = blown fuse (different from "no current")
- **Could integrate with the modules system**: Load as a JSON procedure file with step-by-step fuse box walkthrough for specific vehicles

## Connection to Existing Features

- Uses the meter mode's mV measurement capability
- Could share the signal generator's DAC for audio output
- The FPGA handles meter ADC via USART commands — same data path
- Settings menu could add a "Fuse Test" entry under a new "Automotive" section
