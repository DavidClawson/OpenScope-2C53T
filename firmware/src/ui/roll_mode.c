/*
 * FNIRSI 2C53T Roll Mode Display
 *
 * Circular buffer of samples displayed as a scrolling waveform.
 * Newest sample at right edge, oldest at left. No trigger.
 */

#include "roll_mode.h"
#include <string.h>

#ifndef TEST_BUILD
#include "lcd.h"
#else
/* Stub for test builds */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
void lcd_set_pixel(uint16_t x, uint16_t y, uint16_t color);
#endif

/* --------------------------------------------------------------------------
 * Bresenham line drawing (internal helper)
 * -------------------------------------------------------------------------- */
static void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                      uint16_t color, uint16_t *framebuf, uint16_t fb_width)
{
    int16_t dx = (int16_t)x1 - (int16_t)x0;
    int16_t dy = (int16_t)y1 - (int16_t)y0;
    int16_t sx = dx > 0 ? 1 : -1;
    int16_t sy = dy > 0 ? 1 : -1;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int16_t err = dx - dy;
    int16_t cx = (int16_t)x0, cy = (int16_t)y0;

    for (;;) {
        if (framebuf) {
            framebuf[cy * fb_width + cx] = color;
        } else {
            lcd_set_pixel((uint16_t)cx, (uint16_t)cy, color);
        }

        if (cx == (int16_t)x1 && cy == (int16_t)y1)
            break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
    }
}

/* --------------------------------------------------------------------------
 * Map a signed sample value to a Y pixel coordinate within the display area
 * -------------------------------------------------------------------------- */
static uint16_t sample_to_y(int16_t sample, uint16_t y_off, uint16_t height)
{
    /* Map -32768..32767 to (y_off + height - 1) .. y_off (inverted: positive = up) */
    uint32_t s = (uint32_t)((int32_t)sample + 32768);
    uint16_t py = y_off + (uint16_t)((uint32_t)(height - 1) - s * (height - 1) / 65535);
    return py;
}

/* --------------------------------------------------------------------------
 * roll_init
 * -------------------------------------------------------------------------- */
void roll_init(roll_state_t *state, float time_per_pixel)
{
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->ch1_enabled = true;
    state->ch2_enabled = true;
    state->time_per_pixel = time_per_pixel;
}

/* --------------------------------------------------------------------------
 * roll_add_sample - Insert one sample pair into the circular buffer
 * -------------------------------------------------------------------------- */
void roll_add_sample(roll_state_t *state, int16_t ch1_val, int16_t ch2_val)
{
    if (!state)
        return;

    state->ch1[state->write_pos] = ch1_val;
    state->ch2[state->write_pos] = ch2_val;
    state->write_pos = (state->write_pos + 1) % ROLL_BUFFER_SIZE;

    if (state->count < ROLL_BUFFER_SIZE)
        state->count++;
}

/* --------------------------------------------------------------------------
 * roll_render - Draw the scrolling waveform
 * --------------------------------------------------------------------------
 * Reads from circular buffer: rightmost pixel = most recent sample.
 * Draws connecting lines between adjacent samples for smooth display.
 * ----------------------------------------------------------------------- */
void roll_render(const roll_state_t *state,
                 uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                 uint16_t ch1_color, uint16_t ch2_color,
                 uint16_t *framebuf, uint16_t fb_width)
{
    if (!state || state->count < 2 || width < 2 || height < 2)
        return;

    /* Number of columns to draw (limited by available samples and width) */
    uint16_t draw_count = state->count < width ? state->count : width;

    /* Starting index in the circular buffer for the oldest visible sample.
     * write_pos points to the NEXT write location (= one past newest).
     * Newest sample is at write_pos - 1. We go back draw_count from there. */
    for (uint16_t col = 1; col < draw_count; col++) {
        /* Buffer indices for this column and the previous one.
         * Column 0 = oldest visible, column (draw_count-1) = newest.
         * Pixel X: rightmost = newest. */
        uint16_t pixel_x_prev = x_off + width - draw_count + col - 1;
        uint16_t pixel_x_curr = x_off + width - draw_count + col;

        int16_t buf_idx_prev = ((int16_t)state->write_pos - (int16_t)draw_count + (int16_t)col - 1 + ROLL_BUFFER_SIZE) % ROLL_BUFFER_SIZE;
        int16_t buf_idx_curr = ((int16_t)state->write_pos - (int16_t)draw_count + (int16_t)col + ROLL_BUFFER_SIZE) % ROLL_BUFFER_SIZE;

        if (state->ch1_enabled) {
            uint16_t y_prev = sample_to_y(state->ch1[buf_idx_prev], y_off, height);
            uint16_t y_curr = sample_to_y(state->ch1[buf_idx_curr], y_off, height);
            draw_line(pixel_x_prev, y_prev, pixel_x_curr, y_curr,
                      ch1_color, framebuf, fb_width);
        }

        if (state->ch2_enabled) {
            uint16_t y_prev = sample_to_y(state->ch2[buf_idx_prev], y_off, height);
            uint16_t y_curr = sample_to_y(state->ch2[buf_idx_curr], y_off, height);
            draw_line(pixel_x_prev, y_prev, pixel_x_curr, y_curr,
                      ch2_color, framebuf, fb_width);
        }
    }
}

/* --------------------------------------------------------------------------
 * roll_get_time_span - Total displayed time
 * -------------------------------------------------------------------------- */
float roll_get_time_span(const roll_state_t *state)
{
    if (!state)
        return 0.0f;
    return (float)ROLL_BUFFER_SIZE * state->time_per_pixel;
}
