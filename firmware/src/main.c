/*
 * OpenScope 2C53T - Custom Firmware
 *
 * Target: GD32F307 (ARM Cortex-M4F @ 120MHz)
 * Display: ST7789V 320x240 via EXMC
 * RTOS: FreeRTOS
 *
 * This firmware initializes the LCD, draws a scope-like UI,
 * and responds to button inputs for mode selection.
 */

#include "gd32f30x.h"
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
        count = 12000;
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

        /* Animate the active mode (only scope and siggen have animation) */
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

        uint16_t portc = (uint16_t)gpio_input_port_get(GPIOC);
        uint16_t portb = (uint16_t)gpio_input_port_get(GPIOB);

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
    SystemCoreClock = 120000000;
    *(volatile uint32_t *)0x40021014 = 0x00000114;
    *(volatile uint32_t *)0x40021018 = 0x0000FFFD;
    *(volatile uint32_t *)0x4002101C = 0x3FFFFFFF;
#else
    SystemCoreClockUpdate();
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_EXMC);
#endif

    /* Initialize theme system */
    theme_init(THEME_DARK_BLUE);

    /* Initialize oscilloscope state */
    scope_state_init(scope_state_get());

    /* Initialize LCD */
    lcd_gpio_init();
    lcd_fsmc_init();
    lcd_init();

#if defined(FEATURE_FFT) && !defined(EMULATOR_BUILD)
    /* Initialize FFT engine (skipped in emulator — trig functions too slow) */
    {
        fft_config_t fft_cfg;
        fft_cfg.window         = FFT_WINDOW_HANNING;
        fft_cfg.sample_rate_hz = 44100.0f;
        fft_cfg.ref_level_db   = 0.0f;
        fft_cfg.db_range       = 80.0f;
        fft_cfg.peak_count     = 4;
        fft_cfg.avg_count      = 0;
        fft_cfg.max_hold       = false;
        fft_cfg.zoom_start_bin = 1;
        fft_cfg.zoom_end_bin   = FFT_BINS - 1;
        fft_init(&fft_cfg);
    }
#endif

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

    /* Configure button GPIOs as inputs with pull-up */
    gpio_init(GPIOC, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
              GPIO_PIN_5 | GPIO_PIN_6);
    gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4);

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
uint32_t SystemCoreClock = 120000000;

void SystemInit(void)
{
    /* Stub — do nothing in emulator mode. */
}
#endif
