/*
 * Unit tests for DMM auto-selection scoring.
 *
 * Build:
 *   make -C firmware test-meter-auto
 */

#include <stdio.h>
#include <string.h>

#include "fpga_meter_plan.h"
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
    r.expected_frame_family = (uint8_t)fpga_meter_frame_family_for_submode(submode);
    r.observed_frame_family = r.expected_frame_family;
    r.reject_reason = METER_REJECT_NONE;
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

    r = normal_reading(99, 4997);
    r.expected_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    r.observed_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    ASSERT(meter_auto_score(99, &r) == 0);
    return 1;
}

static int test_dirty_frame_family_state_never_scores(void)
{
    meter_reading_t r = normal_reading(2, 2261);

    ASSERT(meter_auto_score(2, &r) == 50);

    r.reject_reason = METER_REJECT_WRONG_FRAME_FAMILY;
    ASSERT(meter_auto_score(2, &r) == 0);

    r = normal_reading(2, 2261);
    r.observed_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE;
    ASSERT(meter_auto_score(2, &r) == 0);

    r = normal_reading(2, 2261);
    r.expected_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE;
    ASSERT(meter_auto_score(2, &r) == 0);
    return 1;
}

static int test_dc_voltage_scores_without_frequency_or_nonzero_magnitude(void)
{
    meter_reading_t r = normal_reading(0, 4997);

    r.aux_freq_hz = 0.0f;
    ASSERT(meter_auto_score(0, &r) == 90);
    r.bcd_value = 0;
    ASSERT(meter_auto_score(0, &r) == 90);
    return 1;
}

static int test_ac_voltage_requires_frequency_evidence(void)
{
    meter_reading_t r = normal_reading(1, 4997);

    r.aux_freq_hz = 0.0f;
    ASSERT(meter_auto_score(1, &r) == 0);
    r.is_ac = true;
    ASSERT(meter_auto_score(1, &r) == 0);
    r.is_ac = false;

    r.aux_freq_hz = 49.9f;
    ASSERT(meter_auto_score(1, &r) == 90);
    r.bcd_value = 0;
    ASSERT(meter_auto_score(1, &r) == 90);

    r.bcd_value = 4997;
    r.aux_freq_hz = 1.0f;
    ASSERT(meter_auto_score(1, &r) == 0);
    r.aux_freq_hz = 44.9f;
    ASSERT(meter_auto_score(1, &r) == 0);
    r.aux_freq_hz = 45.0f;
    ASSERT(meter_auto_score(1, &r) == 90);
    r.aux_freq_hz = 65.0f;
    ASSERT(meter_auto_score(1, &r) == 90);
    r.aux_freq_hz = 66.0f;
    ASSERT(meter_auto_score(1, &r) == 0);
    return 1;
}

static int test_current_auto_scores_respect_ac_evidence(void)
{
    meter_reading_t r = normal_reading(2, 2261);

    ASSERT(meter_auto_score(2, &r) == 50);
    r.bcd_value = 0;
    ASSERT(meter_auto_score(2, &r) == 50);
    r.bcd_value = 2261;
    r.submode = 3;
    ASSERT(meter_auto_score(3, &r) == 50);
    r.submode = 4;
    ASSERT(meter_auto_score(4, &r) == 0);
    r.submode = 5;
    ASSERT(meter_auto_score(5, &r) == 0);

    r.is_ac = true;
    r.submode = 2;
    ASSERT(meter_auto_score(2, &r) == 50);
    r.submode = 3;
    ASSERT(meter_auto_score(3, &r) == 50);
    r.submode = 4;
    ASSERT(meter_auto_score(4, &r) == 0);
    r.submode = 5;
    ASSERT(meter_auto_score(5, &r) == 0);

    r = normal_reading(4, 2261);
    r.aux_freq_hz = 49.0f;
    ASSERT(meter_auto_score(4, &r) == 50);
    r.bcd_value = 0;
    ASSERT(meter_auto_score(4, &r) == 50);
    r.submode = 5;
    ASSERT(meter_auto_score(5, &r) == 50);

    r = normal_reading(4, 2261);
    r.aux_freq_hz = 1.0f;
    ASSERT(meter_auto_score(4, &r) == 0);
    r.aux_freq_hz = 44.9f;
    ASSERT(meter_auto_score(4, &r) == 0);
    r.aux_freq_hz = 45.0f;
    ASSERT(meter_auto_score(4, &r) == 50);
    r.aux_freq_hz = 65.0f;
    ASSERT(meter_auto_score(4, &r) == 50);
    r.aux_freq_hz = 66.0f;
    ASSERT(meter_auto_score(4, &r) == 0);
    r.submode = 5;
    ASSERT(meter_auto_score(5, &r) == 0);
    r.aux_freq_hz = 49.0f;
    ASSERT(meter_auto_score(5, &r) == 50);
    return 1;
}

static int test_unresolved_microamp_functions_are_not_autoscan_candidates(void)
{
    size_t count = 0;
    const uint8_t *candidates = meter_auto_candidates(&count);
    uint8_t dc_ua =
        fpga_meter_submode_for_logical_function(FPGA_METER_FUNCTION_DC_UA);
    uint8_t ac_ua =
        fpga_meter_submode_for_logical_function(FPGA_METER_FUNCTION_AC_UA);
    meter_reading_t r;

    ASSERT(dc_ua == FPGA_METER_INVALID_LOCAL_SUBMODE);
    ASSERT(ac_ua == FPGA_METER_INVALID_LOCAL_SUBMODE);
    ASSERT(fpga_meter_logical_function_is_unresolved(FPGA_METER_FUNCTION_DC_UA));
    ASSERT(fpga_meter_logical_function_is_unresolved(FPGA_METER_FUNCTION_AC_UA));

    for (size_t i = 0; i < count; i++) {
        ASSERT(candidates[i] != dc_ua);
        ASSERT(candidates[i] != ac_ua);
        ASSERT(fpga_meter_submode_is_valid(candidates[i]));
    }

    r = normal_reading(dc_ua, 2261);
    r.expected_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    r.observed_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    ASSERT(meter_auto_score(dc_ua, &r) == 0);

    r = normal_reading(ac_ua, 2261);
    r.expected_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    r.observed_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID;
    r.aux_freq_hz = 49.0f;
    ASSERT(meter_auto_score(ac_ua, &r) == 0);
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
    continuity.expected_frame_family =
        (uint8_t)fpga_meter_frame_family_for_submode(7);
    continuity.observed_frame_family = continuity.expected_frame_family;

    ASSERT(meter_auto_score(6, &resistance) == 70);
    ASSERT(meter_auto_score(7, &continuity) == 80);
    return 1;
}

int main(void)
{
    printf("Meter auto-selection tests\n");

    TEST(candidate_order_keeps_voltage_before_passive_and_current);
    TEST(wrong_submode_never_scores);
    TEST(dirty_frame_family_state_never_scores);
    TEST(dc_voltage_scores_without_frequency_or_nonzero_magnitude);
    TEST(ac_voltage_requires_frequency_evidence);
    TEST(current_auto_scores_respect_ac_evidence);
    TEST(unresolved_microamp_functions_are_not_autoscan_candidates);
    TEST(temperature_scores_as_passive_candidate);
    TEST(continuity_marker_beats_resistance_normal);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
