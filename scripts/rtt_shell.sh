#!/usr/bin/env bash
# RTT debug console over the already-soldered SWD wires.
#
# The bench unit's USB CDC has never enumerated (see CLAUDE.local.md), so the
# debug shell in src/drivers/usb_debug.c is reachable only this way. Same
# commands, same output — only the transport changed.
#
#   ./scripts/rtt_shell.sh                 # interactive console on :9090
#   ./scripts/rtt_shell.sh -e path.elf     # explicit ELF (default: firmware/build/firmware.elf)
#   ./scripts/rtt_shell.sh -c "fpga idcode"   # run one command, print reply, exit
#
# The control-block address is read out of the ELF with nm rather than found by
# scanning SRAM: at 100 kHz SWD a 224 KB scan takes minutes. That means the ELF
# MUST match the image on the device — if the shell shows nothing, that is the
# first thing to check.
#
# WIRING / SAFETY (from docs/SWD_GOLDEN_REFERENCE_2026_06_09.md):
#  - SWDIO / SWCLK / GND only. Do NOT wire 3V3.
#  - Unplug the ST-Link before any IAP flash; it browns out the USB-powered
#    bootloader and causes bad writes. Replug once the image is running.
#  - Flash reads return 0xFF and OpenOCD may report a bogus HardFault. That is
#    a read-protection artifact and does not affect RTT, which lives in SRAM.
set -euo pipefail

ELF="firmware/build/firmware.elf"
PORT=9090
ONESHOT=""
CFG="${HOME}/at32_attach.cfg"
# 100 kHz (the value in at32_attach.cfg) makes an interactive console painful.
# RTT is plain SRAM polling, so it tolerates a much faster clock.
SPEED="${RTT_ADAPTER_SPEED:-1000}"

usage() { sed -n '2,25p' "$0"; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
  case "$1" in
    -e|--elf)   ELF="$2"; shift 2 ;;
    -p|--port)  PORT="$2"; shift 2 ;;
    -c|--cmd)   ONESHOT="$2"; shift 2 ;;
    -s|--speed) SPEED="$2"; shift 2 ;;
    -h|--help)  usage 0 ;;
    *) echo "unknown arg: $1" >&2; usage 1 ;;
  esac
done

[ -f "$CFG" ] || { echo "missing $CFG" >&2; exit 1; }
[ -f "$ELF" ] || { echo "missing $ELF — build it with 'cd firmware && make guest'" >&2; exit 1; }

# _rtt_cb is deliberately non-static so it lands in the symbol table.
CB=$(arm-none-eabi-nm "$ELF" | awk '$3 == "_rtt_cb" { print $1 }')
[ -n "$CB" ] || { echo "no _rtt_cb symbol in $ELF — is this an RTT-enabled build?" >&2; exit 1; }
CB="0x${CB}"

# The region handed to `rtt setup` is a search window, and it must be at least
# as large as the control block or OpenOCD will not find the id. On ARM32 the
# block is 72 bytes (16 id + 2 counts + 2 channels x 24); 256 is comfortable
# headroom without costing a real scan.
OCD_ARGS=(
  -f "$CFG"
  -c "adapter speed ${SPEED}"
  -c "init"
  -c "rtt setup ${CB} 256 \"SEGGER RTT\""
  -c "rtt start"
  -c "rtt server start ${PORT} 0"
)

if [ -n "$ONESHOT" ]; then
  # Headless: bring OpenOCD up, fire one command at the RTT channel, capture
  # whatever comes back within a short window, then tear everything down.
  echo "# rtt: cb=${CB} speed=${SPEED}kHz cmd='${ONESHOT}'" >&2
  sudo openocd "${OCD_ARGS[@]}" >/tmp/rtt_openocd.log 2>&1 &
  OCD_PID=$!
  trap 'sudo kill $OCD_PID 2>/dev/null || true' EXIT

  for _ in $(seq 1 40); do
    if (exec 3<>/dev/tcp/127.0.0.1/${PORT}) 2>/dev/null; then break; fi
    sleep 0.25
  done

  exec 3<>/dev/tcp/127.0.0.1/${PORT} || {
    echo "could not reach RTT server on :${PORT} — see /tmp/rtt_openocd.log" >&2
    exit 1
  }
  printf '%s\r\n' "$ONESHOT" >&3
  timeout 3 cat <&3 || true
  exec 3<&-
  exit 0
fi

cat >&2 <<EOF
# RTT console
#   control block : ${CB}   (from $ELF)
#   adapter speed : ${SPEED} kHz
#   connect with  : telnet localhost ${PORT}     (or: nc localhost ${PORT})
#   quit          : Ctrl-C here
#
# If the console is silent, the usual causes in order:
#   1. $ELF does not match the image actually flashed
#   2. the target is halted (this attaches without halting; check with 'targets')
#   3. ST-Link not plugged in
EOF

exec sudo openocd "${OCD_ARGS[@]}"
