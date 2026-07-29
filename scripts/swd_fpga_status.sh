#!/usr/bin/env bash
# Drive the FPGA's SSPI port directly from the host over SWD, on a PARKED target.
#
# Purpose: read the Gowin STATUS_REGISTER (0x41) at the CONFIG_ENABLE instant on
# *stock* firmware — a measurement we have never had. Stock never reads the
# status itself (it ignores the reply), so the only way to get it is to halt the
# MCU mid-sequence and clock the transaction ourselves.
#
# Requires a spin image parked with the 05/12 prelude sent, CS LOW, and 0x15 not
# yet clocked:
#   stock : /tmp/APP_2C53T_expE_spin_stock.bin
#   ours  : /tmp/OPENSCOPE_expE_spin_guest.bin   (make guest-spin)
#
# What it does, with the CPU halted:
#   1. verify the park from peripherals (pc is useless here — RDP artifact)
#   2. close stock's open CS frame
#   3. STATUS_BEFORE : read 0x41 at /256 (the only valid SSPI read clock)
#   4. send CONFIG_ENABLE (0x15 00) at /2, exactly as stock would have
#   5. STATUS_AFTER  : read 0x41 at /256 again
#
# Bit 7 of the FIRST status byte is SYSTEM_EDIT_MODE. Set => CONFIG_ENABLE
# engaged. Our firmware measures 0x00039020 here (bit 7 clear).
#
# SAFETY: reads/writes only SPI3 + GPIOB. Never touches PC9 (power hold).
# Unplug the ST-Link before any IAP flash; replug only once the image is parked.
#
# Usage:  scripts/swd_fpga_status.sh stock|ours
set -u

LABEL="${1:-dump}"
OUT="/tmp/swd_fpgastatus_${LABEL}.txt"
CFG="${HOME}/at32_attach.cfg"
[ -f "$CFG" ] || { echo "missing $CFG"; exit 1; }

# AT32F403A SPI3 (0x40003C00): CTRL1 +0x00, CTRL2 +0x04, STS +0x08, DT +0x0C.
# STS bit0 = RDBF (rx buffer full), bit1 = TDBE (tx buffer empty).
# GPIOB (0x40010C00): BSRR/scr +0x10, BRR/clr +0x14.  CS = PB6 = 0x40.
read -r -d '' TCL <<'EOF'
proc rd32 {a} { return [lindex [read_memory $a 32 1] 0] }
proc wr32 {a v} { write_memory $a 32 [list $v] }

proc cs_high {} { wr32 0x40010C10 0x40 }
proc cs_low  {} { wr32 0x40010C14 0x40 }

# Set the SPI3 baud divider without glitching: drop SPE, change BR, restore SPE.
proc set_br {br} {
    set c [rd32 0x40003C00]
    set c [expr {$c & ~0x40}]
    wr32 0x40003C00 $c
    set c [expr {($c & ~0x38) | (($br & 7) << 3)}]
    wr32 0x40003C00 $c
    wr32 0x40003C00 [expr {$c | 0x40}]
}

# Full-duplex byte exchange. Returns the received byte.
proc sx {b} {
    wr32 0x40003C0C $b
    for {set i 0} {$i < 400} {incr i} {
        if {[expr {[rd32 0x40003C08] & 1}]} { break }
    }
    return [expr {[rd32 0x40003C0C] & 0xff}]
}

proc read_status {tag} {
    cs_high
    rd32 0x40003C0C
    cs_low
    sx 0x41
    sx 0x00 ; sx 0x00 ; sx 0x00
    set b0 [sx 0x00] ; set b1 [sx 0x00] ; set b2 [sx 0x00] ; set b3 [sx 0x00]
    cs_high
    set v [expr {($b0<<24)|($b1<<16)|($b2<<8)|$b3}]
    echo [format "%-14s %02X %02X %02X %02X   = 0x%08X   EDIT_MODE(bit7 of b0) = %d" \
          $tag $b0 $b1 $b2 $b3 $v [expr {($b0 >> 7) & 1}]]
    return $v
}

halt

echo "=== park verification (peripherals, not pc) ==="
set c1 [rd32 0x40003C00]
set odr [rd32 0x40010C0C]
echo [format "SPI3 CTRL1 = 0x%08X   SPE=%d  BR=%d" $c1 [expr {($c1>>6)&1}] [expr {($c1>>3)&7}]]
echo [format "GPIOB ODR  = 0x%08X   PB6/CS=%d (0=asserted)  PB11=%d" \
      $odr [expr {($odr>>6)&1}] [expr {($odr>>11)&1}]]
if {[expr {($c1>>6)&1}] == 0} { echo "!! SPE clear — NOT parked in the config sequence. Aborting."; shutdown }

echo ""
echo "=== STATUS before CONFIG_ENABLE (read at /256) ==="
set_br 7
set before [read_status "BEFORE:"]

echo ""
echo "=== sending CONFIG_ENABLE (15 00) at /2, as stock would ==="
set_br 0
cs_low
sx 0x15
sx 0x00
cs_high

echo ""
echo "=== STATUS after CONFIG_ENABLE (read at /256) ==="
set_br 7
set after [read_status "AFTER: "]

echo ""
if {$before == $after} {
    echo ">>> UNCHANGED — CONFIG_ENABLE had no effect on this bus."
} else {
    echo [format ">>> CHANGED 0x%08X -> 0x%08X" $before $after]
}
if {[expr {($after >> 31) & 1}]} { echo ">>> EDIT_MODE ENGAGED — config entry succeeded." }
shutdown
EOF

echo "# SWD-driven FPGA status — label=${LABEL}  $(date -Is)" > "$OUT"
sudo timeout 90 openocd -f "$CFG" -c "init" -c "$TCL" 2>&1 | tee -a "$OUT"
echo
echo "wrote $OUT"
