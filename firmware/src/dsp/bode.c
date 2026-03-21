/*
 * FNIRSI 2C53T Bode Plot Analysis
 *
 * Measures frequency response (gain and phase) by sweeping signal generator
 * output and comparing CH1 (reference) with CH2 (DUT response).
 */

#include "bode.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Static state for active Bode measurement */
static bode_config_t s_config;
static bode_result_t s_result;

/* ========================================================================
 * bode_init — Initialize Bode measurement state
 * ======================================================================== */
void bode_init(const bode_config_t *cfg)
{
    s_config = *cfg;

    memset(&s_result, 0, sizeof(s_result));
    s_result.num_points = cfg->num_points;
    if (s_result.num_points > BODE_MAX_POINTS) {
        s_result.num_points = BODE_MAX_POINTS;
    }
    s_result.current_step = 0;
    s_result.sweep_done = false;
    s_result.bandwidth_hz = 0.0f;
    s_result.peak_gain_db = -999.0f;
    s_result.peak_freq_hz = 0.0f;
}

/* ========================================================================
 * bode_step_frequency — Calculate the frequency for a given sweep step
 *
 * Log sweep:    freq = start * pow(stop/start, step/(num_points-1))
 * Linear sweep: freq = start + (stop-start) * step / (num_points-1)
 * ======================================================================== */
float bode_step_frequency(const bode_config_t *cfg, uint16_t step)
{
    if (cfg->num_points <= 1) {
        return cfg->start_freq_hz;
    }

    float t = (float)step / (float)(cfg->num_points - 1);

    if (cfg->log_sweep) {
        /* Logarithmic spacing */
        float ratio = cfg->stop_freq_hz / cfg->start_freq_hz;
        return cfg->start_freq_hz * powf(ratio, t);
    } else {
        /* Linear spacing */
        return cfg->start_freq_hz + (cfg->stop_freq_hz - cfg->start_freq_hz) * t;
    }
}

/* ========================================================================
 * bode_process_point — Compute gain and phase for one frequency point
 *
 * Gain:  20 * log10(rms_out / rms_in)
 * Phase: Quadrature demodulation using sin/cos correlation
 *   For each channel, compute:
 *     I = sum(signal * cos(2*pi*f*t))
 *     Q = sum(signal * sin(2*pi*f*t))
 *     phase = atan2(Q, I)
 *   Phase difference = phase_out - phase_in
 * ======================================================================== */
void bode_process_point(const int16_t *input_samples, const int16_t *output_samples,
                        uint16_t num_samples, float sample_rate, float freq_hz,
                        bode_point_t *point)
{
    float sum_in_sq = 0.0f;
    float sum_out_sq = 0.0f;

    /* Quadrature components for phase measurement */
    float in_i = 0.0f, in_q = 0.0f;
    float out_i = 0.0f, out_q = 0.0f;

    float omega = 2.0f * M_PI * freq_hz;

    for (uint16_t n = 0; n < num_samples; n++) {
        float t = (float)n / sample_rate;
        float cos_wt = cosf(omega * t);
        float sin_wt = sinf(omega * t);

        float in_val = (float)input_samples[n];
        float out_val = (float)output_samples[n];

        /* RMS accumulators */
        sum_in_sq += in_val * in_val;
        sum_out_sq += out_val * out_val;

        /* Quadrature correlation for phase */
        in_i += in_val * cos_wt;
        in_q += in_val * sin_wt;
        out_i += out_val * cos_wt;
        out_q += out_val * sin_wt;
    }

    /* Gain from RMS ratio */
    float rms_in = sqrtf(sum_in_sq / (float)num_samples);
    float rms_out = sqrtf(sum_out_sq / (float)num_samples);

    point->frequency_hz = freq_hz;

    if (rms_in > 0.0001f) {
        point->gain_db = 20.0f * log10f(rms_out / rms_in);
    } else {
        point->gain_db = -999.0f; /* No input signal */
    }

    /* Phase from quadrature demodulation */
    float phase_in  = atan2f(in_q, in_i);
    float phase_out = atan2f(out_q, out_i);
    float phase_diff = phase_out - phase_in;

    /* Wrap to [-180, +180] degrees */
    while (phase_diff > M_PI)  phase_diff -= 2.0f * M_PI;
    while (phase_diff < -M_PI) phase_diff += 2.0f * M_PI;

    point->phase_deg = phase_diff * (180.0f / M_PI);
}

/* ========================================================================
 * bode_find_bandwidth — Find -3dB bandwidth from sweep data
 *
 * 1. Find peak gain
 * 2. Scan from peak toward higher frequency
 * 3. First point where gain < peak - 3dB
 * 4. Interpolate between adjacent points
 * ======================================================================== */
float bode_find_bandwidth(const bode_result_t *result)
{
    if (result->num_points < 2) {
        return 0.0f;
    }

    /* Find peak gain and its index */
    float peak_gain = -999.0f;
    uint16_t peak_idx = 0;

    for (uint16_t i = 0; i < result->num_points; i++) {
        if (result->points[i].gain_db > peak_gain) {
            peak_gain = result->points[i].gain_db;
            peak_idx = i;
        }
    }

    float threshold = peak_gain - 3.0f;

    /* Scan from peak toward higher frequencies */
    for (uint16_t i = peak_idx + 1; i < result->num_points; i++) {
        if (result->points[i].gain_db < threshold) {
            /* Interpolate between points[i-1] and points[i] */
            float g1 = result->points[i - 1].gain_db;
            float g2 = result->points[i].gain_db;
            float f1 = result->points[i - 1].frequency_hz;
            float f2 = result->points[i].frequency_hz;

            if (fabsf(g2 - g1) < 0.0001f) {
                return f1;
            }

            /* Linear interpolation */
            float frac = (threshold - g1) / (g2 - g1);
            return f1 + frac * (f2 - f1);
        }
    }

    /* -3dB point not found within sweep range */
    return 0.0f;
}

/* ========================================================================
 * Rendering — Bode gain plot
 *
 * Draws gain (dB) vs frequency on the LCD. Uses lcd_set_pixel from the
 * LCD driver. On host test builds without LCD hardware, these are stubs.
 * ======================================================================== */

#ifndef TEST_BUILD

#include "../drivers/lcd.h"

/* Helper: map a value from [in_min, in_max] to [out_min, out_max] */
static int16_t map_range(float val, float in_min, float in_max,
                         int16_t out_min, int16_t out_max)
{
    if (in_max <= in_min) return out_min;
    float t = (val - in_min) / (in_max - in_min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return out_min + (int16_t)(t * (float)(out_max - out_min));
}

void bode_render_gain(const bode_result_t *result,
                      uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                      uint16_t color)
{
    if (result->num_points < 2) return;

    /* Find gain range for Y axis scaling */
    float g_min = result->points[0].gain_db;
    float g_max = result->points[0].gain_db;
    for (uint16_t i = 1; i < result->num_points; i++) {
        if (result->points[i].gain_db < g_min) g_min = result->points[i].gain_db;
        if (result->points[i].gain_db > g_max) g_max = result->points[i].gain_db;
    }

    /* Add some margin */
    float margin = (g_max - g_min) * 0.1f;
    if (margin < 1.0f) margin = 1.0f;
    g_min -= margin;
    g_max += margin;

    /* Plot points */
    for (uint16_t i = 0; i < result->num_points; i++) {
        uint16_t px = x_off + (uint16_t)((float)i / (float)(result->num_points - 1) * (float)(width - 1));
        /* Y axis: higher gain = higher on screen (lower Y coordinate) */
        int16_t py = map_range(result->points[i].gain_db, g_min, g_max,
                               (int16_t)(y_off + height - 1), (int16_t)y_off);
        if (px < x_off + width && py >= (int16_t)y_off && py < (int16_t)(y_off + height)) {
            lcd_set_pixel(px, (uint16_t)py, color);
        }

        /* Connect to previous point with simple line approximation */
        if (i > 0) {
            uint16_t px_prev = x_off + (uint16_t)((float)(i - 1) / (float)(result->num_points - 1) * (float)(width - 1));
            int16_t py_prev = map_range(result->points[i - 1].gain_db, g_min, g_max,
                                         (int16_t)(y_off + height - 1), (int16_t)y_off);
            /* Bresenham-style vertical fill between prev and current */
            int16_t y_start = py_prev < py ? py_prev : py;
            int16_t y_end   = py_prev < py ? py : py_prev;
            for (int16_t yy = y_start; yy <= y_end; yy++) {
                if (yy >= (int16_t)y_off && yy < (int16_t)(y_off + height)) {
                    lcd_set_pixel(px, (uint16_t)yy, color);
                }
            }
        }
    }
}

void bode_render_phase(const bode_result_t *result,
                       uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                       uint16_t color)
{
    if (result->num_points < 2) return;

    /* Phase range is always [-180, +180] */
    float p_min = -180.0f;
    float p_max =  180.0f;

    for (uint16_t i = 0; i < result->num_points; i++) {
        uint16_t px = x_off + (uint16_t)((float)i / (float)(result->num_points - 1) * (float)(width - 1));
        int16_t py = map_range(result->points[i].phase_deg, p_min, p_max,
                               (int16_t)(y_off + height - 1), (int16_t)y_off);
        if (px < x_off + width && py >= (int16_t)y_off && py < (int16_t)(y_off + height)) {
            lcd_set_pixel(px, (uint16_t)py, color);
        }

        if (i > 0) {
            uint16_t px_prev = x_off + (uint16_t)((float)(i - 1) / (float)(result->num_points - 1) * (float)(width - 1));
            int16_t py_prev = map_range(result->points[i - 1].phase_deg, p_min, p_max,
                                         (int16_t)(y_off + height - 1), (int16_t)y_off);
            int16_t y_start = py_prev < py ? py_prev : py;
            int16_t y_end   = py_prev < py ? py : py_prev;
            for (int16_t yy = y_start; yy <= y_end; yy++) {
                if (yy >= (int16_t)y_off && yy < (int16_t)(y_off + height)) {
                    lcd_set_pixel(px, (uint16_t)yy, color);
                }
            }
        }
    }
}

#else /* TEST_BUILD — stub rendering */

void bode_render_gain(const bode_result_t *result,
                      uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                      uint16_t color)
{
    (void)result; (void)x_off; (void)y_off; (void)width; (void)height; (void)color;
}

void bode_render_phase(const bode_result_t *result,
                       uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                       uint16_t color)
{
    (void)result; (void)x_off; (void)y_off; (void)width; (void)height; (void)color;
}

#endif /* TEST_BUILD */
