#!/usr/bin/env bash
# Capture one rig leg on the HiLetgo (fx2lafw) while driving it over UART.
# Wiring: Blue Pill PB3->D0(SCK) PB5->D1(MOSI) PB6->D2(CS), optional PB4->D3(MISO), GND.
# Usage: ./capture.sh <leg> <out.sr> [port] [ms]
set -euo pipefail
LEG=${1:?leg}; OUT=${2:?out.sr}; PORT=${3:-/dev/ttyUSB0}; MS=${4:-1800}
sigrok-cli -d fx2lafw --config samplerate=8m --channels D0,D1,D2,D3 \
           --time "$MS" -o "$OUT" >/dev/null 2>&1 &
CAP=$!
sleep 0.5
timeout 15 python3 "$(dirname "$0")/host.py" --port "$PORT" "$LEG" || true
wait $CAP
echo "captured $LEG -> $OUT"
# Decode with the real SPI PD (against a live target):
#   sigrok-cli -i "$OUT" -P spi:clk=D0:mosi=D1:miso=D3:cs=D2:cpol=1:cpha=1 -A spi
