/*
 * FFT Engine - Phase 1: Custom Radix-2 DIT
 *
 * A correct, windowed FFT with dB output and peak detection.
 * Uses only float math (hardware FPU on Cortex-M4F).
 * No external dependencies — sinf/cosf/sqrtf/log10f from libm.
 *
 * Memory: ~14KB static (.bss), no heap allocation.
 */

#include "fft.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Minimum magnitude to avoid log10(0) */
#define FFT_FLOOR 1e-10f

/* Peak detection: must be this many dB above noise floor */
#define FFT_PEAK_THRESHOLD_DB (-60.0f)

/* ═══════════════════════════════════════════════════════════════════
 * Static buffers (14KB total in .bss)
 * ═══════════════════════════════════════════════════════════════════ */

/* Interleaved real/imaginary for in-place FFT */
static float fft_buf[FFT_SIZE * 2];          /* 8KB: complex pairs */
static float window_coeffs[FFT_SIZE];        /* 4KB: precomputed window */
static float twiddle_re[FFT_SIZE / 2];       /* 2KB: cos table */
static float twiddle_im[FFT_SIZE / 2];       /* 2KB: sin table */

/* Current configuration */
static fft_config_t current_cfg;
static bool initialized = false;

/* ═══════════════════════════════════════════════════════════════════
 * Window coefficient computation
 * ═══════════════════════════════════════════════════════════════════ */

static void compute_window(fft_window_t type)
{
    uint16_t i;
    float n = (float)FFT_SIZE;

    for (i = 0; i < FFT_SIZE; i++) {
        float x = (float)i / (n - 1.0f);
        switch (type) {
        case FFT_WINDOW_HANNING:
            window_coeffs[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * x));
            break;
        case FFT_WINDOW_HAMMING:
            window_coeffs[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * x);
            break;
        case FFT_WINDOW_RECTANGULAR:
        default:
            window_coeffs[i] = 1.0f;
            break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Twiddle factor precomputation
 * ═══════════════════════════════════════════════════════════════════ */

static void compute_twiddles(void)
{
    uint16_t i;
    for (i = 0; i < FFT_SIZE / 2; i++) {
        float angle = -2.0f * M_PI * (float)i / (float)FFT_SIZE;
        twiddle_re[i] = cosf(angle);
        twiddle_im[i] = sinf(angle);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Bit-reversal permutation
 * ═══════════════════════════════════════════════════════════════════ */

static uint16_t bit_reverse(uint16_t x, uint16_t log2n)
{
    uint16_t result = 0;
    uint16_t i;
    for (i = 0; i < log2n; i++) {
        result = (uint16_t)((result << 1) | (x & 1));
        x >>= 1;
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 * Radix-2 Decimation-in-Time FFT (in-place)
 *
 * Input/output: fft_buf[] as interleaved [re0, im0, re1, im1, ...]
 * ═══════════════════════════════════════════════════════════════════ */

static void radix2_fft(void)
{
    uint16_t n = FFT_SIZE;
    uint16_t log2n = 0;
    uint16_t temp = n;

    while (temp > 1) {
        log2n++;
        temp >>= 1;
    }

    /* Bit-reversal permutation */
    uint16_t i;
    for (i = 0; i < n; i++) {
        uint16_t j = bit_reverse(i, log2n);
        if (j > i) {
            /* Swap complex pairs */
            float tmp_re = fft_buf[i * 2];
            float tmp_im = fft_buf[i * 2 + 1];
            fft_buf[i * 2]     = fft_buf[j * 2];
            fft_buf[i * 2 + 1] = fft_buf[j * 2 + 1];
            fft_buf[j * 2]     = tmp_re;
            fft_buf[j * 2 + 1] = tmp_im;
        }
    }

    /* Butterfly stages */
    uint16_t stage;
    for (stage = 1; stage <= log2n; stage++) {
        uint16_t half_size = (uint16_t)(1 << (stage - 1));
        uint16_t full_size = (uint16_t)(1 << stage);
        uint16_t twiddle_step = n >> stage;

        uint16_t group;
        for (group = 0; group < n; group += full_size) {
            uint16_t k;
            for (k = 0; k < half_size; k++) {
                uint16_t idx_top = group + k;
                uint16_t idx_bot = group + k + half_size;
                uint16_t tw_idx = k * twiddle_step;

                float tw_re = twiddle_re[tw_idx];
                float tw_im = twiddle_im[tw_idx];

                float bot_re = fft_buf[idx_bot * 2];
                float bot_im = fft_buf[idx_bot * 2 + 1];

                /* Complex multiply: twiddle * bottom */
                float prod_re = bot_re * tw_re - bot_im * tw_im;
                float prod_im = bot_re * tw_im + bot_im * tw_re;

                /* Butterfly */
                fft_buf[idx_bot * 2]     = fft_buf[idx_top * 2]     - prod_re;
                fft_buf[idx_bot * 2 + 1] = fft_buf[idx_top * 2 + 1] - prod_im;
                fft_buf[idx_top * 2]     += prod_re;
                fft_buf[idx_top * 2 + 1] += prod_im;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Peak detection
 * ═══════════════════════════════════════════════════════════════════ */

static void find_peaks(const float *mag_db, uint16_t num_bins,
                       float bin_width_hz, fft_peak_t *peaks,
                       uint8_t max_peaks, uint8_t *num_found)
{
    *num_found = 0;

    /* Find the global maximum first to set threshold */
    float global_max = -200.0f;
    uint16_t i;
    for (i = 1; i < num_bins; i++) {
        if (mag_db[i] > global_max)
            global_max = mag_db[i];
    }

    float threshold = global_max + FFT_PEAK_THRESHOLD_DB;

    /* Scan all bins for local maxima above threshold */
    for (i = 2; i < num_bins - 1; i++) {
        if (mag_db[i] > mag_db[i - 1] &&
            mag_db[i] > mag_db[i + 1] &&
            mag_db[i] > threshold) {

            /* If array not full, just insert at end */
            if (*num_found < max_peaks) {
                uint8_t pos = *num_found;
                /* Insert sorted by magnitude (descending) */
                while (pos > 0 && mag_db[i] > peaks[pos - 1].magnitude_db) {
                    peaks[pos] = peaks[pos - 1];
                    pos--;
                }
                peaks[pos].bin = i;
                peaks[pos].freq_hz = (float)i * bin_width_hz;
                peaks[pos].magnitude_db = mag_db[i];
                (*num_found)++;
            } else if (mag_db[i] > peaks[max_peaks - 1].magnitude_db) {
                /* Replace the weakest peak if this one is stronger */
                uint8_t pos = max_peaks - 1;
                while (pos > 0 && mag_db[i] > peaks[pos - 1].magnitude_db) {
                    peaks[pos] = peaks[pos - 1];
                    pos--;
                }
                peaks[pos].bin = i;
                peaks[pos].freq_hz = (float)i * bin_width_hz;
                peaks[pos].magnitude_db = mag_db[i];
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void fft_init(const fft_config_t *cfg)
{
    current_cfg = *cfg;
    compute_window(cfg->window);
    compute_twiddles();
    initialized = true;
}

void fft_process(const int16_t *samples, uint16_t num_samples,
                 fft_result_t *result)
{
    if (!initialized)
        return;

    uint16_t i;
    uint16_t count = (num_samples < FFT_SIZE) ? num_samples : FFT_SIZE;

    /* Step 1: Convert int16 to float, apply window, load into complex buffer */
    for (i = 0; i < count; i++) {
        fft_buf[i * 2]     = (float)samples[i] * window_coeffs[i];
        fft_buf[i * 2 + 1] = 0.0f;  /* Imaginary = 0 for real input */
    }

    /* Zero-pad if input shorter than FFT size */
    for (i = count; i < FFT_SIZE; i++) {
        fft_buf[i * 2]     = 0.0f;
        fft_buf[i * 2 + 1] = 0.0f;
    }

    /* Step 2: Run FFT */
    radix2_fft();

    /* Step 3: Compute magnitude in dB for positive frequencies */
    float scale = 2.0f / (float)FFT_SIZE;
    result->num_bins = FFT_BINS;
    result->bin_width_hz = current_cfg.sample_rate_hz / (float)FFT_SIZE;

    for (i = 0; i < FFT_BINS; i++) {
        float re = fft_buf[i * 2];
        float im = fft_buf[i * 2 + 1];
        float mag = sqrtf(re * re + im * im) * scale;

        if (mag < FFT_FLOOR)
            mag = FFT_FLOOR;

        result->magnitude_db[i] = 20.0f * log10f(mag);
    }

    /* DC bin doesn't get the 2x factor */
    {
        float re = fft_buf[0];
        float im = fft_buf[1];
        float mag = sqrtf(re * re + im * im) / (float)FFT_SIZE;
        if (mag < FFT_FLOOR) mag = FFT_FLOOR;
        result->magnitude_db[0] = 20.0f * log10f(mag);
    }

    /* Step 4: Find peaks */
    result->num_peaks = 0;
    result->peak_freq_hz = 0.0f;
    result->peak_mag_db = -200.0f;

    if (current_cfg.peak_count > 0) {
        find_peaks(result->magnitude_db, FFT_BINS,
                   result->bin_width_hz, result->peaks,
                   current_cfg.peak_count, &result->num_peaks);

        if (result->num_peaks > 0) {
            result->peak_freq_hz = result->peaks[0].freq_hz;
            result->peak_mag_db  = result->peaks[0].magnitude_db;
        }
    }
}

const fft_config_t *fft_get_config(void)
{
    return &current_cfg;
}

void fft_set_window(fft_window_t window)
{
    current_cfg.window = window;
    compute_window(window);
}

fft_window_t fft_cycle_window(void)
{
    fft_window_t next = (fft_window_t)((current_cfg.window + 1) % FFT_WINDOW_COUNT);
    fft_set_window(next);
    return next;
}
