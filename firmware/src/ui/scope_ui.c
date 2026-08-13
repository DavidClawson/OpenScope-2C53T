/*
 * OpenScope 2C53T - Oscilloscope UI
 *
 * Waveform display with:
 *   - Trigger level arrow + dotted line
 *   - Ground reference markers (CH1/CH2 zero-volt arrows)
 *   - Trigger status badge (Auto/Trig'd/Ready/Stop)
 *   - Run/Stop indicator
 *   - Auto-measurement badges (Freq, Vpp, Vrms, Duty, Period, Rise)
 *   - Quick-change popup overlay
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "scope_state.h"
#include "math_channel.h"
#include "persistence.h"
#include "fpga.h"
#include "at32f403a_407.h"  /* GPIO port access for pin scanner */
#include <stdio.h>
#include <math.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Layout constants
 * ═══════════════════════════════════════════════════════════════════ */

#define SCOPE_TOP       18      /* Below status bar */
#define SCOPE_BOT       (LCD_HEIGHT - 16)  /* Above info bar */
#define SCOPE_H         (SCOPE_BOT - SCOPE_TOP)
#define SCOPE_MID_Y     (SCOPE_TOP + SCOPE_H / 2)

/* Measurement badge layout */
#define BADGE_W         76
#define BADGE_H         14
#define BADGE_PAD       2
#define BADGE_ROW_Y     (SCOPE_BOT - BADGE_H - 2)
#define BADGE_ROW2_Y    (BADGE_ROW_Y - BADGE_H - 1)

/* Quick-change popup */
#define POPUP_W         200
#define POPUP_H         36
#define POPUP_X         ((LCD_WIDTH - POPUP_W) / 2)
#define POPUP_Y         ((LCD_HEIGHT - POPUP_H) / 2)

/* ═══════════════════════════════════════════════════════════════════
 * Quick-change popup state (shared with main.c via extern)
 * ═══════════════════════════════════════════════════════════════════ */

static char popup_text[32] = "";
static uint8_t popup_frames = 0;  /* Countdown: show for N frames then dismiss */

#define POPUP_DURATION 10  /* ~500ms at 20fps */

void scope_show_popup(const char *text)
{
    int i = 0;
    while (text[i] && i < (int)sizeof(popup_text) - 1) {
        popup_text[i] = text[i];
        i++;
    }
    popup_text[i] = '\0';
    popup_frames = POPUP_DURATION;
}

bool scope_popup_active(void)
{
    return popup_frames > 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Grid
 * ═══════════════════════════════════════════════════════════════════ */

void draw_scope_grid(void)
{
    const theme_t *th = theme_get();
    uint16_t x, y;

    /* Vertical grid lines (dotted) */
    for (x = 0; x < LCD_WIDTH; x += 32) {
        for (y = SCOPE_TOP; y < SCOPE_BOT; y += 2)
            lcd_set_pixel(x, y, th->grid);
    }
    /* Horizontal grid lines (dotted) */
    for (y = SCOPE_TOP; y < SCOPE_BOT; y += 26) {
        for (x = 0; x < LCD_WIDTH; x += 2)
            lcd_set_pixel(x, y, th->grid);
    }

    /* Center crosshair (solid) */
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, SCOPE_MID_Y, th->grid_center);
    for (y = SCOPE_TOP; y < SCOPE_BOT; y++)
        lcd_set_pixel(LCD_WIDTH / 2, y, th->grid_center);
}

/* ═══════════════════════════════════════════════════════════════════
 * Trigger level indicator
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_trigger_indicator(const scope_state_t *ss, const theme_t *th)
{
    /* Trigger level as pixel Y position */
    int16_t trig_y = SCOPE_MID_Y - ss->trigger.level;
    if (trig_y < SCOPE_TOP + 2) trig_y = SCOPE_TOP + 2;
    if (trig_y > SCOPE_BOT - 3) trig_y = SCOPE_BOT - 3;

    /* Right-edge arrow: small triangle pointing left */
    uint16_t ax = LCD_WIDTH - 1;
    lcd_set_pixel(ax,     (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 3, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 4, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)(trig_y - 1), th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)(trig_y - 2), th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)(trig_y + 1), th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)(trig_y + 2), th->trigger);

    /* Dotted horizontal line at trigger level */
    for (uint16_t x = 0; x < LCD_WIDTH - 6; x += 4)
        lcd_set_pixel(x, (uint16_t)trig_y, th->trigger);

    /* Trigger edge indicator (small arrow next to the trigger arrow) */
    if (ss->trigger.edge == TRIG_RISING) {
        /* Up arrow: rising edge */
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 2), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 1), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 1), th->trigger);
        lcd_set_pixel(ax - 7, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 5, (uint16_t)trig_y,       th->trigger);
    } else {
        /* Down arrow: falling edge */
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 2), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 1), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 1), th->trigger);
        lcd_set_pixel(ax - 7, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 5, (uint16_t)trig_y,       th->trigger);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Ground reference markers (left edge)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_ground_markers(const scope_state_t *ss, const theme_t *th)
{
    /* CH1 ground marker — small right-pointing arrow with "1" */
    if (ss->ch1.enabled) {
        int16_t ch1_y = SCOPE_MID_Y - ss->ch1.position;
        if (ch1_y >= SCOPE_TOP + 2 && ch1_y <= SCOPE_BOT - 3) {
            lcd_set_pixel(0, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(1, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(2, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(3, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(1, (uint16_t)(ch1_y - 1), th->ch1);
            lcd_set_pixel(2, (uint16_t)(ch1_y - 2), th->ch1);
            lcd_set_pixel(1, (uint16_t)(ch1_y + 1), th->ch1);
            lcd_set_pixel(2, (uint16_t)(ch1_y + 2), th->ch1);
            font_draw_string(5, (uint16_t)(ch1_y - 5), "1",
                             th->ch1, th->ch1, &font_small);
        }
    }

    /* CH2 ground marker */
    if (ss->ch2.enabled) {
        int16_t ch2_y = SCOPE_MID_Y - ss->ch2.position;
        if (ch2_y >= SCOPE_TOP + 2 && ch2_y <= SCOPE_BOT - 3) {
            lcd_set_pixel(0, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(1, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(2, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(3, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(1, (uint16_t)(ch2_y - 1), th->ch2);
            lcd_set_pixel(2, (uint16_t)(ch2_y - 2), th->ch2);
            lcd_set_pixel(1, (uint16_t)(ch2_y + 1), th->ch2);
            lcd_set_pixel(2, (uint16_t)(ch2_y + 2), th->ch2);
            font_draw_string(5, (uint16_t)(ch2_y - 5), "2",
                             th->ch2, th->ch2, &font_small);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Trigger status badge
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_trigger_status(const scope_state_t *ss, const theme_t *th)
{
    const char *label;
    uint16_t color;

    if (!ss->running) {
        label = "STOP";
        color = th->warning;
    } else if (ss->trigger.mode == TRIG_AUTO) {
        label = "Auto";
        color = th->success;
    } else if (ss->trigger.mode == TRIG_SINGLE) {
        label = "Ready";
        color = th->highlight;
    } else {
        label = "Trig'd";
        color = th->success;
    }

    /* Draw badge at top-right of waveform area */
    uint16_t bw = 44;
    uint16_t bx = LCD_WIDTH - bw - 2;
    uint16_t by = SCOPE_TOP + 2;
    lcd_fill_rect(bx, by, bw, 14, th->background);
    font_draw_string_right(LCD_WIDTH - 4, by + 1, label,
                           color, th->background, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Run/Stop indicator
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_run_stop(const scope_state_t *ss, const theme_t *th)
{
    if (!ss->running) {
        /* Prominent STOP badge top-center */
        uint16_t bw = 40;
        uint16_t bx = (LCD_WIDTH - bw) / 2;
        lcd_fill_rect(bx, SCOPE_TOP + 2, bw, 16, th->warning);
        font_draw_string_center(LCD_WIDTH / 2, SCOPE_TOP + 4, "STOP",
                                th->text_primary, th->warning, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Measurement badges
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_one_badge(uint16_t x, uint16_t y, const char *label,
                           const char *value, uint16_t color, const theme_t *th)
{
    /* Semi-transparent background (just use dim background) */
    lcd_fill_rect(x, y, BADGE_W, BADGE_H, th->background);

    /* Label in dim color, value in bright channel color */
    font_draw_string(x + BADGE_PAD, y + 1, label,
                     th->text_secondary, th->background, &font_small);
    font_draw_string_right(x + BADGE_W - BADGE_PAD, y + 1, value,
                           color, th->background, &font_small);
}

static void draw_measurement_badges(const scope_state_t *ss, const theme_t *th)
{
    /*
     * Demo measurements — when real ADC is available, these will come
     * from measurement_compute(). For now, show plausible values that
     * update based on the current V/div and timebase settings.
     */
    char buf[16];

    /* Row 1 (bottom): Freq, Vpp, Vrms, Duty */
    uint16_t x = 2;
    uint16_t y1 = BADGE_ROW_Y;

    snprintf(buf, sizeof(buf), "1.00kHz");
    draw_one_badge(x, y1, "Freq", buf, th->ch1, th);
    x += BADGE_W + 2;

    snprintf(buf, sizeof(buf), "%s", vdiv_table[ss->ch1.vdiv_idx].label);
    draw_one_badge(x, y1, "Vpp", buf, th->ch1, th);
    x += BADGE_W + 2;

    draw_one_badge(x, y1, "Vrms", "707mV", th->ch1, th);
    x += BADGE_W + 2;

    draw_one_badge(x, y1, "Duty", "50.0%", th->ch1, th);

    /* Row 2 (above row 1): Period, Rise for CH2 context */
    x = 2;
    uint16_t y2 = BADGE_ROW2_Y;

    draw_one_badge(x, y2, "Per", "1.00ms", th->ch2, th);
    x += BADGE_W + 2;

    snprintf(buf, sizeof(buf), "%s", vdiv_table[ss->ch2.vdiv_idx].label);
    draw_one_badge(x, y2, "CH2 V", buf, th->ch2, th);
}

/* ═══════════════════════════════════════════════════════════════════
 * Quick-change popup overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_popup(const theme_t *th)
{
    if (popup_frames == 0) return;
    popup_frames--;

    /* Dark box with border */
    lcd_fill_rect(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, th->background);
    /* Top/bottom border */
    lcd_fill_rect(POPUP_X, POPUP_Y, POPUP_W, 1, th->highlight);
    lcd_fill_rect(POPUP_X, POPUP_Y + POPUP_H - 1, POPUP_W, 1, th->highlight);
    /* Left/right border */
    lcd_fill_rect(POPUP_X, POPUP_Y, 1, POPUP_H, th->highlight);
    lcd_fill_rect(POPUP_X + POPUP_W - 1, POPUP_Y, 1, POPUP_H, th->highlight);

    /* Centered text */
    font_draw_string_center(LCD_WIDTH / 2, POPUP_Y + 10, popup_text,
                            th->text_primary, th->background, &font_large);
}

/* ═══════════════════════════════════════════════════════════════════
 * Demo waveform (sine + square)
 * ═══════════════════════════════════════════════════════════════════ */

void draw_demo_waveform(uint32_t frame)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();

    /* Freeze waveform when stopped — hold the last running frame */
    static uint32_t frozen_frame = 0;
    if (ss->running) {
        frozen_frame = frame;
    }
    uint32_t draw_frame = ss->running ? frame : frozen_frame;

    /*
     * If FPGA has delivered real ADC data, render from the sample buffers.
     * Each buffer holds 512 samples (normal mode) as unsigned 8-bit values
     * centered around 128 (after ADC offset calibration).
     * We map the 0-255 range into the waveform display area.
     */
    if (fpga_data_ready()) {
        /* Latch: once we've ever seen real ADC data, disable the
         * synthetic demo fallback permanently. */
        scope_mark_acquisition_ready();

        const volatile uint8_t *ch1_buf = fpga_get_ch1_buf();
        const volatile uint8_t *ch2_buf = fpga_get_ch2_buf();

        /* Re-triggering DISABLED — see note in demo waveform fallback.
         * TODO: re-enable once SPI3 protocol is confirmed working. */

        /* CH1: real ADC data */
        if (ss->ch1.enabled && ch1_buf != NULL) {
            int16_t ch1_offset = ss->ch1.position;
            for (uint16_t x = 0; x < LCD_WIDTH && x < 512; x++) {
                /* Map 0-255 ADC value to waveform area.
                 * 128 = center (SCOPE_MID_Y), scale to fit SCOPE_H. */
                int16_t sample = (int16_t)ch1_buf[x];
                int16_t y = SCOPE_MID_Y - ((sample - 128) * SCOPE_H / 256)
                            - ch1_offset;
                if (y >= SCOPE_TOP && y < SCOPE_BOT) {
                    lcd_set_pixel(x, (uint16_t)y, th->ch1);
                    if (y + 1 < SCOPE_BOT)
                        lcd_set_pixel(x, (uint16_t)(y + 1), th->ch1);
                }
            }
        }

        /* CH2: real ADC data */
        if (ss->ch2.enabled && ch2_buf != NULL) {
            int16_t ch2_offset = ss->ch2.position;
            for (uint16_t x = 0; x < LCD_WIDTH && x < 512; x++) {
                int16_t sample = (int16_t)ch2_buf[x];
                int16_t y = SCOPE_MID_Y - ((sample - 128) * SCOPE_H / 256)
                            - ch2_offset;
                if (y >= SCOPE_TOP && y < SCOPE_BOT) {
                    lcd_set_pixel(x, (uint16_t)y, th->ch2);
                    if (y + 1 < SCOPE_BOT)
                        lcd_set_pixel(x, (uint16_t)(y + 1), th->ch2);
                }
            }
        }
        return;
    }

    /* ── Fallback: synthetic demo waveform (no FPGA data yet) ──────
     *
     * Only rendered if acquisition has NEVER produced real data. Once
     * the latch flips, we leave the grid empty instead — otherwise a
     * transient FPGA stall would flash fake waveforms on top of a
     * stopped trace, which is worse than an empty screen. */
    if (scope_acquisition_ready()) {
        return;
    }

    /* SPI3 acquisition triggers are now fired from main.c display loop
     * with 500ms warmup delay and early-abort safety. See main.c and
     * fpga_acquisition_task() for the crash-protection logic. */

    static const int8_t sin_lut[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    /* CH1: sine wave — scale based on vdiv index */
    if (ss->ch1.enabled) {
        int16_t ch1_center = SCOPE_MID_Y - ss->ch1.position;
        int16_t amplitude = 40 - (int16_t)(ss->ch1.vdiv_idx * 2);
        if (amplitude < 10) amplitude = 10;

        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            uint8_t idx = (uint8_t)((x * 4 + draw_frame) & 0x3F);
            int16_t y = ch1_center - (sin_lut[idx] * amplitude / 100);
            if (y >= SCOPE_TOP && y < SCOPE_BOT) {
                lcd_set_pixel(x, (uint16_t)y, th->ch1);
                if (y + 1 < SCOPE_BOT)
                    lcd_set_pixel(x, (uint16_t)(y + 1), th->ch1);
            }
        }
    }

    /* CH2: square wave */
    if (ss->ch2.enabled) {
        int16_t ch2_center = SCOPE_MID_Y + 50 - ss->ch2.position;

        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            uint8_t phase = (uint8_t)((x * 4 + draw_frame) & 0x3F);
            int16_t y = ch2_center + (phase < 32 ? -25 : 25);
            if (y >= SCOPE_TOP && y < SCOPE_BOT)
                lcd_set_pixel(x, (uint16_t)y, th->ch2);

            /* Vertical edges */
            if (x > 0) {
                uint8_t prev = (uint8_t)(((x - 1) * 4 + draw_frame) & 0x3F);
                if ((prev < 32) != (phase < 32)) {
                    int16_t y1 = ch2_center - 25;
                    int16_t y2 = ch2_center + 25;
                    for (int16_t yy = y1; yy <= y2; yy++) {
                        if (yy >= SCOPE_TOP && yy < SCOPE_BOT)
                            lcd_set_pixel(x, (uint16_t)yy, th->ch2);
                    }
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Cursor measurement drawing
 * ═══════════════════════════════════════════════════════════════════ */

static void format_si(float val, const char *unit, char *buf, int bufsize)
{
    const char *prefix;
    float abs_val = val < 0.0f ? -val : val;
    int pos = 0;

    if (abs_val == 0.0f) {
        prefix = "";
    } else if (abs_val >= 1.0f) {
        prefix = "";
    } else if (abs_val >= 1.0e-3f) {
        val *= 1.0e3f;
        prefix = "m";
    } else if (abs_val >= 1.0e-6f) {
        val *= 1.0e6f;
        prefix = "u";
    } else {
        val *= 1.0e9f;
        prefix = "n";
    }

    if (val < 0.0f && pos < bufsize - 1) {
        buf[pos++] = '-';
        val = -val;
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 100.0f + 0.5f);
    if (frac >= 100) { integer++; frac -= 100; }

    if (integer >= 1000 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 1000) % 10);
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 100) % 10);
    if (integer >= 10 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac / 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac % 10);

    while (*prefix && pos < bufsize - 1) buf[pos++] = *prefix++;
    while (*unit && pos < bufsize - 1) buf[pos++] = *unit++;
    buf[pos] = '\0';
}

static void draw_vline_dashed(uint16_t x, uint16_t y_top, uint16_t y_bot,
                              uint16_t color, bool is_active)
{
    for (uint16_t y = y_top; y <= y_bot; y++) {
        if (is_active || ((y & 7) < 4))
            lcd_set_pixel(x, y, color);
    }
}

static void draw_hline_dashed(uint16_t y, uint16_t x_left, uint16_t x_right,
                              uint16_t color, bool is_active)
{
    for (uint16_t x = x_left; x <= x_right; x++) {
        if (is_active || ((x & 7) < 4))
            lcd_set_pixel(x, y, color);
    }
}

static void draw_cursors(void)
{
    const scope_state_t *ss = scope_state_get();
    const cursor_state_t *c = &ss->cursor;
    const theme_t *th = theme_get();

    if (c->mode == CURSOR_OFF) return;

    uint16_t color_active   = th->highlight;
    uint16_t color_inactive = th->text_secondary;

    if (c->mode == CURSOR_VERTICAL || c->mode == CURSOR_BOTH) {
        bool v1_active = (c->active == CURSOR_SEL_V1);
        bool v2_active = (c->active == CURSOR_SEL_V2);

        draw_vline_dashed(c->v1_x, SCOPE_TOP, SCOPE_BOT,
                          v1_active ? color_active : color_inactive, v1_active);
        draw_vline_dashed(c->v2_x, SCOPE_TOP, SCOPE_BOT,
                          v2_active ? color_active : color_inactive, v2_active);

        font_draw_string(c->v1_x > 12 ? c->v1_x - 10 : 0, SCOPE_TOP,
                         "V1", v1_active ? color_active : color_inactive,
                         v1_active ? color_active : color_inactive, &font_small);
        font_draw_string(c->v2_x > 12 ? c->v2_x - 10 : 0, SCOPE_TOP,
                         "V2", v2_active ? color_active : color_inactive,
                         v2_active ? color_active : color_inactive, &font_small);
    }

    if (c->mode == CURSOR_HORIZONTAL || c->mode == CURSOR_BOTH) {
        bool h1_active = (c->active == CURSOR_SEL_H1);
        bool h2_active = (c->active == CURSOR_SEL_H2);

        draw_hline_dashed(c->h1_y, 0, LCD_WIDTH - 1,
                          h1_active ? color_active : color_inactive, h1_active);
        draw_hline_dashed(c->h2_y, 0, LCD_WIDTH - 1,
                          h2_active ? color_active : color_inactive, h2_active);

        font_draw_string_right(LCD_WIDTH - 1,
                               c->h1_y > SCOPE_TOP + 2 ? c->h1_y - 13 : c->h1_y + 2,
                               "H1", h1_active ? color_active : color_inactive,
                               h1_active ? color_active : color_inactive, &font_small);
        font_draw_string_right(LCD_WIDTH - 1,
                               c->h2_y > SCOPE_TOP + 2 ? c->h2_y - 13 : c->h2_y + 2,
                               "H2", h2_active ? color_active : color_inactive,
                               h2_active ? color_active : color_inactive, &font_small);
    }

    /* Delta readout */
    uint16_t badge_y = SCOPE_BOT - 28;
    uint16_t badge_x = 4;
    char buf[24];

    if (c->mode == CURSOR_VERTICAL || c->mode == CURSOR_BOTH) {
        int16_t dx = (int16_t)c->v2_x - (int16_t)c->v1_x;
        float dt = (float)dx * c->time_per_pixel;

        lcd_fill_rect(badge_x, badge_y, 100, 13, th->background);
        format_si(dt < 0.0f ? -dt : dt, "s", buf, sizeof(buf));
        {
            char label[32];
            int li = 0;
            label[li++] = 'd'; label[li++] = 't'; label[li++] = '=';
            if (dt < 0.0f) label[li++] = '-';
            int j = 0;
            while (buf[j] && li < 30) label[li++] = buf[j++];
            label[li] = '\0';
            font_draw_string(badge_x, badge_y, label, th->highlight, th->highlight, &font_small);
        }

        if (dx != 0) {
            float freq = 1.0f / (dt < 0.0f ? -dt : dt);
            lcd_fill_rect(badge_x, badge_y + 14, 100, 13, th->background);
            format_si(freq, "Hz", buf, sizeof(buf));
            {
                char label[32];
                int li = 0;
                label[li++] = '1'; label[li++] = '/'; label[li++] = 'd';
                label[li++] = 't'; label[li++] = '=';
                int j = 0;
                while (buf[j] && li < 30) label[li++] = buf[j++];
                label[li] = '\0';
                font_draw_string(badge_x, badge_y + 14, label,
                                 th->highlight, th->highlight, &font_small);
            }
        }
    }

    if (c->mode == CURSOR_HORIZONTAL || c->mode == CURSOR_BOTH) {
        int16_t dy = (int16_t)c->h1_y - (int16_t)c->h2_y;
        float dv = (float)dy * c->volts_per_pixel;

        uint16_t vbadge_x = (c->mode == CURSOR_BOTH) ? 120 : badge_x;
        lcd_fill_rect(vbadge_x, badge_y, 100, 13, th->background);
        format_si(dv < 0.0f ? -dv : dv, "V", buf, sizeof(buf));
        {
            char label[32];
            int li = 0;
            label[li++] = 'd'; label[li++] = 'V'; label[li++] = '=';
            if (dv < 0.0f) label[li++] = '-';
            int j = 0;
            while (buf[j] && li < 30) label[li++] = buf[j++];
            label[li] = '\0';
            font_draw_string(vbadge_x, badge_y, label, th->highlight, th->highlight, &font_small);
        }
    }

    /* Cursor mode indicator */
    {
        const char *mode_str;
        switch (c->mode) {
        case CURSOR_VERTICAL:    mode_str = "CUR:T"; break;
        case CURSOR_HORIZONTAL:  mode_str = "CUR:V"; break;
        case CURSOR_BOTH:        mode_str = "CUR:TV"; break;
        default:                 mode_str = ""; break;
        }
        font_draw_string_right(LCD_WIDTH - 4, SCOPE_BOT - 13,
                               mode_str, th->highlight, th->highlight, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Persistence overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_persistence_overlay(uint32_t frame)
{
    if (!persist_enabled) return;

    /* Feed demo waveform y-values into persistence buffer */
    static const int8_t sin_lut_p[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    uint16_t y_vals[PERSIST_WIDTH];
    uint16_t y_center = SCOPE_H / 2;
    for (uint16_t px = 0; px < PERSIST_WIDTH; px++) {
        uint8_t idx = (uint8_t)((px * 4 + frame) & 0x3F);
        int16_t yv = (int16_t)(y_center - (sin_lut_p[idx] * 40 / 100));
        if (yv < 0) yv = 0;
        if (yv >= PERSIST_HEIGHT) yv = PERSIST_HEIGHT - 1;
        y_vals[px] = (uint16_t)yv;
    }
    persist_add_trace(y_vals, 0);
    persist_decay();

    const uint8_t *buf = persist_get_buffer();
    if (!buf) return;

    for (uint16_t y = 0; y < PERSIST_HEIGHT; y++) {
        for (uint16_t x = 0; x < PERSIST_WIDTH; x++) {
            uint8_t intensity = buf[y * PERSIST_WIDTH + x];
            if (intensity > 0) {
                uint16_t color = persist_intensity_to_color_ch1(intensity);
                if (color != 0x0000)
                    lcd_set_pixel(x, y + SCOPE_TOP, color);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Math channel overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_math_waveform(uint32_t frame)
{
    if (!math_enabled) return;

    const theme_t *th = theme_get();

    static const int8_t sin_lut_m[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    int16_t ch_a[LCD_WIDTH], ch_b[LCD_WIDTH], result[LCD_WIDTH];
    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
        ch_a[x] = (int16_t)(sin_lut_m[idx] * 40);
        uint8_t phase = (uint8_t)((x * 4 + frame) & 0x3F);
        ch_b[x] = (int16_t)(phase < 32 ? -2500 : 2500);
    }

    math_config_t cfg;
    cfg.operation = (math_op_t)math_op;
    cfg.scale = 1.0f;
    math_channel_compute(ch_a, ch_b, result, LCD_WIDTH, &cfg);

    uint16_t y_center = SCOPE_MID_Y;
    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        int16_t y = (int16_t)(y_center - result[x] / 100);
        if (y >= SCOPE_TOP && y < SCOPE_BOT)
            lcd_set_pixel(x, (uint16_t)y, th->warning);
    }

    const char *name = math_channel_name((math_op_t)math_op);
    font_draw_string(130, SCOPE_TOP, "MATH:", th->warning, th->warning, &font_small);
    font_draw_string(170, SCOPE_TOP, name, th->warning, th->warning, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main scope screen compositor
 * ═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Debug Overlay
 * Shows raw acquisition status for diagnosing scope data pipeline.
 * Off by default. Enable with -DSCOPE_DEBUG_OVERLAY at build time.
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef SCOPE_DEBUG_OVERLAY

#define SCOPE_DBG_Y   (LCD_HEIGHT - 60)
#define SCOPE_DBG_H   58

/*
 * GPIO Pin Scanner — "poor man's logic analyzer"
 *
 * Continuously reads all GPIO port input registers (IDT) and tracks
 * which pins have toggled at least once since boot. Displays a
 * toggle mask for each port (A-E).
 *
 * Any pin showing unexpected toggle activity could be the FPGA's
 * actual SPI data output — which might NOT be PB4 if the published
 * firmware doesn't match this V1.4 board hardware.
 *
 * Known toggling pins (expected):
 *   PA2 (USART2 TX), PA3 (USART2 RX)
 *   PA7,PA8 (button matrix)
 *   PB0 (button matrix), PB7 (PRM button)
 *   PC5,PC8,PC10,PC13 (buttons)
 *   PE2,PE3 (button matrix)
 *
 * Interesting if toggling (possible FPGA SPI data out):
 *   Any pin NOT in the known list above
 */

/* Accumulated toggle masks — bits set = pin toggled at least once */
static uint16_t gpio_toggle_a = 0, gpio_toggle_b = 0;
static uint16_t gpio_toggle_c = 0, gpio_toggle_d = 0, gpio_toggle_e = 0;
static uint16_t gpio_prev_a = 0, gpio_prev_b = 0;
static uint16_t gpio_prev_c = 0, gpio_prev_d = 0, gpio_prev_e = 0;
static bool gpio_scan_started = false;

/* Rapid-sample toggle counters (prev_state = 0xFF means "not initialized") */
static uint32_t pa1_toggle_count = 0;
static uint8_t  pa1_prev_state = 0xFF;
static uint32_t pb3_toggle_count = 0;  /* SPI3 SCK — should toggle if GMUX works */
static uint8_t  pb3_prev_state = 0xFF;
static uint32_t pb4_toggle_count = 0;  /* SPI3 MISO — should toggle if FPGA responds */
static uint8_t  pb4_prev_state = 0xFF;

static void gpio_scan_update(void)
{
    uint16_t a = (uint16_t)GPIOA->idt;
    uint16_t b = (uint16_t)GPIOB->idt;
    uint16_t c = (uint16_t)GPIOC->idt;
    uint16_t d = (uint16_t)GPIOD->idt;
    uint16_t e = (uint16_t)GPIOE->idt;

    if (gpio_scan_started) {
        gpio_toggle_a |= (a ^ gpio_prev_a);
        gpio_toggle_b |= (b ^ gpio_prev_b);
        gpio_toggle_c |= (c ^ gpio_prev_c);
        gpio_toggle_d |= (d ^ gpio_prev_d);
        gpio_toggle_e |= (e ^ gpio_prev_e);
    }

    /* In-context SPI3 MISO finder REMOVED (2026-04-06).
     * This was doing its own CS/SPI transfers from the display task,
     * which corrupts the acquisition task's SPI3 transfers (both tasks
     * hit the same SPI peripheral simultaneously on a single-core MCU).
     * The scanner found PC6-LOW enables FPGA SPI — no longer need this. */
    pb3_toggle_count = (gpio_toggle_b >> 3) & 1;
    pb4_toggle_count = (gpio_toggle_b >> 4) & 1;

    gpio_prev_a = a; gpio_prev_b = b;
    gpio_prev_c = c; gpio_prev_d = d; gpio_prev_e = e;
    gpio_scan_started = true;
}

/* Set by draw_scope_screen (whose full-area clear wipes 180-224) so the strip
 * background is refilled exactly when needed; the live incremental path never
 * dirties it, so per-frame text updates go straight over their own opaque
 * cells with no blank state — that blank-then-text sequence at frame rate was
 * half of the visible full-screen flashing. */
static uint8_t scope_dbg_strip_dirty = 1;

/* Pad to a fixed width so opaque redraw erases a previously-longer line
 * (counters shrink after a reset; P0 flips H/L width-stable but SS does not). */
static void dbg_pad(char *s, unsigned cap, unsigned width)
{
    unsigned l = 0;
    while (s[l] != '\0') l++;
    while (l < width && l + 1 < cap) s[l++] = ' ';
    s[l] = '\0';
}

static void draw_scope_debug(const theme_t *th)
{
    /* Update toggle masks */
    gpio_scan_update();

    /* Dark background strip */
#if (defined(FPGA_PIN_SWEEP_BUILD) && FPGA_PIN_SWEEP_BUILD) || \
    (defined(FPGA_CFG_TRACE_BUILD) && FPGA_CFG_TRACE_BUILD) || \
    (defined(FPGA_IDCODE_PROBE) && FPGA_IDCODE_PROBE)
    /* Variant overlays draw variable-length content — keep the full refill. */
    lcd_fill_rect(0, SCOPE_DBG_Y, LCD_WIDTH, SCOPE_DBG_H, 0x0000);
#else
    if (scope_dbg_strip_dirty) {
        lcd_fill_rect(0, SCOPE_DBG_Y, LCD_WIDTH, SCOPE_DBG_H, 0x0000);
        scope_dbg_strip_dirty = 0;
    }
#endif

    char buf[64];

#if defined(FPGA_PIN_SWEEP_BUILD) && FPGA_PIN_SWEEP_BUILD
    /* ── RECONFIG_N pin sweep results ───────────────────────────────────────
     * Press SAVE in scope mode to run it. Read HIT first: it is the only line
     * that can say "we found something".
     *   HIT:-- H:0 AF:0  -> every candidate pulsed, nothing moved the STATUS
     *                       register. No MCU-owned pin stock drives is RECONFIG_N.
     *   HIT:PCn          -> that pin changed the status after CONFIG_ENABLE.
     *   AF non-zero      -> that many candidates failed the IDCODE anchor and
     *                       were DISCARDED, not counted as hits. Worth a look
     *                       anyway: a pin that CLOSES the config port would also
     *                       land here (Exp L behaviour). */
    {
        const char *st = (fpga.sweep_state == 0) ? "IDLE" :
                         (fpga.sweep_state == 1) ? "RUN"  : "DONE";
        snprintf(buf, sizeof(buf), "SWEEP:%s %u/%u", st,
                 (unsigned)fpga.sweep_tested, (unsigned)fpga.sweep_total);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "BASE:%08lX",
                 (unsigned long)fpga.sweep_baseline);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        /* PH distinguishes two very different findings, which Exp O's single
         * snapshot could not tell apart:
         *   P = the PULSE ALONE moved the status — the pin triggered something
         *       on its own, which is what a real RECONFIG_N does (reload from NV).
         *   C = the status only moved after CONFIG_ENABLE — the pulse made 0x15
         *       land, i.e. it unlocked config entry. This is the jackpot. */
        const char *ph = (fpga.sweep_hit_phase == 1) ? "P" :
                         (fpga.sweep_hit_phase == 2) ? "C" : "-";
        snprintf(buf, sizeof(buf), "HIT:%s/%s %08lX",
                 fpga_sweep_pin_name(fpga.sweep_first_hit), ph,
                 (unsigned long)fpga.sweep_hit_status);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "H:%u AF:%u  press SAVE",
                 (unsigned)fpga.sweep_hits, (unsigned)fpga.sweep_anchor_fail);
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

#if defined(FPGA_CFG_TRACE_BUILD) && FPGA_CFG_TRACE_BUILD
    /* ── Step-resolved anchored status trace ────────────────────────────────
     * Six checkpoints through the config sequence, each anchored on the IDCODE.
     * Checked in this order before drawing any conclusion:
     *
     *   A: line     the anchor at each checkpoint. '0' = IDCODE found aligned,
     *               'x' = anchor FAILED and that Tn is NOT a measurement. An 'x'
     *               appearing partway through is itself the finding: the config
     *               port closed at that step (Exp L behaviour).
     *   T0..T5      anchor-corrected STATUS. FFFFFFFF means "not measured".
     *
     * All six identical => the part ignores every config command while still
     * answering reads. Any step where it MOVES is where the sequence first has
     * an effect, and is where to aim next. */
    {
        snprintf(buf, sizeof(buf), "T0:%08lX T1:%08lX",
                 (unsigned long)fpga.cfg_trace[0], (unsigned long)fpga.cfg_trace[1]);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "T2:%08lX T3:%08lX",
                 (unsigned long)fpga.cfg_trace[2], (unsigned long)fpga.cfg_trace[3]);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "T4:%08lX T5:%08lX",
                 (unsigned long)fpga.cfg_trace[4], (unsigned long)fpga.cfg_trace[5]);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        char a[FPGA_CFG_TRACE_N + 1];
        for (unsigned i = 0; i < FPGA_CFG_TRACE_N; i++) {
            int8_t o = fpga.cfg_trace_anchor[i];
            a[i] = (o < 0) ? 'x' : (char)('0' + (o % 10));
        }
        a[FPGA_CFG_TRACE_N] = '\0';
        /* MV = the count of checkpoints whose status differs from T0. 0 means the
         * register never moved anywhere in the sequence. */
        unsigned moved = 0;
        for (unsigned i = 1; i < FPGA_CFG_TRACE_N; i++)
            if (fpga.cfg_trace[i] != fpga.cfg_trace[0]) moved++;
        snprintf(buf, sizeof(buf), "A:%s MV:%u H2:%c", a, moved,
                 fpga.h2_upload_done ? 'Y' : 'N');
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

#if defined(FPGA_IDCODE_PROBE) && FPGA_IDCODE_PROBE
    /* ── Experiment J overlay: the anchored opcode-discrimination probe ──────
     * Replaces the normal overlay entirely for this build. Four opcodes read at
     * /256 on a pristine bus before the prelude, 8 bytes each.
     *
     * READ IT LIKE THIS:
     *   ID / NP identical, and each showing a repeated 4-byte group
     *     => the FPGA free-runs a fixed pattern and ignores MOSI. Confirms the
     *        2026-06-13 conclusion on a valid measurement for the first time;
     *        SSPI cannot reach the config engine and JTAG is the route.
     *   ID differs from NP, ID@ >= 0
     *     => the FPGA DOES decode SSPI opcodes. The "not in config-receive mode"
     *        conclusion collapses and the config-entry search reopens.
     *   ID@ >= 0 but not 0
     *     => IDCODE present but phase-shifted by that many bits; the reply is
     *        real and our sampling alignment is off (see Exp I).
     */
    {
        const volatile uint8_t *p;

        p = fpga.probe_idcode;
        snprintf(buf, sizeof(buf), "ID:%02X%02X%02X%02X %02X%02X%02X%02X",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        p = fpga.probe_noop;
        snprintf(buf, sizeof(buf), "NP:%02X%02X%02X%02X %02X%02X%02X%02X",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "ST:%02X%02X%02X%02X US:%02X%02X%02X%02X",
                 fpga.probe_status[0], fpga.probe_status[1],
                 fpga.probe_status[2], fpga.probe_status[3],
                 fpga.probe_user[0], fpga.probe_user[1],
                 fpga.probe_user[2], fpga.probe_user[3]);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        /* ID@ = bit offset where 0x0120681B was found, -1 = absent.
         * SM  = 0x11 and 0x00 returned identical bytes (Y => no opcode decode).
         * RP  = the 0x11 reply repeats every 4 bytes (Y => free-running pattern).
         * PO@ = same IDCODE search on the 0x11 read taken AFTER CONFIG_ENABLE.
         * CL@ = same again AFTER the full upload and the 0x3A close. Per Exp L a
         *       configured part stops answering 0x11, so CL@ >= 0 means the config
         *       port never closed => we are definitively NOT configured. */
        snprintf(buf, sizeof(buf), "ID@%d SM:%c RP:%c PO@%d CL@%d",
                 (int)fpga.probe_id_bit,
                 fpga.probe_all_same ? 'Y' : 'N',
                 fpga.probe_repeats  ? 'Y' : 'N',
                 (int)fpga.probe_id_bit_post,
                 (int)fpga.probe_id_bit_close);
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

    /* Line 1 (green): transport counters from the real FPGA path */
    /* Line 1 (green): post-upload scope-engine config readback.
     * SS = fpga.scope_status[] from the 0x03 status read — stock capture
     * returns 00 01 42 2E. CL = 0x3A close status — stock returns F8.
     * R  = first 4 raw CH1 bytes (pre-cal), to see if samples vary at all. */
    snprintf(buf, sizeof(buf), "SS:%02X%02X%02X%02X CL:%02X R:%02X%02X%02X%02X",
             fpga.scope_status[0], fpga.scope_status[1],
             fpga.scope_status[2], fpga.scope_status[3],
             fpga.h2_close_status,
             fpga.diag_ch1_raw[0], fpga.diag_ch1_raw[1],
             fpga.diag_ch1_raw[2], fpga.diag_ch1_raw[3]);
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 2, buf,
                     0x07E0, 0x0000, &font_small);  /* green */

    /* Line 2 (cyan): acquisition state and critical control pins */
    snprintf(buf, sizeof(buf), "OK:%u TO:%u 1:%02X P0:%c P6:%c B:%c",
             (unsigned)fpga.spi3_ok_count,
             (unsigned)fpga.spi3_total_timeouts,
             fpga.spi3_first_byte,
             (GPIOC->idt & (1 << 0)) ? 'H' : 'L',
             (GPIOC->odt & (1 << 6)) ? 'H' : 'L',
             (GPIOB->odt & (1 << 11)) ? 'H' : 'L');
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 15, buf,
                     0x07FF, 0x0000, &font_small);  /* cyan */

    /* Line 3 (yellow): Init handshake responses + raw PB4 state
     * G1[0-3] = sync+0x05+pad, G2[4-6] = 0x12+pad, G3[7-10] = 0x15+pad+0x3B
     * PB4: raw GPIO read of the MISO pin (1=HIGH/floating, 0=driven LOW) */
    /* L/H = min/max across the whole CH1 acquisition buffer. If L==H the
     * samples are a stuck constant (analog/trigger problem); if they spread,
     * real signal is arriving and the fault is downstream in rendering. */
    {
        const volatile uint8_t *cb = fpga_get_ch1_buf();
        uint8_t lo = 0xFF, hi = 0x00;
        for (uint32_t i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
            uint8_t v = cb[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        /* CFG = Gowin STATUS_REGISTER (opcode 0x41) read right after the 0x3A
         * close — the authoritative "did the upload take?" verdict. Read at a
         * forced /256 since 2026-07-28; every earlier value was clocked at /2
         * and slipped one bit (the persistent 80 01 C8 10 is 0x00039020 read
         * one bit early — see fpga.c [6b]).
         *
         * D = DONE_FINAL (bit 13) — 1 means configuration completed. This, not
         *     SYSTEM_EDIT_MODE, is the success signal (Exp H showed stock reads
         *     the same 0x00039020 at the config-enable instant that we do, so
         *     ED cannot discriminate).
         * E = the error nibble, bits 3..0 = TIMEOUT | ID_VERIFY_FAILED |
         *     BAD_COMMAND | CRC_ERROR. Non-zero means bytes DID reach the config
         *     engine and were rejected → a content/transport bug, localizable.
         *     Zero with D=0 means the engine never received anything at all. */
        uint32_t sr = ((uint32_t)fpga.cfg_status_reg[0] << 24) |
                      ((uint32_t)fpga.cfg_status_reg[1] << 16) |
                      ((uint32_t)fpga.cfg_status_reg[2] << 8)  |
                      ((uint32_t)fpga.cfg_status_reg[3]);
        /* A = the post-0x3A IDCODE anchor: the bit offset at which 0x0120681B was
         *     found in the read taken immediately before CFG, at the same clock.
         *     A>=0  -> CFG was read on a validated path AND the config port is
         *             still open, i.e. we are definitively NOT configured.
         *     A=-1  -> either the port closed (Exp L: what a successfully
         *             configured part does) or the bus is dead. CFG is then
         *             UNTRUSTWORTHY on its own — corroborate with a live trace. */
        snprintf(buf, sizeof(buf), "CFG:%08lX D%u E%X A%d L%u H%u",
                 (unsigned long)sr,
                 (unsigned)((sr >> 13) & 1u),
                 (unsigned)(sr & 0x0Fu),
                 (int)fpga.probe_id_bit_close,
                 (unsigned)lo, (unsigned)hi);
    }
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 28, buf,
                     0xFFE0, 0x0000, &font_small);  /* yellow */

    /* Line 4 (magenta): SPI3 CTRL1 + the EDIT_MODE probe + H2 upload flag.
     * S1 = SPI3 CTRL1 as latched during the config frame. Bits[5:3] are BR, so
     *      0347 = /2 (stock, Exp F fidelity build) and 037F = /256. This is the
     *      on-device proof of which clock the config frame actually ran at.
     * ED = STATUS_REGISTER (0x41) read at /256 immediately after 0x15.
     *      SYSTEM_EDIT_MODE is bit 7 of the ASSEMBLED 32-bit word, i.e. bit 7 of
     *      ED[3] — NOT bit 7 of ED[0], as this comment and scripts/swd_fpga_status.sh
     *      both said until 2026-07-28. (Same verdict either way for 0x00039020:
     *      ED[3]=0x20, bit 7 clear. But the test was reading bit 31.)
     *      Exp H demoted this from "the wall" to a non-discriminator: stock reads
     *      the same 0x00039020 here and still configures. Compare CFG's D flag.
     * Dropped S2/ST here — CTRL2 and STS were static (0003 / 0002) across every
     * run so far, and ED is the number this experiment turns on. */
    snprintf(buf, sizeof(buf), "S1:%04X ED:%02X%02X%02X%02X H2:%c",
             (uint16_t)fpga.diag_spi_ctrl1,
             fpga.edit_mode_status[0], fpga.edit_mode_status[1],
             fpga.edit_mode_status[2], fpga.edit_mode_status[3],
             fpga.h2_upload_done ? 'Y' : 'N');
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 41, buf,
                     0xF81F, 0x0000, &font_small);  /* magenta */
}

#endif /* SCOPE_DEBUG_OVERLAY */

void draw_scope_screen(uint32_t frame)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();

    /* Clear waveform area */
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, th->background);

    /* Layer 1: Persistence overlay (under everything) */
    draw_persistence_overlay(frame);

    /* Layer 2: Grid */
    draw_scope_grid();

    /* Layer 3: Ground reference markers */
    draw_ground_markers(ss, th);

    /* Layer 4: Trigger level indicator */
    draw_trigger_indicator(ss, th);

    /* Layer 5: Waveform */
    draw_demo_waveform(frame);

    /* Layer 6: Math channel overlay */
    draw_math_waveform(frame);

    /* Layer 7: Cursor lines */
    draw_cursors();

    /* Layer 8: Trigger status badge */
    draw_trigger_status(ss, th);

    /* Layer 9: Run/Stop */
    draw_run_stop(ss, th);

    /* Layer 10: Measurement badges */
    draw_measurement_badges(ss, th);

    /* Layer 11: Quick-change popup (on top of everything) */
    draw_popup(th);

    /* Active channel indicator (top-left) */
    const char *ch_label = (active_channel == 0) ? "CH1" : "CH2";
    uint16_t ch_color = (active_channel == 0) ? th->ch1 : th->ch2;
    font_draw_string(4, SCOPE_TOP + 2, ch_label, ch_color, ch_color, &font_small);

#ifndef EMULATOR_BUILD
    /* SPI3 acquisition diagnostic — small text overlay top-center.
     * Shows probing status so we can see FPGA data flow without serial. */
    {
        char spi3_buf[24];
        uint16_t spi3_color;
        if (fpga.spi3_ok_count > 0) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:OK %u",
                     (unsigned)fpga.spi3_ok_count);
            spi3_color = th->success;
        } else if (fpga.spi3_total_timeouts > 0) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:-- %u [%02X]",
                     (unsigned)fpga.spi3_total_timeouts,
                     fpga.spi3_first_byte);
            spi3_color = th->warning;
        } else if (fpga.initialized) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:wait");
            spi3_color = th->text_secondary;
        } else {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:off");
            spi3_color = th->text_secondary;
        }
        font_draw_string(40, SCOPE_TOP + 2, spi3_buf,
                         spi3_color, th->background, &font_small);
    }
#endif

#ifdef SCOPE_DEBUG_OVERLAY
    /* Layer 12: SPI3 debug overlay (bottom of screen) */
    scope_dbg_strip_dirty = 1;   /* the :1050-style full clear wiped 180-224 */
    draw_scope_debug(th);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * Live incremental frame (2026-08-12 rendering pass)
 *
 * Flicker-free trace update for the real-data path: repaints the waveform
 * band column-by-column, compositing each pixel's FINAL color (trace over
 * trigger line over crosshair over grid over background) into one streamed
 * window write per column — the screen never holds a blank state, unlike
 * draw_scope_screen's clear-then-redraw, which is the visible flash. Grid
 * pixels are re-plotted arithmetically (there is no framebuffer to restore
 * from; pattern mirrors draw_scope_grid exactly). Cost: one lcd_set_window
 * (~13 us) per column + streamed pixel writes ≈ 6-8 ms per frame — 20 fps
 * capable, vs the full path's blank-flash at any rate.
 *
 * Decorations (badges, ground markers, cursors, popups, trigger arrow) are
 * NOT drawn here. main.c falls back to draw_scope_screen whenever a popup
 * is active or cursors are on, and button DCMDs still force full redraws,
 * so those layers refresh exactly when they can change.
 * ═══════════════════════════════════════════════════════════════════ */
void draw_scope_live_frame(void)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();
    const volatile uint8_t *b1 = fpga_get_ch1_buf();
    const volatile uint8_t *b2 = fpga_get_ch2_buf();

    if (!b1 || !b2) return;

    /* The live band stops where fixed furniture begins: the debug strip
     * (debug builds) or the measurement badge rows. Those regions repaint
     * themselves; streaming over them would z-fight at frame rate. */
#ifdef SCOPE_DEBUG_OVERLAY
    const uint16_t band_bot = SCOPE_DBG_Y;
#else
    const uint16_t band_bot = BADGE_ROW2_Y;
#endif
    const uint16_t band_h = band_bot - SCOPE_TOP;

    /* Trigger dotted line, exactly as draw_trigger_indicator places it. */
    int16_t trig_y = SCOPE_MID_Y - ss->trigger.level;
    if (trig_y < SCOPE_TOP + 2) trig_y = SCOPE_TOP + 2;
    if (trig_y > SCOPE_BOT - 3) trig_y = SCOPE_BOT - 3;

    const int16_t off1 = ss->ch1.position;
    const int16_t off2 = ss->ch2.position;
    const bool en1 = ss->ch1.enabled;
    const bool en2 = ss->ch2.enabled;

    int16_t p1 = 0, p2 = 0;  /* previous column's y — vertical continuity */

    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        /* Same y-transform as the full path (scope_ui.c real-data plot). */
        int16_t y1 = SCOPE_MID_Y - (((int16_t)b1[x] - 128) * SCOPE_H / 256) - off1;
        int16_t y2 = SCOPE_MID_Y - (((int16_t)b2[x] - 128) * SCOPE_H / 256) - off2;

        /* 2-px dot plus a span to the previous sample, so steep edges draw
         * as connected verticals instead of the full path's dotted gaps. */
        int16_t lo1 = y1, hi1 = y1 + 1, lo2 = y2, hi2 = y2 + 1;
        if (x > 0) {
            if (p1 < lo1) lo1 = p1;
            if (p1 > hi1) hi1 = p1;
            if (p2 < lo2) lo2 = p2;
            if (p2 > hi2) hi2 = p2;
        }
        p1 = y1;
        p2 = y2;

        lcd_set_window(x, SCOPE_TOP, 1, band_h);
        for (uint16_t y = SCOPE_TOP; y < band_bot; y++) {
            uint16_t c;
            /* Z-order matches the full path: CH2 painted after CH1 there,
             * so CH2 wins here; trace over trigger line over grid. */
            if (en2 && (int16_t)y >= lo2 && (int16_t)y <= hi2)
                c = th->ch2;
            else if (en1 && (int16_t)y >= lo1 && (int16_t)y <= hi1)
                c = th->ch1;
            else if ((int16_t)y == trig_y && (x & 3u) == 0 && x < LCD_WIDTH - 6)
                c = th->trigger;
            else if (y == SCOPE_MID_Y || x == LCD_WIDTH / 2)
                c = th->grid_center;
            else if (((x & 31u) == 0 && (y & 1u) == 0) ||
                     (((y - SCOPE_TOP) % 26u) == 0 && (x & 1u) == 0))
                c = th->grid;
            else
                c = th->background;
            lcd_write_data(c);
        }
    }

#ifdef SCOPE_DEBUG_OVERLAY
    /* Keep the counters moving; opaque fixed-width lines, no strip refill. */
    draw_scope_debug(th);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * FFT views (unchanged — reference scope_state for consistency)
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef FEATURE_FFT

static void format_freq(float freq_hz, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (freq_hz >= 1000000.0f) {
        val = freq_hz / 1000000.0f;
        unit = "MHz";
    } else if (freq_hz >= 1000.0f) {
        val = freq_hz / 1000.0f;
        unit = "kHz";
    } else {
        val = freq_hz;
        unit = "Hz";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 10.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + integer / 100);
    if (integer >= 10  && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac);

    while (*unit && pos < bufsize - 1)
        buf[pos++] = *unit++;
    buf[pos] = '\0';
}

/* ── Display reference auto-ranging ─────────────────────────────────
 *
 * fft_process() reports magnitude in dB relative to ONE ADC COUNT
 * (20*log10(|X| * 2/N), |X| in counts) — not dBFS. A full-scale signal
 * therefore peaks near 20*log10(32767) = 90 dB, and the configured default
 * ref_level_db = 0.0 puts the top of the display ~90 dB BELOW the signal:
 * every populated bin clamps to normalized = 0, i.e. the top colour of the
 * ramp. Measured on the host with the same synthetic 1 kHz square the UI
 * feeds itself (scratch probe, 2026-08-13): peak = 84.4 dB, and 318 of the
 * 320 waterfall columns land in the RED band. That is the "hot red field
 * with thin nulls" seen on the bench the same day.
 *
 * Fix: track the observed peak instead of hard-coding a magic 90. The
 * reference follows frame_max + headroom, moving up immediately (so a
 * growing signal never clips against the top) and easing down slowly
 * through a dead band (so a jittering peak does not make the colours
 * breathe). cfg->ref_level_db is preserved as a USER TRIM applied on top,
 * which keeps fft_adjust_ref_level() meaningful.
 *
 * Known limit, stated rather than hidden: this ranges off the peak only. A
 * spectrum with no dynamic range (broadband noise and nothing else) has its
 * mean within ~10-15 dB of its peak and will still render as a flat, fairly
 * hot field. That is a true statement about such a signal; distinguishing it
 * would need a floor estimator as well, which is not warranted until real
 * captured data (rather than the synthetic square below) drives this view.
 */
#define FFT_REF_HEADROOM_DB   6.0f   /* keep the peak just below the top    */
#define FFT_REF_DEAD_BAND_DB  3.0f   /* ignore downward wobble below this   */
#define FFT_REF_DECAY         0.10f  /* per-frame fraction on the way down  */

static float fft_ref_tracked = 0.0f;
static bool  fft_ref_primed  = false;

static float fft_display_ref(const fft_config_t *cfg, const float *data,
                             uint16_t start_bin, uint16_t end_bin)
{
    if (end_bin >= FFT_BINS) end_bin = FFT_BINS - 1;
    if (start_bin > end_bin) start_bin = end_bin;

    float peak = data[start_bin];
    uint16_t b;
    for (b = (uint16_t)(start_bin + 1); b <= end_bin; b++)
        if (data[b] > peak) peak = data[b];

    float target = peak + FFT_REF_HEADROOM_DB;

    if (!fft_ref_primed) {
        fft_ref_tracked = target;
        fft_ref_primed = true;
    } else if (target > fft_ref_tracked) {
        fft_ref_tracked = target;                       /* attack: immediate */
    } else if (fft_ref_tracked - target > FFT_REF_DEAD_BAND_DB) {
        fft_ref_tracked += (target - fft_ref_tracked) * FFT_REF_DECAY;
    }

    return fft_ref_tracked + cfg->ref_level_db;
}

static void draw_fft_region(uint16_t y_top, uint16_t height)
{
    const fft_config_t *cfg = fft_get_config();
    uint16_t y_bot = y_top + height;

    int16_t *sbuf = fft_get_sample_buf();
    if (!sbuf) return;  /* FFT not initialized */
    test_signal_generate(TEST_SIG_SQUARE, sbuf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);
    fft_process(sbuf, FFT_SIZE, &fft_result);

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.magnitude_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    uint16_t x;

    /* Same auto-ranged reference the waterfall uses: with the configured
     * ref_level_db = 0 every bar pinned to full height, because the dB scale
     * is relative to one count, not to full scale. See fft_display_ref(). */
    float ref_db = fft_display_ref(cfg, draw_data, zoom_start, zoom_end);
    float range_db = cfg->db_range;

    for (x = 0; x < LCD_WIDTH; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / LCD_WIDTH);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;

        float db = draw_data[bin];
        float normalized = (ref_db - db) / range_db;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        uint16_t bar_top = y_top + (uint16_t)(normalized * (float)height);
        if (bar_top < y_bot) {
            uint16_t color;
            if (normalized < 0.25f) color = COLOR_CH1;
            else if (normalized < 0.5f) color = COLOR_GREEN;
            else if (normalized < 0.75f) color = COLOR_CYAN;
            else color = COLOR_GRID;
            lcd_fill_rect(x, bar_top, 1, y_bot - bar_top, color);
        }

        if (fft_result.max_hold_db != NULL) {
            float mh_norm = (ref_db - fft_result.max_hold_db[bin]) / range_db;
            if (mh_norm >= 0.0f && mh_norm <= 1.0f) {
                uint16_t mh_y = y_top + (uint16_t)(mh_norm * (float)height);
                if (mh_y >= y_top && mh_y < y_bot && (x & 1) == 0)
                    lcd_set_pixel(x, mh_y, COLOR_RED);
            }
        }
    }

    /* Horizontal grid lines (every 10dB) */
    float db;
    for (db = ref_db; db > ref_db - range_db; db -= 10.0f) {
        float normalized = (ref_db - db) / range_db;
        uint16_t y = y_top + (uint16_t)(normalized * (float)height);
        if (y > y_top && y < y_bot)
            for (x = 0; x < LCD_WIDTH; x += 4)
                lcd_set_pixel(x, y, COLOR_GRID);
    }

    if (height > 80) {
        for (db = ref_db; db > ref_db - range_db; db -= 20.0f) {
            float normalized = (ref_db - db) / range_db;
            uint16_t y = y_top + (uint16_t)(normalized * (float)height);
            if (y > y_top + 8 && y < y_bot - 8) {
                int db_int = (int)db;
                char label[8];
                int pos = 0;
                if (db_int < 0) { label[pos++] = '-'; db_int = -db_int; }
                if (db_int >= 100) label[pos++] = (char)('0' + db_int / 100);
                if (db_int >= 10)  label[pos++] = (char)('0' + (db_int / 10) % 10);
                label[pos++] = (char)('0' + db_int % 10);
                label[pos] = '\0';
                font_draw_string(2, y - 5, label, COLOR_GRAY, COLOR_GRAY, &font_small);
            }
        }
    }

    uint8_t p;
    for (p = 0; p < fft_result.num_peaks && p < 3; p++) {
        uint16_t peak_bin = fft_result.peaks[p].bin;
        if (peak_bin < zoom_start || peak_bin > zoom_end) continue;

        uint16_t peak_x = (uint16_t)((uint32_t)(peak_bin - zoom_start)
                          * LCD_WIDTH / zoom_span);
        float norm = (ref_db - fft_result.peaks[p].magnitude_db) / range_db;
        if (norm < 0.0f) norm = 0.0f;
        uint16_t peak_y = y_top + (uint16_t)(norm * (float)height);

        if (peak_y >= y_top + 4 && peak_x > 2 && peak_x < LCD_WIDTH - 2) {
            lcd_set_pixel(peak_x, peak_y - 3, COLOR_RED);
            lcd_set_pixel(peak_x - 1, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x + 1, peak_y - 2, COLOR_RED);
        }

        if (p == 0) {
            char freq_str[16];
            format_freq(fft_result.peaks[0].freq_hz, freq_str, sizeof(freq_str));
            font_draw_string(4, y_top + 2, freq_str, COLOR_WHITE, COLOR_WHITE, &font_small);
        }

        if (fft_result.peaks[p].label[0] != '\0' && peak_x > 8 && peak_x < LCD_WIDTH - 30) {
            font_draw_string(peak_x - 8, peak_y - 12,
                             fft_result.peaks[p].label, COLOR_ORANGE, COLOR_ORANGE, &font_small);
        }
    }

    const char *win_names[] = { "Rect", "Hann", "Hamm", "BHar", "Flat" };
    const char *win_name = (cfg->window < FFT_WINDOW_COUNT)
                           ? win_names[cfg->window] : "?";
    font_draw_string_right(LCD_WIDTH - 4, y_top + 2, win_name,
                           COLOR_GRAY, COLOR_GRAY, &font_small);
}

void draw_fft_screen(void)
{
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);
    draw_fft_region(SCOPE_TOP + 2, SCOPE_H - 4);
}

void draw_split_screen(uint32_t frame)
{
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);

    uint16_t scope_top = SCOPE_TOP;
    uint16_t scope_bot = SCOPE_TOP + SCOPE_H / 2 - 1;
    uint16_t scope_mid = (scope_top + scope_bot) / 2;
    uint16_t x;

    for (x = 0; x < LCD_WIDTH; x += 32)
        for (uint16_t y = scope_top; y < scope_bot; y += 2)
            lcd_set_pixel(x, y, COLOR_GRID);
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, scope_mid, COLOR_GRID_CENTER);

    static const int8_t sin_lut[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };
    for (x = 0; x < LCD_WIDTH; x++) {
        uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
        int16_t wy = scope_mid - (sin_lut[idx] * 25 / 100);
        if (wy >= (int16_t)scope_top && wy < (int16_t)scope_bot)
            lcd_set_pixel(x, (uint16_t)wy, COLOR_CH1);
    }

    uint16_t divider_y = SCOPE_TOP + SCOPE_H / 2;
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, divider_y, COLOR_GRID_CENTER);

    draw_fft_region(divider_y + 1, SCOPE_H / 2 - 2);
}

/* Waterfall
 *
 * The 20 KB history buffer lives in the shared pool as a sub-tenant of the
 * FFT region (see SHMEM_FFT_WATERFALL_OFFSET) rather than in .bss. It cannot
 * be a pool owner of its own: this function calls fft_process(), so it only
 * ever runs while SHMEM_OWNER_FFT holds the pool, and claiming it separately
 * would evict the FFT it depends on.
 *
 * Only waterfall_row_idx, the generation counter and one assembled screen
 * line stay static — 645 bytes instead of 20,480.
 */
#define WATERFALL_ROWS  SHMEM_FFT_WATERFALL_ROWS
#define WATERFALL_COLS  SHMEM_FFT_WATERFALL_COLS
static uint8_t waterfall_row_idx = 0;
static uint32_t waterfall_pool_generation = 0;

/* The row blit below sets one window across the full plot width per history
 * row and streams into it, so a history row must be exactly one screen line
 * wide. */
_Static_assert(WATERFALL_COLS == LCD_WIDTH,
               "waterfall row blit assumes one history column per screen pixel");

/*
 * One assembled screen line, 640 bytes. Colours are converted once per
 * history row and pushed row_height times, instead of being recomputed for
 * every sub-row. It is deliberately NOT the 20 KB history buffer's problem:
 * that lives in the shared pool (see shared_mem.h) precisely because it is
 * large, whereas one line is small enough that the pool's ownership
 * lifetime rules would cost more than they save.
 */
static uint16_t waterfall_line[WATERFALL_COLS];

/*
 * Intensity -> colour, with a BLACK floor.
 *
 * intensity 0 = at/above the display reference, 255 = at/below the floor
 * (reference - db_range). The old ramp ended at pure blue, so the noise
 * floor painted the plot solid blue and only the very top 20% of the scale
 * was red; combined with the reference bug (see fft_display_ref) that made
 * the whole plot one flat colour. This one runs black -> blue -> cyan ->
 * yellow -> red in four equal 64-step segments, which is the conventional
 * spectrogram ordering and puts "nothing here" at black.
 */
static uint16_t intensity_to_color(uint8_t intensity)
{
    uint16_t level = (uint16_t)(255u - intensity);   /* 0 = floor, 255 = hot */
    uint8_t  t = (uint8_t)((level & 63u) * 4u);      /* position in segment  */

    if (level < 64)  return RGB565(0, 0, t);                        /* black->blue  */
    if (level < 128) return RGB565(0, t, 255);                      /* blue ->cyan  */
    if (level < 192) return RGB565(t, 255, (uint8_t)(255 - t));     /* cyan ->yellow*/
    return RGB565(255, (uint8_t)(255 - t), 0);                      /* yellow->red  */
}

void draw_waterfall_screen(void)
{
    const fft_config_t *cfg = fft_get_config();

    int16_t *sbuf = fft_get_sample_buf();
    if (!sbuf) return;  /* FFT not initialized */
    test_signal_generate(TEST_SIG_SQUARE, sbuf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);
    fft_process(sbuf, FFT_SIZE, &fft_result);

    /* Resolve the history buffer out of the FFT's pool tenancy. Returns NULL
     * if FFT no longer owns the pool, in which case there is nothing valid to
     * draw into — bail rather than scribble over another owner's data. This is
     * a stricter check than the fft_get_sample_buf() guard above, which only
     * proves the FFT was initialised at some point. */
    uint8_t *wf_pool = shared_mem_get(SHMEM_OWNER_FFT);
    if (!wf_pool) return;
    uint8_t (*waterfall_buf)[WATERFALL_COLS] =
        (uint8_t (*)[WATERFALL_COLS])(wf_pool + SHMEM_FFT_WATERFALL_OFFSET);

    /* shared_mem_acquire() zeroes the pool whenever the owner changes, so a
     * round trip through screenshot/persistence wipes the history. Detect that
     * and restart from row 0 instead of scrolling through a zeroed tail.
     *
     * Refill with 0xFF rather than leaving the pool's zeros: intensity 0 maps
     * to the top of the ramp (red = at the display reference), so zeroed rows
     * would render as maximum signal. 0xFF is the other end of the ramp
     * (black = reference - db_range = the noise floor), which is what "no data
     * yet" should look like. Bench 2026-08-13: with the zero fill, entering
     * WFALL painted the whole plot red and the history grew downward
     * through it. */
    uint32_t pool_generation = shared_mem_transition_count();
    if (pool_generation != waterfall_pool_generation) {
        waterfall_pool_generation = pool_generation;
        waterfall_row_idx = 0;
        memset(waterfall_buf, 0xFF, SHMEM_FFT_WATERFALL_SIZE);
    }

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.magnitude_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    if (zoom_span == 0) zoom_span = 1;

    float ref_db = fft_display_ref(cfg, draw_data, zoom_start, zoom_end);

    uint16_t x;
    for (x = 0; x < WATERFALL_COLS; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / WATERFALL_COLS);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;
        float db = draw_data[bin];
        float normalized = (ref_db - db) / cfg->db_range;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        waterfall_buf[waterfall_row_idx][x] = (uint8_t)(normalized * 255.0f);
    }

    uint8_t newest_row = waterfall_row_idx;
    waterfall_row_idx = (uint8_t)((waterfall_row_idx + 1) % WATERFALL_ROWS);

    /* ── Blit ───────────────────────────────────────────────────────
     *
     * This used to clear the whole plot and then draw every pixel column as
     * its own 1 x row_height lcd_fill_rect: 20,481 fill_rects and therefore
     * 20,481 lcd_set_window() calls per frame, each one 3 commands + 8 byte
     * writes with a bus delay after every single one — ~225,000 delayed bus
     * transactions to paint 61,440 pixels. Bench 2026-08-13: the plot
     * visibly rastered left-to-right, top-to-bottom over seconds.
     *
     * Now: one window per history row, and each row is converted once into
     * waterfall_line and streamed row_height times. 65 set_windows and 192
     * lcd_write_pixels() calls per frame, with the same pixels at the same
     * coordinates.
     *
     * One window for the WHOLE block would cost 2 set_windows instead of 65,
     * but the window is global driver state and vInputTask runs at priority 4
     * against the display task's 1: its power-off countdown overlay
     * (input_handler.c) calls lcd_fill_rect() and so re-points the window
     * from under a stream in progress. Per-row windows bound that to one
     * corrupted row for one frame — the same blast radius the per-column
     * version had — for ~700 extra bus transactions out of 66,000.
     *
     * The clear is gone too: the block covers every pixel it draws over, so
     * the only part of the plot that still needs clearing is the residual
     * strip left when SCOPE_H is not a multiple of WATERFALL_ROWS (14 rows
     * at the current 206/64). That halves the pixel writes as well.
     *
     * Why not scroll-and-append, which would be O(1 row)? The history
     * scrolls by exactly one row per frame, so 63 of 64 rows are unchanged
     * in CONTENT — but they all MOVE, and with no framebuffer on this side
     * of the bus there is nothing to shift; the ST7789's own vertical
     * scroll works on panel rows, which MADCTL = 0xA0 (MV = 1) maps to
     * screen COLUMNS, i.e. it would scroll the waterfall sideways. An
     * O(1)-per-frame waterfall needs either a RAM framebuffer (the shared
     * pool is already spoken for by the FFT here) or a different history
     * layout that lets the newest row wrap in place instead of the image
     * scrolling — which changes what is drawn, so it is a design decision,
     * not a speedup. */
    uint16_t row_height = SCOPE_H / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    uint16_t rows_drawn = WATERFALL_ROWS;
    if ((uint32_t)rows_drawn * row_height > SCOPE_H)
        rows_drawn = (uint16_t)(SCOPE_H / row_height);
    uint16_t block_h = (uint16_t)(rows_drawn * row_height);

    if (block_h < SCOPE_H)
        lcd_fill_rect(0, (uint16_t)(SCOPE_TOP + block_h), LCD_WIDTH,
                      (uint16_t)(SCOPE_H - block_h), COLOR_BLACK);

    uint16_t r;
    for (r = 0; r < rows_drawn; r++) {
        const uint8_t *src =
            waterfall_buf[(newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS];
        for (x = 0; x < WATERFALL_COLS; x++)
            waterfall_line[x] = intensity_to_color(src[x]);

        lcd_set_window(0, (uint16_t)(SCOPE_TOP + r * row_height),
                       WATERFALL_COLS, row_height);

        uint16_t rep;
        for (rep = 0; rep < row_height; rep++)
            lcd_write_pixels(waterfall_line, WATERFALL_COLS);
    }

    font_draw_string(4, SCOPE_TOP + 2, "WFALL", COLOR_WHITE, COLOR_WHITE, &font_small);
}

#endif /* FEATURE_FFT */
