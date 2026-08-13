/*
 * OpenScope 2C53T - experimental continuity buzzer drive.
 */

#include "continuity_buzzer.h"
#include "at32f403a_407.h"
#include "meter_data.h"
#include "ui.h"

#include "FreeRTOS.h"
#include "task.h"

#define BUZZER_PIN       GPIO_PINS_9
#define BUZZER_GPIO      GPIOB
#define BUZZER_TASK_MS   1U
#define BUZZER_IDLE_MS   5U
#define BUZZER_LATCH_MS  350U

static TaskHandle_t buzzer_task_handle;
static volatile TickType_t buzzer_force_until_tick;
static volatile bool buzzer_task_started;
static volatile bool buzzer_active_now;
static volatile uint32_t buzzer_toggle_count;
static volatile uint32_t buzzer_create_fail_count;

static bool continuity_short_confirmed(const meter_reading_t *reading)
{
    return reading->continuity_beep ||
           reading->result_class == METER_RESULT_CONTINUITY;
}

static bool continuity_short_active(TickType_t now)
{
    static TickType_t short_latch_until_tick;
    meter_reading_t reading;

    if (meter_submode != 7 || !meter_data_snapshot(&reading) ||
        !reading.valid || reading.submode != 7) {
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
