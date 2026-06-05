/*
 * Unit tests for DMM USART frame decoding.
 *
 * Build:
 *   make -C firmware test-meter-data
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "meter_data.h"

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

#define ASSERT_STR_EQ(actual, expected) do { \
    if (strcmp((actual), (expected)) != 0) { \
        printf("[line %d: expected \"%s\", got \"%s\"] ", \
               __LINE__, (expected), (actual)); \
        return 0; \
    } \
} while (0)

static int close_to(float actual, float expected, float tolerance)
{
    float delta = actual - expected;
    if (delta < 0.0f) delta = -delta;
    return delta <= tolerance;
}

static int expect_normal_reading(const char *display,
                                 const char *unit,
                                 float value,
                                 float tolerance)
{
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, display);
    ASSERT_STR_EQ(meter_reading.unit_suffix, unit);
    ASSERT(close_to(meter_reading.value, value, tolerance));
    return 1;
}

static void process_frame(const uint8_t frame[12], uint8_t submode)
{
    meter_data_process_frame((const volatile uint8_t *)frame, submode);
}

static uint8_t segment_nibble_for_code(uint8_t code)
{
    switch (code) {
    case 0: return 0xEB;
    case 1: return 0x0A;
    case 2: return 0xAD;
    case 3: return 0x8F;
    case 4: return 0x4E;
    case 5: return 0xC7;
    case 6: return 0xE7;
    case 7: return 0x8A;
    case 8: return 0xEF;
    case 9: return 0xCF;
    case 0x0A: return 0xEE;  /* OL high / continuity marker */
    case 0x10: return 0x00;  /* blank */
    case 0x11: return 0xE1;  /* partial blank */
    case 0x12: return 0xEC;  /* continuity */
    case 0xFF: return 0x01;  /* invalid/unmapped */
    default: return 0x01;
    }
}

static void build_segment_frame(uint8_t frame[12],
                                uint8_t digit0_code,
                                uint8_t digit1_code,
                                uint8_t digit2_code,
                                uint8_t digit3_code,
                                uint8_t frame6_high,
                                uint8_t status,
                                uint8_t meas_flags,
                                uint8_t additional_status,
                                uint16_t extra)
{
    uint8_t n0 = segment_nibble_for_code(digit0_code);
    uint8_t n1 = segment_nibble_for_code(digit1_code);
    uint8_t n2 = segment_nibble_for_code(digit2_code);
    uint8_t n3 = segment_nibble_for_code(digit3_code);

    memset(frame, 0, 12);
    frame[0] = 0x5A;
    frame[1] = 0xA5;
    frame[2] = n0 & 0xF0;
    frame[3] = (n1 & 0xF0) | (n0 & 0x0F);
    frame[4] = (n2 & 0xF0) | (n1 & 0x0F);
    frame[5] = (n3 & 0xF0) | (n2 & 0x0F);
    frame[6] = (frame6_high & 0xF0) | (n3 & 0x0F);
    frame[7] = status;
    frame[8] = meas_flags;
    frame[9] = additional_status;
    frame[10] = (uint8_t)(extra >> 8);
    frame[11] = (uint8_t)extra;
}

static int test_segment_frame_builder_exercises_cross_byte_lookup(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x014E);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.raw_bcd == 5008);
    ASSERT(meter_reading.dbg_nibbles[0] == 0xC7);
    ASSERT(meter_reading.dbg_nibbles[1] == 0xEB);
    ASSERT(meter_reading.dbg_nibbles[2] == 0xEB);
    ASSERT(meter_reading.dbg_nibbles[3] == 0xEF);
    ASSERT(meter_reading.dbg_raw_digits[0] == 5);
    ASSERT(meter_reading.dbg_raw_digits[1] == 0);
    ASSERT(meter_reading.dbg_raw_digits[2] == 0);
    ASSERT(meter_reading.dbg_raw_digits[3] == 8);
    return 1;
}

static int test_dcv_5v_frame_keeps_verified_decimal_and_unit(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.raw_bcd == 5008);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "5.008");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 5.008f, 0.001f));
    return 1;
}

static int test_dcv_5v_synthetic_f6_0f_does_not_become_mains_range(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x014E);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.raw_bcd == 5008);
    ASSERT(meter_reading.dbg_frame[6] == 0x0F);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "5.008");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 5.008f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    return 1;
}

static int test_dcv_7v_f6_07_keeps_default_volt_scale(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 7, 0, 0, 5, 0x00, 0x00, 0x00, 0x00, 0);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.raw_bcd == 7005);
    ASSERT(meter_reading.dbg_frame[6] == 0x07);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "7.005");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 7.005f, 0.001f));
    return 1;
}

static int test_dcv_range_frames_are_not_latched_from_acv_mains(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };

    meter_data_init();
    process_frame(mains_frame, 0);
    ASSERT(expect_normal_reading("228.3", "V", 228.3f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));

    process_frame(dcv_frame, 0);
    ASSERT(meter_reading.raw_bcd == 5008);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(expect_normal_reading("5.008", "V", 5.008f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    return 1;
}

static int test_acv_mains_frame_uses_high_voltage_scale_and_frequency(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };

    meter_data_init();
    process_frame(frame, 1);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.raw_bcd == 2282);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT_STR_EQ(meter_reading.display_str, "228.2");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 228.2f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    return 1;
}

static int test_stock_formatter_families_have_regression_fixtures(void)
{
    uint8_t frame[12];

    meter_data_init();
    build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 2);
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(expect_normal_reading("12.34", "mA", 12.34f, 0.001f));

    process_frame(frame, 3);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(expect_normal_reading("1.234", "A", 1.234f, 0.001f));

    process_frame(frame, 4);
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(expect_normal_reading("12.34", "mA", 12.34f, 0.001f));

    process_frame(frame, 5);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(expect_normal_reading("1.234", "A", 1.234f, 0.001f));

    build_segment_frame(frame, 6, 7, 8, 9, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 8);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT(expect_normal_reading("678.9", "V", 678.9f, 0.001f));

    build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 9);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT(expect_normal_reading("123.4", "nF", 123.4f, 0.001f));
    return 1;
}

static int test_resistance_band_overrides_have_regression_fixtures(void)
{
    uint8_t frame[12];

    meter_data_init();
    build_segment_frame(frame, 4, 8, 2, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 6);
    ASSERT_STR_EQ(meter_reading.unit_suffix, "Ohm");
    ASSERT(close_to(meter_reading.value, 146.6496f, 0.001f));
    ASSERT_STR_EQ(meter_reading.display_str, "146.6");

    build_segment_frame(frame, 3, 3, 0, 0, 0x40, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 6);
    ASSERT_STR_EQ(meter_reading.unit_suffix, "kOhm");
    ASSERT(close_to(meter_reading.value, 3.300f, 0.001f));
    ASSERT_STR_EQ(meter_reading.display_str, "3.300");
    return 1;
}

static int test_voltage_mode_mains_frequency_frame_overrides_dcv_0f_rule(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.raw_bcd == 2283);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT_STR_EQ(meter_reading.display_str, "228.3");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 228.3f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    return 1;
}

static int test_voltage_mode_mains_rotating_frames_stay_high_voltage(void)
{
    static const uint8_t frames[][12] = {
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xF7, 0x07, 0x00, 0x02, 0x00, 0x00, 0x32 },
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x97, 0x0A, 0x00, 0x02, 0x00, 0x00, 0x32 },
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x5F, 0x0E, 0x00, 0x02, 0x00, 0x00, 0x31 },
    };

    meter_data_init();
    for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        process_frame(frames[i], 0);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.decimal_pos == 3);
        ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
        ASSERT(meter_reading.value > 200.0f);
        ASSERT(meter_reading.value < 260.0f);
        ASSERT(meter_reading.aux_freq_hz >= 49.0f);
        ASSERT(meter_reading.aux_freq_hz <= 50.0f);
    }
    return 1;
}

static int test_acv_repeated_rotating_range_frames_do_not_drop_to_2v(void)
{
    static const uint8_t frames[][12] = {
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x0F, 0x0A, 0x00, 0x02, 0x00, 0x00, 0x31 },
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF, 0x0D, 0x00, 0x02, 0x00, 0x00, 0x31 },
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x5F, 0x0E, 0x00, 0x02, 0x00, 0x00, 0x31 },
        { 0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xEF, 0x0F, 0x00, 0x02, 0x00, 0x00, 0x32 },
    };

    meter_data_init();
    for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        process_frame(frames[i], 1);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.decimal_pos == 3);
        ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
        ASSERT(meter_reading.value > 200.0f);
        ASSERT(meter_reading.value < 260.0f);
    }
    return 1;
}

static int test_continuity_frame_sets_beep_from_segment_pattern(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 0, 0x12, 0x0A, 5, 0x00, 0x00, 0x00, 0x00, 0);
    meter_data_init();
    process_frame(frame, 7);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_CONTINUITY);
    ASSERT(meter_reading.continuity_beep);
    ASSERT(meter_reading.dbg_raw_digits[0] == 0);
    ASSERT(meter_reading.dbg_raw_digits[1] == 0x12);
    ASSERT(meter_reading.dbg_raw_digits[2] == 0x0A);
    ASSERT(meter_reading.dbg_raw_digits[3] == 5);
    return 1;
}

static int test_non_continuity_terminal_frames_clear_stale_beep(void)
{
    uint8_t continuity[12];
    uint8_t partial_blank[12];
    uint8_t invalid[12];
    uint8_t normal[12];

    build_segment_frame(continuity, 0, 0x12, 0x0A, 5, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(partial_blank, 0x10, 0x11, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(invalid, 0xFF, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(normal, 0, 0, 1, 0, 0x00, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(continuity, 7);
    ASSERT(meter_reading.continuity_beep);
    process_frame(partial_blank, 7);
    ASSERT(meter_reading.result_class == METER_RESULT_BLANK);
    ASSERT(!meter_reading.continuity_beep);

    process_frame(continuity, 7);
    ASSERT(meter_reading.continuity_beep);
    process_frame(invalid, 7);
    ASSERT(meter_reading.result_class == METER_RESULT_INVALID);
    ASSERT(!meter_reading.continuity_beep);

    process_frame(continuity, 7);
    ASSERT(meter_reading.continuity_beep);
    process_frame(normal, 7);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(!meter_reading.continuity_beep);
    return 1;
}

static int test_special_frames_clear_stale_aux_frequency(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    uint8_t partial_blank[12];
    uint8_t invalid[12];
    uint8_t continuity[12];

    build_segment_frame(partial_blank, 0x10, 0x11, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(invalid, 0xFF, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(continuity, 0, 0x12, 0x0A, 5, 0x00, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(mains_frame, 1);
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    process_frame(partial_blank, 0);
    ASSERT(meter_reading.result_class == METER_RESULT_BLANK);
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));

    process_frame(mains_frame, 1);
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    process_frame(invalid, 0);
    ASSERT(meter_reading.result_class == METER_RESULT_INVALID);
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));

    process_frame(mains_frame, 1);
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    process_frame(continuity, 7);
    ASSERT(meter_reading.result_class == METER_RESULT_CONTINUITY);
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    return 1;
}

int main(void)
{
    printf("Meter data frame tests\n");

    TEST(segment_frame_builder_exercises_cross_byte_lookup);
    TEST(dcv_5v_frame_keeps_verified_decimal_and_unit);
    TEST(dcv_5v_synthetic_f6_0f_does_not_become_mains_range);
    TEST(dcv_7v_f6_07_keeps_default_volt_scale);
    TEST(dcv_range_frames_are_not_latched_from_acv_mains);
    TEST(acv_mains_frame_uses_high_voltage_scale_and_frequency);
    TEST(stock_formatter_families_have_regression_fixtures);
    TEST(resistance_band_overrides_have_regression_fixtures);
    TEST(voltage_mode_mains_frequency_frame_overrides_dcv_0f_rule);
    TEST(voltage_mode_mains_rotating_frames_stay_high_voltage);
    TEST(acv_repeated_rotating_range_frames_do_not_drop_to_2v);
    TEST(continuity_frame_sets_beep_from_segment_pattern);
    TEST(non_continuity_terminal_frames_clear_stale_beep);
    TEST(special_frames_clear_stale_aux_frequency);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
