/*
 * OpenScope 2C53T - Fuse Current Tester UI
 *
 * Estimates circuit current by measuring millivolt-level voltage drop
 * across an in-place automotive fuse.  Three views:
 *
 *   Detail — Single fuse selected, shows calculated current + bar
 *   Multi  — All ratings for selected fuse type at once
 *   Scan   — Pass/fail for parasitic draw hunting
 *
 * Button mapping (when meter_layout == METER_LAYOUT_FUSE):
 *   UP/DOWN    — Change fuse rating
 *   LEFT/RIGHT — Change fuse type
 *   SELECT     — Cycle view (Detail → Multi → Scan)
 *   OK         — Cycle meter layout (back to Full)
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "fuse_table.h"
#include <string.h>

/* Layout constants */
#define FUSE_TOP        18          /* Below status bar */
#define FUSE_BOTTOM     (LCD_HEIGHT - 18)
#define FUSE_CONTENT_H  (FUSE_BOTTOM - FUSE_TOP)

/* ═══════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Get the fuse table for the currently selected type */
static const fuse_table_t *current_table(void)
{
    uint8_t t = fuse_type;
    if (t >= FUSE_TYPE_COUNT) t = 0;
    return &fuse_tables[t];
}

/* Get the currently selected fuse entry */
static const fuse_entry_t *current_entry(void)
{
    const fuse_table_t *tbl = current_table();
    uint8_t idx = fuse_rating_idx;
    if (idx >= tbl->count) idx = 0;
    return &tbl->entries[idx];
}

/* Format integer with suffix into buffer */
static void fmt_int(char *buf, int size, int32_t val)
{
    int i = 0;
    if (val < 0) {
        buf[i++] = '-';
        val = -val;
    }
    char tmp[12];
    int t = 0;
    if (val == 0) {
        tmp[t++] = '0';
    } else {
        while (val > 0 && t < 11) {
            tmp[t++] = '0' + (val % 10);
            val /= 10;
        }
    }
    for (int j = t - 1; j >= 0 && i < size - 1; j--)
        buf[i++] = tmp[j];
    buf[i] = '\0';
}

/* Format resistance in human-readable form (uOhm → mOhm with decimals) */
static void fmt_resistance(char *buf, int size, uint32_t uohm)
{
    /* Convert micro-ohms to milli-ohms with 1 decimal */
    uint32_t mohm_x10 = uohm / 100;  /* e.g. 7900 uOhm → 79 (= 7.9 mOhm) */
    uint32_t whole = mohm_x10 / 10;
    uint32_t frac = mohm_x10 % 10;
    int i = 0;

    /* Write whole part */
    char tmp[8];
    int t = 0;
    if (whole == 0) tmp[t++] = '0';
    else {
        uint32_t w = whole;
        while (w > 0 && t < 7) { tmp[t++] = '0' + (w % 10); w /= 10; }
    }
    for (int j = t - 1; j >= 0 && i < size - 3; j--)
        buf[i++] = tmp[j];

    buf[i++] = '.';
    buf[i++] = '0' + frac;
    buf[i] = '\0';
}

/* Calculate current in mA from voltage drop in mV and resistance in uOhm.
 * current_mA = voltage_mV * 1000 / resistance_uOhm * 1000
 *            = voltage_mV * 1_000_000 / resistance_uOhm
 * We work in integer math with uV internally. */
static uint32_t calc_current_ma(float voltage_mv, uint32_t resistance_uohm)
{
    if (resistance_uohm == 0) return 0;
    /* voltage_mV * 1000 = voltage_uV, then / resistance_uOhm = current_mA */
    float current = (voltage_mv * 1000.0f) / (float)resistance_uohm;
    if (current < 0.0f) current = -current;
    return (uint32_t)(current + 0.5f);
}

/* Format current: show mA or A depending on magnitude */
static void fmt_current(char *buf, int size, uint32_t current_ma)
{
    if (current_ma >= 10000) {
        /* Show in amps: e.g. "12.3 A" */
        uint32_t a_x10 = current_ma / 100;
        int i = 0;
        char tmp[8];
        int t = 0;
        uint32_t whole = a_x10 / 10;
        uint32_t frac = a_x10 % 10;
        if (whole == 0) tmp[t++] = '0';
        else { uint32_t w = whole; while (w && t < 7) { tmp[t++] = '0' + (w % 10); w /= 10; } }
        for (int j = t - 1; j >= 0 && i < size - 5; j--) buf[i++] = tmp[j];
        buf[i++] = '.';
        buf[i++] = '0' + frac;
        buf[i++] = ' ';
        buf[i++] = 'A';
        buf[i] = '\0';
    } else {
        fmt_int(buf, size - 3, (int32_t)current_ma);
        int len = (int)strlen(buf);
        if (len < size - 4) {
            buf[len++] = ' ';
            buf[len++] = 'm';
            buf[len++] = 'A';
            buf[len] = '\0';
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * View 0: Detail — single fuse, full info
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_fuse_detail(float voltage_mv, const theme_t *th)
{
    const fuse_entry_t *fuse = current_entry();
    uint32_t r_uohm = fuse->resistance_uohm;
    uint32_t current_ma = calc_current_ma(voltage_mv, r_uohm);

    char buf[24];
    uint16_t y = FUSE_TOP + 4;

    /* Title bar */
    font_draw_string(4, y, "FUSE CURRENT TEST",
                     th->highlight, th->background, &font_small);

    /* Fuse type + rating (right side) */
    {
        char fuse_label[24];
        const char *tname = fuse_type_names[fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0];
        int i = 0;
        while (*tname && i < 14) fuse_label[i++] = *tname++;
        fuse_label[i++] = ' ';
        fmt_int(buf, sizeof(buf), fuse->rating_amps);
        const char *s = buf;
        while (*s && i < 20) fuse_label[i++] = *s++;
        fuse_label[i++] = 'A';
        fuse_label[i] = '\0';
        font_draw_string_right(LCD_WIDTH - 4, y, fuse_label,
                               th->ch1, th->background, &font_small);
    }
    y += 20;

    /* Large current reading */
    fmt_current(buf, sizeof(buf), current_ma);
    font_draw_string_right(240, y, buf,
                           th->text_primary, th->background, &font_xlarge);
    font_draw_string(248, y + 4, "mA",
                     th->ch1, th->background, &font_large);
    y += 50;

    /* Voltage drop */
    {
        /* Format mV with 3 decimals for sub-mV resolution */
        int whole = (int)voltage_mv;
        int frac = (int)((voltage_mv - (float)whole) * 1000.0f);
        if (frac < 0) frac = -frac;
        int i = 0;
        if (voltage_mv < 0) buf[i++] = '-';
        char tmp[8]; int t = 0;
        if (whole == 0) tmp[t++] = '0';
        else { int w = whole < 0 ? -whole : whole; while (w && t < 7) { tmp[t++] = '0' + (w % 10); w /= 10; } }
        for (int j = t - 1; j >= 0 && i < 16; j--) buf[i++] = tmp[j];
        buf[i++] = '.';
        buf[i++] = '0' + (frac / 100) % 10;
        buf[i++] = '0' + (frac / 10) % 10;
        buf[i++] = '0' + frac % 10;
        buf[i] = '\0';
    }
    font_draw_string(16, y, "V drop:",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(80, y, buf,
                     th->text_primary, th->background, &font_medium);
    font_draw_string(160, y, "mV",
                     th->text_secondary, th->background, &font_small);
    y += 20;

    /* Resistance */
    char rbuf[16];
    fmt_resistance(rbuf, sizeof(rbuf), r_uohm);
    font_draw_string(16, y, "R fuse:",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(80, y, rbuf,
                     th->text_primary, th->background, &font_medium);
    font_draw_string(160, y, "mOhm",
                     th->text_secondary, th->background, &font_small);
    y += 24;

    /* Current bar graph */
    {
        /* Bar: 0 to fuse rating in amps (full scale) */
        float bar_max_ma = (float)fuse->rating_amps * 1000.0f;
        float fraction = (float)current_ma / bar_max_ma;
        if (fraction > 1.0f) fraction = 1.0f;

        uint16_t bar_color = th->success;
        if (current_ma > 50) bar_color = th->highlight;
        if (current_ma > 500) bar_color = th->warning;

        lcd_fill_rect(16, y, 288, 10, th->grid);
        uint16_t fill_w = (uint16_t)(fraction * 288.0f);
        if (fill_w > 288) fill_w = 288;
        if (fill_w > 0)
            lcd_fill_rect(16, y, fill_w, 10, bar_color);

        /* 50mA threshold marker */
        float thresh_frac = 50.0f / bar_max_ma;
        if (thresh_frac < 1.0f) {
            uint16_t tx = 16 + (uint16_t)(thresh_frac * 288.0f);
            lcd_fill_rect(tx, y - 2, 1, 14, th->highlight);
        }

        y += 14;
        font_draw_string(16, y, "0",
                         th->text_secondary, th->background, &font_small);
        char max_label[12];
        fmt_int(max_label, sizeof(max_label) - 1, fuse->rating_amps);
        int len = (int)strlen(max_label);
        max_label[len++] = 'A'; max_label[len] = '\0';
        font_draw_string_right(304, y, max_label,
                               th->text_secondary, th->background, &font_small);
    }
    y += 18;

    /* Navigation hints */
    font_draw_string(16, y, "< >Type",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(120, y, "^ vRating",
                     th->text_secondary, th->background, &font_small);

    /* Warning if current > 50mA */
    if (current_ma > 50) {
        y += 16;
        font_draw_string(16, y, "! DRAW EXCEEDS 50mA",
                         th->warning, th->background, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * View 1: Multi — all ratings for selected fuse type
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_fuse_multi(float voltage_mv, const theme_t *th)
{
    const fuse_table_t *tbl = current_table();
    char buf[24];
    uint16_t y = FUSE_TOP + 2;

    /* Header */
    font_draw_string(4, y, "FUSE CURRENT",
                     th->highlight, th->background, &font_small);

    /* Voltage drop readout */
    {
        int whole = (int)voltage_mv;
        int frac = (int)((voltage_mv - (float)whole) * 100.0f);
        if (frac < 0) frac = -frac;
        int i = 0;
        char tmp[8]; int t = 0;
        if (whole == 0) tmp[t++] = '0';
        else { int w = whole; while (w && t < 7) { tmp[t++] = '0' + (w % 10); w /= 10; } }
        for (int j = t - 1; j >= 0 && i < 10; j--) buf[i++] = tmp[j];
        buf[i++] = '.';
        buf[i++] = '0' + (frac / 10) % 10;
        buf[i++] = '0' + frac % 10;
        buf[i] = '\0';
    }
    font_draw_string(180, y, "Vd:",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(200, y, buf,
                     th->text_primary, th->background, &font_small);
    font_draw_string(260, y, "mV",
                     th->text_secondary, th->background, &font_small);
    y += 16;

    /* Type selector */
    {
        const char *tname = fuse_type_names[fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0];
        font_draw_string(4, y, "<",
                         th->text_secondary, th->background, &font_small);
        font_draw_string(16, y, tname,
                         th->ch1, th->background, &font_small);
        font_draw_string(80, y, ">",
                         th->text_secondary, th->background, &font_small);
    }
    y += 16;

    /* Column headers */
    font_draw_string(8, y, "Fuse",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(60, y, "R(mOhm)",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(150, y, "Current",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(240, y, "Draw?",
                     th->text_secondary, th->background, &font_small);
    y += 14;

    /* Separator line */
    lcd_fill_rect(4, y, 312, 1, th->grid_center);
    y += 3;

    /* Table rows — show as many as fit */
    uint8_t max_rows = (FUSE_BOTTOM - y - 4) / 14;
    uint8_t rows = tbl->count < max_rows ? tbl->count : max_rows;

    for (uint8_t i = 0; i < rows; i++) {
        const fuse_entry_t *e = &tbl->entries[i];
        uint32_t current_ma = calc_current_ma(voltage_mv, e->resistance_uohm);

        /* Highlight selected rating */
        bool selected = (i == fuse_rating_idx);
        uint16_t row_bg = selected ? th->grid : th->background;
        uint16_t text_color = selected ? th->highlight : th->text_primary;
        if (selected) {
            lcd_fill_rect(4, y - 1, 312, 14, row_bg);
        }

        /* Rating */
        fmt_int(buf, sizeof(buf), e->rating_amps);
        int len = (int)strlen(buf);
        buf[len++] = 'A'; buf[len] = '\0';
        font_draw_string(8, y, buf, text_color, row_bg, &font_small);

        /* Resistance */
        fmt_resistance(buf, sizeof(buf), e->resistance_uohm);
        font_draw_string(60, y, buf, text_color, row_bg, &font_small);

        /* Current */
        fmt_current(buf, sizeof(buf), current_ma);
        font_draw_string(150, y, buf, text_color, row_bg, &font_small);

        /* Draw indicator */
        if (current_ma > 50) {
            uint16_t warn_color = current_ma > 500 ? th->warning : th->highlight;
            font_draw_string(240, y, "YES", warn_color, row_bg, &font_small);
        } else if (voltage_mv > 0.01f) {
            font_draw_string(240, y, "low", th->success, row_bg, &font_small);
        } else {
            font_draw_string(240, y, "--", th->text_secondary, row_bg, &font_small);
        }

        y += 14;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * View 2: Scan — pass/fail parasitic draw hunting
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_fuse_scan(float voltage_mv, const theme_t *th)
{
    char buf[24];
    uint16_t y = FUSE_TOP + 4;

    /* Header */
    font_draw_string(4, y, "PARASITIC SCAN",
                     th->highlight, th->background, &font_small);

    /* Voltage readout */
    {
        int whole = (int)voltage_mv;
        int frac = (int)((voltage_mv - (float)whole) * 100.0f);
        if (frac < 0) frac = -frac;
        int i = 0;
        char tmp[8]; int t = 0;
        if (whole == 0) tmp[t++] = '0';
        else { int w = whole; while (w && t < 7) { tmp[t++] = '0' + (w % 10); w /= 10; } }
        for (int j = t - 1; j >= 0 && i < 10; j--) buf[i++] = tmp[j];
        buf[i++] = '.';
        buf[i++] = '0' + (frac / 10) % 10;
        buf[i++] = '0' + frac % 10;
        buf[i] = '\0';
    }
    font_draw_string(190, y, "Vd:",
                     th->text_secondary, th->background, &font_small);
    font_draw_string(210, y, buf,
                     th->text_primary, th->background, &font_small);
    font_draw_string(270, y, "mV",
                     th->text_secondary, th->background, &font_small);

    /* Big pass/fail indicator centered on screen */
    uint16_t center_y = FUSE_TOP + FUSE_CONTENT_H / 2 - 30;
    float threshold = fuse_scan_threshold_mv;

    if (voltage_mv < 0.01f) {
        /* No contact / no reading */
        font_draw_string(80, center_y, "---",
                         th->text_secondary, th->background, &font_xlarge);
        font_draw_string(80, center_y + 50, "Touch probes to fuse",
                         th->text_secondary, th->background, &font_small);
    } else if (voltage_mv < threshold) {
        /* NO DRAW — green background badge */
        lcd_fill_rect(40, center_y - 4, 240, 52, th->success);
        font_draw_string(60, center_y, "NO DRAW",
                         th->background, th->success, &font_xlarge);
        font_draw_string(80, center_y + 56, "Move to next fuse",
                         th->text_secondary, th->background, &font_small);
    } else {
        /* DRAW DETECTED — red/warning background */
        uint16_t alert_color = voltage_mv > (threshold * 5.0f) ? th->warning : th->warning;
        lcd_fill_rect(40, center_y - 4, 240, 52, alert_color);
        font_draw_string(46, center_y, "DRAW!",
                         th->background, alert_color, &font_xlarge);

        /* Show estimated current for common fuse sizes */
        uint16_t info_y = center_y + 58;
        const uint8_t common_ratings[] = { 10, 15, 20, 30 };
        font_draw_string(16, info_y, "If fuse is:",
                         th->text_secondary, th->background, &font_small);
        info_y += 14;
        for (int i = 0; i < 4; i++) {
            uint32_t r = fuse_lookup_resistance_uohm(fuse_type, common_ratings[i]);
            if (r == 0) continue;
            uint32_t ma = calc_current_ma(voltage_mv, r);
            fmt_int(buf, 4, common_ratings[i]);
            int len = (int)strlen(buf);
            buf[len++] = 'A'; buf[len++] = ':'; buf[len++] = ' '; buf[len] = '\0';
            font_draw_string(24 + i * 72, info_y, buf,
                             th->text_secondary, th->background, &font_small);
            fmt_current(buf, sizeof(buf), ma);
            font_draw_string(54 + i * 72, info_y, buf,
                             th->text_primary, th->background, &font_small);
        }
    }

    /* Threshold setting at bottom */
    {
        uint16_t bot_y = FUSE_BOTTOM - 16;
        font_draw_string(16, bot_y, "Threshold:",
                         th->text_secondary, th->background, &font_small);
        int tw = (int)(threshold * 10.0f);
        int i = 0;
        buf[i++] = '0' + (tw / 10);
        buf[i++] = '.';
        buf[i++] = '0' + (tw % 10);
        buf[i++] = ' ';
        buf[i++] = 'm';
        buf[i++] = 'V';
        buf[i] = '\0';
        font_draw_string(90, bot_y, buf,
                         th->ch1, th->background, &font_small);
        font_draw_string(160, bot_y, "(^v to adjust)",
                         th->text_secondary, th->background, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void draw_fuse_screen(float voltage_drop_mv)
{
    const theme_t *th = theme_get();

    /* Clear content area */
    lcd_fill_rect(0, FUSE_TOP, LCD_WIDTH, FUSE_BOTTOM - FUSE_TOP, th->background);

    switch (fuse_view) {
    case FUSE_VIEW_MULTI:
        draw_fuse_multi(voltage_drop_mv, th);
        break;
    case FUSE_VIEW_SCAN:
        draw_fuse_scan(voltage_drop_mv, th);
        break;
    default:
        draw_fuse_detail(voltage_drop_mv, th);
        break;
    }
}

void fuse_cycle_view(void)
{
    fuse_view = (fuse_view + 1) % FUSE_VIEW_COUNT;
}

void fuse_next_rating(void)
{
    const fuse_table_t *tbl = current_table();
    if (fuse_view == FUSE_VIEW_SCAN) {
        /* In scan view, UP/DOWN adjusts threshold */
        fuse_scan_threshold_mv += 0.1f;
        if (fuse_scan_threshold_mv > 5.0f) fuse_scan_threshold_mv = 5.0f;
        return;
    }
    if (fuse_rating_idx + 1 < tbl->count)
        fuse_rating_idx++;
    else
        fuse_rating_idx = 0;
}

void fuse_prev_rating(void)
{
    const fuse_table_t *tbl = current_table();
    if (fuse_view == FUSE_VIEW_SCAN) {
        fuse_scan_threshold_mv -= 0.1f;
        if (fuse_scan_threshold_mv < 0.1f) fuse_scan_threshold_mv = 0.1f;
        return;
    }
    if (fuse_rating_idx == 0)
        fuse_rating_idx = tbl->count - 1;
    else
        fuse_rating_idx--;
}

void fuse_next_type(void)
{
    fuse_type = (fuse_type + 1) % FUSE_TYPE_COUNT;
    /* Clamp rating index to new table size */
    const fuse_table_t *tbl = current_table();
    if (fuse_rating_idx >= tbl->count)
        fuse_rating_idx = tbl->count - 1;
}

void fuse_prev_type(void)
{
    if (fuse_type == 0)
        fuse_type = FUSE_TYPE_COUNT - 1;
    else
        fuse_type--;
    const fuse_table_t *tbl = current_table();
    if (fuse_rating_idx >= tbl->count)
        fuse_rating_idx = tbl->count - 1;
}
