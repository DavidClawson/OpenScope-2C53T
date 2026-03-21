/*
 * Measurement Engine - Auto-measurements for oscilloscope channels
 *
 * Implements: frequency, period, Vpp, Vrms, Vavg, Vmin, Vmax,
 *             duty cycle, rise time, fall time.
 *
 * Uses a timeout state machine for integration with the RTOS task loop.
 */

#include "measurement.h"
#include <math.h>
#include <string.h>

/* ADC voltage scale: 3.3V reference, full-scale = 32768 counts */
#define VOLTAGE_SCALE (3.3f / 32768.0f)

/* ===================================================================
 * Timeout State Machine
 * =================================================================== */

void measurement_init(measurement_context_t *ctx, uint32_t timeout_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state      = MEAS_IDLE;
    ctx->timeout_ms = timeout_ms;
    ctx->elapsed_ms = 0;
}

void measurement_tick(measurement_context_t *ctx, uint32_t delta_ms)
{
    switch (ctx->state) {
    case MEAS_IDLE:
        /* Waiting for acquisition trigger */
        break;

    case MEAS_ACQUIRING:
        ctx->elapsed_ms += delta_ms;
        if (ctx->elapsed_ms >= ctx->timeout_ms) {
            ctx->state = MEAS_TIMEOUT;
            ctx->result.valid = false;
        }
        break;

    case MEAS_COMPUTING:
        /* Computation is synchronous, so this state is transient */
        break;

    case MEAS_DONE:
        /* Results available */
        break;

    case MEAS_TIMEOUT:
        /* Timed out waiting for data */
        break;
    }
}

/* ===================================================================
 * Measurement Algorithms
 * =================================================================== */

void measurement_compute(const int16_t *samples, uint16_t num_samples,
                         float sample_rate, measurement_result_t *result)
{
    if (!samples || num_samples < 2 || sample_rate <= 0.0f) {
        memset(result, 0, sizeof(*result));
        result->valid = false;
        return;
    }

    /* --- Min / Max --- */
    int16_t smin = samples[0];
    int16_t smax = samples[0];
    int64_t sum = 0;
    int64_t sum_sq = 0;

    for (uint16_t i = 0; i < num_samples; i++) {
        int16_t s = samples[i];
        if (s < smin) smin = s;
        if (s > smax) smax = s;
        sum += s;
        sum_sq += (int64_t)s * s;
    }

    result->vmin = smin * VOLTAGE_SCALE;
    result->vmax = smax * VOLTAGE_SCALE;
    result->vpp  = (smax - smin) * VOLTAGE_SCALE;
    result->vavg = ((float)sum / num_samples) * VOLTAGE_SCALE;
    result->vrms = sqrtf((float)sum_sq / num_samples) * VOLTAGE_SCALE;

    /* --- Frequency (zero-crossing method) --- */
    uint16_t crossings = 0;
    for (uint16_t i = 1; i < num_samples; i++) {
        /* Count positive-going zero crossings */
        if (samples[i - 1] < 0 && samples[i] >= 0) {
            crossings++;
        }
    }

    if (crossings >= 1) {
        /*
         * Each positive-going zero crossing represents one full cycle.
         * But we need at least 2 crossings to measure a period between them.
         * With N crossings we have (N) complete half-periods... actually:
         *
         * frequency = crossings / (num_samples / sample_rate)
         *           = crossings * sample_rate / num_samples
         *
         * But each crossing is one full cycle (positive-going only),
         * so this directly gives the frequency.
         */
        result->frequency_hz = (float)crossings * sample_rate / (float)num_samples;
        result->period_s = 1.0f / result->frequency_hz;
    } else {
        result->frequency_hz = 0.0f;
        result->period_s = 0.0f;
    }

    /* --- Duty Cycle --- */
    int16_t threshold = (int16_t)(((int32_t)smin + (int32_t)smax) / 2);
    uint32_t above_count = 0;
    for (uint16_t i = 0; i < num_samples; i++) {
        if (samples[i] > threshold) {
            above_count++;
        }
    }
    result->duty_cycle = (float)above_count / (float)num_samples * 100.0f;

    /* --- Rise Time (10% to 90%) --- */
    float amplitude = (float)(smax - smin);
    float level_10 = smin + 0.1f * amplitude;
    float level_90 = smin + 0.9f * amplitude;

    float rise_start = -1.0f;
    float rise_end   = -1.0f;
    float fall_start = -1.0f;
    float fall_end   = -1.0f;

    /* Find first positive-going 10% crossing, then next 90% crossing */
    for (uint16_t i = 1; i < num_samples; i++) {
        /* Rising edge: crossing 10% level going up */
        if (rise_start < 0.0f &&
            (float)samples[i - 1] < level_10 && (float)samples[i] >= level_10) {
            /* Linear interpolation for sub-sample precision */
            float frac = (level_10 - samples[i - 1]) /
                         (float)(samples[i] - samples[i - 1]);
            rise_start = (i - 1) + frac;
        }
        /* Rising edge: crossing 90% level going up (after 10% found) */
        if (rise_start >= 0.0f && rise_end < 0.0f &&
            (float)samples[i - 1] < level_90 && (float)samples[i] >= level_90) {
            float frac = (level_90 - samples[i - 1]) /
                         (float)(samples[i] - samples[i - 1]);
            rise_end = (i - 1) + frac;
        }

        /* Falling edge: crossing 90% level going down */
        if (fall_start < 0.0f &&
            (float)samples[i - 1] > level_90 && (float)samples[i] <= level_90) {
            float frac = (level_90 - (float)samples[i]) /
                         (float)(samples[i - 1] - samples[i]);
            fall_start = (i - 1) + frac;
        }
        /* Falling edge: crossing 10% level going down (after 90% found) */
        if (fall_start >= 0.0f && fall_end < 0.0f &&
            (float)samples[i - 1] > level_10 && (float)samples[i] <= level_10) {
            float frac = (level_10 - (float)samples[i]) /
                         (float)(samples[i - 1] - samples[i]);
            fall_end = (i - 1) + frac;
        }
    }

    if (rise_start >= 0.0f && rise_end >= 0.0f) {
        result->rise_time_s = (rise_end - rise_start) / sample_rate;
    } else {
        result->rise_time_s = 0.0f;
    }

    if (fall_start >= 0.0f && fall_end >= 0.0f) {
        result->fall_time_s = (fall_end - fall_start) / sample_rate;
    } else {
        result->fall_time_s = 0.0f;
    }

    result->valid = true;
}
