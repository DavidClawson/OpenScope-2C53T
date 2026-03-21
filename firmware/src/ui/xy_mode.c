/*
 * FNIRSI 2C53T XY Mode (Lissajous) Display
 *
 * Maps CH1 samples to X coordinates and CH2 samples to Y coordinates
 * to produce Lissajous figures and phase-relationship plots.
 */

#include "xy_mode.h"

#ifndef TEST_BUILD
#include "lcd.h"
#else
/* Stub for test builds - records pixels for verification */
#include <string.h>
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
#define COLOR_GREEN 0x07E0

static uint16_t test_fb[LCD_WIDTH * LCD_HEIGHT];

void lcd_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x < LCD_WIDTH && y < LCD_HEIGHT)
        test_fb[y * LCD_WIDTH + x] = color;
}

uint16_t *xy_test_get_fb(void) { return test_fb; }
void xy_test_clear_fb(void) { memset(test_fb, 0, sizeof(test_fb)); }
#endif

/* --------------------------------------------------------------------------
 * xy_render - Plot CH1 vs CH2 as XY scatter
 * --------------------------------------------------------------------------
 * Each sample pair (ch1[i], ch2[i]) is mapped to a pixel:
 *   px = x_off + (ch1[i] + 32768) * (width - 1) / 65535
 *   py = y_off + height - 1 - (ch2[i] + 32768) * (height - 1) / 65535
 * Y is inverted so positive values go up.
 * ----------------------------------------------------------------------- */
void xy_render(const int16_t *ch1, const int16_t *ch2, uint16_t num_samples,
               uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
               uint16_t color)
{
    if (!ch1 || !ch2 || num_samples == 0 || width < 2 || height < 2)
        return;

    uint32_t w = width - 1;
    uint32_t h = height - 1;

    for (uint16_t i = 0; i < num_samples; i++) {
        /* Map signed sample to 0..65535 range */
        uint32_t sx = (uint32_t)((int32_t)ch1[i] + 32768);
        uint32_t sy = (uint32_t)((int32_t)ch2[i] + 32768);

        /* Scale to pixel coordinates */
        uint16_t px = x_off + (uint16_t)(sx * w / 65535);
        uint16_t py = y_off + (uint16_t)(h - sy * h / 65535);

        /* Clamp to display area */
        if (px >= x_off && px < x_off + width &&
            py >= y_off && py < y_off + height) {
            lcd_set_pixel(px, py, color);
            /* Draw 2x2 dot for visibility */
            if (px + 1 < x_off + width)
                lcd_set_pixel(px + 1, py, color);
            if (py + 1 < y_off + height)
                lcd_set_pixel(px, py + 1, color);
            if (px + 1 < x_off + width && py + 1 < y_off + height)
                lcd_set_pixel(px + 1, py + 1, color);
        }
    }
}

/* --------------------------------------------------------------------------
 * xy_render_persist - XY plot with phosphor persistence
 * --------------------------------------------------------------------------
 * persist_buf is width*height bytes. Each frame:
 *   1. Decay all pixels by 1 (fade out)
 *   2. Increment pixels hit by new samples (saturate at 255)
 *   3. Render intensity-mapped colors to LCD
 * ----------------------------------------------------------------------- */
void xy_render_persist(const int16_t *ch1, const int16_t *ch2, uint16_t num_samples,
                       uint8_t *persist_buf, uint16_t width, uint16_t height)
{
    if (!ch1 || !ch2 || !persist_buf || num_samples == 0 || width < 2 || height < 2)
        return;

    uint32_t buf_size = (uint32_t)width * height;
    uint32_t w = width - 1;
    uint32_t h = height - 1;

    /* Decay: subtract 1 from all nonzero pixels */
    for (uint32_t j = 0; j < buf_size; j++) {
        if (persist_buf[j] > 0)
            persist_buf[j]--;
    }

    /* Accumulate new sample hits */
    for (uint16_t i = 0; i < num_samples; i++) {
        uint32_t sx = (uint32_t)((int32_t)ch1[i] + 32768);
        uint32_t sy = (uint32_t)((int32_t)ch2[i] + 32768);

        uint16_t px = (uint16_t)(sx * w / 65535);
        uint16_t py = (uint16_t)(h - sy * h / 65535);

        if (px < width && py < height) {
            uint32_t idx = (uint32_t)py * width + px;
            if (persist_buf[idx] < 255)
                persist_buf[idx]++;
        }
    }

    /* Render: map intensity to green color ramp */
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint8_t val = persist_buf[y * width + x];
            if (val > 0) {
                /* Scale green channel: brighter = more intense */
                uint16_t g = (uint16_t)val * 63 / 255;
                uint16_t color = (uint16_t)(g << 5);  /* RGB565 green only */
                lcd_set_pixel(x, y, color);
            }
        }
    }
}
