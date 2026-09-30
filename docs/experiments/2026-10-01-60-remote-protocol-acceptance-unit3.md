# EXP-60 — remote protocol acceptance on hardware (#10 §6)

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build:** `make guest-coldtrace-crumbs`, commit `6658863`+ (firmware unchanged after it), image crc32 `ECEAAF2B`
- **Status:** OPEN

## 1. Problem
Does the CDC binding of esp_comm answer honestly on real hardware: does STATUS report the live mode and battery, and do BUTTON presses and GET_METER act and read what they claim?

## 2. Hypothesis
If the binding works, `openscope info` reports the mode shown on the LCD and follows a mode change made with the physical buttons; BUTTON MENU changes the framebuffer; GET_METER in meter mode returns an advancing update_count with a value within a few % of an external DMM on the same source, and outside meter mode NAKs UNSUPPORTED_IN_MODE. If not, one of those readbacks disagrees with its control.

## 3. Procedure
`python3 scripts/bench_remote_protocol.py --out <dir> --only info,crumbs,press,mode,meter` (interactive mode step). Meter: AA cell on V/COM, measured first with an external DMM.

## 4. Control (record first)
| control | expected | measured | passed? |
|---|---|---|---|
| LCD mode vs operator | operator reads the mode on screen | | |
| press: two screenshots with NO press | identical framebuffers (else the press test is VOID: live trace; switch to a static screen) | | |
| meter source | external DMM reading of the cell | | |
| shell alive next to the protocol | `version` answers between protocol requests | | |

## 5. Results
## 6. Blind spots
- Screenshots are the firmware's 4-bit shadow, not the panel. A press that changes only unshadowed pixels would look like "no change".
- One unit; meter absolute accuracy remains unit-specific (#28).
## 7. Conclusion
