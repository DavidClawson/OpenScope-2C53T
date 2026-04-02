/*
 * OpenScope 2C53T - Status Bar and Splash Screen UI
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "scope_state.h"
#include "battery.h"
#include "at32f403a_407.h"
#include <stdio.h>

/* Track whether status bar has been drawn once (first draw needs full clear) */
static uint8_t status_bar_drawn = 0;

/* Draw the top status bar */
void draw_status_bar(void)
{
    const theme_t *th = theme_get();

    /* Full clear only on first draw or mode change */
    if (!status_bar_drawn) {
        lcd_fill_rect(0, 0, LCD_WIDTH, 16, th->status_bar_bg);
        font_draw_string(4, 2, "OpenScope", th->ch2, th->status_bar_bg, &font_small);
        font_draw_string_right(LCD_WIDTH - 50, 2, "2C53T",
                               th->text_primary, th->status_bar_bg, &font_small);
        status_bar_drawn = 1;
    }

    /* Mode indicator (pad to fixed width to overwrite old text) */
    const char *mode_names[] = { "SCOPE ", "METER ", "SIGGEN", "SETUP " };
    font_draw_string(120, 2, mode_names[current_mode],
                     th->success, th->status_bar_bg, &font_small);

    /* Battery status */
    {
        char buf[12];
        if (battery_is_critical()) {
            /* Flash LOW warning — alternates red/dark each second */
            uint16_t color = (uptime_seconds & 1) ? th->warning : th->status_bar_bg;
            snprintf(buf, sizeof(buf), " LOW");
            font_draw_string_right(LCD_WIDTH - 4, 2, buf,
                                   color, th->status_bar_bg, &font_small);
        } else if (battery_is_charging()) {
            snprintf(buf, sizeof(buf), " CHG");
            font_draw_string_right(LCD_WIDTH - 4, 2, buf,
                                   th->success, th->status_bar_bg, &font_small);
        } else {
            uint8_t pct = battery_percent();
            snprintf(buf, sizeof(buf), "%3d%%", pct);
            uint16_t bat_color = (pct <= 15) ? th->warning :
                                 (pct <= 30) ? 0xFE00 : th->success;
            font_draw_string_right(LCD_WIDTH - 4, 2, buf,
                                   bat_color, th->status_bar_bg, &font_small);
        }
    }

    /* Auto-shutdown at critical battery — protect the LiPo cell */
    if (battery_is_critical()) {
        static uint8_t shutdown_timer = 0;
        shutdown_timer++;
        if (shutdown_timer >= 30) {  /* 30 seconds of critical = shutdown */
            lcd_fill_rect(40, 80, 240, 80, 0x0000);
            font_draw_string_center(160, 95, "Battery Critical",
                                    th->warning, 0x0000, &font_large);
            font_draw_string_center(160, 130, "Shutting down...",
                                    th->text_secondary, 0x0000, &font_medium);
            /* Brief delay so user can see the message */
            for (volatile uint32_t d = 0; d < 100000000; d++);
            GPIOC->clr = (1U << 9);  /* PC9 LOW = power off */
            while (1);
        }
    }
}

/* Force full redraw on next status bar update (call on theme/mode change) */
void status_bar_invalidate(void)
{
    status_bar_drawn = 0;
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
        font_draw_string(4, LCD_HEIGHT - 14, meter_submode_name(meter_submode),
                         th->ch1, ib, &font_small);
        font_draw_string(140, LCD_HEIGHT - 14, "<L/R>",
                         th->text_secondary, ib, &font_small);
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
