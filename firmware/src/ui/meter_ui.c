/*
 * OpenScope 2C53T - Multimeter UI
 *
 * 10 sub-modes matching original firmware (device_mode 5-14):
 *   0: DC Voltage      1: AC Voltage      2: DC mA
 *   3: DC A            4: AC mA           5: AC A
 *   6: Resistance      7: Continuity      8: Diode
 *   9: Capacitance
 *
 * Each mode renders: large reading, unit, bar graph, min/max/avg,
 * range info. Demo values shown until real ADC hardware is connected.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"

/* Layout constants */
#define METER_TOP       18          /* Below status bar */
#define METER_BOTTOM    (LCD_HEIGHT - 18)  /* Above info bar */
#define MAIN_READING_Y  30          /* Main digits vertical position */
#define UNIT_X          248         /* Right side for units */
#define BAR_Y           90          /* Bar graph vertical position */
#define BAR_X           16
#define BAR_W           288
#define BAR_H           10
#define SECONDARY_Y     115         /* Min/Max/Avg start */

/* ═══════════════════════════════════════════════════════════════════
 * Meter mode descriptor table
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;           /* Short name for status bar */
    const char *unit;           /* Display unit (V, mA, A, etc.) */
    const char *ac_dc;          /* "DC", "AC", or "" */
    const char *range_label;    /* Auto range label */
    const char *demo_value;     /* Demo reading string */
    float       demo_bar_pct;   /* Bar graph fill fraction */
    float       bar_max;        /* Full-scale value for bar */
    const char *bar_max_label;  /* Label at right end of bar */
    const char *symbol;         /* Extra symbol (e.g., "~" for AC, diode) */
} meter_mode_info_t;

static const meter_mode_info_t meter_modes[METER_SUBMODE_COUNT] = {
    /* 0: DC Voltage */
    { "DC Voltage",  "V",    "DC", "Auto 20V",    "13.82",  0.69f,  20.0f,   "20V",   ""  },
    /* 1: AC Voltage */
    { "AC Voltage",  "V",    "AC", "Auto 20V",    "120.3",  0.60f,  200.0f,  "200V",  "~" },
    /* 2: DC Current (small) */
    { "DC mA",       "mA",   "DC", "Auto 200mA",  "47.83",  0.24f,  200.0f,  "200mA", ""  },
    /* 3: DC Current (large) */
    { "DC Current",  "A",    "DC", "Auto 10A",    "2.156",  0.22f,  10.0f,   "10A",   ""  },
    /* 4: AC Current (small) */
    { "AC mA",       "mA",   "AC", "Auto 200mA",  "35.12",  0.18f,  200.0f,  "200mA", "~" },
    /* 5: AC Current (large) */
    { "AC Current",  "A",    "AC", "Auto 10A",    "1.832",  0.18f,  10.0f,   "10A",   "~" },
    /* 6: Resistance */
    { "Resistance",  "kOhm", "",   "Auto 20kOhm",  "4.700",  0.24f,  20.0f,   "20kOhm", ""  },
    /* 7: Continuity */
    { "Continuity",  "Ohm",  "",   "200 Ohm",       "0.3",    0.002f, 200.0f,  "200Ohm",  ""  },
    /* 8: Diode */
    { "Diode",       "V",    "",   "Diode",       "0.623",  0.31f,  2.0f,    "2V",    ""  },
    /* 9: Capacitance */
    { "Capacitance", "nF",   "",   "Auto 200nF",  "103.4",  0.52f,  200.0f,  "200nF", ""  },
};

/* ═══════════════════════════════════════════════════════════════════
 * Min / Max / Avg tracking state
 * ═══════════════════════════════════════════════════════════════════ */

static float meter_min_val;
static float meter_max_val;
static float meter_avg_accum;
static uint32_t meter_avg_count;
static uint8_t meter_stats_valid;

static float simple_atof(const char *s)
{
    float result = 0.0f;
    float frac = 0.0f;
    float div = 1.0f;
    int after_dot = 0;
    int negative = 0;

    if (*s == '-') { negative = 1; s++; }
    while (*s) {
        if (*s == '.') { after_dot = 1; s++; continue; }
        if (*s < '0' || *s > '9') break;
        if (after_dot) {
            div *= 10.0f;
            frac += (*s - '0') / div;
        } else {
            result = result * 10.0f + (*s - '0');
        }
        s++;
    }
    result += frac;
    return negative ? -result : result;
}

void meter_reset_minmaxavg(void)
{
    meter_stats_valid = 0;
    meter_min_val = 0.0f;
    meter_max_val = 0.0f;
    meter_avg_accum = 0.0f;
    meter_avg_count = 0;
}

static void meter_update_stats(float value)
{
    if (!meter_stats_valid) {
        meter_min_val = value;
        meter_max_val = value;
        meter_avg_accum = value;
        meter_avg_count = 1;
        meter_stats_valid = 1;
    } else {
        if (value < meter_min_val) meter_min_val = value;
        if (value > meter_max_val) meter_max_val = value;
        meter_avg_accum += value;
        meter_avg_count++;
        if (meter_avg_count > 10000) {
            meter_avg_accum = meter_avg_accum / (float)meter_avg_count;
            meter_avg_count = 1;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Helper: format float into buffer (no snprintf on bare metal)
 * ═══════════════════════════════════════════════════════════════════ */

static void fmt_float(char *buf, int buf_size, float val, int decimals)
{
    int i = 0;
    if (val < 0.0f) {
        buf[i++] = '-';
        val = -val;
    }

    uint32_t int_part = (uint32_t)val;
    float frac_part = val - (float)int_part;

    char tmp[12];
    int t = 0;
    if (int_part == 0) {
        tmp[t++] = '0';
    } else {
        while (int_part > 0 && t < 11) {
            tmp[t++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    for (int j = t - 1; j >= 0 && i < buf_size - 1; j--) {
        buf[i++] = tmp[j];
    }

    if (decimals > 0 && i < buf_size - 2) {
        buf[i++] = '.';
        for (int d = 0; d < decimals && i < buf_size - 1; d++) {
            frac_part *= 10.0f;
            int digit = (int)frac_part;
            if (digit > 9) digit = 9;
            buf[i++] = '0' + digit;
            frac_part -= (float)digit;
        }
    }
    buf[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 * Drawing functions
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_bar_graph(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           float fraction, uint16_t fg_color)
{
    const theme_t *th = theme_get();

    lcd_fill_rect(x, y, w, h, th->grid);

    uint16_t fill_w = (uint16_t)(fraction * w);
    if (fill_w > w) fill_w = w;
    if (fill_w > 0) {
        lcd_fill_rect(x, y, fill_w, h, fg_color);
    }

    for (int i = 1; i <= 3; i++) {
        uint16_t tick_x = x + (w * i) / 4;
        lcd_fill_rect(tick_x, y, 1, h, th->grid_center);
    }
}

static void draw_buzzer_indicator(uint16_t x, uint16_t y, const theme_t *th,
                                  int is_short)
{
    uint16_t color = is_short ? th->success : th->text_secondary;
    const char *label = is_short ? "BEEP" : "OPEN";
    font_draw_string(x, y, label, color, th->background, &font_large);
}

static void draw_diode_indicator(uint16_t x, uint16_t y, const theme_t *th)
{
    font_draw_string(x, y, "|>|", th->ch1, th->background, &font_medium);
}

const char *meter_submode_name(uint8_t submode)
{
    if (submode >= METER_SUBMODE_COUNT) return "???";
    return meter_modes[submode].name;
}

/* ═══════════════════════════════════════════════════════════════════
 * Main meter screen draw
 * ═══════════════════════════════════════════════════════════════════ */

void draw_meter_screen(void)
{
    const theme_t *th = theme_get();
    uint8_t mode = meter_submode;
    if (mode >= METER_SUBMODE_COUNT) mode = 0;

    const meter_mode_info_t *m = &meter_modes[mode];

    float current_val = simple_atof(m->demo_value);
    meter_update_stats(current_val);

    /* Clear content area */
    lcd_fill_rect(0, METER_TOP, LCD_WIDTH, METER_BOTTOM - METER_TOP, th->background);

    /* Mode indicator arrows (show L/R navigation) */
    font_draw_string(4, MAIN_READING_Y, "<",
                     th->text_secondary, th->background, &font_small);
    font_draw_string_right(LCD_WIDTH - 4, MAIN_READING_Y, ">",
                           th->text_secondary, th->background, &font_small);

    /* Main reading: large digits, right-aligned */
    font_draw_string_right(240, MAIN_READING_Y, m->demo_value,
                           th->text_primary, th->background, &font_xlarge);

    /* Unit label */
    font_draw_string(UNIT_X, MAIN_READING_Y + 4, m->unit,
                     th->ch1, th->background, &font_large);

    /* AC/DC indicator */
    if (m->ac_dc[0] != '\0') {
        font_draw_string(UNIT_X, MAIN_READING_Y + 30, m->ac_dc,
                         th->text_secondary, th->background, &font_medium);
    }

    /* AC tilde symbol */
    if (m->symbol[0] == '~') {
        font_draw_string(UNIT_X + 24, MAIN_READING_Y + 30, "~",
                         th->ch1, th->background, &font_medium);
    }

    /* Special indicators for continuity and diode modes */
    if (mode == 7) {
        int is_short = (current_val < 50.0f);
        draw_buzzer_indicator(170, SECONDARY_Y + 44, th, is_short);
    } else if (mode == 8) {
        draw_diode_indicator(170, SECONDARY_Y + 44, th);
    }

    /* Bar graph */
    draw_bar_graph(BAR_X, BAR_Y, BAR_W, BAR_H, m->demo_bar_pct, th->success);

    font_draw_string(BAR_X, BAR_Y + BAR_H + 2, "0",
                     th->text_secondary, th->background, &font_small);
    font_draw_string_right(BAR_X + BAR_W, BAR_Y + BAR_H + 2, m->bar_max_label,
                           th->text_secondary, th->background, &font_small);

    /* Secondary readings: Min / Max / Avg */
    char val_buf[16];

    font_draw_string(16, SECONDARY_Y, "MIN",
                     th->text_secondary, th->background, &font_small);
    if (meter_stats_valid) {
        fmt_float(val_buf, sizeof(val_buf), meter_min_val, 2);
    } else {
        val_buf[0] = '-'; val_buf[1] = '-'; val_buf[2] = '-'; val_buf[3] = '\0';
    }
    font_draw_string_right(120, SECONDARY_Y, val_buf,
                           th->text_primary, th->background, &font_medium);
    font_draw_string(122, SECONDARY_Y, m->unit,
                     th->text_secondary, th->background, &font_small);

    font_draw_string(16, SECONDARY_Y + 22, "MAX",
                     th->text_secondary, th->background, &font_small);
    if (meter_stats_valid) {
        fmt_float(val_buf, sizeof(val_buf), meter_max_val, 2);
    } else {
        val_buf[0] = '-'; val_buf[1] = '-'; val_buf[2] = '-'; val_buf[3] = '\0';
    }
    font_draw_string_right(120, SECONDARY_Y + 22, val_buf,
                           th->text_primary, th->background, &font_medium);
    font_draw_string(122, SECONDARY_Y + 22, m->unit,
                     th->text_secondary, th->background, &font_small);

    font_draw_string(16, SECONDARY_Y + 44, "AVG",
                     th->text_secondary, th->background, &font_small);
    if (meter_stats_valid && meter_avg_count > 0) {
        float avg = meter_avg_accum / (float)meter_avg_count;
        fmt_float(val_buf, sizeof(val_buf), avg, 2);
    } else {
        val_buf[0] = '-'; val_buf[1] = '-'; val_buf[2] = '-'; val_buf[3] = '\0';
    }
    font_draw_string_right(120, SECONDARY_Y + 44, val_buf,
                           th->text_primary, th->background, &font_medium);
    font_draw_string(122, SECONDARY_Y + 44, m->unit,
                     th->text_secondary, th->background, &font_small);

    /* Range info */
    font_draw_string(200, SECONDARY_Y, "Range:",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(200, SECONDARY_Y + 16, m->range_label,
                     th->ch1, th->background, &font_medium);

    /* Sub-mode index indicator (e.g., "3/10") */
    {
        char idx_buf[8];
        int idx = 0;
        if (mode >= 9) {
            idx_buf[idx++] = '1';
            idx_buf[idx++] = '0';
        } else {
            idx_buf[idx++] = '1' + mode;
        }
        idx_buf[idx++] = '/';
        idx_buf[idx++] = '1';
        idx_buf[idx++] = '0';
        idx_buf[idx] = '\0';
        font_draw_string(200, SECONDARY_Y + 36, idx_buf,
                         th->text_secondary, th->background, &font_small);
    }
}
