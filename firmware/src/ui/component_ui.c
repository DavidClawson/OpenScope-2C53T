/*
 * OpenScope 2C53T - Component Tester UI
 *
 * Displays R/L/C/diode/continuity test results with large readings,
 * similar to the multimeter screen layout.
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "component_test.h"
#include <stdio.h>

/* Layout constants */
#define COMP_TOP        18
#define COMP_BOTTOM     (LCD_HEIGHT - 18)
#define MAIN_READING_Y  50
#define STATUS_Y        110
#define DETAIL_Y        140
#define MODE_BAR_Y      20

/* Component type names */
static const char *comp_type_names[] = {
    "Resistor",
    "Capacitor",
    "Diode",
    "Continuity",
};

/* Status strings */
static const char *status_names[] = {
    "PASS",
    "FAIL",
    "OPEN",
    "SHORT",
    "Measuring...",
};

/* Format resistance value with appropriate unit */
static void format_resistance(float ohms, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (ohms >= 1000000.0f) {
        val = ohms / 1000000.0f;
        unit = "M";
    } else if (ohms >= 1000.0f) {
        val = ohms / 1000.0f;
        unit = "k";
    } else {
        val = ohms;
        unit = "";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 100.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + integer / 100);
    if (integer >= 10  && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac / 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac % 10);

    while (*unit && pos < bufsize - 1)
        buf[pos++] = *unit++;
    buf[pos] = '\0';
}

/* Format capacitance with appropriate unit */
static void format_capacitance(float farads, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (farads >= 1e-3f) {
        val = farads * 1e3f;
        unit = "mF";
    } else if (farads >= 1e-6f) {
        val = farads * 1e6f;
        unit = "uF";
    } else if (farads >= 1e-9f) {
        val = farads * 1e9f;
        unit = "nF";
    } else {
        val = farads * 1e12f;
        unit = "pF";
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

/* Format voltage */
static void format_voltage(float volts, char *buf, int bufsize)
{
    int integer = (int)volts;
    int frac = (int)((volts - (float)integer) * 1000.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 10 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac / 100);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + (frac / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac % 10);
    buf[pos] = '\0';
}

void draw_component_test_screen(void)
{
    const theme_t *th = theme_get();
    const comp_test_config_t *cfg = comp_test_get_config();

    /* Clear content area */
    lcd_fill_rect(0, COMP_TOP, LCD_WIDTH, COMP_BOTTOM - COMP_TOP, th->background);

    /* Mode bar: component type label */
    const char *type_name = (cfg->type < COMP_COUNT) ? comp_type_names[cfg->type] : "???";
    font_draw_string_center(LCD_WIDTH / 2, MODE_BAR_Y, type_name,
                            th->highlight, th->background, &font_large);

    /* Generate demo measurement result */
    comp_test_result_t result;
    {
        /* Demo data: simulate a measurement based on type */
        static int16_t demo_v[32], demo_i[32];
        int i;
        for (i = 0; i < 32; i++) {
            /* Create plausible demo samples per type */
            switch (cfg->type) {
            case COMP_RESISTOR:
                demo_v[i] = (int16_t)(1000 + (i & 3));   /* ~0.1V */
                demo_i[i] = (int16_t)(100 + (i & 1));    /* ~0.01A => ~10 ohms */
                break;
            case COMP_CAPACITOR:
                demo_v[i] = (int16_t)(i * 500);          /* Charging ramp */
                demo_i[i] = (int16_t)(5000 - i * 150);
                break;
            case COMP_DIODE:
                demo_v[i] = (int16_t)(6400 + (i & 7));   /* ~0.64V Vf */
                demo_i[i] = (int16_t)(500);
                break;
            case COMP_CONTINUITY:
                demo_v[i] = (int16_t)(50 + (i & 3));     /* Low voltage => low R */
                demo_i[i] = (int16_t)(3000);
                break;
            default:
                demo_v[i] = 0;
                demo_i[i] = 0;
                break;
            }
        }
        comp_test_measure(demo_v, demo_i, 32, 10000.0f, &result);
    }

    /* Main reading: large digits */
    char reading[24];
    const char *unit_str = "";

    switch (result.type) {
    case COMP_RESISTOR:
    case COMP_CONTINUITY:
        format_resistance(result.measured_ohms, reading, sizeof(reading));
        unit_str = "Ohm";
        break;
    case COMP_CAPACITOR:
        format_capacitance(result.measured_farads, reading, sizeof(reading));
        unit_str = "";  /* Unit is embedded in the formatted string */
        break;
    case COMP_DIODE:
        format_voltage(result.measured_vf, reading, sizeof(reading));
        unit_str = "V";
        break;
    default:
        reading[0] = '-'; reading[1] = '-'; reading[2] = '-'; reading[3] = '\0';
        break;
    }

    /* Draw main reading centered with large font */
    font_draw_string_right(230, MAIN_READING_Y, reading,
                           th->text_primary, th->background, &font_xlarge);

    /* Unit */
    font_draw_string(238, MAIN_READING_Y + 8, unit_str,
                     th->ch1, th->background, &font_large);

    /* Status indicator */
    uint16_t status_color;
    switch (result.status) {
    case COMP_RESULT_PASS:    status_color = th->success; break;
    case COMP_RESULT_FAIL:    status_color = th->warning; break;
    case COMP_RESULT_OPEN:    status_color = th->text_secondary; break;
    case COMP_RESULT_SHORT:   status_color = th->warning; break;
    default:                  status_color = th->text_secondary; break;
    }

    const char *status_str = (result.status <= COMP_RESULT_MEASURING)
                             ? status_names[result.status] : "???";

    /* Status badge */
    uint16_t sw = font_string_width(status_str, &font_large);
    uint16_t sx = (LCD_WIDTH - sw) / 2;
    lcd_fill_rect(sx - 8, STATUS_Y - 2, sw + 16, 28, status_color);
    font_draw_string_center(LCD_WIDTH / 2, STATUS_Y, status_str,
                            th->background, status_color, &font_large);

    /* Detail section */
    if (result.type == COMP_RESISTOR && result.status == COMP_RESULT_PASS) {
        /* Show color bands */
        const char *bands = comp_test_resistor_bands(result.measured_ohms);
        font_draw_string(16, DETAIL_Y, "Bands:",
                         th->text_secondary, th->background, &font_medium);
        font_draw_string(80, DETAIL_Y, bands,
                         th->ch1, th->background, &font_medium);
    }

    if (result.type == COMP_CAPACITOR && result.measured_esr > 0.0f) {
        /* Show ESR */
        char esr_buf[16];
        format_resistance(result.measured_esr, esr_buf, sizeof(esr_buf));
        font_draw_string(16, DETAIL_Y, "ESR:",
                         th->text_secondary, th->background, &font_medium);
        font_draw_string(60, DETAIL_Y, esr_buf,
                         th->ch2, th->background, &font_medium);
        font_draw_string(120, DETAIL_Y, "Ohm",
                         th->text_secondary, th->background, &font_medium);
    }

    if (result.deviation_pct > 0.0f) {
        char dev_buf[16];
        int dev_int = (int)result.deviation_pct;
        int dev_frac = (int)((result.deviation_pct - (float)dev_int) * 10.0f);
        if (dev_frac < 0) dev_frac = -dev_frac;
        int dp = 0;
        if (dev_int >= 100 && dp < 14) dev_buf[dp++] = (char)('0' + dev_int / 100);
        if (dev_int >= 10  && dp < 14) dev_buf[dp++] = (char)('0' + (dev_int / 10) % 10);
        if (dp < 14) dev_buf[dp++] = (char)('0' + dev_int % 10);
        if (dp < 14) dev_buf[dp++] = '.';
        if (dp < 14) dev_buf[dp++] = (char)('0' + dev_frac);
        if (dp < 14) dev_buf[dp++] = '%';
        dev_buf[dp] = '\0';

        font_draw_string(16, DETAIL_Y + 22, "Dev:",
                         th->text_secondary, th->background, &font_medium);
        font_draw_string(56, DETAIL_Y + 22, dev_buf,
                         result.is_pass ? th->success : th->warning,
                         th->background, &font_medium);
    }

    /* Navigation hint */
    font_draw_string_center(LCD_WIDTH / 2, COMP_BOTTOM - 20,
                            "SELECT:Type  OK:Back",
                            th->text_secondary, th->background, &font_small);
}
