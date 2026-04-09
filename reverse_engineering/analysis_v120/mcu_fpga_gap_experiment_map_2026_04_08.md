# MCU-FPGA Gap Experiment Map 2026-04-08

Purpose:
- turn the broader decisive bench plan into one execution-oriented sequence
- keep the first three experiments bounded, comparable, and easy to run from the current USB CDC shell
- define a hard stop rule for when command guessing is no longer the best use of time

Current command surface:
- shell commands confirmed in [usb_debug.c](/Users/david/Desktop/osc/firmware/src/drivers/usb_debug.c)
- bench helpers confirmed in [fpga.c](/Users/david/Desktop/osc/firmware/src/drivers/fpga.c)

Assumptions:
- device is booted into scope mode
- USB CDC shell is alive
- current firmware includes:
  - `fpga diag clear`
  - `fpga meter reinit`
  - `fpga scope wake`
  - `fpga scope beat`
  - `fpga stock ...`
  - `fpga wire ...`
  - `fpga frame`
  - `fpga acq`
  - `spi3 read`

## Common Readout

Run this observation block after each experiment:

```text
status
gpio scan
fpga acq 3
spi3 read 32
```

Positive signal means any of:
- `RX bytes` increases in scope posture
- `Data frames` or `Echo frames` increase in scope posture
- `PC0` goes low
- `fpga acq 3` stops returning flat `FF`
- `spi3 read 32` stops returning flat `E3`

Flat failure signature means all of:
- `RX bytes = 0`
- `Data frames = 0`
- `Echo frames = 0`
- `PC0 = 1`
- `fpga acq 3` flat `FF`
- `spi3 read 32` flat `E3`

## Experiment 1: Meter-Alive / Scope-Dead Control Pair

Question:
- does the board still prove FPGA liveness in meter posture on the same boot where scope posture stays dead?

Run:

```text
fpga diag clear
fpga meter reinit 0
status
fpga cmd 0 9
status
fpga scope wake
fpga scope beat 5 50
status
gpio scan
fpga acq 3
spi3 read 32
```

Interpretation:
- meter posture replies, scope posture still flat:
  - not a total FPGA-death case
  - remaining gap is scope-specific
- `scope wake` or `scope beat` starts `RX/DF/EF`, drops `PC0`, or changes SPI:
  - missing runtime posture or timer choreography is the leading cause
- even meter posture stays flat:
  - stop scope experiments and debug basic transport or hardware first

Decision:
- if meter posture is alive and scope posture is still flat, continue to Experiment 2

## Experiment 2: Selector Path vs Final-Wire Path

Question:
- from the same staged posture, is the selector-family reconstruction or the final-wire-word reconstruction closer to stock?

Path A: selector-family staging

```text
fpga diag clear
fpga stock clear
fpga stock state5 3 0
fpga stock select
fpga stock commit
fpga stock commit
fpga stock reenter
fpga scope beat 3 50
status
gpio scan
fpga acq 3
spi3 read 32
```

Path B: final-wire-word staging

```text
fpga diag clear
fpga stock clear
fpga stock state5 3 0
fpga stock reenter
fpga wire scope both
fpga scope beat 3 50
status
gpio scan
fpga acq 3
spi3 read 32
```

Interpretation:
- only Path A changes the failure signature:
  - runtime posture/choreography is closer than the current wire-word bank
- only Path B changes the failure signature:
  - selector-vs-wire split is still wrong
  - current `0x20002D74` wire-word materialization is closer than the selector-side emulation
- both remain flat in the same way:
  - current selector-side and wire-side reconstructions are both insufficient
  - continue to Experiment 3

Decision:
- only continue to Experiment 3 if both Path A and Path B remain flat

## Experiment 3: Payload-Byte Discrimination On Wire Anchors

Question:
- are the `cmd_hi/cmd_lo` anchors plausible, but payload bytes `[4..8]` still wrong?

Scope:
- test only three wire anchors:
  - `0x02A0`
  - `0x0501`
  - `0x0503`
- vary only `p1`
- keep `p2..p5 = 0`
- use `p1 = 00, 01, 02, 05, 09`

Run:

```text
fpga diag clear
fpga frame 02 A0 00 00 00 00 00
fpga frame 02 A0 01 00 00 00 00
fpga frame 02 A0 02 00 00 00 00
fpga frame 02 A0 05 00 00 00 00
fpga frame 02 A0 09 00 00 00 00
status
gpio scan
fpga acq 3
spi3 read 32
```

```text
fpga diag clear
fpga frame 05 01 00 00 00 00 00
fpga frame 05 01 01 00 00 00 00
fpga frame 05 01 02 00 00 00 00
fpga frame 05 01 05 00 00 00 00
fpga frame 05 01 09 00 00 00 00
status
gpio scan
fpga acq 3
spi3 read 32
```

```text
fpga diag clear
fpga frame 05 03 00 00 00 00 00
fpga frame 05 03 01 00 00 00 00
fpga frame 05 03 02 00 00 00 00
fpga frame 05 03 05 00 00 00 00
fpga frame 05 03 09 00 00 00 00
status
gpio scan
fpga acq 3
spi3 read 32
```

Interpretation:
- any payload-sensitive change:
  - payload framing is still a live command-side lead
  - follow up with bounded `p2` or `p1/p2` discrimination on the winning anchor only
- no payload-sensitive change on all three anchors:
  - command-side guessing is now low-yield
  - pivot back to missing stock image/state

## Hard Stop Rule

Stop guessing new commands and new selector families if all three conditions hold:
- Experiment 1 proves meter posture is alive while scope posture stays flat
- Experiment 2 shows both selector-path and wire-path runs fail with the same flat signature
- Experiment 3 shows no payload-sensitive change on `0x02A0`, `0x0501`, or `0x0503`

At that point, the leading causes are no longer “one more missing command”:
- missing MCU-side high-flash data/descriptors in the downloaded stock app
- missing non-app MCU-side state below or outside the standalone app
- smaller chance: external-flash metadata mismatch

Pivot targets after the hard stop:
- [missing_image_state_hypothesis_ranked_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/missing_image_state_hypothesis_ranked_2026_04_08.md)
- W25Q128 boot sniff during stock-app boot attempt
- second-unit W25Q128 dump comparison

## Minimal Logging Format

Record one line per run:

```text
[expX-pathY] TX=? RX=? DF=? EF=? PC0=? SPI3OK=? ACQ=? SPI32=? note=?
```

Examples:
- `ACQ=flat-ff`
- `SPI32=flat-e3`
- `note=pc0-low`
- `note=rx-started`
