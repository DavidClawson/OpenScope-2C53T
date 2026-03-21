/*
 * FNIRSI 2C53T Color Theme System
 *
 * 4 built-in themes optimized for different viewing conditions.
 * Colors are RGB565 for direct ST7789V LCD use.
 */

#include "theme.h"

static theme_id_t current_theme_id;

static const theme_t themes[THEME_COUNT] = {
    /* THEME_DARK_BLUE — original FNIRSI default */
    {
        .background      = 0x0000, /* black */
        .grid            = 0x18C3, /* dark gray */
        .grid_center     = 0x3186, /* lighter gray */
        .ch1             = 0xFFE0, /* yellow */
        .ch2             = 0x07FF, /* cyan */
        .trigger         = 0x07E0, /* green */
        .text_primary    = 0xFFFF, /* white */
        .text_secondary  = 0x8410, /* gray */
        .status_bar_bg   = 0x2104, /* dark gray */
        .menu_selected_bg = 0x2945,/* selected blue-gray */
        .highlight       = 0xFCA0, /* orange */
        .warning         = 0xF800, /* red */
        .success         = 0x07E0, /* green */
        .name            = "Dark Blue",
    },
    /* THEME_CLASSIC_GREEN — Tektronix-style */
    {
        .background      = 0x0000, /* black */
        .grid            = 0x0320, /* dark green */
        .grid_center     = 0x0540, /* medium green */
        .ch1             = 0x07E0, /* bright green */
        .ch2             = 0x07FF, /* cyan */
        .trigger         = 0xFFE0, /* yellow */
        .text_primary    = 0x07E0, /* green */
        .text_secondary  = 0x0320, /* dark green */
        .status_bar_bg   = 0x0200, /* very dark green */
        .menu_selected_bg = 0x0320,/* dark green */
        .highlight       = 0xFFE0, /* yellow */
        .warning         = 0xF800, /* red */
        .success         = 0x07E0, /* green */
        .name            = "Classic Green",
    },
    /* THEME_HIGH_CONTRAST — white background */
    {
        .background      = 0xFFFF, /* white */
        .grid            = 0xC618, /* light gray */
        .grid_center     = 0x8410, /* medium gray */
        .ch1             = 0x001F, /* blue */
        .ch2             = 0xF800, /* red */
        .trigger         = 0x07E0, /* green */
        .text_primary    = 0x0000, /* black */
        .text_secondary  = 0x8410, /* gray */
        .status_bar_bg   = 0xE71C, /* light gray */
        .menu_selected_bg = 0xC618,/* lighter gray */
        .highlight       = 0x001F, /* blue */
        .warning         = 0xF800, /* red */
        .success         = 0x07E0, /* green */
        .name            = "High Contrast",
    },
    /* THEME_NIGHT_RED — dark environment */
    {
        .background      = 0x0000, /* black */
        .grid            = 0x3000, /* dark red */
        .grid_center     = 0x5000, /* medium dark red */
        .ch1             = 0xF800, /* red */
        .ch2             = 0xFBE0, /* orange */
        .trigger         = 0xF800, /* red */
        .text_primary    = 0xF800, /* red */
        .text_secondary  = 0x3000, /* dark red */
        .status_bar_bg   = 0x2000, /* very dark red */
        .menu_selected_bg = 0x3000,/* dark red */
        .highlight       = 0xFBE0, /* orange */
        .warning         = 0xFFE0, /* yellow (stands out on red theme) */
        .success         = 0xFBE0, /* orange */
        .name            = "Night Red",
    },
};

void theme_init(theme_id_t id)
{
    if (id >= THEME_COUNT)
        id = THEME_DARK_BLUE;
    current_theme_id = id;
}

void theme_set(theme_id_t id)
{
    if (id >= THEME_COUNT)
        id = THEME_DARK_BLUE;
    current_theme_id = id;
}

theme_id_t theme_cycle(void)
{
    current_theme_id = (current_theme_id + 1) % THEME_COUNT;
    return current_theme_id;
}

const theme_t *theme_get(void)
{
    return &themes[current_theme_id];
}

theme_id_t theme_get_id(void)
{
    return current_theme_id;
}
