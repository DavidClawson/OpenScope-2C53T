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
        /* Per-glyph clipping lives in font_draw_char() (it skips a glyph
         * that would pass the right edge and still returns its advance).
         * This used to break out early on `x + font->height > LCD_WIDTH`,
         * a "rough overflow check" that priced every glyph at the font
         * height (12 px in font_small): a label right-aligned to x = 318
         * lost its last glyph, seen on the bench as `24.9kH` on the FFT
         * axis (2026-09-22). */
        if (x >= LCD_WIDTH)
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

uint16_t font_draw_string_box(uint16_t x, uint16_t y, uint16_t w, const char *str,
                              uint16_t fg, uint16_t bg, const font_t *font,
                              uint8_t align)
{
    if (w == 0 || x >= LCD_WIDTH) return 0;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    uint8_t h = font->height;

    /* Width that fits: whole glyphs only. */
    uint16_t tw = 0;
    const char *end = str;
    while (*end) {
        uint8_t idx = font_glyph_index(*end, font);
        uint16_t adv = (idx != 0xFF && idx < font->num_glyphs)
                       ? font->advances[idx] : (uint16_t)(font->height / 3);
        if (tw + adv > w) break;
        tw += adv;
        end++;
    }

    uint16_t lead = 0;
    if (align == FONT_ALIGN_RIGHT)       lead = w - tw;
    else if (align == FONT_ALIGN_CENTER) lead = (w - tw) / 2;

    if (lead) lcd_fill_rect(x, y, lead, h, bg);
    uint16_t cx = x + lead;
    for (const char *p = str; p < end; p++) {
        uint8_t idx = font_glyph_index(*p, font);
        if (idx == 0xFF || idx >= font->num_glyphs) {
            uint16_t adv = font->height / 3;
            lcd_fill_rect(cx, y, adv, h, bg);
            cx += adv;
            continue;
        }
        uint8_t gw  = font->widths[idx];
        uint8_t adv = font->advances[idx];
        font_draw_char(cx, y, *p, fg, (fg == bg) ? (uint16_t)~fg : bg, font);
        if (adv > gw) lcd_fill_rect(cx + gw, y, adv - gw, h, bg);
        cx += adv;
    }
    if (cx < x + w) lcd_fill_rect(cx, y, (uint16_t)(x + w - cx), h, bg);
    return tw;
}

bool font_has_glyphs(const char *str, const font_t *font)
{
    for (; *str; str++) {
        uint8_t idx = font_glyph_index(*str, font);
        if (idx == 0xFF || idx >= font->num_glyphs) return false;
    }
    return true;
}
