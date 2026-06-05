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
#include "button_scan.h"
#include "dfu_boot.h"
#include "battery.h"
#include "fpga.h"
#include "meter_data.h"
#include "meter_voltage_wave.h"
#include "flash_fs.h"
#include "usb_debug.h"
#include "rtt.h"
#include "fault.h"

/* ═══════════════════════════════════════════════════════════════════
 * Global State (extern'd via ui.h for UI modules)
 * ═══════════════════════════════════════════════════════════════════ */

#if FPGA_WARM_HANDOFF_TEST
/* Warm-handoff build: boot straight into scope mode — the LCD (scope screen +
 * debug overlay) is the only readout on the bench unit (USB CDC dead). */
volatile device_mode_t current_mode = MODE_OSCILLOSCOPE;
#else
volatile device_mode_t current_mode = MODE_MULTIMETER;
#endif
volatile startup_mode_t startup_mode = STARTUP_METER;
volatile uint32_t      uptime_seconds = 0;
volatile int8_t        settings_selected = 0;
volatile int8_t        settings_depth = 0;
volatile int8_t        settings_sub_selected = 0;
volatile uint8_t       active_channel = 0;  /* 0=CH1, 1=CH2 */
volatile uint8_t       meter_submode = 0;   /* 0-9: current meter sub-mode */
volatile uint8_t       meter_layout = 0;   /* 0=full, 1=chart, 2=stats, 3=fuse */
volatile bool          meter_rel_enabled = false;  /* Relative/delta mode */
volatile float         meter_rel_reference = 0.0f;
volatile bool          meter_hold_enabled = false;  /* Auto-hold mode */
volatile bool          meter_hold_locked = false;   /* Hold has captured */
volatile float         meter_hold_value = 0.0f;

/* Fuse tester state */
volatile uint8_t       fuse_type = 0;               /* FUSE_TYPE_ATO_ATC */
volatile uint8_t       fuse_rating_idx = 4;          /* Default to 10A (index 4 in ATO table) */
volatile uint8_t       fuse_view = 0;                /* FUSE_VIEW_DETAIL */
volatile float         fuse_scan_threshold_mv = 0.5f; /* Pass/fail threshold */

/* Scope feature toggles */
volatile bool          math_enabled = false;
volatile uint8_t       math_op = 0;        /* MATH_ADD */
volatile bool          persist_enabled = false;

#define STARTUP_SETTINGS_ADDR     0x080FF800u
#define STARTUP_SETTINGS_MAGIC    0x3243534Fu  /* "OSC2", little-endian */
#define STARTUP_SETTINGS_VERSION  1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t mode;
    uint32_t checksum;
} startup_settings_record_t;

static uint32_t startup_settings_checksum(uint32_t mode)
{
    return STARTUP_SETTINGS_MAGIC ^ STARTUP_SETTINGS_VERSION ^ mode ^ 0x5A5AA5A5u;
}

static void startup_mode_load(void)
{
#ifndef EMULATOR_BUILD
    const startup_settings_record_t *rec =
        (const startup_settings_record_t *)STARTUP_SETTINGS_ADDR;

    if (rec->magic == STARTUP_SETTINGS_MAGIC &&
        rec->version == STARTUP_SETTINGS_VERSION &&
        rec->mode < STARTUP_COUNT &&
        rec->checksum == startup_settings_checksum(rec->mode)) {
        startup_mode = (startup_mode_t)rec->mode;
    }
#endif
}

static void startup_mode_save(void)
{
#ifndef EMULATOR_BUILD
    startup_settings_record_t rec;
    rec.magic = STARTUP_SETTINGS_MAGIC;
    rec.version = STARTUP_SETTINGS_VERSION;
    rec.mode = (uint32_t)startup_mode;
    rec.checksum = startup_settings_checksum(rec.mode);

    flash_unlock();
    if (flash_sector_erase(STARTUP_SETTINGS_ADDR) == FLASH_OPERATE_DONE) {
        const uint32_t *words = (const uint32_t *)&rec;
        uint32_t addr = STARTUP_SETTINGS_ADDR;
        for (uint32_t i = 0; i < sizeof(rec) / sizeof(uint32_t); i++, addr += 4) {
            if (flash_word_program(addr, words[i]) != FLASH_OPERATE_DONE) {
                break;
            }
        }
    }
    flash_lock();
#endif
}

const char *startup_mode_name(startup_mode_t mode)
{
    switch (mode) {
    case STARTUP_SCOPE: return "Scope";
    case STARTUP_METER: return "Meter";
    default:            return "?";
    }
}

void startup_mode_set(startup_mode_t mode)
{
    if (mode >= STARTUP_COUNT || startup_mode == mode) return;
    startup_mode = mode;
    startup_mode_save();
}

void startup_mode_adjust(int dir)
{
    int next = (int)startup_mode + dir;
    while (next < 0) next += STARTUP_COUNT;
    startup_mode_set((startup_mode_t)(next % STARTUP_COUNT));
}

static device_mode_t startup_target_mode(void)
{
    switch (startup_mode) {
    case STARTUP_METER:
        return MODE_MULTIMETER;
    case STARTUP_SCOPE:
    default:
        return MODE_OSCILLOSCOPE;
    }
}

#ifdef FEATURE_FFT
volatile scope_view_t scope_view = SCOPE_VIEW_TIME;
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
    if (current_mode == MODE_MULTIMETER) {
        draw_meter_screen();
    } else {
        draw_scope_screen(0);
    }

    for (;;) {
        /* Check for commands (non-blocking with short timeout for animation) */
        if (xQueueReceive(xDisplayQueue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (cmd) {
            case DCMD_DRAW_SPLASH:
                draw_splash();
                break;
            case DCMD_REDRAW_ALL:
                lcd_clear(COLOR_BLACK);
                status_bar_invalidate();
                draw_status_bar();
                draw_info_bar();
                /* Draw current mode's screen */
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
                } else if (current_mode == MODE_MULTIMETER) {
                    meter_screen_invalidate();
                    draw_meter_screen();
                } else if (current_mode == MODE_SIGNAL_GEN) {
                    draw_siggen_screen(frame);
                } else if (current_mode == MODE_SETTINGS) {
                    draw_settings_screen();
                }
                break;
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

        /* Track mode transitions for SPI3 acquisition warmup */
        static device_mode_t last_rendered_mode = (device_mode_t)0xFF;
        static uint32_t scope_entered_frame = 0;
        if (current_mode != last_rendered_mode) {
            if (current_mode == MODE_OSCILLOSCOPE)
                scope_entered_frame = frame;
            last_rendered_mode = current_mode;
        }

#ifndef EMULATOR_BUILD
        if (fpga_service_requests()) {
            scope_entered_frame = frame;
        }
#endif

        /* Periodic redraws for active modes.
         *
         * All modes receive explicit redraws via queue commands above
         * (DCMD_DRAW_SCOPE, DCMD_DRAW_SIGGEN, etc.) when button presses
         * or data events require it. The code below only handles cases
         * that need continuous or periodic updates beyond those events.
         *
         * Before the meter fix (2026-04-04), scope and siggen both
         * redrew unconditionally every 50ms tick, causing visible
         * flicker from the full-area lcd_fill_rect + repaint sequence.
         * Now all three data modes are gated the same way. */
        if (current_mode == MODE_OSCILLOSCOPE) {
            const scope_state_t *ss_anim = scope_state_get();
            if (ss_anim->running) {
                /* ── Scope heartbeat / acquisition re-arm ───────────
                 * Keep the existing warmup and cadence for now, but use the
                 * stock-like cmd-3 path instead of a bare SPI trigger. That
                 * re-applies timebase state before each queued read, matching
                 * the live shell experiments more closely. */
#ifndef EMULATOR_BUILD
                if ((frame - scope_entered_frame) >= 10 /* 500ms warmup */
                    && (frame % 7) == 0
                    && fpga.initialized) {
                    fpga_scope_heartbeat();
                }
#endif

                /* Redraw when: popup counting down, new SPI3 data arrived,
                 * or ~1s safety tick for time-based UI elements. */
                static uint32_t last_scope_frame = 0;
                static uint16_t last_spi3_ok = 0;
                bool new_data = (fpga.spi3_ok_count != last_spi3_ok);
                if (new_data) last_spi3_ok = fpga.spi3_ok_count;

                if (scope_popup_active() || new_data
                    || (frame - last_scope_frame) >= 20) {
#ifdef FEATURE_FFT
                    if (scope_view == SCOPE_VIEW_FFT)
                        draw_fft_screen();
                    else if (scope_view == SCOPE_VIEW_SPLIT)
                        draw_split_screen(frame);
                    else if (scope_view == SCOPE_VIEW_WATERFALL)
                        draw_waterfall_screen();
                    else
#endif
#ifndef EMULATOR_BUILD
                    /* Rendering pass (2026-08-12): once real acquisition is
                     * flowing, use the flicker-free column compositor for
                     * periodic updates. The full clear-then-redraw path
                     * remains for popups (they overwrite the band and need
                     * erasing afterward), cursors (drawn only by the full
                     * path), the demo/pre-data state, and all button-driven
                     * DCMD redraws (settings changes want a full repaint). */
                    if (!scope_popup_active() && scope_acquisition_ready()
                        && ss_anim->cursor.mode == CURSOR_OFF)
                        draw_scope_live_frame();
                    else
#endif
                        draw_scope_screen(frame);
                    last_scope_frame = frame;
                }
            }
        } else if (current_mode == MODE_SIGNAL_GEN) {
            /* Siggen UI is static — redraws only on parameter changes
             * via DCMD_DRAW_SIGGEN queue commands from input_handler.
             * No continuous animation needed. */
        } else if (current_mode == MODE_MULTIMETER) {
            /* Redraw the meter when the visible reading changes, when the
             * submode changes, or when a debug/animated panel explicitly
             * needs a heartbeat. Rejected frames still advance raw counters,
             * but they must not clear/repaint the LCD. */
            static uint32_t last_meter_update = 0xFFFFFFFFu;
            static uint32_t last_meter_frame  = 0;
            static uint8_t  last_meter_submode = 0xFFu;
            uint32_t uc = meter_reading.display_update_count;
            bool submode_changed = (meter_submode != last_meter_submode);
            bool enough_time = (frame - last_meter_frame) >= 5;  /* 20Hz loop -> max 4Hz redraw */
            bool periodic_due = meter_screen_needs_periodic_redraw() &&
                                ((frame - last_meter_frame) >= 20);
            if (submode_changed || ((uc != last_meter_update) && enough_time) ||
                periodic_due) {
                draw_meter_screen();
                last_meter_update = uc;
                last_meter_frame  = frame;
                last_meter_submode = meter_submode;
            }
        }

        frame++;
    }
}

/*
 * Input Task (Priority 4 — highest user task, matches original firmware)
 *
 * Receives hardware-debounced button presses from the TMR3 matrix scan
 * driver (button_scan.c), then delegates to input_handle_button() for
 * all action logic.
 *
 * The old GPIO polling code was replaced after hardware testing confirmed
 * the buttons use a bidirectional 4x3 matrix requiring active scanning.
 * See: reverse_engineering/analysis_v120/button_map_confirmed.md
 */
static void vInputTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        button_id_t pressed;

        /* Block until TMR3 ISR confirms a debounced button press */
        if (xQueueReceive(xInputQueue, &pressed, pdMS_TO_TICKS(100)) == pdTRUE) {
            input_handle_button(pressed, xDisplayQueue);
        }

        /* Check in with health monitor */
        health_checkin(health_slot_input);
    }
}

/*
 * Timer callback — runs every 1 second
 */
static void vOneSecondTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    uptime_seconds++;
    battery_update();
    uint8_t cmd = DCMD_DRAW_STATUS_BAR;
    /* Zero timeout is intentional: timer service task is highest priority,
     * so blocking here would stall all timers including the health check.
     * A dropped status bar update is harmless — the next one catches up. */
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
    /* Power hold — PC9 HIGH to keep device on (MUST be first!) */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xF << 4)) | (0x3 << 4); /* PC9 push-pull 50MHz */
    GPIOC->scr = (1 << 9);  /* PC9 HIGH */

    /* Set VTOR. Normal build runs at 0x08004000 under our HID bootloader;
     * GUEST_BUILD runs at 0x08007000 under the FNIRSI stock bootloader (unit #2). */
#ifdef GUEST_BUILD
    SCB->VTOR = FLASH_BASE | 0x7000;
#else
    SCB->VTOR = FLASH_BASE | 0x4000;
#endif

    /* Check if previous run requested DFU reboot (magic word in RAM).
     * RAM persists across soft reset, so the magic word survives. */
    dfu_check_magic();

    /* NOTE: Boot button DFU check (dfu_check_boot_button) is disabled for now.
     * PC8 (POWER) reads LOW during power-on since it's the same button that
     * turns the device on, causing false DFU entry. Need a different trigger
     * (e.g., MENU + POWER combo, or long-hold detection with LCD feedback).
     * For now, use Settings > Firmware Update for software DFU entry. */

    /* NOTE: EOPB0 must be set to 0xFE (224KB SRAM) via DFU option bytes
     * before this firmware will work. See eopb0_setup.c or use:
     *   dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D option_bytes48.bin */

    /* Feed watchdog early — IWDG may still be running from previous boot
     * (it can't be stopped once started, survives system reset) */
    wdt_counter_reload();

    /* Arm the RTT console before anything else can want to print. Costs a few
     * hundred cycles and no peripheral, so it is safe this early — and it means
     * boot-time output is captured even if the device later hangs. */
    rtt_init();

    /* Enable the configurable fault handlers and clear stale CFSR bits before
     * anything can fault. Also recovers the record from a previous boot, which
     * survives reset in .noinit — the point of the whole exercise for a fault
     * that only shows up ~55s in. */
    fault_init();

    /* Clock init to 240MHz */
    system_clock_config();

    wdt_counter_reload();

    /* Enable peripheral clocks */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

#if FPGA_WARM_HANDOFF_TEST
    /* Warm-handoff round 2: drive the FPGA control pins to stock's scope-run
     * posture as EARLY as possible (right after the GPIO clocks) to minimise
     * the float window. Round 1 left these floating through ~2 s of init, which
     * knocked the FPGA out of continuous-capture; we caught one buffer then it
     * went idle. Restore PC6(SPI-en)=HIGH, PB11(active)=HIGH, PC11(meter mux)=LOW
     * (scope posture) before anything else. See docs/fpga_warm_handoff_test.md. */
    GPIOC->cfglr = (GPIOC->cfglr & ~(0xFu << 24)) | (0x3u << 24); /* PC6 PP 50MHz */
    GPIOC->scr   = (1u << 6);                                     /* PC6 HIGH */
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xFu << 12)) | (0x3u << 12); /* PB11 PP 50MHz */
    GPIOB->scr   = (1u << 11);                                    /* PB11 HIGH */
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 12)) | (0x3u << 12); /* PC11 PP 50MHz */
    GPIOC->clr   = (1u << 11);                                    /* PC11 LOW (scope) */
#endif

    /* PB8 = LCD backlight ON */
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xF << 0)) | (0x3 << 0); /* PB8 push-pull 50MHz */
    GPIOB->scr = (1 << 8);
#endif

    /* Initialize battery ADC (PB1) */
    battery_adc_init();

    wdt_counter_reload();

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

    wdt_counter_reload();
    /* LCD init complete — UI will be drawn by FreeRTOS display task */

    /*
     * FFT engine is initialized ON DEMAND when user enters FFT view
     * (via BTN_PRM in input_handler.c). This keeps the 88KB shared
     * memory pool free for other features until actually needed.
     */

#ifndef EMULATOR_BUILD
    /* Initialize signal generator and DAC hardware */
    {
        extern void dac_output_init(void);
        dac_output_init();

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

    /* Initialize meter data parser */
    meter_data_init();
    meter_voltage_wave_init();
    startup_mode_load();
    current_mode = startup_target_mode();

    /* Factory calibration stub: initializes the flash_fs mutex and
     * attempts to load per-channel cal blobs from SPI flash into the
     * RAM mirror. Currently a no-op read (real SPI flash driver is not
     * yet wired) — meter/scope paths still use built-in defaults.
     * Phase 3 will apply the loaded coefficients. */
    (void)flash_fs_init();
    (void)flash_fs_load_factory_cal();

#ifndef EMULATOR_BUILD
    /* Initialize FPGA communication (USART2 + SPI3 + boot sequence).
     * Must happen after clock init and GPIO clocks are enabled.
     * Sends boot commands, configures SPI3 Mode 3, performs handshake,
     * sets PB11 HIGH (FPGA active mode) and PC6 HIGH (SPI enable). */
    /* Initialize USB CDC debug shell (HICK 48MHz clock + CDC class).
     * Must happen after clock init. Enables USB pull-up so device
     * appears on the bus immediately — the debug task handles data
     * once the scheduler starts. */
    usb_debug_init();

    wdt_counter_reload();
    fpga_init();
    wdt_counter_reload();
#endif

    /* Create queues */
    xDisplayQueue = xQueueCreate(20, sizeof(uint8_t));
    xInputQueue   = xQueueCreate(15, sizeof(button_id_t));

    /* Initialize button matrix scan driver (TMR3 ISR at 500Hz).
     * This replaces the old passive GPIO reads that didn't work on hardware.
     * The driver handles all GPIO config for the 4x3 matrix + 3 passive pins. */
    button_scan_init(xInputQueue);

    /* Create tasks */
    xTaskCreate(vDisplayTask, "display", 768, NULL, 1, &xDisplayTaskHandle);
    xTaskCreate(vInputTask,   "key",     384, NULL, 4, &xInputTaskHandle);

    /* Register tasks with health monitor */
    health_slot_display = health_register("display", xDisplayTaskHandle);
    health_slot_input   = health_register("key",     xInputTaskHandle);

#ifndef EMULATOR_BUILD
    /* Create FPGA communication tasks (USART TX/RX + SPI3 acquisition).
     * These match the stock firmware's dvom_TX, dvom_RX, and fpga tasks. */
    fpga_create_tasks();

    /* Create USB debug shell task (CDC virtual serial port).
     * Priority 2 — above display (1) but below input (4) and FPGA tasks. */
    usb_debug_create_task();

    if (current_mode == MODE_MULTIMETER) {
        fpga_set_meter_mode(meter_submode);
    } else {
        fpga_enter_scope_mode();
    }
#endif

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

    /* Boot validation: tell the bootloader we started successfully.
     * Clears the boot attempt counter so we won't enter safe mode on
     * next reset. Must be called after LCD init and task creation —
     * if we got here, the firmware is healthy. */
    boot_validate();

    /* Initialize watchdog LAST — after all tasks and timers are running.
     * Once enabled, the FWDGT cannot be stopped (hardware limitation).
     * Skipped in GUEST_BUILD so interactive SPI3 shell sessions on unit #2
     * can't trip a watchdog reset mid-experiment. */
#ifndef GUEST_BUILD
    watchdog_init();
#endif

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
/* Must match AT32 HAL declaration: "extern unsigned int system_core_clock".
 * The HAL's system_at32f403a_407.c also defines these symbols, but the
 * emu build uses --allow-multiple-definition so main.o's versions win
 * (linked first). */
unsigned int system_core_clock = 240000000;

void SystemInit(void)
{
    /* Stub — do nothing in emulator mode. */
}

void system_clock_config(void)
{
    /* Stub — do nothing in emulator mode. */
}
#endif
