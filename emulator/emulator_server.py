#!/usr/bin/env python3
"""
OpenScope 2C53T — Emulator Display Server

Serves the browser UI and streams the firmware's actual LCD framebuffer
from /tmp/openscope_fb.bin (written by the Renode ST7789V peripheral).

Usage:
  python3 emulator_server.py
  Then open http://localhost:8080

The framebuffer file is created by the ST7789V Renode peripheral
when the firmware writes to the EXMC LCD addresses.
"""

import asyncio
import json
import os
import time
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import threading

try:
    import websockets
    from websockets.asyncio.server import serve
except ImportError:
    print("Install websockets: pip install websockets")
    raise

FB_PATH = "/tmp/openscope_fb.bin"
FB_SIZE = 320 * 240 * 2  # 153600 bytes RGB565
WS_PORT = 8765
HTTP_PORT = 8080
UI_DIR = Path(__file__).parent / "ui"

# Signal injection config (shared with Renode via file)
SIGNAL_CONFIG_PATH = "/tmp/openscope_signal.json"
DEFAULT_SIGNAL = {
    "ch1_waveform": "sine",
    "ch1_frequency": 1000.0,
    "ch1_amplitude": 1.0,
    "ch1_noise": 5,
    "ch2_waveform": "square",
    "ch2_frequency": 5000.0,
    "ch2_amplitude": 0.5,
    "ch2_noise": 0,
}

# ─── State ────────────────────────────────────────────

connected_clients = set()
last_fb = bytearray(FB_SIZE)
last_mtime = 0.0
fps_counter = [0, 0.0]  # [frames, last_time]
signal_config = dict(DEFAULT_SIGNAL)


def read_framebuffer():
    """Read framebuffer from file if changed"""
    global last_fb, last_mtime
    try:
        st = os.stat(FB_PATH)
        if st.st_mtime != last_mtime and st.st_size == FB_SIZE:
            with open(FB_PATH, 'rb') as f:
                last_fb = f.read()
            last_mtime = st.st_mtime
            return True
    except (FileNotFoundError, OSError):
        pass
    return False


def write_signal_config():
    """Write signal config to file for Renode peripheral to read"""
    try:
        with open(SIGNAL_CONFIG_PATH, 'w') as f:
            json.dump(signal_config, f)
    except OSError:
        pass


# ─── WebSocket Server ────────────────────────────────

async def handle_client(websocket):
    connected_clients.add(websocket)
    print(f"[WS] Client connected ({len(connected_clients)} total)")
    try:
        await websocket.send(json.dumps({
            "type": "config",
            "signal": signal_config
        }))
        async for message in websocket:
            try:
                msg = json.loads(message)
                if msg.get("type") == "button":
                    btn = msg.get("button", "")
                    print(f"  [BTN] {btn}")
                    # TODO: Forward to Renode GPIO via monitor telnet
                elif msg.get("type") == "signal":
                    signal_config.update(msg.get("config", {}))
                    write_signal_config()
                    print(f"  [SIG] Updated: {signal_config}")
            except json.JSONDecodeError:
                pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"[WS] Client disconnected ({len(connected_clients)} total)")


async def broadcast_frames():
    """Poll framebuffer file and broadcast to clients"""
    while True:
        changed = read_framebuffer()
        if connected_clients and (changed or True):
            # Always send current framebuffer at target fps
            dead = set()
            results = await asyncio.gather(
                *[c.send(bytes(last_fb)) for c in connected_clients],
                return_exceptions=True
            )
            for client, result in zip(list(connected_clients), results):
                if isinstance(result, Exception):
                    dead.add(client)
            connected_clients.difference_update(dead)
        await asyncio.sleep(1.0 / 20)  # 20fps


# ─── HTTP Server (serves ui/index.html) ──────────────

class UIHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(UI_DIR), **kwargs)

    def log_message(self, format, *args):
        pass  # Suppress access logs


def run_http_server():
    server = HTTPServer(("localhost", HTTP_PORT), UIHandler)
    server.serve_forever()


# ─── Main ─────────────────────────────────────────────

async def main():
    print("OpenScope 2C53T — Emulator Display Server")
    print("=" * 50)
    print(f"  HTTP:      http://localhost:{HTTP_PORT}")
    print(f"  WebSocket: ws://localhost:{WS_PORT}")
    print(f"  Framebuffer: {FB_PATH}")
    print()
    print("Waiting for Renode to write framebuffer...")
    print("Start Renode: cd firmware && make renode")
    print()

    # Write initial signal config
    write_signal_config()

    # Start HTTP server in a thread
    http_thread = threading.Thread(target=run_http_server, daemon=True)
    http_thread.start()

    # Start WebSocket server
    async with serve(handle_client, "localhost", WS_PORT):
        await broadcast_frames()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown")
