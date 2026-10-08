/*
 * Softkey bar -- see softkey.h and docs/specs/platform/softkey-ui.md.
 */
#include "softkey.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"

int8_t softkey_slot_for_button(button_id_t b)
{
    switch (b) {
    case BTN_MOVE:    return 0;
    case BTN_SELECT:  return 1;
    case BTN_TRIGGER: return 2;
    case BTN_PRM:     return 3;
    default:          return -1;
    }
}

bool softkey_bar_valid(const softkey_bar_t *bar)
{
    for (int i = 0; i < SOFTKEY_N; i++) {
        const softkey_t *k = &bar->key[i];
        if (k->label && !k->press) return false;     /* a label that does nothing */
        if (!k->label && (k->press || k->value)) return false;  /* a hidden control */
    }
    return true;
}

bool softkey_bar_press(const softkey_bar_t *bar, int8_t slot)
{
    if (slot < 0 || slot >= SOFTKEY_N) return false;
    const softkey_t *k = &bar->key[slot];
    if (!k->label || !k->press) return false;
    k->press();
    return true;
}

uint32_t softkey_bar_epoch(const softkey_bar_t *bar)
{
    uint32_t h = 2166136261u;
    char v[SOFTKEY_VALUE_MAX];
    for (int i = 0; i < SOFTKEY_N; i++) {
        const softkey_t *k = &bar->key[i];
        h = (h ^ (uint32_t)(uintptr_t)k->label) * 16777619u;
        h = (h ^ (uint32_t)((k->active && k->active()) ? 1u : 0u)) * 16777619u;
        if (k->value) {
            v[0] = '\0';
            k->value(v, sizeof v);
            for (const char *p = v; *p; p++)
                h = (h ^ (uint8_t)*p) * 16777619u;
        }
    }
    return h;
}

void softkey_bar_draw(const softkey_bar_t *bar)
{
    const theme_t *th = theme_get();
    const uint16_t y0 = SOFTKEY_BAR_Y;
    const uint16_t hs = font_small.height;
    const uint16_t hm = font_medium.height;
    const uint16_t ly = y0 + 2;              /* label row */
    const uint16_t vy = ly + hs;             /* value row */
    const uint16_t tail = (vy + hm < LCD_HEIGHT) ? (uint16_t)(LCD_HEIGHT - (vy + hm)) : 0;
    char v[SOFTKEY_VALUE_MAX];

    lcd_fill_rect(0, y0, LCD_WIDTH, 1, th->grid_center);   /* top rule */

    for (int i = 0; i < SOFTKEY_N; i++) {
        const softkey_t *k = &bar->key[i];
        uint16_t x = (uint16_t)(i * SOFTKEY_CELL_W);
        uint16_t w = SOFTKEY_CELL_W - ((i < SOFTKEY_N - 1) ? 1 : 0);
        bool on = k->label && k->active && k->active();
        uint16_t bg = on ? th->menu_selected_bg : th->status_bar_bg;

        lcd_fill_rect(x, y0 + 1, w, 1, bg);                 /* row between rule and label */
        if (k->label) {
            v[0] = '\0';
            if (k->value) k->value(v, sizeof v);
            font_draw_string_box(x + 4, ly, w - 4, k->label,
                                 th->text_secondary, bg, &font_small, FONT_ALIGN_LEFT);
            font_draw_string_box(x + 4, vy, w - 4, v,
                                 th->text_primary, bg, &font_medium, FONT_ALIGN_LEFT);
            lcd_fill_rect(x, ly, 4, (uint16_t)(hs + hm), bg);  /* left padding */
        } else {
            lcd_fill_rect(x, ly, w, (uint16_t)(hs + hm), bg);
        }
        if (tail) lcd_fill_rect(x, vy + hm, w, tail, bg);
        if (i < SOFTKEY_N - 1)
            lcd_fill_rect(x + w, y0 + 1, 1, SOFTKEY_BAR_H - 1, th->grid_center);
    }
}
