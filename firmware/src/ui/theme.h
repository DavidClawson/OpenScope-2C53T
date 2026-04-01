/*
 * FNIRSI 2C53T Color Theme System
 *
 * Provides 4 built-in color themes for the oscilloscope UI.
 * All colors are RGB565 format for direct use with the ST7789V LCD.
 */

#ifndef THEME_H
#define THEME_H

#include <stdint.h>

typedef enum {
    THEME_DARK_BLUE = 0,    /* Current default */
    THEME_CLASSIC_GREEN,    /* Tektronix-style green on black */
    THEME_HIGH_CONTRAST,    /* White background, dark traces */
    THEME_NIGHT_RED,        /* Red on black for dark environments */
    THEME_COUNT
} theme_id_t;

typedef struct {
    uint16_t background;
    uint16_t grid;
    uint16_t grid_center;
    uint16_t ch1;
    uint16_t ch2;
    uint16_t trigger;
    uint16_t text_primary;
    uint16_t text_secondary;
    uint16_t status_bar_bg;
    uint16_t menu_selected_bg;
    uint16_t highlight;
    uint16_t warning;       /* Red for errors/warnings */
    uint16_t success;       /* Green for pass/OK */
    const char *name;
} theme_t;

/* Initialize theme system with the given theme */
void theme_init(theme_id_t id);

/* Switch to a specific theme */
void theme_set(theme_id_t id);

/* Cycle to the next/previous theme, wrapping around. Returns the new theme ID. */
theme_id_t theme_cycle(void);
theme_id_t theme_cycle_reverse(void);

/* Get pointer to current theme data */
const theme_t *theme_get(void);

/* Get current theme ID */
theme_id_t theme_get_id(void);

#endif /* THEME_H */
