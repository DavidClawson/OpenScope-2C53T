# -*- coding: utf-8 -*-
# ST7789V LCD Peripheral for Renode (IronPython 2.7 compatible)
# Captures EXMC writes, maintains 320x240 RGB565 framebuffer.
# Exports to /tmp/openscope_fb.bin

LCD_W = 320
LCD_H = 240
FB_SIZE = LCD_W * LCD_H * 2
FB_PATH = "/tmp/openscope_fb.bin"

# ST7789V commands
CMD_CASET = 0x2A
CMD_RASET = 0x2B
CMD_RAMWR = 0x2C

# States
ST_IDLE = 0
ST_CASET = 1
ST_RASET = 2
ST_PIXELS = 3
ST_PARAM = 4

if request.IsInit:
    fb = bytearray(FB_SIZE)
    state = [ST_IDLE]
    param_count = [0]
    param_bytes = [0, 0, 0, 0]
    col_start = [0]
    col_end = [LCD_W - 1]
    row_start = [0]
    row_end = [LCD_H - 1]
    cursor_x = [0]
    cursor_y = [0]
    write_count = [0]

    f = open(FB_PATH, "wb")
    f.write(fb)
    f.close()

elif request.IsWrite:
    off = request.Offset
    val = request.Value & 0xFFFF

    if off < 0x20000:
        cmd = val & 0xFF
        if cmd == CMD_CASET:
            state[0] = ST_CASET
            param_count[0] = 0
        elif cmd == CMD_RASET:
            state[0] = ST_RASET
            param_count[0] = 0
        elif cmd == CMD_RAMWR:
            # Flush any previous pixel data before starting new write
            if write_count[0] > 0:
                write_count[0] = 0
                f = open(FB_PATH, "wb")
                f.write(bytes(fb))
                f.close()
            state[0] = ST_PIXELS
            cursor_x[0] = col_start[0]
            cursor_y[0] = row_start[0]
        else:
            state[0] = ST_PARAM
            param_count[0] = 0
    else:
        s = state[0]

        if s == ST_CASET:
            param_bytes[param_count[0]] = val & 0xFF
            param_count[0] = param_count[0] + 1
            if param_count[0] >= 4:
                col_start[0] = (param_bytes[0] << 8) | param_bytes[1]
                col_end[0] = (param_bytes[2] << 8) | param_bytes[3]
                if col_end[0] >= LCD_W:
                    col_end[0] = LCD_W - 1
                state[0] = ST_IDLE

        elif s == ST_RASET:
            param_bytes[param_count[0]] = val & 0xFF
            param_count[0] = param_count[0] + 1
            if param_count[0] >= 4:
                row_start[0] = (param_bytes[0] << 8) | param_bytes[1]
                row_end[0] = (param_bytes[2] << 8) | param_bytes[3]
                if row_end[0] >= LCD_H:
                    row_end[0] = LCD_H - 1
                state[0] = ST_IDLE

        elif s == ST_PIXELS:
            x = cursor_x[0]
            y = cursor_y[0]
            if x >= 0 and x < LCD_W and y >= 0 and y < LCD_H:
                idx = (y * LCD_W + x) * 2
                fb[idx] = val & 0xFF
                fb[idx + 1] = (val >> 8) & 0xFF

            cursor_x[0] = cursor_x[0] + 1
            if cursor_x[0] > col_end[0]:
                cursor_x[0] = col_start[0]
                cursor_y[0] = cursor_y[0] + 1

            write_count[0] = write_count[0] + 1
            if write_count[0] >= 5000:
                write_count[0] = 0
                f = open(FB_PATH, "wb")
                f.write(bytes(fb))
                f.close()

elif request.IsRead:
    request.Value = 0
