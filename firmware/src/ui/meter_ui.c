/*
 * OpenScope 2C53T - Multimeter UI
 *
 * 10 sub-modes matching original firmware (device_mode 5-14):
 *   0: DC Voltage      1: AC Voltage      2: DC mA
 *   3: DC A            4: AC mA           5: AC A
 *   6: Resistance      7: Continuity      8: Diode
 *   9: Capacitance
 *
 * 3 switchable layouts (OK to cycle):
 *   Full  — Large digits with bar graph and min/max/avg (classic DMM)
 *   Chart — Reading on top, scrolling strip chart below
 *   Stats — Reading + histogram + min/max/avg/count statistics
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "meter_data.h"
#include "fpga.h"
#include "at32f403a_407.h"
#include <string.h>

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

/* Strip chart constants */
#define CHART_X         10
#define CHART_Y         80
#define CHART_W         300
#define CHART_H         120
#define CHART_SAMPLES   300         /* One sample per pixel column */

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

/* Strip chart ring buffer */
static float chart_buf[CHART_SAMPLES];
static uint16_t chart_head;         /* Next write position */
static uint16_t chart_count;        /* Number of samples stored */
static float chart_min;             /* Auto-scale range */
static float chart_max;

/* Histogram for stats view (20 bins) */
#define HIST_BINS       20
static uint32_t hist_bins[HIST_BINS];
static float hist_min_val;
static float hist_max_val;
static uint32_t hist_total;

/* Auto-hold stability tracking */
#define HOLD_STABLE_COUNT   5       /* Consecutive stable readings to lock */
#define HOLD_THRESHOLD      0.005f  /* Stability threshold (0.5% of reading) */
static float hold_prev_value;
static uint8_t hold_stable_count;

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

    /* Reset chart */
    chart_head = 0;
    chart_count = 0;
    chart_min = 0.0f;
    chart_max = 0.0f;

    /* Reset histogram */
    memset(hist_bins, 0, sizeof(hist_bins));
    hist_total = 0;

    /* Reset hold stability tracking */
    hold_stable_count = 0;
    meter_hold_locked = false;
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

    /* Add to strip chart */
    chart_buf[chart_head] = value;
    chart_head = (chart_head + 1) % CHART_SAMPLES;
    if (chart_count < CHART_SAMPLES) chart_count++;

    /* Update chart auto-scale */
    if (chart_count == 1) {
        chart_min = value - 1.0f;
        chart_max = value + 1.0f;
    } else {
        if (value < chart_min) chart_min = value - (chart_max - chart_min) * 0.1f;
        if (value > chart_max) chart_max = value + (chart_max - chart_min) * 0.1f;
    }
    /* Prevent zero range */
    if (chart_max - chart_min < 0.01f) {
        chart_min -= 0.5f;
        chart_max += 0.5f;
    }

    /* Auto-hold stability detection */
    if (meter_hold_enabled && !meter_hold_locked) {
        float threshold = hold_prev_value * HOLD_THRESHOLD;
        if (threshold < 0.001f) threshold = 0.001f;
        float diff = value - hold_prev_value;
        if (diff < 0) diff = -diff;

        if (diff < threshold) {
            hold_stable_count++;
            if (hold_stable_count >= HOLD_STABLE_COUNT) {
                meter_hold_locked = true;
                meter_hold_value = value;
            }
        } else {
            hold_stable_count = 0;
        }
        hold_prev_value = value;
    }

    /* Update histogram */
    if (meter_stats_valid && meter_max_val > meter_min_val) {
        float range = meter_max_val - meter_min_val;
        if (range < 0.001f) range = 0.001f;
        int bin = (int)((value - meter_min_val) / range * (HIST_BINS - 1));
        if (bin < 0) bin = 0;
        if (bin >= HIST_BINS) bin = HIST_BINS - 1;
        hist_bins[bin]++;
        hist_total++;
        hist_min_val = meter_min_val;
        hist_max_val = meter_max_val;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Helper: format float into buffer
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
 * Shared drawing helpers
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

/* Draw the main reading (shared by all layouts)
 * value_str: the string to display (real or demo) */
static void draw_main_reading(const meter_mode_info_t *m, const theme_t *th,
                              uint16_t y, bool compact, const char *value_str)
{
    if (!compact) {
        /* Mode indicator arrows (show L/R navigation) */
        font_draw_string(4, y, "<",
                         th->text_secondary, th->background, &font_small);
        font_draw_string_right(LCD_WIDTH - 4, y, ">",
                               th->text_secondary, th->background, &font_small);
    }

    /* Main reading */
    font_draw_string_right(240, y, value_str,
                           th->text_primary, th->background,
                           compact ? &font_large : &font_xlarge);

    /* Unit label */
    uint16_t unit_y = compact ? y + 2 : y + 4;
    font_draw_string(UNIT_X, unit_y, m->unit,
                     th->ch1, th->background,
                     compact ? &font_medium : &font_large);

    /* AC/DC indicator */
    if (m->ac_dc[0] != '\0') {
        uint16_t ac_y = compact ? y + 18 : y + 30;
        font_draw_string(UNIT_X, ac_y, m->ac_dc,
                         th->text_secondary, th->background, &font_small);
        if (m->symbol[0] == '~') {
            font_draw_string(UNIT_X + 20, ac_y, "~",
                             th->ch1, th->background, &font_small);
        }
    }
}

static void draw_buzzer_indicator(uint16_t x, uint16_t y, const theme_t *th,
                                  int is_short)
{
    if (is_short) {
        /* Large prominent BEEP badge with green background */
        uint16_t badge_w = 100;
        uint16_t badge_h = 28;
        lcd_fill_rect(x - 4, y - 2, badge_w, badge_h, th->success);
        font_draw_string(x + 10, y, "BEEP", th->background, th->success, &font_large);
    } else {
        font_draw_string(x, y, "OPEN", th->text_secondary, th->background, &font_large);
    }
}

static void draw_diode_indicator(uint16_t x, uint16_t y, const theme_t *th)
{
    font_draw_string(x, y, "|>|", th->ch1, th->background, &font_medium);
}

/* Layout name for info display */
static const char *layout_names[METER_LAYOUT_COUNT] = {
    "Full", "Chart", "Stats", "Fuse"
};

const char *meter_submode_name(uint8_t submode)
{
    if (submode >= METER_SUBMODE_COUNT) return "???";
    return meter_modes[submode].name;
}

/* ═══════════════════════════════════════════════════════════════════
 * Layout 0: Full (classic DMM)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_meter_full(const meter_mode_info_t *m, uint8_t mode,
                            float current_val, const char *value_str,
                            float bar_pct)
{
    const theme_t *th = theme_get();

    draw_main_reading(m, th, MAIN_READING_Y, false, value_str);

    /* Special indicators for continuity and diode modes */
    if (mode == 7) {
        int is_short = (current_val < 50.0f);
        draw_buzzer_indicator(170, SECONDARY_Y + 44, th, is_short);
    } else if (mode == 8) {
        draw_diode_indicator(170, SECONDARY_Y + 44, th);
    }

    /* Bar graph */
    draw_bar_graph(BAR_X, BAR_Y, BAR_W, BAR_H, bar_pct, th->success);

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

    /* Sub-mode index indicator */
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

/* ═══════════════════════════════════════════════════════════════════
 * Layout 1: Chart (reading + strip chart)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_meter_chart(const meter_mode_info_t *m, uint8_t mode,
                             float current_val, const char *value_str)
{
    const theme_t *th = theme_get();
    char val_buf[16];
    (void)mode;

    /* Compact main reading at top */
    draw_main_reading(m, th, METER_TOP + 2, true, value_str);

    /* Current value + unit below reading */
    font_draw_string(16, METER_TOP + 40, m->range_label,
                     th->text_secondary, th->background, &font_small);

    /* Min/Max labels on right side of reading area */
    if (meter_stats_valid) {
        fmt_float(val_buf, sizeof(val_buf), meter_min_val, 2);
        font_draw_string(16, METER_TOP + 54, "Min:",
                         th->text_secondary, th->background, &font_small);
        font_draw_string(44, METER_TOP + 54, val_buf,
                         th->ch2, th->background, &font_small);

        fmt_float(val_buf, sizeof(val_buf), meter_max_val, 2);
        font_draw_string(120, METER_TOP + 54, "Max:",
                         th->text_secondary, th->background, &font_small);
        font_draw_string(152, METER_TOP + 54, val_buf,
                         th->ch1, th->background, &font_small);
    }

    /* Draw chart area border */
    lcd_fill_rect(CHART_X - 1, CHART_Y - 1, CHART_W + 2, 1, th->grid_center);
    lcd_fill_rect(CHART_X - 1, CHART_Y + CHART_H, CHART_W + 2, 1, th->grid_center);
    lcd_fill_rect(CHART_X - 1, CHART_Y, 1, CHART_H, th->grid_center);
    lcd_fill_rect(CHART_X + CHART_W, CHART_Y, 1, CHART_H, th->grid_center);

    /* Clear chart interior */
    lcd_fill_rect(CHART_X, CHART_Y, CHART_W, CHART_H, th->background);

    /* Horizontal grid lines (4 divisions) */
    for (int i = 1; i < 4; i++) {
        uint16_t gy = CHART_Y + (CHART_H * i) / 4;
        for (int x = CHART_X; x < CHART_X + CHART_W; x += 4) {
            lcd_set_pixel(x, gy, th->grid);
        }
    }

    /* Center line (average or midpoint) */
    {
        uint16_t mid_y = CHART_Y + CHART_H / 2;
        for (int x = CHART_X; x < CHART_X + CHART_W; x += 2) {
            lcd_set_pixel(x, mid_y, th->grid_center);
        }
    }

    /* Draw chart trace */
    if (chart_count > 1) {
        float range = chart_max - chart_min;
        if (range < 0.01f) range = 0.01f;

        uint16_t prev_y = 0;
        uint16_t start_idx;

        if (chart_count >= CHART_W) {
            start_idx = chart_head;  /* Oldest sample */
        } else {
            start_idx = 0;
        }

        uint16_t num_draw = (chart_count < CHART_W) ? chart_count : CHART_W;

        for (uint16_t i = 0; i < num_draw; i++) {
            uint16_t buf_idx = (start_idx + i) % CHART_SAMPLES;
            float normalized = (chart_buf[buf_idx] - chart_min) / range;
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;

            uint16_t py = CHART_Y + CHART_H - 1 -
                          (uint16_t)(normalized * (CHART_H - 2));

            uint16_t px = CHART_X + (CHART_W - num_draw) + i;

            lcd_set_pixel(px, py, th->ch1);

            /* Connect to previous point with vertical line */
            if (i > 0 && prev_y != py) {
                uint16_t y0 = prev_y < py ? prev_y : py;
                uint16_t y1 = prev_y < py ? py : prev_y;
                for (uint16_t fy = y0 + 1; fy < y1; fy++) {
                    lcd_set_pixel(px - 1, fy, th->ch1);
                }
            }
            prev_y = py;
        }

        /* Draw current value line (horizontal dotted) */
        {
            float cur_norm = (current_val - chart_min) / range;
            if (cur_norm < 0.0f) cur_norm = 0.0f;
            if (cur_norm > 1.0f) cur_norm = 1.0f;
            uint16_t cur_y = CHART_Y + CHART_H - 1 -
                             (uint16_t)(cur_norm * (CHART_H - 2));
            for (int x = CHART_X; x < CHART_X + CHART_W; x += 6) {
                lcd_set_pixel(x, cur_y, th->highlight);
                lcd_set_pixel(x + 1, cur_y, th->highlight);
            }
        }
    }

    /* Y-axis scale labels */
    fmt_float(val_buf, sizeof(val_buf), chart_max, 1);
    font_draw_string_right(CHART_X + CHART_W, CHART_Y - 1, val_buf,
                           th->text_secondary, th->background, &font_small);
    fmt_float(val_buf, sizeof(val_buf), chart_min, 1);
    font_draw_string_right(CHART_X + CHART_W, CHART_Y + CHART_H + 2, val_buf,
                           th->text_secondary, th->background, &font_small);

    /* Sample count */
    {
        char cnt_buf[16];
        fmt_float(cnt_buf, sizeof(cnt_buf), (float)chart_count, 0);
        font_draw_string(CHART_X, CHART_Y + CHART_H + 2, cnt_buf,
                         th->text_secondary, th->background, &font_small);
        font_draw_string(CHART_X + 30, CHART_Y + CHART_H + 2, "samples",
                         th->text_secondary, th->background, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Layout 2: Stats (reading + histogram + detailed stats)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_meter_stats(const meter_mode_info_t *m, uint8_t mode,
                             float current_val, const char *value_str)
{
    const theme_t *th = theme_get();
    char val_buf[16];
    (void)mode;
    (void)current_val;

    /* Compact main reading at top */
    draw_main_reading(m, th, METER_TOP + 2, true, value_str);

    /* Statistics panel */
    uint16_t sy = METER_TOP + 42;
    uint16_t col1 = 16;
    uint16_t col2 = 170;

    font_draw_string(col1, sy, "MIN",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(col2, sy, "MAX",
                     th->text_secondary, th->background, &font_small);
    sy += 14;

    if (meter_stats_valid) {
        fmt_float(val_buf, sizeof(val_buf), meter_min_val, 3);
        font_draw_string(col1, sy, val_buf,
                         th->ch2, th->background, &font_medium);
        fmt_float(val_buf, sizeof(val_buf), meter_max_val, 3);
        font_draw_string(col2, sy, val_buf,
                         th->ch1, th->background, &font_medium);
    } else {
        font_draw_string(col1, sy, "---", th->text_secondary,
                         th->background, &font_medium);
        font_draw_string(col2, sy, "---", th->text_secondary,
                         th->background, &font_medium);
    }
    sy += 22;

    font_draw_string(col1, sy, "AVG",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(col2, sy, "P-P",
                     th->text_secondary, th->background, &font_small);
    sy += 14;

    if (meter_stats_valid && meter_avg_count > 0) {
        float avg = meter_avg_accum / (float)meter_avg_count;
        fmt_float(val_buf, sizeof(val_buf), avg, 3);
        font_draw_string(col1, sy, val_buf,
                         th->highlight, th->background, &font_medium);
        float pp = meter_max_val - meter_min_val;
        fmt_float(val_buf, sizeof(val_buf), pp, 3);
        font_draw_string(col2, sy, val_buf,
                         th->text_primary, th->background, &font_medium);
    } else {
        font_draw_string(col1, sy, "---", th->text_secondary,
                         th->background, &font_medium);
        font_draw_string(col2, sy, "---", th->text_secondary,
                         th->background, &font_medium);
    }
    sy += 22;

    /* Sample count */
    font_draw_string(col1, sy, "Samples:",
                     th->text_secondary, th->background, &font_small);
    fmt_float(val_buf, sizeof(val_buf), (float)meter_avg_count, 0);
    font_draw_string(col1 + 60, sy, val_buf,
                     th->text_primary, th->background, &font_small);

    /* Unit reminder */
    font_draw_string(col2, sy, m->unit,
                     th->text_secondary, th->background, &font_small);
    sy += 18;

    /* Histogram */
    if (hist_total > 5) {
        uint16_t hist_x = 16;
        uint16_t hist_y = sy;
        uint16_t hist_w = 288;
        uint16_t hist_h = METER_BOTTOM - sy - 22;
        uint16_t bin_w = hist_w / HIST_BINS;

        /* Find max bin for scaling */
        uint32_t max_bin = 1;
        for (int i = 0; i < HIST_BINS; i++) {
            if (hist_bins[i] > max_bin) max_bin = hist_bins[i];
        }

        /* Draw bins */
        for (int i = 0; i < HIST_BINS; i++) {
            uint16_t bx = hist_x + i * bin_w;
            if (hist_bins[i] > 0) {
                uint16_t bh = (uint16_t)((uint32_t)hist_bins[i] * hist_h / max_bin);
                if (bh < 1) bh = 1;
                uint16_t by = hist_y + hist_h - bh;
                lcd_fill_rect(bx, by, bin_w - 1, bh, th->ch1);
            }
        }

        /* Baseline */
        lcd_fill_rect(hist_x, hist_y + hist_h, hist_w, 1, th->grid_center);

        /* Scale labels */
        fmt_float(val_buf, sizeof(val_buf), hist_min_val, 1);
        font_draw_string(hist_x, hist_y + hist_h + 2, val_buf,
                         th->text_secondary, th->background, &font_small);
        fmt_float(val_buf, sizeof(val_buf), hist_max_val, 1);
        font_draw_string_right(hist_x + hist_w, hist_y + hist_h + 2, val_buf,
                               th->text_secondary, th->background, &font_small);
    } else {
        font_draw_string(16, sy + 10, "Collecting data...",
                         th->text_secondary, th->background, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Main meter screen draw — dispatches by layout
 * ═══════════════════════════════════════════════════════════════════ */

/* Toggle relative mode — captures current reading as reference */
void meter_toggle_relative(void)
{
    if (!meter_rel_enabled) {
        /* Enable: capture current reading as reference */
        uint8_t mode = meter_submode;
        if (mode >= METER_SUBMODE_COUNT) mode = 0;
        if (meter_data_valid()) {
            meter_rel_reference = meter_data_get_value();
        } else {
            meter_rel_reference = simple_atof(meter_modes[mode].demo_value);
        }
        meter_rel_enabled = true;
    } else {
        meter_rel_enabled = false;
        meter_rel_reference = 0.0f;
    }
}

/* Toggle auto-hold mode */
void meter_toggle_hold(void)
{
    meter_hold_enabled = !meter_hold_enabled;
    if (!meter_hold_enabled) {
        meter_hold_locked = false;
        hold_stable_count = 0;
    } else {
        meter_hold_locked = false;
        hold_stable_count = 0;
        hold_prev_value = 0.0f;
    }
}

void draw_meter_screen(void)
{
    const theme_t *th = theme_get();
    uint8_t mode = meter_submode;
    if (mode >= METER_SUBMODE_COUNT) mode = 0;

    const meter_mode_info_t *m = &meter_modes[mode];

    /* Request fresh meter data from FPGA via interrupt-driven TX.
     * Polled TX was failing at runtime (F counter frozen after boot). */
    fpga_send_cmd(0x00, 0x09);  /* Meter: start measurement */

    /* Use real FPGA meter data if available, otherwise fall back to demo */
    float current_val;
    const char *value_str;
    float bar_pct;

    if (meter_data_valid() &&
        meter_reading.result_class != METER_RESULT_NONE) {
        current_val = meter_reading.value;
        value_str = meter_reading.display_str;
        bar_pct = meter_reading.bar_fraction;
    } else {
        current_val = simple_atof(m->demo_value);
        value_str = m->demo_value;
        bar_pct = m->demo_bar_pct;
    }

    /* Apply relative offset if enabled */
    float display_val = current_val;
    if (meter_rel_enabled) {
        display_val = current_val - meter_rel_reference;
    }

    /* If auto-hold is locked, use the held value */
    if (meter_hold_enabled && meter_hold_locked) {
        display_val = meter_hold_value;
        if (meter_rel_enabled) {
            display_val = meter_hold_value - meter_rel_reference;
        }
    }

    meter_update_stats(current_val);

    /* Continuity visual indicator: flash the entire background green
     * when continuity is detected (resistance < 50 ohms).
     * This helps in noisy environments where the buzzer can't be heard. */
    bool continuity_flash = false;
    if (mode == 7) {
        continuity_flash = (current_val < 50.0f) ||
                           meter_reading.continuity_beep;
    }

    /* Clear content area (green flash for continuity, normal otherwise) */
    uint16_t clear_bg = continuity_flash ? th->success : th->background;
    lcd_fill_rect(0, METER_TOP, LCD_WIDTH, METER_BOTTOM - METER_TOP, clear_bg);

    /* Mode indicators (top of content area) */
    uint16_t ind_x = 4;

    /* Layout name */
    font_draw_string_right(LCD_WIDTH - 4, METER_TOP + 2,
                           layout_names[meter_layout],
                           th->text_secondary, th->background, &font_small);

    /* REL indicator */
    if (meter_rel_enabled) {
        lcd_fill_rect(ind_x, METER_TOP + 1, 28, 13, th->highlight);
        font_draw_string(ind_x + 2, METER_TOP + 2, "REL",
                         th->background, th->highlight, &font_small);
        ind_x += 32;
    }

    /* HOLD indicator */
    if (meter_hold_enabled) {
        uint16_t hold_bg = meter_hold_locked ? th->warning : th->grid_center;
        lcd_fill_rect(ind_x, METER_TOP + 1, 36, 13, hold_bg);
        font_draw_string(ind_x + 2, METER_TOP + 2,
                         meter_hold_locked ? "HOLD" : "hold",
                         th->background, hold_bg, &font_small);
        ind_x += 40;
    }

    /* Use display_val for REL/HOLD-adjusted readings */
    if (meter_rel_enabled || (meter_hold_enabled && meter_hold_locked)) {
        /* Format the adjusted value for display */
        static char adjusted_str[16];
        int decimals = 2;  /* Default decimal places for adjusted display */
        fmt_float(adjusted_str, sizeof(adjusted_str), display_val, decimals);
        value_str = adjusted_str;
        /* Update bar for adjusted value */
        float abs_dv = display_val < 0 ? -display_val : display_val;
        bar_pct = abs_dv / m->bar_max;
        if (bar_pct > 1.0f) bar_pct = 1.0f;
    }

    switch (meter_layout) {
    case METER_LAYOUT_CHART:
        draw_meter_chart(m, mode, current_val, value_str);
        break;
    case METER_LAYOUT_STATS:
        draw_meter_stats(m, mode, current_val, value_str);
        break;
    case METER_LAYOUT_FUSE:
        /* Fuse tester uses the mV reading as voltage drop.
         * In demo mode, simulate a 1.5 mV drop (typical parasitic draw). */
        draw_fuse_screen(current_val);
        break;
    default:
        draw_meter_full(m, mode, current_val, value_str, bar_pct);
        break;
    }

    /* ── DEBUG OVERLAY — FPGA meter pipeline status ── */
    {
        char dbg[40];
        uint16_t dy = LCD_HEIGHT - 48;
        uint16_t dbg_bg = 0x0000;  /* black */
        lcd_fill_rect(0, dy, LCD_WIDTH, 30, dbg_bg);

        /* Show: D:data E:echo T:tx B:rxbytes | hex */
        int i = 0;

        /* Data frame count */
        dbg[i++] = 'D';
        { uint16_t fc = fpga.frame_count; char tmp[6]; int t = 0;
          if (fc == 0) tmp[t++] = '0';
          else while (fc > 0 && t < 5) { tmp[t++] = '0' + (fc % 10); fc /= 10; }
          for (int j = t - 1; j >= 0; j--) dbg[i++] = tmp[j]; }
        dbg[i++] = ' ';

        /* Echo frame count */
        dbg[i++] = 'E';
        { uint16_t fc = fpga.echo_count; char tmp[6]; int t = 0;
          if (fc == 0) tmp[t++] = '0';
          else while (fc > 0 && t < 5) { tmp[t++] = '0' + (fc % 10); fc /= 10; }
          for (int j = t - 1; j >= 0; j--) dbg[i++] = tmp[j]; }
        dbg[i++] = ' ';

        /* TX commands sent */
        dbg[i++] = 'T';
        { uint16_t fc = fpga.tx_count; char tmp[6]; int t = 0;
          if (fc == 0) tmp[t++] = '0';
          else while (fc > 0 && t < 5) { tmp[t++] = '0' + (fc % 10); fc /= 10; }
          for (int j = t - 1; j >= 0; j--) dbg[i++] = tmp[j]; }
        dbg[i++] = ' ';

        /* RX bytes total */
        dbg[i++] = 'B';
        { uint16_t fc = fpga.rx_byte_count; char tmp[6]; int t = 0;
          if (fc == 0) tmp[t++] = '0';
          else while (fc > 0 && t < 5) { tmp[t++] = '0' + (fc % 10); fc /= 10; }
          for (int j = t - 1; j >= 0; j--) dbg[i++] = tmp[j]; }

        /* Raw rx_frame bytes [2..7] as hex (includes status byte) */
        for (int b = 2; b <= 7 && i < 38; b++) {
            uint8_t v = fpga.rx_frame[b];
            const char *hex = "0123456789ABCDEF";
            dbg[i++] = hex[(v >> 4) & 0xF];
            dbg[i++] = hex[v & 0xF];
            dbg[i++] = ' ';
        }
        dbg[i] = '\0';

        font_draw_string(4, dy + 2, dbg, 0x07E0, dbg_bg, &font_small);

        /* Second debug line: nibble pairs → digits, probe_type, raw_bcd */
        {
            const char *hex = "0123456789ABCDEF";
            int j = 0;
            char db2[48];

            /* Nibble pairs (pre-lookup) */
            db2[j++] = 'N';
            for (int k = 0; k < 4; k++) {
                uint8_t n = meter_reading.dbg_nibbles[k];
                db2[j++] = hex[(n >> 4) & 0xF];
                db2[j++] = hex[n & 0xF];
                db2[j++] = ' ';
            }

            /* Decoded digits (post-lookup) */
            db2[j++] = 'D';
            for (int k = 0; k < 4; k++) {
                uint8_t d = meter_reading.dbg_raw_digits[k];
                db2[j++] = hex[(d >> 4) & 0xF];
                db2[j++] = hex[d & 0xF];
                db2[j++] = ' ';
            }

            /* Probe type and range */
            db2[j++] = 'P';
            db2[j++] = '0' + meter_reading.probe_type;
            db2[j++] = ' ';
            db2[j++] = 'R';
            db2[j++] = '0' + meter_reading.range_indicator;
            db2[j++] = ' ';

            /* Raw BCD */
            db2[j++] = '=';
            { int bcd = meter_reading.raw_bcd; char tmp[6]; int t = 0;
              if (bcd == 0) tmp[t++] = '0';
              else { int v = bcd; while (v > 0 && t < 5) { tmp[t++] = '0' + (v % 10); v /= 10; } }
              for (int q = t - 1; q >= 0; q--) db2[j++] = tmp[q]; }

            db2[j] = '\0';
            font_draw_string(4, dy + 16, db2, 0xFFE0, dbg_bg, &font_small);
        }
    }
}
