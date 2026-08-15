# FPGA reconstruction workspace

This directory is the editable, evidence-tracked reconstruction of the FNIRSI
2C53T scope FPGA. It is not the original vendor RTL.

The target is the physically identified Gowin **GW1N-UV2 in QN48**, a member of
the **GW1N-2** family. The stock configuration stream carries IDCODE
`0x0120681B`. Package identity and device identity are established; board signal
pin assignments are deliberately not copied here until the official QN48 table
and board continuity results are integrated together.

## Layout

- `rtl/` — small, reviewed SystemVerilog blocks reconstructed from observed
  behavior and structural evidence.
- `sim/` — focused simulation testbenches for those blocks.
- `tools/` and `tests/` — structural-comparison helpers and their tests.
- `docs/provenance.md` — immutable stock inputs, hashes, and tool revisions.
- `docs/reconstruction.md` — recovered block map, protocol facts, and unknowns.
- `docs/verification.md` — proof levels and acceptance criteria.

`rtl/capture_channel.sv` is currently a portable circular capture-memory
primitive. Its unit test proves wrap, freeze, readback, and the disabled-capture
negative control. It does **not** yet prove Gowin BSRAM inference, stock memory
ordering, trigger behavior, timing closure, or hardware equivalence.

## Quick local checks

When Icarus Verilog and Verilator are installed:

```sh
iverilog -g2012 -s tb_capture_channel \
  -o /tmp/tb_capture_channel \
  fpga/rtl/capture_channel.sv fpga/sim/tb_capture_channel.sv
vvp /tmp/tb_capture_channel
verilator --lint-only --timing -Wall fpga/rtl/capture_channel.sv
python3 -m unittest fpga/tests/test_netlist_verification.py
```

These are simulation and helper checks only. See
[`docs/verification.md`](docs/verification.md) before making a stronger claim.

## Safety

All target experiments must load volatile **SRAM only**. Never write the FPGA's
internal non-volatile flash: it contains the stock meter design and no verified
recovery image exists. No file in this directory authorizes flashing hardware.
