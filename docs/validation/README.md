# Validation: screens, controls and workflows

This is where the firmware gets checked the way a user meets it: every screen and control,
and the jobs people actually do with a scope and a meter. Unit tests and bench scripts
check the parts. These pages check the whole: is the label true, can you read it, and does
the button do what you'd guess?

It's also the definition of **S4 Polished** in the README's maturity table. A feature
reaches S4 when its screen pass and its workflows pass on a release image.

## Status

| Page | State |
|---|---|
| [inventory.md](inventory.md) | **Drafted 2026-10-05** from the code; not yet walked on a device |
| [findings.md](findings.md) | 45 predicted defects from the code read, plus manual corrections. To be confirmed by the first runs. |
| Screen passes | next: scope Time view, meter Full layout, Settings |
| Workflows | next: stable trace, coupling/probe, trigger level/mode, battery and resistor on the meter |
| Test report issue form | after the first pages have been run once |

## Equipment tiers

| Tier | You need | Covers |
|---|---|---|
| **0** | The device and a USB cable | Every screen pass, themes, settings persistence, firmware update. The signal generator can't drive CH1 in the scope build, so no live-signal workflows. |
| **1** | Tier 0 plus an AA cell, a few resistors, a diode/LED, a capacitor | Meter workflows |
| **2** | Tier 1 plus a function generator (or the ESP32 bench source) | Scope workflows: trigger, timebase, measurements, FFT, masks |
| **3** | A reference meter or a calibrated source | Turning LIVE into MEASURED |
