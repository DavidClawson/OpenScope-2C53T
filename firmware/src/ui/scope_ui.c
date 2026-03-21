/*
 * OpenScope 2C53T - Oscilloscope UI
 */

#include "ui.h"
#include "lcd.h"
#include <math.h>

/* Draw the oscilloscope grid */
void draw_scope_grid(void)
{
    uint16_t x, y;
    for (x = 0; x < LCD_WIDTH; x += 32) {
        for (y = 18; y < LCD_HEIGHT - 16; y += 2) {
            lcd_set_pixel(x, y, COLOR_GRID);
        }
    }
    for (y = 18; y < LCD_HEIGHT - 16; y += 26) {
        for (x = 0; x < LCD_WIDTH; x += 2) {
            lcd_set_pixel(x, y, COLOR_GRID);
        }
    }

    /* Center crosshair (solid) */
    for (x = 0; x < LCD_WIDTH; x++) {
        lcd_set_pixel(x, (LCD_HEIGHT - 32) / 2 + 16, COLOR_GRID_CENTER);
    }
    for (y = 18; y < LCD_HEIGHT - 16; y++) {
        lcd_set_pixel(LCD_WIDTH / 2, y, COLOR_GRID_CENTER);
    }
}

/* Draw a simulated sine waveform for CH1 */
void draw_demo_waveform(uint32_t frame)
{
    static const int8_t sin_lut[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    uint16_t y_center = (LCD_HEIGHT - 32) / 2 + 16;
    uint16_t x;

    /* CH1: sine wave (yellow) */
    for (x = 0; x < LCD_WIDTH; x++) {
        uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
        int16_t y = y_center - (sin_lut[idx] * 40 / 100);
        if (y >= 18 && y < LCD_HEIGHT - 16) {
            lcd_set_pixel(x, (uint16_t)y, COLOR_CH1);
            if (y + 1 < LCD_HEIGHT - 16) lcd_set_pixel(x, (uint16_t)(y + 1), COLOR_CH1);
        }
    }

    /* CH2: square wave (cyan) */
    for (x = 0; x < LCD_WIDTH; x++) {
        uint8_t phase = (uint8_t)((x * 4 + frame) & 0x3F);
        int16_t y = y_center + 50 + (phase < 32 ? -25 : 25);
        if (y >= 18 && y < LCD_HEIGHT - 16) {
            lcd_set_pixel(x, (uint16_t)y, COLOR_CH2);
        }
        if (x > 0) {
            uint8_t prev_phase = (uint8_t)(((x - 1) * 4 + frame) & 0x3F);
            if ((prev_phase < 32) != (phase < 32)) {
                int16_t y1 = y_center + 50 - 25;
                int16_t y2 = y_center + 50 + 25;
                for (int16_t yy = y1; yy <= y2; yy++) {
                    if (yy >= 18 && yy < LCD_HEIGHT - 16) {
                        lcd_set_pixel(x, (uint16_t)yy, COLOR_CH2);
                    }
                }
            }
        }
    }

    /* Trigger marker */
    lcd_fill_rect(LCD_WIDTH - 6, y_center - 3, 5, 7, COLOR_TRIGGER);
}

/* Draw the oscilloscope screen */
void draw_scope_screen(uint32_t frame)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    draw_scope_grid();
    draw_demo_waveform(frame);

    /* Measurements overlay */
    lcd_draw_string(4, 18, "Freq:10.00kHz", COLOR_CH1, COLOR_BLACK);
    lcd_draw_string(4, 34, "Vpp:3.31V", COLOR_CH1, COLOR_BLACK);
    lcd_draw_string(LCD_WIDTH - 104, 18, "Freq:9.96kHz", COLOR_CH2, COLOR_BLACK);
    lcd_draw_string(LCD_WIDTH - 88, 34, "Vpp:660mV", COLOR_CH2, COLOR_BLACK);
}

#ifdef FEATURE_FFT

/* Format a float frequency as a readable string (no sprintf) */
static void format_freq(float freq_hz, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (freq_hz >= 1000000.0f) {
        val = freq_hz / 1000000.0f;
        unit = "MHz";
    } else if (freq_hz >= 1000.0f) {
        val = freq_hz / 1000.0f;
        unit = "kHz";
    } else {
        val = freq_hz;
        unit = "Hz";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 10.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + integer / 100);
    if (integer >= 10  && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac);

    while (*unit && pos < bufsize - 1)
        buf[pos++] = *unit++;
    buf[pos] = '\0';
}

/* Draw the FFT spectrum display */
void draw_fft_screen(void)
{
    const fft_config_t *cfg = fft_get_config();

    const uint16_t fft_y_top = 18;
    const uint16_t fft_y_bot = LCD_HEIGHT - 18;
    const uint16_t fft_height = fft_y_bot - fft_y_top;
    const uint16_t fft_x_left = 0;
    const uint16_t fft_x_right = LCD_WIDTH;
    const uint16_t fft_width = fft_x_right - fft_x_left;

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Generate test signal: 1kHz square at 44.1kHz sample rate */
    test_signal_generate(TEST_SIG_SQUARE, fft_sample_buf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);

    fft_process(fft_sample_buf, FFT_SIZE, &fft_result);

    /* Draw frequency bars */
    float ref_db = cfg->ref_level_db;
    float range_db = cfg->db_range;
    uint16_t x;

    for (x = 0; x < fft_width; x++) {
        uint16_t bin = (uint16_t)((uint32_t)x * fft_result.num_bins / fft_width);
        if (bin >= fft_result.num_bins) bin = fft_result.num_bins - 1;
        if (bin == 0) bin = 1;

        float db = fft_result.magnitude_db[bin];
        float normalized = (ref_db - db) / range_db;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        uint16_t bar_top = fft_y_top + (uint16_t)(normalized * (float)fft_height);

        if (bar_top < fft_y_bot) {
            uint16_t color;
            if (normalized < 0.25f)
                color = COLOR_CH1;
            else if (normalized < 0.5f)
                color = COLOR_GREEN;
            else if (normalized < 0.75f)
                color = COLOR_CYAN;
            else
                color = COLOR_GRID;

            lcd_fill_rect(fft_x_left + x, bar_top, 1, fft_y_bot - bar_top, color);
        }
    }

    /* Horizontal grid lines (every 10dB) */
    float db_step = 10.0f;
    float db;
    for (db = ref_db; db > ref_db - range_db; db -= db_step) {
        float normalized = (ref_db - db) / range_db;
        uint16_t y = fft_y_top + (uint16_t)(normalized * (float)fft_height);
        if (y > fft_y_top && y < fft_y_bot) {
            for (x = 0; x < fft_width; x += 4) {
                lcd_set_pixel(fft_x_left + x, y, COLOR_GRID);
            }
        }
    }

    /* dB scale labels on left */
    for (db = ref_db; db > ref_db - range_db; db -= 20.0f) {
        float normalized = (ref_db - db) / range_db;
        uint16_t y = fft_y_top + (uint16_t)(normalized * (float)fft_height);
        if (y > fft_y_top + 8 && y < fft_y_bot - 8) {
            int db_int = (int)db;
            char label[8];
            int pos = 0;
            if (db_int < 0) { label[pos++] = '-'; db_int = -db_int; }
            if (db_int >= 100) label[pos++] = (char)('0' + db_int / 100);
            if (db_int >= 10)  label[pos++] = (char)('0' + (db_int / 10) % 10);
            label[pos++] = (char)('0' + db_int % 10);
            label[pos] = '\0';
            lcd_draw_string(2, y - 7, label, COLOR_GRAY, COLOR_BLACK);
        }
    }

    /* Peak markers and labels */
    uint8_t p;
    for (p = 0; p < fft_result.num_peaks && p < 3; p++) {
        uint16_t peak_x = (uint16_t)((uint32_t)fft_result.peaks[p].bin
                          * fft_width / fft_result.num_bins);
        float norm = (ref_db - fft_result.peaks[p].magnitude_db) / range_db;
        if (norm < 0.0f) norm = 0.0f;
        uint16_t peak_y = fft_y_top + (uint16_t)(norm * (float)fft_height);

        peak_x += fft_x_left;
        if (peak_y >= fft_y_top + 4 && peak_x > 2 && peak_x < fft_x_right - 2) {
            lcd_set_pixel(peak_x, peak_y - 3, COLOR_RED);
            lcd_set_pixel(peak_x - 1, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x,     peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x + 1, peak_y - 2, COLOR_RED);
        }

        if (p == 0) {
            char freq_str[16];
            format_freq(fft_result.peaks[0].freq_hz, freq_str, sizeof(freq_str));
            lcd_draw_string(fft_x_left + 4, fft_y_top + 2,
                            freq_str, COLOR_WHITE, COLOR_BLACK);
        }
    }

    /* Window type label */
    const char *win_names[] = { "Rect", "Hann", "Hamm" };
    const char *win_name = (cfg->window < FFT_WINDOW_COUNT)
                           ? win_names[cfg->window] : "?";
    lcd_draw_string(LCD_WIDTH - 40, fft_y_top + 2,
                    win_name, COLOR_GRAY, COLOR_BLACK);
}

#endif /* FEATURE_FFT */
