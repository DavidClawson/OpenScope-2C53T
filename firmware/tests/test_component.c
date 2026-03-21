/*
 * test_component.c — Unit tests for component tester module
 *
 * Build:
 *   gcc -o tests/test_component tests/test_component.c \
 *       src/tasks/component_test.c -lm -Isrc/tasks
 *
 * Run:
 *   ./tests/test_component
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "component_test.h"

#define ADC_SCALE (3.3f / 32768.0f)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  [%d] %s ... ", tests_run, name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while (0)

/* Helper: generate constant sample arrays for a known V and I */
static void make_constant_samples(int16_t *v_buf, int16_t *i_buf,
                                  uint16_t n, float volts, float amps)
{
    int16_t v_raw = (int16_t)(volts / ADC_SCALE);
    int16_t i_raw = (int16_t)(amps / ADC_SCALE);
    for (uint16_t i = 0; i < n; i++) {
        v_buf[i] = v_raw;
        i_buf[i] = i_raw;
    }
}

/* Helper: generate a charging RC curve for capacitor testing */
static void make_rc_charge_samples(int16_t *v_buf, int16_t *i_buf,
                                   uint16_t n, float sample_rate,
                                   float v_max, float tau)
{
    for (uint16_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;
        float v = v_max * (1.0f - expf(-t / tau));
        float current = (v_max / 1000.0f) * expf(-t / tau); /* I = V/R * e^(-t/tau) */
        v_buf[i] = (int16_t)(v / ADC_SCALE);
        i_buf[i] = (int16_t)(current / ADC_SCALE);
    }
}

/* ------------------------------------------------------------------ */
/* Test 1: Resistor 4.7K ±5%, measured 4.65K → PASS                   */
/* Uses comp_test_resistor() convenience function for exact values     */
/* ------------------------------------------------------------------ */
static void test_resistor_pass(void)
{
    TEST("Resistor 4.7K +/-5%, measured 4.65K -> PASS");

    bool pass = comp_test_resistor(4650.0f, 4700.0f, 5.0f);
    ASSERT(pass == true, "4650 should be within 5% of 4700");

    /* Also test via comp_test_measure with ADC-friendly values.
     * Use R=100 ohm (V=1.0V, I=0.01V → both have many ADC LSBs).
     * nominal=100, tolerance=5%, measured should be ~100 → PASS */
    comp_test_config_t cfg = {
        .type = COMP_RESISTOR,
        .nominal_ohms = 100.0f,
        .tolerance_pct = 5.0f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 1.0f, 0.01f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);
    ASSERT(result.status == COMP_RESULT_PASS, "100 ohm measure should PASS");
    ASSERT(result.is_pass == true, "expected is_pass=true");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 2: Resistor 4.7K ±5%, measured 5.1K → FAIL (8.5% off)         */
/* ------------------------------------------------------------------ */
static void test_resistor_fail(void)
{
    TEST("Resistor 4.7K +/-5%, measured 5.1K -> FAIL");

    bool pass = comp_test_resistor(5100.0f, 4700.0f, 5.0f);
    ASSERT(pass == false, "5100 should NOT be within 5% of 4700");

    /* Also test via comp_test_measure: nominal=100±5%, measure ~115 → FAIL */
    comp_test_config_t cfg = {
        .type = COMP_RESISTOR,
        .nominal_ohms = 100.0f,
        .tolerance_pct = 5.0f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* R = V/I = 1.15/0.01 = 115 ohms (15% off from 100) */
    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 1.15f, 0.01f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);
    ASSERT(result.status == COMP_RESULT_FAIL, "115 ohm vs 100 nominal should FAIL");
    ASSERT(result.is_pass == false, "expected is_pass=false");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 3: Resistor 4.7K ±5%, measured exactly 4.7K → PASS (0% dev)   */
/* ------------------------------------------------------------------ */
static void test_resistor_exact(void)
{
    TEST("Resistor 4.7K +/-5%, measured exactly 4.7K -> PASS");

    bool pass = comp_test_resistor(4700.0f, 4700.0f, 5.0f);
    ASSERT(pass == true, "4700 should be within 5% of 4700 (0% deviation)");

    /* Also verify deviation is 0 via measure path with R=100 exactly */
    comp_test_config_t cfg = {
        .type = COMP_RESISTOR,
        .nominal_ohms = 100.0f,
        .tolerance_pct = 5.0f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 1.0f, 0.01f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);
    ASSERT(result.status == COMP_RESULT_PASS, "exact match should PASS");
    ASSERT(result.deviation_pct < 2.0f, "deviation should be near 0%");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 4: Continuity: 10 ohms → PASS, 100 ohms → FAIL                */
/* ------------------------------------------------------------------ */
static void test_continuity(void)
{
    TEST("Continuity: 10 ohms -> PASS, 100 ohms -> FAIL");

    comp_test_config_t cfg = { .type = COMP_CONTINUITY };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* 10 ohms: V=0.01V, I=0.001A → R=10 */
    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 0.01f, 0.001f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.is_pass == true, "10 ohm should be continuity PASS");

    /* 100 ohms: V=0.1V, I=0.001A → R=100 */
    make_constant_samples(v, c, 64, 0.1f, 0.001f);
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.is_pass == false, "100 ohm should be continuity FAIL");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 5: Diode Vf=0.65V → silicon, Vf=0.25V → Schottky             */
/* ------------------------------------------------------------------ */
static void test_diode(void)
{
    TEST("Diode: Vf=0.65V silicon, Vf=0.25V Schottky");

    comp_test_config_t cfg = {
        .type = COMP_DIODE,
        .max_vf = 3.5f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* Silicon: 0.65V */
    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 0.65f, 0.0f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.status == COMP_RESULT_PASS, "silicon diode should PASS");
    ASSERT(result.measured_vf > 0.6f && result.measured_vf < 0.7f,
           "silicon Vf should be ~0.65V");

    /* Schottky: 0.25V */
    make_constant_samples(v, c, 64, 0.25f, 0.0f);
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.status == COMP_RESULT_PASS, "Schottky diode should PASS");
    ASSERT(result.measured_vf > 0.2f && result.measured_vf < 0.3f,
           "Schottky Vf should be ~0.25V");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 6: Open circuit → OPEN status                                  */
/* ------------------------------------------------------------------ */
static void test_open_circuit(void)
{
    TEST("Open circuit detection -> OPEN");

    comp_test_config_t cfg = {
        .type = COMP_RESISTOR,
        .nominal_ohms = 4700.0f,
        .tolerance_pct = 5.0f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* Zero current = open circuit */
    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 1.0f, 0.0f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.status == COMP_RESULT_OPEN, "expected OPEN status");
    ASSERT(result.is_pass == false, "open circuit should not pass");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 7: Short circuit → SHORT status                                */
/* ------------------------------------------------------------------ */
static void test_short_circuit(void)
{
    TEST("Short circuit detection -> SHORT");

    comp_test_config_t cfg = {
        .type = COMP_RESISTOR,
        .nominal_ohms = 4700.0f,
        .tolerance_pct = 5.0f,
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* Very low R: V=0.0001V, I=1.0A → R=0.0001 */
    int16_t v[64], c[64];
    make_constant_samples(v, c, 64, 0.0001f, 1.0f);

    comp_test_result_t result;
    comp_test_measure(v, c, 64, 1000.0f, &result);

    ASSERT(result.status == COMP_RESULT_SHORT, "expected SHORT status");
    ASSERT(result.is_pass == false, "short circuit should not pass");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 8: Color bands: 4700 → "Yel-Vio-Red", 10000 → "Brn-Blk-Org"  */
/* ------------------------------------------------------------------ */
static void test_color_bands(void)
{
    TEST("Color bands: 4700 -> Yel-Vio-Red, 10000 -> Brn-Blk-Org");

    const char *bands;

    bands = comp_test_resistor_bands(4700.0f);
    ASSERT(strcmp(bands, "Yel-Vio-Red") == 0,
           "4700 should be Yel-Vio-Red");

    bands = comp_test_resistor_bands(10000.0f);
    ASSERT(strcmp(bands, "Brn-Blk-Org") == 0,
           "10000 should be Brn-Blk-Org");

    PASS();
}

/* ------------------------------------------------------------------ */
/* Test 9: Capacitor: known tau / known R → correct C                  */
/* ------------------------------------------------------------------ */
static void test_capacitor(void)
{
    TEST("Capacitor: known tau/R -> correct C");

    comp_test_config_t cfg = {
        .type = COMP_CAPACITOR,
        .nominal_farads = 0.0f, /* no nominal — just measure */
    };
    comp_test_init();
    comp_test_set_config(&cfg);

    /* RC = 1000 * 1e-6 = 1ms tau, sample at 100kHz */
    float tau = 0.001f;    /* 1ms */
    float sr = 100000.0f;  /* 100kHz */
    uint16_t n = 512;
    int16_t v[512], c[512];
    make_rc_charge_samples(v, c, n, sr, 3.0f, tau);

    comp_test_result_t result;
    comp_test_measure(v, c, n, sr, &result);

    /* Expected C = tau / R_test = 0.001 / 1000 = 1e-6 = 1µF */
    float expected_c = 1e-6f;
    float error_pct = fabsf(result.measured_farads - expected_c) / expected_c * 100.0f;

    ASSERT(result.status == COMP_RESULT_PASS, "capacitor measurement should pass");
    ASSERT(error_pct < 5.0f, "capacitance should be within 5% of 1uF");
    PASS();
}

/* ------------------------------------------------------------------ */
/* Test runner                                                         */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("Component Tester - Unit Tests\n");
    printf("========================================\n");

    test_resistor_pass();
    test_resistor_fail();
    test_resistor_exact();
    test_continuity();
    test_diode();
    test_open_circuit();
    test_short_circuit();
    test_color_bands();
    test_capacitor();

    printf("========================================\n");
    printf("Results: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
