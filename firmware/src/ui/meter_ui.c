/*
 * OpenScope 2C53T - Multimeter UI
 *
 * 11 local sub-modes mapped onto the recovered stock DMM selector families.
 * Current ranges and the capacitance/temperature split are local UI policy
 * over shared stock slots until additional stock/runtime evidence proves a
 * narrower hardware selector. uA is intentionally absent from this UI/submode
 * surface: no recovered stock selector, formatter path, or safe live trace
 * proves a microamp frontend/range yet.
 *   0: DC Voltage      1: AC Voltage      2: DC mA
 *   3: DC A            4: AC mA           5: AC A
 *   6: Resistance      7: Continuity      8: Diode
 *   9: Capacitance     10: Temperature
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
#include "meter_autoselect.h"
#include "softkey.h"
#include "fuse_table.h"
#include "fuse_model.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include "fpga.h"
#include "meter_voltage_wave.h"
#include "shared_mem.h"
#include "at32f403a_407.h"
#include <string.h>

/* Layout constants */
#define METER_TOP       18          /* Below status bar */
#define METER_BOTTOM    SOFTKEY_BAR_Y      /* above the softkey bar */
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
#define CHART_H         108         /* labels below end at y=202, clear of the softkey bar (210) */
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
    { "DC Voltage",  "V",    "DC", "Auto DCV",    "13.82",  0.69f,  200.0f,  "Auto",  ""  },
    /* 1: AC Voltage */
    { "AC Voltage",  "V",    "AC", "Auto ACV",    "120.3",  0.60f,  600.0f,  "Auto",  "~" },
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
    /* 10: Temperature */
    { "Temperature", "C",    "",   "Thermo",      "24.8",   0.25f,  100.0f,  "100C",   ""  },
};

/* ═══════════════════════════════════════════════════════════════════
 * Min / Max / Avg tracking state
 * ═══════════════════════════════════════════════════════════════════ */

static float meter_min_val;
static float meter_max_val;
static float meter_avg_accum;
static uint32_t meter_avg_count;
static uint8_t meter_stats_valid;

/* FPGA frame debug overlay toggle. Default off in release builds.
 * Flip to true from the debugger, a future settings entry, or a
 * button handler to re-enable the three-line RX frame / BCD / f6
 * history strip at the bottom of the meter screen. Used during
 * the low-Ω band-interpretation reverse engineering session
 * (2026-04-04) — see meter_data.c for the band override story. */
static bool meter_debug_overlay = false;

volatile uint32_t meter_screen_draw_count;
volatile uint32_t meter_screen_full_clear_count;
volatile uint32_t meter_screen_partial_clear_count;
volatile uint32_t meter_screen_last_draw_us;
volatile uint32_t meter_screen_max_draw_us;
volatile uint32_t meter_screen_over_budget_count;
volatile uint32_t meter_screen_last_reading_display_update;
volatile uint8_t  meter_screen_last_full_clear;
volatile uint8_t  meter_screen_last_live;
volatile uint8_t  meter_screen_last_continuity_flash;

static bool       meter_screen_retained_valid;
static uint8_t    meter_screen_last_mode;
static uint8_t    meter_screen_last_layout;
static theme_id_t meter_screen_last_theme;
static bool       meter_screen_last_rel_enabled;
static bool       meter_screen_last_hold_enabled;
static bool       meter_screen_last_hold_locked;
static bool       meter_screen_last_debug_overlay;

#define METER_LCD_FRAME_BUDGET_US 16667U

static uint32_t meter_draw_cycles_now(void)
{
#ifdef EMULATOR_BUILD
    return 0;
#else
    static bool dwt_ready;

    if (!dwt_ready) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_ready = true;
    }
    return DWT->CYCCNT;
#endif
}

static void meter_record_draw_time(uint32_t start_cycles, bool full_clear)
{
#ifdef EMULATOR_BUILD
    (void)start_cycles;
    (void)full_clear;
#else
    uint32_t elapsed_cycles = DWT->CYCCNT - start_cycles;
    uint32_t cycles_per_us = system_core_clock / 1000000U;
    if (cycles_per_us == 0U) cycles_per_us = 1U;

    uint32_t elapsed_us = elapsed_cycles / cycles_per_us;
    meter_screen_last_draw_us = elapsed_us;
    if (elapsed_us > meter_screen_max_draw_us) {
        meter_screen_max_draw_us = elapsed_us;
    }
    if (elapsed_us > METER_LCD_FRAME_BUDGET_US) {
        meter_screen_over_budget_count++;
    }
    meter_screen_last_full_clear = full_clear ? 1U : 0U;
#endif
}

void meter_toggle_debug_overlay(void)
{
    meter_debug_overlay = !meter_debug_overlay;
}

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




static bool live_reading_for_mode(const meter_reading_t *reading, uint8_t mode)
{
    return reading->valid &&
           reading->submode == mode &&
           reading->result_class != METER_RESULT_NONE;
}

/* Live unit string: prefer the suffix decoded by meter_data from the
 * current frame only when that frame belongs to the displayed submode.
 * Otherwise stale voltage readings can be relabeled as amps/ohms/Hz while
 * the meter frontend is still settling after a mode change. */
static const char *live_unit(const meter_mode_info_t *m,
                             const meter_reading_t *reading,
                             uint8_t mode)
{
    if (live_reading_for_mode(reading, mode) &&
        reading->unit_suffix != NULL &&
        reading->unit_suffix[0] != '\0') {
        return reading->unit_suffix;
    }
    return m->unit;
}

static bool live_voltage_auto_ac(const meter_reading_t *reading, uint8_t mode)
{
    return mode == 0 &&
           live_reading_for_mode(reading, mode) &&
           reading->aux_freq_hz >= 1.0f;
}

static const char *live_ac_dc(const meter_mode_info_t *m,
                              const meter_reading_t *reading,
                              uint8_t mode)
{
    return live_voltage_auto_ac(reading, mode) ? "AC" : m->ac_dc;
}

static const char *live_symbol(const meter_mode_info_t *m,
                               const meter_reading_t *reading,
                               uint8_t mode)
{
    return live_voltage_auto_ac(reading, mode) ? "~" : m->symbol;
}

static const char *live_range_label(const meter_mode_info_t *m,
                                    const meter_reading_t *reading,
                                    uint8_t mode)
{
    return live_voltage_auto_ac(reading, mode) ? "Auto ACV" : m->range_label;
}

/* Draw the main reading (shared by all layouts)
 * value_str: the string to display (real or demo) */
static void draw_main_reading(const meter_mode_info_t *m, const theme_t *th,
                              const meter_reading_t *reading,
                              uint8_t mode, uint16_t y, bool compact,
                              const char *value_str)
{
    const char *unit_str = live_unit(m, reading, mode);
    /* Main reading
     *
     * font_xlarge only has the numeric subset 0-9, '.', '-', '+', ':', ' '
     * plus unit letters V/A/k/m/M/H/z/W/F/O. Special state strings like
     * "OL", "CONT", "ERR", "---" contain letters (L, N, T, R, -) that
     * aren't in the xlarge font — they silently drop and the user sees
     * a truncated display (for example "OL" becoming only "O").
     * Fall back to font_large for non-numeric state strings. */
    bool is_numeric = true;
    for (const char *p = value_str; *p; p++) {
        char c = *p;
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == ' ')) {
            is_numeric = false;
            break;
        }
    }
    const font_t *main_font =
        compact        ? &font_large :
        is_numeric     ? &font_xlarge :
                         &font_large;
    if (!compact) {
        lcd_fill_rect(0, y, 244, font_xlarge.height, th->background);
        /* Mode indicator arrows (show L/R navigation) */
        font_draw_string(4, y, "<",
                         th->text_secondary, th->background, &font_small);
        font_draw_string_right(LCD_WIDTH - 4, y, ">",
                               th->text_secondary, th->background, &font_small);
    }
    font_draw_string_right(240, y, value_str,
                           th->text_primary, th->background, main_font);

    /* Unit label — use the live suffix decoded by meter_data from the
     * current USART frame. Falls back to the static mode table string
     * before any data has arrived. */
    uint16_t unit_y = compact ? y + 2 : y + 4;
    if (!compact && (mode == 0 || mode == 1)) {
        lcd_fill_rect(UNIT_X, unit_y, LCD_WIDTH - UNIT_X, 44, th->background);
    }
    font_draw_string(UNIT_X, unit_y, unit_str,
                     th->ch1, th->background,
                     compact ? &font_medium : &font_large);

    /* AC/DC indicator */
    const char *ac_dc = live_ac_dc(m, reading, mode);
    const char *symbol = live_symbol(m, reading, mode);
    if (ac_dc[0] != '\0') {
        uint16_t ac_y = compact ? y + 18 : y + 30;
        font_draw_string(UNIT_X, ac_y, ac_dc,
                         th->text_secondary, th->background, &font_small);
        if (symbol[0] == '~') {
            font_draw_string(UNIT_X + 20, ac_y, "~",
                             th->ch1, th->background, &font_small);
        }
    }
}


/* Continuity beeps below this many ohms (2026-10-06). Shorted leads read
 * ~0.14 Ohm as a plain resistance, so the chip's own continuity flag alone
 * never fired. Common DMM practice is 10-50 Ohm; the softkey cycles these. */
static const float cont_thresholds[] = { 10.0f, 30.0f, 50.0f, 100.0f };
#define CONT_THRESHOLD_N (sizeof cont_thresholds / sizeof cont_thresholds[0])
static uint8_t cont_threshold_idx = 1;      /* 30 Ohm */

float meter_continuity_threshold(void)
{
    return cont_thresholds[cont_threshold_idx < CONT_THRESHOLD_N ? cont_threshold_idx : 1];
}

static bool meter_continuity_short_active(const meter_reading_t *reading,
                                          bool has_live_reading,
                                          float current_val)
{
    static uint8_t continuity_latch_frames;

    if (!has_live_reading) {
        continuity_latch_frames = 0;
        return false;
    }

    if (meter_continuity_is_short(reading, meter_continuity_threshold())) {
        continuity_latch_frames = 8;
        return true;
    }

    if (reading->result_class == METER_RESULT_OVERLOAD ||
        reading->result_class == METER_RESULT_BLANK ||
        reading->result_class == METER_RESULT_NONE) {
        continuity_latch_frames = 0;
        return false;
    }

    /* The live meter sometimes emits one continuity marker followed by a few
     * invalid/ERR frames while settling. Let those coast an already-confirmed
     * short briefly, but never let ERR start or hold the green state forever. */
    if (reading->result_class == METER_RESULT_INVALID &&
        continuity_latch_frames > 0) {
        continuity_latch_frames--;
        return true;
    }

    continuity_latch_frames = 0;
    (void)current_val;
    return false;
}


static void meter_clear_dynamic_areas(uint8_t layout, uint8_t mode,
                                      uint16_t clear_bg)
{
    /* Normal meter updates used to blank y=18..221 in one large write before
     * repainting the value. On the live ST7789 this visible blanking is the
     * flicker. Keep the static frame retained and only erase regions whose
     * contents change on ordinary sample updates. */
    bool full_voltage = (layout == METER_LAYOUT_FULL) && (mode == 0 || mode == 1);
    if (full_voltage) {
        lcd_fill_rect(0, METER_TOP, LCD_WIDTH, 14, clear_bg);
    } else {
        lcd_fill_rect(0, METER_TOP, LCD_WIDTH, 66, clear_bg);
    }

    switch (layout) {
    case METER_LAYOUT_CHART:
        lcd_fill_rect(CHART_X - 1, CHART_Y - 14,
                      CHART_W + 2, METER_BOTTOM - (CHART_Y - 14),
                      clear_bg);
        break;
    case METER_LAYOUT_STATS:
        lcd_fill_rect(0, METER_TOP + 42,
                      LCD_WIDTH, METER_BOTTOM - (METER_TOP + 42),
                      clear_bg);
        break;
    case METER_LAYOUT_FUSE:
        break;
    default:
        if (mode == 0 || mode == 1) {
            lcd_fill_rect(BAR_X, BAR_Y - 2, BAR_W, BAR_H + 6, clear_bg);
        } else {
            lcd_fill_rect(BAR_X, BAR_Y - 2, BAR_W, BAR_H + 18, clear_bg);
            lcd_fill_rect(0, SECONDARY_Y - 4,
                          LCD_WIDTH, 82, clear_bg);
        }
        break;
    }
}


const char *meter_submode_name(uint8_t submode)
{
    if (submode >= METER_SUBMODE_COUNT) return "???";
    return meter_modes[submode].name;
}

void meter_screen_invalidate(void)
{
    meter_screen_retained_valid = false;
}

bool meter_screen_needs_periodic_redraw(void)
{
    return meter_debug_overlay || (meter_hold_enabled && !meter_hold_locked);
}

/* ═══════════════════════════════════════════════════════════════════
 * Layout 0: Full (classic DMM)
 * ═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════
 * Layout 1: Chart (reading + strip chart)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_meter_chart(const meter_mode_info_t *m, uint8_t mode,
                             const meter_reading_t *reading,
                             float current_val, const char *value_str)
{
    const theme_t *th = theme_get();
    char val_buf[16];
    (void)mode;

    /* Compact main reading at top */
    draw_main_reading(m, th, reading, mode, METER_TOP + 2, true, value_str);

    /* Current value + unit below reading */
    font_draw_string(16, METER_TOP + 40, live_range_label(m, reading, mode),
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
                             const meter_reading_t *reading,
                             float current_val, const char *value_str)
{
    const theme_t *th = theme_get();
    char val_buf[16];
    (void)mode;
    (void)current_val;

    /* Compact main reading at top */
    draw_main_reading(m, th, reading, mode, METER_TOP + 2, true, value_str);

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
 * Softkey UI: big reading, limits, instant hold
 * (docs/specs/platform/softkey-ui.md, P1 -- 2026-10-06)
 *
 * Softkeys under the screen: MOVE = Function, SELECT = Min/Max reset (Low in
 * Limits, Show in Fuse), TRIGGER = Relative (High in Limits), PRM = View.
 * OK = Hold. The Big and Limits views paint every pixel of their area in
 * place each draw -- fixed boxes, opaque text, gaps refilled with the same
 * colour -- so a changing reading never blanks (the once-a-second blink).
 *
 * State the old meter never had (held text, limits, pass/fail counts) lives
 * in one heap block taken on first use: coldtrace's static RAM is full.
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    char        held[16];       /* reading text at the HOLD press */
    const char *held_unit;
    float       lo, hi;         /* limits, in the unit they were set in */
    const char *lim_unit;
    uint8_t     lim_mode;       /* function the limits belong to */
    int8_t      lim_dec;        /* decimals of the reading they came from */
    bool        lim_set;
    uint8_t     lim_active;     /* 0 = Low, 1 = High: what the arrows move */
    uint16_t    pass, fail;
    float       last_fail;
    bool        has_fail;
    uint8_t     verdict;        /* 0 none, 1 pass, 2 fail */
    uint32_t    counted_update; /* display_update_count already judged */
} meter_ext_t;

static meter_ext_t *mx;

static meter_ext_t *meter_ext(void)
{
    if (!mx) {
        mx = (meter_ext_t *)pvPortMalloc(sizeof(*mx));
        if (mx) memset(mx, 0, sizeof(*mx));
    }
    return mx;
}

static const char *const meter_short_names[METER_SUBMODE_COUNT] = {
    "DC V", "AC V", "DC mA", "DC A", "AC mA", "AC A",
    "Ohms", "Contin.", "Diode", "Cap.", "Temp",
};

static const char *const meter_view_names[METER_LAYOUT_COUNT] = {
    "Big", "Graph", "Stats", "Fuse", "Limits",
};
static const uint8_t meter_view_order[METER_LAYOUT_COUNT] = {
    METER_LAYOUT_BIG, METER_LAYOUT_CHART, METER_LAYOUT_STATS,
    METER_LAYOUT_LIMITS, METER_LAYOUT_FUSE,
};

static int str_decimals(const char *s)
{
    const char *d = strchr(s, '.');
    if (!d) return 0;
    int n = 0;
    for (d++; *d >= '0' && *d <= '9'; d++) n++;
    return n;
}

static bool str_is_number(const char *s)
{
    bool digit = false;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') digit = true;
        else if (*s != '.' && *s != '-' && *s != ' ' && *s != '+') return false;
    }
    return digit;
}

static float pow10_neg(int dec)
{
    float f = 1.0f;
    while (dec-- > 0) f *= 0.1f;
    return f;
}

/* Unit text in font_unit's glyphs: "kOhm" -> "k@", "uF" -> "`F", "C" -> "^C".
 * False if a character has no glyph (caller falls back to a text font). */
static bool unit_to_glyphs(const char *u, char *out, size_t n)
{
    size_t j = 0;
    if (strcmp(u, "C") == 0) u = "^C";
    for (size_t i = 0; u[i] && j + 1 < n; ) {
        if (strncmp(&u[i], "Ohm", 3) == 0) { out[j++] = '@'; i += 3; continue; }
        out[j++] = (u[i] == 'u') ? '`' : u[i];
        i++;
    }
    out[j] = '\0';
    return font_has_glyphs(out, &font_unit);
}

/* ── Hold (OK) and function stepping ──────────────────────────────── */

void meter_hold_release(void)
{
    meter_hold_enabled = false;
    meter_hold_locked = false;
    hold_stable_count = 0;
    if (mx) mx->held[0] = '\0';
}

bool meter_hold_is_on(void)
{
    return meter_hold_enabled && meter_hold_locked;
}

/* OK: freeze the reading on screen now, or go back to live. (Until
 * 2026-10-06 hold was an auto-hold on TRIGGER that waited for 5 stable
 * readings and showed a grey "hold" meanwhile -- undiscoverable, and its
 * held value was re-printed with 2 decimals, F42.) */
void meter_hold_toggle_now(void)
{
    if (meter_hold_enabled) {
        meter_hold_release();
        scope_show_popup("Live");
        return;
    }
    uint8_t mode = meter_submode < METER_SUBMODE_COUNT ? meter_submode : 0;
    meter_ext_t *e = meter_ext();
    meter_reading_t r;
    if (!e) { scope_show_popup("HOLD: no memory"); return; }
    if (!meter_data_snapshot(&r) || !live_reading_for_mode(&r, mode)) {
        scope_show_popup("HOLD: no reading");
        return;
    }
    strncpy(e->held, r.display_str, sizeof(e->held) - 1);
    e->held[sizeof(e->held) - 1] = '\0';
    e->held_unit = live_unit(&meter_modes[mode], &r, mode);
    meter_hold_value = r.value;
    meter_hold_enabled = true;
    meter_hold_locked = true;
    scope_show_popup("HOLD");
}

static void meter_function_set(uint8_t m)
{
    meter_submode = m;
    meter_reset_minmaxavg();
    meter_hold_release();
    if (meter_rel_enabled) meter_toggle_relative();     /* a reference in another function means nothing */
    fpga_request_meter_mode(meter_submode);             /* in the background: the UI shows dashes meanwhile */
}

void meter_function_step(int8_t dir)
{
    uint8_t m = meter_submode < METER_SUBMODE_COUNT ? meter_submode : 0;
    if (dir > 0) m = (uint8_t)((m + 1) % METER_SUBMODE_COUNT);
    else         m = (m == 0) ? (uint8_t)(METER_SUBMODE_COUNT - 1) : (uint8_t)(m - 1);
    meter_function_set(m);
}

/* ── Limits (pass/fail) ───────────────────────────────────────────── */

static bool limits_init_from_reading(meter_ext_t *e)
{
    uint8_t mode = meter_submode < METER_SUBMODE_COUNT ? meter_submode : 0;
    meter_reading_t r;
    if (!meter_data_snapshot(&r) || !live_reading_for_mode(&r, mode) ||
        !str_is_number(r.display_str))
        return false;
    int dec = str_decimals(r.display_str);
    float v = r.value;
    float a = (v < 0 ? -v : v) * 0.05f;
    float q = pow10_neg(dec);
    if (a < 10.0f * q) a = 10.0f * q;
    e->lo = v - a;
    e->hi = v + a;
    e->lim_unit = live_unit(&meter_modes[mode], &r, mode);
    e->lim_mode = mode;
    e->lim_dec = (int8_t)dec;
    e->lim_set = true;
    e->pass = e->fail = 0;
    e->has_fail = false;
    e->verdict = 0;
    return true;
}

static bool limits_valid_now(const meter_ext_t *e)
{
    return e && e->lim_set && e->lim_mode == meter_submode;
}

/* UP/DOWN in the Limits view: move the active limit by one count of the
 * digit before last of the reading it came from. */
void meter_limits_nudge(int8_t dir)
{
    meter_ext_t *e = meter_ext();
    if (!limits_valid_now(e)) {
        if (!e || !limits_init_from_reading(e)) { scope_show_popup("Limits: no reading"); return; }
    }
    float step = pow10_neg(e->lim_dec) * 10.0f;
    if (e->lim_active == 0) {
        e->lo += dir * step;
        if (e->lo > e->hi) e->lo = e->hi;
    } else {
        e->hi += dir * step;
        if (e->hi < e->lo) e->hi = e->lo;
    }
    e->pass = e->fail = 0;
    e->has_fail = false;
    e->verdict = 0;
}

static void limit_press(uint8_t which)
{
    meter_ext_t *e = meter_ext();
    char b[24], v[12];
    if (!e) { scope_show_popup("Limits: no memory"); return; }
    if (!limits_valid_now(e)) {
        if (!limits_init_from_reading(e)) { scope_show_popup("Limits: no reading"); return; }
        e->lim_active = which;
        scope_show_popup("Limits set: reading +/-5%");
        return;
    }
    if (e->lim_active != which) {
        e->lim_active = which;
        snprintf(b, sizeof b, "Arrows: %s", which ? "High" : "Low");
        scope_show_popup(b);
        return;
    }
    /* Second press on the active limit: snap it to the reading. */
    uint8_t mode = meter_submode < METER_SUBMODE_COUNT ? meter_submode : 0;
    meter_reading_t r;
    if (!meter_data_snapshot(&r) || !live_reading_for_mode(&r, mode) ||
        !str_is_number(r.display_str)) {
        scope_show_popup("Limits: no reading");
        return;
    }
    if (which) { e->hi = r.value; if (e->lo > e->hi) e->lo = e->hi; }
    else       { e->lo = r.value; if (e->hi < e->lo) e->hi = e->lo; }
    e->pass = e->fail = 0;
    e->has_fail = false;
    fmt_float(v, sizeof v, r.value, e->lim_dec);
    snprintf(b, sizeof b, "%s = %s", which ? "High" : "Low", v);
    scope_show_popup(b);
}

/* Judge each new reading once while the Limits view is up. */
static void limits_judge(const meter_reading_t *r, bool live, const char *unit)
{
    meter_ext_t *e = mx;
    if (!limits_valid_now(e)) return;
    if (!live || meter_hold_is_on() || !str_is_number(r->display_str) ||
        unit != e->lim_unit) {
        e->verdict = 0;
        return;
    }
    if (r->display_update_count == e->counted_update) return;
    e->counted_update = r->display_update_count;
    if (r->value >= e->lo && r->value <= e->hi) {
        e->verdict = 1;
        if (e->pass < 0xFFFF) e->pass++;
    } else {
        e->verdict = 2;
        if (e->fail < 0xFFFF) e->fail++;
        e->last_fail = r->value;
        e->has_fail = true;
    }
}

/* ── Drawing helpers ──────────────────────────────────────────────── */

#define BIG_BADGE_Y   (METER_TOP + 3)
#define BIG_READ_Y    (METER_TOP + 22)
#define BIG_UNIT_Y    (BIG_READ_Y + 76)
#define BIG_SUB_Y     (BIG_UNIT_Y + 48)
#define BIG_RIGHT     308

static uint16_t draw_badge(uint16_t x, uint16_t y, const char *t,
                           uint16_t fg, uint16_t fill, uint16_t area_bg)
{
    uint16_t w = font_string_width(t, &font_small) + 8;
    font_draw_string_box(x, y, w, t, fg, fill, &font_small, FONT_ALIGN_CENTER);
    lcd_fill_rect(x + w, y, 4, font_small.height, area_bg);
    return w + 4;
}

static void draw_badge_row(uint16_t y, uint16_t bg, const theme_t *th)
{
    uint16_t x = 4;
    lcd_fill_rect(0, y, 4, font_small.height, bg);
    if (meter_hold_is_on())
        x += draw_badge(x, y, "HOLD", th->background, th->warning, bg);
    if (meter_rel_enabled)
        x += draw_badge(x, y, "REL", th->background, th->highlight, bg);
    if (meter_autoselect_is_running())
        x += draw_badge(x, y, "AUTO", th->background, th->highlight, bg);
    if (x < LCD_WIDTH) lcd_fill_rect(x, y, LCD_WIDTH - x, font_small.height, bg);
}

/* Text in a band of height `band`, the font centred vertically in it;
 * every pixel of the band written once. */
static void draw_band_text(uint16_t x, uint16_t y, uint16_t w, uint16_t band,
                           const char *t, uint16_t fg, uint16_t bg,
                           const font_t *f, uint8_t align)
{
    uint16_t top = (band > f->height) ? (uint16_t)((band - f->height) / 2) : 0;
    if (top) lcd_fill_rect(x, y, w, top, bg);
    font_draw_string_box(x, y + top, w, t, fg, bg, f, align);
    uint16_t used = top + f->height;
    if (band > used) lcd_fill_rect(x, y + used, w, band - used, bg);
}

static void draw_unit_line(uint16_t x, uint16_t y, uint16_t w, const char *unit,
                           const char *acdc, uint16_t fg, uint16_t bg,
                           uint8_t align)
{
    char g[16], t[20];
    if (unit_to_glyphs(unit, g, sizeof g)) {
        snprintf(t, sizeof t, acdc[0] ? "%s %s" : "%s", g, acdc);
        draw_band_text(x, y, w, font_unit.height, t, fg, bg, &font_unit, align);
    } else {
        snprintf(t, sizeof t, acdc[0] ? "%s %s" : "%s", unit, acdc);
        draw_band_text(x, y, w, font_unit.height, t, fg, bg, &font_large, align);
    }
}

/* ── Big view ─────────────────────────────────────────────────────── */

static void draw_meter_big(const meter_mode_info_t *m, uint8_t mode,
                           const meter_reading_t *r, bool live,
                           const char *value_str, uint16_t bg)
{
    const theme_t *th = theme_get();
    uint16_t fg = (bg == th->background) ? th->text_primary : th->background;
    const char *unit = (meter_hold_is_on() && mx && mx->held_unit)
                       ? mx->held_unit : live_unit(m, r, mode);
    char sub[48];

    lcd_fill_rect(0, METER_TOP, LCD_WIDTH, BIG_BADGE_Y - METER_TOP, bg);
    draw_badge_row(BIG_BADGE_Y, bg, th);
    lcd_fill_rect(0, BIG_BADGE_Y + font_small.height, LCD_WIDTH,
                  BIG_READ_Y - (BIG_BADGE_Y + font_small.height), bg);

    if (font_has_glyphs(value_str, &font_huge)) {
        font_draw_string_box(0, BIG_READ_Y, BIG_RIGHT, value_str, fg, bg,
                             &font_huge, FONT_ALIGN_RIGHT);
        lcd_fill_rect(BIG_RIGHT, BIG_READ_Y, LCD_WIDTH - BIG_RIGHT, font_huge.height, bg);
    } else {
        draw_band_text(0, BIG_READ_Y, LCD_WIDTH, font_huge.height, value_str,
                       fg, bg, &font_large, FONT_ALIGN_CENTER);
    }
    lcd_fill_rect(0, BIG_READ_Y + font_huge.height, LCD_WIDTH,
                  BIG_UNIT_Y - (BIG_READ_Y + font_huge.height), bg);

    lcd_fill_rect(0, BIG_UNIT_Y, 4, font_unit.height, bg);
    draw_unit_line(4, BIG_UNIT_Y, BIG_RIGHT - 4, unit, live_ac_dc(m, r, mode),
                   (bg == th->background) ? th->text_secondary : th->background,
                   bg, FONT_ALIGN_RIGHT);
    lcd_fill_rect(BIG_RIGHT, BIG_UNIT_Y, LCD_WIDTH - BIG_RIGHT, font_unit.height, bg);
    lcd_fill_rect(0, BIG_UNIT_Y + font_unit.height, LCD_WIDTH,
                  BIG_SUB_Y - (BIG_UNIT_Y + font_unit.height), bg);

    /* One quiet line: relative reference, continuity verdict, or min/max. */
    sub[0] = '\0';
    int dec = str_decimals(live ? r->display_str : value_str);
    if (meter_rel_enabled) {
        char v[16];
        fmt_float(v, sizeof v, meter_rel_reference, dec);
        snprintf(sub, sizeof sub, "Relative to %s %s", v, unit);
    } else if (mode == 7 && live) {
        snprintf(sub, sizeof sub, "%s  (beeps below %d Ohm)",
                 meter_continuity_short_active(r, live, r->value) ? "SHORT" : "OPEN",
                 (int)meter_continuity_threshold());
    } else if (meter_stats_valid) {
        char a[16], b[16];
        fmt_float(a, sizeof a, meter_min_val, dec);
        fmt_float(b, sizeof b, meter_max_val, dec);
        snprintf(sub, sizeof sub, "Min %s    Max %s", a, b);
    }
    font_draw_string_box(0, BIG_SUB_Y, LCD_WIDTH, sub,
                         (bg == th->background) ? th->text_secondary : th->background,
                         bg, &font_medium, FONT_ALIGN_CENTER);
    lcd_fill_rect(0, BIG_SUB_Y + font_medium.height, LCD_WIDTH,
                  METER_BOTTOM - (BIG_SUB_Y + font_medium.height), bg);
}

/* ── Limits view ──────────────────────────────────────────────────── */

#define LIM_READ_Y   (METER_TOP + 20)
#define LIM_BOX_X    216
#define LIM_BOX_W    100
#define LIM_BOX_H    46
#define LIM_BAR_Y    (LIM_READ_Y + 58)
#define LIM_BAR_H    14
#define LIM_BAR_X    16
#define LIM_BAR_W    288
#define LIM_LBL_Y    (LIM_BAR_Y + LIM_BAR_H + 4)
#define LIM_CNT_Y    (LIM_LBL_Y + 22)
#define LIM_LAST_Y   (LIM_CNT_Y + 24)

static void draw_meter_limits(const meter_mode_info_t *m, uint8_t mode,
                              const meter_reading_t *r, bool live,
                              const char *value_str)
{
    const theme_t *th = theme_get();
    uint16_t bg = th->background;
    const char *unit = live_unit(m, r, mode);
    meter_ext_t *e = mx;
    bool ok = limits_valid_now(e);
    char t[40], a[16], b[16];

    limits_judge(r, live, unit);

    lcd_fill_rect(0, METER_TOP, LCD_WIDTH, BIG_BADGE_Y - METER_TOP, bg);
    draw_badge_row(BIG_BADGE_Y, bg, th);
    lcd_fill_rect(0, BIG_BADGE_Y + font_small.height, LCD_WIDTH,
                  LIM_READ_Y - (BIG_BADGE_Y + font_small.height), bg);

    /* Reading + unit, left of the verdict box */
    const font_t *rf = font_has_glyphs(value_str, &font_xlarge) ? &font_xlarge : &font_large;
    draw_band_text(0, LIM_READ_Y, 170, LIM_BOX_H, value_str, th->text_primary, bg, rf, FONT_ALIGN_RIGHT);
    draw_band_text(170, LIM_READ_Y, 46, LIM_BOX_H, unit, th->text_secondary, bg, &font_medium, FONT_ALIGN_CENTER);

    /* Verdict box */
    uint8_t vd = ok ? e->verdict : 0;
    uint16_t vbg = vd == 1 ? th->success : vd == 2 ? th->warning : th->grid_center;
    const char *vt = vd == 1 ? "PASS" : vd == 2 ? "FAIL" : (ok ? "---" : "SET");
    draw_band_text(LIM_BOX_X, LIM_READ_Y, LIM_BOX_W, LIM_BOX_H, vt, th->background, vbg, &font_large, FONT_ALIGN_CENTER);
    lcd_fill_rect(LIM_BOX_X + LIM_BOX_W, LIM_READ_Y, LCD_WIDTH - (LIM_BOX_X + LIM_BOX_W), LIM_BOX_H, bg);
    lcd_fill_rect(0, LIM_READ_Y + LIM_BOX_H, LCD_WIDTH, LIM_BAR_Y - (LIM_READ_Y + LIM_BOX_H), bg);

    /* Bar: [lo - span/4, hi + span/4], pass zone between, a marker at the value */
    lcd_fill_rect(0, LIM_BAR_Y, LIM_BAR_X, LIM_BAR_H, bg);
    lcd_fill_rect(LIM_BAR_X + LIM_BAR_W, LIM_BAR_Y, LCD_WIDTH - (LIM_BAR_X + LIM_BAR_W), LIM_BAR_H, bg);
    if (ok) {
        float span = e->hi - e->lo;
        if (span <= 0.0f) span = pow10_neg(e->lim_dec) * 10.0f;
        float x0 = e->lo - span * 0.25f, x1 = e->hi + span * 0.25f;
        int lo_x = (int)((e->lo - x0) / (x1 - x0) * LIM_BAR_W);
        int hi_x = (int)((e->hi - x0) / (x1 - x0) * LIM_BAR_W);
        int mk = -10;
        if (live && str_is_number(r->display_str) && !meter_hold_is_on()) {
            mk = (int)((r->value - x0) / (x1 - x0) * LIM_BAR_W);
            if (mk < 0) mk = 0;
            if (mk > LIM_BAR_W - 3) mk = LIM_BAR_W - 3;
        }
        for (int i = 0; i < LIM_BAR_W; i++) {
            uint16_t c = (i >= mk && i < mk + 3) ? th->text_primary
                       : (i >= lo_x && i <= hi_x) ? th->success : th->grid;
            lcd_fill_rect((uint16_t)(LIM_BAR_X + i), LIM_BAR_Y, 1, LIM_BAR_H, c);
        }
    } else {
        lcd_fill_rect(LIM_BAR_X, LIM_BAR_Y, LIM_BAR_W, LIM_BAR_H, th->grid);
    }
    lcd_fill_rect(0, LIM_BAR_Y + LIM_BAR_H, LCD_WIDTH, LIM_LBL_Y - (LIM_BAR_Y + LIM_BAR_H), bg);

    /* Limit labels; the one the arrows move is highlighted */
    if (ok) {
        fmt_float(a, sizeof a, e->lo, e->lim_dec);
        fmt_float(b, sizeof b, e->hi, e->lim_dec);
    } else {
        strcpy(a, "--"); strcpy(b, "--");
    }
    snprintf(t, sizeof t, "Low %s", a);
    font_draw_string_box(0, LIM_LBL_Y, 160, t,
                         (ok && e->lim_active == 0) ? th->highlight : th->text_secondary,
                         bg, &font_medium, FONT_ALIGN_CENTER);
    snprintf(t, sizeof t, "High %s", b);
    font_draw_string_box(160, LIM_LBL_Y, 160, t,
                         (ok && e->lim_active == 1) ? th->highlight : th->text_secondary,
                         bg, &font_medium, FONT_ALIGN_CENTER);
    lcd_fill_rect(0, LIM_LBL_Y + font_medium.height, LCD_WIDTH, LIM_CNT_Y - (LIM_LBL_Y + font_medium.height), bg);

    if (ok) snprintf(t, sizeof t, "Pass %u     Fail %u", (unsigned)e->pass, (unsigned)e->fail);
    else    snprintf(t, sizeof t, "Press Low or High to set limits");
    font_draw_string_box(0, LIM_CNT_Y, LCD_WIDTH, t, th->text_primary, bg, &font_medium, FONT_ALIGN_CENTER);
    lcd_fill_rect(0, LIM_CNT_Y + font_medium.height, LCD_WIDTH, LIM_LAST_Y - (LIM_CNT_Y + font_medium.height), bg);

    t[0] = '\0';
    if (ok && unit != e->lim_unit && live)
        snprintf(t, sizeof t, "Range changed: limits are in %s", e->lim_unit);
    else if (ok && e->has_fail) {
        fmt_float(a, sizeof a, e->last_fail, e->lim_dec);
        snprintf(t, sizeof t, "Last fail %s %s", a, e->lim_unit);
    }
    font_draw_string_box(0, LIM_LAST_Y, LCD_WIDTH, t, th->text_secondary, bg, &font_medium, FONT_ALIGN_CENTER);
    lcd_fill_rect(0, LIM_LAST_Y + font_medium.height, LCD_WIDTH, METER_BOTTOM - (LIM_LAST_Y + font_medium.height), bg);
}

/* ── Softkey tables ───────────────────────────────────────────────── */

static void skv_function(char *b, uint8_t n)
{
    uint8_t m = meter_submode < METER_SUBMODE_COUNT ? meter_submode : 0;
    snprintf(b, n, "%s", meter_short_names[m]);
}
static void skp_function(void) { meter_function_step(+1); }

static void skv_reset(char *b, uint8_t n) { snprintf(b, n, "Min/Max"); }
static void skp_reset(void) { meter_reset_minmaxavg(); scope_show_popup("Min/Max reset"); }

static void skv_rel(char *b, uint8_t n) { snprintf(b, n, "%s", meter_rel_enabled ? "On" : "Off"); }
static bool ska_rel(void) { return meter_rel_enabled; }
static void skp_rel(void)
{
    meter_toggle_relative();
    scope_show_popup(meter_rel_enabled ? "Relative: On" : "Relative: Off");
}

static void skv_view(char *b, uint8_t n)
{
    snprintf(b, n, "%s", meter_view_names[meter_layout < METER_LAYOUT_COUNT ? meter_layout : 0]);
}
static void skp_view(void)
{
    uint8_t i = 0;
    while (i < METER_LAYOUT_COUNT && meter_view_order[i] != meter_layout) i++;
    meter_layout = meter_view_order[(i + 1) % METER_LAYOUT_COUNT];
    if (meter_layout == METER_LAYOUT_FUSE && meter_submode != 0) {
        meter_function_set(0);                          /* the fuse tester reads a DC drop */
        scope_show_popup("Fuse tester: DC V");
    }
    if (meter_layout == METER_LAYOUT_LIMITS) {
        meter_ext_t *e = meter_ext();
        if (e && !limits_valid_now(e)) (void)limits_init_from_reading(e);
    }
}

static void skv_low(char *b, uint8_t n)
{
    if (limits_valid_now(mx)) fmt_float(b, n, mx->lo, mx->lim_dec);
    else snprintf(b, n, "--");
}
static void skv_high(char *b, uint8_t n)
{
    if (limits_valid_now(mx)) fmt_float(b, n, mx->hi, mx->lim_dec);
    else snprintf(b, n, "--");
}
static bool ska_low(void)  { return limits_valid_now(mx) && mx->lim_active == 0; }
static bool ska_high(void) { return limits_valid_now(mx) && mx->lim_active == 1; }
static void skp_low(void)  { limit_press(0); }
static void skp_high(void) { limit_press(1); }

static void skv_fuse_type(char *b, uint8_t n)
{
    snprintf(b, n, "%s", fuse_type_names[fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0]);
}
static void skp_fuse_type(void) { fuse_next_type(); }
static void skv_fuse_show(char *b, uint8_t n)
{
    static const char *const v[FUSE_VIEW_COUNT] = { "Detail", "Table", "Scan", "Types" };
    snprintf(b, n, "%s", v[fuse_view < FUSE_VIEW_COUNT ? fuse_view : 0]);
}
static void skp_fuse_show(void) { fuse_cycle_view(); }
static void skv_fuse_rating(char *b, uint8_t n)
{
    const fuse_table_t *t = &fuse_tables[fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0];
    char l[8];
    fuse_rating_label(t->entries[fuse_rating_idx < t->count ? fuse_rating_idx : 0].rating_amps,
                      l, sizeof l);
    snprintf(b, n, "%s A", l);
}
static bool ska_arrows(void) { return true; }          /* UP/DOWN adjust this key */
static void skp_fuse_rating(void) { fuse_rating_press(); }
static void skv_fuse_thresh(char *b, uint8_t n)
{
    int t = (int)(fuse_scan_threshold_mv * 10.0f + 0.5f);
    snprintf(b, n, "%d.%d mV", t / 10, t % 10);
}
static void skp_fuse_thresh(void) { fuse_threshold_press(); }

static void skv_cont(char *b, uint8_t n) { snprintf(b, n, "< %d Ohm", (int)meter_continuity_threshold()); }
static void skp_cont(void)
{
    char t[24];
    cont_threshold_idx = (uint8_t)((cont_threshold_idx + 1) % CONT_THRESHOLD_N);
    snprintf(t, sizeof t, "Beep below %d Ohm", (int)meter_continuity_threshold());
    scope_show_popup(t);
}

static const softkey_bar_t meter_bar_cont = {{
    { "Function", skv_function,  skp_function, NULL },
    { "Reset",    skv_reset,     skp_reset,    NULL },
    { "Beep",     skv_cont,      skp_cont,     NULL },
    { "View",     skv_view,      skp_view,     NULL },
}};
static const softkey_bar_t meter_bar_main = {{
    { "Function", skv_function,  skp_function, NULL },
    { "Reset",    skv_reset,     skp_reset,    NULL },
    { "Relative", skv_rel,       skp_rel,      ska_rel },
    { "View",     skv_view,      skp_view,     NULL },
}};
static const softkey_bar_t meter_bar_limits = {{
    { "Function", skv_function,  skp_function, NULL },
    { "Low",      skv_low,       skp_low,      ska_low },
    { "High",     skv_high,      skp_high,     ska_high },
    { "View",     skv_view,      skp_view,     NULL },
}};
static const softkey_bar_t meter_bar_fuse = {{
    { "Fuse type", skv_fuse_type,   skp_fuse_type,   NULL },
    { "Rating",    skv_fuse_rating, skp_fuse_rating, ska_arrows },
    { "Show",      skv_fuse_show,   skp_fuse_show,   NULL },
    { "View",      skv_view,        skp_view,        NULL },
}};
static const softkey_bar_t meter_bar_fuse_scan = {{
    { "Fuse type", skv_fuse_type,   skp_fuse_type,   NULL },
    { "Draw if >", skv_fuse_thresh, skp_fuse_thresh, ska_arrows },
    { "Show",      skv_fuse_show,   skp_fuse_show,   NULL },
    { "View",      skv_view,        skp_view,        NULL },
}};

const softkey_bar_t *meter_softkey_bar(uint8_t layout, uint8_t submode)
{
    if (layout == METER_LAYOUT_LIMITS) return &meter_bar_limits;
    if (layout == METER_LAYOUT_FUSE)
        return fuse_view == FUSE_VIEW_SCAN ? &meter_bar_fuse_scan : &meter_bar_fuse;
    if (submode == 7)                  return &meter_bar_cont;   /* continuity: Beep threshold */
    return &meter_bar_main;
}

static const softkey_bar_t *meter_bar(void)
{
    return meter_softkey_bar(meter_layout, meter_submode);
}

bool meter_softkey_press(int8_t slot)
{
    if (meter_autoselect_is_running()) meter_autoselect_cancel();
    return softkey_bar_press(meter_bar(), slot);
}

void meter_softkeys_draw(void)        { softkey_bar_draw(meter_bar()); }
uint32_t meter_softkeys_epoch(void)   { return softkey_bar_epoch(meter_bar()); }

/* ═══════════════════════════════════════════════════════════════════
 * Main meter screen draw — dispatches by layout
 * ═══════════════════════════════════════════════════════════════════ */

/* Toggle relative mode — captures current reading as reference */
void meter_toggle_relative(void)
{
    if (!meter_rel_enabled) {
        /* Enable: capture current reading as reference */
        uint8_t mode = meter_submode;
        meter_reading_t reading;
        if (mode >= METER_SUBMODE_COUNT) mode = 0;
        if (meter_data_snapshot(&reading) &&
            live_reading_for_mode(&reading, mode)) {
            meter_rel_reference = reading.value;
        } else {
            meter_rel_reference = 0.0f;
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
    uint32_t draw_start_cycles = meter_draw_cycles_now();
    const theme_t *th = theme_get();
    uint8_t mode = meter_submode;
    if (mode >= METER_SUBMODE_COUNT) mode = 0;

    const meter_mode_info_t *m = &meter_modes[mode];
    meter_reading_t reading;

    /* Meter IC polling is now handled by fpga_meter_poll_task at ~4 Hz.
     * Previously, this function called fpga_send_cmd(0x00, 0x09) on every
     * redraw to keep FPGA data frames flowing, which coupled the data
     * acquisition cadence to the UI refresh loop — stop redrawing and the
     * FPGA went silent within ~5 frames. The dedicated poll task decouples
     * the two. See fpga.c:fpga_meter_poll_task and
     * analysis_v120/usart2_isr_state_machine.md. */

    /* Use real FPGA meter data only when it belongs to the currently
     * displayed submode. Never relabel stale voltage data as current,
     * resistance, capacitance, etc. */
    float current_val;
    const char *value_str;
    float bar_pct;

    meter_screen_draw_count++;

    if (!meter_data_snapshot(&reading)) {
        memset(&reading, 0, sizeof(reading));
        reading.display_str[0] = '-';
        reading.display_str[1] = '-';
        reading.display_str[2] = '-';
        reading.display_str[3] = '\0';
        reading.unit_suffix = "";
    }
    meter_screen_last_reading_display_update = reading.display_update_count;

    bool has_live_reading = live_reading_for_mode(&reading, mode);
    meter_screen_last_live = has_live_reading ? 1U : 0U;
    if (has_live_reading) {
        current_val = reading.value;
        value_str = reading.display_str;
        bar_pct = reading.bar_fraction;
    } else {
        current_val = 0.0f;
        value_str = "---";
        bar_pct = 0.0f;
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

    if (has_live_reading) {
        meter_update_stats(current_val);
    }

    /* Continuity visual indicator: flash the entire background green
     * when continuity is detected (resistance < 50 ohms).
     * This helps in noisy environments where the buzzer can't be heard. */
    bool continuity_flash = false;
    if (mode == 7) {
        /* Flash green for parser-confirmed continuity/zero-short frames. */
        continuity_flash = meter_continuity_short_active(&reading,
                                                         has_live_reading,
                                                         current_val);
    }
    uint8_t continuity_flash_u = continuity_flash ? 1U : 0U;

    bool full_clear =
        !meter_screen_retained_valid ||
        meter_screen_last_mode != mode ||
        meter_screen_last_layout != meter_layout ||
        meter_screen_last_theme != theme_get_id() ||
        meter_screen_last_rel_enabled != meter_rel_enabled ||
        meter_screen_last_hold_enabled != meter_hold_enabled ||
        meter_screen_last_hold_locked != meter_hold_locked ||
        meter_screen_last_continuity_flash != continuity_flash_u ||
        meter_screen_last_debug_overlay != meter_debug_overlay;

    /* Clear content area (green flash for continuity, normal otherwise). Keep
     * the big visible blank only for structural changes; ordinary reading
     * updates retain the frame and erase just the dynamic regions. */
    uint16_t clear_bg = continuity_flash ? th->success : th->background;
    bool in_place = (meter_layout == METER_LAYOUT_BIG || meter_layout == METER_LAYOUT_LIMITS ||
                     meter_layout == METER_LAYOUT_FUSE);
    if (full_clear) {
        lcd_fill_rect(0, METER_TOP, LCD_WIDTH, METER_BOTTOM - METER_TOP, clear_bg);
        meter_screen_full_clear_count++;
    } else if (in_place) {
        /* Big and Limits repaint every pixel of their area in place. */
    } else {
        meter_clear_dynamic_areas(meter_layout, mode, clear_bg);
        meter_screen_partial_clear_count++;
    }

    meter_screen_retained_valid = true;
    meter_screen_last_mode = mode;
    meter_screen_last_layout = meter_layout;
    meter_screen_last_theme = theme_get_id();
    meter_screen_last_rel_enabled = meter_rel_enabled;
    meter_screen_last_hold_enabled = meter_hold_enabled;
    meter_screen_last_hold_locked = meter_hold_locked;
    meter_screen_last_continuity_flash = continuity_flash_u;
    meter_screen_last_debug_overlay = meter_debug_overlay;

    /* Mode indicators (top of content area) */
    uint16_t ind_x = 4;


    /* REL indicator (Graph/Stats; Big and Limits draw their own badge row) */
    if (meter_rel_enabled && !in_place) {
        lcd_fill_rect(ind_x, METER_TOP + 1, 28, 13, th->highlight);
        font_draw_string(ind_x + 2, METER_TOP + 2, "REL",
                         th->background, th->highlight, &font_small);
        ind_x += 32;
    }

    /* HOLD indicator */
    if (meter_hold_enabled && !in_place) {
        uint16_t hold_bg = meter_hold_locked ? th->warning : th->grid_center;
        lcd_fill_rect(ind_x, METER_TOP + 1, 36, 13, hold_bg);
        font_draw_string(ind_x + 2, METER_TOP + 2,
                         meter_hold_locked ? "HOLD" : "hold",
                         th->background, hold_bg, &font_small);
        ind_x += 40;
    }

    /* HOLD shows the text that was on screen at the press; REL re-prints the
     * difference with the reading's own decimals (it used 2, F42). */
    if (meter_hold_is_on() && mx && mx->held[0] && !meter_rel_enabled) {
        value_str = mx->held;
    } else if (meter_rel_enabled || meter_hold_is_on()) {
        static char adjusted_str[16];
        int decimals = str_decimals(has_live_reading ? reading.display_str
                                    : (mx && mx->held[0] ? mx->held : "0.00"));
        fmt_float(adjusted_str, sizeof(adjusted_str), display_val, decimals);
        value_str = adjusted_str;
        /* Update bar for adjusted value */
        float abs_dv = display_val < 0 ? -display_val : display_val;
        bar_pct = abs_dv / m->bar_max;
        if (bar_pct > 1.0f) bar_pct = 1.0f;
    }

    switch (meter_layout) {
    case METER_LAYOUT_CHART:
        draw_meter_chart(m, mode, &reading, current_val, value_str);
        break;
    case METER_LAYOUT_STATS:
        draw_meter_stats(m, mode, &reading, current_val, value_str);
        break;
    case METER_LAYOUT_FUSE: {
        /* The drop across the fuse, in mV, from a DC-volts reading (held
         * while HOLD is on). Anything else is "no reading", never a guess. */
        float mv = 0.0f;
        int8_t dec = (mode == 0) ? -1 : -2;            /* -2: on another function */
        if (mode == 0) {
            if (meter_hold_is_on() && mx && mx->held[0]) {
                if (fuse_drop_mv_from_reading(meter_hold_value, mx->held_unit, &mv))
                    dec = (int8_t)fuse_mv_decimals(mx->held, mx->held_unit);
            } else if (has_live_reading &&
                       fuse_drop_mv_from_reading(reading.value, reading.unit_suffix, &mv)) {
                dec = (int8_t)fuse_mv_decimals(reading.display_str, reading.unit_suffix);
            }
        }
        draw_fuse_screen(mv, dec, full_clear);
        break;
    }
    case METER_LAYOUT_LIMITS:
        draw_meter_limits(m, mode, &reading, has_live_reading, value_str);
        break;
    default:
        draw_meter_big(m, mode, &reading, has_live_reading, value_str, clear_bg);
        break;
    }

    /* ── DEBUG OVERLAY — FPGA meter frame capture ──
     *
     * Used during reverse engineering to correlate the 12-byte RX
     * frame + parsed BCD + f6 history with known-input readings.
     * This is how we found the upper-nibble band-interpretation bug
     * (frame[6] = 0x0A/0x0B/0x0D/... all meant low-Ω).
     *
     * Disabled by default in release builds — the block is still
     * compiled in so it can be re-enabled from the debugger or a
     * future settings toggle without a rebuild. To enable, set
     * meter_debug_overlay = true via a breakpoint, a settings entry,
     * or a button handler.
     */
    if (meter_debug_overlay) {
        char dbg[64];
        const char *hex = "0123456789ABCDEF";
        uint16_t dy = LCD_HEIGHT - 48;
        uint16_t dbg_bg = 0x0000;  /* black */
        lcd_fill_rect(0, dy, LCD_WIDTH, 30, dbg_bg);

        int i = 0;

        /* Short frame counter prefix so we can see updates */
        dbg[i++] = 'F';
        { uint16_t fc = fpga.frame_count; char tmp[6]; int t = 0;
          if (fc == 0) tmp[t++] = '0';
          else while (fc > 0 && t < 5) { tmp[t++] = '0' + (fc % 10); fc /= 10; }
          for (int j = t - 1; j >= 0; j--) dbg[i++] = tmp[j]; }
        dbg[i++] = ' ';

        /* All 12 frame bytes — fits in 64-char buffer with room to spare.
         * 12 bytes × 3 chars ("HH ") = 36 chars, plus ~8 prefix = ~44 total. */
        for (int b = 0; b < 12; b++) {
            uint8_t v = fpga.rx_frame[b];
            dbg[i++] = hex[(v >> 4) & 0xF];
            dbg[i++] = hex[v & 0xF];
            if (b < 11) dbg[i++] = ' ';
        }
        dbg[i] = '\0';

        font_draw_string(4, dy + 2, dbg, 0x07E0, dbg_bg, &font_small);

        /* Second debug line: nibble pairs → digits, probe_type, bcd_value */
        {
            const char *hex = "0123456789ABCDEF";
            int j = 0;
            char db2[48];

            /* Nibble pairs (pre-lookup) */
            db2[j++] = 'N';
            for (int k = 0; k < 4; k++) {
                uint8_t n = reading.dbg_nibbles[k];
                db2[j++] = hex[(n >> 4) & 0xF];
                db2[j++] = hex[n & 0xF];
                db2[j++] = ' ';
            }

            /* Decoded digits (post-lookup) */
            db2[j++] = 'D';
            for (int k = 0; k < 4; k++) {
                uint8_t d = reading.dbg_raw_digits[k];
                db2[j++] = hex[(d >> 4) & 0xF];
                db2[j++] = hex[d & 0xF];
                db2[j++] = ' ';
            }

            /* Probe type and range */
            db2[j++] = 'P';
            db2[j++] = '0' + reading.probe_type;
            db2[j++] = ' ';
            db2[j++] = 'R';
            db2[j++] = '0' + reading.range_indicator;
            db2[j++] = ' ';

            /* Raw BCD */
            db2[j++] = '=';
            { int bcd = reading.bcd_value; char tmp[6]; int t = 0;
              if (bcd == 0) tmp[t++] = '0';
              else { int v = bcd; while (v > 0 && t < 5) { tmp[t++] = '0' + (v % 10); v /= 10; } }
              for (int q = t - 1; q >= 0; q--) db2[j++] = tmp[q]; }

            /* Decoder state — what dp and unit the parser chose for
             * the most recent frame. If this says "dp1 kOhm" but the
             * main reading still shows "32.30", we know the UI path
             * is ignoring it. If this says "dp2 Ohm", the decoder
             * didn't fire at all. */
            db2[j++] = ' ';
            db2[j++] = 'd';
            db2[j++] = 'p';
            db2[j++] = '0' + (reading.decimal_pos & 7);
            db2[j++] = ' ';
            const char *us = reading.unit_suffix;
            if (us != NULL) {
                while (*us && j < (int)sizeof(db2) - 1) db2[j++] = *us++;
            }

            db2[j] = '\0';
            font_draw_string(4, dy + 16, db2, 0xFFE0, dbg_bg, &font_small);
        }

        /* Third line: distinct frame[6] values seen since boot.
         * Reveals how many different frame types the FPGA is sending
         * per measurement — without this we can't distinguish real
         * range frames from coincidentally-matching spurious frames. */
        {
            const char *hex2 = "0123456789ABCDEF";
            char db3[64];
            int k = 0;
            db3[k++] = 'f';
            db3[k++] = '6';
            db3[k++] = ':';
            db3[k++] = ' ';
            for (int idx = 0; idx < meter_f6_history_count &&
                              k < (int)sizeof(db3) - 4; idx++) {
                uint8_t v = meter_f6_history[idx];
                db3[k++] = hex2[(v >> 4) & 0xF];
                db3[k++] = hex2[v & 0xF];
                db3[k++] = ' ';
            }
            db3[k] = '\0';
            /* Render in cyan, below the yellow line. */
            font_draw_string(4, dy + 30, db3, 0x07FF, dbg_bg, &font_small);
        }
    }

    meter_record_draw_time(draw_start_cycles, full_clear);
}
