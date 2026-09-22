# EXP-57 — first CDC flash on unit #1: `fwapply` hung in the RAM installer and left the app slot unbootable

- **Date:** 2026-09-22
- **Unit:** bench unit #1 (genuine Winbond W25Q128JV; unit #2, where the loader was proven, has a Zbit clone)
- **Build:** running image v10 = `make guest-coldtrace` at `1e63757` (Build Sep 22 2026 17:04:37); payload v9 = same source, Build 14:19:08, 619 676 B, crc32 `0840521D`
- **Status:** **FAILED — device recovered.** Root cause NOT established.

## 1. Problem
Can unit #1 be reflashed over the USB shell (`scripts/cdc_flash.py` → `fwload` → `fwapply`) so that
bench iterations stop needing a MENU+Power gesture? EXP-204 proved the loader on unit #2; unit #1
had never run it, and `fwapply` (install the just-staged slot) had never run on any unit.

## 2. Hypothesis
Staging reaches `STAGED` with the at-rest CRC matching; `fwapply` installs from RAM, issues
SYSRESETREQ, and the device re-enumerates on the v9 build stamp (14:19:08 vs the running 17:04:37).
Falsifier: no re-enumeration within ~60 s of the apply.

## 3. Procedure
v10 flashed through the factory IAP drive (user gesture); `fwstat` read `cache: A=0 B=0`.
`python3 scripts/cdc_flash.py --port /dev/ttyACM0 --slot b center_coldtrace_v9.bin`.

## 4. Control
EXP-204 on unit #2 is the only prior run of this path (same host script, `fwswap` not `fwapply`,
payloads 131 KB via this firmware's installer and 607 KB via the 2C23T port's). No same-session
control on unit #1. That is part of why the result is recorded as a failure of the path, not a finding
about its cause.

## 5. Results
| step | reading |
|---|---|
| stage | `cache: A=0/00000000 B=619676/0840521D` — CRC matches the image, ~100 s |
| `fwapply` | device printed "applying: erase+program+verify from RAM, then SYSTEM RESET" |
| host USB | **no disconnect and no re-enumeration**; kernel log shows device 16 (the v10 boot, 17:05:15) attached until the user unplugged at 17:13:07. A port open then blocked. |
| screen (user) | frozen; no button responded |
| pinhole reset | went dark; did not boot. Unplug/replug + reset: still dark |
| MENU+Power (from off) | **nothing** |
| **MENU held + pinhole reset** | **IAP drive enumerated** (`2e3c:5720`, 17:17:36) |
| recovery | v9 IAP-flashed, boots (14:19:08); cache slot B still holds v9 at CRC `0840521D` |

Reading of the symptoms, not proven: the installer ran with interrupts off (`cpsid i`, so USB and
the UI stop) and ended in its `dead:` spin loop instead of SYSRESETREQ. A spin with the rail held
explains "frozen, no disconnect". The slot not booting after reset means erase had begun before the
exit. `dead:` is reached from eight places: SPI2 reclaim timeout, size check, flash unlock (per bank),
erase busy/error, program busy/error, and the read-back verify of each chunk.

## 6. Blind spots
- Which `goto dead` fired: the installer reports nothing (no LED, no screen write, no RAM breadcrumb
  that survives the reset).
- Whether the difference from unit #2 is the payload (619 KB here; this firmware's installer had only
  ever installed 131 KB, which never reaches flash bank 2 at 0x08080000), the W25Q part (genuine vs
  clone, read by the installer's own RAM-resident SPI2 routine), or `fwapply` vs `fwswap`. The bank-2
  register addresses were checked against the vendor header and match (`unlock2` 0x44, `sts2` 0x4C,
  `ctrl2` 0x50, `addr2` 0x54).
- Why MENU+Power from power-off failed while MENU+pinhole worked. Recorded as observed only.

## 7. Conclusion
- **Established:** on unit #1 `fwapply` of a 619 KB coldtrace image hangs inside the RAM installer
  after erase has begun, leaving an unbootable app slot. The factory bootloader is untouched (the
  installer writes only 0x08007000 up) and **MENU held + pinhole reset** reaches it when MENU+Power
  does not.
- **Excluded:** loss of the factory bootloader; a staging/CRC fault (the cached image reads back
  correct after the event).
- **NOT excluded:** any of the eight dead-loop exits; bank 2; the W25Q part; `fwapply` vs `fwswap`.
- **Follow-up, before anyone runs this again:** (1) make every `dead:` exit leave a breadcrumb that
  survives a reset (a backup register or a fixed SRAM word read on next boot) and blink the
  backlight, so the next failure names its exit; (2) host-test the installer's bank-2 path;
  (3) retry on unit #1 with a < 500 KB image (bank 1 only) to split the bank-2 question from the rest.
  **`cdc_flash.py` without `--stage-only` is unsafe on unit #1 until then.**
