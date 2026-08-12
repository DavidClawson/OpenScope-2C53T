# Bench plan — next session (after the 2026-08-13 breakthrough)

Context: 2026-08-13 broke the config-entry wall AND reached **cold-boot-to-live-scope**
on our own firmware (`guest-coldtrace`; devlog `2026-08-13-cold-boot-to-scope.md`;
memory `config-entry-solved-buildB`). Config entry = bit-bang loader (Build B) not
hardware-SPI (Build A). Engine arm = the five writes with PB11(IOR1B)+PC6(IOB7B)
held. Live readout = the warmtest 0x04/0x05 pipeline. The scope now CAPTURES from
cold but has no timebase, so it only tracks slow signals; audio-band waveforms alias.

Bench aids ready: ESP32 signal generator on the LOLIN D32 PRO (`esp32_siggen/`,
gitignored) — serial-controllable (`sine/square/tri/saw/dc/amp` @ 115200 on
/dev/ttyUSB0), DAC1(GPIO25)→CH1, GND→ground. The agent can drive it live.

## 1. Timebase control — THE feature that makes it a usable scope ⭐
The blocker to displaying real waveforms: `guest-coldtrace` never sets the FPGA
sample rate, so each 1024-sample sweep is ~µs long, refreshed ~34 Hz. Slow signals
track as a moving level; >~15 Hz aliases.
- Send the timebase commands (`0x0F` prescaler / `0x10` period / `0x11` mode — see
  `fpga.h` FPGA_CMD_SCOPE_CFG_*) in the coldtrace/scope path to slow acquisition so
  1024 samples span ms.
- Decode stock's per-timebase values (ripcord: TMR3 is stock's pacer, 9-entry 1-2-5
  PR table; but TMR3 is OURS for the 500 Hz button scan — resolve the conflict).
- Wire the timebase up/down buttons (`scope_adjust_timebase` already exists) to
  re-send the config.
- **Validate with the ESP32:** drive a known 1 kHz square, confirm the period reads
  correctly across timebase steps, and watch the wave stretch/compress. This is the
  first real calibration opportunity.
Deliverable: a `guest-coldtrace`-derived build that shows a stable multi-cycle sine.

## 2. Fold cold-boot-to-scope into the DEFAULT boot path (shippability)
`guest-coldtrace` is a special build. Make cold config + arm + readout the default
so a normal `make guest` boots to a live scope — the shippable path (no case-crack).
- Reconcile with the meter path (config runs, THEN meter USART init) and the mode
  boot default (currently MODE_MULTIMETER).
- Keep the warm-handoff build around for A/B.
Deliverable: `make guest` cold-boots to scope; update CLAUDE.md.

## 3. Config-entry bisect — which single variable broke the wall
Build B differs from Build A in TWO ways (bit-bang-vs-AF AND no-0x05 ERASE). Isolate:
- Build B + re-add `0x05` ERASE → still works? (isolates the 0x05 variable)
- Build A (hardware-SPI) + inter-byte gaps / GPIO-mode PB3/4/5 → does AF ever work?
Why it matters: hardware-SPI (if it can be made to work) is ~faster than bit-bang
and cleaner. If bit-bang is truly required, document why (GW1N config-engine timing).

## 4. CH2 cleanup + trigger source
David observed CH1/CH2 "triggering each other" and CH2 coupling CH1's signal.
- Bring up the CH2 trigger reference: `guest-warmtest-ch2` (TMR13 CH1 PWM on PA6 —
  decoded, UNCONFIRMED; watch for CH2 responding, and for PA6/frontend conflict).
- Per-channel trigger source select (stop the cross-triggering).
- Confirm PA6 = TMR13_CH1 on the bench (graduate the HARDWARE_PINOUT entry).

## 5. Real calibration (now that we can inject known signals)
Placeholder cal (base=0, upper=4095) makes vertical/trigger rough. With the ESP32
producing known DC levels and amplitudes, derive real per-range vertical scale +
offset and the trigger-DAC cal. Start with DCV points, then Vpp.

## 6. PR #13 (Komzpa) — follow through on the review
4 blockers filed (SPI3_GMUX revert, trigger-byte interception, H2 SHA guard, unit-
table retraction) + should-fixes. Options: (a) wait for Komzpa's fixes, (b) push the
mechanical B1–B4 fixes to his branch ourselves (offered in the review). Re-review,
merge when green. Hardware-check the merged build (task #5 from last session).

## 7. USB data out / PC remote (issue #10)
The app's USB CDC doesn't enumerate on unit #1 (suspected HICK clock drift). Fixing
it unlocks scope-data-to-host (the debug shell already has trace/screenshot cmds)
and answers issue #10. Path: copy how stock clocks USB (48 MHz divider at 240 MHz
SYSCLK) into our app. The ESP32 could also serve as a bridge later.

## 8. Community / housekeeping
- Watch #18 for Stlkv/maksidze reactions to the breakthrough; answer the netlist
  follow-ups Stlkv asked for (single-write arm-address sweep, runtime read framing).
- Repo at 93⭐ — expect a bump after the cold-boot post.
- Commit + push the devlog entry and this plan.
- Consider a short show-and-tell video (cold boot → live trace) now that it's real.

## Priority order
1 (timebase) → 2 (default boot) → then 4 (CH2) / 5 (cal) / 3 (bisect) as they unblock
UI work, with 6 (PR #13) and 7 (USB) interleaved. Item 1 is the one that turns
"captures" into "usable."
