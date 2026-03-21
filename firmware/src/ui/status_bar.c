/*
 * OpenScope 2C53T - Status Bar and Splash Screen UI
 */

#include "ui.h"
#include "lcd.h"

/* Draw the top status bar */
void draw_status_bar(void)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, 16, COLOR_DARK_GRAY);

    lcd_draw_string(4, 1, "OpenScope", COLOR_CYAN, COLOR_DARK_GRAY);
    lcd_draw_string(LCD_WIDTH - 40, 1, "2C53T", COLOR_WHITE, COLOR_DARK_GRAY);

    /* Mode indicator */
    const char *mode_names[] = { "SCOPE", "METER", "SIGGEN", "SETUP" };
    lcd_draw_string(100, 1, mode_names[current_mode], COLOR_GREEN, COLOR_DARK_GRAY);

    /* Uptime display */
    char buf[16];
    int mins = uptime_seconds / 60;
    int secs = uptime_seconds % 60;
    /* Manual int-to-string to avoid printf/sprintf */
    buf[0] = '0' + (mins / 10);
    buf[1] = '0' + (mins % 10);
    buf[2] = ':';
    buf[3] = '0' + (secs / 10);
    buf[4] = '0' + (secs % 10);
    buf[5] = '\0';
    lcd_draw_string(200, 1, buf, COLOR_GRAY, COLOR_DARK_GRAY);
}

/* Draw the bottom info bar */
void draw_info_bar(void)
{
    lcd_fill_rect(0, LCD_HEIGHT - 16, LCD_WIDTH, 16, COLOR_DARK_GRAY);

    switch (current_mode) {
    case MODE_OSCILLOSCOPE:
#ifdef FEATURE_FFT
        if (scope_view == SCOPE_VIEW_FFT) {
            const char *win_names[] = { "Rect", "Hann", "Hamm", "BHar", "Flat" };
            const fft_config_t *cfg = fft_get_config();
            lcd_draw_string(4, LCD_HEIGHT - 15, "FFT 4096pt", COLOR_YELLOW, COLOR_DARK_GRAY);
            lcd_draw_string(100, LCD_HEIGHT - 15,
                            (cfg->window < FFT_WINDOW_COUNT) ? win_names[cfg->window] : "?",
                            COLOR_GREEN, COLOR_DARK_GRAY);
            if (cfg->avg_count > 0)
                lcd_draw_string(148, LCD_HEIGHT - 15, "AVG", COLOR_CYAN, COLOR_DARK_GRAY);
            if (cfg->max_hold)
                lcd_draw_string(184, LCD_HEIGHT - 15, "MH", COLOR_RED, COLOR_DARK_GRAY);
            lcd_draw_string(220, LCD_HEIGHT - 15, "PRM:View", COLOR_GRAY, COLOR_DARK_GRAY);
        } else
#endif
        {
            lcd_draw_string(4, LCD_HEIGHT - 15, "CH1:2V DC", COLOR_YELLOW, COLOR_DARK_GRAY);
            lcd_draw_string(100, LCD_HEIGHT - 15, "Auto", COLOR_GREEN, COLOR_DARK_GRAY);
            lcd_draw_string(160, LCD_HEIGHT - 15, "H=50uS", COLOR_WHITE, COLOR_DARK_GRAY);
            lcd_draw_string(240, LCD_HEIGHT - 15, "CH2:200mV", COLOR_CYAN, COLOR_DARK_GRAY);
        }
        break;
    case MODE_MULTIMETER:
        lcd_draw_string(4, LCD_HEIGHT - 15, "DC Voltage", COLOR_YELLOW, COLOR_DARK_GRAY);
        lcd_draw_string(200, LCD_HEIGHT - 15, "Auto Range", COLOR_GREEN, COLOR_DARK_GRAY);
        break;
    case MODE_SIGNAL_GEN:
        lcd_draw_string(4, LCD_HEIGHT - 15, "Sine 1.000kHz", COLOR_YELLOW, COLOR_DARK_GRAY);
        lcd_draw_string(200, LCD_HEIGHT - 15, "3.3Vpp", COLOR_GREEN, COLOR_DARK_GRAY);
        break;
    case MODE_SETTINGS:
        lcd_draw_string(4, LCD_HEIGHT - 15, "MENU/SELECT to navigate", COLOR_GRAY, COLOR_DARK_GRAY);
        break;
    default:
        break;
    }
}

/* Draw the splash screen */
void draw_splash(void)
{
    lcd_clear(COLOR_BLACK);

    /* Title */
    lcd_draw_string(72, 60, "OpenScope 2C53T", COLOR_CYAN, COLOR_BLACK);
    /* Double-up for bold effect */
    lcd_draw_string(73, 60, "OpenScope 2C53T", COLOR_CYAN, COLOR_BLACK);
    lcd_draw_string(72, 61, "OpenScope 2C53T", COLOR_CYAN, COLOR_BLACK);

    lcd_draw_string(72, 90, "Custom Firmware v0.1", COLOR_WHITE, COLOR_BLACK);

    lcd_draw_string(56, 130, "GD32F307 | FreeRTOS", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(64, 148, "ST7789V 320x240", COLOR_GRAY, COLOR_BLACK);

    lcd_draw_string(88, 190, "github.com/", COLOR_DARK_GRAY, COLOR_BLACK);
    lcd_draw_string(48, 208, "DavidClawson/OpenScope-2C53T", COLOR_DARK_GRAY, COLOR_BLACK);
}
