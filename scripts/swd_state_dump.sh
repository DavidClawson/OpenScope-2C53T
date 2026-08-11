#!/usr/bin/env bash
# Experiment E — SWD peripheral state dump at the config-enable instant.
#
# Target must be running a *spin* image, parked with the prelude (05/12) sent,
# CS LOW, and CONFIG_ENABLE (0x15) not yet clocked:
#   stock : /tmp/APP_2C53T_expE_spin_stock.bin   (b . patched at flash 0x0802DA42)
#   ours  : /tmp/OPENSCOPE_expE_spin_guest.bin   (make guest-spin)
#
# Dumps every register that could plausibly differ, so the two runs can be
# diffed mechanically:  ./swd_state_dump.sh stock   then   ./swd_state_dump.sh ours
#   diff -u /tmp/swd_stock.txt /tmp/swd_ours.txt
#
# NOTES (from docs/SWD_GOLDEN_REFERENCE_2026_06_09.md):
#  - Unplug the ST-Link before any IAP flash (it browns out the USB-powered
#    bootloader and causes bad writes). Replug only once the image is running.
#  - Flash reads return 0xFF and OpenOCD may report a bogus "HardFault" — that
#    is a read-protection artifact. Peripheral (0x4000xxxx) reads are live and
#    are all this script uses.
#  - Wire SWDIO / SWCLK / GND only. Do NOT wire 3V3.
set -u

LABEL="${1:-dump}"
OUT="/tmp/swd_${LABEL}.txt"
CFG="${HOME}/at32_attach.cfg"

[ -f "$CFG" ] || { echo "missing $CFG"; exit 1; }

# base+offset groups: GPIO CRL/CRH/IDR/ODR = 4 words from base
read -r -d '' CMDS <<'EOF'
echo "=== GPIOA (CRL CRH IDR ODR) ==="
mdw 0x40010800 4
echo "=== GPIOB (CRL CRH IDR ODR) ==="
mdw 0x40010C00 4
echo "=== GPIOC (CRL CRH IDR ODR) ==="
mdw 0x40011000 4
echo "=== GPIOD (CRL CRH IDR ODR) ==="
mdw 0x40011400 4
echo "=== GPIOE (CRL CRH IDR ODR) ==="
mdw 0x40011800 4
echo "=== AFIO (EVCR MAPR EXTICR1-4 .. MAPR2) ==="
mdw 0x40010000 8
echo "=== SPI3 (CTRL1 CTRL2 STS) ==="
mdw 0x40003C00 3
echo "=== CRM (0x40021000 .. +0x2C) ==="
mdw 0x40021000 12
echo "=== USART2 (STS CTRL1@+0x0C) ==="
mdw 0x40004400 5
EOF

ARGS=(-f "$CFG" -c "init" -c "halt")
while IFS= read -r line; do
  [ -z "$line" ] && continue
  ARGS+=(-c "$line")
done <<< "$CMDS"
ARGS+=(-c "resume" -c "shutdown")

echo "# Experiment E SWD dump — label=${LABEL}  $(date -Is)" > "$OUT"
sudo timeout 60 openocd "${ARGS[@]}" 2>&1 | tee -a "$OUT"

echo
echo "wrote $OUT"
echo "when both are captured:  diff -u /tmp/swd_stock.txt /tmp/swd_ours.txt"
