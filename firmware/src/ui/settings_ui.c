/*
 * OpenScope 2C53T - Settings UI
 *
 * Multi-level navigable menu system:
 *   depth 0: Top-level settings (7 items)
 *   depth 1: Oscilloscope settings sub-menu (CH1/CH2/trigger config)
 *   depth 2: About screen (device info)
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "watchdog.h"
#include "scope_state.h"
#include "math_channel.h"
#include "component_test.h"
#include "shared_mem.h"
#include <stdio.h>

/* Top-level menu items */
static const char *settings_items[SETTINGS_ITEM_COUNT] = {
    "Oscilloscope Settings",
    "Sound and Light",
    "Auto Shutdown",
    "Display Mode",
    "Math / Persist",
    "Component Tester",
    "Bode Plot",
    "Startup on Boot",
    "About",
    "FPGA SPI Scanner",
    "Firmware Update",
    "Factory Reset",
};

/* Math / Persistence sub-menu items */
#define MATH_SUB_ITEMS  3
static const char *math_sub_labels[MATH_SUB_ITEMS] = {
    "Math Channel",
    "Persistence",
    "Back",
};

/* Layout constants */
#define MENU_TOP        42
#define MENU_ITEM_H     24
#define MENU_LEFT       16
#define MENU_WIDTH      288

/* ═══════════════════════════════════════════════════════════════════
 * Helper: draw a generic menu list
 * ═══════════════════════════════════════════════════════════════════ */

/* Maximum visible items given screen geometry */
#define MENU_VISIBLE    ((LCD_HEIGHT - 20 - MENU_TOP) / MENU_ITEM_H)

static void draw_menu_item(int i, int scroll_offset, int selected_idx,
                           const char *label, const char *value,
                           const theme_t *th)
{
    int row = i - scroll_offset;
    if (row < 0 || row >= MENU_VISIBLE) return;

    uint16_t y = MENU_TOP + row * MENU_ITEM_H;
    bool selected = (i == selected_idx);

    uint16_t bg = selected ? th->menu_selected_bg : th->background;

    if (selected)
        lcd_fill_rect(MENU_LEFT - 4, y, MENU_WIDTH + 8, MENU_ITEM_H - 2, bg);

    if (selected)
        font_draw_string(MENU_LEFT, y + 4, ">",
                         th->highlight, bg, &font_medium);

    uint16_t fg = selected ? th->text_primary : th->text_secondary;
    font_draw_string(MENU_LEFT + 16, y + 4, label, fg, bg, &font_medium);

    if (value)
        font_draw_string_right(MENU_LEFT + MENU_WIDTH - 8, y + 4,
                               value, th->highlight, bg, &font_medium);
}

/* ═══════════════════════════════════════════════════════════════════
 * Top-level settings screen (depth 0)
 * ═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════
 * Math / Persistence sub-menu (depth 3)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_settings_math(void)
{
    const theme_t *th = theme_get();

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, th->background);

    font_draw_string_center(LCD_WIDTH / 2, 20, "Math / Persist",
                            th->text_primary, th->background, &font_large);

    for (int i = 0; i < MATH_SUB_ITEMS; i++) {
        const char *val = NULL;

        if (i == 0) {
            if (math_enabled) {
                val = math_channel_name((math_op_t)math_op);
            } else {
                val = "Off";
            }
        }
        if (i == 1) {
            val = persist_enabled ? "On" : "Off";
        }

        draw_menu_item(i, 0, settings_sub_selected, math_sub_labels[i], val, th);
    }
}

static void draw_settings_top(void)
{
    const theme_t *th = theme_get();

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, th->background);

    font_draw_string_center(LCD_WIDTH / 2, 20, "Settings",
                            th->text_primary, th->background, &font_large);

    /* Compute scroll offset so selected item is always visible */
    int scroll = 0;
    if (settings_selected >= MENU_VISIBLE)
        scroll = settings_selected - MENU_VISIBLE + 1;

    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        const char *value = NULL;

        /* Dynamic values for certain items */
        if (i == 0) value = ">";        /* Has sub-menu */
        if (i == 3) value = th->name;   /* Display Mode: theme name */
        if (i == 4) value = ">";        /* Math/Persist: sub-menu */
        if (i == 5) value = ">";        /* Component Tester */
        if (i == 6) value = ">";        /* Bode Plot */
        if (i == 8) value = ">";        /* About: has detail screen */
        if (i == 9) value = "DFU";      /* Firmware Update: reboot to bootloader */

        draw_menu_item(i, scroll, settings_selected, settings_items[i], value, th);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Oscilloscope settings sub-menu (depth 1)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_settings_osc(void)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, th->background);

    font_draw_string_center(LCD_WIDTH / 2, 20, "Oscilloscope",
                            th->text_primary, th->background, &font_large);

    /* CH1 section */
    draw_menu_item(0, 0, settings_sub_selected,
                   "CH1 Coupling", coupling_labels[ss->ch1.coupling], th);
    draw_menu_item(1, 0, settings_sub_selected,
                   "CH1 Probe", probe_labels[ss->ch1.probe], th);
    draw_menu_item(2, 0, settings_sub_selected,
                   "CH1 20M Limit", ss->ch1.bw_limit ? "ON" : "OFF", th);

    /* CH2 section */
    draw_menu_item(3, 0, settings_sub_selected,
                   "CH2 Coupling", coupling_labels[ss->ch2.coupling], th);
    draw_menu_item(4, 0, settings_sub_selected,
                   "CH2 Probe", probe_labels[ss->ch2.probe], th);
    draw_menu_item(5, 0, settings_sub_selected,
                   "CH2 20M Limit", ss->ch2.bw_limit ? "ON" : "OFF", th);

    /* Trigger section */
    draw_menu_item(6, 0, settings_sub_selected,
                   "Trigger Mode", trigger_mode_labels[ss->trigger.mode], th);
    draw_menu_item(7, 0, settings_sub_selected,
                   "Trigger Edge", trigger_edge_labels[ss->trigger.edge], th);
}

/* ═══════════════════════════════════════════════════════════════════
 * About screen (depth 2)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_settings_about(void)
{
    const theme_t *th = theme_get();

    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, th->background);

    font_draw_string_center(LCD_WIDTH / 2, 20, "About",
                            th->text_primary, th->background, &font_large);

    uint16_t y = 55;
    uint16_t fg = th->text_secondary;
    uint16_t bg = th->background;

    font_draw_string(MENU_LEFT, y, "OpenScope 2C53T", th->text_primary, bg, &font_medium);
    y += 22;
    font_draw_string(MENU_LEFT, y, "Custom Firmware v0.2", fg, bg, &font_medium);
    y += 22;
    font_draw_string(MENU_LEFT, y, "MCU: AT32F403A @240MHz", fg, bg, &font_medium);
    y += 22;
    font_draw_string(MENU_LEFT, y, "LCD: ST7789V 320x240", fg, bg, &font_medium);
    y += 22;
    font_draw_string(MENU_LEFT, y, "RTOS: FreeRTOS", fg, bg, &font_medium);
    y += 26;

    /* Pool status */
    {
        char pbuf[40];
        snprintf(pbuf, sizeof(pbuf), "Pool: %s (%luKB)",
                 shared_mem_owner_name(),
                 (unsigned long)(shared_mem_owner_need() / 1024));
        font_draw_string(MENU_LEFT, y, pbuf, th->highlight, bg, &font_small);
        y += 14;
        snprintf(pbuf, sizeof(pbuf), "Heap free: %luB  Transitions: %lu",
                 (unsigned long)xPortGetFreeHeapSize(),
                 (unsigned long)shared_mem_transition_count());
        font_draw_string(MENU_LEFT, y, pbuf, fg, bg, &font_small);
    }
    y += 20;

    font_draw_string(MENU_LEFT, y, "License: GPL v3",
                     th->text_secondary, bg, &font_small);

    y += 24;
    font_draw_string_center(LCD_WIDTH / 2, y, "MENU to go back",
                            th->text_secondary, bg, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Public entry point — dispatches by depth
 * ═══════════════════════════════════════════════════════════════════ */

void draw_settings_screen(void)
{
    switch (settings_depth) {
    case 1:  draw_settings_osc();              break;
    case 2:  draw_settings_about();            break;
    case 3:  draw_settings_math();             break;
    case 4:  draw_component_test_screen();     break;
    case 5:  draw_resistor_calc_screen();      break;
    case 6:  draw_bode_screen();              break;
    default: draw_settings_top();              break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Task health diagnostics panel
 * ═══════════════════════════════════════════════════════════════════ */

void draw_health_panel(uint16_t y_start)
{
    const task_health_t *tasks = health_get_tasks();
    int count = health_get_count();
    char buf[40];

    lcd_fill_rect(0, y_start - 4, LCD_WIDTH, LCD_HEIGHT - y_start + 4,
                  COLOR_BLACK);
    font_draw_string(10, y_start - 2, "TASK HEALTH",
                     COLOR_GRAY, COLOR_BLACK, &font_small);

    for (int i = 0; i < count && i < 4; i++) {
        uint16_t y = y_start + 14 + i * 16;
        bool stalled = health_is_stalled(i);
        uint16_t fg = stalled ? COLOR_RED : COLOR_GREEN;

        snprintf(buf, sizeof(buf), "%-8s", tasks[i].name);
        font_draw_string(10, y, buf, fg, COLOR_BLACK, &font_small);

        snprintf(buf, sizeof(buf), "stk:%3lu",
                 (unsigned long)tasks[i].stack_hwm);
        font_draw_string(80, y, buf, COLOR_GRAY, COLOR_BLACK, &font_small);

        font_draw_string(170, y, stalled ? "STALL" : "OK",
                         fg, COLOR_BLACK, &font_small);
    }
}
