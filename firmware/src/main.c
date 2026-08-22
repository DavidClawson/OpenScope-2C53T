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
#include "cal_dump.h"

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
#include "continuity_buzzer.h"
#include "dfu_boot.h"
#include "battery.h"
#include "fpga.h"
#include "meter_autoselect.h"
#include "meter_data.h"
#include "meter_voltage_wave.h"
#include "flash_fs.h"
#include "settings_store.h"
#include "usb_debug.h"
#include "rtt.h"
#include "fault.h"
#include "redraw_gate.h"

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
volatile uint8_t       meter_submode = 0;   /* 0-10: current meter sub-mode */
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

/* Modal overlay lock. While true, the display task suppresses ALL rendering
 * (queue commands are drained and dropped, periodic repaints skipped) so an
 * overlay painted from another task — currently only the BTN_POWER shutdown
 * countdown in input_handler.c — stays on screen instead of being overdrawn
 * by the next scope frame. The overlay owner clears the flag and sends
 * DCMD_REDRAW_ALL to hand the screen back. */
volatile bool          ui_modal_active = false;

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
 * Redraw gating for the scope and signal-generator screens
 *
 * The multimeter was fixed on 2026-04-04 by refusing to repaint unless
 * its reading counter had moved. Scope and siggen kept the old
 * behaviour: the scope repainted on an unconditional 1 s tick whether or
 * not anything had changed, and took the clear-and-repaint path at the
 * capture rate whenever cursors were on or the view was FFT/waterfall;
 * siggen full-cleared the whole 320x240 on every button press, including
 * presses that hit a clamp and changed nothing.
 *
 * The gate itself is in ui/redraw_gate.h (pure C, host-tested). This
 * file supplies the two "epochs" — hashes over exactly the state each
 * screen renders. Equal epoch + no new samples => the next draw would
 * produce identical pixels.
 *
 * Cadences below are in display-task ticks (50 ms each).
 * ═══════════════════════════════════════════════════════════════════ */

/* Floor between data-driven FULL repaints. The live compositor
 * (draw_scope_live_frame) is exempt — it never blanks — so this only
 * throttles the paths that do clear: FFT, split, waterfall, cursors-on,
 * and the pre-data demo state. 4 ticks = 5 Hz. */
#define SCOPE_FULL_MIN_FRAMES   4

/* Cadence while the screen is genuinely a function of `frame`: the
 * synthetic demo waveform before the first real sample, and the
 * persistence overlay (which decays once per draw). Unchanged from the
 * pre-B2 tick so the demo animation looks the same. */
#define SCOPE_ANIM_TICK_FRAMES  20

/* Idle heartbeat, split by whether the heartbeat blanks the screen.
 *
 * Something must still tick over when nothing changes, because the
 * SCOPE_DEBUG_OVERLAY strip (compiled into every build today, Makefile
 * C_DEFS) shows SPI3 timeout and status counters that move without
 * touching anything the epoch can see — and that strip is the only
 * instrument on bench unit #1.
 *
 * draw_scope_live_frame() redraws that strip as opaque fixed-width text
 * with no refill (scope_ui.c), so when the compositor is available the
 * heartbeat costs no blank and can stay at the pre-B2 1 s. When it is
 * not available the heartbeat is a full clear-and-repaint, and that is
 * the one that was flashing a settled screen once a second forever. */
#define SCOPE_IDLE_LIVE_FRAMES  20    /* 1 s, flicker-free */
#define SCOPE_IDLE_FULL_FRAMES  100   /* 5 s, blanks — back it off */

/* Draw accounting. Non-static on purpose: these are the evidence that
 * the gate works, and they need to be reachable from a debugger or a
 * future overlay without recompiling. */
volatile uint32_t ui_scope_full_draws  = 0;
volatile uint32_t ui_scope_live_draws  = 0;
volatile uint32_t ui_scope_skipped     = 0;
volatile uint32_t ui_siggen_full_draws = 0;
volatile uint32_t ui_siggen_skipped    = 0;

static redraw_gate_t scope_gate  = { 0, 0, 0, false, false };
static redraw_gate_t siggen_gate = { 0, 0, 0, false, false };

/* Hash of everything draw_scope_screen()/draw_fft_screen()/
 * draw_waterfall_screen() read out of program state. Sample data is NOT
 * hashed — that is what the new_data term is for — so this stays valid
 * as the FFT/waterfall/measurement-badge inputs get rewired to real
 * capture. */
static uint32_t scope_ui_epoch(const scope_state_t *s)
{
    uint32_t h = REDRAW_EPOCH_SEED;

    h = redraw_epoch_mix(h, (uint32_t)theme_get_id());
    h = redraw_epoch_mix(h, (uint32_t)active_channel);
    h = redraw_epoch_mix(h, (uint32_t)s->timebase_idx
                            | ((uint32_t)s->running << 8));

    h = redraw_epoch_mix(h, (uint32_t)s->ch1.enabled
                            | ((uint32_t)s->ch1.vdiv_idx << 1)
                            | ((uint32_t)s->ch1.coupling << 8)
                            | ((uint32_t)s->ch1.probe    << 12)
                            | ((uint32_t)s->ch1.bw_limit << 16));
    h = redraw_epoch_mix(h, (uint32_t)(uint16_t)s->ch1.position);
    h = redraw_epoch_mix(h, (uint32_t)s->ch2.enabled
                            | ((uint32_t)s->ch2.vdiv_idx << 1)
                            | ((uint32_t)s->ch2.coupling << 8)
                            | ((uint32_t)s->ch2.probe    << 12)
                            | ((uint32_t)s->ch2.bw_limit << 16));
    h = redraw_epoch_mix(h, (uint32_t)(uint16_t)s->ch2.position);

    h = redraw_epoch_mix(h, (uint32_t)s->trigger.mode
                            | ((uint32_t)s->trigger.edge   << 4)
                            | ((uint32_t)s->trigger.source << 8));
    h = redraw_epoch_mix(h, (uint32_t)(uint16_t)s->trigger.level);

    h = redraw_epoch_mix(h, (uint32_t)s->cursor.mode
                            | ((uint32_t)s->cursor.active << 4));
    h = redraw_epoch_mix(h, ((uint32_t)s->cursor.v1_x << 16) | s->cursor.v2_x);
    h = redraw_epoch_mix(h, ((uint32_t)s->cursor.h1_y << 16) | s->cursor.h2_y);

    h = redraw_epoch_mix(h, (uint32_t)math_enabled
                            | ((uint32_t)math_op         << 1)
                            | ((uint32_t)persist_enabled << 8));

#ifdef FEATURE_FFT
    h = redraw_epoch_mix(h, (uint32_t)scope_view);
    {
        const fft_config_t *fc = fft_get_config();
        if (fc != NULL) {
            h = redraw_epoch_mix_f(h, fc->ref_level_db);
            h = redraw_epoch_mix_f(h, fc->db_range);
            h = redraw_epoch_mix_f(h, fc->sample_rate_hz);
            h = redraw_epoch_mix(h, (uint32_t)fc->window
                                    | ((uint32_t)fc->peak_count << 8)
                                    | ((uint32_t)fc->avg_count  << 16)
                                    | ((uint32_t)fc->max_hold   << 24));
            h = redraw_epoch_mix(h, ((uint32_t)fc->zoom_start_bin << 16)
                                    | fc->zoom_end_bin);
        }
    }
#endif
    return h;
}

/* Hash of everything draw_siggen_screen() renders. */
static uint32_t siggen_ui_epoch(void)
{
    uint32_t h = REDRAW_EPOCH_SEED;
    const siggen_config_t *c = siggen_get_config();

    h = redraw_epoch_mix(h, (uint32_t)theme_get_id());
    if (c == NULL) return h;

    h = redraw_epoch_mix(h, (uint32_t)c->waveform
                            | ((uint32_t)c->duty_cycle_pct << 8)
                            | ((uint32_t)c->output_enabled << 16));
    h = redraw_epoch_mix_f(h, c->frequency_hz);
    h = redraw_epoch_mix_f(h, c->amplitude_vpp);
    h = redraw_epoch_mix_f(h, c->offset_v);
    return h;
}

/* Can this frame be served by the flicker-free column compositor?
 * It draws the trace band only, so anything living outside that band or
 * layered on top (popup, cursors) forces the full path. */
static bool scope_incremental_available(const scope_state_t *s)
{
#ifdef EMULATOR_BUILD
    (void)s;
    return false;   /* draw_scope_live_frame reads FPGA buffers */
#else
#ifdef FEATURE_FFT
    if (scope_view != SCOPE_VIEW_TIME) return false;
#endif
    /* A popup no longer disqualifies the compositor. It used to, and that is
     * what made every timebase change blank the whole screen ten times over:
     * the popup pinned the renderer to the full clear-then-redraw for its
     * entire life. draw_scope_live_frame() now steps around the popup box.
     * Cursors still force the full path — only it draws them. */
    return scope_acquisition_ready()
           && s->cursor.mode == CURSOR_OFF;
#endif
}

/* Single scope render entry point, shared by the queue commands and the
 * periodic path so both always pick the same renderer for a given view. */
static void scope_render(uint32_t frame, redraw_action_t action)
{
    (void)action;

#ifdef FEATURE_FFT
    if (scope_view == SCOPE_VIEW_FFT)       { ui_scope_full_draws++;
                                              draw_fft_screen();        return; }
    if (scope_view == SCOPE_VIEW_SPLIT)     { ui_scope_full_draws++;
                                              draw_split_screen(frame); return; }
    if (scope_view == SCOPE_VIEW_WATERFALL) { ui_scope_full_draws++;
                                              draw_waterfall_screen();  return; }
    if (scope_view == SCOPE_VIEW_XY)        { ui_scope_full_draws++;
                                              draw_xy_screen();         return; }
#endif
#ifndef EMULATOR_BUILD
    if (action == REDRAW_INCREMENTAL) {
        ui_scope_live_draws++;
        draw_scope_live_frame();
        scope_popup_overlay_tick();
        return;
    }
#endif
    ui_scope_full_draws++;
    draw_scope_screen(frame);
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

    /* The splash is already on screen — main() paints it before the backlight
     * comes up and before the FPGA config sequence, so by the time this task
     * runs it has typically been visible for seconds. Hold it briefly for
     * builds where init is fast (emulator, warm boots), then move on. */
    vTaskDelay(pdMS_TO_TICKS(750));

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
        /* Track mode transitions: for SPI3 acquisition warmup, and to
         * invalidate both redraw gates. Entering a screen must always
         * repaint it, even when that screen's own state is unchanged
         * from the last time it was visible. Hoisted above the queue
         * handling (2026-08-13) so a DCMD arriving on the same tick as a
         * mode switch sees an already-invalidated gate. */
        static device_mode_t last_rendered_mode = (device_mode_t)0xFF;
        static uint32_t scope_entered_frame = 0;
        if (current_mode != last_rendered_mode) {
            /* PC11 = meter MUX enable. Stock switches it WITH the mode (HIGH in
             * meter mode, LOW in scope mode); we used to pin it LOW forever, so
             * the meter was dead in every scope build and every USART
             * measurement was taken with the far end switched off. Bench A/B/A
             * 2026-08-17: HIGH -> 276 bytes returned, LOW -> 0, HIGH -> 276. */
            fpga_set_meter_mux(current_mode == MODE_MULTIMETER);
            if (current_mode == MODE_OSCILLOSCOPE)
                scope_entered_frame = frame;
            redraw_gate_invalidate(&scope_gate);
            redraw_gate_invalidate(&siggen_gate);
            last_rendered_mode = current_mode;
        }

        /* Check for commands (non-blocking with short timeout for animation).
         * While a modal overlay owns the screen (ui_modal_active), commands
         * are still drained — senders must never block — but dropped instead
         * of rendered: one frame painted here would overdraw the overlay,
         * which is exactly the countdown flicker this flag exists to stop.
         * The overlay owner ends with DCMD_REDRAW_ALL, which repaints
         * everything a dropped command would have. */
        if (xQueueReceive(xDisplayQueue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (ui_modal_active)
                cmd = DCMD_NONE_MODAL_DROP;
            switch (cmd) {
            case DCMD_DRAW_SPLASH:
                draw_splash();
                break;
            case DCMD_REDRAW_ALL:
                /* NEVER GATED, deliberately. This is the hard invalidate,
                 * and some of its senders have already scribbled on the
                 * LCD from outside this task — BTN_POWER paints its
                 * "Hold to power off" overlay inline (input_handler.c) and
                 * relies on REDRAW_ALL to erase it on release. An epoch
                 * comparison cannot see that, so gating here would strand
                 * the overlay on screen. Screens that want cheap,
                 * gate-able repaints send their own DCMD_DRAW_* instead. */
                lcd_clear(COLOR_BLACK);
                status_bar_invalidate();
                draw_status_bar();
                draw_info_bar();
                /* Draw current mode's screen */
                if (current_mode == MODE_OSCILLOSCOPE) {
                    scope_render(frame, REDRAW_FULL);
                    redraw_gate_mark(&scope_gate,
                                     scope_ui_epoch(scope_state_get()),
                                     frame, REDRAW_FULL);
                } else if (current_mode == MODE_MULTIMETER) {
                    meter_screen_invalidate();
                    draw_meter_screen();
                } else if (current_mode == MODE_SIGNAL_GEN) {
                    ui_siggen_full_draws++;
                    draw_siggen_screen(frame);
                    redraw_gate_mark(&siggen_gate, siggen_ui_epoch(),
                                     frame, REDRAW_FULL);
                } else if (current_mode == MODE_SETTINGS) {
                    draw_settings_screen();
                }
                break;
            case DCMD_DRAW_SCOPE:
                /* Always honoured: the sender changed scope state or
                 * raised a popup. Marking the gate stops the periodic
                 * path from immediately repeating the same paint. */
                if (current_mode == MODE_OSCILLOSCOPE) {
                    scope_render(frame, REDRAW_FULL);
                    redraw_gate_mark(&scope_gate,
                                     scope_ui_epoch(scope_state_get()),
                                     frame, REDRAW_FULL);
                }
                break;
            case DCMD_DRAW_METER:
                if (current_mode == MODE_MULTIMETER) draw_meter_screen();
                break;
            case DCMD_DRAW_SIGGEN:
                if (current_mode == MODE_SIGNAL_GEN) {
                    if (siggen_gate.primed &&
                        siggen_ui_epoch() == siggen_gate.drawn_epoch) {
                        ui_siggen_skipped++;
                        break;
                    }
                    ui_siggen_full_draws++;
                    draw_siggen_screen(frame);
                    redraw_gate_mark(&siggen_gate, siggen_ui_epoch(),
                                     frame, REDRAW_FULL);
                }
                break;
            case DCMD_DRAW_SETTINGS:
                if (current_mode == MODE_SETTINGS) draw_settings_screen();
                break;
            case DCMD_DRAW_STATUS_BAR:
                draw_status_bar();
                break;
#ifdef FEATURE_FFT
            case DCMD_DRAW_FFT:
                if (current_mode == MODE_OSCILLOSCOPE) {
                    ui_scope_full_draws++;
                    draw_fft_screen();
                    redraw_gate_mark(&scope_gate,
                                     scope_ui_epoch(scope_state_get()),
                                     frame, REDRAW_FULL);
                }
                break;
#endif
            default:
                break;
            }
        }

        /* Check in with health monitor */
        health_checkin(health_slot_display);

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
         * The meter was gated then; scope and siggen were gated on
         * 2026-08-13 (B2) via redraw_gate.h — see the epoch functions
         * above. All three data modes are now gated the same way.
         *
         * ui_modal_active suppresses this whole section: while the shutdown
         * countdown (or any future modal) owns the screen, a live scope
         * would otherwise overdraw it within one 50 ms tick — the "flashing
         * countdown" bug (2026-08-20). Acquisition heartbeats pause with it;
         * three seconds of paused re-arm is harmless and resumes on the
         * DCMD_REDRAW_ALL that ends every modal. */
        if (ui_modal_active) {
            frame++;
            continue;
        }
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

                /* Redraw decision (B2, 2026-08-13).
                 *
                 * Previously: popup OR new SPI3 data OR an unconditional
                 * 1 s tick. The 1 s arm repainted a settled screen once a
                 * second forever, and the data arm ran the full
                 * clear-and-repaint at the capture rate in every view the
                 * live compositor cannot serve — the flash observed on
                 * the bench 2026-08-12.
                 *
                 * Now: redraw_gate_step() picks between skipping, the
                 * flicker-free compositor (unthrottled, so the live trace
                 * still updates at the capture rate) and a rate-capped
                 * full repaint, using an epoch over everything the screen
                 * renders. Rendering pass note (2026-08-12) still holds:
                 * the full clear-then-redraw path stays for popups (they
                 * overwrite the band and need erasing afterward), cursors
                 * (drawn only by the full path), the demo/pre-data state,
                 * and all button-driven DCMD redraws. */
                static uint16_t last_spi3_ok = 0;
                uint16_t spi3_now = fpga.spi3_ok_count;
                bool new_data = (spi3_now != last_spi3_ok);
                last_spi3_ok = spi3_now;

                redraw_query_t q;
                q.frame            = frame;
                q.epoch            = scope_ui_epoch(ss_anim);
                /* A live popup counts as new data so the compositor runs on
                 * every 50 ms tick while it is up. POPUP_DURATION is measured
                 * in DRAWS, and on the incremental path draws follow capture,
                 * so a quiet input would otherwise stretch a 500 ms popup to
                 * ten seconds. This keeps the popup on wall-clock without
                 * reintroducing a single full repaint. */
                q.new_data         = new_data || scope_popup_active();
                q.incremental_ok   = scope_incremental_available(ss_anim);
                /* Only the full path can show a popup when the compositor is
                 * unavailable (demo trace, cursors on, FFT views). When it IS
                 * available the popup rides along as an overlay, so forcing
                 * here would reinstate exactly the flashing this replaced. */
                q.force            = scope_popup_active() && !q.incremental_ok;
                /* Both of these genuinely produce new pixels every tick
                 * from `frame` alone: the synthetic demo waveform (only
                 * drawn before the first real sample) and the
                 * persistence overlay (which decays once per draw). */
                q.animating        = !scope_acquisition_ready() || persist_enabled;
                q.full_min_frames  = SCOPE_FULL_MIN_FRAMES;
                q.anim_tick_frames = SCOPE_ANIM_TICK_FRAMES;
                q.idle_tick_frames = q.incremental_ok ? SCOPE_IDLE_LIVE_FRAMES
                                                      : SCOPE_IDLE_FULL_FRAMES;

                redraw_action_t act = redraw_gate_step(&scope_gate, &q);
                if (act == REDRAW_SKIP)
                    ui_scope_skipped++;
                else
                    scope_render(frame, act);
            }
        } else if (current_mode == MODE_SIGNAL_GEN) {
            /* Siggen UI is static: no animation, no live data. It is
             * drawn only from queue commands, and those are gated on the
             * siggen epoch above, so a settled generator screen costs
             * zero LCD writes per tick. */
        } else if (current_mode == MODE_MULTIMETER) {
            /* Redraw the meter when the visible reading changes, when the
             * submode changes, or when a debug/animated panel explicitly
             * needs a heartbeat. Rejected frames still advance raw counters,
             * but they must not clear/repaint the LCD. */
            static uint32_t last_meter_update = 0xFFFFFFFFu;
            static uint32_t last_meter_frame  = 0;
            static uint8_t  last_meter_submode = 0xFFu;
            uint32_t uc = meter_reading.display_update_count;
            bool auto_select_running = meter_autoselect_is_running();
            bool submode_changed = (meter_submode != last_meter_submode);
            bool enough_time = (frame - last_meter_frame) >= 5;  /* 20Hz loop -> max 4Hz redraw */
            bool periodic_due = meter_screen_needs_periodic_redraw() &&
                                ((frame - last_meter_frame) >= 20);
            if (!auto_select_running &&
                (submode_changed || ((uc != last_meter_update) && enough_time) ||
                 periodic_due)) {
                draw_meter_screen();
                last_meter_update = meter_screen_last_reading_display_update;
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
    button_scan_start();

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

    /* PB8 = LCD backlight — configured as output but held LOW (dark) until
     * the panel has been initialized and the first frame painted. Raising it
     * here used to light the ST7789V's uninitialized GRAM for the entire LCD
     * init window (~0.7 s of visible static at every boot). The pin floats
     * during the bootloader, so drive it LOW explicitly for a deterministic
     * dark panel; it goes HIGH right after draw_splash() below. */
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xF << 0)) | (0x3 << 0); /* PB8 push-pull 50MHz */
    GPIOB->clr = (1 << 8);
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

    /* First frame, then light the backlight. Painting before PB8 goes HIGH is
     * what eliminates the boot static (uninitialized GRAM was visible from
     * the moment the backlight rose until the display task's first draw).
     * Drawing the splash HERE — not in the display task — also means the
     * multi-second FPGA config sequence below runs behind the logo instead of
     * behind garbage. The display task takes over rendering from this point. */
    draw_splash();
#ifndef EMULATOR_BUILD
    GPIOB->scr = (1 << 8);  /* PB8 = backlight ON, panel already showing splash */
#endif

#if CAL_DUMP_MODE
    /* `make guest-caldump` only. Reports MCU flash 0x08006000..0x08006FFF —
     * the saved-config sector that is stock's candidate home for per-device
     * calibration, and which nobody has ever read on any unit because RDP
     * blocks external debuggers (the CPU can read its own flash fine).
     *
     * Deliberately placed here: the LCD and backlight are up, and NOTHING
     * else has run — no FPGA init, no flash_fs, no settings store, no
     * scheduler. It never returns. A diagnostic image should do one thing,
     * and this one is read-only. See src/util/cal_dump.h. */
    cal_dump_run();
#endif

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
#if FPGA_WARM_HANDOFF_TEST
    current_mode = MODE_OSCILLOSCOPE;
#else
    current_mode = startup_target_mode();
#endif

    /* Factory calibration boundary: initialize the W25Q wrapper, then
     * leave the calibration mirror unloaded. Stock evidence has not
     * recovered a host-readable DMM factory-calibration file or H2/SPI3
     * apply proof, so this path deliberately fails closed instead of
     * consuming invented filenames or low-voltage coefficients. */
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

    /* User settings: bind the W25Q region layer, load the newest saved record
     * (or defaults) and apply it to live UI state. Never fails in a way main()
     * has to handle — no storage, or a corrupt/foreign record, simply means
     * defaults. See settings_store.h.
     *
     * POSITION IS DELIBERATE, and it is the one thing to re-check if the cold
     * boot ever regresses. It must come AFTER:
     *   - flash_fs_init(), whose mutex and raw SPI2 primitives it uses;
     *   - theme_init() / scope_state_init(), whose defaults it overwrites;
     *   - fpga_init(), because this is the first code in this firmware's
     *     history to drive SPI2 (and PB12/CS) before the scheduler, and the
     *     FPGA config sequence right above it is bench-validated and fragile.
     *     Keeping it downstream leaves that sequence's electrical environment
     *     exactly as it was validated.
     * And BEFORE the fpga_set_meter_mode(meter_submode) call below, which
     * consumes the restored meter submode, and before the display task exists. */
    settings_store_init();

    /* settings_store_init() just restored scope_state.timebase_idx. Push it at
     * the FPGA so the axis label and the actual sample rate agree — without
     * this the restore silently undoes the reconcile inside fpga_init(), which
     * is exactly what the 2026-08-19 bench run showed (reconcile reported
     * "pulled", display still read 0x0A). Must stay AFTER the restore and
     * BEFORE fpga_create_tasks(), which is the point the acquisition task
     * could start using SPI3. */
#ifndef EMULATOR_BUILD
    fpga_reconcile_timebase_after_arm();
    /* Same reconcile for the vertical axis: fpga_init() applied the frontend
     * relays before the restore, so push the restored vdiv indices out too
     * (EXP-19, 2026-08-20). */
    fpga_reconcile_frontend_after_arm();
#endif

    /* Create queues */
    xDisplayQueue = xQueueCreate(20, sizeof(uint8_t));
    xInputQueue   = xQueueCreate(15, sizeof(button_id_t));

    continuity_buzzer_init();

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
    continuity_buzzer_create_task();
    meter_autoselect_create_task();

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
