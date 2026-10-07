# EXP-60 — remote protocol acceptance on hardware (#10 §6)

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build:** `make guest-coldtrace-crumbs` (v2, with the screenshot trailer CRC), image crc32 `A76CBABE`, Build Oct 1 2026 09:28:26
- **Status:** CONFIRMED
- **Note (2026-10-05, rebase onto 9170a50):** `FWL_INSTALL_CRUMBS`, `fwcrumb` and `make guest-coldtrace-crumbs` no longer exist on this branch. They were dropped in favour of main's c4e40bd (installer record in BPR DT1..5 in every build, printed by `fwstat`, exit code blinked on the backlight), as agreed in #45. The record below is kept as it was measured.
- **CORRECTED 2026-10-01 (operator's confirmation):** the "external DMM" named below was the **unit under test itself in multimeter mode**, read on its LCD before the protocol run. Every "agrees with the external DMM" in this file is therefore *protocol reading = LCD reading of the same meter path*, which is what the §6 acceptance needs (GET_METER reports what the display reports) but is **not** an independent check of the meter's accuracy. The lines are kept as written, with this note; the 3.292 V figure is the device's own, uncorroborated. An external reference is still to be found (EXP-64 §6).

## 1. Problem
Does the CDC binding of esp_comm answer honestly on real hardware: does STATUS report the live mode and battery, and do BUTTON presses and GET_METER act and read what they claim?

## 2. Hypothesis
If the binding works, `openscope info` reports the mode shown on the LCD and follows a mode change made with the physical buttons; BUTTON MENU changes the framebuffer; GET_METER in meter mode returns an advancing update_count with a value within a few % of an external DMM on the same source, and outside meter mode NAKs UNSUPPORTED_IN_MODE. If not, one of those readbacks disagrees with its control.

## 3. Procedure
`python3 scripts/bench_remote_protocol.py --out <dir> --only info,crumbs,press,mode,meter` (interactive mode step). Meter: AA cell on V/COM, measured first with an external DMM.

## 4. Control (record first)
| control | expected | measured | passed? |
|---|---|---|---|
| LCD mode vs STATUS | screenshot header and STATUS agree | screenshot "SETUP" while STATUS = settings; "METER" while STATUS = meter | ✓ |
| press: two screenshots with NO press | identical framebuffers | scope screen: D57719EB ≠ 6382C751 → **VOID on the live trace** (as designed); meter screen: E31BF73C = E31BF73C ✓ | ✓ (on a static screen) |
| meter source | external DMM reading | Kode Dot 3V3 rail: **3.292 V** on the operator's DMM *[corrected: the unit's own meter mode, read on the LCD]* | ✓ as a display readback; not an external reference |
| shell alive next to the protocol | `usbstat`/`fwcrumb` answer between frames | yes, throughout | ✓ |

## 5. Results
- `openscope info`: firmware Build Oct 1 2026 09:28:26, protocol v1, mode scope, battery **unknown (no sample yet, charging)** (booted on USB: the flag works instead of a fake 0 %), capture real samples.
- Mode follows the device: STATUS went scope → meter at t=175.7 s when the operator changed mode with the buttons (watcher log). Later: settings after two MENU presses, meter after two more.
- BUTTON: `press MENU` from scope → the screen became the METER screen (screenshot). On the static meter screen: control equal, then MENU → 72FBC08E, MENU → 00757A30 (siggen → settings; STATUS = settings). PASS.
- GET_METER: in scope mode → NAK UNSUPPORTED_IN_MODE (10/10); meter mode, DC V, no input → NAK NOT_READY; with the 3V3 rail: **3.292 V** ×8, raw BCD 3292, update_count 717→730 (advancing), CSV `dumps/meter_3v3_kodedot.csv`; the LCD showed 3.292 V (screenshot). Agrees with the external DMM to the mV *[corrected: that "DMM" was this LCD; the agreement is protocol vs display]*.
- Screenshot: live scope screen → returned as *torn* (trailer CRC ok, header CRC differs); static screens → CRC verified. The v1 build (header CRC only) failed 3/3 on the live screen, which is why v2 added the trailer.
- MCP (mcp 2.2.0 client over stdio): scope_info live JSON, scope_meter refused with the reason visible to the model, scope_screenshot 3550 B PNG, scope_shell fwapply refused by the allowlist, usbstat allowed.

## 6. Blind spots
- Screenshots are the firmware's 4-bit shadow, not the panel.
- One unit; the meter agreement is one voltage on one range (DC V, 3.3 V).

## 7. Conclusion
- **Established:** the §6 acceptance criteria hold on hardware (live mode/battery, mode follows the device, BUTTON acts, GET_METER reads what the LCD shows *[corrected: no external DMM was involved]*, refuses when it cannot).
- **Follow-up:** more meter ranges/functions once the meter build lands; a second unit.
## 6. Blind spots
- Screenshots are the firmware's 4-bit shadow, not the panel. A press that changes only unshadowed pixels would look like "no change".
- One unit; meter absolute accuracy remains unit-specific (#28).
## 7. Conclusion
