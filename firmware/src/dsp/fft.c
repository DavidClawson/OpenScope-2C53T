/*
 * FFT Engine - Phase 2: 4096-point with averaging and max hold
 *
 * A correct, windowed FFT with dB output, peak detection,
 * exponential moving average, and max hold envelope.
 * Uses only float math (hardware FPU on Cortex-M4F).
 * When USE_CMSIS_DSP is defined, uses ARM's arm_rfft_fast_f32().
 * Otherwise falls back to custom radix-2 DIT. No other dependencies.
 *
 * Memory: ~88KB static (.bss), no heap allocation.
 */

#include "fft.h"
#include <math.h>
#include <string.h>

#ifdef USE_CMSIS_DSP
#include "arm_math.h"
static arm_rfft_fast_instance_f32 fft_instance;
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Minimum magnitude to avoid log10(0) */
#define FFT_FLOOR 1e-10f

/* Peak detection: must be this many dB above noise floor */
#define FFT_PEAK_THRESHOLD_DB (-60.0f)

/* ═══════════════════════════════════════════════════════════════════
 * Static buffers (~88KB total in .bss)
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef USE_CMSIS_DSP
/* CMSIS-DSP: real input + complex output */
static float fft_input[FFT_SIZE];            /* 16KB */
static float fft_cmsis_output[FFT_SIZE];     /* 16KB */
#else
/* Custom radix-2: interleaved complex buffer + twiddle tables */
static float fft_buf[FFT_SIZE * 2];          /* 32KB */
static float twiddle_re[FFT_SIZE / 2];       /*  8KB */
static float twiddle_im[FFT_SIZE / 2];       /*  8KB */
#endif

static float window_coeffs[FFT_SIZE];        /* 16KB */

/* Output buffers (pointed to by fft_result_t) */
static float magnitude_buf[FFT_BINS];        /*  8KB */
static float avg_buf[FFT_BINS];              /*  8KB */
static float max_hold_buf[FFT_BINS];         /*  8KB */

/* Current configuration */
static fft_config_t current_cfg;
static bool initialized = false;
static bool avg_primed = false;              /* Has avg_buf been initialized? */

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
        case FFT_WINDOW_BLACKMAN_HARRIS:
            window_coeffs[i] = 0.35875f
                - 0.48829f * cosf(2.0f * M_PI * x)
                + 0.14128f * cosf(4.0f * M_PI * x)
                - 0.01168f * cosf(6.0f * M_PI * x);
            break;
        case FFT_WINDOW_FLAT_TOP:
            window_coeffs[i] = 0.21557895f
                - 0.41663158f * cosf(2.0f * M_PI * x)
                + 0.277263158f * cosf(4.0f * M_PI * x)
                - 0.083578947f * cosf(6.0f * M_PI * x)
                + 0.006947368f * cosf(8.0f * M_PI * x);
            break;
        case FFT_WINDOW_RECTANGULAR:
        default:
            window_coeffs[i] = 1.0f;
            break;
        }
    }
}

#ifndef USE_CMSIS_DSP
/* ═══════════════════════════════════════════════════════════════════
 * Twiddle factor precomputation (radix-2 fallback only)
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

                float prod_re = bot_re * tw_re - bot_im * tw_im;
                float prod_im = bot_re * tw_im + bot_im * tw_re;

                fft_buf[idx_bot * 2]     = fft_buf[idx_top * 2]     - prod_re;
                fft_buf[idx_bot * 2 + 1] = fft_buf[idx_top * 2 + 1] - prod_im;
                fft_buf[idx_top * 2]     += prod_re;
                fft_buf[idx_top * 2 + 1] += prod_im;
            }
        }
    }
}
#endif /* !USE_CMSIS_DSP */

/* ═══════════════════════════════════════════════════════════════════
 * Peak detection
 * ═══════════════════════════════════════════════════════════════════ */

static void find_peaks(const float *mag_db, uint16_t num_bins,
                       float bin_width_hz, fft_peak_t *peaks,
                       uint8_t max_peaks, uint8_t *num_found)
{
    *num_found = 0;

    float global_max = -200.0f;
    uint16_t i;
    for (i = 1; i < num_bins; i++) {
        if (mag_db[i] > global_max)
            global_max = mag_db[i];
    }

    float threshold = global_max + FFT_PEAK_THRESHOLD_DB;

    for (i = 2; i < num_bins - 1; i++) {
        if (mag_db[i] > mag_db[i - 1] &&
            mag_db[i] > mag_db[i + 1] &&
            mag_db[i] > threshold) {

            if (*num_found < max_peaks) {
                uint8_t pos = *num_found;
                while (pos > 0 && mag_db[i] > peaks[pos - 1].magnitude_db) {
                    peaks[pos] = peaks[pos - 1];
                    pos--;
                }
                peaks[pos].bin = i;
                peaks[pos].freq_hz = (float)i * bin_width_hz;
                peaks[pos].magnitude_db = mag_db[i];
                peaks[pos].label[0] = '\0';
                (*num_found)++;
            } else if (mag_db[i] > peaks[max_peaks - 1].magnitude_db) {
                uint8_t pos = max_peaks - 1;
                while (pos > 0 && mag_db[i] > peaks[pos - 1].magnitude_db) {
                    peaks[pos] = peaks[pos - 1];
                    pos--;
                }
                peaks[pos].bin = i;
                peaks[pos].freq_hz = (float)i * bin_width_hz;
                peaks[pos].magnitude_db = mag_db[i];
                peaks[pos].label[0] = '\0';
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Harmonic labeling
 * ═══════════════════════════════════════════════════════════════════ */

static void label_harmonics(fft_peak_t *peaks, uint8_t num_peaks)
{
    if (num_peaks == 0) return;

    /* Find the strongest peak */
    uint8_t strongest_idx = 0;
    float strongest_mag = peaks[0].magnitude_db;
    uint8_t i;
    for (i = 1; i < num_peaks; i++) {
        if (peaks[i].magnitude_db > strongest_mag) {
            strongest_mag = peaks[i].magnitude_db;
            strongest_idx = i;
        }
    }

    /* Find fundamental: lowest-frequency peak within 20dB of strongest */
    uint8_t fund_idx = strongest_idx;
    float fund_freq = peaks[strongest_idx].freq_hz;
    float mag_threshold = strongest_mag - 20.0f;

    for (i = 0; i < num_peaks; i++) {
        if (peaks[i].magnitude_db >= mag_threshold &&
            peaks[i].freq_hz < fund_freq) {
            fund_freq = peaks[i].freq_hz;
            fund_idx = i;
        }
    }

    /* Label the fundamental */
    peaks[fund_idx].label[0] = 'F';
    peaks[fund_idx].label[1] = 'u';
    peaks[fund_idx].label[2] = 'n';
    peaks[fund_idx].label[3] = 'd';
    peaks[fund_idx].label[4] = '\0';

    if (fund_freq <= 0.0f) return;
    float fund_bin = (float)peaks[fund_idx].bin;

    /* Check each other peak for harmonic relationship */
    for (i = 0; i < num_peaks; i++) {
        if (i == fund_idx) continue;

        float ratio = (float)peaks[i].bin / fund_bin;
        int n = (int)(ratio + 0.5f);

        if (n >= 2 && n <= 9) {
            float expected_bin = fund_bin * (float)n;
            float diff = (float)peaks[i].bin - expected_bin;
            if (diff < 0.0f) diff = -diff;

            if (diff <= 1.5f) {
                peaks[i].label[0] = 'H';
                peaks[i].label[1] = (char)('0' + n);
                peaks[i].label[2] = '\0';
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

    /* Set defaults for zoom if not specified */
    if (current_cfg.zoom_start_bin == 0)
        current_cfg.zoom_start_bin = 1;
    if (current_cfg.zoom_end_bin == 0)
        current_cfg.zoom_end_bin = FFT_BINS - 1;

    compute_window(cfg->window);
#ifdef USE_CMSIS_DSP
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
#else
    compute_twiddles();
#endif

    /* Initialize averaging and max hold buffers */
    uint16_t i;
    for (i = 0; i < FFT_BINS; i++) {
        avg_buf[i] = -200.0f;
        max_hold_buf[i] = -200.0f;
    }
    avg_primed = false;
    initialized = true;
}

void fft_process(const int16_t *samples, uint16_t num_samples,
                 fft_result_t *result)
{
    if (!initialized)
        return;

    uint16_t i;
    uint16_t count = (num_samples < FFT_SIZE) ? num_samples : FFT_SIZE;

    result->num_bins = FFT_BINS;
    result->bin_width_hz = current_cfg.sample_rate_hz / (float)FFT_SIZE;

#ifdef USE_CMSIS_DSP
    /* ── CMSIS-DSP path ── */

    /* Step 1: Window the real-valued input */
    for (i = 0; i < count; i++)
        fft_input[i] = (float)samples[i] * window_coeffs[i];
    for (i = count; i < FFT_SIZE; i++)
        fft_input[i] = 0.0f;

    /* Step 2: Real FFT */
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_cmsis_output, 0);

    /* Step 3: Magnitude in dB.
     * arm_rfft_fast_f32 packs DC and Nyquist in first two elements:
     *   output[0] = DC real, output[1] = Nyquist real */
    float scale = 2.0f / (float)FFT_SIZE;

    arm_cmplx_mag_f32(&fft_cmsis_output[2], &magnitude_buf[1], FFT_BINS - 1);
    arm_scale_f32(&magnitude_buf[1], scale, &magnitude_buf[1], FFT_BINS - 1);

    /* DC bin (no 2x factor) */
    {
        float dc_mag = fabsf(fft_cmsis_output[0]) / (float)FFT_SIZE;
        if (dc_mag < FFT_FLOOR) dc_mag = FFT_FLOOR;
        magnitude_buf[0] = 20.0f * log10f(dc_mag);
    }

    /* Convert bins 1..N/2-1 to dB */
    for (i = 1; i < FFT_BINS; i++) {
        if (magnitude_buf[i] < FFT_FLOOR)
            magnitude_buf[i] = FFT_FLOOR;
        magnitude_buf[i] = 20.0f * log10f(magnitude_buf[i]);
    }

#else
    /* ── Custom radix-2 fallback ── */

    /* Step 1: Convert int16 to float, apply window, load into complex buffer */
    for (i = 0; i < count; i++) {
        fft_buf[i * 2]     = (float)samples[i] * window_coeffs[i];
        fft_buf[i * 2 + 1] = 0.0f;
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

    for (i = 0; i < FFT_BINS; i++) {
        float re = fft_buf[i * 2];
        float im = fft_buf[i * 2 + 1];
        float mag = sqrtf(re * re + im * im) * scale;

        if (mag < FFT_FLOOR)
            mag = FFT_FLOOR;

        magnitude_buf[i] = 20.0f * log10f(mag);
    }

    /* DC bin doesn't get the 2x factor */
    {
        float re = fft_buf[0];
        float im = fft_buf[1];
        float mag = sqrtf(re * re + im * im) / (float)FFT_SIZE;
        if (mag < FFT_FLOOR) mag = FFT_FLOOR;
        magnitude_buf[0] = 20.0f * log10f(mag);
    }
#endif /* USE_CMSIS_DSP */

    result->magnitude_db = magnitude_buf;

    /* Step 4: Exponential moving average */
    if (current_cfg.avg_count > 0) {
        float alpha = 1.0f / (float)current_cfg.avg_count;
        if (!avg_primed) {
            /* First frame: copy directly */
            for (i = 0; i < FFT_BINS; i++)
                avg_buf[i] = magnitude_buf[i];
            avg_primed = true;
        } else {
            for (i = 0; i < FFT_BINS; i++)
                avg_buf[i] = avg_buf[i] * (1.0f - alpha) + magnitude_buf[i] * alpha;
        }
        result->avg_db = avg_buf;
    } else {
        result->avg_db = NULL;
    }

    /* Step 5: Max hold */
    if (current_cfg.max_hold) {
        for (i = 0; i < FFT_BINS; i++) {
            if (magnitude_buf[i] > max_hold_buf[i])
                max_hold_buf[i] = magnitude_buf[i];
        }
        result->max_hold_db = max_hold_buf;
    } else {
        result->max_hold_db = NULL;
    }

    /* Step 6: Find peaks (use averaged data if available) */
    const float *peak_source = (result->avg_db != NULL) ? result->avg_db : magnitude_buf;

    result->num_peaks = 0;
    result->peak_freq_hz = 0.0f;
    result->peak_mag_db = -200.0f;

    if (current_cfg.peak_count > 0) {
        find_peaks(peak_source, FFT_BINS,
                   result->bin_width_hz, result->peaks,
                   current_cfg.peak_count, &result->num_peaks);

        if (result->num_peaks > 0) {
            result->peak_freq_hz = result->peaks[0].freq_hz;
            result->peak_mag_db  = result->peaks[0].magnitude_db;
        }

        /* Label harmonics relative to fundamental */
        label_harmonics(result->peaks, result->num_peaks);
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

void fft_set_averaging(uint8_t count)
{
    current_cfg.avg_count = count;
    avg_primed = false;
}

void fft_set_max_hold(bool enable)
{
    current_cfg.max_hold = enable;
    if (enable)
        fft_reset_max_hold();
}

void fft_reset_max_hold(void)
{
    uint16_t i;
    for (i = 0; i < FFT_BINS; i++)
        max_hold_buf[i] = -200.0f;
}

float fft_adjust_ref_level(float delta_db)
{
    current_cfg.ref_level_db += delta_db;
    return current_cfg.ref_level_db;
}

void fft_zoom_in(void)
{
    uint16_t center = (current_cfg.zoom_start_bin + current_cfg.zoom_end_bin) / 2;
    uint16_t span = current_cfg.zoom_end_bin - current_cfg.zoom_start_bin;
    uint16_t new_span = span / 2;

    if (new_span < 16) new_span = 16;  /* Minimum 16 bins visible */

    uint16_t half = new_span / 2;
    current_cfg.zoom_start_bin = (center > half) ? center - half : 1;
    current_cfg.zoom_end_bin = current_cfg.zoom_start_bin + new_span;
    if (current_cfg.zoom_end_bin >= FFT_BINS)
        current_cfg.zoom_end_bin = FFT_BINS - 1;
}

void fft_zoom_out(void)
{
    uint16_t center = (current_cfg.zoom_start_bin + current_cfg.zoom_end_bin) / 2;
    uint16_t span = current_cfg.zoom_end_bin - current_cfg.zoom_start_bin;
    uint16_t new_span = span * 2;

    if (new_span >= FFT_BINS - 1) {
        current_cfg.zoom_start_bin = 1;
        current_cfg.zoom_end_bin = FFT_BINS - 1;
        return;
    }

    uint16_t half = new_span / 2;
    current_cfg.zoom_start_bin = (center > half) ? center - half : 1;
    current_cfg.zoom_end_bin = current_cfg.zoom_start_bin + new_span;
    if (current_cfg.zoom_end_bin >= FFT_BINS)
        current_cfg.zoom_end_bin = FFT_BINS - 1;
}

void fft_auto_configure(const int16_t *samples, uint16_t num_samples)
{
    int crossings = 0;
    int i;
    for (i = 1; i < (int)num_samples; i++) {
        if ((samples[i - 1] >= 0 && samples[i] < 0) ||
            (samples[i - 1] < 0 && samples[i] >= 0))
            crossings++;
    }
    float est_freq = (float)crossings * current_cfg.sample_rate_hz
                     / (2.0f * (float)num_samples);

    if (est_freq > 0.0f) {
        float max_freq = est_freq * 12.0f;
        uint16_t max_bin = (uint16_t)(max_freq / (current_cfg.sample_rate_hz / (float)FFT_SIZE));
        if (max_bin > FFT_BINS - 1) max_bin = FFT_BINS - 1;
        if (max_bin < 32) max_bin = 32;
        current_cfg.zoom_start_bin = 1;
        current_cfg.zoom_end_bin = max_bin;
    }
}
