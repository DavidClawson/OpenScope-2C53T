# EXP-23 — fw_loader's per-page FWDGT feed survives a real install under a live 3.0 s watchdog

- **Date:** 2026-08-25
- **Unit:** bench unit #2 (Stlkv)
- **Build:** `make guest-coldtrace` at commit `8c8778e`, **plus one local
  bench-only edit**: the `#ifndef GUEST_BUILD` guard around `watchdog_init()`
  (`main.c`) replaced with `#if 1`, so the guest image arms the FWDGT it
  normally never arms. 597 464 B, crc32 `61A13811`, kept as
  `mydevice/53t-coldtrace-wdtarmed-bench.bin` (outside this repo). The edit is
  NOT committed anywhere.
- **Status:** CONFIRMED

## 1. Problem

PR #29 review (DavidClawson): `fwl_ram_install()` runs `cpsid i` through a
10–20 s erase+program with the FWDGT armed at 3.0 s on non-guest builds, fed by
a health task that dies the moment interrupts go off — so the install should
die mid-erase and reset into an unbootable slot. The fix (one reload-key store
per 2 KB page, `fw_loader.c:458`) had never run under an armed watchdog: every
`guest*`/`coldtrace*` flavour defines `GUEST_BUILD`, which skips
`watchdog_init()` (`main.c:1067`), and the 2C23T port arms no watchdog at all.
Does the feed actually hold a real install up?

## 2. Hypothesis

If the per-page feed works, a 597 KB self-reinstall (`fwswap b` from the
firmware running out of slot B) completes, the device boots back into the same
image, and the WDT reset flag (`CRM_CTRLSTS` bit 29, `wdtrstf`,
`at32f403a_407_crm.h:970`) stays clear. If the feed does not work (wrong
register, wrong key, or not reached per page), the FWDGT — a free-running
hardware counter that interrupts cannot mask — fires within 3.0 s of the last
health-task feed, the reset lands on a half-erased app slot, and the device
does not re-enumerate (recovery: MENU+Power IAP).

## 3. Procedure

1. Stage the watchdog-armed image into slot B from the running 2C23T port:
   `python3 scripts/cdc_flash.py mydevice/53t-coldtrace-wdtarmed-bench.bin
   --port /dev/cu.usbmodem00012 --slot b --stage-only` →
   `fwload: STAGED slot=b size=597464 crc=61a13811`.
2. `fwswap b` from the port (this install runs on the port — no watchdog —
   known-good path from the 08-24 session).
3. On the rebooted firmware: verify the FWDGT is armed by register readback
   (`mem read 0x40003000 4`).
4. `fwswap b` — the armed firmware reinstalls ITSELF: 597 464 B ≈ 292 pages of
   2 KB, the full-size case from the review. Time the window from command to
   CDC re-enumeration.
5. Read `version`, `CRM_CTRLSTS` (`mem read 0x40021024 1`), `fwstat`.
6. `fwswap a` — second data point (131 148 B port install under the same armed
   dog), and the way home.

**Preconditions verified by readback** (not assumed):

| what | expected | measured |
|---|---|---|
| FWDGT divider (`0x40003004`) | 4 (DIV_64, `watchdog.c`) | `0x00000004` |
| FWDGT reload (`0x40003008`) | 1875 = `0x753` (3.0 s) | `0x00000753` |
| running build | Aug 25 bench build | `Build: Aug 25 2026 17:44:35` |
| slot B manifest | 597464 / `61A13811` | `B=597464/61A13811` (fwstat) |

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| same install path, watchdog NOT armed (port's `fwswap b`, step 2) | boots into slot B image | booted, banner + fwstat correct | yes |
| FWDGT actually armed before the test (readback above) | div 4, rld `0x753` | div 4, rld `0x753` | yes |

The counterfactual control — the same install with the feed REMOVED, expected
to half-brick — was **not run** (costs a MENU+Power recovery; see blind spots
for what stands in for it).

## 5. Results

1. `fwswap b` under the armed FWDGT: CDC gone at +16.1 s, back at +18.3 s —
   an **install window of ~16 s, more than five 3.0 s watchdog periods**, all
   with interrupts off and the health task dead.
2. Boot verdict: same image (`Build: Aug 25 2026 17:44:35`), shell live.
3. `CRM_CTRLSTS = 0x14000000` — bit 28 (`swrstf`, our SYSRESETREQ) and bit 26
   (`nrstf`) set; **bit 29 (`wdtrstf`) CLEAR: the watchdog never fired.**
4. FWDGT still armed after reboot (div 4, rld `0x753` re-read).
5. `fwswap a` (131 148 B port install, same armed dog): window ~4.5 s, port
   booted, both slot manifests intact (`A=131148/1624067E B=597464/61A13811`).

## 6. Blind spots

1. **The counterfactual (no feed → brick) was not demonstrated**, only derived:
   the FWDGT is a hardware down-counter clocked from IRC40K that no `cpsid i`
   can pause, armed at 3.0 s by readback, and left unfed by software for ~16 s
   except via the per-page store. If some *other* mechanism silently fed or
   paused it (a debug-mode freeze bit we don't know about), this test would
   pass for the wrong reason. `wdtrstf` staying clear across a 16 s window is
   consistent with "fed per page", not proof of "would have fired otherwise".
2. Unit #2 only; unit #1's silicon not exercised.
3. The bench image differs from any committed build (the `#if 1` edit), so
   this validates the fw_loader code path, not a shippable binary.
4. Timing measured by USB re-enumeration, not a scope on the rail — the true
   interrupts-off window is somewhat shorter than 16 s.

## 7. Conclusion

- **Established:** the per-page reload-key store (`fw_loader.c:458`) keeps a
  live 3.0 s FWDGT from firing across a full-size 597 KB erase+program+verify
  (~292 pages, ~16 s) and across a 131 KB install (~4.5 s); the reviewer's
  suggested fix works as written on real silicon.
- **Excluded:** the reviewer's must-fix failure mode (watchdog reset mid-erase)
  for installs running THIS code with the feed present.
- **NOT excluded (explicitly):** the failure on a plain-linked (`0x08004000`)
  build older than commit `1136141`; unit-#1-specific behaviour; the
  counterfactual brick (not demonstrated, see blind spot 1).
- **Follow-up:** none required for the PR; the counterfactual demo is available
  on request at the cost of one MENU+Power recovery.
