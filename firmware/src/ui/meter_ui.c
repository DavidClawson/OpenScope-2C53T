/*
 * OpenScope 2C53T - Multimeter UI
 */

#include "ui.h"
#include "lcd.h"

/* Draw the multimeter screen */
void draw_meter_screen(void)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Big reading */
    lcd_draw_string(60, 60, "13.82", COLOR_WHITE, COLOR_BLACK);
    /* Scale it up by drawing twice shifted */
    lcd_draw_string(61, 60, "13.82", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(60, 61, "13.82", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(61, 61, "13.82", COLOR_WHITE, COLOR_BLACK);

    lcd_draw_string(170, 65, "V DC", COLOR_YELLOW, COLOR_BLACK);

    /* Bar graph */
    lcd_fill_rect(20, 120, 280, 12, COLOR_DARK_GRAY);
    lcd_fill_rect(20, 120, 180, 12, COLOR_GREEN);

    /* Secondary readings */
    lcd_draw_string(20, 150, "MIN: 13.79V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(20, 168, "MAX: 13.86V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(20, 186, "AVG: 13.82V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(180, 150, "Range: 20V", COLOR_YELLOW, COLOR_BLACK);
}
