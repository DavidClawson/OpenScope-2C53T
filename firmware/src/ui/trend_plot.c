/*
 * FNIRSI 2C53T Trend Plot Display
 *
 * Circular buffer of measurement values rendered as a line graph.
 * Auto-scales Y axis with 10% margin. Shows grid, labels, and statistics.
 */

#include "trend_plot.h"
#include <string.h>
#include <math.h>

#ifndef TEST_BUILD
#include "lcd.h"
#else
/* Stubs for test builds */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
void lcd_set_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);
#endif

/* --------------------------------------------------------------------------
 * Bresenham line drawing (internal helper)
 * -------------------------------------------------------------------------- */
static void trend_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                            uint16_t color)
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
        lcd_set_pixel((uint16_t)cx, (uint16_t)cy, color);

        if (cx == (int16_t)x1 && cy == (int16_t)y1)
            break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
    }
}

/* --------------------------------------------------------------------------
 * Scan all stored values and recompute min/max with 10% margin
 * -------------------------------------------------------------------------- */
static void trend_update_range(trend_state_t *state)
{
    if (!state || state->num_points == 0)
        return;

    uint16_t count = state->num_points < TREND_MAX_POINTS
                     ? state->num_points : TREND_MAX_POINTS;

    float lo = state->values[0];
    float hi = state->values[0];

    for (uint16_t i = 1; i < count; i++) {
        if (state->values[i] < lo) lo = state->values[i];
        if (state->values[i] > hi) hi = state->values[i];
    }

    /* Add 10% margin */
    float range = hi - lo;
    if (range < 1e-9f) {
        /* All values are the same or nearly so */
        range = fabsf(lo) * 0.1f;
        if (range < 1e-9f) range = 1.0f;
    }
    float margin = range * 0.1f;
    state->min_val = lo - margin;
    state->max_val = hi + margin;
}

/* --------------------------------------------------------------------------
 * trend_init
 * -------------------------------------------------------------------------- */
void trend_init(trend_state_t *state, trend_source_t source, float interval_s)
{
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->source = source;
    state->sample_interval_s = interval_s;
    state->min_val = 0.0f;
    state->max_val = 1.0f;
}

/* --------------------------------------------------------------------------
 * trend_add_point - Insert one measurement into the circular buffer
 * -------------------------------------------------------------------------- */
void trend_add_point(trend_state_t *state, float value)
{
    if (!state)
        return;

    state->values[state->write_pos] = value;
    state->write_pos = (state->write_pos + 1) % TREND_MAX_POINTS;

    if (state->num_points < TREND_MAX_POINTS)
        state->num_points++;

    /* Cast away const for internal mutation — state is mutable here */
    trend_update_range(state);
}

/* --------------------------------------------------------------------------
 * Map a float value to a Y pixel coordinate within the display area
 * -------------------------------------------------------------------------- */
static uint16_t value_to_y(float val, float min_v, float max_v,
                           uint16_t y_off, uint16_t height)
{
    if (max_v <= min_v)
        return y_off + height / 2;

    float ratio = (val - min_v) / (max_v - min_v);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    /* Invert: high values at top */
    return y_off + (uint16_t)((1.0f - ratio) * (float)(height - 1));
}

/* --------------------------------------------------------------------------
 * trend_render - Draw the trend line graph
 * -------------------------------------------------------------------------- */
void trend_render(const trend_state_t *state,
                  uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                  uint16_t color, uint16_t grid_color)
{
    if (!state || state->num_points < 2 || width < 2 || height < 2)
        return;

    uint16_t draw_count = state->num_points < width ? state->num_points : width;
    float min_v = state->min_val;
    float max_v = state->max_val;

    /* Draw horizontal grid lines (4 lines dividing area into 5 sections) */
    for (int g = 1; g <= 4; g++) {
        uint16_t gy = y_off + (uint16_t)((uint32_t)g * height / 5);
        for (uint16_t gx = x_off; gx < x_off + width; gx += 4) {
            lcd_set_pixel(gx, gy, grid_color);
        }
    }

    /* Draw the trend line: oldest visible at left, newest at right */
    for (uint16_t col = 1; col < draw_count; col++) {
        uint16_t pixel_x_prev = x_off + width - draw_count + col - 1;
        uint16_t pixel_x_curr = x_off + width - draw_count + col;

        int16_t buf_idx_prev = ((int16_t)state->write_pos - (int16_t)draw_count + (int16_t)col - 1
                                + TREND_MAX_POINTS) % TREND_MAX_POINTS;
        int16_t buf_idx_curr = ((int16_t)state->write_pos - (int16_t)draw_count + (int16_t)col
                                + TREND_MAX_POINTS) % TREND_MAX_POINTS;

        uint16_t y_prev = value_to_y(state->values[buf_idx_prev], min_v, max_v, y_off, height);
        uint16_t y_curr = value_to_y(state->values[buf_idx_curr], min_v, max_v, y_off, height);

        trend_draw_line(pixel_x_prev, y_prev, pixel_x_curr, y_curr, color);
    }
}

/* --------------------------------------------------------------------------
 * Statistics
 * -------------------------------------------------------------------------- */
float trend_get_min(const trend_state_t *state)
{
    if (!state || state->num_points == 0)
        return 0.0f;

    uint16_t count = state->num_points < TREND_MAX_POINTS
                     ? state->num_points : TREND_MAX_POINTS;
    float lo = state->values[0];
    for (uint16_t i = 1; i < count; i++) {
        if (state->values[i] < lo) lo = state->values[i];
    }
    return lo;
}

float trend_get_max(const trend_state_t *state)
{
    if (!state || state->num_points == 0)
        return 0.0f;

    uint16_t count = state->num_points < TREND_MAX_POINTS
                     ? state->num_points : TREND_MAX_POINTS;
    float hi = state->values[0];
    for (uint16_t i = 1; i < count; i++) {
        if (state->values[i] > hi) hi = state->values[i];
    }
    return hi;
}

float trend_get_avg(const trend_state_t *state)
{
    if (!state || state->num_points == 0)
        return 0.0f;

    uint16_t count = state->num_points < TREND_MAX_POINTS
                     ? state->num_points : TREND_MAX_POINTS;
    float sum = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        sum += state->values[i];
    }
    return sum / (float)count;
}

/* --------------------------------------------------------------------------
 * trend_source_name
 * -------------------------------------------------------------------------- */
const char *trend_source_name(trend_source_t source)
{
    switch (source) {
        case TREND_FREQUENCY:  return "Freq";
        case TREND_VPP:        return "Vpp";
        case TREND_VRMS:       return "Vrms";
        case TREND_DUTY_CYCLE: return "Duty";
        default:               return "?";
    }
}
