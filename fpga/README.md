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

The top-level default has two 1024 x 8 raw capture stores. The portable
registered-read/write split maps through the installed Yosys Gowin mapper to
two `DPX9B` BSRAM primitives, one per channel. Each store needs 8192 bits; the
mapper's 14-bit, byte-addressed BSRAM primitive has 18432 initialization bits,
so the two stores need two primitives rather than one shared store. This is a
resource-mapping result, not a claim about stock order, trigger behavior, or
the sample-clock/SPI-clock CDC protocol.

The unit test proves wrap, freeze, readback, and the disabled-capture negative
control. The BSRAM check proves the target synthesis netlist retains two `DPX9B`
cells and rejects a register fallback. The register, comparator, and divider
blocks carry their own testbenches for the bench-grounded runtime writes,
digital trigger level, opcode aliasing, and divided-tick contracts. None of
these checks proves stock memory order, trigger behavior, timing closure, or
hardware equivalence.

## Debug-clock hardware image

`rtl/debugclk_hw_top.sv` plus `constraints/fnirsi_2c53t_qn48.cst` form the
first buildable image for the exact GW1N-UV2/QN48 part. It uses only the six
pins with netlist-grade evidence (runtime SPI, the IOR1B run line, and the
clock-capable IOB7B pad as an MCU-driven debug sample clock) and feeds the
capture path synthetic on-chip ramp and walking-one data, so no unproven ADC,
clock, or board net is touched. `tools/build_debugclk_image.sh` reproduces the
`.fs` build (synthesis, exact-part place-and-route, packing) with pinned tools;
build products stay out of the tree. The image is for volatile SRAM loading
only and has not yet been proven on hardware.

## Quick local checks

When Icarus Verilog and Verilator are installed:

```sh
iverilog -g2012 -s tb_capture_channel \
  -o /tmp/tb_capture_channel \
  fpga/rtl/capture_channel.sv fpga/sim/tb_capture_channel.sv
vvp /tmp/tb_capture_channel
verilator --lint-only --timing -Wall \
  fpga/rtl/capture_channel.sv fpga/rtl/trigger_timebase.sv \
  fpga/rtl/trigger_comparator.sv fpga/rtl/spi_runtime_interface.sv \
  fpga/rtl/spi_control_registers.sv fpga/rtl/rate_divider.sv \
  fpga/rtl/fnirsi_2c53t_top.sv fpga/rtl/debugclk_hw_top.sv
python3 -m unittest fpga/tests/test_capture_bsram_mapping.py
python3 -m unittest fpga/tests/test_netlist_verification.py
```

These are simulation and helper checks only. See
[`docs/verification.md`](docs/verification.md) before making a stronger claim.

## Safety

All target experiments must load volatile **SRAM only**. Never write the FPGA's
internal non-volatile flash: it contains the stock meter design and no verified
recovery image exists. No file in this directory authorizes flashing hardware.
