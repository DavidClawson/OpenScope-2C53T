"""Turn a `screen dumpbin` capture into an image.

The firmware keeps a 4-bit indexed shadow of the LCD, rounded to the
16-colour palette in lcd.c, so colours are close but not pixel-exact
(same caveat as scripts/screenshot.py)."""
from __future__ import annotations

# RGB565 values of lcd.c's lcd_shadow_palette[], in index order (lcd.h).
PALETTE_565 = [0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F,
               0x2104, 0x8410, 0xFCA0, 0x055F, 0x2945, 0x18C3, 0x3186, 0x6BB0]


def rgb(c: int):
    return ((c >> 11) & 0x1F) * 255 // 31, ((c >> 5) & 0x3F) * 255 // 63, (c & 0x1F) * 255 // 31


def to_rgb_bytes(w: int, h: int, data: bytes) -> bytes:
    pal = [rgb(c) for c in PALETTE_565]
    row_len = (w + 1) // 2
    out = bytearray()
    for y in range(h):
        row = data[y * row_len:(y + 1) * row_len]
        for x in range(w):
            b = row[x >> 1]
            out += bytes(pal[(b & 0x0F) if (x & 1) else (b >> 4)])
    return bytes(out)


def save_png(path: str, w: int, h: int, data: bytes, scale: int = 2) -> None:
    from PIL import Image  # optional dependency (pip install openscope[image])
    img = Image.frombytes("RGB", (w, h), to_rgb_bytes(w, h, data))
    if scale > 1:
        img = img.resize((w * scale, h * scale), Image.NEAREST)
    img.save(path)
