# EXP-59 — `fwapply` of a bank-crossing image hangs on unit #3 too (second genuine-Winbond unit)

- **Date:** 2026-10-01 (~04:05 local)
- **Unit:** unit #3 (Quero's): V1.4 `2C53T-V1.4_20250507`, W25Q JEDEC `EF 40 18` (genuine Winbond, like unit #1), never custom-flashed before 2026-10-01 (#28)
- **Build running:** release `openscope-2c53t-v0.4.0-coldtrace.bin` (Sep 30 2026 00:04:02), flashed via factory IAP
- **Payload:** `feat/remote-protocol` guest-coldtrace, 621 824 B, crc32 `E94E5986`
- **Status:** REPRODUCED (symptom); cause OPEN

## 1. Problem
Is the EXP-57 `fwapply` hang specific to unit #1, or does this installer fail on bank-crossing images in general?

## 2. Hypothesis
If the hang is specific to unit #1, the same procedure on another genuine-Winbond unit completes: the port drops (SYSRESETREQ) and re-enumerates on the payload's build stamp within ~60 s. If the hang is not unit-specific, the port stays enumerated but silent, exactly as in EXP-57.

## 3. Procedure
`cdc_flash.py ../bin/rp1-guest-coldtrace.bin --slot b --stage-only` (71.6 s), then `fwapply` typed on the shell. Host watched `/dev/cu.usbmodem083540BF89911` for 120 s, then probed it with `version`.

**Preconditions verified by readback:**
| what | expected | measured |
|---|---|---|
| running build | release coldtrace | `version`: Build Sep 30 2026 00:04:02, SRAM 224KB (EOPB0=0xFE) |
| cache before | empty | `fwstat`: A=0/00000000 B=0/00000000 |
| W25Q part | — | `flash jedec`: EF 40 18 |
| staged image | crc = host crc | `fwload: STAGED slot=b 621824/621824 crc=E94E5986`; host zlib crc32 `E94E5986` |

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| staging path (same session, same port) | STAGED with at-rest CRC = host CRC | yes | ✓ |
| shell alive right before `fwapply` | `fwstat` answers | yes | ✓ |
| installer completing on this unit | — | **none available** (no small bank-0-only image was tried first) | — |

The only positive control that would discriminate the cause (a bank-0-only image installing on this unit through the same path, as on unit #2) was **not** run. So this is a reproduction of the symptom, not a result about its cause.

## 5. Results
- The device printed `applying: erase+program+verify from RAM, then SYSTEM RESET …`.
- The port **never disappeared** in 120 s (no SYSRESETREQ, no re-enumeration). Opening it and sending `version` returned 0 bytes.
- Screen: not observed (operator asleep) — no evidence either way.
- Recovery: pending (MENU held + pinhole reset → IAP). The factory IAP at 0x08000000 cannot have been touched (the installer writes only 0x08007000 up; the code at 0x0000–0x5FFF was dumped and archived earlier the same night).

## 6. Blind spots
- Which of the installer's exits fired: the running v0.4.0 installer leaves no trace (hence FWL_INSTALL_CRUMBS in 6bda2a6, EXP-62).
- Whether the payload (this branch) matters: it is ~2 KB larger than release coldtrace; both cross 0x08080000.
- Bank-0-only behaviour on this unit (no control run).

## 7. Conclusion
- **Established:** on 2 of 2 genuine-Winbond units (#1, #3) `fwapply` of a ~620 KB coldtrace image hangs inside the RAM installer with the same signature.
- **Excluded:** "only unit #1" as an explanation.
- **NOT excluded:** genuine vs clone W25Q, bank-crossing vs size, fwapply vs fwswap — unit #2 differs from #1/#3 in all three.
- **Follow-up:** EXP-62 (same repro under a breadcrumb build, plus the bank-0-only control).
