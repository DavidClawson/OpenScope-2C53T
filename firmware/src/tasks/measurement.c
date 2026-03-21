/*
 * measurement.c - Timeout-aware auto-measurement subsystem
 *
 * Prevents the auto-measurement hang (community bug #1) by enforcing
 * a configurable timeout on all measurement operations. Uses a simple
 * state machine with volatile state for safe cross-task cancellation.
 */

#include "measurement.h"
#include "FreeRTOS.h"
#include "task.h"

/* ═══════════════════════════════════════════════════════════════════
 * Internal state
 * ═══════════════════════════════════════════════════════════════════ */

static volatile measurement_state_t state = MEAS_STATE_IDLE;
static measurement_result_t         last_result;
static uint32_t                     timeout_ticks = 0;

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void measurement_init(void)
{
    state = MEAS_STATE_IDLE;
    last_result.valid = false;
    last_result.frequency_hz = 0.0f;
    last_result.vpp = 0.0f;
    last_result.vrms = 0.0f;
    last_result.vavg = 0.0f;
    last_result.duty_cycle = 0.0f;
    timeout_ticks = 0;
}

void measurement_auto_start(uint32_t timeout_ms)
{
    /* Reset result validity */
    last_result.valid = false;

    /* Record start time and deadline */
    TickType_t start_tick = xTaskGetTickCount();
    timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    /* Transition to MEASURING */
    state = MEAS_STATE_MEASURING;

    /*
     * Measurement loop — placeholder implementation.
     *
     * In the real firmware this will read ADC data from the FPGA,
     * compute frequency/Vpp/Vrms/Vavg/duty. For now we simulate
     * a measurement that takes a few iterations, checking the
     * timeout and cancel flag each iteration.
     */
    for (uint32_t iter = 0; iter < 100; iter++) {
        /* Check for cancellation (set by another task or ISR) */
        if (state == MEAS_STATE_CANCELLED) {
            return;
        }

        /* Check for timeout */
        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        if (elapsed > timeout_ticks) {
            state = MEAS_STATE_TIMEOUT;
            return;
        }

        /* Yield to other tasks between iterations */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Check cancel again after waking (may have been set while sleeping) */
        if (state == MEAS_STATE_CANCELLED) {
            return;
        }
    }

    /* Measurement complete — fill in placeholder results */
    last_result.frequency_hz = 1000.0f;   /* 1 kHz placeholder */
    last_result.vpp          = 3.3f;
    last_result.vrms         = 1.17f;     /* ~Vpp / (2 * sqrt(2)) */
    last_result.vavg         = 0.0f;
    last_result.duty_cycle   = 50.0f;
    last_result.valid        = true;

    state = MEAS_STATE_DONE;
}

void measurement_cancel(void)
{
    /*
     * Single volatile write — safe to call from any context
     * (ISR, another task, etc.) without synchronization.
     */
    state = MEAS_STATE_CANCELLED;
}

measurement_state_t measurement_get_state(void)
{
    return state;
}

const measurement_result_t *measurement_get_result(void)
{
    return &last_result;
}
