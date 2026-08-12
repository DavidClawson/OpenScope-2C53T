# Bench plan 2026-08-13 — arming day

Context: 2026-08-12 broke the config-entry wall twice. Our side: warm handoff
bench-confirmed (live, flicker-free, DC-responding scope under `guest-warmtest`;
commits `664329d`..`9296580`; recipe + findings in `docs/fpga_warm_handoff_test.md`).
Community side: **Stlkv cold-started the FPGA** with maksidze's 2C23T-V0.4
bit-bang loader on our pins with our payload — STATUS `0x0003F460`, DONE_FINAL
set, 4/4 reproducible (#18, report pkg mirrored in the session scratchpad;
code: `Stlkv/OpenScope-2C23T-2C53T-port` branch `2c53t-port`). Remaining gap to
power-button→live-trace: the capture engine starts UNARMED after config.

## 1. PR #13 review (PROMISED to Komzpa — do first)
Same treatment as #16: fetch, build every target, run the test scripts, read
the diff, then hardware-check what's checkable. Extra lens: the meter is a
separate SD7501-family DMM SoC on USART2 (NOT the FPGA — maksidze's trace,
#18) — re-examine any DMM frontend logic in the PR that assumed FPGA behavior.
Deliverable: review comment; merge if green.

## 2. Netlist session — Stlkv's two asks (owed, posted in #18)
Materials: `reverse_engineering/analysis_v120/mcu_fpga_boundary_reconcile_2026-06-13.md`
(R3 §5), the apicula netlist tooling (m_arming.py or successor — locate it),
`R3_CAPTURE_ARMING_FROM_APICULA.md`.
- (a) Which SPI-control register address+bit sources the CEA/run cone
  (the SI-sourced bit R3 traced). Stlkv can put any value on any register
  same-night once we name it.
- (b) Is `IOB7B` (capture co-enable) PC6 or PC11? If PC11, one-line fix —
  we hold PC11 LOW (meter-MUX-off posture) everywhere.
Deliverable: both answers posted to #18.

## 3. V0.4 config-entry A/B on unit #1 — isolate the load-bearing variable
Stlkv's cold start used our pins (PB6 CS!) and our payload, so only three
candidates remain: slow/gapped bit-bang clocking of the WHOLE sequence, the
richer prelude (0x11/0x13/0x41 reads before 0x12/0x15), GPIO-mode vs SPI-AF pins.
- **Build A:** our hardware-SPI3 `fpga_spi3_config_sequence` with (i) cmd_br
  AND upload_br at /256, (ii) prelude reads 0x11/0x13/0x41 inserted before
  INIT_ADDR. Success = CFG field shows DONE (D1) / STATUS `0x0003F460`-class;
  expect one-shot 04/05 baseline. If it works: clock+prelude sufficient,
  bit-bang unnecessary — fold into fpga_init as the default boot path.
- **Build B (only if A fails):** true bit-bang transplant (GPIO-mode PB3/4/5,
  port from Stlkv's branch). Then A/B GPIO-vs-AF as the final variable.
- NOTE upload at /256 takes 115,638 B × 8 × (256/120MHz) ≈ 2.0 s — acceptable.
  Also try /8 or /16 middle ground if /256 works (boot-time optimization).
- **If config entry lands in OUR firmware, the stock-stub chainload idea is
  DEAD — cold boot straight into OpenScope becomes the shippable path** (meets
  the no-case-crack constraint). Update CLAUDE.md + memory accordingly.

## 4. Engine-arm hunt (uses #2's answer; can interleave with #3)
On a configured-but-unarmed design (post-Build-A or via warm handoff):
- Test the named run-register bit from #2 directly.
- Replicate stock's post-close shape exactly: 600 ms gap → five writes →
  0x03 read; watch byte 1 (`01` = armed hypothesis, Stlkv's finding).
- If IOB7B=PC11: try PC11 HIGH post-config (contradicts meter-MUX naming —
  cheap to test).
- Remember from today: engine free-runs once armed and survives MCU resets;
  MENU+Power stops it (stock shutdown); five writes do NOT start it.

## 5. TMR13 CH2 trigger reference (v7)
Ripcord contract 38: CH2's comparator reference is TMR13 CH1 PWM
(`C1DT @ 0x40001C34`), same cal formula as DAC1. Our firmware never touches
TMR13. Need stock's ARR/prescaler (decode around `0x08008C5C` / FUN_08008A58,
or ask the ripcord session when it's back — it offered). Bring-up mirrors the
DAC1 fix; validates against Stlkv's CH2 mystery.

## 6. Housekeeping
- Morning `/repo-pulse` — expect a big traffic/engagement day after #18.
- **`git push` — local main is now ~8 commits ahead of origin** (includes the
  two pre-session commits). David's call/creds.
- Ripcord session (when back): David to decide on the F2C23T-GW-EN_V2.0.1.bin
  fetch for the V0.4 loader liveness decode (does it branch on prelude reads
  or fire blind — sharpens Build A's interpretation).
- Refresh-rate target once armed: stock reads 04/05 pairs at ~29 ms (~34 Hz).
- Stretch: show-and-tell video once boot-to-trace is joined end-to-end.
