/*
 * OpenScope 2C53T - Settings UI
 *
 * Navigable menu with UP/DOWN selection and new font system.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "watchdog.h"
#include <stdio.h>

/* Menu item labels (index 3 is dynamic — theme name appended at draw time) */
static const char *settings_items[SETTINGS_ITEM_COUNT] = {
    "Language: English",
    "Sound and Light",
    "Auto Shutdown",
    "Display Mode",     /* Shows ": <theme name>" dynamically */
    "Startup on Boot",
    "About",
    "Factory Reset",
};

/* Layout */
#define MENU_TOP        42
#define MENU_ITEM_H     24
#define MENU_LEFT       16
#define MENU_WIDTH      288

/* Draw the settings screen */
void draw_settings_screen(void)
{
    const theme_t *th = theme_get();

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, th->background);

    /* Title */
    font_draw_string_center(LCD_WIDTH / 2, 20, "Settings",
                            th->text_primary, th->background, &font_large);

    /* Menu items */
    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint16_t y = MENU_TOP + i * MENU_ITEM_H;
        bool selected = (i == settings_selected);

        if (y + MENU_ITEM_H > LCD_HEIGHT - 20) break;

        uint16_t bg = selected ? th->menu_selected_bg : th->background;

        /* Highlight bar for selected item */
        if (selected) {
            lcd_fill_rect(MENU_LEFT - 4, y, MENU_WIDTH + 8, MENU_ITEM_H - 2, bg);
        }

        /* Selection indicator */
        if (selected) {
            font_draw_string(MENU_LEFT, y + 4, ">",
                             th->highlight, bg, &font_medium);
        }

        /* Item text */
        uint16_t fg = selected ? th->text_primary : th->text_secondary;
        font_draw_string(MENU_LEFT + 16, y + 4, settings_items[i],
                         fg, bg, &font_medium);

        /* "Display Mode" item — append current theme name */
        if (i == 3) {
            font_draw_string(MENU_LEFT + 140, y + 4, th->name,
                             th->highlight, bg, &font_medium);
        }
    }
}

/* Draw task health diagnostics panel starting at given y position */
void draw_health_panel(uint16_t y_start)
{
    const task_health_t *tasks = health_get_tasks();
    int count = health_get_count();
    char buf[40];

    lcd_fill_rect(0, y_start - 4, LCD_WIDTH, LCD_HEIGHT - y_start + 4,
                  COLOR_BLACK);
    font_draw_string(10, y_start - 2, "TASK HEALTH",
                     COLOR_GRAY, COLOR_BLACK, &font_small);

    for (int i = 0; i < count && i < 4; i++) {
        uint16_t y = y_start + 14 + i * 16;
        bool stalled = health_is_stalled(i);
        uint16_t fg = stalled ? COLOR_RED : COLOR_GREEN;

        /* Task name + status indicator */
        snprintf(buf, sizeof(buf), "%-8s", tasks[i].name);
        font_draw_string(10, y, buf, fg, COLOR_BLACK, &font_small);

        /* Stack high water mark */
        snprintf(buf, sizeof(buf), "stk:%3lu",
                 (unsigned long)tasks[i].stack_hwm);
        font_draw_string(80, y, buf, COLOR_GRAY, COLOR_BLACK, &font_small);

        /* Status */
        font_draw_string(170, y, stalled ? "STALL" : "OK",
                         fg, COLOR_BLACK, &font_small);
    }
}
