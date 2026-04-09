# MCU-FPGA Gap Decisive Bench Plan 2026-04-08

Purpose:
- define the next live experiments that best separate:
  - `(a)` wrong selector-vs-wire reconstruction
  - `(b)` missing runtime posture / choreography
  - `(c)` missing stock image / state outside the current clean-room model
- keep the plan bounded and executable from the current USB CDC shell

Primary references:
- [scope_experiment_priorities_after_ripcord_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_experiment_priorities_after_ripcord_2026_04_08.md)
- [key_task_dispatch_surface_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/key_task_dispatch_surface_contradiction_2026_04_08.md)
- [right_panel_stage_entry_event_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_entry_event_split_2026_04_08.md)
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)
- [usb_debug.c](/Users/david/Desktop/osc/firmware/src/drivers/usb_debug.c)
- [fpga.c](/Users/david/Desktop/osc/firmware/src/drivers/fpga.c)

## Common Observation Set

Run these after each experiment block unless the block already includes them:

```text
status
gpio scan
fpga acq 3
spi3 read 32
```

Watch these fields every time:
- transport:
  - `TX count`
  - `RX bytes`
  - `Data frames`
  - `Echo frames`
- ready / mode pins:
  - `PC0`
  - `PB11`
  - `PC6`
  - `PC11`
- acquisition result:
  - `SPI3 OK`
  - `CH1/CH2 first bytes`
  - `varies`
  - flat `E3` buffer in `spi3 read 32`

Meaningful positive signal:
- `RX/DF/EF` increments in scope posture, or
- `PC0` goes low, or
- `fpga acq` stops being flat `FF`, or
- `spi3 read 32` stops being flat `E3`

Current failure signature:
- meter posture can produce replies
- scope posture stays `RX=0 DF=0 EF=0`
- `PC0=1`
- `fpga acq` remains all `FF`
- `spi3 read` remains all `E3`

## Experiment 1: Meter-Alive / Scope-Dead Control Pair

Goal:
- prove on the same boot that the FPGA is still alive in meter posture
- immediately test whether `scope wake + beat` closes the gap

Commands:

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
- meter replies come back, scope still dead:
  - this is not a total FPGA death case
  - remaining gap is scope-only: `(a)`, `(b)`, or `(c)`
- `scope wake + beat` starts `RX/DF/EF` or drops `PC0`:
  - `(b)` missing runtime posture / timer choreography is the leading cause
- even meter posture is dead:
  - stop here and debug basic transport / hardware before more scope work

## Experiment 2: Selector Path vs Final-Wire Path From The Same Posture

Goal:
- compare the staged selector-family model against the recovered final-word model
- hold posture as constant as possible

Path A: staged selector family

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
fpga acq 3
spi3 read 32
```

Path B: final wire-word family

```text
fpga diag clear
fpga stock clear
fpga stock state5 3 0
fpga stock reenter
fpga wire scope both
fpga scope beat 3 50
status
fpga acq 3
spi3 read 32
```

Interpretation:
- only Path A changes `RX/DF/EF`, `PC0`, or SPI:
  - `(b)` posture / choreography dominates
  - the selector-side model is closer than the current wire bank
- only Path B changes anything:
  - `(a)` selector-vs-wire split is still wrong
  - `0x20002D74` final words are closer than the current stock-shadow emulation
- both remain dead in the same way:
  - current selector and current wire reconstruction are both insufficient
  - move to payload-byte discrimination

## Experiment 3: Payload-Byte Discrimination On Confirmed Final Words

Goal:
- test whether the missing gap is in frame payload bytes `[4..8]`, not in the
  `cmd_hi/cmd_lo` pair itself

Rationale:
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)
  supports `0x02A0`, `0x0501`, `0x0503`, and the `0x050C/0x050D/...` families
  as real wire-word anchors
- current clean-room paths still mostly assume zero payload bytes

Bounded sweep:
- for each of `0x02A0`, `0x0501`, `0x0503`
- sweep only `p1`
- keep `p2..p5 = 0`
- use `p1 ∈ {0, 1, 2, 5, 9}`

Example:

```text
fpga diag clear
fpga frame 02 A0 00 00 00 00 00
fpga frame 02 A0 01 00 00 00 00
fpga frame 02 A0 02 00 00 00 00
fpga frame 02 A0 05 00 00 00 00
fpga frame 02 A0 09 00 00 00 00
status
```

Repeat the same shape for `05 01` and `05 03`, then run:

```text
gpio scan
fpga acq 3
spi3 read 32
```

Interpretation:
- any payload-sensitive change:
  - `(a)` wrong wire framing is now the leading cause
  - final words were plausible, but the current payload-byte model was too weak
- no payload-sensitive change at all:
  - payload framing is less likely to be the blocker
  - weight shifts toward `(b)` or `(c)`

## Experiment 4: Runtime-Choreography Split Test

Goal:
- determine whether one specific stock-like runtime branch is missing, rather
  than the whole command family set

Path A: coarse editor path (`state 6 -> state 5 -> selector 2 family`)

```text
fpga diag clear
fpga stock clear
fpga stock state6 3 0
fpga stock reenter
fpga stock next
fpga scope beat 3 50
status
fpga acq 3
spi3 read 32
```

Path B: staged-detail commit path (`state 5 -> selector 5 family`)

```text
fpga diag clear
fpga stock clear
fpga stock state5 3 0
fpga stock select
fpga stock commit
fpga stock commit
fpga stock commit
fpga stock reenter
fpga scope beat 3 50
status
fpga acq 3
spi3 read 32
```

Path C: packed preset consume path (`state 2 -> 9/1/2/0 -> 2`)

```text
fpga diag clear
fpga stock clear
fpga stock preset 9 1 2 0 1
fpga stock consume
fpga stock reenter
fpga scope beat 3 50
status
fpga acq 3
spi3 read 32
```

Interpretation:
- only one path wakes scope:
  - `(b)` missing runtime posture / choreography is the main gap
  - focus RE and firmware work on that one branch
- none of the three paths changes the failure signature:
  - current runtime reconstruction is saturated
  - `(c)` missing stock image / state rises sharply

## Stop Rule

If Experiments 1-4 all show this exact pattern:
- meter posture still produces replies
- every scope-side path stays `RX=0 DF=0 EF=0`
- `PC0` never drops
- SPI remains flat `FF/E3`

then stop spending bench time on new selector and wire guesses for now.

At that point the most defensible read is:
- current clean-room queue split is probably directionally right
- current scope runtime reconstruction is probably close enough to have shown
  some signal if the downloaded app state were sufficient
- the leading remaining cause becomes `(c)` missing stock image / state

That is the point to pivot back to:
- missing high-flash tables / descriptors
- stock image shape mismatch
- stock-only runtime state outside the current downloaded app
