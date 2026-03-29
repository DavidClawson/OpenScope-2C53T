/*
 * OpenScope 2C53T - Signal Generator UI
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "signal_gen.h"

/* Draw the signal generator screen */
void draw_siggen_screen(uint32_t frame)
{
    (void)frame;
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    const siggen_config_t *cfg = siggen_get_config();

    /* Waveform preview: generate samples and plot */
    static int16_t preview_buf[280];
    float preview_rate = cfg->frequency_hz * 280.0f / 2.5f;
    if (preview_rate < 1.0f) preview_rate = 1.0f;

    siggen_fill_buffer(preview_buf, 280, preview_rate);

    uint16_t y_center = 75;
    for (uint16_t x = 0; x < 280; x++) {
        int16_t y_off = (int16_t)(((int32_t)preview_buf[x] * 30) / 32767);
        uint16_t py = (uint16_t)(y_center - y_off);
        if (py >= 18 && py < LCD_HEIGHT - 16) {
            lcd_set_pixel(x + 20, py, COLOR_CH1);
            if (py + 1 < LCD_HEIGHT - 16)
                lcd_set_pixel(x + 20, py + 1, COLOR_CH1);
        }
    }

    /* Parameter display below waveform */
    uint16_t param_y = 120;
    uint16_t label_x = 20;
    uint16_t value_x = 120;

    /* Waveform name */
    static const char *wave_names[] = { "Sine", "Square", "Triangle", "Sawtooth" };
    const char *wname = (cfg->waveform < SIGGEN_WAVEFORM_COUNT)
                        ? wave_names[cfg->waveform] : "?";

    font_draw_string(label_x, param_y, "Waveform",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string(value_x, param_y, wname,
                     COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* Frequency */
    char freq_str[16];
    int i = 0;
    float f = cfg->frequency_hz;
    if (f >= 1000.0f) {
        int fk = (int)(f / 1000.0f);
        int fd = ((int)f % 1000) / 100;
        if (fk >= 10) freq_str[i++] = (char)('0' + fk / 10);
        freq_str[i++] = (char)('0' + fk % 10);
        freq_str[i++] = '.';
        freq_str[i++] = (char)('0' + fd);
        freq_str[i++] = ' ';
        freq_str[i++] = 'k'; freq_str[i++] = 'H'; freq_str[i++] = 'z';
    } else {
        int fi = (int)f;
        if (fi >= 100) freq_str[i++] = (char)('0' + fi / 100);
        if (fi >= 10) freq_str[i++] = (char)('0' + (fi / 10) % 10);
        freq_str[i++] = (char)('0' + fi % 10);
        freq_str[i++] = ' ';
        freq_str[i++] = 'H'; freq_str[i++] = 'z';
    }
    freq_str[i] = '\0';

    font_draw_string(label_x, param_y + 22, "Frequency",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string(value_x, param_y + 20, freq_str,
                     COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* Amplitude */
    font_draw_string(label_x, param_y + 44, "Amplitude",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string(value_x, param_y + 42, "3.3 Vpp",
                     COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* Offset */
    font_draw_string(label_x, param_y + 66, "Offset",
                     COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string(value_x, param_y + 64, "0.0 V",
                     COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* Output state - prominent */
    if (cfg->output_enabled) {
        font_draw_string(value_x, param_y + 88, "OUTPUT ON",
                         COLOR_GREEN, COLOR_BLACK, &font_large);
    } else {
        font_draw_string(value_x, param_y + 88, "OUTPUT OFF",
                         COLOR_RED, COLOR_BLACK, &font_large);
    }
}
