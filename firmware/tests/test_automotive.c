/*
 * Test suite for automotive diagnostic modules:
 *   - Relative Compression Test
 *   - Alternator Ripple Analysis
 *
 * Build (native):
 *   gcc -o tests/test_automotive tests/test_automotive.c \
 *       src/modules/compression_test.c src/modules/alternator_test.c \
 *       -lm -Isrc/modules
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "compression_test.h"
#include "alternator_test.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else      { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

#define RIPPLE_GOOD_MAX_MV_TEST 100.0f

#define ASSERT_NEAR(val, expected, tol, msg) do { \
    tests_run++; \
    float _v = (float)(val), _e = (float)(expected); \
    if (fabsf(_v - _e) <= (float)(tol)) { tests_passed++; printf("  PASS: %s (%.2f ~ %.2f)\n", msg, _v, _e); } \
    else { tests_failed++; printf("  FAIL: %s (got %.2f, expected %.2f, tol %.2f)\n", msg, _v, _e, (float)(tol)); } \
} while(0)

/* -----------------------------------------------------------------------
 * Helper: generate synthetic cranking current waveform
 * Each cylinder gets a Gaussian-shaped current peak.
 * amplitudes[ncyl] sets the relative height of each cylinder's peak.
 * ----------------------------------------------------------------------- */
static void generate_cranking_waveform(int16_t *buf, uint32_t len,
                                        uint8_t ncyl, uint8_t num_cycles,
                                        const float *amplitudes,
                                        float baseline)
{
    memset(buf, 0, len * sizeof(int16_t));

    /* Space peaks evenly across the buffer */
    uint16_t total_peaks = ncyl * num_cycles;
    float spacing = (float)len / (float)(total_peaks + 1);
    float peak_width = spacing * 0.2f; /* Gaussian sigma */

    for (uint16_t p = 0; p < total_peaks; p++) {
        float center = spacing * (float)(p + 1);
        uint8_t cyl = p % ncyl;
        float amp = amplitudes[cyl];

        /* Draw Gaussian peak */
        int start = (int)(center - peak_width * 4);
        int end   = (int)(center + peak_width * 4);
        if (start < 0) start = 0;
        if (end >= (int)len) end = (int)len - 1;

        for (int i = start; i <= end; i++) {
            float x = (float)i - center;
            float g = amp * expf(-(x * x) / (2.0f * peak_width * peak_width));
            float val = baseline + g;
            if (val > 32767.0f) val = 32767.0f;
            if (buf[i] < (int16_t)val) {
                buf[i] = (int16_t)val;
            }
        }
    }

    /* Fill non-peak regions with baseline */
    for (uint32_t i = 0; i < len; i++) {
        if (buf[i] == 0) buf[i] = (int16_t)baseline;
    }
}

/* -----------------------------------------------------------------------
 * Helper: generate alternator voltage waveform
 * 3-phase full-wave rectified ripple on top of DC.
 * phases_active: bitmask of which phases are active (0x7 = all three)
 * ----------------------------------------------------------------------- */
static void generate_alternator_waveform(int16_t *buf, uint32_t len,
                                          float sample_rate,
                                          float dc_voltage,
                                          float ripple_amplitude_v,
                                          float rotor_freq_hz,
                                          uint8_t phases_active,
                                          float voltage_scale)
{
    for (uint32_t i = 0; i < len; i++) {
        float t = (float)i / sample_rate;
        float v = dc_voltage;

        /* Add 3-phase rectified ripple.
         * Each phase is 120 degrees apart. Full-wave rectification
         * gives |sin| for each phase. The sum of 3 rectified phases
         * produces ripple at 3x rotor frequency. */
        for (int phase = 0; phase < 3; phase++) {
            if (!(phases_active & (1 << phase))) continue;
            float angle = 2.0f * (float)M_PI * rotor_freq_hz * t +
                          (float)phase * 2.0f * (float)M_PI / 3.0f;
            v += ripple_amplitude_v * fabsf(sinf(angle));
        }

        /* Convert voltage to ADC counts using inverse of voltage_scale */
        buf[i] = (int16_t)(v / voltage_scale);
    }
}

/* =======================================================================
 * Compression Test Cases
 * ======================================================================= */

static void test_compression_equal_cylinders(void)
{
    printf("\n--- Compression: 4 equal cylinders ---\n");

    compression_config_t cfg = {
        .num_cylinders = 4,
        .firing_order = {1, 3, 4, 2},
        .pass_threshold_pct = 85.0f,
        .cranks_to_average = 3,
    };
    compression_init(&cfg);

    float amps[4] = {1000.0f, 1000.0f, 1000.0f, 1000.0f};
    int16_t waveform[4000];
    generate_cranking_waveform(waveform, 4000, 4, 3, amps, 100.0f);

    compression_result_t res;
    compression_analyze(waveform, 4000, 10000.0f, &res);

    ASSERT_TRUE(res.valid, "Result is valid");
    ASSERT_TRUE(res.num_cylinders == 4, "4 cylinders reported");

    for (int i = 0; i < 4; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Cyl %d relative ~100%%", i);
        ASSERT_NEAR(res.relative_pct[i], 100.0f, 5.0f, msg);
        snprintf(msg, sizeof(msg), "Cyl %d PASS", i);
        ASSERT_TRUE(res.pass[i], msg);
    }

    ASSERT_NEAR(res.variation_pct, 0.0f, 5.0f, "Low variation");
}

static void test_compression_weak_cylinder(void)
{
    printf("\n--- Compression: 1 weak cylinder (70%%) ---\n");

    compression_config_t cfg = {
        .num_cylinders = 4,
        .firing_order = {1, 3, 4, 2},
        .pass_threshold_pct = 85.0f,
        .cranks_to_average = 3,
    };
    compression_init(&cfg);

    /* Cylinder 2 (index 2) is weak at 70% */
    float amps[4] = {1000.0f, 1000.0f, 700.0f, 1000.0f};
    int16_t waveform[4000];
    generate_cranking_waveform(waveform, 4000, 4, 3, amps, 100.0f);

    compression_result_t res;
    compression_analyze(waveform, 4000, 10000.0f, &res);

    ASSERT_TRUE(res.valid, "Result is valid");
    ASSERT_TRUE(res.pass[0], "Cyl 0 PASS");
    ASSERT_TRUE(res.pass[1], "Cyl 1 PASS");
    ASSERT_TRUE(!res.pass[2], "Cyl 2 FAIL (weak)");
    ASSERT_TRUE(res.pass[3], "Cyl 3 PASS");
    ASSERT_TRUE(res.weakest_cylinder == 2, "Weakest = cyl 2");
    ASSERT_NEAR(res.relative_pct[2], 70.0f, 5.0f, "Weak cyl ~70%");
    ASSERT_TRUE(res.variation_pct > 25.0f, "Variation > 25%");
}

static void test_compression_6_cylinder(void)
{
    printf("\n--- Compression: 6 cylinders ---\n");

    compression_config_t cfg = {
        .num_cylinders = 6,
        .firing_order = {1, 5, 3, 6, 2, 4},
        .pass_threshold_pct = 85.0f,
        .cranks_to_average = 2,
    };
    compression_init(&cfg);

    float amps[6] = {1000.0f, 950.0f, 980.0f, 1000.0f, 960.0f, 970.0f};
    int16_t waveform[6000];
    generate_cranking_waveform(waveform, 6000, 6, 2, amps, 100.0f);

    compression_result_t res;
    compression_analyze(waveform, 6000, 10000.0f, &res);

    ASSERT_TRUE(res.valid, "Result is valid");
    ASSERT_TRUE(res.num_cylinders == 6, "6 cylinders reported");

    /* All should pass since weakest is 950/1000 = 95% > 85% threshold */
    for (int i = 0; i < 6; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Cyl %d PASS (all healthy)", i);
        ASSERT_TRUE(res.pass[i], msg);
    }
}

static void test_compression_peak_detection(void)
{
    printf("\n--- Compression: peak detection with noise ---\n");

    compression_config_t cfg = {
        .num_cylinders = 4,
        .firing_order = {1, 3, 4, 2},
        .pass_threshold_pct = 85.0f,
        .cranks_to_average = 1,
    };
    compression_init(&cfg);

    /* Create 4 clean peaks separated by noise */
    int16_t samples[2000];
    memset(samples, 0, sizeof(samples));
    /* Add baseline noise */
    for (int i = 0; i < 2000; i++) {
        samples[i] = 50 + (int16_t)((i * 7) % 30); /* Small pseudo-noise */
    }
    /* Add 4 clear peaks */
    samples[250]  = 800;
    samples[251]  = 900;
    samples[252]  = 800;
    samples[750]  = 850;
    samples[751]  = 950;
    samples[752]  = 850;
    samples[1250] = 800;
    samples[1251] = 880;
    samples[1252] = 800;
    samples[1750] = 820;
    samples[1751] = 920;
    samples[1752] = 820;

    uint16_t peak_idx[16];
    uint16_t count = compression_find_peaks(samples, 2000, peak_idx, 16, 200);

    ASSERT_TRUE(count == 4, "Found 4 peaks");
    if (count == 4) {
        ASSERT_NEAR(peak_idx[0], 251, 5, "Peak 0 near 251");
        ASSERT_NEAR(peak_idx[1], 751, 5, "Peak 1 near 751");
        ASSERT_NEAR(peak_idx[2], 1251, 5, "Peak 2 near 1251");
        ASSERT_NEAR(peak_idx[3], 1751, 5, "Peak 3 near 1751");
    }
}

static void test_compression_averaging(void)
{
    printf("\n--- Compression: 3-cycle averaging stability ---\n");

    compression_config_t cfg = {
        .num_cylinders = 4,
        .firing_order = {1, 3, 4, 2},
        .pass_threshold_pct = 85.0f,
        .cranks_to_average = 3,
    };
    compression_init(&cfg);

    /* Slight variations between cycles, but consistent per cylinder */
    float amps[4] = {1000.0f, 900.0f, 950.0f, 980.0f};
    int16_t waveform[8000];
    generate_cranking_waveform(waveform, 8000, 4, 3, amps, 100.0f);

    compression_result_t res;
    compression_analyze(waveform, 8000, 10000.0f, &res);

    ASSERT_TRUE(res.valid, "Result is valid");
    ASSERT_TRUE(res.cranks_analyzed >= 2, "Multiple cycles analyzed");

    /* Cyl 1 should be weakest (900 amplitude) */
    ASSERT_TRUE(res.weakest_cylinder == 1, "Weakest = cyl 1 (lowest amp)");

    /* All should still pass since 900/1000 = 90% > 85% */
    for (int i = 0; i < 4; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Cyl %d PASS (within threshold)", i);
        ASSERT_TRUE(res.pass[i], msg);
    }
}

/* =======================================================================
 * Alternator Test Cases
 * ======================================================================= */

static void test_alternator_good(void)
{
    printf("\n--- Alternator: Good (3-phase, low ripple) ---\n");

    /* Generate healthy alternator: 14.2V DC, ~50mV ripple, 3-phase at 100Hz rotor */
    float voltage_scale = 0.001f;  /* 1 count = 1mV */
    float rotor_hz = 100.0f;       /* 6000 RPM / 60 */
    float sample_rate = 10000.0f;
    uint32_t len = 2048;

    int16_t samples[2048];
    /* Ripple amplitude per phase — total ripple will be small due to
     * 3-phase cancellation. Use small amplitude. */
    generate_alternator_waveform(samples, len, sample_rate,
                                  14.2f, 0.010f, rotor_hz, 0x7,
                                  voltage_scale);

    alternator_result_t res;
    alternator_analyze(samples, len, sample_rate, voltage_scale, &res);

    ASSERT_TRUE(res.status == ALT_GOOD, "Status = GOOD");
    ASSERT_TRUE(res.is_charging, "Is charging");
    ASSERT_NEAR(res.dc_voltage, 14.2f, 1.0f, "DC ~14.2V");
    ASSERT_TRUE(res.ripple_mv < RIPPLE_GOOD_MAX_MV_TEST,
                "Ripple < 100mV");
    ASSERT_TRUE(res.diodes_working >= 5, "5-6 diodes working");
    printf("  INFO: diagnosis = \"%s\"\n", res.diagnosis);
}

static void test_alternator_diode_fault(void)
{
    printf("\n--- Alternator: Diode fault (missing phase) ---\n");

    float voltage_scale = 0.001f;
    float rotor_hz = 100.0f;
    float sample_rate = 10000.0f;
    uint32_t len = 2048;

    int16_t samples[2048];
    /* Only 2 of 3 phases active — bigger ripple, strong 1x component.
     * With a missing phase the ripple amplitude is much larger. */
    generate_alternator_waveform(samples, len, sample_rate,
                                  13.8f, 0.200f, rotor_hz, 0x3,
                                  voltage_scale);

    alternator_result_t res;
    alternator_analyze(samples, len, sample_rate, voltage_scale, &res);

    ASSERT_TRUE(res.status == ALT_DIODE_FAULT, "Status = DIODE_FAULT");
    ASSERT_TRUE(res.ripple_mv > 100.0f, "High ripple");
    ASSERT_TRUE(res.diodes_working <= 5, "Diodes <= 5");
    printf("  INFO: ripple = %.0f mV, diodes = %u, diagnosis = \"%s\"\n",
           res.ripple_mv, res.diodes_working, res.diagnosis);
}

static void test_alternator_not_charging(void)
{
    printf("\n--- Alternator: Not charging (DC only) ---\n");

    float voltage_scale = 0.001f;
    float sample_rate = 10000.0f;
    uint32_t len = 1024;

    /* Pure DC at battery voltage — no ripple = alternator not working */
    int16_t samples[1024];
    for (uint32_t i = 0; i < len; i++) {
        samples[i] = (int16_t)(12.6f / voltage_scale);
    }

    alternator_result_t res;
    alternator_analyze(samples, len, sample_rate, voltage_scale, &res);

    ASSERT_TRUE(res.status == ALT_NOT_CHARGING, "Status = NOT_CHARGING");
    ASSERT_TRUE(!res.is_charging, "Not charging");
    ASSERT_NEAR(res.dc_voltage, 12.6f, 0.1f, "DC = 12.6V");
    ASSERT_TRUE(res.ripple_mv < 10.0f, "No ripple");
    printf("  INFO: diagnosis = \"%s\"\n", res.diagnosis);
}

static void test_alternator_high_ripple(void)
{
    printf("\n--- Alternator: High ripple (500mV+) ---\n");

    float voltage_scale = 0.001f;
    float rotor_hz = 50.0f;
    float sample_rate = 10000.0f;
    uint32_t len = 2048;

    int16_t samples[2048];
    /* Very high ripple amplitude — bad diode(s) */
    generate_alternator_waveform(samples, len, sample_rate,
                                  13.8f, 0.400f, rotor_hz, 0x3,
                                  voltage_scale);

    alternator_result_t res;
    alternator_analyze(samples, len, sample_rate, voltage_scale, &res);

    ASSERT_TRUE(res.status == ALT_DIODE_FAULT, "Status = DIODE_FAULT");
    ASSERT_TRUE(res.ripple_mv >= 200.0f, "Ripple >= 200mV (high)");
    printf("  INFO: ripple = %.0f mV, diagnosis = \"%s\"\n",
           res.ripple_mv, res.diagnosis);
}

static void test_alternator_weak_output(void)
{
    printf("\n--- Alternator: Weak output (low DC voltage) ---\n");

    float voltage_scale = 0.001f;
    float rotor_hz = 80.0f;
    float sample_rate = 10000.0f;
    uint32_t len = 2048;

    int16_t samples[2048];
    /* Low DC voltage (12.0V) with noticeable ripple = weak alternator */
    generate_alternator_waveform(samples, len, sample_rate,
                                  12.0f, 0.030f, rotor_hz, 0x7,
                                  voltage_scale);

    alternator_result_t res;
    alternator_analyze(samples, len, sample_rate, voltage_scale, &res);

    ASSERT_TRUE(res.status == ALT_WEAK_OUTPUT, "Status = WEAK_OUTPUT");
    ASSERT_TRUE(!res.is_charging, "Not properly charging");
    ASSERT_NEAR(res.dc_voltage, 12.0f, 1.0f, "DC ~12.0V");
    printf("  INFO: diagnosis = \"%s\"\n", res.diagnosis);
}

/* ======================================================================= */

int main(void)
{
    printf("=== FNIRSI 2C53T Automotive Diagnostics Tests ===\n");

    /* Compression tests */
    test_compression_equal_cylinders();
    test_compression_weak_cylinder();
    test_compression_6_cylinder();
    test_compression_peak_detection();
    test_compression_averaging();

    /* Alternator tests */
    test_alternator_good();
    test_alternator_diode_fault();
    test_alternator_not_charging();
    test_alternator_high_ripple();
    test_alternator_weak_output();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
