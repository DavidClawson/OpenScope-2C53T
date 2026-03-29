#!/usr/bin/env python3
"""
Font bitmap generator for OpenScope 2C53T firmware.

Renders TTF/OTF fonts into C source files with variable-width bitmap data.
Output format is optimized for embedded LCD rendering (1-bit per pixel).

Usage:
    python3 generate_font.py <font_path> <pixel_size> <output_name> [--chars CHARSET]

Example:
    python3 generate_font.py /System/Library/Fonts/Menlo.ttc 48 font_digits --chars digits
    python3 generate_font.py /System/Library/Fonts/SFNS.ttf 16 font_medium --chars ascii

Charsets:
    ascii   - printable ASCII 0x20-0x7E (default)
    digits  - 0-9 . - + : V A m k M H z W F Ω µ (meter/siggen readouts)
    labels  - A-Z a-z 0-9 and common punctuation
"""

import argparse
import sys
import os
from PIL import Image, ImageFont, ImageDraw

CHARSETS = {
    'ascii': [chr(c) for c in range(0x20, 0x7F)],
    'digits': list('0123456789.-+: VAkmMHzWFO'),  # O stands in for Ω on device
    'labels': list('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .-+:/%()'),
}


def render_glyph(font, char, size):
    """Render a single character and return (bitmap_rows, width, height).
    Each row is a list of 0/1 pixel values."""
    # Use a generous canvas
    canvas_w = size * 3
    canvas_h = size * 3
    img = Image.new('L', (canvas_w, canvas_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((size, size), char, font=font, fill=255)

    # Find bounding box of non-zero pixels
    pixels = img.load()
    min_x, min_y, max_x, max_y = canvas_w, canvas_h, 0, 0
    for y in range(canvas_h):
        for x in range(canvas_w):
            if pixels[x, y] > 0:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)

    if min_x > max_x:
        # Empty glyph (space character)
        # Use a width proportional to the font size
        space_width = max(size // 3, 4)
        return [], space_width, 0

    glyph_w = max_x - min_x + 1
    glyph_h = max_y - min_y + 1

    # Extract bitmap with threshold
    rows = []
    for y in range(min_y, max_y + 1):
        row = []
        for x in range(min_x, max_x + 1):
            row.append(1 if pixels[x, y] > 100 else 0)
        rows.append(row)

    return rows, glyph_w, glyph_h


def render_font(font_path, pixel_size, charset, font_index=0):
    """Render all characters in charset. Returns list of glyph dicts."""
    try:
        font = ImageFont.truetype(font_path, pixel_size, index=font_index)
    except Exception as e:
        print(f"Error loading font: {e}")
        sys.exit(1)

    glyphs = []
    max_height = 0

    # First pass: render all glyphs and find common baseline
    raw_glyphs = []
    for char in charset:
        rows, glyph_w, glyph_h = render_glyph(font, char, pixel_size)
        raw_glyphs.append((char, rows, glyph_w, glyph_h))
        if glyph_h > max_height:
            max_height = glyph_h

    # Second pass: re-render with consistent vertical positioning
    # Use a shared canvas approach for consistent baselines
    canvas_h = pixel_size * 2
    canvas_w = pixel_size * 3
    baseline_img = Image.new('L', (canvas_w, canvas_h), 0)
    baseline_draw = ImageDraw.Draw(baseline_img)

    # Find the common top and bottom by rendering a reference set
    ref_chars = '0Ag|'
    global_top = canvas_h
    global_bottom = 0
    for rc in ref_chars:
        if rc not in charset:
            continue
        baseline_img_t = Image.new('L', (canvas_w, canvas_h), 0)
        baseline_draw_t = ImageDraw.Draw(baseline_img_t)
        baseline_draw_t.text((pixel_size, pixel_size // 2), rc, font=font, fill=255)
        px = baseline_img_t.load()
        for y in range(canvas_h):
            for x in range(canvas_w):
                if px[x, y] > 100:
                    global_top = min(global_top, y)
                    global_bottom = max(global_bottom, y)

    if global_bottom <= global_top:
        global_top = pixel_size // 2
        global_bottom = global_top + pixel_size

    font_height = global_bottom - global_top + 1
    # Add a pixel of padding top and bottom
    font_height += 2
    render_y = pixel_size // 2

    glyphs = []
    for char in charset:
        img = Image.new('L', (canvas_w, canvas_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((pixel_size, render_y), char, font=font, fill=255)
        px = img.load()

        # Find horizontal bounds
        min_x, max_x = canvas_w, 0
        for y in range(global_top, global_bottom + 1):
            for x in range(canvas_w):
                if px[x, y] > 100:
                    min_x = min(min_x, x)
                    max_x = max(max_x, x)

        if min_x > max_x:
            # Space or empty
            advance = max(pixel_size // 3, 4)
            bitmap_rows = [[0] * advance for _ in range(font_height)]
            glyphs.append({
                'char': char,
                'width': advance,
                'advance': advance + 1,
                'rows': bitmap_rows,
            })
            continue

        glyph_w = max_x - min_x + 1

        # Extract bitmap for the full font_height
        bitmap_rows = []
        for y in range(global_top - 1, global_top - 1 + font_height):
            row = []
            for x in range(min_x, max_x + 1):
                if 0 <= y < canvas_h:
                    row.append(1 if px[x, y] > 100 else 0)
                else:
                    row.append(0)
            bitmap_rows.append(row)

        # Advance width = glyph width + 1px spacing
        advance = glyph_w + max(1, pixel_size // 12)

        glyphs.append({
            'char': char,
            'width': glyph_w,
            'advance': advance,
            'rows': bitmap_rows,
        })

    return glyphs, font_height


def pack_rows_to_bytes(rows, width):
    """Pack a list of pixel rows into bytes (MSB first, padded to byte boundary)."""
    packed = []
    bytes_per_row = (width + 7) // 8
    for row in rows:
        for b in range(bytes_per_row):
            byte = 0
            for bit in range(8):
                px_idx = b * 8 + bit
                if px_idx < len(row) and row[px_idx]:
                    byte |= (0x80 >> bit)
            packed.append(byte)
    return packed


def generate_c_source(glyphs, font_height, name, font_path, pixel_size):
    """Generate a C source file with font bitmap data."""
    lines = []
    lines.append(f'/* Auto-generated font: {name}')
    lines.append(f' * Source: {os.path.basename(font_path)}, {pixel_size}px')
    lines.append(f' * Height: {font_height}px, {len(glyphs)} glyphs')
    lines.append(f' * Generator: scripts/generate_font.py')
    lines.append(f' */')
    lines.append('')
    lines.append('#include "font.h"')
    lines.append('')

    # Character table comment
    char_list = ''.join(g['char'] for g in glyphs)
    lines.append(f'/* Characters: {repr(char_list)} */')
    lines.append('')

    # Glyph bitmap data
    lines.append(f'static const uint8_t {name}_data[] = {{')
    offsets = []
    current_offset = 0
    for g in glyphs:
        packed = pack_rows_to_bytes(g['rows'], g['width'])
        offsets.append(current_offset)
        char_repr = repr(g['char']) if g['char'] != '\\' else "'\\\\''"
        lines.append(f'    /* {char_repr} (w={g["width"]}, adv={g["advance"]}) offset={current_offset} */')
        # Write bytes in rows for readability
        bytes_per_row = (g['width'] + 7) // 8
        for row_idx in range(font_height):
            start = row_idx * bytes_per_row
            end = start + bytes_per_row
            row_bytes = packed[start:end]
            hex_str = ', '.join(f'0x{b:02X}' for b in row_bytes)
            lines.append(f'    {hex_str},')
        current_offset += len(packed)
    lines.append('};')
    lines.append('')

    # Offsets table
    lines.append(f'static const uint16_t {name}_offsets[{len(glyphs)}] = {{')
    for i, g in enumerate(glyphs):
        lines.append(f'    {offsets[i]},  /* {repr(g["char"])} */')
    lines.append('};')
    lines.append('')

    # Widths table
    lines.append(f'static const uint8_t {name}_widths[{len(glyphs)}] = {{')
    row = '    '
    for i, g in enumerate(glyphs):
        row += f'{g["width"]}, '
        if (i + 1) % 16 == 0:
            lines.append(row)
            row = '    '
    if row.strip():
        lines.append(row)
    lines.append('};')
    lines.append('')

    # Advance widths table
    lines.append(f'static const uint8_t {name}_advances[{len(glyphs)}] = {{')
    row = '    '
    for i, g in enumerate(glyphs):
        row += f'{g["advance"]}, '
        if (i + 1) % 16 == 0:
            lines.append(row)
            row = '    '
    if row.strip():
        lines.append(row)
    lines.append('};')
    lines.append('')

    # Character map (ASCII value -> glyph index, or 0xFF if missing)
    first_char = min(ord(g['char']) for g in glyphs)
    last_char = max(ord(g['char']) for g in glyphs)
    char_to_idx = {ord(g['char']): i for i, g in enumerate(glyphs)}

    lines.append(f'static const uint8_t {name}_charmap[{last_char - first_char + 1}] = {{')
    row = '    '
    for c in range(first_char, last_char + 1):
        idx = char_to_idx.get(c, 0xFF)
        row += f'{idx}, '
        if (c - first_char + 1) % 16 == 0:
            lines.append(row)
            row = '    '
    if row.strip():
        lines.append(row)
    lines.append('};')
    lines.append('')

    # Font descriptor struct
    lines.append(f'const font_t {name} = {{')
    lines.append(f'    .height     = {font_height},')
    lines.append(f'    .first_char = {first_char},  /* {repr(chr(first_char))} */')
    lines.append(f'    .last_char  = {last_char},  /* {repr(chr(last_char))} */')
    lines.append(f'    .num_glyphs = {len(glyphs)},')
    lines.append(f'    .data       = {name}_data,')
    lines.append(f'    .offsets    = {name}_offsets,')
    lines.append(f'    .widths     = {name}_widths,')
    lines.append(f'    .advances   = {name}_advances,')
    lines.append(f'    .charmap    = {name}_charmap,')
    lines.append('};')
    lines.append('')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Generate bitmap font C source for embedded LCD')
    parser.add_argument('font_path', help='Path to TTF/OTF/TTC font file')
    parser.add_argument('pixel_size', type=int, help='Font size in pixels')
    parser.add_argument('output_name', help='C variable name (e.g., font_large)')
    parser.add_argument('--chars', default='ascii', choices=CHARSETS.keys(),
                        help='Character set to include')
    parser.add_argument('--index', type=int, default=0,
                        help='Font index within TTC collection')
    parser.add_argument('--output-dir', default=None,
                        help='Output directory (default: firmware/src/fonts/)')
    args = parser.parse_args()

    charset = CHARSETS[args.chars]
    print(f'Rendering {len(charset)} chars from {os.path.basename(args.font_path)} at {args.pixel_size}px...')

    glyphs, font_height = render_font(args.font_path, args.pixel_size, charset, args.index)
    print(f'Font height: {font_height}px')

    c_source = generate_c_source(glyphs, font_height, args.output_name,
                                  args.font_path, args.pixel_size)

    out_dir = args.output_dir or os.path.join(os.path.dirname(__file__), '..', 'firmware', 'src', 'fonts')
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f'{args.output_name}.c')

    with open(out_path, 'w') as f:
        f.write(c_source)

    data_size = sum(len(pack_rows_to_bytes(g['rows'], g['width'])) for g in glyphs)
    total_size = data_size + len(glyphs) * 5  # offsets(2) + width(1) + advance(1) + charmap(~1)
    print(f'Written: {out_path}')
    print(f'Bitmap data: {data_size} bytes, total overhead: ~{total_size} bytes')


if __name__ == '__main__':
    main()
