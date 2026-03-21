# FNIRSI 2C53T — Community Issues & Feature Requests

*Curated from EEVblog forums, Elektor Magazine, YouTube reviews, blogs, and user communities.*

62 issues documented across firmware, oscilloscope, signal generator, multimeter, and UI categories. 35 are critical or high severity. These inform the priorities for custom firmware development.

## Custom Firmware Priority (Based on Community Pain Points)

1. **Fix hang/lockup bugs** (#1, #2, #7) — the #1 reason users are frustrated
2. **Fix flash filesystem corruption** (#3) — proper FatFS mutex protection
3. **Make FFT actually work** (#6) — listed feature that doesn't produce usable results
4. **Restore 1MHz signal generator** (#11) — biggest regression vs predecessor
5. **Add min/max/avg reset** (#9, #10) — easy win, frequently requested
6. **Fix signal generator accuracy** (#5, #14) — wrong frequencies below 20Hz, visible steps

## Hardware Discovery

Issue #1 confirms the external SPI flash chip is a **Winbond W25Q128JVSQ** (128Mbit = 16MB). This is much larger than initially estimated — plenty of space for modules, waveform references, vehicle definitions, and other data.

The flash stores both firmware data and system graphics. A firmware update alone is not sufficient to restore a corrupted device — the external flash contents must also be restored.

---

## Critical Bugs

| # | Issue | Source |
|---|-------|--------|
| 1 | **Auto-measurement hang** — long-pressing auto starts calibration with no escape; device must drain battery to recover | [EEVblog (ee00)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 2 | **GUI lockup on vertical position** — freezes when adjusting V-pos with probes connected; worsens when ground clips touch | [EEVblog (Rainwater)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 3 | **Flash filesystem corruption** — corrupts user storage (Drive 2) and system graphics (Drive 3), causing "Save failed" errors and missing display elements | [EEVblog (Dr. Blast)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |

## High-Priority Bugs

| # | Issue | Source |
|---|-------|--------|
| 4 | **Generator output depends on battery voltage** — DAC appears tied directly to Vbatt with no reference; at half charge, full 3.3V output is impossible | [EEVblog / Reddit](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 5 | **Signal generator inaccurate below 20Hz** — DDS produces wrong frequencies at low end; some values output entirely different frequencies | [EEVblog (Rainwater)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 6 | **FFT/spectrum analysis non-functional** — listed feature doesn't produce usable results | [EEVblog (tutochkin)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 7 | **Unexpected restarts on AUTO button** — device reboots/freezes during normal scope use | [OneSDR Review](https://www.onesdr.com/fnirsi-2c53t-oscilloscope-review/) |
| 8 | **Firmware update failures** — some Amazon units stuck on old versions, refuse to run past v1.0.3 | [EEVblog](https://www.eevblog.com/forum/testgear/assistance-needed-for-fnirsi-2c53t-firmware-update-issue/) |

## Feature Requests & Regressions

| # | Issue | Type | Source |
|---|-------|------|--------|
| 9 | **DMM average can't be reset** — no way to reset the running average without exiting and re-entering measurement mode | Feature Request | [Community (multiple)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 10 | **No min/max hold reset** — min/max values accumulate over entire session with no reset option | Feature Request | Community (multiple) |
| 11 | **Generator frequency reduced to 50kHz** — was 1MHz on predecessor 2C23T; biggest community complaint | Regression | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 12 | **Generator output is unipolar only** — no output capacitor or symmetrical supply; can't generate bipolar signals | Complaint | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 13 | **Generator max voltage reduced** — from 3.3V to 3V p-p vs predecessor | Regression | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 14 | **Generator sine wave shows visible steps/flattening** — unsuitable for high-quality signal applications | Complaint | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 15 | **Pause + timebase change doesn't update display** — frozen waveform persists regardless of timebase changes | Bug | [EEVblog (tutochkin)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 16 | **Trigger position inconsistent** — front edge not always centered on screen for single-shot captures | Bug | [EEVblog (tutochkin)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 17 | **Storage depth limited to ~1k points** — possible reduction from predecessor's 32KB | Complaint | [EEVblog](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |
| 18 | **Input stage nonlinearity** — amplitude increases slightly from 10MHz to 50MHz | Bug | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |

## UI / Usability Issues

| # | Issue | Source |
|---|-------|--------|
| 19 | **Screen too crowded** with multiple measurements displayed | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 20 | **Counter-intuitive navigation** — button functions change between modes; settings only accessible from startup screen | [Audio Investigations Blog](http://audioinvestigations.blogspot.com/2024/12/fnirst-2c53t-scope.html) |
| 21 | **Language selection confusion** — accidentally selecting Chinese makes device nearly unusable; requires right-arrow (not down) to switch back | [Audio Investigations Blog](http://audioinvestigations.blogspot.com/2024/12/fnirst-2c53t-scope.html) |
| 22 | **Manual is tiny and nearly unreadable** — online version described as "not very useful" either | [Audio Investigations Blog](http://audioinvestigations.blogspot.com/2024/12/fnirst-2c53t-scope.html) |
| 23 | **Long-press auto button has no warning** — silently starts calibration, leading to unintended hangs | [EEVblog (ee00)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |

## Hardware Issues (Not Firmware-Fixable)

| # | Issue | Source |
|---|-------|--------|
| 24 | **BNC connectors too close** — probes with plastic covers can't both fit simultaneously | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 25 | **Multimeter socket spacing non-standard** — slightly less than 19mm, incompatible with some accessories | [Elektor Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review) |
| 26 | **Difficult to disassemble** — rubber gasket appears permanently bonded; only reset pinhole accessible | [EEVblog (ee00)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/) |

## Reference: Existing Open-Source FNIRSI Firmware

The [FNIRSI 1013D open-source firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware) project is a community-developed replacement for a similar FNIRSI oscilloscope (FPGA+ARM+ADC architecture). It addressed display compatibility, touch calibration, and RMS measurement improvements — a useful reference for the 2C53T firmware rewrite.

## Sources

- [EEVblog Forum — Main 2C53T Thread](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/)
- [EEVblog Forum — 2C53T Reset Thread](https://www.eevblog.com/forum/testgear/fnirsi-2c53t-reset/)
- [EEVblog Forum — Firmware Update Issue](https://www.eevblog.com/forum/testgear/assistance-needed-for-fnirsi-2c53t-firmware-update-issue/)
- [Elektor Magazine Review](https://www.elektormagazine.com/review/fnirsi-2c53t-50-mhz-two-channel-oscilloscope-multimeter-generator-review)
- [Audio Investigations Blog](http://audioinvestigations.blogspot.com/2024/12/fnirst-2c53t-scope.html)
- [OneSDR Review](https://www.onesdr.com/fnirsi-2c53t-oscilloscope-review/)
- [OneSDR 2C23T vs 2C53T Comparison](https://www.onesdr.com/fnirsi-2c23t-vs-2c53t/)
- [Facebook FNIRSI Club — Bug List Thread](https://www.facebook.com/groups/1522719838657577/posts/1772023723727186/)
- [FNIRSI 1013D Open Source Firmware](https://github.com/pecostm32/FNIRSI_1013D_Firmware)
