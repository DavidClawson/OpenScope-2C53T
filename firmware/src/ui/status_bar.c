/*
 * OpenScope 2C53T - Status Bar and Splash Screen UI
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "scope_state.h"
#include <stdio.h>

/* Draw the top status bar */
void draw_status_bar(void)
{
    const theme_t *th = theme_get();
    lcd_fill_rect(0, 0, LCD_WIDTH, 16, th->status_bar_bg);

    font_draw_string(4, 2, "OpenScope", th->ch2, th->status_bar_bg, &font_small);

    /* Mode indicator */
    const char *mode_names[] = { "SCOPE", "METER", "SIGGEN", "SETUP" };
    font_draw_string(120, 2, mode_names[current_mode],
                     th->success, th->status_bar_bg, &font_small);

    /* Uptime display */
    char buf[16];
    int mins = uptime_seconds / 60;
    int secs = uptime_seconds % 60;
    buf[0] = '0' + (mins / 10);
    buf[1] = '0' + (mins % 10);
    buf[2] = ':';
    buf[3] = '0' + (secs / 10);
    buf[4] = '0' + (secs % 10);
    buf[5] = '\0';
    font_draw_string_right(LCD_WIDTH - 4, 2, buf,
                           th->text_secondary, th->status_bar_bg, &font_small);

    /* Model name */
    font_draw_string_right(LCD_WIDTH - 50, 2, "2C53T",
                           th->text_primary, th->status_bar_bg, &font_small);
}

/* Draw the bottom info bar */
void draw_info_bar(void)
{
    const theme_t *th = theme_get();
    lcd_fill_rect(0, LCD_HEIGHT - 16, LCD_WIDTH, 16, th->status_bar_bg);

    uint16_t ib = th->status_bar_bg;
    switch (current_mode) {
    case MODE_OSCILLOSCOPE:
#ifdef FEATURE_FFT
        if (scope_view == SCOPE_VIEW_FFT) {
            const char *win_names[] = { "Rect", "Hann", "Hamm", "BHar", "Flat" };
            const fft_config_t *cfg = fft_get_config();
            font_draw_string(4, LCD_HEIGHT - 14, "FFT 4096pt",
                             th->ch1, ib, &font_small);
            font_draw_string(80, LCD_HEIGHT - 14,
                             (cfg->window < FFT_WINDOW_COUNT) ? win_names[cfg->window] : "?",
                             th->success, ib, &font_small);
            if (cfg->avg_count > 0)
                font_draw_string(120, LCD_HEIGHT - 14, "AVG",
                                 th->ch2, ib, &font_small);
            if (cfg->max_hold)
                font_draw_string(150, LCD_HEIGHT - 14, "MH",
                                 th->warning, ib, &font_small);
            font_draw_string(220, LCD_HEIGHT - 14, "PRM:View",
                             th->text_secondary, ib, &font_small);
        } else
#endif
        {
            const scope_state_t *ss = scope_state_get();
            char buf[24];

            /* CH1: vdiv + coupling */
            snprintf(buf, sizeof(buf), "CH1:%s %s",
                     vdiv_table[ss->ch1.vdiv_idx].label,
                     coupling_labels[ss->ch1.coupling]);
            font_draw_string(4, LCD_HEIGHT - 14, buf,
                             ss->ch1.enabled ? th->ch1 : th->text_secondary,
                             ib, &font_small);

            /* Trigger mode */
            font_draw_string(108, LCD_HEIGHT - 14,
                             trigger_mode_labels[ss->trigger.mode],
                             th->success, ib, &font_small);

            /* Timebase */
            snprintf(buf, sizeof(buf), "H=%s",
                     timebase_table[ss->timebase_idx].label);
            font_draw_string(155, LCD_HEIGHT - 14, buf,
                             th->text_primary, ib, &font_small);

            /* CH2: vdiv + coupling */
            snprintf(buf, sizeof(buf), "CH2:%s %s",
                     vdiv_table[ss->ch2.vdiv_idx].label,
                     coupling_labels[ss->ch2.coupling]);
            font_draw_string(215, LCD_HEIGHT - 14, buf,
                             ss->ch2.enabled ? th->ch2 : th->text_secondary,
                             ib, &font_small);
        }
        break;
    case MODE_MULTIMETER:
        font_draw_string(4, LCD_HEIGHT - 14, "DC Voltage",
                         th->ch1, ib, &font_small);
        font_draw_string(200, LCD_HEIGHT - 14, "Auto Range",
                         th->success, ib, &font_small);
        break;
    case MODE_SIGNAL_GEN:
        font_draw_string(4, LCD_HEIGHT - 14, "Sine 1.000kHz",
                         th->ch1, ib, &font_small);
        font_draw_string(200, LCD_HEIGHT - 14, "3.3Vpp",
                         th->success, ib, &font_small);
        break;
    case MODE_SETTINGS:
        font_draw_string(4, LCD_HEIGHT - 14, "MENU/SELECT to navigate",
                         th->text_secondary, ib, &font_small);
        break;
    default:
        break;
    }
}

/* Draw the splash screen */
void draw_splash(void)
{
    lcd_clear(COLOR_BLACK);

    /* Title - large font, centered */
    font_draw_string_center(LCD_WIDTH / 2, 50, "OpenScope 2C53T",
                            COLOR_CYAN, COLOR_BLACK, &font_large);

    /* Version */
    font_draw_string_center(LCD_WIDTH / 2, 85, "Custom Firmware v0.1",
                            COLOR_WHITE, COLOR_BLACK, &font_medium);

    /* Hardware info */
    font_draw_string_center(LCD_WIDTH / 2, 125, "GD32F307 | FreeRTOS",
                            COLOR_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_center(LCD_WIDTH / 2, 142, "ST7789V 320x240",
                            COLOR_GRAY, COLOR_BLACK, &font_small);

    /* Repo */
    font_draw_string_center(LCD_WIDTH / 2, 190, "github.com/",
                            COLOR_DARK_GRAY, COLOR_BLACK, &font_small);
    font_draw_string_center(LCD_WIDTH / 2, 206, "DavidClawson/OpenScope-2C53T",
                            COLOR_DARK_GRAY, COLOR_BLACK, &font_small);
}
