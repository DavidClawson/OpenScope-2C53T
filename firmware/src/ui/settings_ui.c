/*
 * OpenScope 2C53T - Settings UI
 *
 * Navigable menu with UP/DOWN selection and new font system.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "watchdog.h"
#include <stdio.h>

/* Menu item labels */
static const char *settings_items[SETTINGS_ITEM_COUNT] = {
    "Language: English",
    "Sound and Light",
    "Auto Shutdown",
    "Display Mode",
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
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Title */
    font_draw_string_center(LCD_WIDTH / 2, 20, "Settings",
                            COLOR_WHITE, COLOR_BLACK, &font_large);

    /* Menu items */
    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint16_t y = MENU_TOP + i * MENU_ITEM_H;
        bool selected = (i == settings_selected);

        if (y + MENU_ITEM_H > LCD_HEIGHT - 20) break;

        /* Highlight bar for selected item */
        if (selected) {
            lcd_fill_rect(MENU_LEFT - 4, y, MENU_WIDTH + 8, MENU_ITEM_H - 2,
                          COLOR_SELECTED_BG);
        }

        /* Selection indicator */
        if (selected) {
            font_draw_string(MENU_LEFT, y + 4, ">",
                             COLOR_CYAN, COLOR_SELECTED_BG, &font_medium);
        }

        /* Item text */
        uint16_t fg = selected ? COLOR_WHITE : COLOR_GRAY;
        uint16_t bg = selected ? COLOR_SELECTED_BG : COLOR_BLACK;
        font_draw_string(MENU_LEFT + 16, y + 4, settings_items[i],
                         fg, bg, &font_medium);
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
