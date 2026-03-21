/*
 * OpenScope 2C53T - Signal Generator UI
 */

#include "ui.h"
#include "lcd.h"

/* Draw the signal generator screen */
void draw_siggen_screen(uint32_t frame)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Waveform preview */
    uint16_t y_center = 80;
    for (uint16_t x = 20; x < LCD_WIDTH - 20; x++) {
        static const int8_t sin_lut[64] = {
             0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
            100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
             0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
           -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
        };
        uint8_t idx = (uint8_t)((x * 3 + frame) & 0x3F);
        int16_t y = y_center - (sin_lut[idx] * 30 / 100);
        lcd_set_pixel(x, (uint16_t)y, COLOR_CH1);
    }

    /* Settings */
    lcd_draw_string(20, 130, "Waveform: Sine", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 148, "Frequency: 1.000 kHz", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 166, "Amplitude: 3.3 Vpp", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 184, "Offset:    0.0 V", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 202, "Output:    ON", COLOR_GREEN, COLOR_BLACK);
}
