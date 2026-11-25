# EXP-75: Charge screen and power-off on USB

- **Date:** 2026-11-25
- **Unit:** bench unit (number not recorded)
- **Build:** `make guest-coldtrace` on `feat/charge-screen`, image sha256
  `bae85864431afbcb3a558b056da77bc4d81b663113db5671efcaf37d46779ae8`
- **Status:** CONFIRMED by operator report (checks not itemised, see section 5)

## 1. Problem

Plugging USB into a switched-off unit boots the whole instrument, and on USB the POWER-hold
shutdown hangs on "Goodbye!", because `main()` latched PC9 unconditionally and power-off only
dropped PC9. Does the stock-style charge screen (`firmware/src/drivers/power.c`,
`firmware/src/ui/charge_screen.c`, decode in
`reverse_engineering/analysis_v120/stock_charge_screen_2026-11-24.md`) fix both without
breaking the battery boot, the IAP flash loop or the pinhole reset?

## 2. Hypothesis

`power_boot_decide()` reads POWER, the reset flags, the `.noinit` power-off request and (guest
builds) the IAP's PC9 latch, and only a plain power-on with POWER up, or our own power-off,
lands on the charge screen with PC9 LOW.

- **If true:** USB into a dark unit shows the charge screen, not the splash. Unplugging USB
  there turns the unit dark. Power-off on USB returns to the charge screen. Battery boot, IAP
  post-flash boot and pinhole reset reach the instrument without the charge screen.
- **If false:** one of these shows the opposite. Each check below names what its failure
  points at.

## 3. Procedure

Flash: MENU + POWER into upgrade mode, then
`python3 scripts/iap_flash.py flash .scratchpad/guest-coldtrace-charge.bin` from the main
checkout. Record how upgrade mode was entered.

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| image on device is this build | charge screen text "Hold POWER to turn on" appears in T2 (no earlier image has it) | |
| IAP copy completed | `iap_flash.py` reports success | |

Checks, in order:

| # | action | expected | failure points at |
|---|---|---|---|
| T1 | IAP flash completes | instrument boots by itself, no POWER needed, no charge screen | `latched` signal (PC9 still HIGH from upgrade mode) not seen |
| T2 | unit dark, USB unplugged; plug USB in | charge screen: battery outline, label, "Hold POWER to turn on" | instrument boots: POR flag or POWER sampling wrong |
| T3 | on the charge screen, hold POWER about 0.5 s | normal boot; scope live (demo trace disappears) | warm-FPGA boot path after the charge screen |
| T4 | running on USB, POWER-hold shutdown | "Goodbye!", then the charge screen within about 1 s; keeping POWER held does not boot it again; release then hold boots | `power_off()` reset or the `.noinit` request |
| T5 | on the charge screen, unplug USB | unit goes dark | PC9 not LOW on the charge screen |
| T6 | battery only, running, POWER-hold shutdown | unit goes dark | `power_off()` on battery |
| T7 | battery only, dark, press and hold POWER again within about 1 s of T6 | boots on the first press | stale-request guard (POR plus request) |
| T8 | running on USB, pinhole reset | instrument boots straight in | non-POR resets misclassified |
| T9 | charge screen label against the charger LED, at least once while charging | label matches the LED's charging state | PC7/PC14 reading |
| T10 | on the charge screen, plug and unplug the meter leads and a scope probe | label does not change | PC7 is the "probe detect" line after all |

## 4. Control

Run first. If C1 fails the image is bad and every check below is void.

| control | expected | measured | passed? |
|---|---|---|---|
| C1: battery only, dark, press and hold POWER | normal boot to the instrument (old path, unchanged) | covered by "works well", not itemised | yes (report) |
| C2: after C1, scope mode with a signal on CH1 | live trace, demo trace gone | covered by "works well", not itemised | yes (report) |

> If a control fails, this experiment is **VOID, not negative**.

## 5. Results

Operator report after running the checks on the bench: "works well". The checks were not
recorded one by one, so the table in section 3 stands as the procedure. It is not a
per-check record.

One photo (screen observation, weak evidence):
`reverse_engineering/captures/exp75/charge_screen_charged.jpg`. It shows the charge screen
on USB with all five segments lit, the label "Charged" and the hint "Hold POWER to turn on".
"Charged" is the PC7 HIGH, PC14 LOW branch, so on this unit PC14 read LOW with a full
battery, as the stock decode predicts. A light at the top right of the case is green. It may
be the charger LED, which would agree, but which LED that is was not checked. The faint copy
of the label above "Charged" is most likely a reflection in the cover glass, which shows
other reflections in the same photo. No code path draws text there.

## 6. Blind spots

- All evidence is the screen and whether the unit goes dark. Nothing here reads PC9, PC7,
  PC14 or the reset flags directly (SWD halts the CPU on this unit, CLAUDE.md RDP note).
- T5 shows PC9 is LOW on the charge screen only in combination with a working battery path:
  a dark unit after unplugging could also be a flat battery. Check that C1 boots on battery
  in the same session.
- T7 cannot tell whether SRAM actually kept the request through the outage. A pass shows only
  that the re-press boots, not that the guard was exercised. `tests/test_power_boot.c`
  carries that case.
- T9 cannot separate "PC7 is CHRG" from "PC7 is something that happens to be LOW while
  charging". T10 is the discriminating check for the probe-detect reading.
- "Charged" (PC14) needs a full battery and may not be reachable in one session.
- The 300 ms hold threshold is not measured, only that a deliberate hold works.

## 7. Conclusion

- **Established:** on USB the unit no longer boots by itself. It shows the charge screen
  and boots on a POWER hold. The POWER-hold shutdown on USB no longer hangs. The battery
  boot, the IAP flash loop and the pinhole reset still work. All of this rests on the
  operator report. The "Charged" branch (PC14 LOW) is seen on a full battery (photo).
- **Excluded:** the unconditional PC9 latch is not needed for any of the boots checked.
- **NOT excluded (explicitly):** the PC7 dispute. The "Charging" branch is not in the
  recorded evidence, and T10 (label against leads and probe) is not recorded either, so
  PC7 = charger CHRG versus PC7 = probe detect is still open. The stale-request guard
  (T7) is covered by the host test only. The 300 ms hold threshold is not measured.
- **Follow-up:** (1) record T9/T10 with a part-charged battery to settle PC7.
  (2) Backlight timeout on the charge screen; it stays lit for as long as USB is in.
  (3) Use PC7/PC14 for the status bar's CHG badge instead of the 4300 mV heuristic, once
  PC7 is settled.
