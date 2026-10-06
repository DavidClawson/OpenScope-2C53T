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
| **Screen passes** | [Scope Time view](screens/scope-time.md) · [Meter](screens/meter.md) · [Settings, tools, siggen](screens/settings.md). Drafted, not yet run. |
| **Workflows** | [W01 stable trace](workflows/W01-stable-trace.md) · [W02 coupling & probe](workflows/W02-coupling-and-probe.md) · [W03 trigger](workflows/W03-trigger.md) · [W04 battery & resistors](workflows/W04-meter-battery-and-resistors.md). Drafted, not yet run. |
| Still to write | Theme sweep (every screen × 4 themes), settings survive a power cycle, firmware update, power & battery, FFT, cursors & measurements, masks, meter's untested functions |
| Test report issue form | After the first pages have been run once, so the form matches what testers actually report |

## Running a page

1. Note the image file name and the build line (Settings → About, or `version` over USB).
2. Work top to bottom. For each row: **✓** as expected, **✗** wrong (say what you saw), or
   **?** works but confusing (say why). A **?** is as valuable as a ✗: it's the UX signal.
3. Rows naming an **F-number** are where the code read predicts a defect. Your result confirms
   it or retires it. Don't skip them because they're "known".
4. Photos beat descriptions for anything visual (screenshots quantise colour, so they're no
   good for judging themes).
5. Report every ✗ and ? by ID. Until the issue form exists, use a comment on the tracking issue.

## Equipment tiers

| Tier | You need | Covers |
|---|---|---|
| **0** | The device and a USB cable | Every screen pass, themes, settings persistence, firmware update. The signal generator can't drive CH1 in the scope build, so no live-signal workflows. |
| **1** | Tier 0 plus an AA cell, a few resistors, a diode/LED, a capacitor | Meter workflows |
| **2** | Tier 1 plus a function generator (or the ESP32 bench source) | Scope workflows: trigger, timebase, measurements, FFT, masks |
| **3** | A reference meter or a calibrated source | Turning LIVE into MEASURED |
