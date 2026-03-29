/*
 * OpenScope 2C53T - Multimeter UI
 *
 * Clean DMM-style display with large digits, bar graph,
 * and min/max/avg secondary readings.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"

/* Layout constants */
#define METER_TOP       18          /* Below status bar */
#define METER_BOTTOM    (LCD_HEIGHT - 18)  /* Above info bar */
#define MAIN_READING_Y  30          /* Main digits vertical position */
#define UNIT_X          270         /* Right side for units */
#define BAR_Y           90          /* Bar graph vertical position */
#define BAR_X           16
#define BAR_W           288
#define BAR_H           10
#define SECONDARY_Y     115         /* Min/Max/Avg start */

/* Demo values (will be replaced by real ADC readings) */
static const char *demo_reading = "13.82";
static const char *demo_unit = "V";
static const char *demo_mode = "DC";
static const char *demo_range = "Auto 20V";
static float demo_bar_pct = 0.69f;  /* 13.82 / 20.0 */

/* Draw the horizontal bar graph */
static void draw_bar_graph(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           float fraction, uint16_t fg_color)
{
    /* Background track */
    lcd_fill_rect(x, y, w, h, COLOR_DARK_GRAY);

    /* Filled portion */
    uint16_t fill_w = (uint16_t)(fraction * w);
    if (fill_w > w) fill_w = w;
    if (fill_w > 0) {
        lcd_fill_rect(x, y, fill_w, h, fg_color);
    }

    /* Tick marks at 25%, 50%, 75% */
    for (int i = 1; i <= 3; i++) {
        uint16_t tick_x = x + (w * i) / 4;
        lcd_fill_rect(tick_x, y, 1, h, COLOR_GRID);
    }
}

/* Draw the multimeter screen */
void draw_meter_screen(void)
{
    /* Clear content area */
    lcd_fill_rect(0, METER_TOP, LCD_WIDTH, METER_BOTTOM - METER_TOP, COLOR_BLACK);

    /* === Main reading: large digits, right-aligned === */
    /* Reading value - right-aligned so digits don't jump as value changes */
    font_draw_string_right(240, MAIN_READING_Y, demo_reading,
                           COLOR_WHITE, COLOR_BLACK, &font_xlarge);

    /* Unit label next to reading */
    font_draw_string(248, MAIN_READING_Y + 4, demo_unit,
                     COLOR_YELLOW, COLOR_BLACK, &font_large);

    /* AC/DC indicator */
    font_draw_string(248, MAIN_READING_Y + 30, demo_mode,
                     COLOR_GRAY, COLOR_BLACK, &font_medium);

    /* === Bar graph === */
    draw_bar_graph(BAR_X, BAR_Y, BAR_W, BAR_H, demo_bar_pct, COLOR_GREEN);

    /* Bar scale labels */
    font_draw_string(BAR_X, BAR_Y + BAR_H + 2, "0",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_right(BAR_X + BAR_W, BAR_Y + BAR_H + 2, "20V",
                           COLOR_GRAY, COLOR_BLACK, &font_small);

    /* === Secondary readings === */
    font_draw_string(16, SECONDARY_Y, "MIN",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_right(120, SECONDARY_Y, "13.79V",
                           COLOR_WHITE, COLOR_BLACK, &font_medium);

    font_draw_string(16, SECONDARY_Y + 22, "MAX",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_right(120, SECONDARY_Y + 22, "13.86V",
                           COLOR_WHITE, COLOR_BLACK, &font_medium);

    font_draw_string(16, SECONDARY_Y + 44, "AVG",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_right(120, SECONDARY_Y + 44, "13.82V",
                           COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* === Range info === */
    font_draw_string(200, SECONDARY_Y, "Range:",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string(200, SECONDARY_Y + 16, demo_range,
                     COLOR_YELLOW, COLOR_BLACK, &font_medium);

    /* Hold indicator (when implemented) */
    /* font_draw_string(200, SECONDARY_Y + 44, "HOLD",
                        COLOR_RED, COLOR_BLACK, &font_medium); */
}
