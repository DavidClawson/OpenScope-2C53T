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

#define SCOPE_DBG_Y   (LCD_HEIGHT - 48)
#define SCOPE_DBG_H   44

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

    /* In-context SPI3 "MISO finder" — slow SPI clock to catch all ports.
     * Normal SPI at 30-60MHz is too fast to sample during a transfer.
     * Temporarily switch to /256 prescaler (~1MHz) so each byte takes
     * ~8μs = ~1920 CPU cycles, giving us hundreds of GPIO samples. */
    {
        #define SPI3_REG ((spi_type *)0x40003C00)
        static uint16_t spi_xor_a = 0, spi_xor_b = 0, spi_xor_c = 0;
        static uint16_t spi_xor_d = 0, spi_xor_e = 0;

        /* Slow down SPI3: disable, set /256, re-enable */
        SPI3_REG->ctrl1 &= ~(1 << 6);  /* SPE off */
        uint32_t saved_ctrl1 = SPI3_REG->ctrl1;
        SPI3_REG->ctrl1 = (saved_ctrl1 & ~(0x7u << 3)) | (0x7u << 3);  /* MDIV=/256 */
        SPI3_REG->ctrl1 |= (1 << 6);   /* SPE on */

        /* Snapshot all ports BEFORE transfer */
        uint16_t a0 = (uint16_t)GPIOA->idt;
        uint16_t b0 = (uint16_t)GPIOB->idt;
        uint16_t c0 = (uint16_t)GPIOC->idt;
        uint16_t d0 = (uint16_t)GPIOD->idt;
        uint16_t e0 = (uint16_t)GPIOE->idt;

        /* Assert CS and transfer 4 bytes at slow speed */
        GPIOB->clr = (1 << 6);  /* CS LOW */

        for (int byte = 0; byte < 4; byte++) {
            while (!(SPI3_REG->sts & (1 << 1))) {}  /* Wait TDBE */
            SPI3_REG->dt = 0xFF;

            /* Sample all ports — at ~1MHz we get hundreds of samples per byte */
            for (int i = 0; i < 500; i++) {
                spi_xor_a |= (uint16_t)GPIOA->idt ^ a0;
                spi_xor_b |= (uint16_t)GPIOB->idt ^ b0;
                spi_xor_c |= (uint16_t)GPIOC->idt ^ c0;
                spi_xor_d |= (uint16_t)GPIOD->idt ^ d0;
                spi_xor_e |= (uint16_t)GPIOE->idt ^ e0;
            }

            while (!(SPI3_REG->sts & (1 << 0))) {}  /* Wait RDBF */
            (void)SPI3_REG->dt;
        }

        GPIOB->scr = (1 << 6);  /* CS HIGH */

        /* Restore normal SPI speed */
        SPI3_REG->ctrl1 &= ~(1 << 6);  /* SPE off */
        SPI3_REG->ctrl1 = saved_ctrl1;  /* Restore original MDIV */
        SPI3_REG->ctrl1 |= (1 << 6);   /* SPE on */

        /* Merge into toggle maps */
        gpio_toggle_a |= spi_xor_a;
        gpio_toggle_b |= spi_xor_b;
        gpio_toggle_c |= spi_xor_c;
        gpio_toggle_d |= spi_xor_d;
        gpio_toggle_e |= spi_xor_e;

        pb3_toggle_count = (spi_xor_b >> 3) & 1;  /* 1 if SCK seen toggling */
        pb4_toggle_count = (spi_xor_b >> 4) & 1;  /* 1 if MISO seen toggling */
    }

    gpio_prev_a = a; gpio_prev_b = b;
    gpio_prev_c = c; gpio_prev_d = d; gpio_prev_e = e;
    gpio_scan_started = true;
}

static void draw_scope_debug(const theme_t *th)
{
    /* Update toggle masks */
    gpio_scan_update();

    /* Dark background strip */
    lcd_fill_rect(0, SCOPE_DBG_Y, LCD_WIDTH, SCOPE_DBG_H, 0x0000);

    char buf[64];

    /* Line 1 (green): PA1 analysis + SPI3 GMUX remap register
     * PA1T: total PA1 toggles (if ~4Hz = USART RTS, if faster = data)
     * R5: IOMUX remap5 — bits[6:4] = SPI3_GMUX (000=PB3/4/5, 001=PC10/11/12) */
    /* SCK/MISO toggle counts + SPI3 ok counter */
    snprintf(buf, sizeof(buf), "SCK:%lu MI:%lu OK:%u [%02X]",
             (unsigned long)pb3_toggle_count,
             (unsigned long)pb4_toggle_count,
             (unsigned)fpga.spi3_ok_count,
             fpga.spi3_first_byte);
    font_draw_string(2, SCOPE_DBG_Y + 2, buf,
                     0x07E0, 0x0000, &font_small);  /* green */

    /* Line 2 (cyan): GPIO toggle map — find the REAL MISO pin!
     * Now that SCK works, FPGA may be sending data on an unexpected pin */
    snprintf(buf, sizeof(buf), "A:%04X B:%04X C:%04X",
             gpio_toggle_a, gpio_toggle_b, gpio_toggle_c);
    font_draw_string(2, SCOPE_DBG_Y + 15, buf,
                     0x07FF, 0x0000, &font_small);  /* cyan */

    /* Line 3 (cyan): ports D, E + live PB */
    snprintf(buf, sizeof(buf), "D:%04X E:%04X PB:%04X",
             gpio_toggle_d, gpio_toggle_e,
             (uint16_t)GPIOB->idt);
    font_draw_string(2, SCOPE_DBG_Y + 28, buf,
                     0x07FF, 0x0000, &font_small);  /* cyan */
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

    float ref_db = cfg->ref_level_db;
    float range_db = cfg->db_range;
    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    uint16_t x;

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

/* Waterfall */
#define WATERFALL_ROWS  64
#define WATERFALL_COLS  320
static uint8_t waterfall_buf[WATERFALL_ROWS][WATERFALL_COLS];
static uint8_t waterfall_row_idx = 0;

static uint16_t intensity_to_color(uint8_t intensity)
{
    uint8_t inv = 255 - intensity;
    if (inv > 204) return COLOR_RED;
    if (inv > 153) return RGB565(255, (uint8_t)((inv - 153) * 5), 0);
    if (inv > 102) return RGB565((uint8_t)((153 - inv + 102) * 5), 255, 0);
    if (inv > 51)  return RGB565(0, 255, (uint8_t)((inv - 51) * 5));
    return RGB565(0, (uint8_t)(inv * 5), 255);
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

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.magnitude_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    if (zoom_span == 0) zoom_span = 1;

    uint16_t x;
    for (x = 0; x < WATERFALL_COLS; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / WATERFALL_COLS);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;
        float db = draw_data[bin];
        float normalized = (cfg->ref_level_db - db) / cfg->db_range;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        waterfall_buf[waterfall_row_idx][x] = (uint8_t)(normalized * 255.0f);
    }

    uint8_t newest_row = waterfall_row_idx;
    waterfall_row_idx = (uint8_t)((waterfall_row_idx + 1) % WATERFALL_ROWS);

    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);

    uint16_t row_height = SCOPE_H / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    uint8_t r;
    for (r = 0; r < WATERFALL_ROWS; r++) {
        uint8_t buf_row = (uint8_t)((newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS);
        uint16_t y = (uint16_t)(SCOPE_TOP + r * row_height);
        if (y + row_height > SCOPE_BOT) break;
        for (x = 0; x < WATERFALL_COLS; x++)
            lcd_fill_rect(x, y, 1, row_height, intensity_to_color(waterfall_buf[buf_row][x]));
    }

    font_draw_string(4, SCOPE_TOP + 2, "WFALL", COLOR_WHITE, COLOR_WHITE, &font_small);
}

#endif /* FEATURE_FFT */
