/*
 * OpenScope 2C53T - Settings UI
 */

#include "ui.h"
#include "lcd.h"

/* Draw the settings screen */
void draw_settings_screen(void)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    lcd_draw_string(110, 24, "SETTINGS", COLOR_WHITE, COLOR_BLACK);

    const char *items[] = {
        "Language: English",
        "Sound and Light",
        "Auto Shutdown",
        "Display Mode",
        "Startup on Boot",
        "About",
        "Factory Reset",
    };

    for (int i = 0; i < 7; i++) {
        uint16_t y = 50 + i * 24;
        uint16_t bg = (i == 0) ? COLOR_SELECTED_BG : COLOR_BLACK;
        uint16_t fg = (i == 0) ? COLOR_WHITE : COLOR_GRAY;
        lcd_fill_rect(20, y, 280, 20, bg);
        lcd_draw_string(28, y + 3, items[i], fg, bg);
    }
}
