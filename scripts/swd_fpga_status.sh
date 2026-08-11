#!/usr/bin/env bash
# Drive the FPGA's SSPI port directly from the host over SWD, on a PARKED target.
#
# Purpose: read the Gowin STATUS_REGISTER (0x41) at the CONFIG_ENABLE instant on
# *stock* firmware — a measurement we have never had. Stock never reads the
# status itself (it ignores the reply), so the only way to get it is to halt the
# MCU mid-sequence and clock the transaction ourselves.
#
# ── ANCHORED (2026-07-28, Exp J) ──────────────────────────────────────────────
# Every read here is now validated against a KNOWN ANSWER before anything else is
# believed: opcode 0x11 must return IDCODE 0x0120681B (Gowin GW1N-2 family;
# established independently from the .fs preamble at file offset 0x4AD19).
#
# This is not ceremony. Three separate artifacts survived for weeks in this
# project purely because no measurement had a known correct answer:
#   * reads at /2 return garbage    -> the persistent "80 01 C8 10"
#   * PB4/MISO left floating        -> no defined idle level
#   * this script's own 4-byte read -> reported a 2-byte rotation as EDIT_MODE
# A stable wrong number is indistinguishable from a right one. If the IDCODE does
# not come back, this script ABORTS rather than print a plausible-looking value.
#
# Two further fixes over the first version:
#   * reads are 8 bytes, not 4, and every value is located by sliding a 32-bit
#     window across all 33 bit alignments — so a phase-shifted-but-real reply is
#     reported as such instead of being mistaken for a different value.
#   * SYSTEM_EDIT_MODE is bit 7 of the ASSEMBLED 32-bit word (i.e. byte[3]).
#     The first version tested bit 31 and would have missed a real hit.
#
# Requires a spin image parked with the 05/12 prelude sent, CS LOW, and 0x15 not
# yet clocked:
#   stock : /tmp/APP_2C53T_expE_spin_stock.bin
#   ours  : /tmp/OPENSCOPE_expE_spin_guest.bin   (make guest-spin)
#
# What it does, with the CPU halted:
#   1. verify the park from peripherals (pc is useless here — RDP artifact)
#   2. close stock's open CS frame
#   3. ANCHOR: read 0x11, require IDCODE 0x0120681B, record its bit offset
#   4. STATUS_BEFORE : read 0x41 at /256, corrected by the anchor offset
#   5. send CONFIG_ENABLE (0x15 00) at /2, exactly as stock would have
#   6. STATUS_AFTER  : read 0x41 again, and re-anchor to prove the bus survived
#
# CAVEAT on step 5: SWD forces ~100us between injected bytes vs ~17us native. If
# the GW1N SSPI has a frame timeout, the injected CONFIG_ENABLE is not equivalent
# to stock's. The BEFORE reading does not depend on it and stands alone.
#
# ⚠ DO NOT RUN THIS AGAINST A FULLY-BOOTED, WORKING DEVICE unless you intend to
# disturb it. Once the FPGA is configured, this SPI port belongs to the USER
# DESIGN and carries ADC sample data — asserting CS and clocking 12 bytes into it
# injects garbage mid-stream and desynchronises acquisition (observed 2026-07-28,
# Exp L: the scope trace stopped). Harmless and fully recovered by a POWER-cycle,
# which makes stock re-run its boot upload. Nothing touches the FPGA's NV flash.
# The intended targets are the PARKED spin images, where the config port is still
# the thing on the other end of the wire.
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

# Read a Gowin register, sibling/openFPGALoader framing: a bare clock with CS
# HIGH to frame, then CS LOW, the 4-byte command word <opcode 00 00 00>, then
# clock the reply. EIGHT bytes so a repeating 4-byte group is visible as data.
proc read_reg8 {opcode} {
    cs_high
    sx 0x00
    cs_low
    sx $opcode
    sx 0x00 ; sx 0x00 ; sx 0x00
    set out {}
    for {set i 0} {$i < 8} {incr i} { lappend out [sx 0x00] }
    cs_high
    return $out
}

proc as64 {bytes} {
    set w 0
    foreach b $bytes { set w [expr {($w << 8) | $b}] }
    return $w
}

# 32-bit window at bit offset s (0..32) of the 64-bit reply.
proc win32 {bytes s} {
    return [expr {([as64 $bytes] >> (32 - $s)) & 0xFFFFFFFF}]
}

# Slide across all 33 alignments looking for a known value. -1 = absent.
proc find_at {bytes target} {
    for {set s 0} {$s <= 32} {incr s} {
        if {[win32 $bytes $s] == $target} { return $s }
    }
    return -1
}

proc hex8 {bytes} {
    set s ""
    foreach b $bytes { append s [format "%02X" $b] }
    return $s
}

halt

echo "=== park verification (peripherals, not pc) ==="
set c1 [rd32 0x40003C00]
set odr [rd32 0x40010C0C]
echo [format "SPI3 CTRL1 = 0x%08X   SPE=%d  BR=%d" $c1 [expr {($c1>>6)&1}] [expr {($c1>>3)&7}]]
echo [format "GPIOB ODR  = 0x%08X   PB6/CS=%d (0=asserted)  PB11=%d" \
      $odr [expr {($odr>>6)&1}] [expr {($odr>>11)&1}]]
if {[expr {($c1>>6)&1}] == 0} { echo "!! SPE clear — NOT parked in the config sequence. Aborting."; resume; shutdown }

echo ""
echo "=== ANCHOR: IDCODE (0x11) at /256 — known answer 0x0120681B ==="
set_br 7
set idb [read_reg8 0x11]
set idoff [find_at $idb 0x0120681B]
echo [format "IDCODE raw : %s" [hex8 $idb]]
if {$idoff < 0} {
    echo "!! IDCODE NOT FOUND at any bit alignment."
    echo "!! The read path is not validated, so no value from this session can be"
    echo "!! trusted. Aborting rather than printing a plausible-looking number."
    resume
    shutdown
}
echo [format "ANCHOR OK  : IDCODE found at bit offset %d" $idoff]
if {$idoff != 0} {
    echo [format "NOTE       : replies are phase-shifted %d bits; correcting all reads below." $idoff]
}

echo ""
echo "=== STATUS before CONFIG_ENABLE (0x41 at /256, anchor-corrected) ==="
set sb [read_reg8 0x41]
set before [win32 $sb $idoff]
echo [format "raw        : %s" [hex8 $sb]]
echo [format "STATUS     : 0x%08X   EDIT_MODE(bit7)=%d  DONE(bit13)=%d  ERR(bits0-3)=0x%X" \
      $before [expr {($before>>7)&1}] [expr {($before>>13)&1}] [expr {$before & 0xF}]]

echo ""
echo "=== sending CONFIG_ENABLE (15 00) at /2, as stock would ==="
echo "    (SWD inter-byte gap ~100us vs ~17us native — see CAVEAT in the header)"
set_br 0
cs_low
sx 0x15
sx 0x00
cs_high

echo ""
echo "=== re-anchor + STATUS after CONFIG_ENABLE ==="
set_br 7
set idb2 [read_reg8 0x11]
set idoff2 [find_at $idb2 0x0120681B]
if {$idoff2 < 0} {
    echo [format "IDCODE raw : %s" [hex8 $idb2]]
    echo "!! IDCODE no longer answers after CONFIG_ENABLE — the bus did not survive."
    echo "!! Any STATUS read here would be meaningless. Aborting."
    resume
    shutdown
}
echo [format "re-anchor  : IDCODE still present at bit offset %d" $idoff2]
set sa [read_reg8 0x41]
set after [win32 $sa $idoff2]
echo [format "raw        : %s" [hex8 $sa]]
echo [format "STATUS     : 0x%08X   EDIT_MODE(bit7)=%d  DONE(bit13)=%d  ERR(bits0-3)=0x%X" \
      $after [expr {($after>>7)&1}] [expr {($after>>13)&1}] [expr {$after & 0xF}]]

echo ""
if {$before == $after} {
    echo ">>> STATUS UNCHANGED across CONFIG_ENABLE."
} else {
    echo [format ">>> STATUS CHANGED 0x%08X -> 0x%08X" $before $after]
}
if {[expr {($after >> 7) & 1}]} {
    echo ">>> SYSTEM_EDIT_MODE SET — config entry succeeded."
} else {
    echo ">>> SYSTEM_EDIT_MODE clear — config entry did NOT engage."
}
resume
shutdown
EOF

echo "# SWD-driven FPGA status (anchored) — label=${LABEL}  $(date -Is)" > "$OUT"
sudo timeout 90 openocd -f "$CFG" -c "init" -c "$TCL" 2>&1 | tee -a "$OUT"
echo
echo "wrote $OUT"
