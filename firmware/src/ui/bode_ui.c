/*
 * OpenScope 2C53T - Bode Plot UI
 *
 * Displays frequency response (gain + phase) of a device under test.
 * Signal generator sweeps frequency on CH1 (reference), CH2 measures
 * the DUT response. Gain and phase are plotted vs frequency.
 *
 * Access: Settings > Component Tester > Resistor Calc > OK enters Bode
 *         (or will be its own top-level mode later)
 *
 * For now, generates demo data showing a low-pass filter response.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "bode.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Layout */
#define BODE_TOP        18
#define BODE_BOTTOM     (LCD_HEIGHT - 18)
#define GAIN_Y          38
#define GAIN_H          80
#define PHASE_Y         (GAIN_Y + GAIN_H + 20)
#define PHASE_H         60
#define PLOT_X          40
#define PLOT_W          270

/* State */
static bode_config_t bode_cfg;
static bode_result_t bode_res;
static bool bode_initialized = false;

/* ═══════════════════════════════════════════════════════════════════
 * Generate demo Bode data (1st-order low-pass filter, fc=1kHz)
 * ═══════════════════════════════════════════════════════════════════ */

static void bode_generate_demo(void)
{
    bode_cfg.start_freq_hz = 10.0f;
    bode_cfg.stop_freq_hz = 20000.0f;
    bode_cfg.num_points = 80;
    bode_cfg.amplitude_vpp = 1.0f;
    bode_cfg.log_sweep = true;

    bode_init(&bode_cfg);

    /* Simulate a 1st-order RC low-pass filter: fc = 1kHz */
    float fc = 1000.0f;

    for (uint16_t i = 0; i < bode_cfg.num_points; i++) {
        float f = bode_step_frequency(&bode_cfg, i);

        /* H(f) = 1 / (1 + j*f/fc) */
        float ratio = f / fc;
        float mag = 1.0f / sqrtf(1.0f + ratio * ratio);
        float phase = -atan2f(ratio, 1.0f) * (180.0f / M_PI);

        bode_res.points[i].frequency_hz = f;
        bode_res.points[i].gain_db = 20.0f * log10f(mag);
        bode_res.points[i].phase_deg = phase;
    }

    bode_res.num_points = bode_cfg.num_points;
    bode_res.sweep_done = true;

    /* Find bandwidth */
    bode_res.bandwidth_hz = bode_find_bandwidth(&bode_res);

    /* Find peak */
    bode_res.peak_gain_db = bode_res.points[0].gain_db;
    bode_res.peak_freq_hz = bode_res.points[0].frequency_hz;
    for (uint16_t i = 1; i < bode_res.num_points; i++) {
        if (bode_res.points[i].gain_db > bode_res.peak_gain_db) {
            bode_res.peak_gain_db = bode_res.points[i].gain_db;
            bode_res.peak_freq_hz = bode_res.points[i].frequency_hz;
        }
    }

    bode_initialized = true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Format helpers
 * ═══════════════════════════════════════════════════════════════════ */

static void fmt_freq(float hz, char *buf, int bufsz)
{
    if (hz >= 1000.0f) {
        int k = (int)(hz / 1000.0f);
        int frac = (int)((hz / 1000.0f - (float)k) * 10.0f);
        int p = 0;
        if (k >= 10 && p < bufsz - 1) buf[p++] = (char)('0' + (k / 10) % 10);
        if (p < bufsz - 1) buf[p++] = (char)('0' + k % 10);
        if (frac > 0) {
            if (p < bufsz - 1) buf[p++] = '.';
            if (p < bufsz - 1) buf[p++] = (char)('0' + frac);
        }
        if (p < bufsz - 1) buf[p++] = 'k';
        buf[p] = '\0';
    } else {
        int i = (int)hz;
        int p = 0;
        if (i >= 100 && p < bufsz - 1) buf[p++] = (char)('0' + (i / 100) % 10);
        if (i >= 10 && p < bufsz - 1) buf[p++] = (char)('0' + (i / 10) % 10);
        if (p < bufsz - 1) buf[p++] = (char)('0' + i % 10);
        buf[p] = '\0';
    }
}

static void fmt_db(float db, char *buf, int bufsz)
{
    int p = 0;
    if (db < 0.0f) {
        if (p < bufsz - 1) buf[p++] = '-';
        db = -db;
    }
    int integer = (int)db;
    int frac = (int)((db - (float)integer) * 10.0f);
    if (integer >= 10 && p < bufsz - 1) buf[p++] = (char)('0' + (integer / 10) % 10);
    if (p < bufsz - 1) buf[p++] = (char)('0' + integer % 10);
    if (p < bufsz - 1) buf[p++] = '.';
    if (p < bufsz - 1) buf[p++] = (char)('0' + frac);
    buf[p] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 * Draw Bode plot screen
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_plot_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            const char *title, const theme_t *th)
{
    /* Border */
    lcd_fill_rect(x - 1, y - 1, w + 2, 1, th->grid_center);
    lcd_fill_rect(x - 1, y + h, w + 2, 1, th->grid_center);
    lcd_fill_rect(x - 1, y, 1, h, th->grid_center);
    lcd_fill_rect(x + w, y, 1, h, th->grid_center);

    /* Title */
    font_draw_string(x + 2, y + 2, title, th->text_secondary, th->background, &font_small);

    /* Horizontal grid lines (4 divisions) */
    for (int i = 1; i < 4; i++) {
        uint16_t gy = y + (h * i) / 4;
        for (uint16_t gx = x; gx < x + w; gx += 4) {
            lcd_set_pixel(gx, gy, th->grid);
        }
    }
}

void draw_bode_screen(void)
{
    const theme_t *th = theme_get();

    if (!bode_initialized) {
        bode_generate_demo();
    }

    /* Clear content area */
    lcd_fill_rect(0, BODE_TOP, LCD_WIDTH, BODE_BOTTOM - BODE_TOP, th->background);

    /* Title */
    font_draw_string_center(LCD_WIDTH / 2, BODE_TOP + 2, "Bode Plot",
                            th->text_primary, th->background, &font_medium);

    /* Sweep info */
    char info[32];
    fmt_freq(bode_cfg.start_freq_hz, info, sizeof(info));
    font_draw_string(PLOT_X, BODE_TOP + 2, info,
                     th->text_secondary, th->background, &font_small);
    fmt_freq(bode_cfg.stop_freq_hz, info, sizeof(info));
    font_draw_string_right(PLOT_X + PLOT_W, BODE_TOP + 2, info,
                           th->text_secondary, th->background, &font_small);

    /* ── Gain plot ───────────────────────────────────────── */
    draw_plot_frame(PLOT_X, GAIN_Y, PLOT_W, GAIN_H, "Gain (dB)", th);

    /* Use bode_render_gain for the trace */
    bode_render_gain(&bode_res, PLOT_X, GAIN_Y, PLOT_W, GAIN_H, th->ch1);

    /* Y axis labels for gain */
    font_draw_string_right(PLOT_X - 2, GAIN_Y, "0",
                           th->text_secondary, th->background, &font_small);
    font_draw_string_right(PLOT_X - 2, GAIN_Y + GAIN_H - 10, "-40",
                           th->text_secondary, th->background, &font_small);

    /* -3dB marker */
    if (bode_res.bandwidth_hz > 0.0f) {
        /* Find x position of bandwidth frequency */
        float log_range = log10f(bode_cfg.stop_freq_hz / bode_cfg.start_freq_hz);
        float log_bw = log10f(bode_res.bandwidth_hz / bode_cfg.start_freq_hz);
        float t = log_bw / log_range;
        uint16_t bw_x = PLOT_X + (uint16_t)(t * (float)PLOT_W);

        /* Vertical dashed line at -3dB */
        for (uint16_t y = GAIN_Y; y < GAIN_Y + GAIN_H; y += 3) {
            lcd_set_pixel(bw_x, y, th->warning);
        }

        /* Label */
        fmt_freq(bode_res.bandwidth_hz, info, sizeof(info));
        font_draw_string(bw_x + 2, GAIN_Y + GAIN_H - 12, info,
                         th->warning, th->background, &font_small);
        font_draw_string(bw_x + 2, GAIN_Y + GAIN_H - 24, "-3dB",
                         th->warning, th->background, &font_small);
    }

    /* ── Phase plot ──────────────────────────────────────── */
    draw_plot_frame(PLOT_X, PHASE_Y, PLOT_W, PHASE_H, "Phase (deg)", th);

    bode_render_phase(&bode_res, PLOT_X, PHASE_Y, PLOT_W, PHASE_H, th->ch2);

    /* Phase Y axis labels */
    font_draw_string_right(PLOT_X - 2, PHASE_Y, "0",
                           th->text_secondary, th->background, &font_small);
    font_draw_string_right(PLOT_X - 2, PHASE_Y + PHASE_H - 10, "-90",
                           th->text_secondary, th->background, &font_small);

    /* ── Summary stats ───────────────────────────────────── */
    uint16_t stat_y = PHASE_Y + PHASE_H + 4;

    font_draw_string(4, stat_y, "BW:",
                     th->text_secondary, th->background, &font_small);
    if (bode_res.bandwidth_hz > 0.0f) {
        fmt_freq(bode_res.bandwidth_hz, info, sizeof(info));
        font_draw_string(24, stat_y, info,
                         th->highlight, th->background, &font_small);
    } else {
        font_draw_string(24, stat_y, ">sweep",
                         th->text_secondary, th->background, &font_small);
    }

    font_draw_string(90, stat_y, "Peak:",
                     th->text_secondary, th->background, &font_small);
    fmt_db(bode_res.peak_gain_db, info, sizeof(info));
    font_draw_string(120, stat_y, info,
                     th->ch1, th->background, &font_small);
    font_draw_string(155, stat_y, "dB",
                     th->text_secondary, th->background, &font_small);

    font_draw_string(180, stat_y, "Demo: RC LP 1kHz",
                     th->text_secondary, th->background, &font_small);
}
