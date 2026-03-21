/*
 * Tests for math channels and auto-measurements
 *
 * Build:
 *   gcc -o tests/test_math_measure tests/test_math_measure.c \
 *       src/dsp/math_channel.c src/tasks/measurement.c \
 *       -lm -Isrc/dsp -Isrc/tasks
 *
 * Run:
 *   ./tests/test_math_measure
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "math_channel.h"
#include "measurement.h"

#define SAMPLE_COUNT 4096
#define PASS "\033[32mPASS\033[0m"
#define FAIL "\033[31mFAIL\033[0m"

static int tests_run = 0;
static int tests_passed = 0;

/* ===================================================================
 * Test signal generators
 * =================================================================== */

/* Generate a sine wave into an int16 buffer */
static void gen_sine(int16_t *buf, uint16_t n, float freq_hz,
                     float sample_rate, int16_t amplitude)
{
    for (uint16_t i = 0; i < n; i++) {
        double phase = 2.0 * M_PI * freq_hz * i / sample_rate;
        buf[i] = (int16_t)(amplitude * sin(phase));
    }
}

/* Generate a square wave into an int16 buffer */
static void gen_square(int16_t *buf, uint16_t n, float freq_hz,
                       float sample_rate, int16_t amplitude, float duty)
{
    float period_samples = sample_rate / freq_hz;
    for (uint16_t i = 0; i < n; i++) {
        float pos = fmodf(i, period_samples);
        buf[i] = (pos < period_samples * duty) ? amplitude : -amplitude;
    }
}

/* Generate DC signal */
static void gen_dc(int16_t *buf, uint16_t n, int16_t level)
{
    for (uint16_t i = 0; i < n; i++) {
        buf[i] = level;
    }
}

/* ===================================================================
 * Assertion helpers
 * =================================================================== */

static void assert_near(const char *test_name, float actual, float expected,
                        float tolerance_pct)
{
    tests_run++;
    float diff;
    if (expected == 0.0f) {
        diff = fabsf(actual);
    } else {
        diff = fabsf((actual - expected) / expected) * 100.0f;
    }

    if (diff <= tolerance_pct || (expected == 0.0f && fabsf(actual) < 0.001f)) {
        tests_passed++;
        printf("  [%s] %s: got %.6f, expected %.6f (%.1f%% err)\n",
               PASS, test_name, actual, expected, diff);
    } else {
        printf("  [%s] %s: got %.6f, expected %.6f (%.1f%% err, max %.1f%%)\n",
               FAIL, test_name, actual, expected, diff, tolerance_pct);
    }
}

static void assert_true(const char *test_name, int condition)
{
    tests_run++;
    if (condition) {
        tests_passed++;
        printf("  [%s] %s\n", PASS, test_name);
    } else {
        printf("  [%s] %s\n", FAIL, test_name);
    }
}

/* ===================================================================
 * Math Channel Tests
 * =================================================================== */

static void test_math_add(void)
{
    printf("\n--- Math A+B ---\n");
    int16_t a[SAMPLE_COUNT], b[SAMPLE_COUNT], out[SAMPLE_COUNT];

    gen_sine(a, SAMPLE_COUNT, 1000.0f, 44100.0f, 10000);
    gen_sine(b, SAMPLE_COUNT, 1000.0f, 44100.0f, 5000);

    math_config_t cfg = { .operation = MATH_ADD, .scale = 1.0f };
    math_channel_compute(a, b, out, SAMPLE_COUNT, &cfg);

    /* At peak of sine: a ~ 10000, b ~ 5000, sum ~ 15000 */
    int16_t max_out = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (out[i] > max_out) max_out = out[i];
    }
    assert_near("A+B peak amplitude", max_out, 15000.0f, 2.0f);
}

static void test_math_sub_same(void)
{
    printf("\n--- Math A-B (same signal) ---\n");
    int16_t a[SAMPLE_COUNT], out[SAMPLE_COUNT];

    gen_sine(a, SAMPLE_COUNT, 1000.0f, 44100.0f, 16000);

    math_config_t cfg = { .operation = MATH_SUB, .scale = 1.0f };
    math_channel_compute(a, a, out, SAMPLE_COUNT, &cfg);

    /* A - A should be zero everywhere */
    int all_zero = 1;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (out[i] != 0) { all_zero = 0; break; }
    }
    assert_true("A-A is all zeros", all_zero);
}

static void test_math_mul(void)
{
    printf("\n--- Math A*B (AM modulation) ---\n");
    int16_t a[SAMPLE_COUNT], b[SAMPLE_COUNT], out[SAMPLE_COUNT];

    gen_sine(a, SAMPLE_COUNT, 1000.0f, 44100.0f, 16384);
    gen_square(b, SAMPLE_COUNT, 100.0f, 44100.0f, 16384, 0.5f);

    math_config_t cfg = { .operation = MATH_MUL, .scale = 1.0f };
    math_channel_compute(a, b, out, SAMPLE_COUNT, &cfg);

    /* When square = +16384: out ~ 16384*16384/32768 = 8192
     * When square = -16384: out ~ -8192
     * So the output should be amplitude modulated */
    int has_positive = 0, has_negative = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (out[i] > 4000) has_positive = 1;
        if (out[i] < -4000) has_negative = 1;
    }
    assert_true("A*B has positive excursion", has_positive);
    assert_true("A*B has negative excursion", has_negative);
}

static void test_math_invert(void)
{
    printf("\n--- Math Invert ---\n");
    int16_t a[SAMPLE_COUNT], b[SAMPLE_COUNT], out[SAMPLE_COUNT];

    gen_sine(a, SAMPLE_COUNT, 1000.0f, 44100.0f, 10000);
    memset(b, 0, sizeof(b)); /* unused for INV_A */

    math_config_t cfg = { .operation = MATH_INV_A, .scale = 1.0f };
    math_channel_compute(a, b, out, SAMPLE_COUNT, &cfg);

    int all_negated = 1;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (out[i] != -a[i]) { all_negated = 0; break; }
    }
    assert_true("Invert: output == -input", all_negated);
}

static void test_math_overflow(void)
{
    printf("\n--- Math Overflow Clamping ---\n");
    int16_t a[4] = { 32767, 32767, -32768, -32768 };
    int16_t b[4] = { 32767, 1,     -32768, -1 };
    int16_t out[4];

    math_config_t cfg = { .operation = MATH_ADD, .scale = 1.0f };
    math_channel_compute(a, b, out, 4, &cfg);

    assert_true("32767+32767 clamped to 32767", out[0] == 32767);
    assert_true("32767+1 clamped to 32767", out[1] == 32767);
    assert_true("-32768+(-32768) clamped to -32768", out[2] == -32768);
    assert_true("-32768+(-1) clamped to -32768", out[3] == -32768);
}

/* ===================================================================
 * Measurement Tests
 * =================================================================== */

static void test_freq_sine_1khz(void)
{
    printf("\n--- Frequency: 1kHz sine @ 44100 ---\n");
    int16_t buf[SAMPLE_COUNT];
    gen_sine(buf, SAMPLE_COUNT, 1000.0f, 44100.0f, 16384);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    assert_true("Result is valid", result.valid);
    assert_near("Frequency ~ 1000 Hz", result.frequency_hz, 1000.0f, 5.0f);
    assert_near("Period ~ 1ms", result.period_s, 0.001f, 5.0f);
}

static void test_vpp_sine(void)
{
    printf("\n--- Vpp: sine amplitude 16384 ---\n");
    int16_t buf[SAMPLE_COUNT];
    gen_sine(buf, SAMPLE_COUNT, 1000.0f, 44100.0f, 16384);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    /* Vpp = 2 * 16384 * 3.3/32768 = 3.3 * (2*16384/32768) = 3.3V */
    /* But sine peak may not land exactly on a sample */
    float expected_vpp = 2.0f * 16384.0f * (3.3f / 32768.0f);
    assert_near("Vpp ~ 3.3V", result.vpp, expected_vpp, 2.0f);
}

static void test_duty_50(void)
{
    printf("\n--- Duty Cycle: 50%% square ---\n");
    int16_t buf[SAMPLE_COUNT];
    gen_square(buf, SAMPLE_COUNT, 1000.0f, 44100.0f, 16384, 0.5f);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    assert_near("Duty ~ 50%%", result.duty_cycle, 50.0f, 3.0f);
}

static void test_duty_25(void)
{
    printf("\n--- Duty Cycle: 25%% square ---\n");
    int16_t buf[SAMPLE_COUNT];
    gen_square(buf, SAMPLE_COUNT, 1000.0f, 44100.0f, 16384, 0.25f);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    assert_near("Duty ~ 25%%", result.duty_cycle, 25.0f, 3.0f);
}

static void test_dc_signal(void)
{
    printf("\n--- DC Signal ---\n");
    int16_t buf[SAMPLE_COUNT];
    int16_t dc_level = 10000;
    gen_dc(buf, SAMPLE_COUNT, dc_level);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    float expected_vavg = dc_level * (3.3f / 32768.0f);
    assert_near("Frequency ~ 0 Hz", result.frequency_hz, 0.0f, 1.0f);
    assert_near("Vavg ~ DC level", result.vavg, expected_vavg, 1.0f);
    assert_near("Vpp ~ 0 (DC)", result.vpp, 0.0f, 1.0f);
}

static void test_vrms_sine(void)
{
    printf("\n--- Vrms: sine wave ---\n");
    int16_t buf[SAMPLE_COUNT];
    int16_t amplitude = 16384;
    gen_sine(buf, SAMPLE_COUNT, 1000.0f, 44100.0f, amplitude);

    measurement_result_t result;
    measurement_compute(buf, SAMPLE_COUNT, 44100.0f, &result);

    /* For a sine wave, Vrms = Vpeak / sqrt(2)
     * Vpeak in volts = amplitude * 3.3/32768
     * Vrms = amplitude * 3.3/32768 / sqrt(2) */
    float vpeak = amplitude * (3.3f / 32768.0f);
    float expected_vrms = vpeak / sqrtf(2.0f);
    assert_near("Vrms = Vpeak/sqrt(2)", result.vrms, expected_vrms, 2.0f);
}

static void test_state_machine(void)
{
    printf("\n--- State Machine ---\n");
    measurement_context_t ctx;
    measurement_init(&ctx, 500);

    assert_true("Initial state is IDLE", ctx.state == MEAS_IDLE);

    /* Simulate acquisition */
    ctx.state = MEAS_ACQUIRING;
    measurement_tick(&ctx, 100);
    assert_true("Still acquiring at 100ms", ctx.state == MEAS_ACQUIRING);

    measurement_tick(&ctx, 400);
    assert_true("Timeout at 500ms", ctx.state == MEAS_TIMEOUT);
    assert_true("Result invalid on timeout", ctx.result.valid == false);
}

/* ===================================================================
 * Main
 * =================================================================== */

int main(void)
{
    printf("=== Math Channel & Measurement Tests ===\n");

    /* Math channel tests */
    test_math_add();
    test_math_sub_same();
    test_math_mul();
    test_math_invert();
    test_math_overflow();

    /* Measurement tests */
    test_freq_sine_1khz();
    test_vpp_sine();
    test_duty_50();
    test_duty_25();
    test_dc_signal();
    test_vrms_sine();
    test_state_machine();

    printf("\n========================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
