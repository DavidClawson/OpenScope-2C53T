# Bench Sidecar Experiment 1 Shell Check 2026-04-08

Purpose:
- verify that Experiment 1 from `/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mcu_fpga_gap_experiment_map_2026_04_08.md` is executable on the current firmware shell
- tighten the command list to the smallest safe first hardware run

Verdict:
- Experiment 1 is executable on the current shell as shipped in `/Users/david/Desktop/osc/firmware/src/drivers/usb_debug.c`
- there are no command-name mismatches in the original block
- two practical tightenings are worth making:
  - add an initial `status` after `fpga diag clear` to confirm `Initialized: YES` and zeroed counters before the run
  - treat `spi3 read 32` as optional on the first pass, because `fpga acq 3` already prints the first sample bytes and whether data varies

Confirmed command spellings:
- `fpga diag clear`
- `fpga meter reinit 0`
- `fpga cmd 0 9`
- `fpga scope wake`
- `fpga scope beat 5 50`
- `status`
- `gpio scan`
- `fpga acq 3`
- `spi3 read 32`

Current-shell behavior that matters:
- `fpga diag clear` resets TX/RX/DF/EF/SPI counters, clears the last RX frame, clears SPI sample diagnostics, and resets the stock-shadow state
- `fpga meter reinit 0` is valid, but it does not print a delta block by itself
- `fpga cmd 0 9` is valid and does print a delta block after a short wait
- `fpga scope wake` is valid, but it does not print a delta block by itself
- `fpga scope beat 5 50` is valid and does print a delta block
- `fpga acq 3` is valid; here `3` is a direct low-level acquisition trigger byte, not the policy-mode path used by bare `fpga acq`

Prerequisites:
- boot the device into scope mode first
- confirm the USB CDC shell is alive
- after `fpga diag clear`, run `status` and confirm `Initialized: YES`
- if `Initialized: NO`, stop and fix basic FPGA init first; do not run the rest of the experiment

Exact first experiment to run:

```text
fpga diag clear
status
fpga meter reinit 0
fpga cmd 0 9
status
fpga scope wake
fpga scope beat 5 50
status
gpio scan
fpga acq 3
```

Optional follow-up only if the `fpga acq 3` result is ambiguous:

```text
spi3 read 32
```

Why this is the smallest safe first run:
- it keeps the original meter-alive vs scope-dead control pair intact
- it avoids the selector-path and wire-word helpers, so it only exercises already-confirmed shell commands
- it keeps one explicit low-level acquisition read at the end, but drops the raw buffer dump unless needed

What to look for:
- after `fpga cmd 0 9`, meter posture should be considered alive if `RX bytes` and/or `Data frames` increase
- after `fpga scope beat 5 50`, scope posture is still flat if:
  - `RX bytes`, `Data frames`, and `Echo frames` remain unchanged
  - `gpio scan` shows `PC0 (data ready): 1`
  - `fpga acq 3` reports flat `FF` data or no useful SPI variation

Notes:
- `status` already includes the stock-shadow printout, so `fpga stock diag` is not needed for this first control experiment
- `gpio scan` is the only command in this first block that directly exposes `PC0`
- if this tightened block still shows meter alive and scope flat on the same boot, move to Experiment 2; if meter also stays flat, stop and debug transport/basic liveness before any scope-specific command work
