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

/* Include our drivers */
#include "lcd.h"

/* ═══════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════ */

/* Device modes (matching original firmware structure) */
typedef enum {
    MODE_OSCILLOSCOPE = 0,
    MODE_MULTIMETER,
    MODE_SIGNAL_GEN,
    MODE_SETTINGS,
    MODE_COUNT
} device_mode_t;

/* Display commands sent via queue */
typedef enum {
    DCMD_DRAW_SPLASH = 0,
    DCMD_DRAW_SCOPE,
    DCMD_DRAW_METER,
    DCMD_DRAW_SIGGEN,
    DCMD_DRAW_SETTINGS,
    DCMD_DRAW_STATUS_BAR,
    DCMD_REDRAW_ALL,
} display_cmd_t;

/* Button IDs */
typedef enum {
    BTN_NONE = 0,
    BTN_CH1,
    BTN_CH2,
    BTN_MOVE,
    BTN_SELECT,
    BTN_TRIGGER,
    BTN_PRM,
    BTN_AUTO,
    BTN_SAVE,
    BTN_MENU,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_OK,
    BTN_POWER,
} button_id_t;

/* ═══════════════════════════════════════════════════════════════════
 * Global State
 * ═══════════════════════════════════════════════════════════════════ */

static volatile device_mode_t current_mode = MODE_OSCILLOSCOPE;
static volatile uint32_t uptime_seconds = 0;

/* FreeRTOS handles */
static TaskHandle_t  xDisplayTaskHandle = NULL;
static TaskHandle_t  xInputTaskHandle   = NULL;
static QueueHandle_t xDisplayQueue      = NULL;
static QueueHandle_t xInputQueue        = NULL;

/* ═══════════════════════════════════════════════════════════════════
 * Simple delay (used before RTOS starts)
 * ═══════════════════════════════════════════════════════════════════ */

/* delay_ms is declared extern in lcd.h — provide the implementation here */
void delay_ms(uint32_t ms)
{
    /* Rough delay assuming 120MHz, good enough for LCD init */
    volatile uint32_t count;
    while (ms--) {
        count = 12000;
        while (count--) {
            __asm volatile("nop");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * UI Drawing Functions
 * ═══════════════════════════════════════════════════════════════════ */

/* Draw the top status bar */
static void draw_status_bar(void)
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
static void draw_info_bar(void)
{
    lcd_fill_rect(0, LCD_HEIGHT - 16, LCD_WIDTH, 16, COLOR_DARK_GRAY);

    switch (current_mode) {
    case MODE_OSCILLOSCOPE:
        lcd_draw_string(4, LCD_HEIGHT - 15, "CH1:2V DC", COLOR_YELLOW, COLOR_DARK_GRAY);
        lcd_draw_string(100, LCD_HEIGHT - 15, "Auto", COLOR_GREEN, COLOR_DARK_GRAY);
        lcd_draw_string(160, LCD_HEIGHT - 15, "H=50uS", COLOR_WHITE, COLOR_DARK_GRAY);
        lcd_draw_string(240, LCD_HEIGHT - 15, "CH2:200mV", COLOR_CYAN, COLOR_DARK_GRAY);
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

/* Draw the oscilloscope grid */
static void draw_scope_grid(void)
{
    /* Grid lines */
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
static void draw_demo_waveform(uint32_t frame)
{
    /* Simple sine wave approximation using integer math */
    /* sin lookup table (256 entries, scaled to -100..+100) */
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
            /* Make line thicker */
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
        /* Draw vertical transitions */
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
static void draw_scope_screen(uint32_t frame)
{
    /* Clear scope area */
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    draw_scope_grid();
    draw_demo_waveform(frame);

    /* Measurements overlay */
    lcd_draw_string(4, 18, "Freq:10.00kHz", COLOR_CH1, COLOR_BLACK);
    lcd_draw_string(4, 34, "Vpp:3.31V", COLOR_CH1, COLOR_BLACK);
    lcd_draw_string(LCD_WIDTH - 104, 18, "Freq:9.96kHz", COLOR_CH2, COLOR_BLACK);
    lcd_draw_string(LCD_WIDTH - 88, 34, "Vpp:660mV", COLOR_CH2, COLOR_BLACK);
}

/* Draw the multimeter screen */
static void draw_meter_screen(void)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Big reading */
    lcd_draw_string(60, 60, "13.82", COLOR_WHITE, COLOR_BLACK);
    /* Scale it up by drawing twice shifted */
    lcd_draw_string(61, 60, "13.82", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(60, 61, "13.82", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(61, 61, "13.82", COLOR_WHITE, COLOR_BLACK);

    lcd_draw_string(170, 65, "V DC", COLOR_YELLOW, COLOR_BLACK);

    /* Bar graph */
    lcd_fill_rect(20, 120, 280, 12, COLOR_DARK_GRAY);
    lcd_fill_rect(20, 120, 180, 12, COLOR_GREEN);

    /* Secondary readings */
    lcd_draw_string(20, 150, "MIN: 13.79V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(20, 168, "MAX: 13.86V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(20, 186, "AVG: 13.82V", COLOR_GRAY, COLOR_BLACK);
    lcd_draw_string(180, 150, "Range: 20V", COLOR_YELLOW, COLOR_BLACK);
}

/* Draw the signal generator screen */
static void draw_siggen_screen(uint32_t frame)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    /* Waveform preview */
    uint16_t y_center = 80;
    for (uint16_t x = 20; x < LCD_WIDTH - 20; x++) {
        static const int8_t sin_lut[64] = {
             0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
            100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
             0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
           -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
        };
        uint8_t idx = (uint8_t)((x * 3 + frame) & 0x3F);
        int16_t y = y_center - (sin_lut[idx] * 30 / 100);
        lcd_set_pixel(x, (uint16_t)y, COLOR_CH1);
    }

    /* Settings */
    lcd_draw_string(20, 130, "Waveform: Sine", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 148, "Frequency: 1.000 kHz", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 166, "Amplitude: 3.3 Vpp", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 184, "Offset:    0.0 V", COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(20, 202, "Output:    ON", COLOR_GREEN, COLOR_BLACK);
}

/* Draw the settings screen */
static void draw_settings_screen(void)
{
    lcd_fill_rect(0, 16, LCD_WIDTH, LCD_HEIGHT - 32, COLOR_BLACK);

    lcd_draw_string(110, 24, "SETTINGS", COLOR_WHITE, COLOR_BLACK);

    const char *items[] = {
        "Language: English",
        "Sound and Light",
        "Auto Shutdown",
        "Display Mode",
        "Startup on Boot",
        "About",
        "Factory Reset",
    };

    for (int i = 0; i < 7; i++) {
        uint16_t y = 50 + i * 24;
        uint16_t bg = (i == 0) ? COLOR_SELECTED_BG : COLOR_BLACK;
        uint16_t fg = (i == 0) ? COLOR_WHITE : COLOR_GRAY;
        lcd_fill_rect(20, y, 280, 20, bg);
        lcd_draw_string(28, y + 3, items[i], fg, bg);
    }
}

/* Draw the splash screen */
static void draw_splash(void)
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
                if (current_mode == MODE_OSCILLOSCOPE) draw_scope_screen(frame);
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
            default:
                break;
            }
        }

        /* Animate the active mode (only scope and siggen have animation) */
        if (current_mode == MODE_OSCILLOSCOPE) {
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
 * Scans GPIO pins for button presses, debounces, and sends
 * commands to the display queue. In the original firmware this
 * is the "key" task.
 *
 * Button GPIO mapping (TBD — will be confirmed from hardware):
 *   These are placeholder pin assignments. Real pin mapping will be
 *   determined from the PCB teardown or by probing with the SWD debugger.
 */
static void vInputTask(void *pvParameters)
{
    (void)pvParameters;
    button_id_t last_button = BTN_NONE;
    uint8_t debounce_count = 0;

    for (;;) {
        button_id_t pressed = BTN_NONE;

        /* Read GPIO pins for button states */
        /* TODO: Replace with actual GPIO reads once pin mapping is known */
        /* For now, check GPIOC pin 8 as a test (the FPGA ready pin we found) */
        /* In the emulator, we can simulate button presses via the React frontend */

        /* Placeholder: scan a few GPIO pins */
        uint16_t portc = (uint16_t)gpio_input_port_get(GPIOC);
        uint16_t portb = (uint16_t)gpio_input_port_get(GPIOB);

        /* Map GPIO states to button IDs */
        /* These mappings are guesses — will be corrected after PCB analysis */
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

        /* Simple debounce */
        if (pressed != BTN_NONE && pressed == last_button) {
            debounce_count++;
            if (debounce_count == 3) { /* Confirmed press */
                uint8_t cmd;

                switch (pressed) {
                case BTN_MENU:
                    /* Cycle through modes */
                    current_mode = (device_mode_t)((current_mode + 1) % MODE_COUNT);
                    cmd = DCMD_REDRAW_ALL;
                    xQueueSend(xDisplayQueue, &cmd, 0);
                    break;

                case BTN_AUTO:
                    if (current_mode == MODE_OSCILLOSCOPE) {
                        /* Auto-set in scope mode */
                        cmd = DCMD_DRAW_SCOPE;
                        xQueueSend(xDisplayQueue, &cmd, 0);
                    }
                    break;

                case BTN_UP:
                case BTN_DOWN:
                case BTN_LEFT:
                case BTN_RIGHT:
                case BTN_OK:
                    /* Navigation — trigger redraw of current mode */
                    cmd = DCMD_DRAW_SCOPE + (uint8_t)current_mode;
                    xQueueSend(xDisplayQueue, &cmd, 0);
                    break;

                default:
                    break;
                }
            }
        } else {
            debounce_count = 0;
        }
        last_button = pressed;

        vTaskDelay(pdMS_TO_TICKS(20)); /* 50Hz scan rate */
    }
}

/*
 * Timer callback — runs every 1 second
 * Updates the uptime counter and redraws the status bar
 */
static void vOneSecondTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    uptime_seconds++;
    uint8_t cmd = DCMD_DRAW_STATUS_BAR;
    xQueueSend(xDisplayQueue, &cmd, 0);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void)
{
    /* SystemInit() is called by startup code but may hang in emulator
     * waiting for PLL/PMU ready bits. Set SystemCoreClock directly. */
#ifdef EMULATOR_BUILD
    /* In emulator mode, skip HAL clock init — just enable peripheral clocks directly */
    SystemCoreClock = 120000000;
    /* Enable all peripheral clocks by writing directly to RCU registers */
    *(volatile uint32_t *)0x40021014 = 0x00000114; /* AHBEN: SRAM, DMA0, EXMC */
    *(volatile uint32_t *)0x40021018 = 0x0000FFFD; /* APB2EN: all GPIO + AFIO + SPI0 */
    *(volatile uint32_t *)0x4002101C = 0x3FFFFFFF; /* APB1EN: all timers, UART, I2C, SPI */
#else
    /* On real hardware, use the HAL for proper clock setup */
    SystemCoreClockUpdate();
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_EXMC);
#endif

    /* Initialize LCD */
    lcd_gpio_init();
    lcd_fsmc_init();
    lcd_init();

    /* Configure button GPIOs as inputs with pull-up */
    /* TODO: These pin assignments are placeholders */
    gpio_init(GPIOC, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4);
    gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4);

    /* Create queues */
    xDisplayQueue = xQueueCreate(20, sizeof(uint8_t));
    xInputQueue   = xQueueCreate(15, sizeof(uint8_t));

    /* Create tasks (matching original firmware priorities and stack sizes) */
    xTaskCreate(vDisplayTask, "display", 512, NULL, 1, &xDisplayTaskHandle);
    xTaskCreate(vInputTask,   "key",     256, NULL, 4, &xInputTaskHandle);

    /* Create 1-second timer for uptime/status updates */
    TimerHandle_t xSecTimer = xTimerCreate(
        "1sec", pdMS_TO_TICKS(1000), pdTRUE, NULL, vOneSecondTimerCallback);
    if (xSecTimer != NULL) {
        xTimerStart(xSecTimer, 0);
    }

    /* Start the scheduler — never returns */
    vTaskStartScheduler();

    for (;;) {}
}

/* ═══════════════════════════════════════════════════════════════════
 * FreeRTOS Hooks
 * ═══════════════════════════════════════════════════════════════════ */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    for (;;) {}
}

/* ═══════════════════════════════════════════════════════════════════
 * SystemInit override for emulator
 *
 * The startup assembly (startup_gd32f30x_cl.S) calls SystemInit()
 * before main(). In emulator mode, we provide a stub that does nothing
 * since we can't wait for PLL/PMU ready bits without real hardware.
 * ═══════════════════════════════════════════════════════════════════ */
#ifdef EMULATOR_BUILD
/* SystemCoreClock is normally defined in system_gd32f30x.c which we exclude */
uint32_t SystemCoreClock = 120000000;

void SystemInit(void)
{
    /* Stub — do nothing in emulator mode. Real hardware uses HAL version. */
}
#endif
