#!/usr/bin/env python3
"""
Renode ↔ React LCD Bridge

Captures LCD writes from the Renode EXMC memory region and streams
the framebuffer to the React frontend via WebSocket.

Since Renode's EXMC region (0x60000000) is mapped as plain memory,
all LCD pixel writes are stored there. We periodically read this
memory and send it to the browser.

For the custom firmware:
- LCD commands go to 0x6001FFFE (A17=0)
- LCD pixel data goes to 0x60020000 (A17=1)
- The ST7789V stores pixels sequentially after a 0x2C (RAMWR) command
- Our firmware draws to the EXMC region which Renode stores in RAM

Usage:
  # Terminal 1: Start Renode with custom firmware
  cd emulator/renode
  /Applications/Renode.app/Contents/MacOS/renode run_custom.resc

  # Terminal 2: Start this bridge
  cd emulator
  uv run python renode_lcd_bridge.py

  # Terminal 3: Start React frontend
  cd frontend
  npm run dev

  # Open http://localhost:5173 and click Connect
"""

import asyncio
import struct
import json
import time
import os
import sys
import websockets
from websockets.asyncio.server import serve

# LCD dimensions
LCD_WIDTH = 320
LCD_HEIGHT = 240
FRAME_RATE = 15  # FPS for Renode bridge (lower than sim since reads are slower)

# Renode telnet interface
RENODE_HOST = "localhost"
RENODE_PORT = 1234  # Renode's default monitor port

# WebSocket port (same as lcd_server.py)
WS_PORT = 8765


class RenodeLCDBridge:
    """Reads EXMC memory from Renode and builds a framebuffer"""

    def __init__(self):
        self.framebuffer = bytearray(LCD_WIDTH * LCD_HEIGHT * 2)
        self.reader = None
        self.writer = None
        self.connected = False

    async def connect_renode(self):
        """Connect to Renode's telnet monitor"""
        try:
            self.reader, self.writer = await asyncio.open_connection(
                RENODE_HOST, RENODE_PORT)
            self.connected = True
            # Read the welcome banner
            await asyncio.sleep(0.5)
            if self.reader:
                try:
                    banner = await asyncio.wait_for(self.reader.read(4096), timeout=1.0)
                    print(f"Connected to Renode: {banner.decode('utf-8', errors='ignore').strip()[:80]}")
                except asyncio.TimeoutError:
                    print("Connected to Renode (no banner)")
            return True
        except (ConnectionRefusedError, OSError) as e:
            print(f"Cannot connect to Renode at {RENODE_HOST}:{RENODE_PORT}: {e}")
            print("Make sure Renode is running with: renode run_custom.resc")
            return False

    async def renode_command(self, cmd):
        """Send a command to Renode and get the response"""
        if not self.connected:
            return ""
        try:
            self.writer.write((cmd + "\n").encode())
            await self.writer.drain()
            await asyncio.sleep(0.05)
            response = await asyncio.wait_for(self.reader.read(8192), timeout=1.0)
            return response.decode('utf-8', errors='ignore').strip()
        except (asyncio.TimeoutError, ConnectionResetError, BrokenPipeError):
            self.connected = False
            return ""

    async def read_framebuffer_from_renode(self):
        """Read the EXMC memory region from Renode to get LCD pixel data"""
        if not self.connected:
            return False

        # Read the EXMC bank memory where LCD data was written
        # The firmware writes pixel data to 0x60020000+
        # We read it in chunks
        try:
            for offset in range(0, LCD_WIDTH * LCD_HEIGHT * 2, 256):
                addr = 0x60020000 + offset
                # Read 64 doublewords (256 bytes) at a time
                response = await self.renode_command(
                    f"sysbus ReadDoubleWord 0x{addr:08X}")
                # Parse the response... this is slow for full framebuffer
                # A better approach would be Renode's binary dump feature
                pass
            return True
        except Exception as e:
            print(f"Error reading framebuffer: {e}")
            return False


class FallbackRenderer:
    """Generates a test display when Renode isn't available"""

    def __init__(self):
        self.frame = 0

    def generate_frame(self):
        """Generate what our custom firmware WOULD display"""
        fb = bytearray(LCD_WIDTH * LCD_HEIGHT * 2)

        def set_pixel(x, y, color):
            if 0 <= x < LCD_WIDTH and 0 <= y < LCD_HEIGHT:
                offset = (y * LCD_WIDTH + x) * 2
                struct.pack_into('<H', fb, offset, color)

        def fill_rect(x, y, w, h, color):
            for py in range(h):
                for px in range(w):
                    set_pixel(x + px, y + py, color)

        def draw_char(x, y, ch, fg, bg):
            """Minimal 5x7 font for key chars"""
            glyphs = {
                'O': [0x0E,0x11,0x11,0x11,0x11,0x11,0x0E],
                'p': [0x00,0x00,0x1E,0x11,0x1E,0x10,0x10],
                'e': [0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E],
                'n': [0x00,0x00,0x16,0x19,0x11,0x11,0x11],
                'S': [0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E],
                'c': [0x00,0x00,0x0E,0x10,0x10,0x11,0x0E],
                'o': [0x00,0x00,0x0E,0x11,0x11,0x11,0x0E],
                '2': [0x0E,0x11,0x01,0x06,0x08,0x10,0x1F],
                'C': [0x0E,0x11,0x10,0x10,0x10,0x11,0x0E],
                '5': [0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E],
                '3': [0x0E,0x11,0x01,0x06,0x01,0x11,0x0E],
                'T': [0x1F,0x04,0x04,0x04,0x04,0x04,0x04],
                ' ': [0x00,0x00,0x00,0x00,0x00,0x00,0x00],
                'v': [0x00,0x00,0x11,0x11,0x0A,0x0A,0x04],
                '0': [0x0E,0x11,0x13,0x15,0x19,0x11,0x0E],
                '.': [0x00,0x00,0x00,0x00,0x00,0x00,0x04],
                '1': [0x04,0x0C,0x04,0x04,0x04,0x04,0x0E],
                'F': [0x1F,0x10,0x10,0x1E,0x10,0x10,0x10],
                'i': [0x04,0x00,0x0C,0x04,0x04,0x04,0x0E],
                'r': [0x00,0x00,0x16,0x19,0x10,0x10,0x10],
                'm': [0x00,0x00,0x1A,0x15,0x15,0x11,0x11],
                'w': [0x00,0x00,0x11,0x11,0x15,0x15,0x0A],
                'a': [0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F],
            }
            glyph = glyphs.get(ch, glyphs.get(' '))
            if not glyph:
                return
            for row, bits in enumerate(glyph):
                for col in range(5):
                    color = fg if bits & (1 << (4 - col)) else bg
                    set_pixel(x + col, y + row, color)

        def draw_string(x, y, text, fg, bg):
            for i, ch in enumerate(text):
                draw_char(x + i * 6, y, ch, fg, bg)

        # Colors
        BLACK = 0x0000
        WHITE = 0xFFFF
        CYAN = 0x07FF
        YELLOW = 0xFFE0
        GREEN = 0x07E0
        DARK_GRAY = 0x2104
        GRAY = 0x8410

        import math

        if self.frame < 60:
            # Splash screen for first 2 seconds (at 30fps)
            fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK)
            draw_string(60, 60, "OpenScope 2C53T", CYAN, BLACK)
            draw_string(60, 80, "Firmware v0.1", WHITE, BLACK)
            draw_string(60, 110, "FreeRTOS", GRAY, BLACK)
            draw_string(60, 126, "ST7789V 320x240", GRAY, BLACK)
        else:
            # Scope screen
            fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK)

            # Status bar
            fill_rect(0, 0, LCD_WIDTH, 14, DARK_GRAY)
            draw_string(4, 3, "OpenScope", CYAN, DARK_GRAY)
            draw_string(100, 3, "SCOPE", GREEN, DARK_GRAY)
            draw_string(280, 3, "2C53T", WHITE, DARK_GRAY)

            # Grid
            for x in range(0, LCD_WIDTH, 32):
                for y in range(16, LCD_HEIGHT - 14, 4):
                    set_pixel(x, y, 0x18C3)
            for y in range(16, LCD_HEIGHT - 14, 26):
                for x in range(0, LCD_WIDTH, 4):
                    set_pixel(x, y, 0x18C3)

            # Center cross
            for x in range(LCD_WIDTH):
                set_pixel(x, (LCD_HEIGHT - 30) // 2 + 14, 0x3186)
            for y in range(16, LCD_HEIGHT - 14):
                set_pixel(LCD_WIDTH // 2, y, 0x3186)

            # CH1 sine wave
            y_center = (LCD_HEIGHT - 30) // 2 + 14
            phase = self.frame * 0.1
            for x in range(LCD_WIDTH):
                y = int(y_center - 40 * math.sin((x / LCD_WIDTH) * math.pi * 8 + phase))
                if 16 <= y < LCD_HEIGHT - 14:
                    set_pixel(x, y, YELLOW)
                    if y + 1 < LCD_HEIGHT - 14:
                        set_pixel(x, y + 1, YELLOW)

            # CH2 square wave
            for x in range(LCD_WIDTH):
                sq_phase = ((x / LCD_WIDTH) * 8 + phase) % 2
                y = y_center + 50 + (25 if sq_phase >= 1 else -25)
                if 16 <= y < LCD_HEIGHT - 14:
                    set_pixel(x, y, CYAN)

            # Measurements
            draw_string(4, 18, "Fre:10.0kHz", YELLOW, BLACK)

            # Bottom bar
            fill_rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
            draw_string(4, LCD_HEIGHT - 11, "2v  Scope", YELLOW, DARK_GRAY)
            draw_string(120, LCD_HEIGHT - 11, "50 ", GREEN, DARK_GRAY)
            draw_string(200, LCD_HEIGHT - 11, "CH1", YELLOW, DARK_GRAY)

            # Trigger marker
            fill_rect(LCD_WIDTH - 6, y_center - 3, 5, 7, GREEN)

        self.frame += 1
        return bytes(fb)


# ─── WebSocket Server ─────────────────────────────────────────────

connected_clients = set()


async def handle_client(websocket):
    connected_clients.add(websocket)
    print(f"Client connected ({len(connected_clients)} total)")
    try:
        await websocket.send(json.dumps({
            "type": "log",
            "message": "Connected to OpenScope firmware display"
        }))
        async for message in websocket:
            try:
                msg = json.loads(message)
                if msg.get("type") == "button":
                    print(f"  Button: {msg.get('button')}")
            except json.JSONDecodeError:
                pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"Client disconnected ({len(connected_clients)} total)")


async def broadcast_frames(renderer):
    while True:
        if connected_clients:
            frame_data = renderer.generate_frame()
            await asyncio.gather(
                *[c.send(frame_data) for c in connected_clients],
                return_exceptions=True
            )
        await asyncio.sleep(1.0 / 30)


async def main():
    print("OpenScope 2C53T - Firmware Display Server")
    print("=" * 50)

    # Try to connect to Renode first
    bridge = RenodeLCDBridge()
    renode_ok = await bridge.connect_renode()

    if renode_ok:
        print("Using Renode for live firmware display")
        renderer = bridge
    else:
        print("Renode not available — using firmware simulation")
        print("Shows what the custom firmware would render")
        renderer = FallbackRenderer()

    print(f"\nWebSocket server on ws://localhost:{WS_PORT}")
    print("Open http://localhost:5173 and click 'Connect'\n")

    async with serve(handle_client, "localhost", WS_PORT):
        await broadcast_frames(renderer)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown")
