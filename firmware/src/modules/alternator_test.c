/*
 * Alternator Ripple Analysis — Automotive Diagnostic Module
 *
 * Analyzes AC ripple on the battery voltage to diagnose alternator health.
 * A healthy 3-phase alternator produces a small (~50mV) ripple at 3x the
 * rotational frequency. A failed diode removes one phase, increasing ripple
 * and shifting the dominant frequency to 1x or 2x RPM.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "alternator_test.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Simple in-place real FFT (radix-2 DIT) for power-of-2 lengths.
 * This is a minimal implementation for embedded use — not the fastest,
 * but small and self-contained. Operates on float arrays. */

static uint32_t next_power_of_2(uint32_t n)
{
    uint32_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

/*
 * Compute magnitude spectrum of real-valued input.
 * re[] is input (length n, must be power of 2). Overwritten in place.
 * im[] is workspace of same length (zeroed by this function).
 * mag_out[] receives n/2 magnitude bins.
 */
static void simple_fft_magnitude(float *re, float *im, uint32_t n,
                                  float *mag_out)
{
    /* Zero imaginary part */
    memset(im, 0, n * sizeof(float));

    /* Bit-reversal permutation */
    uint32_t j = 0;
    for (uint32_t i = 0; i < n - 1; i++) {
        if (i < j) {
            float tmp;
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
        }
        uint32_t k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    /* FFT butterfly stages */
    for (uint32_t stage = 1; stage < n; stage <<= 1) {
        float angle = -(float)M_PI / (float)stage;
        float wr = cosf(angle);
        float wi = sinf(angle);
        for (uint32_t grp = 0; grp < n; grp += stage << 1) {
            float tr = 1.0f, ti = 0.0f;
            for (uint32_t pair = 0; pair < stage; pair++) {
                uint32_t a = grp + pair;
                uint32_t b = a + stage;
                float br = tr * re[b] - ti * im[b];
                float bi = tr * im[b] + ti * re[b];
                re[b] = re[a] - br;
                im[b] = im[a] - bi;
                re[a] += br;
                im[a] += bi;
                float tnr = tr * wr - ti * wi;
                ti = tr * wi + ti * wr;
                tr = tnr;
            }
        }
    }

    /* Compute magnitude for bins 0..n/2-1 */
    for (uint32_t i = 0; i < n / 2; i++) {
        mag_out[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }
}

/* Thresholds */
#define RIPPLE_GOOD_MAX_MV    100.0f   /* Below this = good */
#define RIPPLE_DIODE_MIN_MV   300.0f   /* Above this = diode fault likely */
#define DC_CHARGING_MIN_V     13.0f    /* Must be above this to be charging */
#define DC_CHARGING_GOOD_V    13.5f    /* Healthy charging voltage */
#define DC_BATTERY_NOM_V      12.6f    /* Nominal battery voltage */
#define RIPPLE_PRESENT_MV     5.0f     /* Below this = no ripple */

/* Maximum FFT size we support on this embedded target */
#define MAX_FFT_SIZE  2048

void alternator_analyze(const int16_t *samples, uint32_t num_samples,
                        float sample_rate, float voltage_scale,
                        alternator_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));

    if (!samples || num_samples < 16 || sample_rate <= 0.0f ||
        voltage_scale <= 0.0f) {
        result->status = ALT_NOT_CHARGING;
        snprintf(result->diagnosis, sizeof(result->diagnosis), "No data");
        return;
    }

    /* --- Step 1: DC average --- */
    float dc_sum = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        dc_sum += (float)samples[i];
    }
    float dc_avg_raw = dc_sum / (float)num_samples;
    result->dc_voltage = dc_avg_raw * voltage_scale;

    /* --- Step 2: AC component and ripple Vpp --- */
    float ac_min = 1e9f, ac_max = -1e9f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float ac = ((float)samples[i] - dc_avg_raw) * voltage_scale;
        if (ac < ac_min) ac_min = ac;
        if (ac > ac_max) ac_max = ac;
    }
    float ripple_v = ac_max - ac_min;
    result->ripple_mv = ripple_v * 1000.0f;

    /* --- Step 3: FFT to find dominant ripple frequency --- */
    uint32_t fft_len = next_power_of_2(num_samples);
    if (fft_len > MAX_FFT_SIZE) fft_len = MAX_FFT_SIZE;

    /* Use stack buffers — these are small at 2048 max (24 KB total) */
    float re[MAX_FFT_SIZE];
    float im[MAX_FFT_SIZE];
    float mag[MAX_FFT_SIZE / 2];

    uint32_t copy_len = (num_samples < fft_len) ? num_samples : fft_len;
    for (uint32_t i = 0; i < copy_len; i++) {
        re[i] = ((float)samples[i] - dc_avg_raw);  /* AC-coupled */
    }
    /* Zero-pad remainder */
    for (uint32_t i = copy_len; i < fft_len; i++) {
        re[i] = 0.0f;
    }

    simple_fft_magnitude(re, im, fft_len, mag);

    /* Find peak frequency bin (skip bin 0 = DC) */
    float bin_resolution = sample_rate / (float)fft_len;
    float peak_mag = 0.0f;
    uint32_t peak_bin = 1;
    for (uint32_t i = 1; i < fft_len / 2; i++) {
        if (mag[i] > peak_mag) {
            peak_mag = mag[i];
            peak_bin = i;
        }
    }
    result->frequency_hz = (float)peak_bin * bin_resolution;

    /* --- Step 4: Harmonic analysis for diode count estimation --- */
    /* For a 3-phase full-wave rectifier with all 6 diodes:
     *   dominant ripple at 3x rotor frequency (6 pulses per rev,
     *   but the fundamental ripple component is at 3x).
     * With a missing diode: strong component at 1x rotor freq.
     *
     * Estimate rotor frequency: peak_freq / 3 for healthy,
     * or peak_freq itself if diode is bad. We check the ratio of
     * the 1x harmonic to the 3x harmonic. */

    /* Try to find the 1x and 3x relationship */
    float mag_at_1x = 0.0f;
    float mag_at_3x = 0.0f;

    /* Assume peak_bin could be either 1x or 3x */
    if (peak_bin >= 3 && (peak_bin % 3 == 0)) {
        /* peak_bin might be 3x rotor freq */
        uint32_t bin_1x = peak_bin / 3;
        mag_at_3x = mag[peak_bin];
        mag_at_1x = mag[bin_1x];
    } else {
        /* peak_bin might be 1x — look for 3x */
        mag_at_1x = mag[peak_bin];
        uint32_t bin_3x = peak_bin * 3;
        if (bin_3x < fft_len / 2) {
            mag_at_3x = mag[bin_3x];
        }
    }

    /* Diode estimation based on harmonic ratio */
    float ratio_1x_3x = (mag_at_3x > 0.001f) ? (mag_at_1x / mag_at_3x) : 999.0f;

    if (result->ripple_mv < RIPPLE_PRESENT_MV && result->dc_voltage < DC_CHARGING_MIN_V) {
        result->diodes_working = 0;  /* No ripple and not charging = not connected */
    } else if (result->ripple_mv < RIPPLE_PRESENT_MV && result->dc_voltage >= DC_CHARGING_GOOD_V) {
        result->diodes_working = 6;  /* Charging well, negligible ripple = very good */
    } else if (result->ripple_mv <= RIPPLE_GOOD_MAX_MV && ratio_1x_3x < 1.0f) {
        result->diodes_working = 6;  /* Low ripple with reasonable harmonics = good */
    } else if (ratio_1x_3x < 0.3f) {
        result->diodes_working = 6;  /* Strong 3x, weak 1x = all diodes good */
    } else if (ratio_1x_3x < 0.7f) {
        result->diodes_working = 5;  /* Some 1x leaking through */
    } else {
        result->diodes_working = 4;  /* Strong 1x = missing phase(s) */
    }

    /* --- Step 5: Overall diagnosis --- */
    result->is_charging = (result->dc_voltage > DC_CHARGING_MIN_V);

    if (result->dc_voltage < DC_CHARGING_MIN_V &&
        result->ripple_mv < RIPPLE_PRESENT_MV) {
        result->status = ALT_NOT_CHARGING;
        result->diodes_working = 0;
        snprintf(result->diagnosis, sizeof(result->diagnosis),
                 "No charge %.1fV", result->dc_voltage);
    } else if (result->dc_voltage < DC_CHARGING_MIN_V &&
               result->ripple_mv >= RIPPLE_PRESENT_MV) {
        result->status = ALT_WEAK_OUTPUT;
        snprintf(result->diagnosis, sizeof(result->diagnosis),
                 "Weak %.1fV %umV", result->dc_voltage,
                 (unsigned)(result->ripple_mv + 0.5f));
    } else if (result->ripple_mv >= RIPPLE_DIODE_MIN_MV ||
               (result->diodes_working <= 4 &&
                result->ripple_mv > RIPPLE_GOOD_MAX_MV)) {
        result->status = ALT_DIODE_FAULT;
        snprintf(result->diagnosis, sizeof(result->diagnosis),
                 "Bad diode %umV", (unsigned)(result->ripple_mv + 0.5f));
    } else if (result->ripple_mv <= RIPPLE_GOOD_MAX_MV &&
               result->dc_voltage >= DC_CHARGING_GOOD_V) {
        result->status = ALT_GOOD;
        snprintf(result->diagnosis, sizeof(result->diagnosis),
                 "Good %.1fV %umV", result->dc_voltage,
                 (unsigned)(result->ripple_mv + 0.5f));
    } else {
        /* Marginal — between good and diode fault thresholds */
        result->status = ALT_GOOD;
        snprintf(result->diagnosis, sizeof(result->diagnosis),
                 "OK %.1fV %umV", result->dc_voltage,
                 (unsigned)(result->ripple_mv + 0.5f));
    }
}
