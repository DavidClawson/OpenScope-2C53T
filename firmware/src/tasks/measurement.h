#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>
#include <stdbool.h>

/* Auto-measurement results for a single channel */
typedef struct {
    float frequency_hz;
    float period_s;
    float vpp;
    float vrms;
    float vavg;
    float vmin;
    float vmax;
    float duty_cycle;
    float rise_time_s;
    float fall_time_s;
    bool  valid;
} measurement_result_t;

/* Measurement state for the timeout state machine */
typedef enum {
    MEAS_IDLE = 0,
    MEAS_ACQUIRING,
    MEAS_COMPUTING,
    MEAS_DONE,
    MEAS_TIMEOUT
} measurement_state_t;

typedef struct {
    measurement_state_t state;
    uint32_t            timeout_ms;
    uint32_t            elapsed_ms;
    measurement_result_t result;
} measurement_context_t;

/* Initialize the measurement context */
void measurement_init(measurement_context_t *ctx, uint32_t timeout_ms);

/* Tick the state machine (call periodically, e.g., every 10ms) */
void measurement_tick(measurement_context_t *ctx, uint32_t delta_ms);

/*
 * Compute all measurements from a sample buffer.
 *
 * samples      - signed 16-bit ADC data
 * num_samples  - number of samples in the buffer
 * sample_rate  - sampling rate in Hz (e.g. 250000000.0 for 250 MS/s)
 * result       - output structure filled with computed values
 */
void measurement_compute(const int16_t *samples, uint16_t num_samples,
                         float sample_rate, measurement_result_t *result);

#endif
