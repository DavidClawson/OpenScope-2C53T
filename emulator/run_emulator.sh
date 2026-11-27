#!/usr/bin/env bash
# OpenScope 2C53T: run the firmware in Renode with the LCD + key panel viewer.
#
#   emulator/run_emulator.sh                 build the emulator image, start Renode, open the viewer
#   emulator/run_emulator.sh --no-build      reuse firmware/build (must be a `make emu` build)
#   emulator/run_emulator.sh 3               LCD at 3x (1-4, default 2)
#   emulator/run_emulator.sh menu menu menu  press keys once Renode answers (here: to the scope)
#
# Closing the viewer (Q, Esc or the window's close button) stops Renode.
# Needs: Renode (RENODE=/path/to/renode, default /Applications/Renode.app),
# SDL3 (brew install sdl3), arm-none-eabi-gcc. See emulator/README.md.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
RENODE="${RENODE:-/Applications/Renode.app/Contents/MacOS/renode}"
PORT="${RENODE_MONITOR_PORT:-1234}"
tmp="${TMPDIR:-/tmp}"
log="${tmp%/}/openscope_renode.log"

build=1
viewer_args=()
for a in "$@"; do
    case "$a" in
        --no-build) build=0 ;;
        -h|--help)  sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)          viewer_args+=("$a") ;;
    esac
done

if [ ! -x "$RENODE" ]; then
    echo "Renode not found at $RENODE"
    echo "Install it (emulator/renode/README.md) or set RENODE=/path/to/renode."
    exit 1
fi

elf="$root/firmware/build/firmware.elf"
if [ "$build" = 1 ] || [ ! -f "$elf" ]; then
    echo "== Building the emulator image (make emu, serial: its clean must finish first)"
    make -C "$root/firmware" -j1 emu
fi
# Only the emulator build has the key mailbox; a hardware image would boot
# into a fault loop here, and the viewer could not press anything.
# (grep without -q: with pipefail, -q's early exit kills nm with SIGPIPE.)
if ! arm-none-eabi-nm "$elf" | grep ' g_emu_key$' >/dev/null; then
    echo "$elf is not an emulator build (no g_emu_key). Run without --no-build."
    exit 1
fi

echo "== Building the viewer"
make -s -C "$here" lcd_viewer

# Port taken: an earlier Renode is stopped, anything else is left alone.
for pid in $(lsof -ti :"$PORT" 2>/dev/null || true); do
    if ps -p "$pid" -o command= | grep -i renode >/dev/null; then
        echo "== Stopping an earlier Renode (pid $pid) on port $PORT"
        kill "$pid" 2>/dev/null || true
    else
        echo "Port $PORT is used by another program (pid $pid): $(ps -p "$pid" -o comm=)"
        exit 1
    fi
done
for _ in $(seq 1 20); do
    lsof -ti :"$PORT" >/dev/null 2>&1 || break
    sleep 0.25
done

# A frame left over from an earlier run would show until the first new one.
rm -f /tmp/openscope_fb.bin

echo "== Starting Renode (log: $log)"
( cd "$here/renode" && exec "$RENODE" --disable-gui --port "$PORT" run_interactive.resc ) >"$log" 2>&1 &
renode_pid=$!
cleanup() { kill "$renode_pid" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "== Opening the viewer (close it to stop Renode)"
RENODE_MONITOR_PORT="$PORT" "$here/lcd_viewer" ${viewer_args[@]+"${viewer_args[@]}"}
