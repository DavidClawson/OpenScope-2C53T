/*
 * OpenScope 2C53T - Font Rendering Engine
 *
 * Renders variable-width bitmap fonts to the ST7789V LCD.
 * Supports opaque (fg+bg) and transparent (fg only) drawing modes.
 */

#include "font.h"
#include "lcd.h"

/* Look up glyph index for an ASCII character. Returns 0xFF if not found. */
static uint8_t font_glyph_index(char c, const font_t *font)
{
    uint8_t code = (uint8_t)c;
    if (code < font->first_char || code > font->last_char)
        return 0xFF;
    return font->charmap[code - font->first_char];
}

uint8_t font_draw_char(uint16_t x, uint16_t y, char c,
                       uint16_t fg, uint16_t bg, const font_t *font)
{
    uint8_t idx = font_glyph_index(c, font);
    if (idx == 0xFF || idx >= font->num_glyphs)
        return font->height / 3;  /* fallback advance for unknown chars */

    uint8_t glyph_w = font->widths[idx];
    uint8_t advance = font->advances[idx];
    uint8_t bytes_per_row = (glyph_w + 7) / 8;
    const uint8_t *glyph_data = &font->data[font->offsets[idx]];
    int transparent = (fg == bg);

    if (x + glyph_w > LCD_WIDTH || y + font->height > LCD_HEIGHT)
        return advance;

    if (transparent) {
        /* Transparent mode: only set foreground pixels, skip background.
         * Must use per-pixel writes (slower but preserves background). */
        for (uint8_t row = 0; row < font->height; row++) {
            const uint8_t *row_data = &glyph_data[row * bytes_per_row];
            for (uint8_t col = 0; col < glyph_w; col++) {
                if (row_data[col / 8] & (0x80 >> (col % 8))) {
                    lcd_set_pixel(x + col, y + row, fg);
                }
            }
        }
    } else {
        /* Opaque mode: use window write for speed */
        lcd_set_window(x, y, glyph_w, font->height);
        for (uint8_t row = 0; row < font->height; row++) {
            const uint8_t *row_data = &glyph_data[row * bytes_per_row];
            for (uint8_t col = 0; col < glyph_w; col++) {
                if (row_data[col / 8] & (0x80 >> (col % 8))) {
                    lcd_write_data(fg);
                } else {
                    lcd_write_data(bg);
                }
            }
        }
    }

    return advance;
}

uint16_t font_draw_string(uint16_t x, uint16_t y, const char *str,
                          uint16_t fg, uint16_t bg, const font_t *font)
{
    uint16_t start_x = x;
    while (*str) {
        if (x + font->height > LCD_WIDTH)  /* rough overflow check */
            break;
        x += font_draw_char(x, y, *str, fg, bg, font);
        str++;
    }
    return x - start_x;
}

uint16_t font_string_width(const char *str, const font_t *font)
{
    uint16_t width = 0;
    while (*str) {
        uint8_t idx = font_glyph_index(*str, font);
        if (idx != 0xFF && idx < font->num_glyphs) {
            width += font->advances[idx];
        } else {
            width += font->height / 3;
        }
        str++;
    }
    return width;
}

uint16_t font_draw_string_right(uint16_t x_right, uint16_t y, const char *str,
                                uint16_t fg, uint16_t bg, const font_t *font)
{
    uint16_t w = font_string_width(str, font);
    uint16_t x = (x_right >= w) ? x_right - w : 0;
    return font_draw_string(x, y, str, fg, bg, font);
}

uint16_t font_draw_string_center(uint16_t x_center, uint16_t y, const char *str,
                                 uint16_t fg, uint16_t bg, const font_t *font)
{
    uint16_t w = font_string_width(str, font);
    uint16_t x = (x_center >= w / 2) ? x_center - w / 2 : 0;
    return font_draw_string(x, y, str, fg, bg, font);
}
