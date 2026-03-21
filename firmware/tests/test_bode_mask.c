/*
 * Tests for Bode Plot and Mask Testing modules
 *
 * Build:
 *   gcc -o tests/test_bode_mask tests/test_bode_mask.c \
 *       src/dsp/bode.c src/tasks/mask_test.c \
 *       -lm -Isrc/dsp -Isrc/tasks -DTEST_BUILD
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bode.h"
#include "mask_test.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("  PASS: %s\n", msg); \
    } \
} while(0)

#define ASSERT_NEAR(val, expected, tol, msg) do { \
    tests_run++; \
    float _v = (val), _e = (expected), _t = (tol); \
    if (fabsf(_v - _e) > _t) { \
        printf("  FAIL: %s (got %.4f, expected %.4f +/- %.4f)\n", msg, _v, _e, _t); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("  PASS: %s (%.4f)\n", msg, _v); \
    } \
} while(0)

/* ========================================================================
 * Generate a sine wave in int16_t samples
 * ======================================================================== */
static void gen_sine(int16_t *buf, uint16_t num_samples, float sample_rate,
                     float freq_hz, float amplitude, float phase_rad)
{
    for (uint16_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freq_hz * t + phase_rad));
    }
}

/* ========================================================================
 * Bode Tests
 * ======================================================================== */
static void test_bode_log_sweep(void)
{
    printf("\n--- Bode: Logarithmic frequency sweep ---\n");

    bode_config_t cfg = {
        .start_freq_hz = 10.0f,
        .stop_freq_hz  = 10000.0f,
        .num_points    = 50,
        .amplitude_vpp = 1.0f,
        .log_sweep     = true
    };

    bode_init(&cfg);

    float f0 = bode_step_frequency(&cfg, 0);
    float f_mid = bode_step_frequency(&cfg, 25);
    float f_last = bode_step_frequency(&cfg, 49);

    ASSERT_NEAR(f0, 10.0f, 0.01f, "Log sweep start = 10 Hz");
    ASSERT_NEAR(f_last, 10000.0f, 1.0f, "Log sweep end = 10 kHz");

    /* Mid-point of log sweep: sqrt(10 * 10000) = 316.2 Hz */
    /* Actually: 10 * (1000)^(25/49) */
    float expected_mid = 10.0f * powf(1000.0f, 25.0f / 49.0f);
    ASSERT_NEAR(f_mid, expected_mid, 1.0f, "Log sweep midpoint is geometrically spaced");

    /* Verify monotonically increasing */
    int monotonic = 1;
    for (uint16_t i = 1; i < 50; i++) {
        if (bode_step_frequency(&cfg, i) <= bode_step_frequency(&cfg, i - 1)) {
            monotonic = 0;
            break;
        }
    }
    ASSERT(monotonic, "Log sweep frequencies are monotonically increasing");
}

static void test_bode_linear_sweep(void)
{
    printf("\n--- Bode: Linear frequency sweep ---\n");

    bode_config_t cfg = {
        .start_freq_hz = 100.0f,
        .stop_freq_hz  = 5000.0f,
        .num_points    = 50,
        .amplitude_vpp = 1.0f,
        .log_sweep     = false
    };

    bode_init(&cfg);

    float f0 = bode_step_frequency(&cfg, 0);
    float f1 = bode_step_frequency(&cfg, 1);
    float f_last = bode_step_frequency(&cfg, 49);

    ASSERT_NEAR(f0, 100.0f, 0.01f, "Linear sweep start = 100 Hz");
    ASSERT_NEAR(f_last, 5000.0f, 0.1f, "Linear sweep end = 5000 Hz");

    /* Step size should be (5000-100)/49 = 100 Hz */
    float step_size = f1 - f0;
    ASSERT_NEAR(step_size, 100.0f, 0.1f, "Linear sweep step = 100 Hz");

    /* Verify even spacing */
    float f25 = bode_step_frequency(&cfg, 25);
    ASSERT_NEAR(f25, 100.0f + 25.0f * 100.0f, 0.5f, "Linear sweep midpoint is evenly spaced");
}

static void test_bode_unity_gain(void)
{
    printf("\n--- Bode: Unity gain (same signal in/out) ---\n");

    float sample_rate = 100000.0f;
    uint16_t num_samples = 1000;
    float freq = 1000.0f;

    int16_t *input  = malloc(num_samples * sizeof(int16_t));
    int16_t *output = malloc(num_samples * sizeof(int16_t));

    gen_sine(input, num_samples, sample_rate, freq, 1000.0f, 0.0f);
    memcpy(output, input, num_samples * sizeof(int16_t));

    bode_point_t point;
    bode_process_point(input, output, num_samples, sample_rate, freq, &point);

    ASSERT_NEAR(point.gain_db, 0.0f, 0.1f, "Unity gain -> 0 dB");
    ASSERT_NEAR(point.phase_deg, 0.0f, 1.0f, "Same signal -> 0 degrees phase");

    free(input);
    free(output);
}

static void test_bode_6db_attenuation(void)
{
    printf("\n--- Bode: 6dB attenuation (half amplitude) ---\n");

    float sample_rate = 100000.0f;
    uint16_t num_samples = 1000;
    float freq = 1000.0f;

    int16_t *input  = malloc(num_samples * sizeof(int16_t));
    int16_t *output = malloc(num_samples * sizeof(int16_t));

    gen_sine(input, num_samples, sample_rate, freq, 2000.0f, 0.0f);
    gen_sine(output, num_samples, sample_rate, freq, 1000.0f, 0.0f);

    bode_point_t point;
    bode_process_point(input, output, num_samples, sample_rate, freq, &point);

    ASSERT_NEAR(point.gain_db, -6.02f, 0.2f, "Half amplitude -> -6 dB");

    free(input);
    free(output);
}

static void test_bode_phase(void)
{
    printf("\n--- Bode: Phase measurement ---\n");

    float sample_rate = 100000.0f;
    uint16_t num_samples = 2000;
    float freq = 500.0f;

    int16_t *input  = malloc(num_samples * sizeof(int16_t));
    int16_t *output = malloc(num_samples * sizeof(int16_t));

    /* Same signal -> 0 degrees */
    gen_sine(input, num_samples, sample_rate, freq, 1000.0f, 0.0f);
    gen_sine(output, num_samples, sample_rate, freq, 1000.0f, 0.0f);

    bode_point_t point;
    bode_process_point(input, output, num_samples, sample_rate, freq, &point);
    ASSERT_NEAR(point.phase_deg, 0.0f, 1.0f, "In-phase -> 0 degrees");

    /* Inverted signal -> 180 degrees */
    gen_sine(output, num_samples, sample_rate, freq, 1000.0f, M_PI);

    bode_process_point(input, output, num_samples, sample_rate, freq, &point);
    ASSERT_NEAR(fabsf(point.phase_deg), 180.0f, 1.0f, "Inverted signal -> 180 degrees");

    free(input);
    free(output);
}

static void test_bode_bandwidth(void)
{
    printf("\n--- Bode: Bandwidth (-3dB point) of RC filter ---\n");

    /* Simulate a first-order RC lowpass filter:
     * H(f) = 1 / sqrt(1 + (f/fc)^2)
     * fc = 1000 Hz -> -3dB at 1000 Hz
     * Gain_dB = -10*log10(1 + (f/fc)^2) */

    float fc = 1000.0f;
    bode_result_t result;
    memset(&result, 0, sizeof(result));

    result.num_points = 100;
    for (uint16_t i = 0; i < result.num_points; i++) {
        float f = 10.0f * powf(100000.0f / 10.0f, (float)i / (float)(result.num_points - 1));
        float ratio = f / fc;
        float gain_db = -10.0f * log10f(1.0f + ratio * ratio);

        result.points[i].frequency_hz = f;
        result.points[i].gain_db = gain_db;
        result.points[i].phase_deg = -atanf(ratio) * 180.0f / M_PI;
    }

    float bw = bode_find_bandwidth(&result);
    ASSERT_NEAR(bw, 1000.0f, 50.0f, "RC filter -3dB bandwidth ~1000 Hz");
}

/* ========================================================================
 * Mask Tests
 * ======================================================================== */
static void test_mask_same_sine_pass(void)
{
    printf("\n--- Mask: Same sine wave -> PASS ---\n");

    uint16_t num = 320;
    int16_t *ref = malloc(num * sizeof(int16_t));
    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    bool result = mask_test(&mask, ref, num);
    ASSERT(result == true, "Same waveform passes mask test");

    free(ref);
}

static void test_mask_2x_amplitude_fail(void)
{
    printf("\n--- Mask: 2x amplitude -> FAIL ---\n");

    uint16_t num = 320;
    int16_t *ref = malloc(num * sizeof(int16_t));
    int16_t *big = malloc(num * sizeof(int16_t));

    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);
    gen_sine(big, num, 320.0f, 1.0f, 10000.0f, 0.0f);

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    bool result = mask_test(&mask, big, num);
    ASSERT(result == false, "2x amplitude fails mask test");

    free(ref);
    free(big);
}

static void test_mask_slight_variation_pass(void)
{
    printf("\n--- Mask: Slight variation within tolerance -> PASS ---\n");

    uint16_t num = 320;
    int16_t *ref   = malloc(num * sizeof(int16_t));
    int16_t *noisy = malloc(num * sizeof(int16_t));

    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);

    /* Add small noise (well within 10% tolerance + 100 minimum margin) */
    for (uint16_t i = 0; i < num; i++) {
        noisy[i] = ref[i] + (int16_t)(50 * sinf(13.0f * i));
    }

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    bool result = mask_test(&mask, noisy, num);
    ASSERT(result == true, "Small noise within tolerance passes");

    free(ref);
    free(noisy);
}

static void test_mask_pass_rate(void)
{
    printf("\n--- Mask: Pass rate calculation ---\n");

    uint16_t num = 320;
    int16_t *ref = malloc(num * sizeof(int16_t));
    int16_t *big = malloc(num * sizeof(int16_t));

    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);
    gen_sine(big, num, 320.0f, 1.0f, 10000.0f, 0.0f);

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    /* 8 passes */
    for (int i = 0; i < 8; i++) {
        mask_test(&mask, ref, num);
    }
    /* 2 fails */
    for (int i = 0; i < 2; i++) {
        mask_test(&mask, big, num);
    }

    float rate = mask_pass_rate(&mask);
    ASSERT_NEAR(rate, 80.0f, 0.01f, "8 pass + 2 fail = 80%");
    ASSERT(mask.total_tests == 10, "Total tests = 10");
    ASSERT(mask.pass_count == 8, "Pass count = 8");
    ASSERT(mask.fail_count == 2, "Fail count = 2");

    free(ref);
    free(big);
}

static void test_mask_reset_counts(void)
{
    printf("\n--- Mask: Reset counters ---\n");

    uint16_t num = 320;
    int16_t *ref = malloc(num * sizeof(int16_t));
    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    mask_test(&mask, ref, num);
    mask_test(&mask, ref, num);
    mask_reset_counts(&mask);

    ASSERT(mask.total_tests == 0, "total_tests = 0 after reset");
    ASSERT(mask.pass_count == 0, "pass_count = 0 after reset");
    ASSERT(mask.fail_count == 0, "fail_count = 0 after reset");

    free(ref);
}

static void test_mask_clear(void)
{
    printf("\n--- Mask: Clear mask ---\n");

    uint16_t num = 320;
    int16_t *ref = malloc(num * sizeof(int16_t));
    gen_sine(ref, num, 320.0f, 1.0f, 5000.0f, 0.0f);

    mask_state_t mask;
    mask_create_from_waveform(&mask, ref, num, 10.0f);

    ASSERT(mask.enabled == true, "Mask is enabled after creation");

    mask_clear(&mask);

    ASSERT(mask.enabled == false, "Mask is disabled after clear");

    int any_defined = 0;
    for (int i = 0; i < MASK_WIDTH; i++) {
        if (mask.defined[i]) { any_defined = 1; break; }
    }
    ASSERT(any_defined == 0, "All defined[] = false after clear");

    free(ref);
}

/* ========================================================================
 * Main
 * ======================================================================== */
int main(void)
{
    printf("=== Bode Plot & Mask Testing Unit Tests ===\n");

    /* Bode tests */
    test_bode_log_sweep();
    test_bode_linear_sweep();
    test_bode_unity_gain();
    test_bode_6db_attenuation();
    test_bode_phase();
    test_bode_bandwidth();

    /* Mask tests */
    test_mask_same_sine_pass();
    test_mask_2x_amplitude_fail();
    test_mask_slight_variation_pass();
    test_mask_pass_rate();
    test_mask_reset_counts();
    test_mask_clear();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
