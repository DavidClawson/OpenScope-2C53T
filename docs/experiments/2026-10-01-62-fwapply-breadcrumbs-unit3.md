# EXP-62 — which installer exit fires (EXP-57/59), with breadcrumbs

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build running:** `make guest-coldtrace-crumbs` (FWL_INSTALL_CRUMBS=1), image crc32 `ECEAAF2B`
- **Status:** OPEN

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
## 6. Blind spots
- The trail is one snapshot (the last step reached). A failure that corrupts RAM or faults before the first store records nothing (DT11 then shows no magic, which is itself a result).
- BPR survives resets, not a full power loss without VBAT: recovery must keep USB attached.
## 7. Conclusion
