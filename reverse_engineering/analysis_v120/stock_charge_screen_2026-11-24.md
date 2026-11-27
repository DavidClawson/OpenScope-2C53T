# Stock charge screen and power-off on USB (2026-11-24)

Static decode of how stock V1.2.0 and the factory IAP bootloader handle USB power, and what
OpenScope now does with it (`firmware/src/drivers/power.c`, `firmware/src/ui/charge_screen.c`).
Addresses are flash addresses: app linked at `0x08007000`, factory IAP at `0x08000000`.
The OpenScope side was bench-tested on 2026-11-25:
`docs/experiments/2026-11-25-75-charge-screen-on-usb.md` (EXP-75). The stock decode itself
was not re-checked on hardware.

## The problem it solves

The battery path is latched by PC9. USB VBUS powers the board regardless of PC9. So:

- plugging USB into a switched-off unit starts the MCU with nobody touching POWER;
- dropping PC9 while on USB removes nothing; the unit hangs on whatever it last drew.

OpenScope drove PC9 HIGH unconditionally as the first thing in `main()`, so a USB plug-in
booted the whole instrument, and a power-off on USB hung on "Goodbye!".

## Factory IAP (`archive/factory_iap_bootloader_2C53T.bin`)

| Address | What |
|---|---|
| `0x08000968`..`0x0800096E` | PC9 configured as output, then **BRR (LOW)**. Called from the IAP's main (`0x08002952`) on every boot, before the MENU check |
| `0x08002986`..`0x0800298C` | MENU not held: jump to the app (`0x08002784`: set MSP, call reset vector). PC9 is still LOW |
| `0x080029B0`..`0x080029B6` | Upgrade mode only: PC9 **BSRR (HIGH)**, then wait for POWER (PC8) to read released |
| `0x08001D8A`, `0x08001DDC` | Further calls to the same jump routine (the post-flash exit). **The IAP does not reset after a flash, it jumps**, with PC9 still HIGH from upgrade mode |
| `0x08001954` | The IAP's only `NVIC_SystemReset`, after programming EOPB0 = `0xFE` |

The IAP never writes `CRM_CTRLSTS.RSTFC` (the only `CRM+0x24` write, `0x08001628`, is the LICK
enable bit), so reset flags reach the app intact.

## Stock app, `master_init`

| Address | What |
|---|---|
| `0x0802AAB2` / `0x0802AAB6` | PC9 output, then **BRR (LOW)**. The battery path is released at app entry, whatever the IAP did |
| `0x0802B350`..`0x0802B40C` | **The charge screen.** A loop, entered unconditionally |
| `0x0802B364`..`0x0802B38E` | Frame counter 0..24, image index `163 + n/5`, `sprintf("3:/System file/%d.jpg")` (`0x080BC681`) |
| `0x0802B39A` | GPIOC IDR bit 7: **PC7 LOW** draws the animated frame (163..167.jpg) at (80, 82) |
| `0x0802B3A2` | Else bit 14: **PC14 LOW** draws `3:/System file/162.jpg` (`0x0802C094`); both HIGH draws nothing |
| `0x0802B3BA` | GPIOC IDR bit 8 (PC8, POWER, active LOW): HIGH loops back. LOW waits on SysTick and reads it again (`0x0802B35E`); still LOW exits |
| `0x0802B40E` / `0x0802B412` | PC9 **BSRR (HIGH)**, then the rest of `master_init` (FPGA config and all) |

On battery this loop runs only while POWER is held (the button bridges the rail), so it is one
frame and out. On USB it runs until someone presses POWER.

The SWD dump taken in this state (`swd_chargemode_baseline.txt`) agrees: PC9 output LOW,
PC8/PC7/PC14 inputs with pull-up, PC8 reading HIGH, SPI2 and the backlight live, SPI3 and USART2
untouched.

### Power-off

`0x08029D2E`..`0x08029D3C`: PC9 BRR, then `bl 0x0803B070` -> `0x0802F314` = `NVIC_SystemReset`
(`AIRCR = 0x05FA0004`). On battery the rail is gone before the reset completes. On USB the
reset runs the IAP and then `master_init` again, which lands in the charge loop.

### PC7 and PC14

Read as a 4056-class charger's open-drain status pair (the unlabelled board's TC4056A), with
pull-ups:

- **PC7 LOW = charging.** Three independent stock sites read it that way: the charge loop above,
  the status-bar battery icon (`0x0801F93C`: at level <= 10 and PC7 HIGH it switches to `6.jpg`,
  the low-battery icon), and the battery monitor (`0x08009CAE`).
- **PC14 LOW = charge complete** (only read in the charge loop).

`HARDWARE_PINOUT.md` listed PC7 as "probe detect", inferred from the meter command tail
(`0x07` when PC7 HIGH, `0x0A` when LOW). That inference and this one cannot both be right.
`reverse_engineering/TIMING_AND_STATE_MACHINES.md` already called it "AC adapter detect". Open:
a bench read of PC7 with USB in and out settles it. Note that OpenScope reads PC7 for the
meter tail without ever configuring it, so it is a floating input in our builds.

## What OpenScope does now

`power_boot_decide()` is the first call in `main()`. It reads POWER, the reset flags (then
clears them), our own power-off request (a `.noinit` word, `0x20031C5C`+, far above the IAP's
RAM), and in guest builds whether PC9 was already latched by the IAP. Then:

| Boot | Result |
|---|---|
| Our power-off (request word set, no POR since) | charge screen; POWER must be released before a new hold counts |
| POWER held | PC9 HIGH immediately, normal boot (as before) |
| PC9 already HIGH from the IAP (post-flash jump) | normal boot |
| POR, no later SW/WDT reset | charge screen (USB plugged into an off unit) |
| anything else (pinhole, SW reset, watchdog) | normal boot (as before) |

A request word that comes with a POR is stale: the rail went down after the power-off
(battery, or USB pulled during the wait), and SRAM merely kept it. Honouring it would put a
quick POWER re-press on battery onto the charge screen, where releasing POWER drops the rail.
Such a boot is judged like any other power-on.

The deliberate difference from stock: a pinhole reset and a flash reboot boot straight into the
app instead of the charge screen, so the case-closed bench loop is unchanged.

The charge screen runs before the FPGA, USB and scheduler, with PC9 LOW. It draws a battery
outline whose segments animate while PC7 is LOW, fill while PC14 is LOW, and stay empty with
"USB power" otherwise. Holding POWER for 300 ms latches PC9 and continues the normal boot.
Unplugging USB switches the unit off, as on stock.

Power-off (`power_off()`, used by the POWER hold and the critical-battery shutdown) follows stock:
request word, PC9 LOW, 400 ms, `NVIC_SystemReset`.

The decision table is host-tested with four wrong classifiers as negative controls:
`make -C firmware test-power-boot`.

## Bench checks

Run in EXP-75 and reported working as a whole (not itemised). Still open: item 6's
"Charging" half and the PC7 probe-detect question.


1. USB into an off unit: charge screen, not a boot. Hold POWER: normal boot, live scope.
2. Power-off on USB: "Goodbye!", then the charge screen (no hang). Power-off on battery: dark.
3. Unplug USB on the charge screen: unit goes dark.
4. IAP flash, entered from the running app and from cold: new image boots without POWER.
5. Pinhole reset on USB: boots straight in.
6. Charging / Charged label against the charger LED, which tests the PC7/PC14 reading.
7. Scope live after charge screen -> POWER. The FPGA stayed powered and configured, the same
   warm state the IAP flash loop already boots into.
