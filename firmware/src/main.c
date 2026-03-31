/*
 * OpenScope 2C53T - Custom Firmware
 *
 * Target: Artery AT32F403A (ARM Cortex-M4F @ 240MHz)
 * Display: ST7789V 320x240 via EXMC/XMC
 * RTOS: FreeRTOS
 *
 * This firmware initializes the LCD, draws a scope-like UI,
 * and responds to button inputs for mode selection.
 */

#include "at32f403a_407.h"

/* AT32 clock config (from at32f403a_gcc/user/) */
extern void system_clock_config(void);
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Include our drivers and UI */
#include "lcd.h"
#include "ui.h"
#include "signal_gen.h"
#include "watchdog.h"
#include "scope_state.h"
#include "input_handler.h"
#include "theme.h"
#include "math_channel.h"
#include "component_test.h"
#include "persistence.h"

/* ═══════════════════════════════════════════════════════════════════
 * Global State (extern'd via ui.h for UI modules)
 * ═══════════════════════════════════════════════════════════════════ */

volatile device_mode_t current_mode = MODE_OSCILLOSCOPE;
volatile uint32_t      uptime_seconds = 0;
volatile int8_t        settings_selected = 0;
volatile int8_t        settings_depth = 0;
volatile int8_t        settings_sub_selected = 0;
volatile uint8_t       active_channel = 0;  /* 0=CH1, 1=CH2 */
volatile uint8_t       meter_submode = 0;   /* 0-9: current meter sub-mode */
volatile uint8_t       meter_layout = 0;   /* 0=full, 1=chart, 2=stats */
volatile bool          meter_rel_enabled = false;  /* Relative/delta mode */
volatile float         meter_rel_reference = 0.0f;
volatile bool          meter_hold_enabled = false;  /* Auto-hold mode */
volatile bool          meter_hold_locked = false;   /* Hold has captured */
volatile float         meter_hold_value = 0.0f;

/* Scope feature toggles */
volatile bool          math_enabled = false;
volatile uint8_t       math_op = 0;        /* MATH_ADD */
volatile bool          persist_enabled = false;

#ifdef FEATURE_FFT
volatile scope_view_t scope_view = SCOPE_VIEW_TIME;
int16_t fft_sample_buf[FFT_SIZE];
fft_result_t fft_result;
#endif

/* FreeRTOS handles */
static TaskHandle_t  xDisplayTaskHandle = NULL;
static TaskHandle_t  xInputTaskHandle   = NULL;
static QueueHandle_t xDisplayQueue      = NULL;
static QueueHandle_t xInputQueue        = NULL;

/* Health monitor slots (assigned in main, used in task loops) */
static int health_slot_display = -1;
static int health_slot_input   = -1;

/* ═══════════════════════════════════════════════════════════════════
 * Simple delay (used before RTOS starts)
 * ═══════════════════════════════════════════════════════════════════ */

/* delay_ms is declared extern in lcd.h — provide the implementation here */
void delay_ms(uint32_t ms)
{
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;  /* Clock-speed independent */
        while (count--) {
            __asm volatile("nop");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * FreeRTOS Tasks
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Display Task (Priority 1 — lowest, matches original firmware)
 *
 * Receives display commands via queue and renders the appropriate screen.
 * This is the only task that writes to the LCD.
 */
static void vDisplayTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t cmd;
    uint32_t frame = 0;

    /* Show splash screen for 2 seconds */
    draw_splash();
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Initial draw */
    lcd_clear(COLOR_BLACK);
    draw_status_bar();
    draw_info_bar();
    draw_scope_screen(0);

    for (;;) {
        /* Check for commands (non-blocking with short timeout for animation) */
        if (xQueueReceive(xDisplayQueue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (cmd) {
            case DCMD_DRAW_SPLASH:
                draw_splash();
                break;
            case DCMD_REDRAW_ALL:
                lcd_clear(COLOR_BLACK);
                draw_status_bar();
                draw_info_bar();
                /* Fall through to draw current mode */
                /* FALLTHROUGH */
            case DCMD_DRAW_SCOPE:
                if (current_mode == MODE_OSCILLOSCOPE) {
#ifdef FEATURE_FFT
                    if (scope_view == SCOPE_VIEW_FFT)
                        draw_fft_screen();
                    else if (scope_view == SCOPE_VIEW_SPLIT)
                        draw_split_screen(frame);
                    else if (scope_view == SCOPE_VIEW_WATERFALL)
                        draw_waterfall_screen();
                    else
#endif
                        draw_scope_screen(frame);
                }
                break;
            case DCMD_DRAW_METER:
                if (current_mode == MODE_MULTIMETER) draw_meter_screen();
                break;
            case DCMD_DRAW_SIGGEN:
                if (current_mode == MODE_SIGNAL_GEN) draw_siggen_screen(frame);
                break;
            case DCMD_DRAW_SETTINGS:
                if (current_mode == MODE_SETTINGS) draw_settings_screen();
                break;
            case DCMD_DRAW_STATUS_BAR:
                draw_status_bar();
                break;
#ifdef FEATURE_FFT
            case DCMD_DRAW_FFT:
                if (current_mode == MODE_OSCILLOSCOPE)
                    draw_fft_screen();
                break;
#endif
            default:
                break;
            }
        }

        /* Check in with health monitor */
        health_checkin(health_slot_display);

        /* Animate the active mode (only scope and siggen have animation).
         * When scope is stopped, skip continuous redraws — waveform is
         * frozen and redraws only on explicit commands (V/div, timebase
         * changes send DCMD_REDRAW_ALL via the queue above). */
        if (current_mode == MODE_OSCILLOSCOPE) {
            const scope_state_t *ss_anim = scope_state_get();
            if (ss_anim->running) {
#ifdef FEATURE_FFT
                if (scope_view == SCOPE_VIEW_FFT)
                    draw_fft_screen();
                else if (scope_view == SCOPE_VIEW_SPLIT)
                    draw_split_screen(frame);
                else if (scope_view == SCOPE_VIEW_WATERFALL)
                    draw_waterfall_screen();
                else
#endif
                    draw_scope_screen(frame);
            }
        } else if (current_mode == MODE_SIGNAL_GEN) {
            draw_siggen_screen(frame);
        }

        frame++;
    }
}

/*
 * Input Task (Priority 4 — highest user task, matches original firmware)
 *
 * Polls GPIO for button presses, debounces, then delegates to
 * input_handle_button() in input_handler.c for all action logic.
 */
static void vInputTask(void *pvParameters)
{
    (void)pvParameters;
    button_id_t last_button = BTN_NONE;
    uint8_t debounce_count = 0;

    for (;;) {
        button_id_t pressed = BTN_NONE;

        uint16_t portc = (uint16_t)GPIOC->idt;
        uint16_t portb = (uint16_t)GPIOB->idt;

        /* Map GPIO states to button IDs */
        if (!(portc & (1 << 0))) pressed = BTN_MENU;
        else if (!(portc & (1 << 1))) pressed = BTN_AUTO;
        else if (!(portc & (1 << 2))) pressed = BTN_SAVE;
        else if (!(portc & (1 << 3))) pressed = BTN_CH1;
        else if (!(portc & (1 << 4))) pressed = BTN_CH2;
        else if (!(portb & (1 << 0))) pressed = BTN_UP;
        else if (!(portb & (1 << 1))) pressed = BTN_DOWN;
        else if (!(portb & (1 << 2))) pressed = BTN_LEFT;
        else if (!(portb & (1 << 3))) pressed = BTN_RIGHT;
        else if (!(portb & (1 << 4))) pressed = BTN_OK;
        else if (!(portc & (1 << 5))) pressed = BTN_PRM;
        else if (!(portc & (1 << 6))) pressed = BTN_SELECT;

        /* Simple debounce */
        if (pressed != BTN_NONE && pressed == last_button) {
            debounce_count++;
            if (debounce_count == 3) {
                input_handle_button(pressed, xDisplayQueue);
            }
        } else {
            debounce_count = 0;
        }
        last_button = pressed;

        /* Check in with health monitor */
        health_checkin(health_slot_input);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/*
 * Timer callback — runs every 1 second
 */
static void vOneSecondTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    uptime_seconds++;
    uint8_t cmd = DCMD_DRAW_STATUS_BAR;
    xQueueSend(xDisplayQueue, &cmd, 0);
}

/*
 * Health check timer — runs every 500ms
 *
 * Checks all registered tasks for liveness and stack health.
 * Only feeds the watchdog if everything is OK.
 */
static void vHealthCheckCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    health_check();
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void)
{
#ifdef EMULATOR_BUILD
    system_core_clock = 240000000;
    *(volatile uint32_t *)0x40021014 = 0x00000114;
    *(volatile uint32_t *)0x40021018 = 0x0000FFFD;
    *(volatile uint32_t *)0x4002101C = 0x3FFFFFFF;
#else
    /* FIRST: Power hold — PC9 HIGH to keep device on */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xF << 4)) | (0x3 << 4); /* PC9 push-pull 50MHz */
    GPIOC->scr = (1 << 9);  /* PC9 HIGH */

    /* NOTE: EOPB0 must be set to 0xFE (224KB SRAM) via DFU option bytes
     * before this firmware will work. See eopb0_setup.c or use:
     *   dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D option_bytes48.bin */

    /* Clock init to 240MHz */
    system_clock_config();

    /* Enable peripheral clocks */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* PB8 = LCD backlight ON */
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xF << 0)) | (0x3 << 0); /* PB8 push-pull 50MHz */
    GPIOB->scr = (1 << 8);
#endif

    /* Initialize theme system */
    theme_init(THEME_DARK_BLUE);

    /* Initialize oscilloscope state */
    scope_state_init(scope_state_get());

    /* Initialize LCD — using proven hwtest approach */
    {
        /* GPIO config for EXMC pins (same as hwtest) */
        #define _GPIO_CFG(base, pin, mode, cnf) do { \
            volatile uint32_t *r = (pin < 8) ? \
                (volatile uint32_t *)(base + 0x00) : \
                (volatile uint32_t *)(base + 0x04); \
            uint8_t p = (pin < 8) ? pin : (pin - 8); \
            uint32_t v = *r; \
            v &= ~(0xFU << (p * 4)); \
            v |= (((mode) | ((cnf) << 2)) << (p * 4)); \
            *r = v; \
        } while(0)

        /* PD0,1,4,5,7,8,9,10,11,12,14,15 as AF push-pull */
        uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
        for (int i = 0; i < 12; i++)
            _GPIO_CFG(0x40011400, pd_pins[i], 3, 2);
        /* PE7-15 as AF push-pull */
        for (int i = 7; i <= 15; i++)
            _GPIO_CFG(0x40011800, i, 3, 2);

        /* EXMC config — proven working values */
        *(volatile uint32_t *)0xA0000000 = 0x00005010;
        *(volatile uint32_t *)0xA0000004 = 0x02020424;
        *(volatile uint32_t *)0xA0000104 = 0x00000202;
        *(volatile uint32_t *)0xA0000000 |= 0x0001;
        delay_ms(50);

        /* LCD init — proven working sequence from hwtest */
        lcd_write_cmd(0x01);  /* Software reset */
        delay_ms(200);
        lcd_write_cmd(0x11);  /* Sleep out */
        delay_ms(200);
        lcd_write_cmd(0x36);  /* MADCTL — landscape, flipped */
        delay_ms(1);
        lcd_write_data8(0xA0);
        delay_ms(10);
        lcd_write_cmd(0x3A);  /* Pixel format 16-bit */
        delay_ms(1);
        lcd_write_data8(0x55);
        delay_ms(10);
        lcd_write_cmd(0x29);  /* Display on */
        delay_ms(50);

        #undef _GPIO_CFG
    }

    /* LCD init complete — UI will be drawn by FreeRTOS display task */

    /*
     * FFT engine is initialized ON DEMAND when user enters FFT view
     * (via BTN_PRM in input_handler.c). This keeps the 88KB shared
     * memory pool free for other features until actually needed.
     */

#ifndef EMULATOR_BUILD
    /* Initialize signal generator (skipped in emulator) */
    {
        siggen_config_t sg_cfg;
        sg_cfg.waveform       = SIGGEN_SINE;
        sg_cfg.frequency_hz   = 1000.0f;
        sg_cfg.amplitude_vpp  = 3.3f;
        sg_cfg.offset_v       = 0.0f;
        sg_cfg.duty_cycle_pct = 50;
        sg_cfg.output_enabled = false;
        siggen_init(&sg_cfg);
    }
#endif

    /* Configure button GPIOs as inputs with pull-up
     * Mode=0 (input), CNF=2 (input with pull-up/down), then set ODR for pull-up */
    GPIOC->cfglr = (GPIOC->cfglr & 0xF0000000) | 0x08888888; /* PC0-PC6 input pull-up */
    GPIOC->scr = 0x007F;  /* Pull-up on PC0-PC6 */
    GPIOB->cfglr = (GPIOB->cfglr & 0xFFF00000) | 0x00088888; /* PB0-PB4 input pull-up */
    GPIOB->scr = 0x001F;  /* Pull-up on PB0-PB4 */

    /* Create queues */
    xDisplayQueue = xQueueCreate(20, sizeof(uint8_t));
    xInputQueue   = xQueueCreate(15, sizeof(uint8_t));

    /* Create tasks */
    xTaskCreate(vDisplayTask, "display", 512, NULL, 1, &xDisplayTaskHandle);
    xTaskCreate(vInputTask,   "key",     256, NULL, 4, &xInputTaskHandle);

    /* Register tasks with health monitor */
    health_slot_display = health_register("display", xDisplayTaskHandle);
    health_slot_input   = health_register("key",     xInputTaskHandle);

    /* Create 1-second timer for uptime/status updates */
    TimerHandle_t xSecTimer = xTimerCreate(
        "1sec", pdMS_TO_TICKS(1000), pdTRUE, NULL, vOneSecondTimerCallback);
    if (xSecTimer != NULL) {
        xTimerStart(xSecTimer, 0);
    }

    /* Create 500ms health check timer — monitors tasks + feeds watchdog */
    TimerHandle_t xHealthTimer = xTimerCreate(
        "health", pdMS_TO_TICKS(500), pdTRUE, NULL, vHealthCheckCallback);
    if (xHealthTimer != NULL) {
        xTimerStart(xHealthTimer, 0);
    }

    /* Initialize watchdog LAST — after all tasks and timers are running.
     * Once enabled, the FWDGT cannot be stopped (hardware limitation). */
    watchdog_init();

    vTaskStartScheduler();

    for (;;) {}
}

/* ═══════════════════════════════════════════════════════════════════
 * FreeRTOS Hooks
 * ═══════════════════════════════════════════════════════════════════ */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    fault_display("STACK OVERFLOW", pcTaskName);
    /* Watchdog will reset us */
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    fault_display("HEAP EXHAUSTED", "pvPortMalloc returned NULL");
    /* Watchdog will reset us */
    for (;;) {}
}

/* ═══════════════════════════════════════════════════════════════════
 * SystemInit override for emulator
 * ═══════════════════════════════════════════════════════════════════ */
#ifdef EMULATOR_BUILD
uint32_t system_core_clock = 240000000;

void SystemInit(void)
{
    /* Stub — do nothing in emulator mode. */
}

void system_clock_config(void)
{
    /* Stub — do nothing in emulator mode. */
}
#endif
