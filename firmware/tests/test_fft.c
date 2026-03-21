/*
 * FFT Unit Test - Phase 2: 4096-point, 5 windows, averaging, max hold
 *
 * Build and run:
 *   gcc -o test_fft test_fft.c ../src/dsp/fft.c ../src/dsp/fft_test_signals.c -lm -I../src/dsp
 *   ./test_fft
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fft.h"
#include "fft_test_signals.h"

#define SAMPLE_RATE 44100.0f

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

static int16_t samples[FFT_SIZE];
static fft_result_t result;

/* Test 1: Single 1kHz sine — peak at correct frequency */
static void test_sine_peak(void)
{
    printf("\n[Test 1] 1kHz sine wave peak detection (4096-pt)\n");

    test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, 1000.0f, 0.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    printf("  Bin width: %.2f Hz\n", result.bin_width_hz);
    printf("  Peak freq: %.1f Hz (expected ~1000 Hz)\n", result.peak_freq_hz);
    printf("  Peak mag:  %.1f dB\n", result.peak_mag_db);

    float error = fabsf(result.peak_freq_hz - 1000.0f);
    ASSERT(error < result.bin_width_hz * 2.0f,
           "Peak frequency within 2 bins of 1kHz");
    ASSERT(result.bin_width_hz < 11.0f,
           "Bin width is ~10.77 Hz (4096-pt resolution)");
    ASSERT(result.num_peaks >= 1, "At least one peak detected");
}

/* Test 2: Square wave — odd harmonics */
static void test_square_harmonics(void)
{
    printf("\n[Test 2] 1kHz square wave odd harmonics (4096-pt)\n");

    test_signal_generate(TEST_SIG_SQUARE, samples, FFT_SIZE,
                         SAMPLE_RATE, 1000.0f, 0.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    printf("  Num peaks: %d\n", result.num_peaks);
    for (int i = 0; i < result.num_peaks && i < 4; i++)
        printf("  Peak %d: %.1f Hz @ %.1f dB\n",
               i + 1, result.peaks[i].freq_hz, result.peaks[i].magnitude_db);

    ASSERT(result.num_peaks >= 3, "At least 3 peaks detected");

    int found_3rd = 0;
    for (int i = 0; i < result.num_peaks; i++) {
        if (fabsf(result.peaks[i].freq_hz - 3000.0f) < result.bin_width_hz * 2.0f)
            found_3rd = 1;
    }
    ASSERT(found_3rd, "3rd harmonic detected near 3kHz");
}

/* Test 3: Dual sine — both frequencies detected */
static void test_dual_sine(void)
{
    printf("\n[Test 3] Dual sine (1kHz + 5kHz)\n");

    test_signal_generate(TEST_SIG_DUAL_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, 1000.0f, 5000.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    int found_1k = 0, found_5k = 0;
    for (int i = 0; i < result.num_peaks; i++) {
        if (fabsf(result.peaks[i].freq_hz - 1000.0f) < result.bin_width_hz * 2.0f)
            found_1k = 1;
        if (fabsf(result.peaks[i].freq_hz - 5000.0f) < result.bin_width_hz * 2.0f)
            found_5k = 1;
    }
    ASSERT(found_1k, "1kHz component detected");
    ASSERT(found_5k, "5kHz component detected");
}

/* Test 4: All 5 window functions */
static void test_all_windows(void)
{
    printf("\n[Test 4] All 5 window functions\n");

    float test_freq = 1234.5f;
    test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, test_freq, 0.0f, 0.8f);

    const char *names[] = { "Rectangular", "Hanning", "Hamming",
                            "Blackman-Harris", "Flat-Top" };
    float leakage_ratios[FFT_WINDOW_COUNT];

    for (int w = 0; w < FFT_WINDOW_COUNT; w++) {
        fft_set_window((fft_window_t)w);
        fft_process(samples, FFT_SIZE, &result);

        int peak_bin = result.peaks[0].bin;
        float leakage = 0.0f, main_lobe = 0.0f;
        for (int i = 1; i < result.num_bins; i++) {
            float linear = powf(10.0f, result.magnitude_db[i] / 20.0f);
            if (abs(i - peak_bin) <= 5)
                main_lobe += linear;
            else
                leakage += linear;
        }
        leakage_ratios[w] = (main_lobe > 0.0f) ? leakage / main_lobe : 999.0f;

        printf("  %-16s: peak=%.1f Hz, mag=%.1f dB, leakage=%.4f\n",
               names[w], result.peak_freq_hz, result.peak_mag_db,
               leakage_ratios[w]);
    }

    /* Blackman-Harris should have least leakage */
    ASSERT(leakage_ratios[FFT_WINDOW_BLACKMAN_HARRIS] < leakage_ratios[FFT_WINDOW_HANNING],
           "Blackman-Harris has less leakage than Hanning");
    ASSERT(leakage_ratios[FFT_WINDOW_HANNING] < leakage_ratios[FFT_WINDOW_RECTANGULAR],
           "Hanning has less leakage than Rectangular");

    /* Reset to Hanning */
    fft_set_window(FFT_WINDOW_HANNING);
}

/* Test 5: Bin width at 4096 */
static void test_bin_width(void)
{
    printf("\n[Test 5] Bin width at 4096 points\n");

    test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, 1000.0f, 0.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    float expected_bw = SAMPLE_RATE / (float)FFT_SIZE;
    printf("  Bin width: %.2f Hz (expected %.2f Hz)\n",
           result.bin_width_hz, expected_bw);
    printf("  Num bins: %d (expected %d)\n", result.num_bins, FFT_BINS);

    ASSERT(fabsf(result.bin_width_hz - expected_bw) < 0.01f,
           "Bin width = sample_rate / FFT_SIZE");
    ASSERT(result.num_bins == FFT_BINS, "Num bins = FFT_SIZE / 2");
}

/* Test 6: Averaging reduces noise */
static void test_averaging(void)
{
    printf("\n[Test 6] EMA averaging\n");

    fft_set_averaging(8);

    /* Run FFT multiple times with noise to build up average */
    for (int i = 0; i < 16; i++) {
        test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                             SAMPLE_RATE, 1000.0f, 0.0f, 0.8f);
        fft_process(samples, FFT_SIZE, &result);
    }

    ASSERT(result.avg_db != NULL, "Averaging buffer is non-NULL");
    ASSERT(result.peak_freq_hz > 0.0f, "Peak still detected with averaging");

    printf("  Peak freq: %.1f Hz (from averaged data)\n", result.peak_freq_hz);

    fft_set_averaging(0);
    ASSERT(1, "Averaging disabled without crash");
}

/* Test 7: Max hold retains peaks */
static void test_max_hold(void)
{
    printf("\n[Test 7] Max hold\n");

    fft_set_max_hold(true);

    /* First frame: 1kHz */
    test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, 1000.0f, 0.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    ASSERT(result.max_hold_db != NULL, "Max hold buffer is non-NULL");

    /* Remember the peak magnitude at 1kHz bin */
    int peak_bin_1k = result.peaks[0].bin;
    float peak_mag_1k = result.max_hold_db[peak_bin_1k];

    /* Second frame: 5kHz (different frequency) */
    test_signal_generate(TEST_SIG_SINE, samples, FFT_SIZE,
                         SAMPLE_RATE, 5000.0f, 0.0f, 0.8f);
    fft_process(samples, FFT_SIZE, &result);

    /* Max hold should still have the 1kHz peak */
    float retained = result.max_hold_db[peak_bin_1k];
    printf("  1kHz bin max hold: %.1f dB (should be >= %.1f dB)\n",
           retained, peak_mag_1k);

    ASSERT(retained >= peak_mag_1k - 0.01f,
           "Max hold retains 1kHz peak after switching to 5kHz");

    fft_set_max_hold(false);
}

/* Test 8: Zoom */
static void test_zoom(void)
{
    printf("\n[Test 8] Zoom in/out\n");

    const fft_config_t *cfg = fft_get_config();
    uint16_t initial_span = cfg->zoom_end_bin - cfg->zoom_start_bin;

    fft_zoom_in();
    uint16_t zoomed_span = cfg->zoom_end_bin - cfg->zoom_start_bin;
    ASSERT(zoomed_span < initial_span, "Zoom in reduces visible span");

    fft_zoom_out();
    fft_zoom_out(); /* Back to full */
    uint16_t full_span = cfg->zoom_end_bin - cfg->zoom_start_bin;
    ASSERT(full_span >= initial_span, "Zoom out restores full span");
}

/* Test 9: Ref level adjustment */
static void test_ref_level(void)
{
    printf("\n[Test 9] Reference level adjustment\n");

    const fft_config_t *cfg = fft_get_config();
    float original = cfg->ref_level_db;

    float new_ref = fft_adjust_ref_level(10.0f);
    ASSERT(fabsf(new_ref - (original + 10.0f)) < 0.01f,
           "Ref level increased by 10 dB");

    fft_adjust_ref_level(-10.0f);  /* Reset */
}

int main(void)
{
    printf("═══════════════════════════════════════\n");
    printf("  FFT Engine Unit Tests (Phase 2)\n");
    printf("  FFT Size: %d, Bins: %d\n", FFT_SIZE, FFT_BINS);
    printf("  Sample Rate: %.0f Hz\n", SAMPLE_RATE);
    printf("  Bin Width: %.2f Hz\n", SAMPLE_RATE / FFT_SIZE);
    printf("═══════════════════════════════════════\n");

    fft_config_t cfg = {
        .window         = FFT_WINDOW_HANNING,
        .sample_rate_hz = SAMPLE_RATE,
        .ref_level_db   = 0.0f,
        .db_range       = 80.0f,
        .peak_count     = 4,
        .avg_count      = 0,
        .max_hold       = false,
        .zoom_start_bin = 1,
        .zoom_end_bin   = FFT_BINS - 1,
    };
    fft_init(&cfg);

    test_sine_peak();
    test_square_harmonics();
    test_dual_sine();
    test_all_windows();
    test_bin_width();
    test_averaging();
    test_max_hold();
    test_zoom();
    test_ref_level();

    printf("\n═══════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════\n");

    return tests_failed > 0 ? 1 : 0;
}
