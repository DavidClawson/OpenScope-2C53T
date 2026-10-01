# EXP-62 — which installer exit fires (EXP-57/59), with breadcrumbs

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build running:** `make guest-coldtrace-crumbs` (FWL_INSTALL_CRUMBS=1), image crc32 `ECEAAF2B`
- **Status:** CONTROL OK; this branch's installer installs the bank-crossing image 2/2 (instrumented and not); **the release v0.4.0 installer hung 1/1 on the same unit with the same payload in the same session** (§5)

## 1. Problem
EXP-57/59: the RAM installer hangs on genuine-Winbond units with bank-crossing images. Which exit fires, at which page, with what flash status?

## 2. Hypothesis
If the failure is in bank 1 handling, the trail shows a dead-site with stage `0x1x` at a page index ≥ (0x08080000-0x08007000)/2048 = 242. If it fails in bank 0 or before, the page is < 242. A verify mismatch (site 7) records read vs expected bytes.

## 3. Procedure
1. `fwcrumb clear`.
2. **Control first:** stage + `fwswap`/`fwapply` a **bank-0-only** image (< 495 616 B, e.g. `make guest-caldump` output, 62 KB). Expect a reset into it, then reflash the crumbs build via IAP and read `fwcrumb`: stage 8 (verified, reset issued).
3. Stage the default image (crc `D95CA938`) and `fwapply`. If it hangs: MENU + pinhole → IAP → flash the crumbs build → `fwcrumb`.

## 4. Control (record first)
| control | expected | measured | passed? |
|---|---|---|---|
| bank-0-only install on this unit | completes; trail stage 8 | | |
| trail survives IAP reflash | DT11 = 0xFC57 after recovery | | |

## 5. Results
- **Control (bank-0-only, 62 548 B caldump, slot A, `fwapply`):** reset after 1.7 s, caldump booted and showed the cal page (operator: "pantallas en bucle de los chunk"). After MENU+pinhole recovery and an IAP reflash of the crumbs build, `fwcrumb`: `last stage 0x08 (verified, reset issued) dead-site 0 (none) page 30 (0x08016000)`. 62 548 / 2048 = 30.5 → pages 0..30 ✓. The trail survives the IAP reflash ✓.
- **Repro (bank-crossing, 623 476 B default build crc D95CA938, slot B, `fwapply`, trail cleared first):** port gone after **13.5 s** = SYSRESETREQ issued; the device booted the installed image (`version`: Build Oct 1 2026 05:27:55). `fwcrumb` (read from the installed, non-crumbs build): `last stage 0x08 (verified, reset issued) dead-site 0 (none) page 304 (0x0809F000)`. 623 476 / 2048 = 304.4 → pages 0..304 ✓, i.e. the installer crossed into bank 1 (pages ≥ 242) and verified every page.
- So on unit #3 the INSTRUMENTED installer installs a bank-crossing image; last night (EXP-59) the v0.4.0 release installer hung on the same unit with a payload of the same size.
- **A/B, same session, same unit, same payload (slot B, 623 476 B, crc D95CA938), same host script, `fwapply`:**
  | installer (running build) | built by | result |
  |---|---|---|
  | this branch, FWL_INSTALL_CRUMBS=1 (Build Oct 1 09:28:26) | Arm GNU 15.3 | reset at 13.5 s, booted ✓ |
  | this branch, FWL_INSTALL_CRUMBS=0 (Build Oct 1 05:27:55) | Arm GNU 15.3 | reset at 13.5 s, back at 16.0 s ✓ |
  | **release v0.4.0 coldtrace (Build Sep 30 00:04:02)** | maintainer's toolchain | **"applying…" then the port stayed enumerated and silent for 120 s — hung** (3rd maintainer-built hang: EXP-57 unit #1, EXP-59 and this on unit #3) |
- **Discriminating run (NON-instrumented installer = this branch's default build, FWL_INSTALL_CRUMBS=0, running Build 05:27:55; payload crumbs-v2 624 032 B crc A76CBABE in slot A; trail cleared; `fwapply`):** port gone after **13.5 s**, back at **16.0 s**, the installed crumbs-v2 build booted. So the instrumentation is NOT what makes the install succeed.
## 6. Blind spots
- Neither of today's installers is the one that hung: EXP-57 (unit #1) and EXP-59 (unit #3) both ran maintainer-built binaries (v10 / release v0.4.0); today's two successes ran this branch built with Arm GNU Toolchain 15.3. The fw_loader source differs only trivially (a local for the verify read; the compiled-out crumb macros). Compiler/codegen of the RAM-resident installer is an untested variable.
- The trail is one snapshot (the last step reached). A failure that corrupts RAM or faults before the first store records nothing (DT11 then shows no magic, which is itself a result).
- BPR survives resets, not a full power loss without VBAT: recovery must keep USB attached.
## 7. Conclusion
- **Established:** this branch's installer, built with GCC 15.3, installs a bank-crossing image on unit #3 (2/2: instrumented and not), crossing into bank 1 and verifying every page; the breadcrumb trail works and survives an IAP reflash.
- **Excluded:** "bank-crossing images cannot be installed on genuine-Winbond units".
- **NOT excluded:** that the maintainer-built binaries hang (compiler/codegen of `fwl_ram_install`), or intermittency.
- **Established (A/B):** on this unit the maintainer-built release installer hangs where this branch's own build succeeds, with everything else held constant. The failure is in the maintainer-built binary (toolchain/codegen of the RAM-resident installer, or something else in that binary), not in the unit, the W25Q part or the payload size.
- **Follow-up:** diff of the two `fwl_ram_install` disassemblies (release .bin vs our ELF); then build this branch with the maintainer's toolchain version, or ship an installer whose codegen is pinned (e.g. hand-checked asm or `-O0`/`-Os` for the RF section).
