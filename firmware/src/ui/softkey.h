/*
 * Softkey bar (docs/specs/platform/softkey-ui.md).
 *
 * The four buttons directly under the screen -- MOVE, SELECT, TRIGGER, PRM,
 * left to right -- are labelled by a bar along the bottom edge of the screen,
 * each cell sitting over its button. A screen describes its bar as a table;
 * this module draws it and routes presses.
 *
 * The rule that keeps it honest (the inert-controls lesson, EXP-70): a slot
 * with a label MUST have a press handler, and a slot without one is drawn
 * empty. softkey_bar_valid() checks a table; host tests call it.
 */
#ifndef SOFTKEY_H
#define SOFTKEY_H

#include <stdbool.h>
#include <stdint.h>
#include "ui.h"
#include "lcd.h"

#define SOFTKEY_N       4
#define SOFTKEY_BAR_H   30
#define SOFTKEY_BAR_Y   (LCD_HEIGHT - SOFTKEY_BAR_H)
#define SOFTKEY_CELL_W  (LCD_WIDTH / SOFTKEY_N)
#define SOFTKEY_VALUE_MAX 14

typedef struct {
    const char *label;                          /* NULL = empty slot */
    void (*value)(char *buf, uint8_t n);        /* current value text; NULL = none */
    void (*press)(void);                        /* required when label != NULL */
    bool (*active)(void);                       /* highlighted (arrows act on it); NULL = never */
} softkey_t;

typedef struct {
    softkey_t key[SOFTKEY_N];
} softkey_bar_t;

/* Map a physical button to a softkey slot: MOVE 0, SELECT 1, TRIGGER 2,
 * PRM 3; -1 for any other button. */
int8_t softkey_slot_for_button(button_id_t b);

/* Draw the whole bar. Flicker-free: every pixel written once, no blanking. */
void softkey_bar_draw(const softkey_bar_t *bar);

/* Run slot `slot`'s handler. False if the slot is empty (nothing happened). */
bool softkey_bar_press(const softkey_bar_t *bar, int8_t slot);

/* Every labelled slot has a handler, and no handler hides behind an empty
 * label. */
bool softkey_bar_valid(const softkey_bar_t *bar);

/* Hash of everything the bar shows, for redraw gating. */
uint32_t softkey_bar_epoch(const softkey_bar_t *bar);

#endif /* SOFTKEY_H */
