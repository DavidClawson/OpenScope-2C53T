#ifndef MEASUREMENT_H
#define MEASUREMENT_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MEAS_STATE_IDLE = 0,
    MEAS_STATE_MEASURING,
    MEAS_STATE_DONE,
    MEAS_STATE_TIMEOUT,
    MEAS_STATE_CANCELLED
} measurement_state_t;

typedef struct {
    float frequency_hz;
    float vpp;
    float vrms;
    float vavg;
    float duty_cycle;
    bool  valid;
} measurement_result_t;

/* Start auto-measurement with timeout in ms. Returns immediately. */
void measurement_auto_start(uint32_t timeout_ms);

/* Cancel ongoing measurement. Safe to call from ISR or other task. */
void measurement_cancel(void);

/* Get current state */
measurement_state_t measurement_get_state(void);

/* Get latest result (only valid when state == MEAS_STATE_DONE) */
const measurement_result_t *measurement_get_result(void);

/* Initialize measurement subsystem. Call once from main(). */
void measurement_init(void);

#endif
