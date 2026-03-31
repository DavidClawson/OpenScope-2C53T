/*
 * OpenScope 2C53T - Resistor Color Band Calculator
 *
 * Interactive graphical resistor with selectable color bands.
 * Calculates expected value from bands, compares against measured
 * resistance for pass/fail verification.
 *
 * Controls:
 *   LEFT/RIGHT — move between bands
 *   UP/DOWN    — change band color
 *   OK         — back to component tester
 *   SELECT     — measure and compare (future: real ADC)
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Color band definitions (standard 4-band resistor)
 * ═══════════════════════════════════════════════════════════════════ */

#define NUM_BANDS       4       /* Band 1, Band 2, Multiplier, Tolerance */
#define NUM_DIGIT_COLORS 10     /* 0-9 for digit bands */
#define NUM_MULT_COLORS  9      /* x1 to x0.01 */
#define NUM_TOL_COLORS   5      /* 1%, 2%, 5%, 10%, 20% */

/* RGB565 colors for resistor bands */
static const uint16_t band_colors[] = {
    0x0000,     /* 0: Black */
    0x8200,     /* 1: Brown (dark red-brown) */
    0xF800,     /* 2: Red */
    0xFC00,     /* 3: Orange */
    0xFFE0,     /* 4: Yellow */
    0x07E0,     /* 5: Green */
    0x001F,     /* 6: Blue */
    0x780F,     /* 7: Violet */
    0x8410,     /* 8: Gray */
    0xFFFF,     /* 9: White */
    0xCA40,     /* Gold (for multiplier/tolerance) */
    0xC618,     /* Silver (for multiplier/tolerance) */
};

static const char *band_color_names[] = {
    "Black", "Brown", "Red", "Orange", "Yellow",
    "Green", "Blue", "Violet", "Gray", "White",
    "Gold", "Silver",
};

/* Digit band values (index 0-9 = digit 0-9) */

/* Multiplier values (band index -> multiplier) */
static const float mult_values[] = {
    1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f,
    100000.0f, 1000000.0f,
    0.1f,   /* Gold = index 10 */
    0.01f,  /* Silver = index 11 */
};

/* Multiplier band valid indices: 0-7 map to colors 0-7, index 8=Gold(10), index 9=Silver(11) */
static const uint8_t mult_color_idx[] = { 0, 1, 2, 3, 4, 5, 6, 7, 10, 11 };
#define NUM_MULT_STEPS 10

/* Tolerance band valid indices and values */
static const uint8_t tol_color_idx[] = { 1, 2, 5, 10, 11 };
static const float tol_values[]     = { 1.0f, 2.0f, 0.5f, 5.0f, 10.0f };
static const char *tol_labels[]     = { "1%", "2%", "0.5%", "5%", "10%" };
#define NUM_TOL_STEPS 5

/* ═══════════════════════════════════════════════════════════════════
 * Calculator state
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t active_band = 0;      /* 0-3: which band is selected */
static uint8_t band1_digit = 4;      /* Digit 1 (0-9) -> Brown=4 */
static uint8_t band2_digit = 7;      /* Digit 2 (0-9) -> Violet=7 */
static uint8_t mult_step = 2;        /* Multiplier step index (0-9) */
static uint8_t tol_step = 3;         /* Tolerance step index (0-4) */

/* Measured value for comparison (demo) */
static float measured_ohms = 0.0f;
static bool  has_measurement = false;

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void resistor_calc_init(void)
{
    active_band = 0;
    band1_digit = 4;  /* 4.7k default */
    band2_digit = 7;
    mult_step = 3;    /* x1k */
    tol_step = 3;     /* 5% (gold) */
    has_measurement = false;
}

void resistor_calc_move_band(int direction)
{
    if (direction > 0) {
        active_band = (active_band + 1) % NUM_BANDS;
    } else {
        active_band = (active_band + NUM_BANDS - 1) % NUM_BANDS;
    }
}

void resistor_calc_change_color(int direction)
{
    switch (active_band) {
    case 0: /* Band 1 digit */
        if (direction > 0)
            band1_digit = (band1_digit + 1) % 10;
        else
            band1_digit = (band1_digit + 9) % 10;
        break;
    case 1: /* Band 2 digit */
        if (direction > 0)
            band2_digit = (band2_digit + 1) % 10;
        else
            band2_digit = (band2_digit + 9) % 10;
        break;
    case 2: /* Multiplier */
        if (direction > 0)
            mult_step = (mult_step + 1) % NUM_MULT_STEPS;
        else
            mult_step = (mult_step + NUM_MULT_STEPS - 1) % NUM_MULT_STEPS;
        break;
    case 3: /* Tolerance */
        if (direction > 0)
            tol_step = (tol_step + 1) % NUM_TOL_STEPS;
        else
            tol_step = (tol_step + NUM_TOL_STEPS - 1) % NUM_TOL_STEPS;
        break;
    }
}

float resistor_calc_get_value(void)
{
    float digits = (float)(band1_digit * 10 + band2_digit);
    uint8_t mi = mult_color_idx[mult_step];
    float mult = (mi <= 7) ? mult_values[mi] : mult_values[mi - 3];
    /* Gold=index 10 -> mult_values[7]=0.1, Silver=index 11 -> mult_values[8]=0.01 */
    return digits * mult;
}

void resistor_calc_simulate_measure(void)
{
    /* Demo: measured value is expected +-3% */
    float expected = resistor_calc_get_value();
    measured_ohms = expected * 1.02f;  /* Simulate +2% deviation */
    has_measurement = true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Format resistance value
 * ═══════════════════════════════════════════════════════════════════ */

static void format_r_value(float ohms, char *buf, int bufsize)
{
    const char *suffix;
    float val;

    if (ohms >= 1000000.0f) {
        val = ohms / 1000000.0f;
        suffix = "M";
    } else if (ohms >= 1000.0f) {
        val = ohms / 1000.0f;
        suffix = "k";
    } else {
        val = ohms;
        suffix = "";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 100.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 1000 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 1000) % 10);
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 100) % 10);
    if (integer >= 10 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);

    if (frac > 0) {
        if (pos < bufsize - 1) buf[pos++] = '.';
        if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac / 10);
        if (frac % 10 != 0 && pos < bufsize - 1)
            buf[pos++] = (char)('0' + frac % 10);
    }

    while (*suffix && pos < bufsize - 1)
        buf[pos++] = *suffix++;

    buf[pos] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 * Drawing
 * ═══════════════════════════════════════════════════════════════════ */

/* Draw a single color band on the resistor body */
static void draw_band(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color, bool selected, const theme_t *th)
{
    lcd_fill_rect(x, y, w, h, color);

    /* Selection indicator: bracket above and below */
    if (selected) {
        lcd_fill_rect(x, y - 3, w, 2, th->highlight);
        lcd_fill_rect(x, y + h + 1, w, 2, th->highlight);
    }
}

void draw_resistor_calc_screen(void)
{
    const theme_t *th = theme_get();

    /* Clear content area */
    lcd_fill_rect(0, 18, LCD_WIDTH, LCD_HEIGHT - 36, th->background);

    /* Title */
    font_draw_string_center(LCD_WIDTH / 2, 20, "Resistor Calculator",
                            th->text_primary, th->background, &font_medium);

    /* ── Resistor body ───────────────────────────────────── */
    uint16_t body_x = 40;
    uint16_t body_y = 55;
    uint16_t body_w = 240;
    uint16_t body_h = 40;

    /* Lead wires */
    lcd_fill_rect(20, body_y + body_h / 2 - 1, 20, 3, th->text_secondary);
    lcd_fill_rect(body_x + body_w, body_y + body_h / 2 - 1, 20, 3, th->text_secondary);

    /* Resistor body (beige/tan) */
    uint16_t body_color = 0xD69A;  /* Tan/beige in RGB565 */
    lcd_fill_rect(body_x, body_y, body_w, body_h, body_color);

    /* Rounded ends (simple: darker rectangles) */
    lcd_fill_rect(body_x, body_y, 8, body_h, 0xB596);
    lcd_fill_rect(body_x + body_w - 8, body_y, 8, body_h, 0xB596);

    /* ── Color bands ─────────────────────────────────────── */
    uint16_t band_w = 16;
    uint16_t band_h = body_h - 4;
    uint16_t band_y = body_y + 2;

    /* Band positions on the resistor body */
    uint16_t band_x[4] = {
        body_x + 30,        /* Band 1 */
        body_x + 55,        /* Band 2 */
        body_x + 80,        /* Multiplier */
        body_x + body_w - 50, /* Tolerance (gap before it) */
    };

    /* Get color indices for each band */
    uint8_t color_idx[4];
    color_idx[0] = band1_digit;
    color_idx[1] = band2_digit;
    color_idx[2] = mult_color_idx[mult_step];
    color_idx[3] = tol_color_idx[tol_step];

    for (int i = 0; i < 4; i++) {
        draw_band(band_x[i], band_y, band_w, band_h,
                  band_colors[color_idx[i]], (i == active_band), th);
    }

    /* ── Band labels below resistor ──────────────────────── */
    uint16_t label_y = body_y + body_h + 8;
    const char *band_labels[4] = { "Digit1", "Digit2", "Multi", "Toler" };

    for (int i = 0; i < 4; i++) {
        uint16_t fg = (i == active_band) ? th->highlight : th->text_secondary;
        font_draw_string(band_x[i] - 4, label_y, band_labels[i],
                         fg, th->background, &font_small);
    }

    /* Active band color name */
    const char *active_color_name = band_color_names[color_idx[active_band]];
    font_draw_string_center(LCD_WIDTH / 2, label_y + 16, active_color_name,
                            th->highlight, th->background, &font_medium);

    /* ── Calculated value ────────────────────────────────── */
    uint16_t val_y = 140;
    float expected = resistor_calc_get_value();
    char val_buf[24];

    font_draw_string(16, val_y, "Expected:",
                     th->text_secondary, th->background, &font_medium);

    format_r_value(expected, val_buf, sizeof(val_buf));
    font_draw_string(110, val_y, val_buf,
                     th->text_primary, th->background, &font_large);

    /* Tolerance */
    font_draw_string(240, val_y + 4, tol_labels[tol_step],
                     th->ch1, th->background, &font_medium);

    /* ── Measured comparison ─────────────────────────────── */
    uint16_t meas_y = val_y + 30;

    if (has_measurement) {
        font_draw_string(16, meas_y, "Measured:",
                         th->text_secondary, th->background, &font_medium);
        format_r_value(measured_ohms, val_buf, sizeof(val_buf));
        font_draw_string(110, meas_y, val_buf,
                         th->ch2, th->background, &font_large);

        /* Deviation and pass/fail */
        float dev_pct = 0.0f;
        if (expected > 0.001f) {
            dev_pct = ((measured_ohms - expected) / expected) * 100.0f;
        }
        float tolerance = tol_values[tol_step];
        float abs_dev = dev_pct < 0 ? -dev_pct : dev_pct;
        bool pass = (abs_dev <= tolerance);

        /* Deviation percentage */
        char dev_buf[16];
        int dev_sign = (dev_pct < 0);
        float abs_pct = dev_sign ? -dev_pct : dev_pct;
        int di = 0;
        if (dev_sign && di < 14) dev_buf[di++] = '-';
        else if (di < 14) dev_buf[di++] = '+';
        int dev_int = (int)abs_pct;
        int dev_frac = (int)((abs_pct - (float)dev_int) * 10.0f);
        if (dev_int >= 10 && di < 14) dev_buf[di++] = (char)('0' + (dev_int / 10) % 10);
        if (di < 14) dev_buf[di++] = (char)('0' + dev_int % 10);
        if (di < 14) dev_buf[di++] = '.';
        if (di < 14) dev_buf[di++] = (char)('0' + dev_frac);
        if (di < 14) dev_buf[di++] = '%';
        dev_buf[di] = '\0';

        uint16_t result_color = pass ? th->success : th->warning;
        const char *result_str = pass ? "PASS" : "FAIL";

        /* Result badge */
        uint16_t badge_y = meas_y + 28;
        uint16_t badge_w = font_string_width(result_str, &font_large);
        lcd_fill_rect(16, badge_y - 1, badge_w + 16, 26, result_color);
        font_draw_string(24, badge_y, result_str,
                         th->background, result_color, &font_large);

        font_draw_string(badge_w + 40, badge_y + 4, dev_buf,
                         result_color, th->background, &font_medium);
    } else {
        font_draw_string(16, meas_y, "Press SELECT to measure",
                         th->text_secondary, th->background, &font_small);
    }

    /* Navigation hints */
    font_draw_string_center(LCD_WIDTH / 2, LCD_HEIGHT - 34,
                            "< > Band   ^ v Color   OK:Back",
                            th->text_secondary, th->background, &font_small);
}
