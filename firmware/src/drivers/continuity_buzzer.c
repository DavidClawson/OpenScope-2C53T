/*
 * OpenScope 2C53T - experimental continuity buzzer drive.
 */

#include "continuity_buzzer.h"
#include "continuity_veto.h"
#include "at32f403a_407.h"
#include "fpga.h"
#include "meter_data.h"
#include "ui.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define BUZZER_PIN       GPIO_PINS_9
#define BUZZER_GPIO      GPIOB
#define BUZZER_TASK_MS   1U
#define BUZZER_IDLE_MS   5U
#define BUZZER_LATCH_MS  350U

/* PC3 carries the meter SoC's BUZ output through an optocoupler: a 2.24 kHz
 * pulse train while the probes are shorted, no edges while open (#94,
 * docs/re/system_map_corrections.md section 6, unit #2). Stock maps EXTI3 to
 * PC3, rising edge (master_init 0x0802C75A), and its EXTI3 ISR sets the buzzer
 * duty on every edge, which is why it beeps 24-39 ms after contact. The
 * decoded reading lags contact by 475-1012 ms (EXP-93, my unit).
 *
 * So the beep follows PC3 EDGES, counted by EXINT3, and the reading only gets
 * a veto (see continuity_short_active). Edges, not the level: the pulse shape
 * at the pin depends on the optocoupler (on my unit a ~38 Hz level poll read
 * it as solid LOW), and an edge count does not care about duty or idle level. */
#define CONT_DETECT_GPIO      GPIOC
#define CONT_DETECT_PIN       GPIO_PINS_3
#define CONT_DETECT_MIN_EDGES 2U    /* new edges in one sample to start */
/* No edges this long: probes released. Stock's beep ends 100-110 ms after the
 * last edge (an 11-tick countdown on a 10 ms tick, 0x20002D67; Metalaria, PR
 * #102), and a tail that long also bridges scratchy contact. */
#define CONT_DETECT_HOLD_MS   105U

static volatile uint32_t pc3_edges;  /* EXINT3 rising edges since boot */

static TaskHandle_t buzzer_task_handle;
static volatile TickType_t buzzer_force_until_tick;
static volatile bool buzzer_task_started;
static volatile bool buzzer_active_now;
static volatile uint32_t buzzer_toggle_count;
static volatile uint32_t buzzer_create_fail_count;

/* Same rule as the screen (meter_data.c). Used by the fallback path; on the
 * PC3 path the beep leads the screen by the reading's lag (up to ~1 s). */
extern float meter_continuity_threshold(void);
static bool continuity_short_confirmed(const meter_reading_t *reading)
{
    return meter_continuity_is_short(reading, meter_continuity_threshold());
}

/* A resistance reading at or above the user's beep threshold. kOhm readings are
 * always above it (the softkey tops out at 100 Ohm). */
static bool reading_above_threshold(const meter_reading_t *r)
{
    if (!r->valid || r->result_class != METER_RESULT_NORMAL || !r->unit_suffix)
        return false;
    if (strcmp(r->unit_suffix, "kOhm") == 0)
        return true;
    return strcmp(r->unit_suffix, "Ohm") == 0 &&
           r->value >= meter_continuity_threshold();
}

#ifndef EMULATOR_BUILD
/* PC3 rising-edge counter. No FreeRTOS calls, so any NVIC priority is safe. */
void EXINT3_IRQHandler(void)
{
    exint_flag_clear(EXINT_LINE_3);
    pc3_edges++;
}
#endif

/* PC3 short detector. Sampled from buzzer_task every 1 ms while beeping and
 * every 5 ms while idle; at 2.24 kHz that is ~2 or ~11 edges per sample. */
static struct {
    bool       seen;          /* PC3 tone seen at least once this session */
    bool       is_short;
    uint32_t   last_edges;    /* pc3_edges at the previous sample */
    TickType_t last_edge_tick;
    TickType_t onset_tick;    /* when the tone started */
    cont_veto_t veto;         /* the reading's mute, continuity_veto.h */
} hw;

static void hw_detect_reset(void)
{
    memset(&hw, 0, sizeof hw);
    hw.last_edges = pc3_edges;
}

static void hw_detect_sample(TickType_t now)
{
    uint32_t e = pc3_edges;
    uint32_t fresh = e - hw.last_edges;
    hw.last_edges = e;

    /* One stray edge cannot start a beep; once beeping, any edge holds it. */
    if (fresh >= CONT_DETECT_MIN_EDGES || (hw.is_short && fresh > 0))
        hw.last_edge_tick = now;

    if (!hw.is_short && fresh >= CONT_DETECT_MIN_EDGES) {
        meter_reading_t r;
        hw.is_short = true;
        hw.seen = true;
        hw.onset_tick = now;
        cont_veto_start(&hw.veto, meter_data_snapshot(&r) ? r.update_count : 0);
    } else if (hw.is_short &&
               (now - hw.last_edge_tick) >= pdMS_TO_TICKS(CONT_DETECT_HOLD_MS)) {
        hw.is_short = false;
    }
}

static bool continuity_short_active(TickType_t now)
{
    static TickType_t short_latch_until_tick;
    meter_reading_t reading;

    if (current_mode != MODE_MULTIMETER || meter_submode != 7 ||
        fpga_meter_transition_busy()) {
        short_latch_until_tick = 0;
        hw_detect_reset();
        return false;
    }

    /* Fast path. Once the PC3 tone has been seen in this session it is the
     * only on/off source, so the beep also stops as soon as the probes
     * separate instead of waiting out the reading's lag. The other side of
     * that: a part between the SoC's threshold and a higher user setting does
     * not beep, though the screen says SHORT. The SoC's threshold is above
     * 47 Ohm on my unit and not measured further. The reading can only mute
     * the tone, for a part between the user's threshold and the SoC's, and
     * only with frames new enough to show the probes as they are now
     * (continuity_veto.h). */
    hw_detect_sample(now);
    if (hw.seen) {
        short_latch_until_tick = 0;
        if (!hw.is_short)
            return false;
        if (meter_data_snapshot(&reading))
            cont_veto_step(&hw.veto,
                           (uint32_t)((now - hw.onset_tick) * portTICK_PERIOD_MS),
                           reading.update_count,
                           reading_above_threshold(&reading));
        return !hw.veto.muted;
    }

    /* Fallback: no PC3 tone yet in this session (or PC3 is not wired on this
     * board). Beep from the decoded reading, as before EXP-93. */
    if (!meter_data_snapshot(&reading) || !reading.valid ||
        reading.submode != 7) {
        short_latch_until_tick = 0;
        return false;
    }

    if (continuity_short_confirmed(&reading)) {
        short_latch_until_tick = now + pdMS_TO_TICKS(BUZZER_LATCH_MS);
        return true;
    }

    if (reading.result_class == METER_RESULT_OVERLOAD ||
        reading.result_class == METER_RESULT_BLANK ||
        reading.result_class == METER_RESULT_NONE) {
        short_latch_until_tick = 0;
        return false;
    }

    return short_latch_until_tick != 0 &&
           (int32_t)(short_latch_until_tick - now) > 0;
}

void continuity_buzzer_init(void)
{
#ifdef EMULATOR_BUILD
    return;
#else
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    gpio_init_type cfg;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BUZZER_PIN;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(BUZZER_GPIO, &cfg);
    BUZZER_GPIO->clr = BUZZER_PIN;

    /* PC3 continuity detect: input with PULL-UP, as stock configures it
     * (gpio_init struct before 0x0802C74A: pins 0x0008, pull 0x18, mode 0x00).
     * The optocoupler can only pull the line low; without the pull-up it most
     * likely does not swing back (EXP-93 read it floating as solid LOW).
     * Rising edges are counted on EXINT3, as stock does. Same shape as
     * fpga.c's PC0 counter on EXINT0. */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = CONT_DETECT_PIN;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(CONT_DETECT_GPIO, &cfg);

    IOMUX->exintc1_bit.exint3 = 2;         /* EXINT line 3 <- port C */
    {
        exint_init_type ei;
        exint_default_para_init(&ei);
        ei.line_select   = EXINT_LINE_3;
        ei.line_mode     = EXINT_LINE_INTERRUPT;
        ei.line_polarity = EXINT_TRIGGER_RISING_EDGE;
        ei.line_enable   = TRUE;
        exint_init(&ei);
    }
    exint_flag_clear(EXINT_LINE_3);
    nvic_irq_enable(EXINT3_IRQn, 6, 0);
#endif
}

static void buzzer_task(void *arg)
{
    (void)arg;

    bool level = false;
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        bool force_active = buzzer_force_until_tick != 0 &&
                            (int32_t)(buzzer_force_until_tick - now) > 0;
        if (!force_active && buzzer_force_until_tick != 0) {
            buzzer_force_until_tick = 0;
        }
        buzzer_active_now = force_active || continuity_short_active(now);

        if (buzzer_active_now) {
            if (level) {
                BUZZER_GPIO->clr = BUZZER_PIN;
            } else {
                BUZZER_GPIO->scr = BUZZER_PIN;
            }
            level = !level;
            buzzer_toggle_count++;
            vTaskDelay(pdMS_TO_TICKS(BUZZER_TASK_MS));
        } else {
            BUZZER_GPIO->clr = BUZZER_PIN;
            level = false;
            vTaskDelay(pdMS_TO_TICKS(BUZZER_IDLE_MS));
        }
    }
}

void continuity_buzzer_create_task(void)
{
#ifdef EMULATOR_BUILD
    return;
#else
    if (buzzer_task_handle != NULL) {
        return;
    }
    if (xTaskCreate(buzzer_task, "buzz", 192, NULL, 2,
                    &buzzer_task_handle) == pdPASS) {
        buzzer_task_started = true;
    } else {
        buzzer_task_handle = NULL;
        buzzer_task_started = false;
        buzzer_create_fail_count++;
        BUZZER_GPIO->clr = BUZZER_PIN;
    }
#endif
}

void continuity_buzzer_force_ms(uint32_t duration_ms)
{
#ifdef EMULATOR_BUILD
    (void)duration_ms;
#else
    if (duration_ms == 0) {
        buzzer_force_until_tick = 0;
        buzzer_active_now = false;
        BUZZER_GPIO->clr = BUZZER_PIN;
        return;
    }
    if (duration_ms > 5000U) {
        duration_ms = 5000U;
    }
    buzzer_force_until_tick = xTaskGetTickCount() + pdMS_TO_TICKS(duration_ms);
#endif
}

void continuity_buzzer_snapshot(bool *task_started, bool *active,
                                uint32_t *toggle_count,
                                uint32_t *create_fail_count)
{
    if (task_started) {
        *task_started = buzzer_task_started;
    }
    if (active) {
        *active = buzzer_active_now;
    }
    if (toggle_count) {
        *toggle_count = buzzer_toggle_count;
    }
    if (create_fail_count) {
        *create_fail_count = buzzer_create_fail_count;
    }
}
