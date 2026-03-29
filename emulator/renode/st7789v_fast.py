# -*- coding: utf-8 -*-
# ST7789V LCD Peripheral — FAST STUB for soak testing
# IronPython 2.7 compatible — Renode PyDev
#
# Accepts all EXMC writes but does NO framebuffer tracking.
# This eliminates the per-pixel Python overhead that makes
# full emulation ~20-30x slower than necessary.
#
# For freeze detection, the soak test uses a heartbeat file
# written by a separate Renode hook (not LCD-dependent).
#
# Pixel writes are counted for basic liveness stats.
# A minimal framebuffer snapshot is written every SNAPSHOT_INTERVAL
# pixels (set high) so the SDL viewer can still show something
# if desired.

SNAPSHOT_INTERVAL = 500000   # Write FB snapshot rarely (~6-7 full screens)
FB_PATH = "/tmp/openscope_fb.bin"
LCD_W = 320
LCD_H = 240
FB_SIZE = LCD_W * LCD_H * 2

# ST7789V commands
CMD_CASET = 0x2A
CMD_RASET = 0x2B
CMD_RAMWR = 0x2C

ST_IDLE = 0
ST_CASET = 1
ST_RASET = 2
ST_PIXELS = 3
ST_PARAM = 4

if request.IsInit:
    state = [ST_IDLE]
    param_count = [0]
    param_bytes = [0, 0, 0, 0]
    col_start = [0]
    col_end = [LCD_W - 1]
    row_start = [0]
    row_end = [LCD_H - 1]
    pixel_count = [0]
    total_pixels = [0]

    # Sparse framebuffer — only maintained for occasional snapshots
    fb = bytearray(FB_SIZE)
    cursor_x = [0]
    cursor_y = [0]

elif request.IsWrite:
    off = request.Offset
    val = request.Value & 0xFFFF

    if off < 0x20000:
        # Command byte
        cmd = val & 0xFF
        if cmd == CMD_CASET:
            state[0] = ST_CASET
            param_count[0] = 0
        elif cmd == CMD_RASET:
            state[0] = ST_RASET
            param_count[0] = 0
        elif cmd == CMD_RAMWR:
            state[0] = ST_PIXELS
            cursor_x[0] = col_start[0]
            cursor_y[0] = row_start[0]
        else:
            state[0] = ST_PARAM
            param_count[0] = 0
    else:
        # Data byte
        s = state[0]

        if s == ST_CASET:
            param_bytes[param_count[0]] = val & 0xFF
            param_count[0] += 1
            if param_count[0] >= 4:
                col_start[0] = (param_bytes[0] << 8) | param_bytes[1]
                col_end[0] = (param_bytes[2] << 8) | param_bytes[3]
                if col_end[0] >= LCD_W:
                    col_end[0] = LCD_W - 1
                state[0] = ST_IDLE

        elif s == ST_RASET:
            param_bytes[param_count[0]] = val & 0xFF
            param_count[0] += 1
            if param_count[0] >= 4:
                row_start[0] = (param_bytes[0] << 8) | param_bytes[1]
                row_end[0] = (param_bytes[2] << 8) | param_bytes[3]
                if row_end[0] >= LCD_H:
                    row_end[0] = LCD_H - 1
                state[0] = ST_IDLE

        elif s == ST_PIXELS:
            # Track pixel in framebuffer (cheap — just array writes)
            x = cursor_x[0]
            y = cursor_y[0]
            if 0 <= x < LCD_W and 0 <= y < LCD_H:
                idx = (y * LCD_W + x) * 2
                fb[idx] = val & 0xFF
                fb[idx + 1] = (val >> 8) & 0xFF

            cursor_x[0] += 1
            if cursor_x[0] > col_end[0]:
                cursor_x[0] = col_start[0]
                cursor_y[0] += 1

            pixel_count[0] += 1
            total_pixels[0] += 1

            # Rare snapshot for optional visual monitoring
            if pixel_count[0] >= SNAPSHOT_INTERVAL:
                pixel_count[0] = 0
                try:
                    f = open(FB_PATH, "wb")
                    f.write(bytes(fb))
                    f.close()
                except:
                    pass

elif request.IsRead:
    request.Value = 0
