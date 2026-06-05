/*
 * Unit tests for DMM auto-selection scoring.
 *
 * Build:
 *   make -C firmware test-meter-auto
 */

#include <stdio.h>
#include <string.h>

#include "meter_auto.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-48s ", #name); \
    if (test_##name()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAIL\n"); } \
} while (0)

#define ASSERT(cond) do { \
    if (!(cond)) { printf("[line %d: %s] ", __LINE__, #cond); return 0; } \
} while (0)

static meter_reading_t normal_reading(uint8_t submode, int bcd_value)
{
    meter_reading_t r;

    memset(&r, 0, sizeof(r));
    r.valid = true;
    r.submode = submode;
    r.bcd_value = bcd_value;
    r.result_class = METER_RESULT_NORMAL;
    return r;
}

static int test_candidate_order_keeps_voltage_before_passive_and_current(void)
{
    size_t count = 0;
    const uint8_t *candidates = meter_auto_candidates(&count);
    static const uint8_t expected[] = { 0, 1, 6, 7, 8, 9, 10, 2, 4, 3, 5 };

    ASSERT(count == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < count; i++) {
        ASSERT(candidates[i] == expected[i]);
    }
    return 1;
}

static int test_wrong_submode_never_scores(void)
{
    meter_reading_t r = normal_reading(0, 4997);

    ASSERT(meter_auto_score(1, &r) == 0);
    ASSERT(meter_auto_score(2, &r) == 0);
    return 1;
}

static int test_dc_voltage_scores_without_frequency(void)
{
    meter_reading_t r = normal_reading(0, 4997);

    r.aux_freq_hz = 0.0f;
    ASSERT(meter_auto_score(0, &r) == 90);
    return 1;
}

static int test_ac_voltage_requires_frequency_evidence(void)
{
    meter_reading_t r = normal_reading(1, 4997);

    r.aux_freq_hz = 0.0f;
    ASSERT(meter_auto_score(1, &r) == 0);

    r.aux_freq_hz = 49.9f;
    ASSERT(meter_auto_score(1, &r) == 90);
    return 1;
}

static int test_current_scores_below_voltage(void)
{
    meter_reading_t r = normal_reading(2, 2261);

    ASSERT(meter_auto_score(2, &r) == 50);
    r.submode = 5;
    ASSERT(meter_auto_score(5, &r) == 50);
    return 1;
}

static int test_temperature_scores_as_passive_candidate(void)
{
    meter_reading_t r = normal_reading(10, 248);

    ASSERT(meter_auto_score(10, &r) == 60);
    return 1;
}

static int test_continuity_marker_beats_resistance_normal(void)
{
    meter_reading_t continuity;
    meter_reading_t resistance = normal_reading(6, 100);

    memset(&continuity, 0, sizeof(continuity));
    continuity.valid = true;
    continuity.submode = 7;
    continuity.result_class = METER_RESULT_CONTINUITY;

    ASSERT(meter_auto_score(6, &resistance) == 70);
    ASSERT(meter_auto_score(7, &continuity) == 80);
    return 1;
}

int main(void)
{
    printf("Meter auto-selection tests\n");

    TEST(candidate_order_keeps_voltage_before_passive_and_current);
    TEST(wrong_submode_never_scores);
    TEST(dc_voltage_scores_without_frequency);
    TEST(ac_voltage_requires_frequency_evidence);
    TEST(current_scores_below_voltage);
    TEST(temperature_scores_as_passive_candidate);
    TEST(continuity_marker_beats_resistance_normal);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
