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


def png_bytes(w: int, h: int, data: bytes, scale: int = 1) -> bytes:
    """Encode the capture as an RGB PNG using only zlib (no Pillow)."""
    import struct
    import zlib
    rgb_rows = to_rgb_bytes(w, h, data)
    raw = bytearray()
    for y in range(h):
        row = rgb_rows[y * w * 3:(y + 1) * w * 3]
        if scale > 1:
            row = b"".join(row[x * 3:x * 3 + 3] * scale for x in range(w))
        for _ in range(scale):
            raw += b"\x00" + row
    W, H = w * scale, h * scale

    def chunk(tag: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


def save_png(path: str, w: int, h: int, data: bytes, scale: int = 2) -> None:
    with open(path, "wb") as f:
        f.write(png_bytes(w, h, data, scale))
