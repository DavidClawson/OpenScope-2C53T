#!/usr/bin/env python3
"""
LCD WebSocket Server for OpenScope-2C53T Emulator

Serves the LCD framebuffer to the React frontend via WebSocket.
Can run in test mode (generates patterns) or connected to the emulator.

Usage:
  uv run python lcd_server.py                    # Test pattern mode
  uv run python lcd_server.py --mode scope       # Simulated scope display
  uv run python lcd_server.py --mode emulator    # Connected to Unicorn emulator
"""

import asyncio
import struct
import math
import time
import json
import argparse
import os
import websockets
from websockets.asyncio.server import serve

LCD_WIDTH = 320
LCD_HEIGHT = 240
FRAME_RATE = 30  # FPS

# ─── RGB565 Color Helpers ─────────────────────────────────────────────

def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565"""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def pack_rgb565(pixel):
    """Pack RGB565 as little-endian bytes"""
    return struct.pack('<H', pixel)

# ─── Scope Colors (matching FNIRSI 2C53T) ─────────────────────────────

COLOR_BG        = rgb888_to_rgb565(0, 0, 0)
COLOR_GRID      = rgb888_to_rgb565(30, 30, 50)
COLOR_GRID_CTR  = rgb888_to_rgb565(50, 50, 80)
COLOR_CH1       = rgb888_to_rgb565(255, 255, 0)     # Yellow
COLOR_CH2       = rgb888_to_rgb565(0, 255, 255)     # Cyan
COLOR_TRIGGER   = rgb888_to_rgb565(0, 255, 0)       # Green
COLOR_TEXT_WHITE = rgb888_to_rgb565(255, 255, 255)
COLOR_TEXT_GRAY  = rgb888_to_rgb565(128, 128, 128)
COLOR_BAR_BG    = rgb888_to_rgb565(16, 16, 32)
COLOR_RED       = rgb888_to_rgb565(255, 50, 50)
COLOR_ORANGE    = rgb888_to_rgb565(239, 147, 17)

# ─── Framebuffer Drawing Primitives ───────────────────────────────────

class FrameBuffer:
    """Simple RGB565 framebuffer with drawing primitives"""

    def __init__(self, width=LCD_WIDTH, height=LCD_HEIGHT):
        self.width = width
        self.height = height
        self.data = bytearray(width * height * 2)

    def clear(self, color=COLOR_BG):
        pixel = struct.pack('<H', color)
        self.data = bytearray(pixel * (self.width * self.height))

    def set_pixel(self, x, y, color):
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = (y * self.width + x) * 2
            struct.pack_into('<H', self.data, offset, color)

    def hline(self, x, y, w, color):
        for i in range(w):
            self.set_pixel(x + i, y, color)

    def vline(self, x, y, h, color):
        for i in range(h):
            self.set_pixel(x, y + i, color)

    def rect(self, x, y, w, h, color):
        for py in range(h):
            for px in range(w):
                self.set_pixel(x + px, y + py, color)

    def draw_char(self, x, y, ch, color, scale=1):
        """Draw a character using a minimal 5x7 bitmap font"""
        glyph = MINI_FONT.get(ch, MINI_FONT.get('?', []))
        for row_idx, row in enumerate(glyph):
            for col in range(5):
                if row & (1 << (4 - col)):
                    for sy in range(scale):
                        for sx in range(scale):
                            self.set_pixel(x + col * scale + sx, y + row_idx * scale + sy, color)

    def draw_text(self, x, y, text, color, scale=1):
        """Draw a string of text"""
        for i, ch in enumerate(text):
            self.draw_char(x + i * 6 * scale, y, ch, color, scale)

    def draw_waveform(self, samples, y_center, color, thickness=1):
        """Draw a waveform from sample values (0.0 to 1.0 mapped to screen)"""
        for x in range(min(len(samples), self.width)):
            y = int(y_center - samples[x] * 80)
            for t in range(thickness):
                self.set_pixel(x, y + t, color)
            # Connect to previous point with line
            if x > 0:
                prev_y = int(y_center - samples[x - 1] * 80)
                if abs(y - prev_y) > 1:
                    step = 1 if y > prev_y else -1
                    for ly in range(prev_y, y, step):
                        for t in range(thickness):
                            self.set_pixel(x, ly + t, color)

    def to_bytes(self):
        return bytes(self.data)


# ─── Minimal 5x7 Bitmap Font ──────────────────────────────────────────

MINI_FONT = {
    ' ': [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
    '0': [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
    '1': [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
    '2': [0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F],
    '3': [0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E],
    '4': [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02],
    '5': [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
    '6': [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E],
    '7': [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
    '8': [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E],
    '9': [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
    'A': [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    'B': [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
    'C': [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
    'D': [0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E],
    'E': [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
    'F': [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
    'G': [0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E],
    'H': [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    'I': [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    'K': [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
    'L': [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
    'M': [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
    'N': [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
    'O': [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    'P': [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
    'R': [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
    'S': [0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E],
    'T': [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
    'U': [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    'V': [0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x04],
    'W': [0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11],
    'X': [0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11],
    'Y': [0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04],
    'Z': [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
    'a': [0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F],
    'c': [0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E],
    'd': [0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F],
    'e': [0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E],
    'h': [0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11],
    'i': [0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E],
    'k': [0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12],
    'l': [0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    'm': [0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11],
    'n': [0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11],
    'o': [0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E],
    'p': [0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10],
    'r': [0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10],
    's': [0x00, 0x00, 0x0E, 0x10, 0x0E, 0x01, 0x1E],
    't': [0x04, 0x04, 0x0E, 0x04, 0x04, 0x05, 0x02],
    'u': [0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D],
    'v': [0x00, 0x00, 0x11, 0x11, 0x0A, 0x0A, 0x04],
    'w': [0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A],
    'y': [0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E],
    ':': [0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00],
    '.': [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04],
    '/': [0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10],
    '-': [0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00],
    '=': [0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00],
    '+': [0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00],
    '?': [0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04],
}


# ─── Scope Display Renderer ──────────────────────────────────────────

class ScopeRenderer:
    """Generates a realistic oscilloscope display"""

    def __init__(self):
        self.fb = FrameBuffer()
        self.t = 0
        self.timebase = "50uS"
        self.ch1_vdiv = "2V"
        self.ch2_vdiv = "200mV"
        self.trigger_mode = "Auto"
        self.running = True

    def draw_grid(self):
        """Draw the oscilloscope grid"""
        # Horizontal grid lines
        for y in range(20, LCD_HEIGHT - 14, 24):
            color = COLOR_GRID_CTR if y == LCD_HEIGHT // 2 else COLOR_GRID
            for x in range(0, LCD_WIDTH, 2 if y == LCD_HEIGHT // 2 else 4):
                self.fb.set_pixel(x, y, color)

        # Vertical grid lines
        for x in range(0, LCD_WIDTH, 32):
            color = COLOR_GRID_CTR if x == LCD_WIDTH // 2 else COLOR_GRID
            for y in range(20, LCD_HEIGHT - 14, 2 if x == LCD_WIDTH // 2 else 4):
                self.fb.set_pixel(x, y, color)

    def draw_status_bar(self):
        """Draw top status bar"""
        self.fb.rect(0, 0, LCD_WIDTH, 14, COLOR_BAR_BG)
        self.fb.draw_text(4, 3, "FNIRSI", COLOR_TEXT_GRAY)
        self.fb.draw_text(60, 3, "H=" + self.timebase, COLOR_TEXT_WHITE)
        self.fb.draw_text(140, 3, "RUN" if self.running else "STOP",
                         COLOR_TRIGGER if self.running else COLOR_RED)
        self.fb.draw_text(275, 3, "2C53T", COLOR_TEXT_WHITE)

    def draw_info_bar(self):
        """Draw bottom info bar"""
        self.fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, COLOR_BAR_BG)
        self.fb.draw_text(4, LCD_HEIGHT - 11, self.ch1_vdiv + " DC", COLOR_CH1)
        self.fb.draw_text(100, LCD_HEIGHT - 11, self.trigger_mode, COLOR_TRIGGER)
        self.fb.draw_text(170, LCD_HEIGHT - 11, "CH1", COLOR_CH1)
        self.fb.draw_text(250, LCD_HEIGHT - 11, self.ch2_vdiv, COLOR_CH2)

    def draw_trigger_marker(self):
        """Draw trigger level indicator on right edge"""
        ty = LCD_HEIGHT // 2
        self.fb.set_pixel(LCD_WIDTH - 5, ty - 3, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 4, ty - 2, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 3, ty - 1, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 2, ty, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 3, ty + 1, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 4, ty + 2, COLOR_TRIGGER)
        self.fb.set_pixel(LCD_WIDTH - 5, ty + 3, COLOR_TRIGGER)

    def generate_frame(self):
        """Generate one frame of scope display"""
        self.fb.clear(COLOR_BG)
        self.draw_grid()
        self.draw_status_bar()
        self.draw_info_bar()
        self.draw_trigger_marker()

        # Generate CH1 waveform (sine wave with slight noise)
        ch1_samples = []
        for x in range(LCD_WIDTH):
            phase = (x / LCD_WIDTH) * math.pi * 8 + self.t * 0.1
            val = math.sin(phase) * 0.5
            val += math.sin(phase * 3) * 0.05  # harmonic
            ch1_samples.append(val)
        self.fb.draw_waveform(ch1_samples, LCD_HEIGHT // 2 - 20, COLOR_CH1)

        # Generate CH2 waveform (square wave)
        ch2_samples = []
        for x in range(LCD_WIDTH):
            phase = ((x / LCD_WIDTH) * 8 + self.t * 0.1) % 2
            val = 0.3 if phase < 1 else -0.3
            ch2_samples.append(val)
        self.fb.draw_waveform(ch2_samples, LCD_HEIGHT // 2 + 40, COLOR_CH2)

        # Measurement display
        freq = 10000 + math.sin(self.t * 0.02) * 50
        self.fb.draw_text(4, 16, f"Freq:{freq:.0f}Hz", COLOR_CH1)

        self.t += 1
        return self.fb.to_bytes()


# ─── Test Pattern Generator ──────────────────────────────────────────

class TestPatternGenerator:
    """Generates test patterns for display pipeline verification"""

    def __init__(self):
        self.fb = FrameBuffer()
        self.frame = 0

    def generate_frame(self):
        self.fb.clear()

        # Color bars
        colors = [
            rgb888_to_rgb565(255, 0, 0),
            rgb888_to_rgb565(0, 255, 0),
            rgb888_to_rgb565(0, 0, 255),
            rgb888_to_rgb565(255, 255, 0),
            rgb888_to_rgb565(0, 255, 255),
            rgb888_to_rgb565(255, 0, 255),
            rgb888_to_rgb565(255, 255, 255),
        ]

        bar_width = LCD_WIDTH // len(colors)
        for i, color in enumerate(colors):
            self.fb.rect(i * bar_width, 0, bar_width, LCD_HEIGHT // 2, color)

        # Moving gradient bar
        offset = self.frame % LCD_WIDTH
        for x in range(LCD_WIDTH):
            brightness = int(((x + offset) % LCD_WIDTH) / LCD_WIDTH * 255)
            color = rgb888_to_rgb565(brightness, brightness, brightness)
            for y in range(LCD_HEIGHT // 2, LCD_HEIGHT // 2 + 20):
                self.fb.set_pixel(x, y, color)

        # Text
        self.fb.draw_text(10, LCD_HEIGHT // 2 + 30, "OpenScope 2C53T", COLOR_TEXT_WHITE, 2)
        self.fb.draw_text(10, LCD_HEIGHT // 2 + 55, f"Frame: {self.frame}", COLOR_CH1)
        self.fb.draw_text(10, LCD_HEIGHT // 2 + 65, "RGB565 Test Pattern", COLOR_CH2)

        # Bouncing pixel
        bx = int(abs(math.sin(self.frame * 0.05) * (LCD_WIDTH - 20)) + 10)
        by = int(abs(math.cos(self.frame * 0.03) * 40) + LCD_HEIGHT // 2 + 80)
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                self.fb.set_pixel(bx + dx, by + dy, COLOR_RED)

        self.frame += 1
        return self.fb.to_bytes()


# ─── WebSocket Server ─────────────────────────────────────────────────

connected_clients = set()

async def handle_client(websocket):
    """Handle a connected browser client"""
    connected_clients.add(websocket)
    print(f"Client connected ({len(connected_clients)} total)")

    try:
        # Send welcome message
        await websocket.send(json.dumps({
            "type": "log",
            "message": f"Connected to LCD server ({LCD_WIDTH}x{LCD_HEIGHT} RGB565)"
        }))

        # Handle incoming messages (button presses)
        async for message in websocket:
            try:
                msg = json.loads(message)
                if msg.get("type") == "button":
                    button = msg.get("button", "?")
                    print(f"  Button: {button}")
                    # Broadcast to all clients
                    log_msg = json.dumps({"type": "log", "message": f"Button press: {button}"})
                    await asyncio.gather(
                        *[c.send(log_msg) for c in connected_clients if c != websocket],
                        return_exceptions=True
                    )
            except json.JSONDecodeError:
                pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"Client disconnected ({len(connected_clients)} total)")


async def broadcast_frames(renderer):
    """Continuously generate and broadcast framebuffer data"""
    while True:
        if connected_clients:
            frame_data = renderer.generate_frame()
            # Send binary framebuffer to all connected clients
            await asyncio.gather(
                *[client.send(frame_data) for client in connected_clients],
                return_exceptions=True
            )
        await asyncio.sleep(1.0 / FRAME_RATE)


async def main_server(mode="scope", port=8765):
    """Start the WebSocket server"""
    if mode == "test":
        renderer = TestPatternGenerator()
        print(f"LCD Server: Test pattern mode")
    else:
        renderer = ScopeRenderer()
        print(f"LCD Server: Scope simulation mode")

    print(f"Listening on ws://localhost:{port}")
    print(f"Open http://localhost:5173 and click 'Connect'")

    async with serve(handle_client, "localhost", port):
        await broadcast_frames(renderer)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="OpenScope-2C53T LCD Server")
    parser.add_argument("--mode", choices=["test", "scope"], default="scope",
                        help="Display mode: test (color bars) or scope (simulated oscilloscope)")
    parser.add_argument("--port", type=int, default=8765, help="WebSocket port")
    args = parser.parse_args()

    try:
        asyncio.run(main_server(mode=args.mode, port=args.port))
    except KeyboardInterrupt:
        print("\nShutdown")
