/*
 * OpenScope 2C53T - Fuse Current Tester UI
 *
 * Estimates circuit current from the millivolt drop across a fuse left in
 * place: I = V_drop / R_fuse (src/ui/fuse_model.c). Four views, picked with
 * the Show softkey (design: docs/specs/platform/softkey-ui.md, canvas boards
 * FuseDetail / FuseScan / FuseTypes):
 *
 *   Detail -- one fuse: a picture of it, the current, a 0-150 mA bar with the
 *             50 mA parasitic mark, the drop and the fuse resistance
 *   Table  -- every rating of the type, largest at the top
 *   Scan   -- DRAW / NO DRAW for walking a fuse box, plus the current if the
 *             fuse is one of four common ratings
 *   Types  -- "which fuse is this?": the five types to scale, front and top,
 *             and the rating colours of the selected type
 *
 * Buttons (meter_layout == METER_LAYOUT_FUSE): softkeys Fuse type / Rating
 * (Draw-if threshold in Scan) / Show / View; UP/DOWN change the rating (the
 * threshold in Scan), UP = larger; LEFT/RIGHT change the type.
 *
 * Drawing: the static parts are painted once per structural change (view,
 * type, rating, theme, or a meter full repaint); a reading update rewrites
 * only the values, through opaque fixed-width boxes, so no pixel is blanked
 * and redrawn. Until 2026-10-06 every update cleared the whole area first,
 * which was the visible flicker.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "fuse_table.h"
#include "fuse_model.h"
#include "softkey.h"
#include <stdio.h>
#include <string.h>

#define FUSE_TOP        18              /* below the status bar */
#define FUSE_BOTTOM     SOFTKEY_BAR_Y   /* above the softkey bar */

/* Detail / Scan: picture column on the left, readings on the right. */
#define PIC_CX          58
#define RX              122             /* right column x */
#define RW              (LCD_WIDTH - 4 - RX)
#define VAL_X           222             /* value boxes in the right column */
#define VAL_W           (LCD_WIDTH - 4 - VAL_X)

/* Detail */
#define NUM_Y           36
#define BAR_Y           112
#define BAR_H           10
#define MARK_X          (RX + (RW * FUSE_DRAW_LIMIT_MA) / FUSE_BAR_FULL_MA)
#define DROP_Y          146
#define RES_Y           166
#define VERDICT_Y       188

/* Scan */
#define SCAN_BOX_H      70
#define CELL_Y          114
#define CELL_W          46
#define CELL_H          36
#define CELL_GAP        3

/* Table */
#define TBL_ROW0        58
#define TBL_ROW_H       15
#define TBL_ROWS        ((FUSE_BOTTOM - TBL_ROW0) / TBL_ROW_H)

/* What the static paint was made for; anything different repaints. */
static struct {
    bool    valid;
    uint8_t view, type, rating, theme;
    uint8_t unit;           /* Detail: 0 mA, 1 A, 0xFF unknown */
    uint8_t scan_state;     /* Scan: 0 no reading, 1 no draw, 2 draw, 3 not DC V,
                             * 4 not settled */
} fz;

/* ═══════════════════════════════════════════════════════════════════
 * Small helpers
 * ═══════════════════════════════════════════════════════════════════ */

static const fuse_table_t *current_table(void)
{
    return &fuse_tables[fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0];
}

static const fuse_entry_t *current_entry(void)
{
    const fuse_table_t *tbl = current_table();
    return &tbl->entries[fuse_rating_idx < tbl->count ? fuse_rating_idx : 0];
}

static void rect(int x, int y, int w, int h, uint16_t c)
{
    if (w > 0 && h > 0 && x >= 0 && y >= 0)
        lcd_fill_rect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, c);
}

static uint16_t c565(uint32_t rgb)
{
    uint8_t r = (uint8_t)(rgb >> 16), g = (uint8_t)(rgb >> 8), b = (uint8_t)rgb;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* Mix rgb toward `to` by pct percent. */
static uint32_t mix(uint32_t rgb, uint32_t to, unsigned pct)
{
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        unsigned a = (rgb >> s) & 0xFF, b = (to >> s) & 0xFF;
        out |= (uint32_t)((a * (100 - pct) + b * pct) / 100) << s;
    }
    return out;
}

static bool is_light(uint32_t rgb)
{
    unsigned r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000 > 150;
}

static bool is_dark(uint32_t rgb)
{
    unsigned r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000 < 60;
}

static uint32_t edge_of(uint32_t body)
{
    return is_dark(body) ? mix(body, 0xFFFFFF, 40) : mix(body, 0x000000, 45);
}

#define NEUTRAL_RGB  0x6B737C   /* a rating with no standard colour */
#define METAL_RGB    0xC9CED4

static uint32_t rating_rgb(uint8_t type, uint8_t amps)
{
    uint32_t rgb;
    return fuse_rating_rgb((fuse_type_t)type, amps, &rgb) ? rgb : NEUTRAL_RGB;
}

/* "48 mA", "1.23 A" in parts: number text and unit index (0 mA, 1 A). */
static uint8_t fmt_current_big(char *b, size_t n, uint32_t ma)
{
    if (ma < 1000)   { snprintf(b, n, "%u", (unsigned)ma); return 0; }
    if (ma < 10000)  { snprintf(b, n, "%u.%02u", (unsigned)(ma / 1000), (unsigned)(ma % 1000) / 10); return 1; }
    if (ma < 100000) { snprintf(b, n, "%u.%u", (unsigned)(ma / 1000), (unsigned)(ma % 1000) / 100); return 1; }
    snprintf(b, n, "OL");
    return 1;
}

/* `tight` drops the space so "257mA" fits a 46 px Scan cell. */
static void fmt_current_short(char *b, size_t n, uint32_t ma, bool tight)
{
    const char *sp = tight ? "" : " ";
    if (ma < 1000)        snprintf(b, n, "%u%smA", (unsigned)ma, sp);
    else if (ma < 10000)  snprintf(b, n, "%u.%u%sA", (unsigned)(ma / 1000), (unsigned)(ma % 1000) / 100, sp);
    else if (ma < 100000) snprintf(b, n, "%u%sA", (unsigned)(ma / 1000), sp);
    else                  snprintf(b, n, "OL");
}

static void fmt_mohm(char *b, size_t n, uint32_t uohm)
{
    uint32_t t = (uohm + 50) / 100;          /* tenths of a milliohm */
    snprintf(b, n, "%u.%u mOhm", (unsigned)(t / 10), (unsigned)(t % 10));
}

/* Fixed point by hand: the firmware links newlib-nano, whose printf has no %f
 * (the first bench run printed a bare "mV"). */
static void fmt_drop(char *b, size_t n, float mv, int8_t dec)
{
    if (dec < 0) { snprintf(b, n, "-- mV"); return; }
    if (dec > 3) dec = 3;
    uint32_t scale = 1;
    for (int8_t i = 0; i < dec; i++) scale *= 10;
    float a = mv < 0 ? -mv : mv;
    uint32_t v = (uint32_t)(a * (float)scale + 0.5f);
    const char *sign = (mv < 0 && v != 0) ? "-" : "";
    if (dec == 0)
        snprintf(b, n, "%s%u mV", sign, (unsigned)v);
    else
        snprintf(b, n, "%s%u.%0*u mV", sign, (unsigned)(v / scale), (int)dec,
                 (unsigned)(v % scale));
}

/* ═══════════════════════════════════════════════════════════════════
 * Fuse pictures, from the shape table (4 px/mm) scaled by num/64
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct { int ox, oy, num; } xf_t;
#define XF_DEN 64

static int sxp(const xf_t *t, int v) { return t->ox + v * t->num / XF_DEN; }
static int syp(const xf_t *t, int v) { return t->oy + v * t->num / XF_DEN; }

/* Scaled box with a 1 px edge; `round` knocks the corners back to bg. */
static void sbox(const xf_t *t, int x, int y, int w, int h,
                 uint16_t fill, uint16_t edge, uint16_t bg, bool round)
{
    int x0 = sxp(t, x), y0 = syp(t, y), x1 = sxp(t, x + w), y1 = syp(t, y + h);
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    int w2 = x1 - x0, h2 = y1 - y0;
    if (w2 > 2 && h2 > 2) {
        rect(x0, y0, w2, 1, edge);
        rect(x0, y1 - 1, w2, 1, edge);
        rect(x0, y0 + 1, 1, h2 - 2, edge);
        rect(x1 - 1, y0 + 1, 1, h2 - 2, edge);
        rect(x0 + 1, y0 + 1, w2 - 2, h2 - 2, fill);
    } else {
        rect(x0, y0, w2, h2, fill);
    }
    if (round && w2 > 4 && h2 > 4) {
        lcd_set_pixel((uint16_t)x0, (uint16_t)y0, bg);
        lcd_set_pixel((uint16_t)(x1 - 1), (uint16_t)y0, bg);
        lcd_set_pixel((uint16_t)x0, (uint16_t)(y1 - 1), bg);
        lcd_set_pixel((uint16_t)(x1 - 1), (uint16_t)(y1 - 1), bg);
    }
}

/* Front view: blades, body, window, the zig-zag element, the rating. */
static void draw_front(const xf_t *t, uint8_t type, uint32_t body,
                       const char *label, uint16_t bg)
{
    const fuse_shape_t *s = fuse_shape((fuse_type_t)type);
    uint16_t c_body = c565(body), c_edge = c565(edge_of(body));
    uint32_t win_rgb = mix(body, 0xFFFFFF, 45);
    uint16_t c_win = c565(win_rgb);

    if (!s->cartridge) {
        uint16_t m = c565(METAL_RGB), me = c565(0x8B939C);
        sbox(t, s->blade_x, s->blade_y, s->blade_w, s->blade_h, m, me, bg, false);
        sbox(t, s->w - s->blade_x - s->blade_w, s->blade_y, s->blade_w, s->blade_h,
             m, me, bg, false);
    }
    sbox(t, 1, 1, s->w - 2, s->body_h, c_body, c_edge, bg, true);

    int wx0 = sxp(t, s->win_x), wy0 = syp(t, s->win_y);
    int ww = sxp(t, s->win_x + s->win_w) - wx0, wh = syp(t, s->win_y + s->win_h) - wy0;
    rect(wx0, wy0, ww, wh, c_win);

    /* Element: up, across, down, across, up, across, down. */
    int th = (5 * t->num + XF_DEN) / (2 * XF_DEN);
    if (th < 1) th = 1;
    int xa = wx0 + ww * 12 / 100, xb = wx0 + ww * 33 / 100;
    int xc = wx0 + ww * 66 / 100 - th, xd = wx0 + ww * 88 / 100 - th;
    int yt = wy0 + wh * 27 / 100, ym = wy0 + wh * 62 / 100, yb = wy0 + wh * 88 / 100;
    uint16_t el = is_light(win_rgb) ? c565(0x3A4048) : c565(0xF2F4F6);
    rect(xa, yt, th, yb - yt, el);
    rect(xa, yt, xb - xa + th, th, el);
    rect(xb, yt, th, ym - yt + th, el);
    rect(xb, ym, xc - xb + th, th, el);
    rect(xc, yt, th, ym - yt + th, el);
    rect(xc, yt, xd - xc + th, th, el);
    rect(xd, yt, th, yb - yt, el);

    if (s->cartridge) {
        uint16_t slot = c565(0x2A1520);
        sbox(t, s->blade_x, s->blade_y, s->blade_w, s->blade_h, slot, slot, c_body, false);
        sbox(t, s->w - s->blade_x - s->blade_w, s->blade_y, s->blade_w, s->blade_h,
             slot, slot, c_body, false);
    }

    if (label) {
        int top = syp(t, 1) + 1;
        int cx = sxp(t, s->w / 2);
        uint8_t fh = font_small.height;
        if (wy0 - top >= fh + 1)
            font_draw_string_center((uint16_t)cx, (uint16_t)(top + (wy0 - top - fh) / 2), label,
                                    is_light(body) ? c565(0x101317) : c565(0xFFFFFF),
                                    c_body, &font_small);
        else
            font_draw_string_center((uint16_t)cx, (uint16_t)(wy0 + 1), label,
                                    is_light(win_rgb) ? c565(0x101317) : c565(0xFFFFFF),
                                    c_win, &font_small);
    }
}

/* Top view: where the probes go. */
static void draw_top(const xf_t *t, uint8_t type, uint32_t body, uint16_t bg)
{
    const fuse_shape_t *s = fuse_shape((fuse_type_t)type);
    uint16_t c_body = c565(body);
    sbox(t, 1, 1, s->w - 2, s->top_h - 2, c_body, c565(edge_of(body)), bg, true);
    uint16_t m = c565(METAL_RGB), me = c565(0x5E666F);
    sbox(t, s->pad_x, s->pad_y, s->pad_w, s->pad_h, m, me, c_body, s->cartridge);
    sbox(t, s->w - s->pad_x - s->pad_w, s->pad_y, s->pad_w, s->pad_h, m, me, c_body,
         s->cartridge);
}

/* Left column of Detail and Scan: the selected fuse, front over top, then a
 * caption and its name. */
static void draw_picture_column(const char *cap1, const char *cap2, const theme_t *th)
{
    uint8_t type = fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0;
    const fuse_shape_t *s = fuse_shape((fuse_type_t)type);
    const fuse_entry_t *e = current_entry();

    int num = 96 * XF_DEN / s->w;
    int n2 = 86 * XF_DEN / s->h;              if (n2 < num) num = n2;
    n2 = 118 * XF_DEN / (s->h + s->top_h);    if (n2 < num) num = n2;

    xf_t t = { PIC_CX - (s->w * num / XF_DEN) / 2, FUSE_TOP + 4, num };
    uint32_t body = rating_rgb(type, e->rating_amps);
    char lbl[8];
    fuse_rating_label(e->rating_amps, lbl, sizeof lbl);
    draw_front(&t, type, body, lbl, th->background);
    t.oy = FUSE_TOP + 4 + s->h * num / XF_DEN + 4;
    draw_top(&t, type, body, th->background);

    font_draw_string_center(PIC_CX, 150, cap1, th->text_secondary, th->background, &font_small);
    font_draw_string_center(PIC_CX, 163, cap2, th->text_secondary, th->background, &font_small);

    char name[24];
    snprintf(name, sizeof name, "%s %s A", fuse_type_names[type], lbl);
    font_draw_string_center(PIC_CX, 184, name, th->text_primary, th->background, &font_medium);
}

/* ═══════════════════════════════════════════════════════════════════
 * Detail
 * ═══════════════════════════════════════════════════════════════════ */

static void detail_static(const theme_t *th)
{
    char b[24];
    draw_picture_column("probe both metal", "tips on top", th);

    font_draw_string(RX, FUSE_TOP + 4, "Current through it",
                     th->text_secondary, th->background, &font_small);
    rect(MARK_X, BAR_Y - 3, 2, BAR_H + 6, th->warning);
    font_draw_string(RX, BAR_Y + BAR_H + 6, "0", th->text_secondary, th->background, &font_small);
    snprintf(b, sizeof b, "%u mA limit", (unsigned)FUSE_DRAW_LIMIT_MA);
    font_draw_string_center(MARK_X + 1, BAR_Y + BAR_H + 6, b, th->warning, th->background,
                            &font_small);
    snprintf(b, sizeof b, "%u", (unsigned)FUSE_BAR_FULL_MA);
    font_draw_string_right(RX + RW, BAR_Y + BAR_H + 6, b, th->text_secondary, th->background,
                           &font_small);

    font_draw_string(RX, DROP_Y, "Voltage drop", th->text_secondary, th->background, &font_medium);
    font_draw_string(RX, RES_Y, "Resistance", th->text_secondary, th->background, &font_medium);
    fmt_mohm(b, sizeof b, current_entry()->resistance_uohm);
    font_draw_string_box(VAL_X, RES_Y, VAL_W, b, th->text_primary, th->background,
                         &font_medium, FONT_ALIGN_RIGHT);
}

/* One run of the bar, split at `fill_end`. */
static void bar_span(int x0, int x1, int fill_end, uint16_t on, uint16_t off)
{
    int f = fill_end < x0 ? x0 : (fill_end > x1 ? x1 : fill_end);
    rect(x0, BAR_Y, f - x0, BAR_H, on);
    rect(f, BAR_Y, x1 - f, BAR_H, off);
}

static void detail_dynamic(float mv, int8_t dec, bool steady, const theme_t *th)
{
    bool valid = dec >= 0;
    uint32_t ma = valid ? fuse_current_ma(mv, current_entry()->resistance_uohm) : 0;
    char num[12], b[24];
    uint8_t unit = 0;
    if (valid) unit = fmt_current_big(num, sizeof num, ma);
    else       snprintf(num, sizeof num, "--");

    if (unit != fz.unit) {                      /* number box changes width */
        rect(RX, NUM_Y, RW, font_huge.height, th->background);
        fz.unit = unit;
    }
    const char *us = unit ? "A" : "mA";
    uint16_t uw = font_string_width(us, &font_unit);
    uint16_t nw = (uint16_t)(RW - uw - 4);
    font_draw_string_box(RX, NUM_Y, nw, num, steady ? th->text_primary : th->grid,
                         th->background, &font_huge, FONT_ALIGN_RIGHT);
    font_draw_string_box((uint16_t)(RX + nw + 4), NUM_Y + 28, uw, us, th->ch1,
                         th->background, &font_unit, FONT_ALIGN_LEFT);

    uint32_t capped = ma > FUSE_BAR_FULL_MA ? FUSE_BAR_FULL_MA : ma;
    int fill = RX + (int)(capped * RW / FUSE_BAR_FULL_MA);
    uint16_t on = !steady ? th->text_secondary
                : (ma > FUSE_DRAW_LIMIT_MA ? th->warning : th->success);
    bar_span(RX, MARK_X, fill, on, th->grid);
    bar_span(MARK_X + 2, RX + RW, fill, on, th->grid);

    fmt_drop(b, sizeof b, mv, dec);
    font_draw_string_box(VAL_X, DROP_Y, VAL_W, b, th->text_primary, th->background,
                         &font_medium, FONT_ALIGN_RIGHT);

    uint16_t vc = th->text_secondary;
    if (dec == -2) {
        snprintf(b, sizeof b, "needs DC V: View, Fn");
        vc = th->warning;
    } else if (!valid) {
        snprintf(b, sizeof b, "no reading");
    } else if (!steady) {
        snprintf(b, sizeof b, "unsteady: probes on?");
    } else if (ma > FUSE_DRAW_LIMIT_MA) {
        snprintf(b, sizeof b, "over %u mA: a draw", (unsigned)FUSE_DRAW_LIMIT_MA);
        vc = th->warning;
    } else {
        snprintf(b, sizeof b, "under %u mA", (unsigned)FUSE_DRAW_LIMIT_MA);
        vc = th->success;
    }
    font_draw_string_box(RX, VERDICT_Y, RW, b, vc, th->background, &font_medium,
                         FONT_ALIGN_LEFT);
}

/* ═══════════════════════════════════════════════════════════════════
 * Table -- largest rating at the top, so UP moves the highlight up
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t table_start(const fuse_table_t *tbl)
{
    int rows = tbl->count < TBL_ROWS ? tbl->count : TBL_ROWS;
    int sel_pos = tbl->count - 1 - (fuse_rating_idx < tbl->count ? fuse_rating_idx : 0);
    int start = sel_pos - rows / 2;
    if (start > tbl->count - rows) start = tbl->count - rows;
    if (start < 0) start = 0;
    return (uint8_t)start;
}

static void table_static(const theme_t *th)
{
    const fuse_table_t *tbl = current_table();
    uint8_t type = fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0;
    char b[24];

    snprintf(b, sizeof b, "%s, all ratings", fuse_type_names[type]);
    font_draw_string(4, FUSE_TOP + 4, b, th->highlight, th->background, &font_small);
    font_draw_string(196, FUSE_TOP + 4, "drop", th->text_secondary, th->background, &font_small);

    uint16_t y = 40;
    font_draw_string(22, y, "Fuse", th->text_secondary, th->background, &font_small);
    font_draw_string(74, y, "Resistance", th->text_secondary, th->background, &font_small);
    font_draw_string(150, y, "Current", th->text_secondary, th->background, &font_small);
    font_draw_string(250, y, "Draw?", th->text_secondary, th->background, &font_small);
    rect(4, 54, LCD_WIDTH - 8, 1, th->grid_center);

    uint8_t start = table_start(tbl);
    uint8_t rows = tbl->count < TBL_ROWS ? tbl->count : TBL_ROWS;
    for (uint8_t k = 0; k < rows; k++) {
        uint8_t idx = (uint8_t)(tbl->count - 1 - (start + k));
        const fuse_entry_t *e = &tbl->entries[idx];
        bool sel = (idx == fuse_rating_idx);
        uint16_t bg = sel ? th->menu_selected_bg : th->background;
        uint16_t fg = sel ? th->highlight : th->text_primary;
        int y0 = TBL_ROW0 + k * TBL_ROW_H;
        rect(0, y0 - 1, LCD_WIDTH, TBL_ROW_H, bg);

        uint32_t rgb;
        if (fuse_rating_rgb((fuse_type_t)type, e->rating_amps, &rgb)) {
            rect(6, y0, 10, 11, c565(edge_of(rgb)));
            rect(7, y0 + 1, 8, 9, c565(rgb));
        } else {
            rect(6, y0, 10, 11, th->text_secondary);
            rect(7, y0 + 1, 8, 9, bg);
        }
        char lbl[8];
        fuse_rating_label(e->rating_amps, lbl, sizeof lbl);
        snprintf(b, sizeof b, "%s A", lbl);
        font_draw_string(22, y0, b, fg, bg, &font_small);
        fmt_mohm(b, sizeof b, e->resistance_uohm);
        font_draw_string(74, y0, b, fg, bg, &font_small);
    }
}

static void table_dynamic(float mv, int8_t dec, bool steady, const theme_t *th)
{
    const fuse_table_t *tbl = current_table();
    char b[24];
    fmt_drop(b, sizeof b, mv, dec);
    font_draw_string_box(222, FUSE_TOP + 4, LCD_WIDTH - 4 - 222, b, th->text_primary,
                         th->background, &font_small, FONT_ALIGN_RIGHT);

    uint8_t start = table_start(tbl);
    uint8_t rows = tbl->count < TBL_ROWS ? tbl->count : TBL_ROWS;
    for (uint8_t k = 0; k < rows; k++) {
        uint8_t idx = (uint8_t)(tbl->count - 1 - (start + k));
        const fuse_entry_t *e = &tbl->entries[idx];
        bool sel = (idx == fuse_rating_idx);
        uint16_t bg = sel ? th->menu_selected_bg : th->background;
        uint16_t fg = sel ? th->highlight : th->text_primary;
        uint16_t y0 = (uint16_t)(TBL_ROW0 + k * TBL_ROW_H);
        uint16_t vc = th->text_secondary;
        const char *v = "--";
        if (dec >= 0) {
            uint32_t ma = fuse_current_ma(mv, e->resistance_uohm);
            fmt_current_short(b, sizeof b, ma, false);
            bool draw = ma > FUSE_DRAW_LIMIT_MA;
            v = !steady ? "?" : (draw ? "YES" : "no");
            vc = !steady ? th->text_secondary : (draw ? th->warning : th->success);
        } else {
            snprintf(b, sizeof b, "--");
        }
        font_draw_string_box(150, y0, 96, b, fg, bg, &font_small, FONT_ALIGN_LEFT);
        font_draw_string_box(250, y0, LCD_WIDTH - 4 - 250, v, vc, bg, &font_small,
                             FONT_ALIGN_LEFT);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Scan
 * ═══════════════════════════════════════════════════════════════════ */

static void scan_cell_xy(uint8_t i, uint16_t *x)
{
    *x = (uint16_t)(RX + i * (CELL_W + CELL_GAP));
}

static void scan_static(const theme_t *th)
{
    draw_picture_column("probe each fuse", "engine off", th);

    font_draw_string(RX, 98, "If this fuse is...", th->text_secondary, th->background,
                     &font_small);
    uint8_t r[4];
    uint8_t n = fuse_scan_ratings((fuse_type_t)fuse_type, r);
    uint8_t sel = current_entry()->rating_amps;
    for (uint8_t i = 0; i < n; i++) {
        uint16_t x;
        scan_cell_xy(i, &x);
        uint16_t bg = r[i] == sel ? th->menu_selected_bg : th->grid;
        rect(x, CELL_Y, CELL_W, CELL_H, bg);
        char b[8];
        snprintf(b, sizeof b, "%u A", (unsigned)r[i]);
        font_draw_string_center((uint16_t)(x + CELL_W / 2), CELL_Y + 3, b,
                                th->text_secondary, bg, &font_small);
    }
    font_draw_string(RX, 158, "Voltage drop", th->text_secondary, th->background, &font_medium);
    font_draw_string(RX, 184, "estimates, +/-10%", th->text_secondary, th->background,
                     &font_small);
}

static void scan_dynamic(float mv, int8_t dec, bool steady, const theme_t *th)
{
    float a = mv < 0 ? -mv : mv;
    uint8_t st = dec == -2 ? 3 : (dec < 0 ? 0 : (!steady ? 4
               : (a >= fuse_scan_threshold_mv ? 2 : 1)));
    if (st != fz.scan_state) {
        static const char *const word[5] = { "---", "NO DRAW", "DRAW", "DC V?", "WAIT" };
        static const char *const sub[5]  = { "touch the probes to a fuse",
                                             "move to the next fuse",
                                             "current is flowing",
                                             "set Function to DC V",
                                             "hold the probes on the fuse" };
        uint16_t bg = st == 2 ? th->warning : (st == 1 ? th->success : th->grid);
        uint16_t fg = (st == 1 || st == 2) ? th->background : th->text_primary;
        rect(RX, FUSE_TOP + 4, RW, SCAN_BOX_H, bg);
        font_draw_string_center(RX + RW / 2, FUSE_TOP + 16, word[st], fg, bg, &font_large);
        font_draw_string_center(RX + RW / 2, FUSE_TOP + 48, sub[st], fg, bg, &font_small);
        fz.scan_state = st;
    }

    uint8_t r[4];
    uint8_t n = fuse_scan_ratings((fuse_type_t)fuse_type, r);
    uint8_t sel = current_entry()->rating_amps;
    char b[16];
    for (uint8_t i = 0; i < n; i++) {
        uint16_t x;
        scan_cell_xy(i, &x);
        uint16_t bg = r[i] == sel ? th->menu_selected_bg : th->grid;
        if (dec >= 0)
            fmt_current_short(b, sizeof b,
                              fuse_current_ma(mv, fuse_lookup_resistance_uohm((fuse_type_t)fuse_type, r[i])),
                              true);
        else
            snprintf(b, sizeof b, "--");
        font_draw_string_box((uint16_t)(x + 1), CELL_Y + 18, CELL_W - 2, b, th->text_primary, bg,
                             &font_small, FONT_ALIGN_CENTER);
    }

    fmt_drop(b, sizeof b, mv, dec);
    font_draw_string_box(VAL_X, 158, VAL_W, b, th->text_primary, th->background,
                         &font_medium, FONT_ALIGN_RIGHT);
}

/* ═══════════════════════════════════════════════════════════════════
 * Types -- "which fuse is this?" (static only)
 * ═══════════════════════════════════════════════════════════════════ */

#define TYPES_NUM    40          /* 2.5 px per mm */
#define TYPES_BASE   126         /* front views stand on this line */
#define TYPES_TOP_Y  130

static void types_static(const theme_t *th)
{
    static const char *const short_name[FUSE_TYPE_COUNT] = {
        "ATO", "Mini", "Micro2", "Maxi", "J-Case",
    };
    uint8_t sel = fuse_type < FUSE_TYPE_COUNT ? fuse_type : 0;

    font_draw_string(4, FUSE_TOP + 3, "Which fuse is this?", th->text_primary,
                     th->background, &font_medium);
    font_draw_string_right(LCD_WIDTH - 4, FUSE_TOP + 6, "front, top, to scale",
                           th->text_secondary, th->background, &font_small);

    int total = 0;
    for (int t = 0; t < FUSE_TYPE_COUNT; t++)
        total += fuse_shape((fuse_type_t)t)->w * TYPES_NUM / XF_DEN;
    int gap = (LCD_WIDTH - total) / (FUSE_TYPE_COUNT + 1);
    int x = gap;
    for (int t = 0; t < FUSE_TYPE_COUNT; t++) {
        const fuse_shape_t *s = fuse_shape((fuse_type_t)t);
        int w = s->w * TYPES_NUM / XF_DEN;
        uint32_t body = fuse_type_rgb((fuse_type_t)t);
        xf_t f = { x, TYPES_BASE - s->h * TYPES_NUM / XF_DEN, TYPES_NUM };
        draw_front(&f, (uint8_t)t, body, NULL, th->background);
        f.oy = TYPES_TOP_Y;
        draw_top(&f, (uint8_t)t, body, th->background);

        bool on = (t == sel);
        uint16_t cx = (uint16_t)(x + w / 2);
        font_draw_string_center(cx, 158, short_name[t],
                                on ? th->highlight : th->text_secondary, th->background,
                                &font_small);
        if (on) {
            uint16_t nw = font_string_width(short_name[t], &font_small);
            rect(cx - nw / 2, 171, nw, 2, th->highlight);
        }
        x += w + gap;
    }

    /* Rating colours of the selected type; the selected rating outlined. */
    const fuse_table_t *tbl = &fuse_tables[sel];
    int n = tbl->count;
    int cw = (LCD_WIDTH - 8 - (n - 1) * 2) / n;
    if (cw > 28) cw = 28;
    int cx0 = (LCD_WIDTH - (n * cw + (n - 1) * 2)) / 2;
    for (int i = 0; i < n; i++) {
        const fuse_entry_t *e = &tbl->entries[i];
        int x0 = cx0 + i * (cw + 2);
        char lbl[8];
        fuse_rating_label(e->rating_amps, lbl, sizeof lbl);
        uint32_t rgb;
        if (i == fuse_rating_idx)
            rect(x0 - 1, 176, cw + 2, 18, th->text_primary);
        if (fuse_rating_rgb((fuse_type_t)sel, e->rating_amps, &rgb)) {
            rect(x0, 177, cw, 16, c565(edge_of(rgb)));
            rect(x0 + 1, 178, cw - 2, 14, c565(rgb));
            font_draw_string_center((uint16_t)(x0 + cw / 2), 179, lbl,
                                    is_light(rgb) ? c565(0x101317) : c565(0xFFFFFF),
                                    c565(rgb), &font_small);
        } else {                                   /* no standard colour */
            rect(x0, 177, cw, 16, th->text_secondary);
            rect(x0 + 1, 178, cw - 2, 14, th->background);
            font_draw_string_center((uint16_t)(x0 + cw / 2), 179, lbl,
                                    th->text_secondary, th->background, &font_small);
        }
    }
    font_draw_string_center(LCD_WIDTH / 2, 197, "Read the number on top; colours vary.",
                            th->text_secondary, th->background, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void draw_fuse_screen(float drop_mv, int8_t drop_decimals, bool steady, bool repaint)
{
    const theme_t *th = theme_get();
    uint8_t view = fuse_view < FUSE_VIEW_COUNT ? fuse_view : FUSE_VIEW_DETAIL;
    uint8_t theme = (uint8_t)theme_get_id();

    if (repaint || !fz.valid || fz.view != view || fz.type != fuse_type ||
        fz.rating != fuse_rating_idx || fz.theme != theme) {
        lcd_fill_rect(0, FUSE_TOP, LCD_WIDTH, FUSE_BOTTOM - FUSE_TOP, th->background);
        fz.valid = true;
        fz.view = view;
        fz.type = fuse_type;
        fz.rating = fuse_rating_idx;
        fz.theme = theme;
        fz.unit = 0xFF;
        fz.scan_state = 0xFF;
        switch (view) {
        case FUSE_VIEW_MULTI: table_static(th);  break;
        case FUSE_VIEW_SCAN:  scan_static(th);   break;
        case FUSE_VIEW_TYPES: types_static(th);  break;
        default:              detail_static(th); break;
        }
    }

    switch (view) {
    case FUSE_VIEW_MULTI: table_dynamic(drop_mv, drop_decimals, steady, th);  break;
    case FUSE_VIEW_SCAN:  scan_dynamic(drop_mv, drop_decimals, steady, th);   break;
    case FUSE_VIEW_TYPES: break;
    default:              detail_dynamic(drop_mv, drop_decimals, steady, th); break;
    }
}

void fuse_cycle_view(void)
{
    fuse_view = (uint8_t)((fuse_view + 1) % FUSE_VIEW_COUNT);
}

static void clamp_threshold(void)
{
    if (fuse_scan_threshold_mv < FUSE_SCAN_THRESH_MIN_MV) fuse_scan_threshold_mv = FUSE_SCAN_THRESH_MIN_MV;
    if (fuse_scan_threshold_mv > FUSE_SCAN_THRESH_MAX_MV) fuse_scan_threshold_mv = FUSE_SCAN_THRESH_MAX_MV;
}

/* UP: a larger rating (the threshold, in Scan). Stops at the end. */
void fuse_next_rating(void)
{
    if (fuse_view == FUSE_VIEW_SCAN) {
        fuse_scan_threshold_mv += FUSE_SCAN_THRESH_STEP;
        clamp_threshold();
        return;
    }
    if (fuse_rating_idx + 1 < current_table()->count) fuse_rating_idx++;
}

void fuse_prev_rating(void)
{
    if (fuse_view == FUSE_VIEW_SCAN) {
        fuse_scan_threshold_mv -= FUSE_SCAN_THRESH_STEP;
        clamp_threshold();
        return;
    }
    if (fuse_rating_idx > 0) fuse_rating_idx--;
}

/* Rating softkey: the next rating, wrapping. */
void fuse_rating_press(void)
{
    fuse_rating_idx = (uint8_t)((fuse_rating_idx + 1) % current_table()->count);
}

/* Draw-if softkey: the next preset. */
void fuse_threshold_press(void)
{
    fuse_scan_threshold_mv = fuse_scan_threshold_next_preset(fuse_scan_threshold_mv);
}

/* A new type keeps the same rating in amps when it has one, else the
 * nearest below (or the smallest). */
static void keep_rating(uint8_t old_type, uint8_t old_idx)
{
    const fuse_table_t *ot = &fuse_tables[old_type < FUSE_TYPE_COUNT ? old_type : 0];
    uint8_t amps = ot->entries[old_idx < ot->count ? old_idx : 0].rating_amps;
    const fuse_table_t *nt = current_table();
    uint8_t best = 0;
    for (uint8_t i = 0; i < nt->count; i++)
        if (nt->entries[i].rating_amps <= amps) best = i;
    fuse_rating_idx = best;
}

void fuse_next_type(void)
{
    uint8_t ot = fuse_type, oi = fuse_rating_idx;
    fuse_type = (uint8_t)((fuse_type + 1) % FUSE_TYPE_COUNT);
    keep_rating(ot, oi);
}

void fuse_prev_type(void)
{
    uint8_t ot = fuse_type, oi = fuse_rating_idx;
    fuse_type = fuse_type == 0 ? FUSE_TYPE_COUNT - 1 : (uint8_t)(fuse_type - 1);
    keep_rating(ot, oi);
}
