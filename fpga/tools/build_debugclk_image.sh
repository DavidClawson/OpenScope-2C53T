#!/bin/sh
# Build the debug-clock hardware validation image for the exact GW1N-UV2/QN48
# part.  Produces an SRAM-load-only .fs bitstream in a build directory outside
# the repository; build products are deliberately not committed.  Nothing here
# authorizes writing the FPGA's non-volatile flash.
#
# Host-provided prerequisites (tool provenance in fpga/docs/provenance.md):
#   APICULA_PYTHON  python with apycula installed from the apicula branch
#                   carrying the GW1N-UV2 QN48 pinout and speed-keyed package
#                   (agent/gw1n2-qn48-chipdb-20260815) plus a built
#                   apycula/GW1N-2.msgpack.xz chip database
#   NEXTPNR         nextpnr-himbaechel binary built from the gw1n2 branch
#   NEXTPNR_SRC     that nextpnr source tree (for gowin_arch_gen.py)
#   BBASM           the bba assembler from the same nextpnr build
set -eu

REPO=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${OUT:-"${TMPDIR:-/tmp}/openscope-debugclk-build"}
mkdir -p "$OUT"

RTL="$REPO/fpga/rtl/capture_channel.sv
$REPO/fpga/rtl/trigger_timebase.sv
$REPO/fpga/rtl/trigger_comparator.sv
$REPO/fpga/rtl/spi_runtime_interface.sv
$REPO/fpga/rtl/spi_control_registers.sv
$REPO/fpga/rtl/rate_divider.sv
$REPO/fpga/rtl/fnirsi_2c53t_top.sv
$REPO/fpga/rtl/debugclk_hw_top.sv"

yosys -Q -p "read_verilog -sv $(printf '%s ' $RTL); \
    synth_gowin -family gw1n -top debugclk_hw_top -json $OUT/debugclk_hw_top.json"

"$APICULA_PYTHON" "$NEXTPNR_SRC/himbaechel/uarch/gowin/gowin_arch_gen.py" \
    -d GW1N-2 -o "$OUT/chipdb-GW1N-2.bba"
"$BBASM" --l "$OUT/chipdb-GW1N-2.bba" "$OUT/chipdb-GW1N-2.bin"

# The fully qualified ordering code is required: the nextpnr gowin uarch
# splits the speed grade off the device string, and gowin_pack looks the full
# code up in the chip database.  disable_gp_clock_routing keeps the CS/SCLK
# nets off the dedicated clock network, which cannot reach their sinks from
# these pads; at MCU debug-clock rates fabric routing has orders-of-magnitude
# timing margin (reported >90 MHz).
"$NEXTPNR" --json "$OUT/debugclk_hw_top.json" \
    --write "$OUT/debugclk_hw_top_routed.json" \
    --device 'GW1N-UV2QN48XFC6/I5' \
    --chipdb "$OUT/chipdb-GW1N-2.bin" \
    --vopt family=GW1N-2 \
    --vopt disable_gp_clock_routing \
    --vopt cst="$REPO/fpga/constraints/fnirsi_2c53t_qn48.cst"

"$(dirname -- "$APICULA_PYTHON")/gowin_pack" -d GW1N-2 \
    -o "$OUT/debugclk_hw_top.fs" "$OUT/debugclk_hw_top_routed.json"

sha256sum "$OUT/debugclk_hw_top.fs"
echo "SRAM-only image ready: $OUT/debugclk_hw_top.fs"
