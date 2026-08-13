/*
 * OpenScope 2C53T - asynchronous DMM function auto-select service
 */

#include "meter_autoselect.h"

#include "FreeRTOS.h"
#include "task.h"

#include "fpga.h"
#include "meter_auto.h"
#include "meter_data.h"
#include "meter_voltage_wave.h"
#include "ui.h"

#define METER_AUTOSELECT_DEFAULT_SETTLE_MS 2500U
#define METER_AUTOSELECT_MAX_SETTLE_MS     3000U
#define METER_AUTOSELECT_MIN_WAIT_MS       2500U
#define METER_AUTOSELECT_POLL_MS           100U

static volatile bool autoselect_start_pending;
static volatile bool autoselect_cancel_pending;
static volatile uint32_t autoselect_requested_settle_ms =
    METER_AUTOSELECT_DEFAULT_SETTLE_MS;

static meter_autoselect_status_t autoselect_status = {
    .state = METER_AUTOSELECT_IDLE,
};

const char *meter_autoselect_state_name(meter_autoselect_state_t state)
{
    switch (state) {
    case METER_AUTOSELECT_IDLE:      return "idle";
    case METER_AUTOSELECT_RUNNING:   return "running";
    case METER_AUTOSELECT_DONE:      return "done";
    case METER_AUTOSELECT_CANCELLED: return "cancelled";
    default:                         return "?";
    }
}

bool meter_autoselect_start(uint32_t settle_ms)
{
    if (settle_ms == 0U) settle_ms = METER_AUTOSELECT_DEFAULT_SETTLE_MS;
    if (settle_ms > METER_AUTOSELECT_MAX_SETTLE_MS) return false;

    taskENTER_CRITICAL();
    if (autoselect_start_pending ||
        autoselect_status.state == METER_AUTOSELECT_RUNNING) {
        taskEXIT_CRITICAL();
        return false;
    }
    autoselect_requested_settle_ms = settle_ms;
    autoselect_cancel_pending = false;
    autoselect_start_pending = true;
    taskEXIT_CRITICAL();
    return true;
}

void meter_autoselect_cancel(void)
{
    taskENTER_CRITICAL();
    autoselect_cancel_pending = true;
    taskEXIT_CRITICAL();
}

bool meter_autoselect_is_running(void)
{
    bool running;

    taskENTER_CRITICAL();
    running = autoselect_start_pending ||
              autoselect_status.state == METER_AUTOSELECT_RUNNING;
    taskEXIT_CRITICAL();
    return running;
}

void meter_autoselect_get_status(meter_autoselect_status_t *out)
{
    if (!out) return;

    taskENTER_CRITICAL();
    *out = autoselect_status;
    out->cancel_pending = autoselect_cancel_pending;
    taskEXIT_CRITICAL();
}

static void meter_autoselect_set_status(meter_autoselect_state_t state,
                                        uint8_t current_submode,
                                        uint8_t current_index,
                                        uint8_t candidate_count,
                                        uint8_t best_submode,
                                        uint8_t best_score,
                                        uint8_t last_score,
                                        uint32_t settle_ms,
                                        uint32_t wait_budget_ms)
{
    taskENTER_CRITICAL();
    autoselect_status.state = state;
    autoselect_status.current_submode = current_submode;
    autoselect_status.current_index = current_index;
    autoselect_status.candidate_count = candidate_count;
    autoselect_status.best_submode = best_submode;
    autoselect_status.best_score = best_score;
    autoselect_status.last_score = last_score;
    autoselect_status.settle_ms = settle_ms;
    autoselect_status.wait_budget_ms = wait_budget_ms;
    autoselect_status.cancel_pending = autoselect_cancel_pending;
    taskEXIT_CRITICAL();
}

static bool meter_autoselect_cancel_requested(void)
{
    bool cancel;

    taskENTER_CRITICAL();
    cancel = autoselect_cancel_pending;
    taskEXIT_CRITICAL();
    return cancel;
}

static bool meter_autoselect_mode_changed(void)
{
    return current_mode != MODE_MULTIMETER;
}

static void meter_autoselect_prepare_candidate(uint8_t submode)
{
    meter_submode = submode;
    meter_reset_minmaxavg();
    meter_voltage_wave_reset();
    /*
     * Follow the same transition path as the UI buttons. `fpga_meter_reinit()`
     * is a stronger debug recovery path with a DCV wake preamble; using it for
     * every autoscan candidate makes the physical relay path and producer frames
     * differ from normal user mode changes.
     */
    fpga_set_meter_mode(submode);
}

static uint8_t meter_autoselect_wait_score(uint8_t submode,
                                           uint32_t wait_budget_ms)
{
    uint8_t score = 0;

    for (uint32_t waited = 0; waited < wait_budget_ms;
         waited += METER_AUTOSELECT_POLL_MS) {
        meter_reading_t snap;

        if (meter_autoselect_cancel_requested()) break;
        if (meter_autoselect_mode_changed()) break;
        vTaskDelay(pdMS_TO_TICKS(METER_AUTOSELECT_POLL_MS));
        if (meter_data_snapshot(&snap)) {
            score = meter_auto_score(submode, &snap);
            if (score > 0) break;
        }
    }
    return score;
}

static void meter_autoselect_run_once(uint32_t settle_ms)
{
    size_t candidate_count_sz = 0;
    const uint8_t *candidates = meter_auto_candidates(&candidate_count_sz);
    uint8_t candidate_count = (uint8_t)candidate_count_sz;
    uint8_t original_mode = meter_submode;
    uint8_t best_mode = original_mode;
    uint8_t best_score = 0;
    uint32_t wait_budget_ms =
        (settle_ms < METER_AUTOSELECT_MIN_WAIT_MS) ?
        METER_AUTOSELECT_MIN_WAIT_MS : settle_ms;

    meter_layout = METER_LAYOUT_FULL;
    meter_autoselect_set_status(METER_AUTOSELECT_RUNNING, original_mode, 0,
                                candidate_count, best_mode, best_score, 0,
                                settle_ms, wait_budget_ms);

    for (uint8_t i = 0; i < candidate_count; i++) {
        uint8_t submode = candidates[i];
        uint8_t score;

        if (meter_autoselect_cancel_requested() || meter_autoselect_mode_changed()) {
            break;
        }

        meter_autoselect_prepare_candidate(submode);
        meter_autoselect_set_status(METER_AUTOSELECT_RUNNING, submode, i,
                                    candidate_count, best_mode, best_score, 0,
                                    settle_ms, wait_budget_ms);

        score = meter_autoselect_wait_score(submode, wait_budget_ms);
        if (score > best_score) {
            best_score = score;
            best_mode = submode;
        }
        meter_autoselect_set_status(METER_AUTOSELECT_RUNNING, submode, i,
                                    candidate_count, best_mode, best_score,
                                    score, settle_ms, wait_budget_ms);
    }

    if (meter_autoselect_cancel_requested() ||
        meter_autoselect_mode_changed()) {
        if (!meter_autoselect_mode_changed()) {
            best_mode = original_mode;
            best_score = 0;
            meter_autoselect_prepare_candidate(best_mode);
        }
        meter_autoselect_set_status(METER_AUTOSELECT_CANCELLED, best_mode,
                                    candidate_count, candidate_count,
                                    best_mode, best_score, 0,
                                    settle_ms, wait_budget_ms);
    } else {
        meter_autoselect_prepare_candidate(best_mode);
        meter_autoselect_set_status(METER_AUTOSELECT_DONE, best_mode,
                                    candidate_count, candidate_count,
                                    best_mode, best_score, best_score,
                                    settle_ms, wait_budget_ms);
    }
}

static void vMeterAutoselectTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        bool start;
        uint32_t settle_ms;

        taskENTER_CRITICAL();
        start = autoselect_start_pending;
        settle_ms = autoselect_requested_settle_ms;
        if (start) {
            autoselect_start_pending = false;
            autoselect_cancel_pending = false;
        }
        taskEXIT_CRITICAL();

        if (start) {
            meter_autoselect_run_once(settle_ms);
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void meter_autoselect_create_task(void)
{
#ifndef EMULATOR_BUILD
    xTaskCreate(vMeterAutoselectTask, "meter_auto", 512, NULL, 2, NULL);
#endif
}
