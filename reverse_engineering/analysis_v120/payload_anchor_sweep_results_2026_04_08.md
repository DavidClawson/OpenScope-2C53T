# Payload Anchor Sweep Results 2026-04-08

Purpose:
- record the bounded Experiment 3 payload-byte discrimination run from the live
  USB CDC shell
- determine whether payload byte `p1` changes the scope failure signature for
  the three best current wire anchors:
  - `0x02A0`
  - `0x0501`
  - `0x0503`

Primary reference:
- [mcu_fpga_gap_experiment_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mcu_fpga_gap_experiment_map_2026_04_08.md)

## Setup

All runs were performed on the current bench firmware with:
- device booted normally
- CDC shell alive
- common observation block:
  - `status`
  - `gpio scan`
  - `fpga acq 3`
  - `spi3 read 32`

Sweep scope:
- vary only payload byte `p1`
- keep `p2..p5 = 0`
- tested `p1 = 0x00, 0x01, 0x02, 0x05, 0x09`

## Results

### Anchor `0x02A0`

Commands:

```text
fpga diag clear
fpga frame 0x02 0xA0 0x00 0 0 0 0
fpga frame 0x02 0xA0 0x01 0 0 0 0
fpga frame 0x02 0xA0 0x02 0 0 0 0
fpga frame 0x02 0xA0 0x05 0 0 0 0
fpga frame 0x02 0xA0 0x09 0 0 0 0
```

Observed end state:
- `RX bytes = 0`
- `Data frames = 0`
- `Echo frames = 0`
- `PC0 = 1`
- `fpga acq 3` flat `FF`
- `spi3 read 32` flat `E3`

Interpretation:
- no payload-sensitive effect from `p1` on the `0x02A0` anchor

### Anchor `0x0501`

Commands:

```text
fpga diag clear
fpga frame 0x05 0x01 0x00 0 0 0 0
fpga frame 0x05 0x01 0x01 0 0 0 0
fpga frame 0x05 0x01 0x02 0 0 0 0
fpga frame 0x05 0x01 0x05 0 0 0 0
fpga frame 0x05 0x01 0x09 0 0 0 0
```

Observed end state:
- `RX bytes = 0`
- `Data frames = 0`
- `Echo frames = 0`
- `PC0 = 1`
- `fpga acq 3` flat `FF`
- `spi3 read 32` flat `E3`

Interpretation:
- no payload-sensitive effect from `p1` on the `0x0501` anchor

### Anchor `0x0503`

Commands:

```text
fpga diag clear
fpga frame 0x05 0x03 0x00 0 0 0 0
fpga frame 0x05 0x03 0x01 0 0 0 0
fpga frame 0x05 0x03 0x02 0 0 0 0
fpga frame 0x05 0x03 0x05 0 0 0 0
fpga frame 0x05 0x03 0x09 0 0 0 0
```

Observed end state:
- `RX bytes = 0`
- `Data frames = 0`
- `Echo frames = 0`
- `PC0 = 1`
- `fpga acq 3` flat `FF`
- `spi3 read 32` flat `E3`

Interpretation:
- no payload-sensitive effect from `p1` on the `0x0503` anchor

## Conclusion

The bounded `p1` sweep produced the same flat scope-failure signature on all
three current wire anchors.

That satisfies the command-side hard stop from
[mcu_fpga_gap_experiment_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mcu_fpga_gap_experiment_map_2026_04_08.md):

- meter posture has been shown alive on prior runs
- selector-path and wire-path experiments both stayed insufficient
- payload variation on `0x02A0`, `0x0501`, and `0x0503` produced no positive
  scope-side change

So the leading causes should now be treated as:
- missing MCU-side high-flash data or descriptors in the downloaded stock app
- missing adjacent stock-only runtime state not reconstructed in the clean-room
  firmware
- smaller possibility: external-flash metadata mismatch

Recommended pivot:
- stop broad command guessing
- prioritize stock-state / image-dependency falsifiers, especially a W25Q128 bus
  sniff during stock-app boot
