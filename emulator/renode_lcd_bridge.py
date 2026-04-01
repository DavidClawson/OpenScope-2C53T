#!/usr/bin/env python3
"""
OpenScope 2C53T — Firmware Display Simulator

Simulates what the custom firmware renders on the ST7789V LCD.
Streams RGB565 framebuffer to the React frontend via WebSocket.
Handles button presses from the frontend to change modes and settings.

Usage:
  cd emulator && uv run python renode_lcd_bridge.py
  Then open http://localhost:5173 and click Connect
"""

import asyncio
import struct
import json
import math
import websockets
from websockets.asyncio.server import serve

LCD_WIDTH = 320
LCD_HEIGHT = 240
WS_PORT = 8765

# ─── Colors (RGB565, matching firmware lcd.h) ─────────────────────

BLACK      = 0x0000
WHITE      = 0xFFFF
RED        = 0xF800
GREEN      = 0x07E0
BLUE       = 0x001F
YELLOW     = 0xFFE0
CYAN       = 0x07FF
MAGENTA    = 0xF81F
DARK_GRAY  = 0x2104
GRAY       = 0x8410
LIGHT_GRAY = 0xC618
GRID_COLOR = 0x18C3
GRID_CTR   = 0x3186
ORANGE     = 0xFCA0
SELECTED   = 0x2945
DARK_BLUE  = 0x0010


# ─── Framebuffer Drawing ─────────────────────────────────────────

class FB:
    """RGB565 framebuffer with drawing primitives"""

    # Minimal 5x7 bitmap font
    FONT = {
        ' ':[0,0,0,0,0,0,0],'!':[4,4,4,4,4,0,4],'.':[0,0,0,0,0,0,4],
        ',':[0,0,0,0,0,4,8],':':[0,4,0,0,0,4,0],'-':[0,0,0,0x1F,0,0,0],
        '+':[0,4,4,0x1F,4,4,0],'=':[0,0,0x1F,0,0x1F,0,0],'/':[1,2,2,4,8,8,16],
        '(':[2,4,8,8,8,4,2],')':[8,4,2,2,2,4,8],'%':[0x19,0x1A,2,4,0xB,0x13,0],
        '0':[0xE,0x11,0x13,0x15,0x19,0x11,0xE],'1':[4,0xC,4,4,4,4,0xE],
        '2':[0xE,0x11,1,6,8,0x10,0x1F],'3':[0xE,0x11,1,6,1,0x11,0xE],
        '4':[2,6,0xA,0x12,0x1F,2,2],'5':[0x1F,0x10,0x1E,1,1,0x11,0xE],
        '6':[6,8,0x10,0x1E,0x11,0x11,0xE],'7':[0x1F,1,2,4,8,8,8],
        '8':[0xE,0x11,0x11,0xE,0x11,0x11,0xE],'9':[0xE,0x11,0x11,0xF,1,2,0xC],
        'A':[0xE,0x11,0x11,0x1F,0x11,0x11,0x11],'B':[0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E],
        'C':[0xE,0x11,0x10,0x10,0x10,0x11,0xE],'D':[0x1E,0x11,0x11,0x11,0x11,0x11,0x1E],
        'E':[0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F],'F':[0x1F,0x10,0x10,0x1E,0x10,0x10,0x10],
        'G':[0xE,0x11,0x10,0x17,0x11,0x11,0xE],'H':[0x11,0x11,0x11,0x1F,0x11,0x11,0x11],
        'I':[0xE,4,4,4,4,4,0xE],'J':[0xE,2,2,2,2,0x12,0xC],
        'K':[0x11,0x12,0x14,0x18,0x14,0x12,0x11],'L':[0x10,0x10,0x10,0x10,0x10,0x10,0x1F],
        'M':[0x11,0x1B,0x15,0x15,0x11,0x11,0x11],'N':[0x11,0x19,0x15,0x13,0x11,0x11,0x11],
        'O':[0xE,0x11,0x11,0x11,0x11,0x11,0xE],'P':[0x1E,0x11,0x11,0x1E,0x10,0x10,0x10],
        'Q':[0xE,0x11,0x11,0x11,0x15,0x12,0xD],'R':[0x1E,0x11,0x11,0x1E,0x14,0x12,0x11],
        'S':[0xE,0x11,0x10,0xE,1,0x11,0xE],'T':[0x1F,4,4,4,4,4,4],
        'U':[0x11,0x11,0x11,0x11,0x11,0x11,0xE],'V':[0x11,0x11,0x11,0xA,0xA,4,4],
        'W':[0x11,0x11,0x11,0x15,0x15,0x1B,0x11],'X':[0x11,0x11,0xA,4,0xA,0x11,0x11],
        'Y':[0x11,0x11,0xA,4,4,4,4],'Z':[0x1F,1,2,4,8,0x10,0x1F],
        'a':[0,0,0xE,1,0xF,0x11,0xF],'b':[0x10,0x10,0x1E,0x11,0x11,0x11,0x1E],
        'c':[0,0,0xE,0x10,0x10,0x11,0xE],'d':[1,1,0xF,0x11,0x11,0x11,0xF],
        'e':[0,0,0xE,0x11,0x1F,0x10,0xE],'f':[6,8,0x1C,8,8,8,8],
        'g':[0,0,0xF,0x11,0xF,1,0xE],'h':[0x10,0x10,0x16,0x19,0x11,0x11,0x11],
        'i':[4,0,0xC,4,4,4,0xE],'k':[0x10,0x10,0x12,0x14,0x18,0x14,0x12],
        'l':[0xC,4,4,4,4,4,0xE],'m':[0,0,0x1A,0x15,0x15,0x11,0x11],
        'n':[0,0,0x16,0x19,0x11,0x11,0x11],'o':[0,0,0xE,0x11,0x11,0x11,0xE],
        'p':[0,0,0x1E,0x11,0x1E,0x10,0x10],'r':[0,0,0x16,0x19,0x10,0x10,0x10],
        's':[0,0,0xE,0x10,0xE,1,0x1E],'t':[4,4,0xE,4,4,5,2],
        'u':[0,0,0x11,0x11,0x11,0x13,0xD],'v':[0,0,0x11,0x11,0xA,0xA,4],
        'w':[0,0,0x11,0x11,0x15,0x15,0xA],'x':[0,0,0x11,0xA,4,0xA,0x11],
        'y':[0,0,0x11,0x11,0xF,1,0xE],'z':[0,0,0x1F,2,4,8,0x1F],
    }

    def __init__(self):
        self.data = bytearray(LCD_WIDTH * LCD_HEIGHT * 2)

    def clear(self, color=BLACK):
        p = struct.pack('<H', color)
        self.data = bytearray(p * LCD_WIDTH * LCD_HEIGHT)

    def pixel(self, x, y, c):
        if 0 <= x < LCD_WIDTH and 0 <= y < LCD_HEIGHT:
            struct.pack_into('<H', self.data, (y * LCD_WIDTH + x) * 2, c)

    def rect(self, x, y, w, h, c):
        for py in range(max(0, y), min(LCD_HEIGHT, y + h)):
            for px in range(max(0, x), min(LCD_WIDTH, x + w)):
                struct.pack_into('<H', self.data, (py * LCD_WIDTH + px) * 2, c)

    def hline(self, x, y, w, c):
        for px in range(max(0, x), min(LCD_WIDTH, x + w)):
            if 0 <= y < LCD_HEIGHT:
                struct.pack_into('<H', self.data, (y * LCD_WIDTH + px) * 2, c)

    def char(self, x, y, ch, fg, bg=None):
        glyph = self.FONT.get(ch, self.FONT.get('?', [0]*7))
        for row, bits in enumerate(glyph):
            for col in range(5):
                if bits & (1 << (4 - col)):
                    self.pixel(x + col, y + row, fg)
                elif bg is not None:
                    self.pixel(x + col, y + row, bg)

    def text(self, x, y, s, fg, bg=None):
        for i, ch in enumerate(s):
            self.char(x + i * 6, y, ch, fg, bg)

    def bytes(self):
        return bytes(self.data)


# ─── Device State ────────────────────────────────────────────────

MODE_SCOPE = 0
MODE_METER = 1
MODE_SIGGEN = 2
MODE_SETTINGS = 3
MODE_COUNT = 4

MODE_NAMES = ["SCOPE", "METER", "SIGGEN", "SETUP"]

VIEW_TIME = 0
VIEW_FFT = 1
VIEW_SPLIT = 2
VIEW_WATERFALL = 3
VIEW_COUNT = 4
VIEW_NAMES = ["TIME", "FFT", "SPLIT", "WFALL"]

WINDOW_NAMES = ["Rect", "Hann", "Hamm", "BHar", "Flat"]

class DeviceState:
    def __init__(self):
        self.mode = MODE_SCOPE
        self.frame = 0
        self.uptime = 0
        self.ch1_vdiv = "2V"
        self.ch2_vdiv = "200mV"
        self.timebase = "50uS"
        self.trigger_mode = "Auto"
        self.running = True
        self.settings_cursor = 0
        self.meter_value = 13.82
        self.meter_unit = "V DC"
        self.siggen_freq = "1.000"
        self.siggen_wave = 0  # 0=sine, 1=square, 2=triangle, 3=saw
        self.siggen_wave_names = ["Sine", "Square", "Triangle", "Sawtooth"]
        self.splash_done = False
        # FFT state
        self.scope_view = VIEW_TIME
        self.fft_window = 1  # Hanning
        self.waterfall_history = []  # list of 320-element intensity rows

    def handle_button(self, btn):
        """Process a button press, return True if display needs redraw"""
        if btn == "MENU":
            self.mode = (self.mode + 1) % MODE_COUNT
            return True
        elif btn == "OK":
            if self.mode == MODE_SCOPE:
                self.running = not self.running
            return True
        elif btn == "AUTO":
            if self.mode == MODE_SCOPE:
                self.running = True
                self.trigger_mode = "Auto"
            return True
        elif btn == "UP":
            if self.mode == MODE_SETTINGS:
                self.settings_cursor = max(0, self.settings_cursor - 1)
            elif self.mode == MODE_SIGGEN:
                self.siggen_wave = (self.siggen_wave - 1) % 4
            return True
        elif btn == "DOWN":
            if self.mode == MODE_SETTINGS:
                self.settings_cursor = min(6, self.settings_cursor + 1)
            elif self.mode == MODE_SIGGEN:
                self.siggen_wave = (self.siggen_wave + 1) % 4
            return True
        elif btn == "CH1":
            self.mode = MODE_SCOPE
            return True
        elif btn == "CH2":
            self.mode = MODE_SCOPE
            return True
        elif btn == "PRM":
            if self.mode == MODE_SCOPE:
                self.scope_view = (self.scope_view + 1) % VIEW_COUNT
            return True
        elif btn == "SELECT":
            if self.mode == MODE_SCOPE and self.scope_view in (VIEW_FFT, VIEW_SPLIT, VIEW_WATERFALL):
                self.fft_window = (self.fft_window + 1) % 5
            elif self.mode == MODE_SIGGEN:
                self.siggen_wave = (self.siggen_wave + 1) % 4
            return True
        elif btn == "SAVE":
            return True  # flash "Saved!" briefly
        elif btn == "TRIGGER":
            if self.mode == MODE_SCOPE:
                modes = ["Auto", "Normal", "Single"]
                idx = modes.index(self.trigger_mode) if self.trigger_mode in modes else 0
                self.trigger_mode = modes[(idx + 1) % 3]
            return True
        return False


# ─── Screen Renderers ────────────────────────────────────────────

def draw_status_bar(fb, state):
    fb.rect(0, 0, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, 3, "OpenScope", CYAN, DARK_GRAY)
    fb.text(100, 3, MODE_NAMES[state.mode], GREEN, DARK_GRAY)
    fb.text(280, 3, "2C53T", WHITE, DARK_GRAY)
    # Uptime
    m, s = divmod(state.uptime, 60)
    fb.text(200, 3, f"{m:02d}:{s:02d}", GRAY, DARK_GRAY)


def draw_info_bar_scope(fb, state):
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, f"{state.ch1_vdiv} DC", YELLOW, DARK_GRAY)
    color = GREEN if state.trigger_mode == "Auto" else ORANGE
    fb.text(100, LCD_HEIGHT - 11, state.trigger_mode, color, DARK_GRAY)
    fb.text(160, LCD_HEIGHT - 11, f"H={state.timebase}", WHITE, DARK_GRAY)
    fb.text(250, LCD_HEIGHT - 11, state.ch2_vdiv, CYAN, DARK_GRAY)


def draw_scope(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    # Grid dots
    for x in range(0, LCD_WIDTH, 32):
        for y in range(18, LCD_HEIGHT - 16, 4):
            fb.pixel(x, y, GRID_COLOR)
    for y in range(18, LCD_HEIGHT - 16, 26):
        for x in range(0, LCD_WIDTH, 4):
            fb.pixel(x, y, GRID_COLOR)

    # Center crosshair
    y_center = (LCD_HEIGHT - 30) // 2 + 16
    fb.hline(0, y_center, LCD_WIDTH, GRID_CTR)
    for y in range(18, LCD_HEIGHT - 16):
        fb.pixel(LCD_WIDTH // 2, y, GRID_CTR)

    if state.running or state.frame % 30 < 20:
        phase = state.frame * 0.08 if state.running else 0

        # CH1 sine
        for x in range(LCD_WIDTH):
            y = int(y_center - 40 * math.sin((x / LCD_WIDTH) * math.pi * 8 + phase)
                    - 5 * math.sin((x / LCD_WIDTH) * math.pi * 24 + phase * 3))
            if 18 <= y < LCD_HEIGHT - 16:
                fb.pixel(x, y, YELLOW)
                if y + 1 < LCD_HEIGHT - 16:
                    fb.pixel(x, y + 1, YELLOW)

        # CH2 square
        for x in range(LCD_WIDTH):
            p = ((x / LCD_WIDTH) * 8 + phase) % 2
            y = y_center + 55 + (25 if p >= 1 else -25)
            if 18 <= y < LCD_HEIGHT - 16:
                fb.pixel(x, y, CYAN)
            # Vertical edges
            if x > 0:
                p2 = (((x-1) / LCD_WIDTH) * 8 + phase) % 2
                if (p >= 1) != (p2 >= 1):
                    for yy in range(y_center + 30, y_center + 80):
                        if 18 <= yy < LCD_HEIGHT - 16:
                            fb.pixel(x, yy, CYAN)

    # Trigger marker
    fb.rect(LCD_WIDTH - 6, y_center - 3, 5, 7, GREEN)

    # Measurements
    fb.text(4, 18, "Freq:10.00kHz", YELLOW, BLACK)
    fb.text(4, 27, "Vpp:3.31V", YELLOW, BLACK)
    fb.text(LCD_WIDTH - 90, 18, "Fre:9.96kHz", CYAN, BLACK)
    fb.text(LCD_WIDTH - 78, 27, "Vpp:660mV", CYAN, BLACK)

    if not state.running:
        fb.text(140, y_center - 4, "STOP", RED, BLACK)

    draw_info_bar_scope(fb, state)


_fft_cache = {}  # keyed by window type

def compute_fft_spectrum(state, num_bins=320):
    """Compute FFT spectrum of a 1kHz square wave (cached per window type)"""
    if state.fft_window in _fft_cache:
        return _fft_cache[state.fft_window]

    N = 1024
    sample_rate = 44100.0
    freq = 1000.0
    samples = [1.0 if (i / sample_rate * freq) % 1.0 < 0.5 else -1.0 for i in range(N)]

    # Apply window
    if state.fft_window == 1:  # Hanning
        samples = [s * 0.5 * (1 - math.cos(2*math.pi*i/N)) for i, s in enumerate(samples)]
    elif state.fft_window == 2:  # Hamming
        samples = [s * (0.54 - 0.46 * math.cos(2*math.pi*i/N)) for i, s in enumerate(samples)]

    # Compute DFT magnitudes (one-time cost)
    magnitudes = []
    for k in range(num_bins):
        bin_idx = int(k * (N//2) / num_bins) + 1
        re = sum(samples[n] * math.cos(-2*math.pi*bin_idx*n/N) for n in range(0, N, 8)) * 8
        im = sum(samples[n] * math.sin(-2*math.pi*bin_idx*n/N) for n in range(0, N, 8)) * 8
        mag = math.sqrt(re*re + im*im) * 2.0 / N
        if mag < 1e-10: mag = 1e-10
        magnitudes.append(20 * math.log10(mag))

    _fft_cache[state.fft_window] = magnitudes
    return magnitudes


def draw_fft_view(fb, state, y_top, height):
    """Draw FFT spectrum in a region"""
    mags = compute_fft_spectrum(state)
    ref_db = 0.0
    range_db = 80.0

    for x in range(LCD_WIDTH):
        db = mags[x] if x < len(mags) else -200
        normalized = (ref_db - db) / range_db
        normalized = max(0.0, min(1.0, normalized))
        bar_top = int(y_top + normalized * height)

        if bar_top < y_top + height:
            if normalized < 0.25: color = YELLOW
            elif normalized < 0.5: color = GREEN
            elif normalized < 0.75: color = CYAN
            else: color = GRID_COLOR
            for y in range(bar_top, y_top + height):
                fb.pixel(x, y, color)

    # Grid lines
    for db_val in range(0, -80, -10):
        norm = (ref_db - db_val) / range_db
        y = int(y_top + norm * height)
        if y_top < y < y_top + height:
            for x in range(0, LCD_WIDTH, 4):
                fb.pixel(x, y, GRID_COLOR)

    # Labels
    fb.text(4, y_top + 2, "1.0kHz Fund", WHITE, BLACK)
    fb.text(LCD_WIDTH - 30, y_top + 2, WINDOW_NAMES[state.fft_window], GRAY, BLACK)


def draw_fft_screen(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)
    draw_fft_view(fb, state, 18, LCD_HEIGHT - 34)

    # Info bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, "FFT 4096pt", YELLOW, DARK_GRAY)
    fb.text(100, LCD_HEIGHT - 11, WINDOW_NAMES[state.fft_window], GREEN, DARK_GRAY)
    fb.text(160, LCD_HEIGHT - 11, "PRM:View", GRAY, DARK_GRAY)
    fb.text(240, LCD_HEIGHT - 11, "SEL:Win", GRAY, DARK_GRAY)


def draw_split_screen(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    # Top half: scope waveform (simplified)
    y_center = 68
    phase = state.frame * 0.08
    for x in range(LCD_WIDTH):
        y = int(y_center - 25 * math.sin((x / LCD_WIDTH) * math.pi * 8 + phase))
        if 18 <= y < 118:
            fb.pixel(x, y, YELLOW)
    # Center line
    fb.hline(0, y_center, LCD_WIDTH, GRID_CTR)
    # Divider
    fb.hline(0, 120, LCD_WIDTH, GRID_CTR)

    # Bottom half: FFT
    draw_fft_view(fb, state, 122, 100)

    # Info bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, "SPLIT", YELLOW, DARK_GRAY)
    fb.text(60, LCD_HEIGHT - 11, WINDOW_NAMES[state.fft_window], GREEN, DARK_GRAY)


def intensity_to_rgb565(intensity):
    """Heatmap: strong(0)=red, weak(255)=blue"""
    inv = 255 - intensity
    if inv > 204: return RED
    if inv > 153: return 0xFBE0  # orange-yellow
    if inv > 102: return GREEN
    if inv > 51: return CYAN
    return BLUE


def draw_waterfall_screen(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    mags = compute_fft_spectrum(state)

    # Convert to intensity row
    row = []
    for x in range(LCD_WIDTH):
        db = mags[x] if x < len(mags) else -200
        normalized = max(0.0, min(1.0, -db / 80.0))
        row.append(int(normalized * 255))

    state.waterfall_history.insert(0, row)
    if len(state.waterfall_history) > 64:
        del state.waterfall_history[64:]

    # Render rows
    row_height = 3
    for r, hist_row in enumerate(state.waterfall_history):
        y = 18 + r * row_height
        if y + row_height > LCD_HEIGHT - 16:
            break
        for x in range(min(LCD_WIDTH, len(hist_row))):
            color = intensity_to_rgb565(hist_row[x])
            for dy in range(row_height):
                fb.pixel(x, y + dy, color)

    fb.text(4, 18, "WFALL", WHITE, BLACK)

    # Info bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, "WATERFALL", YELLOW, DARK_GRAY)
    fb.text(120, LCD_HEIGHT - 11, WINDOW_NAMES[state.fft_window], GREEN, DARK_GRAY)


def draw_meter(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    # Mode label
    fb.text(20, 22, "DC Voltage", YELLOW, BLACK)
    fb.text(220, 22, "Auto Range", GREEN, BLACK)

    # Big reading - draw at 2x by doubling pixels
    reading = f"{state.meter_value:.2f}"
    bx, by = 40, 55
    for i, ch in enumerate(reading):
        for row, bits in enumerate(FB.FONT.get(ch, [0]*7)):
            for col in range(5):
                if bits & (1 << (4 - col)):
                    for dy in range(3):
                        for dx in range(3):
                            fb.pixel(bx + i * 18 + col * 3 + dx, by + row * 3 + dy, WHITE)

    fb.text(bx + len(reading) * 18 + 10, 70, state.meter_unit, YELLOW, BLACK)

    # Bar graph
    fb.rect(20, 130, 280, 14, DARK_GRAY)
    bar_w = int(280 * (state.meter_value / 20.0))
    bar_color = GREEN if state.meter_value < 15 else ORANGE if state.meter_value < 18 else RED
    fb.rect(20, 130, min(bar_w, 280), 14, bar_color)

    # Scale labels
    fb.text(20, 148, "0", GRAY, BLACK)
    fb.text(155, 148, "10", GRAY, BLACK)
    fb.text(290, 148, "20V", GRAY, BLACK)

    # Statistics
    fb.text(20, 168, f"MIN: {state.meter_value - 0.03:.2f}V", GRAY, BLACK)
    fb.text(20, 180, f"MAX: {state.meter_value + 0.04:.2f}V", GRAY, BLACK)
    fb.text(20, 192, f"AVG: {state.meter_value:.2f}V", GRAY, BLACK)
    fb.text(20, 204, f"Range: 20V", YELLOW, BLACK)

    # Slowly drift the value
    state.meter_value += math.sin(state.frame * 0.05) * 0.01

    # Bottom bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, "DC Voltage", YELLOW, DARK_GRAY)
    fb.text(200, LCD_HEIGHT - 11, "Auto Range", GREEN, DARK_GRAY)


def draw_siggen(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    # Waveform preview box
    fb.rect(19, 29, 282, 82, DARK_GRAY)
    fb.rect(20, 30, 280, 80, BLACK)

    y_center = 70
    phase = state.frame * 0.1
    wave = state.siggen_wave

    for x in range(280):
        t = (x / 280) * math.pi * 6 + phase
        if wave == 0:  # Sine
            val = math.sin(t)
        elif wave == 1:  # Square
            val = 1.0 if (t % (2 * math.pi)) < math.pi else -1.0
        elif wave == 2:  # Triangle
            val = 2.0 * abs(2.0 * ((t / (2*math.pi)) % 1.0) - 1.0) - 1.0
        else:  # Sawtooth
            val = 2.0 * ((t / (2*math.pi)) % 1.0) - 1.0

        y = int(y_center - val * 30)
        if 30 <= y < 110:
            fb.pixel(20 + x, y, YELLOW)
            if y + 1 < 110:
                fb.pixel(20 + x, y + 1, YELLOW)

    # Settings list
    items = [
        ("Waveform:", state.siggen_wave_names[state.siggen_wave]),
        ("Frequency:", f"{state.siggen_freq} kHz"),
        ("Amplitude:", "3.3 Vpp"),
        ("Offset:", "0.0 V"),
        ("Output:", "ON"),
    ]

    for i, (label, value) in enumerate(items):
        y = 120 + i * 18
        fb.text(30, y, label, GRAY, BLACK)
        color = GREEN if label == "Output:" and value == "ON" else WHITE
        fb.text(140, y, value, color, BLACK)

    # Bottom bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, f"Signal Generator", YELLOW, DARK_GRAY)
    fb.text(220, LCD_HEIGHT - 11, "3.3Vpp", GREEN, DARK_GRAY)


def draw_settings(fb, state):
    fb.rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 30, BLACK)

    fb.text(120, 22, "SETTINGS", WHITE, BLACK)

    items = [
        "Language: English",
        "Sound and Light",
        "Auto Shutdown: 30min",
        "Display Mode",
        "Startup on Boot: Scope",
        "About",
        "Factory Reset",
    ]

    for i, item in enumerate(items):
        y = 42 + i * 22
        selected = (i == state.settings_cursor)
        bg = SELECTED if selected else BLACK
        fg = WHITE if selected else GRAY
        fb.rect(16, y, 288, 18, bg)
        fb.text(24, y + 5, item, fg, bg)
        if selected:
            fb.text(16, y + 5, ">", CYAN, bg)

    # Bottom bar
    fb.rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, DARK_GRAY)
    fb.text(4, LCD_HEIGHT - 11, "UP/DOWN navigate", GRAY, DARK_GRAY)
    fb.text(180, LCD_HEIGHT - 11, "MENU to exit", GRAY, DARK_GRAY)


def draw_splash(fb, state):
    fb.clear(BLACK)

    # Border accent
    fb.rect(0, 0, LCD_WIDTH, 2, CYAN)
    fb.rect(0, LCD_HEIGHT - 2, LCD_WIDTH, 2, CYAN)

    # Title (double-rendered for bold)
    fb.text(64, 50, "OpenScope 2C53T", CYAN, BLACK)
    fb.text(65, 50, "OpenScope 2C53T", CYAN, BLACK)

    fb.text(72, 75, "Custom Firmware", WHITE, BLACK)
    fb.text(112, 90, "v0.1", WHITE, BLACK)

    fb.text(80, 120, "GD32F307 120MHz", GRAY, BLACK)
    fb.text(72, 135, "FreeRTOS  256KB RAM", GRAY, BLACK)
    fb.text(68, 150, "ST7789V  320x240 LCD", GRAY, BLACK)

    fb.text(40, 185, "github.com/DavidClawson", DARK_GRAY, BLACK)
    fb.text(64, 198, "OpenScope-2C53T", DARK_GRAY, BLACK)


# ─── Main Renderer ───────────────────────────────────────────────

class FirmwareRenderer:
    def __init__(self):
        self.fb = FB()
        self.state = DeviceState()
        self.last_second = 0

    def handle_button(self, button):
        self.state.handle_button(button)

    def generate_frame(self):
        self.state.frame += 1

        # Uptime counter
        if self.state.frame % 30 == 0:
            self.state.uptime += 1

        # Splash screen for first 2.5 seconds
        if self.state.frame < 75:
            draw_splash(self.fb, self.state)
            return self.fb.bytes()

        self.state.splash_done = True

        # Clear and draw status bar (shared across all modes)
        self.fb.clear(BLACK)
        draw_status_bar(self.fb, self.state)

        # Draw current mode
        if self.state.mode == MODE_SCOPE:
            if self.state.scope_view == VIEW_FFT:
                draw_fft_screen(self.fb, self.state)
            elif self.state.scope_view == VIEW_SPLIT:
                draw_split_screen(self.fb, self.state)
            elif self.state.scope_view == VIEW_WATERFALL:
                draw_waterfall_screen(self.fb, self.state)
            else:
                draw_scope(self.fb, self.state)
        elif self.state.mode == MODE_METER:
            draw_meter(self.fb, self.state)
        elif self.state.mode == MODE_SIGGEN:
            draw_siggen(self.fb, self.state)
        elif self.state.mode == MODE_SETTINGS:
            draw_settings(self.fb, self.state)

        return self.fb.bytes()


# ─── WebSocket Server ────────────────────────────────────────────

renderer = FirmwareRenderer()
connected_clients = set()


async def handle_client(websocket):
    connected_clients.add(websocket)
    print(f"Client connected ({len(connected_clients)} total)")
    try:
        await websocket.send(json.dumps({
            "type": "log",
            "message": "Connected to OpenScope firmware simulator"
        }))
        async for message in websocket:
            try:
                msg = json.loads(message)
                if msg.get("type") == "button":
                    btn = msg.get("button", "")
                    print(f"  Button: {btn}")
                    renderer.handle_button(btn)
                    # Send confirmation back
                    mode = MODE_NAMES[renderer.state.mode]
                    await websocket.send(json.dumps({
                        "type": "log",
                        "message": f"Button: {btn} → Mode: {mode}"
                    }))
            except json.JSONDecodeError:
                pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"Client disconnected ({len(connected_clients)} total)")


async def broadcast_frames():
    while True:
        if connected_clients:
            frame = renderer.generate_frame()
            # Send to all clients, remove dead ones
            dead = set()
            results = await asyncio.gather(
                *[c.send(frame) for c in connected_clients],
                return_exceptions=True
            )
            for client, result in zip(list(connected_clients), results):
                if isinstance(result, Exception):
                    dead.add(client)
            connected_clients.difference_update(dead)
        else:
            # No clients — still advance state but don't render
            renderer.state.frame += 1
        await asyncio.sleep(1.0 / 20)  # 20fps


async def main():
    print("OpenScope 2C53T — Firmware Display Simulator")
    print("=" * 50)
    print(f"WebSocket: ws://localhost:{WS_PORT}")
    print(f"Frontend:  http://localhost:5173")
    print()
    print("Click buttons on the device photo to interact:")
    print("  MENU    → cycle modes (Scope/Meter/SigGen/Settings)")
    print("  OK      → run/stop (scope mode)")
    print("  TRIGGER → cycle trigger mode")
    print("  UP/DOWN → navigate settings, change waveform")
    print("  CH1/CH2 → switch to scope mode")
    print()

    async with serve(handle_client, "localhost", WS_PORT):
        await broadcast_frames()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown")
