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

/* Draw FFT spectrum into a region (used by both full and split view) */
static void draw_fft_region(uint16_t y_top, uint16_t height)
{
    const fft_config_t *cfg = fft_get_config();
    uint16_t y_bot = y_top + height;

    /* Generate test signal and run FFT */
    test_signal_generate(TEST_SIG_SQUARE, fft_sample_buf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);
    fft_process(fft_sample_buf, FFT_SIZE, &fft_result);

    /* Use averaged data if available, otherwise raw */
    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.magnitude_db;

    float ref_db = cfg->ref_level_db;
    float range_db = cfg->db_range;
    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    uint16_t x;

    /* Draw frequency bars */
    for (x = 0; x < LCD_WIDTH; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / LCD_WIDTH);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;

        float db = draw_data[bin];
        float normalized = (ref_db - db) / range_db;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        uint16_t bar_top = y_top + (uint16_t)(normalized * (float)height);

        if (bar_top < y_bot) {
            uint16_t color;
            if (normalized < 0.25f)
                color = COLOR_CH1;
            else if (normalized < 0.5f)
                color = COLOR_GREEN;
            else if (normalized < 0.75f)
                color = COLOR_CYAN;
            else
                color = COLOR_GRID;

            lcd_fill_rect(x, bar_top, 1, y_bot - bar_top, color);
        }

        /* Max hold overlay (dotted line) */
        if (fft_result.max_hold_db != NULL) {
            float mh_norm = (ref_db - fft_result.max_hold_db[bin]) / range_db;
            if (mh_norm >= 0.0f && mh_norm <= 1.0f) {
                uint16_t mh_y = y_top + (uint16_t)(mh_norm * (float)height);
                if (mh_y >= y_top && mh_y < y_bot && (x & 1) == 0)
                    lcd_set_pixel(x, mh_y, COLOR_RED);
            }
        }
    }

    /* Horizontal grid lines (every 10dB) */
    float db;
    for (db = ref_db; db > ref_db - range_db; db -= 10.0f) {
        float normalized = (ref_db - db) / range_db;
        uint16_t y = y_top + (uint16_t)(normalized * (float)height);
        if (y > y_top && y < y_bot) {
            for (x = 0; x < LCD_WIDTH; x += 4)
                lcd_set_pixel(x, y, COLOR_GRID);
        }
    }

    /* dB scale labels (only if region tall enough) */
    if (height > 80) {
        for (db = ref_db; db > ref_db - range_db; db -= 20.0f) {
            float normalized = (ref_db - db) / range_db;
            uint16_t y = y_top + (uint16_t)(normalized * (float)height);
            if (y > y_top + 8 && y < y_bot - 8) {
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
    }

    /* Peak markers and frequency label */
    uint8_t p;
    for (p = 0; p < fft_result.num_peaks && p < 3; p++) {
        uint16_t peak_bin = fft_result.peaks[p].bin;
        if (peak_bin < zoom_start || peak_bin > zoom_end) continue;

        uint16_t peak_x = (uint16_t)((uint32_t)(peak_bin - zoom_start)
                          * LCD_WIDTH / zoom_span);
        float norm = (ref_db - fft_result.peaks[p].magnitude_db) / range_db;
        if (norm < 0.0f) norm = 0.0f;
        uint16_t peak_y = y_top + (uint16_t)(norm * (float)height);

        if (peak_y >= y_top + 4 && peak_x > 2 && peak_x < LCD_WIDTH - 2) {
            lcd_set_pixel(peak_x, peak_y - 3, COLOR_RED);
            lcd_set_pixel(peak_x - 1, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x,     peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x + 1, peak_y - 2, COLOR_RED);
        }

        if (p == 0) {
            char freq_str[16];
            format_freq(fft_result.peaks[0].freq_hz, freq_str, sizeof(freq_str));
            lcd_draw_string(4, y_top + 2, freq_str, COLOR_WHITE, COLOR_BLACK);
        }

        /* Draw harmonic label if present */
        if (fft_result.peaks[p].label[0] != '\0' && peak_x > 8 && peak_x < LCD_WIDTH - 30) {
            lcd_draw_string(peak_x - 8, peak_y - 12,
                            fft_result.peaks[p].label, COLOR_ORANGE, COLOR_BLACK);
        }
    }

    /* Window type label */
    const char *win_names[] = { "Rect", "Hann", "Hamm", "BHar", "Flat" };
    const char *win_name = (cfg->window < FFT_WINDOW_COUNT)
                           ? win_names[cfg->window] : "?";
    lcd_draw_string(LCD_WIDTH - 40, y_top + 2, win_name, COLOR_GRAY, COLOR_BLACK);
}

/* Draw full-screen FFT */
void draw_fft_screen(void)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);
    draw_fft_region(18, LCD_HEIGHT - 36);
}

/* Draw split view: time-domain top, FFT bottom */
void draw_split_screen(uint32_t frame)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Top half: scope waveform (y=18 to y=119, 101px) */
    /* Simplified scope grid for half height */
    uint16_t x, y;
    uint16_t scope_top = 18;
    uint16_t scope_bot = 119;
    uint16_t scope_mid = (scope_top + scope_bot) / 2;

    for (x = 0; x < LCD_WIDTH; x += 32) {
        for (y = scope_top; y < scope_bot; y += 2)
            lcd_set_pixel(x, y, COLOR_GRID);
    }
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, scope_mid, COLOR_GRID_CENTER);

    /* Draw waveform in top half */
    static const int8_t sin_lut[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };
    for (x = 0; x < LCD_WIDTH; x++) {
        uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
        int16_t wy = scope_mid - (sin_lut[idx] * 25 / 100);
        if (wy >= scope_top && wy < scope_bot)
            lcd_set_pixel(x, (uint16_t)wy, COLOR_CH1);
    }

    /* Divider line */
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, 120, COLOR_GRID_CENTER);

    /* Bottom half: FFT (y=121 to y=222, 101px) */
    draw_fft_region(121, 101);
}

/* ═══════════════════════════════════════════════════════════════════
 * Waterfall / Spectrogram display
 * ═══════════════════════════════════════════════════════════════════ */

#define WATERFALL_ROWS  64
#define WATERFALL_COLS  320
static uint8_t waterfall_buf[WATERFALL_ROWS][WATERFALL_COLS];
static uint8_t waterfall_row_idx = 0;

/* Heatmap: strong=red, weak=blue */
static uint16_t intensity_to_color(uint8_t intensity)
{
    uint8_t inv = 255 - intensity;  /* 255=strong, 0=weak */
    if (inv > 204) return COLOR_RED;
    if (inv > 153) return RGB565(255, (uint8_t)((inv - 153) * 5), 0);
    if (inv > 102) return RGB565((uint8_t)((153 - inv + 102) * 5), 255, 0);
    if (inv > 51)  return RGB565(0, 255, (uint8_t)((inv - 51) * 5));
    return RGB565(0, (uint8_t)(inv * 5), 255);
}

void draw_waterfall_screen(void)
{
    const fft_config_t *cfg = fft_get_config();

    /* Generate test signal and run FFT */
    test_signal_generate(TEST_SIG_SQUARE, fft_sample_buf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);
    fft_process(fft_sample_buf, FFT_SIZE, &fft_result);

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.magnitude_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    if (zoom_span == 0) zoom_span = 1;

    /* Convert magnitude to intensity for this row */
    uint16_t x;
    for (x = 0; x < WATERFALL_COLS; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / WATERFALL_COLS);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;

        float db = draw_data[bin];
        float normalized = (cfg->ref_level_db - db) / cfg->db_range;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        waterfall_buf[waterfall_row_idx][x] = (uint8_t)(normalized * 255.0f);
    }

    uint8_t newest_row = waterfall_row_idx;
    waterfall_row_idx = (uint8_t)((waterfall_row_idx + 1) % WATERFALL_ROWS);

    /* Clear and render */
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    uint16_t display_height = LCD_HEIGHT - 34;
    uint16_t row_height = display_height / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    uint8_t r;
    for (r = 0; r < WATERFALL_ROWS; r++) {
        uint8_t buf_row = (uint8_t)((newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS);
        uint16_t y = (uint16_t)(18 + r * row_height);

        if (y + row_height > LCD_HEIGHT - 18) break;

        for (x = 0; x < WATERFALL_COLS; x++) {
            uint16_t color = intensity_to_color(waterfall_buf[buf_row][x]);
            lcd_fill_rect(x, y, 1, row_height, color);
        }
    }

    lcd_draw_string(4, 18, "WFALL", COLOR_WHITE, COLOR_BLACK);
}

#endif /* FEATURE_FFT */
