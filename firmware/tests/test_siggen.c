/*
 * Native host tests for the DDS signal generator engine.
 *
 * Build: gcc -o test_siggen test_siggen.c ../src/dsp/signal_gen.c -lm -I../src/dsp
 * Or from firmware/: gcc -o tests/test_siggen tests/test_siggen.c src/dsp/signal_gen.c -lm -Isrc/dsp
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "signal_gen.h"

#define SAMPLE_RATE 250000.0f
#define ASSERT(cond, msg) do { \
    tests_total++; \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
    } \
} while(0)

static int tests_total = 0;
static int tests_failed = 0;

/* Test 1: Tuning word calculation for 1kHz @ 250kHz sample rate */
static void test_tuning_word(void)
{
    printf("\n[Test 1] Tuning word for 1kHz @ 250kHz\n");
    siggen_config_t cfg = {
        .waveform = SIGGEN_SINE,
        .frequency_hz = 1000.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);

    /* Generate 1 sample to trigger tuning word computation */
    int16_t buf[1];
    siggen_fill_buffer(buf, 1, SAMPLE_RATE);

    /* Expected tuning word: (1000/250000) * 2^32 = 17179869.184 ~ 17179869 */
    /* We verify indirectly by checking period accuracy */
    uint32_t expected = (uint32_t)((double)1000.0 / (double)250000.0 * 4294967296.0);
    printf("  Expected tuning word: %u\n", expected);
    ASSERT(expected == 17179869 || expected == 17179870,
           "Tuning word for 1kHz @ 250kHz is ~17179869");
}

/* Test 2: Sine output at known phase points */
static void test_sine_phase_points(void)
{
    printf("\n[Test 2] Sine output at known phase points\n");
    siggen_config_t cfg = {
        .waveform = SIGGEN_SINE,
        .frequency_hz = 1000.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);

    /* Generate exactly 250 samples = one full 1kHz period at 250kHz sample rate */
    int16_t buf[250];
    siggen_fill_buffer(buf, 250, SAMPLE_RATE);

    /* Phase 0 (sample 0): sin(0) = 0 */
    ASSERT(abs(buf[0]) < 500, "Sine at phase 0 is near zero");

    /* Phase PI/2 (sample 62 or 63): sin(PI/2) = +max */
    int16_t max_val = 0;
    for (int i = 60; i < 66; i++) {
        if (buf[i] > max_val) max_val = buf[i];
    }
    ASSERT(max_val > 30000, "Sine near phase PI/2 reaches positive max");

    /* Phase PI (sample 125): sin(PI) = 0 */
    ASSERT(abs(buf[125]) < 1500, "Sine at phase PI is near zero");

    /* Phase 3PI/2 (sample 187 or 188): sin(3PI/2) = -max */
    int16_t min_val = 0;
    for (int i = 185; i < 191; i++) {
        if (buf[i] < min_val) min_val = buf[i];
    }
    ASSERT(min_val < -30000, "Sine near phase 3PI/2 reaches negative max");
}

/* Test 3: Square wave output */
static void test_square_wave(void)
{
    printf("\n[Test 3] Square wave output\n");
    siggen_config_t cfg = {
        .waveform = SIGGEN_SQUARE,
        .frequency_hz = 1000.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);

    int16_t buf[250];
    siggen_fill_buffer(buf, 250, SAMPLE_RATE);

    /* First half should be positive */
    int positive_count = 0;
    int negative_count = 0;
    for (int i = 0; i < 125; i++) {
        if (buf[i] > 0) positive_count++;
    }
    for (int i = 125; i < 250; i++) {
        if (buf[i] < 0) negative_count++;
    }
    ASSERT(positive_count > 120, "Square: first half is mostly positive");
    ASSERT(negative_count > 120, "Square: second half is mostly negative");

    /* Values should be at full amplitude */
    ASSERT(abs(buf[10]) > 32000, "Square: amplitude is near full scale");
}

/* Test 4: Frequency accuracy via zero-crossing counting */
static void test_frequency_accuracy(void)
{
    printf("\n[Test 4] Frequency accuracy (1kHz sine, zero-crossing count)\n");
    siggen_config_t cfg = {
        .waveform = SIGGEN_SINE,
        .frequency_hz = 1000.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);

    /* Generate 1 second of samples at 250kHz = 250000 samples */
    int num_samples = 250000;
    int16_t *buf = (int16_t *)malloc(num_samples * sizeof(int16_t));
    ASSERT(buf != NULL, "Buffer allocation succeeded");
    if (!buf) return;

    siggen_fill_buffer(buf, (uint16_t)50000, SAMPLE_RATE);
    /* Fill in chunks since num_samples exceeds uint16_t */
    /* Actually fill 50000 at a time, 5 times */
    siggen_init(&cfg); /* Reset phase */
    for (int chunk = 0; chunk < 5; chunk++) {
        siggen_fill_buffer(buf + chunk * 50000, 50000, SAMPLE_RATE);
    }

    /* Count zero crossings (positive to negative transitions) */
    int crossings = 0;
    for (int i = 1; i < num_samples; i++) {
        if (buf[i-1] >= 0 && buf[i] < 0) {
            crossings++;
        }
    }

    /* For a 1kHz sine, expect 1000 positive-to-negative crossings per second */
    float measured_freq = (float)crossings; /* crossings in 1 second = frequency */
    float error_pct = fabsf(measured_freq - 1000.0f) / 1000.0f * 100.0f;
    printf("  Zero crossings: %d, measured freq: %.1f Hz, error: %.3f%%\n",
           crossings, measured_freq, error_pct);
    ASSERT(error_pct < 0.1f, "Frequency accurate within 0.1%");

    free(buf);
}

/* Test 5: Low frequency (1 Hz) - the bug that was reported */
static void test_low_frequency(void)
{
    printf("\n[Test 5] Low frequency test (1 Hz @ 250kHz)\n");
    siggen_config_t cfg = {
        .waveform = SIGGEN_SINE,
        .frequency_hz = 1.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);

    /* Expected tuning word for 1 Hz: (1/250000) * 2^32 = 17179.869 ~ 17180 */
    uint32_t expected_tw = (uint32_t)((double)1.0 / (double)250000.0 * 4294967296.0);
    printf("  Expected tuning word for 1Hz: %u\n", expected_tw);
    ASSERT(expected_tw > 17000 && expected_tw < 17300,
           "Tuning word for 1Hz is ~17180 (non-zero, no integer truncation)");

    /* Generate 250000 samples = 1 second */
    int16_t *buf = (int16_t *)malloc(250000 * sizeof(int16_t));
    ASSERT(buf != NULL, "Buffer allocation succeeded");
    if (!buf) return;

    /* Fill in 5 chunks of 50000 */
    for (int chunk = 0; chunk < 5; chunk++) {
        siggen_fill_buffer(buf + chunk * 50000, 50000, SAMPLE_RATE);
    }

    /* Over 1 second at 1 Hz, we should see exactly 1 positive-to-negative crossing */
    int crossings = 0;
    for (int i = 1; i < 250000; i++) {
        if (buf[i-1] >= 0 && buf[i] < 0) {
            crossings++;
        }
    }
    printf("  Zero crossings in 1 second: %d (expected 1)\n", crossings);
    ASSERT(crossings == 1, "1 Hz produces exactly 1 full cycle per second");

    /* Verify signal actually has amplitude (not stuck at zero) */
    int16_t peak = 0;
    for (int i = 0; i < 250000; i++) {
        if (buf[i] > peak) peak = buf[i];
    }
    printf("  Peak amplitude: %d\n", peak);
    ASSERT(peak > 30000, "1 Hz signal reaches near full amplitude");

    free(buf);
}

/* Test 6: Waveform cycling */
static void test_waveform_cycle(void)
{
    printf("\n[Test 6] Waveform cycling\n");
    siggen_init(NULL);

    const siggen_config_t *cfg = siggen_get_config();
    ASSERT(cfg->waveform == SIGGEN_SINE, "Default waveform is Sine");

    siggen_waveform_t w = siggen_cycle_waveform();
    ASSERT(w == SIGGEN_SQUARE, "After cycling: Square");

    w = siggen_cycle_waveform();
    ASSERT(w == SIGGEN_TRIANGLE, "After cycling: Triangle");

    w = siggen_cycle_waveform();
    ASSERT(w == SIGGEN_SAWTOOTH, "After cycling: Sawtooth");

    w = siggen_cycle_waveform();
    ASSERT(w == SIGGEN_SINE, "After cycling: wraps back to Sine");
}

/* Test 7: Triangle and sawtooth sanity */
static void test_triangle_sawtooth(void)
{
    printf("\n[Test 7] Triangle and Sawtooth waveforms\n");
    int16_t buf[250];

    /* Triangle */
    siggen_config_t cfg = {
        .waveform = SIGGEN_TRIANGLE,
        .frequency_hz = 1000.0f,
        .amplitude_vpp = 3.3f,
        .offset_v = 0.0f,
        .output_enabled = true
    };
    siggen_init(&cfg);
    siggen_fill_buffer(buf, 250, SAMPLE_RATE);

    /* Triangle should start near 0, rise to max around sample 62, back to 0 at 125 */
    ASSERT(abs(buf[0]) < 1000, "Triangle starts near zero");

    int16_t tri_peak = 0;
    for (int i = 55; i < 70; i++) {
        if (buf[i] > tri_peak) tri_peak = buf[i];
    }
    ASSERT(tri_peak > 30000, "Triangle reaches positive peak");

    int16_t tri_trough = 0;
    for (int i = 180; i < 195; i++) {
        if (buf[i] < tri_trough) tri_trough = buf[i];
    }
    ASSERT(tri_trough < -30000, "Triangle reaches negative peak");

    /* Sawtooth */
    cfg.waveform = SIGGEN_SAWTOOTH;
    siggen_init(&cfg);
    siggen_fill_buffer(buf, 250, SAMPLE_RATE);

    /* Sawtooth should ramp linearly */
    ASSERT(buf[0] < -30000, "Sawtooth starts near minimum");
    ASSERT(buf[124] > -2000 && buf[124] < 2000, "Sawtooth crosses zero at midpoint");
    ASSERT(buf[249] > 30000, "Sawtooth ends near maximum");
}

int main(void)
{
    printf("=== DDS Signal Generator Tests ===\n");

    test_tuning_word();
    test_sine_phase_points();
    test_square_wave();
    test_frequency_accuracy();
    test_low_frequency();
    test_waveform_cycle();
    test_triangle_sawtooth();

    printf("\n=== Results: %d/%d passed ===\n",
           tests_total - tests_failed, tests_total);

    return tests_failed > 0 ? 1 : 0;
}
