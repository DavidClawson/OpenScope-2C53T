/*
 * Unit tests for DMM USART frame decoding.
 *
 * Build:
 *   make -C firmware test-meter-data
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fpga_meter_plan.h"
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

static int expect_payload_cleared(const char *display)
{
    ASSERT_STR_EQ(meter_reading.display_str, display);
    ASSERT_STR_EQ(meter_reading.unit_suffix, "");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    ASSERT(meter_reading.bcd_value == 0);
    ASSERT(meter_reading.decimal_pos == 0);
    ASSERT(meter_reading.unit_variant == 0);
    ASSERT(meter_reading.digits[0] == 0);
    ASSERT(meter_reading.digits[1] == 0);
    ASSERT(meter_reading.digits[2] == 0);
    ASSERT(meter_reading.digits[3] == 0);
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    ASSERT(!meter_reading.continuity_beep);
    return 1;
}

static int expect_family_debug(uint8_t expected, uint8_t observed,
                               uint8_t reject_reason)
{
    ASSERT(meter_reading.expected_frame_family == expected);
    ASSERT(meter_reading.observed_frame_family == observed);
    ASSERT(meter_reading.reject_reason == reject_reason);
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
    case 0x0B: return 0x23;  /* OL low */
    case 0x0C: return 0x65;  /* special */
    case 0x0D: return 0x27;  /* special */
    case 0x0E: return 0x61;  /* special */
    case 0x0F: return 0x04;  /* special */
    case 0x10: return 0x00;  /* blank */
    case 0x11: return 0xE1;  /* partial blank */
    case 0x12: return 0xEC;  /* continuity */
    case 0x13: return 0xE5;  /* mode-change/special */
    case 0x14: return 0x24;  /* special */
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

static bool test_frame_has_voltage_payload_marker(const uint8_t frame[12])
{
    if (frame[9] != 0x00) {
        return false;
    }
    return (frame[8] & 0x7FU) == 0x02U;
}

static bool test_raw_digits_are_continuity_marker(const uint8_t raw_digits[4])
{
    return raw_digits[1] == 0x12 && raw_digits[2] == 0x0A &&
           raw_digits[3] == 5;
}

static void build_valid_frame_for_mode(uint8_t frame[12], uint8_t mode);

static int test_segment_frame_builder_exercises_cross_byte_lookup(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x014E);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 5008);
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
    ASSERT(meter_reading.bcd_value == 5008);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "5.008");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 5.008f, 0.001f));
    return 1;
}

static int test_dcv_live_5v_frame_uses_stock_range_hint(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0x46, 0xDE, 0xCF, 0x4F,
        0x0E, 0x00, 0x02, 0x00, 0x01, 0x82,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 4994);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "4.994");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 4.994f, 0.001f));
    return 1;
}

static int test_dcv_live_32v_frame_uses_stock_range_hint(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0x86, 0x0F, 0xDA, 0xEF,
        0x07, 0x00, 0x02, 0x00, 0x03, 0xFF,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 3196);
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT_STR_EQ(meter_reading.display_str, "31.96");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 31.96f, 0.01f));
    return 1;
}

static int test_dcv_1v2949_frame_uses_stock_extended_raw_and_class4(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 2, 9, 4, 9, 0x00, 0x00, 0x82, 0x00, 0);
    frame[2] |= 0x08;
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.bcd_value == 12949);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "1.2949");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 1.2949f, 0.0001f));
    ASSERT((meter_reading.dbg_frame[2] & 0x08U) != 0);
    ASSERT(meter_reading.dbg_frame[8] == 0x82);
    return 1;
}

static int test_dcv_1v4979_frame_uses_stock_extended_raw_and_class4(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 4, 9, 7, 9, 0x00, 0x00, 0x82, 0x00, 0);
    frame[2] |= 0x08;
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.bcd_value == 14979);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "1.4979");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 1.4979f, 0.0001f));
    ASSERT((meter_reading.dbg_frame[2] & 0x08U) != 0);
    ASSERT(meter_reading.dbg_frame[8] == 0x82);
    return 1;
}

static int test_dcv_live_1v5_frame_uses_stock_extended_raw_and_class4(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.bcd_value == 14977);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT_STR_EQ(meter_reading.display_str, "1.4977");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 1.4977f, 0.0001f));
    ASSERT((meter_reading.dbg_frame[2] & 0x08U) != 0);
    ASSERT(meter_reading.dbg_frame[8] == 0x82);
    return 1;
}

static int test_dcv_live_1v5_rotating_frames_keep_stock_class4(void)
{
    static const uint8_t frames[][12] = {
        { 0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0xEA, 0x0F, 0x00, 0x82, 0x00, 0x01, 0x7F },
        { 0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A, 0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F },
        { 0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0xEA, 0x07, 0x00, 0x82, 0x00, 0x01, 0x7F },
    };

    meter_data_init();
    for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        process_frame(frames[i], 0);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.decimal_pos == 1);
        ASSERT(strncmp(meter_reading.display_str, "1.497", 5) == 0);
        ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
        ASSERT(meter_reading.value > 1.4975f);
        ASSERT(meter_reading.value < 1.4979f);
        ASSERT(meter_reading.bcd_value >= 14976);
        ASSERT(meter_reading.bcd_value <= 14978);
        ASSERT((meter_reading.dbg_frame[2] & 0x08U) != 0);
        ASSERT(meter_reading.dbg_frame[8] == 0x82);
    }
    return 1;
}

static int test_dcv_live_0200_bare_class_bit_fails_closed(void)
{
    /*
     * Live visual check on 2026-06-05: the PSU/load display showed 0.200 V,
     * while this bare-class-bit DCV frame rendered 0.4366 V. Good low-DCV
     * frames recovered so far use frame[8]=0x82; this frame only has the
     * exponent bit 0x80. The exponent bit is not enough voltage-family proof,
     * so the parser fails closed instead of turning producer-state leakage into
     * a confident wrong voltage.
     */
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x07, 0x24, 0x80, 0x00, 0x01, 0x89,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NONE);
    ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
    ASSERT(expect_payload_cleared("---"));
    ASSERT((meter_reading.dbg_frame[8] & 0x7FU) == 0);
    ASSERT((meter_reading.dbg_frame[8] & 0x80U) != 0);
    return 1;
}

static int test_dcv_stock_range_class_priority_table(void)
{
    struct range_case {
        uint8_t frame3_or;
        uint8_t frame4_or;
        uint8_t frame5_or;
        uint8_t frame8_or;
        int bcd;
        const char *display;
        float value;
    };
    static const struct range_case cases[] = {
        { 0x00, 0x00, 0x00, 0x00, 1234, "1234",   1234.0f },
        { 0x00, 0x00, 0x10, 0x00, 1234, "123.4",   123.4f },
        { 0x00, 0x10, 0x10, 0x00, 1234, "12.34",    12.34f },
        { 0x10, 0x10, 0x10, 0x00, 1234, "1.234",     1.234f },
        { 0x10, 0x10, 0x10, 0x80, 1234, "0.1234",    0.1234f },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t frame[12];

        build_segment_frame(frame, 1, 2, 3, 4,
                            0x00, 0x00, 0x02, 0x00, 0);
        frame[3] |= cases[i].frame3_or;
        frame[4] |= cases[i].frame4_or;
        frame[5] |= cases[i].frame5_or;
        frame[8] |= cases[i].frame8_or;

        meter_data_init();
        process_frame(frame, 0);

        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
        ASSERT(meter_reading.bcd_value == cases[i].bcd);
        ASSERT_STR_EQ(meter_reading.display_str, cases[i].display);
        ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
        ASSERT(close_to(meter_reading.value, cases[i].value, 0.0002f));
    }
    return 1;
}

static int test_dcv_stock_range_class_priority_all_bit_combinations(void)
{
    static const char *display_no_extend[] = {
        "1234", "123.4", "12.34", "1.234", "0.1234"
    };
    static const char *display_extend[] = {
        "11234", "1123.4", "112.34", "11.234", "1.1234"
    };
    static const float expected_no_extend[] = {
        1234.0f, 123.4f, 12.34f, 1.234f, 0.1234f
    };
    static const float expected_extend[] = {
        11234.0f, 1123.4f, 112.34f, 11.234f, 1.1234f
    };

    for (uint8_t bits = 0; bits < 16; bits++) {
        for (uint8_t extend = 0; extend < 2; extend++) {
            uint8_t frame[12];
            uint8_t expected_class =
                (bits & 0x8U) ? 4U :
                (bits & 0x4U) ? 3U :
                (bits & 0x2U) ? 2U :
                (bits & 0x1U) ? 1U : 0U;

            build_segment_frame(frame, 1, 2, 3, 4,
                                0x00, 0x00, 0x02, 0x00, 0);
            if (bits & 0x1U) frame[5] |= 0x10U;
            if (bits & 0x2U) frame[4] |= 0x10U;
            if (bits & 0x4U) frame[3] |= 0x10U;
            if (bits & 0x8U) frame[8] |= 0x80U;
            if (extend) frame[2] |= 0x08U;

            meter_data_init();
            process_frame(frame, 0);

            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
            ASSERT(meter_reading.bcd_value == (extend ? 11234 : 1234));
            ASSERT_STR_EQ(meter_reading.display_str,
                          extend ? display_extend[expected_class]
                                 : display_no_extend[expected_class]);
            ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
            ASSERT(close_to(meter_reading.value,
                            extend ? expected_extend[expected_class]
                                   : expected_no_extend[expected_class],
                            0.0003f));
        }
    }
    return 1;
}

static int test_dcv_class4_priority_requires_frame8_bit7(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x02, 0x00, 0x01, 0x7F,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 14977);
    ASSERT(meter_reading.decimal_pos == 0);
    ASSERT_STR_EQ(meter_reading.display_str, "14977");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 14977.0f, 0.001f));
    ASSERT((meter_reading.dbg_frame[2] & 0x08U) != 0);
    ASSERT(meter_reading.dbg_frame[8] == 0x02);
    return 1;
}

static int test_dcv_synthetic_5008_without_class_bits_stays_class0(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x014E);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 5008);
    ASSERT(meter_reading.dbg_frame[6] == 0x0F);
    ASSERT(meter_reading.decimal_pos == 0);
    ASSERT_STR_EQ(meter_reading.display_str, "5008");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 5008.0f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    return 1;
}

static int test_dcv_extra_frequency_hint_does_not_set_voltage_range(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x0031);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 5008);
    ASSERT(meter_reading.decimal_pos == 0);
    ASSERT_STR_EQ(meter_reading.display_str, "5008");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 5008.0f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    return 1;
}

static int test_dcv_aux_extra_bytes_do_not_change_stock_range_class(void)
{
    static const char *display_no_extend[] = {
        "5008", "500.8", "50.08", "5.008", "0.5008"
    };
    static const char *display_extend[] = {
        "15008", "1500.8", "150.08", "15.008", "1.5008"
    };
    static const float expected_no_extend[] = {
        5008.0f, 500.8f, 50.08f, 5.008f, 0.5008f
    };
    static const float expected_extend[] = {
        15008.0f, 1500.8f, 150.08f, 15.008f, 1.5008f
    };
    static const uint16_t extra_cases[] = {
        0x0000, 0x0031, 0x014E, 0x017F, 0x03FF, 0xFFFF
    };

    for (uint8_t bits = 0; bits < 16; bits++) {
        uint8_t expected_class =
            (bits & 0x8U) ? 4U :
            (bits & 0x4U) ? 3U :
            (bits & 0x2U) ? 2U :
            (bits & 0x1U) ? 1U : 0U;

        for (uint8_t extend = 0; extend < 2; extend++) {
            for (unsigned i = 0; i < sizeof(extra_cases) / sizeof(extra_cases[0]); i++) {
                uint8_t frame[12];

                build_segment_frame(frame, 5, 0, 0, 8,
                                    0x00, 0x00, 0x02, 0x00, extra_cases[i]);
                if (bits & 0x1U) frame[5] |= 0x10U;
                if (bits & 0x2U) frame[4] |= 0x10U;
                if (bits & 0x4U) frame[3] |= 0x10U;
                if (bits & 0x8U) frame[8] |= 0x80U;
                if (extend) frame[2] |= 0x08U;

                meter_data_init();
                process_frame(frame, 0);

                ASSERT(meter_reading.valid);
                ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
                ASSERT(meter_reading.bcd_value == (extend ? 15008 : 5008));
                ASSERT_STR_EQ(meter_reading.display_str,
                              extend ? display_extend[expected_class]
                                     : display_no_extend[expected_class]);
                ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
                ASSERT(close_to(meter_reading.value,
                                extend ? expected_extend[expected_class]
                                       : expected_no_extend[expected_class],
                                0.0003f));
                ASSERT(meter_reading.dbg_frame[10] == (uint8_t)(extra_cases[i] >> 8));
                ASSERT(meter_reading.dbg_frame[11] == (uint8_t)extra_cases[i]);
            }
        }
    }
    return 1;
}

static int test_dcv_7005_without_class_bits_stays_class0(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 7, 0, 0, 5, 0x00, 0x00, 0x02, 0x00, 0);
    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.bcd_value == 7005);
    ASSERT(meter_reading.dbg_frame[6] == 0x07);
    ASSERT(meter_reading.decimal_pos == 0);
    ASSERT_STR_EQ(meter_reading.display_str, "7005");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 7005.0f, 0.001f));
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
    ASSERT(meter_reading.bcd_value == 5008);
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
    ASSERT(meter_reading.bcd_value == 2282);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT_STR_EQ(meter_reading.display_str, "228.2");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 228.2f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    return 1;
}

static int test_acv_rejects_dc_voltage_without_ac_evidence(void)
{
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };

    meter_data_init();
    process_frame(dcv_frame, 1);

    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 1);
    ASSERT(meter_reading.result_class == METER_RESULT_NONE);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               METER_REJECT_MISSING_AC_EVIDENCE));
    return 1;
}

static int test_acv_rejects_live_low_dcv_status24_without_frequency_hint(void)
{
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x0F, 0x24, 0x80, 0x00, 0x01, 0x8B,
    };

    meter_data_init();
    process_frame(dcv_frame, 1);

    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 1);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               METER_REJECT_WRONG_FRAME_FAMILY));
    return 1;
}

static int test_ac_current_rejects_current_frame_without_ac_evidence(void)
{
    uint8_t current_frame[12];

    build_segment_frame(current_frame, 2, 2, 6, 1,
                        0x00, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(current_frame, 4);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 4);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               METER_REJECT_MISSING_AC_EVIDENCE));

    meter_data_init();
    process_frame(current_frame, 5);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 5);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               METER_REJECT_MISSING_AC_EVIDENCE));
    return 1;
}

static int test_ac_modes_require_frequency_hint_boundaries(void)
{
    static const uint8_t ac_modes[] = { 1, 4, 5 };
    static const uint8_t statuses[] = { 0x00, 0x01, 0x04, 0x24, 0xFF };
    static const uint16_t extras[] = { 0x0000, 0x002C, 0x002D, 0x0041, 0x0042 };

    for (unsigned m = 0; m < sizeof(ac_modes); m++) {
        uint8_t mode = ac_modes[m];

        for (unsigned s = 0; s < sizeof(statuses); s++) {
            for (unsigned e = 0; e < sizeof(extras) / sizeof(extras[0]); e++) {
                uint8_t frame[12];
                bool has_frequency_hint = extras[e] >= 45U && extras[e] <= 65U;

                if (mode == 1) {
                    build_segment_frame(frame, 2, 2, 8, 2,
                                        0x00, statuses[s], 0x02, 0x00, extras[e]);
                } else {
                    build_segment_frame(frame, 2, 2, 6, 1,
                                        0x00, statuses[s], 0x00, 0x00, extras[e]);
                }

                meter_data_init();
                process_frame(frame, mode);

                ASSERT(meter_reading.submode == mode);
                ASSERT(meter_reading.is_ac == ((statuses[s] & 0x04U) != 0));
                if (has_frequency_hint) {
                    ASSERT(meter_reading.valid);
                    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
                    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
                    ASSERT(close_to(meter_reading.aux_freq_hz,
                                    (float)extras[e], 0.001f));
                } else {
                    ASSERT(!meter_reading.valid);
                    ASSERT(meter_reading.result_class == METER_RESULT_NONE);
                    ASSERT(expect_payload_cleared("---"));
                    ASSERT(meter_reading.reject_reason ==
                           METER_REJECT_MISSING_AC_EVIDENCE);
                }
            }
        }
    }
    return 1;
}

static int test_stock_formatter_families_have_regression_fixtures(void)
{
    uint8_t frame[12];
    uint8_t ac_current_frame[12];

    meter_data_init();
    build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 1, 2, 3, 4,
                        0x00, 0x00, 0x00, 0x00, 0x0031);
    process_frame(frame, 2);
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(expect_normal_reading("12.34", "mA", 12.34f, 0.001f));

    process_frame(frame, 3);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(expect_normal_reading("1.234", "A", 1.234f, 0.001f));

    process_frame(ac_current_frame, 4);
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(expect_normal_reading("12.34", "mA", 12.34f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));

    process_frame(ac_current_frame, 5);
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(expect_normal_reading("1.234", "A", 1.234f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));

    build_segment_frame(frame, 6, 7, 8, 9, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 8);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT(expect_normal_reading("678.9", "V", 678.9f, 0.001f));

    build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 9);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT(expect_normal_reading("123.4", "nF", 123.4f, 0.001f));

    process_frame(frame, 10);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT(expect_normal_reading("123.4", "C", 123.4f, 0.001f));
    return 1;
}

static int test_passive_formatter_debug_fields_cover_diode_and_extended_splits(void)
{
    uint8_t frame[12];

    meter_data_init();
    build_segment_frame(frame, 6, 7, 8, 9, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 8);
    ASSERT(expect_normal_reading("678.9", "V", 678.9f, 0.001f));
    ASSERT(meter_reading.expected_frame_family ==
           (uint8_t)FPGA_METER_FRAME_FAMILY_DIODE);
    ASSERT(meter_reading.observed_frame_family ==
           meter_reading.expected_frame_family);
    ASSERT(meter_reading.stock_mode == 7);
    ASSERT(meter_reading.stock_unit_index == 8);
    ASSERT(meter_reading.stock_composite_index == 12);

    build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 9);
    ASSERT(expect_normal_reading("123.4", "nF", 123.4f, 0.001f));
    ASSERT(meter_reading.expected_frame_family ==
           (uint8_t)FPGA_METER_FRAME_FAMILY_EXTENDED);
    ASSERT(meter_reading.observed_frame_family ==
           meter_reading.expected_frame_family);
    ASSERT(meter_reading.stock_mode == 5);
    ASSERT(meter_reading.stock_unit_index == 7);
    ASSERT(meter_reading.stock_composite_index == 9);

    process_frame(frame, 10);
    ASSERT(expect_normal_reading("123.4", "C", 123.4f, 0.001f));
    ASSERT(meter_reading.expected_frame_family ==
           (uint8_t)FPGA_METER_FRAME_FAMILY_EXTENDED);
    ASSERT(meter_reading.observed_frame_family ==
           meter_reading.expected_frame_family);
    ASSERT(meter_reading.stock_mode == 5);
    ASSERT(meter_reading.stock_unit_index == 7);
    ASSERT(meter_reading.stock_composite_index == 9);
    return 1;
}

static int test_resistance_low_ohm_fails_closed_without_factory_cal(void)
{
    uint8_t frame[12];

    meter_data_init();
    build_segment_frame(frame, 4, 8, 2, 4, 0x00, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 6);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_UNRESOLVED_CALIBRATION);
    ASSERT_STR_EQ(meter_reading.display_str, "---");
    ASSERT(meter_reading.result_class == METER_RESULT_NONE);

    build_segment_frame(frame, 3, 3, 0, 0, 0x40, 0x00, 0x00, 0x00, 0);
    process_frame(frame, 6);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT_STR_EQ(meter_reading.unit_suffix, "kOhm");
    ASSERT(close_to(meter_reading.value, 3.300f, 0.001f));
    ASSERT_STR_EQ(meter_reading.display_str, "3.300");
    return 1;
}

static int test_invalidate_clears_stale_reading_before_mode_transition(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    uint8_t ohms_frame[12];
    uint32_t updates_after_frame;
    uint32_t display_updates_after_frame;

    build_segment_frame(ohms_frame, 3, 3, 0, 0, 0x40, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(mains_frame, 1);
    ASSERT(expect_normal_reading("228.2", "V", 228.2f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    updates_after_frame = meter_reading.update_count;
    display_updates_after_frame = meter_reading.display_update_count;

    meter_data_invalidate(6);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 6);
    ASSERT(meter_reading.result_class == METER_RESULT_NONE);
    ASSERT_STR_EQ(meter_reading.display_str, "---");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
    ASSERT(!meter_reading.continuity_beep);
    ASSERT(meter_reading.update_count == updates_after_frame + 1);
    ASSERT(meter_reading.display_update_count == display_updates_after_frame + 1);

    process_frame(ohms_frame, 6);
    ASSERT(expect_normal_reading("3.300", "kOhm", 3.300f, 0.001f));
    ASSERT(meter_reading.submode == 6);
    return 1;
}

static int test_invalidate_clears_stale_reading_for_every_submode(void)
{
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };
    static const uint8_t expected_stock_mode[] = {
        0, 1, 2, 2, 3, 3, 4, 6, 7, 5, 5
    };

    for (uint8_t mode = 0; mode < sizeof(expected_stock_mode); mode++) {
        uint32_t updates_after_frame;
        uint32_t display_updates_after_frame;

        meter_data_init();
        process_frame(dcv_frame, 0);
        ASSERT(expect_normal_reading("5.008", "V", 5.008f, 0.001f));
        updates_after_frame = meter_reading.update_count;
        display_updates_after_frame = meter_reading.display_update_count;

        meter_data_invalidate(mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT(expect_payload_cleared("---"));
        ASSERT(meter_reading.stock_mode == expected_stock_mode[mode]);
        ASSERT(meter_reading.expected_frame_family ==
               (uint8_t)fpga_meter_frame_family_for_submode(mode));
        ASSERT(meter_reading.observed_frame_family ==
               meter_reading.expected_frame_family);
        ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
        ASSERT(meter_reading.update_count == updates_after_frame + 1);
        ASSERT(meter_reading.display_update_count == display_updates_after_frame + 1);
        for (unsigned i = 0; i < sizeof(meter_reading.dbg_frame); i++) {
            ASSERT(meter_reading.dbg_frame[i] == 0);
        }
    }
    return 1;
}

static int test_parser_stock_mode_tracks_transition_plan_for_every_submode(void)
{
    uint8_t frame[12];

    for (uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode++) {
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(mode);
        build_valid_frame_for_mode(frame, mode);

        meter_data_init();
        meter_data_invalidate(mode);
        ASSERT(meter_reading.stock_mode == plan.stock_mode);
        ASSERT(meter_reading.submode == mode);
        ASSERT(!meter_reading.valid);

        process_frame(frame, mode);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.stock_mode == plan.stock_mode);
    }
    return 1;
}

static int test_invalid_submode_rejects_without_becoming_dcv(void)
{
    uint8_t frame[12];

    build_segment_frame(frame, 5, 0, 0, 8, 0x00,
                        0x00, 0x02, 0x00, 0x014E);
    meter_data_init();
    process_frame(frame, 99);

    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.submode == 99);
    ASSERT(meter_reading.result_class == METER_RESULT_NONE);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(meter_reading.stock_mode == FPGA_METER_INVALID_STOCK_MODE);
    ASSERT(expect_family_debug(
        (uint8_t)FPGA_METER_FRAME_FAMILY_INVALID,
        (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
        METER_REJECT_INVALID_SUBMODE));
    return 1;
}

static int test_invalid_submode_rejects_every_frame_family_corpus(void)
{
    static const uint8_t low_dcv_frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x07, 0x24, 0x80, 0x00, 0x01, 0x89,
    };
    uint8_t voltage_frame[12];
    uint8_t ac_voltage_frame[12];
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];
    uint8_t resistance_frame[12];
    uint8_t continuity_frame[12];
    uint8_t diode_frame[12];
    uint8_t extended_frame[12];
    const uint8_t *frames[9];
    uint8_t stale_seed[12];

    build_segment_frame(voltage_frame, 4, 9, 9, 4,
                        0x00, 0x00, 0x02, 0x00, 0);
    build_segment_frame(ac_voltage_frame, 4, 9, 9, 4,
                        0x00, 0x00, 0x02, 0x00, 0x0031);
    build_segment_frame(current_frame, 2, 2, 6, 1,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1,
                        0x00, 0x00, 0x00, 0x00, 0x0031);
    build_segment_frame(resistance_frame, 3, 3, 0, 0,
                        0x40, 0x00, 0x00, 0x00, 0);
    build_segment_frame(continuity_frame, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(diode_frame, 0, 6, 2, 3,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(extended_frame, 1, 2, 3, 4,
                        0x00, 0x00, 0x00, 0x00, 0);

    frames[0] = voltage_frame;
    frames[1] = ac_voltage_frame;
    frames[2] = low_dcv_frame;
    frames[3] = current_frame;
    frames[4] = ac_current_frame;
    frames[5] = resistance_frame;
    frames[6] = continuity_frame;
    frames[7] = diode_frame;
    frames[8] = extended_frame;

    for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        meter_data_init();
        build_segment_frame(stale_seed, 1, 2, 3, 4,
                            0x00, 0x00, 0x02, 0x00, 0);
        process_frame(stale_seed, 0);
        ASSERT(meter_reading.valid);
        ASSERT_STR_EQ(meter_reading.display_str, "1234");

        process_frame(frames[i], 99);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.submode == 99);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT(meter_reading.expected_frame_family ==
               FPGA_METER_FRAME_FAMILY_INVALID);
        ASSERT(meter_reading.reject_reason == METER_REJECT_INVALID_SUBMODE);
        ASSERT(expect_payload_cleared("---"));
    }

    return 1;
}

static int test_state_machine_property_matrix_covers_all_submodes(void)
{
    static const uint8_t modes[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    };
    uint8_t voltage_frame[12];
    uint8_t unresolved_low_dcv_frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x07, 0x24, 0x82, 0x00, 0x01, 0x89,
    };
    uint8_t ac_voltage_frame[12];
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];
    uint8_t resistance_frame[12];
    uint8_t continuity_frame[12];
    uint8_t extended_frame[12];

    build_segment_frame(voltage_frame, 1, 2, 3, 4,
                        0x00, 0x00, 0x02, 0x00, 0);
    build_segment_frame(ac_voltage_frame, 1, 2, 3, 4,
                        0x00, 0x00, 0x02, 0x00, 0x0031);
    build_segment_frame(current_frame, 2, 2, 6, 1,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1,
                        0x00, 0x00, 0x00, 0x00, 0x0031);
    build_segment_frame(resistance_frame, 3, 3, 0, 0,
                        0x40, 0x00, 0x00, 0x00, 0);
    build_segment_frame(continuity_frame, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(extended_frame, 1, 2, 3, 4,
                        0x00, 0x00, 0x00, 0x00, 0);

    for (unsigned i = 0; i < sizeof(modes); i++) {
        uint8_t mode = modes[i];
        const uint8_t *good =
            (mode == 0) ? voltage_frame :
            (mode == 1) ? ac_voltage_frame :
            (mode == 2 || mode == 3) ? current_frame :
            (mode == 4 || mode == 5) ? ac_current_frame :
            (mode == 6) ? resistance_frame :
            (mode == 7) ? continuity_frame :
            extended_frame;
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(mode);

        meter_data_init();
        meter_data_invalidate(mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.stock_mode == plan.stock_mode);
        ASSERT(meter_reading.expected_frame_family == plan.frame_family);
        ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);

        if (mode == 1 || mode == 4 || mode == 5) {
            process_frame((mode == 1) ? voltage_frame : current_frame, mode);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.reject_reason == METER_REJECT_MISSING_AC_EVIDENCE);
            ASSERT(expect_payload_cleared("---"));
        }

        process_frame(good, mode);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.stock_mode == plan.stock_mode);
        ASSERT(meter_reading.expected_frame_family == plan.frame_family);
        ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);

        if (mode != 7) {
            process_frame(continuity_frame, mode);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
            ASSERT(expect_payload_cleared("---"));
            process_frame(good, mode);
            ASSERT(meter_reading.valid);
        }

        if (plan.frame_family != FPGA_METER_FRAME_FAMILY_VOLTAGE) {
            process_frame(unresolved_low_dcv_frame, mode);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == mode);
            ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
            ASSERT(expect_payload_cleared("---"));
        }
    }
    return 1;
}

static void build_valid_frame_for_mode(uint8_t frame[12], uint8_t mode)
{
    switch (mode) {
    case 0:
        build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x02, 0x00, 0);
        break;
    case 1:
        build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x02, 0x00, 0x0031);
        break;
    case 4:
    case 5:
        build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0x0031);
        break;
    case 6:
        build_segment_frame(frame, 3, 3, 0, 0, 0x40, 0x00, 0x00, 0x00, 0);
        break;
    case 7:
        build_segment_frame(frame, 0, 0x12, 0x0A, 5, 0x00, 0x00, 0x00, 0x00, 0);
        break;
    default:
        build_segment_frame(frame, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
        break;
    }
}

static int test_goal_surface_property_enumerates_dmm_state_machine(void)
{
    enum {
        PHASE_INVALIDATE = 1U << 0,
        PHASE_BUSY_DROP = 1U << 1,
        PHASE_DISCARD_DROP = 1U << 2,
        PHASE_STABLE_ACCEPT = 1U << 3,
    };
    static const uint16_t all_logical_functions =
        (uint16_t)((1U << FPGA_METER_LOGICAL_FUNCTION_COUNT) - 1U);
    static const uint16_t all_local_submodes =
        (uint16_t)((1U << FPGA_METER_LOCAL_SUBMODE_COUNT) - 1U);
    static const uint16_t unresolved_ua_functions =
        (uint16_t)((1U << FPGA_METER_FUNCTION_DC_UA) |
                   (1U << FPGA_METER_FUNCTION_AC_UA));
    uint16_t logical_seen = 0;
    uint16_t unresolved_seen = 0;
    uint16_t supported_submodes_seen = 0;
    uint16_t local_submodes_seen = 0;
    uint16_t stale_transition_sources_seen = 0;
    uint16_t stale_transition_dests_seen = 0;
    uint8_t expected_families_seen = 0;
    uint8_t ac_evidence_seen = 0;
    uint8_t range_classes_seen = 0;
    uint8_t transition_phases_seen = 0;
    uint8_t marker_mismatch_seen = 0;
    uint8_t unclassified_active_policy_seen = 0;
    uint8_t invalid_submode_seen = 0;
    unsigned stale_transition_pairs = 0;
    uint8_t unresolved_low_dcv_frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x07, 0x24, 0x82, 0x00, 0x01, 0x89,
    };
    uint8_t continuity_marker[12];
    uint8_t frame[12];

    build_segment_frame(continuity_marker, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);

    for (uint8_t fn = 0; fn < FPGA_METER_LOGICAL_FUNCTION_COUNT; fn++) {
        uint8_t submode = fpga_meter_submode_for_logical_function(fn);

        logical_seen |= (uint16_t)(1U << fn);
        if (fpga_meter_logical_function_is_unresolved(fn)) {
            unresolved_seen |= (uint16_t)(1U << fn);
            ASSERT(submode == FPGA_METER_INVALID_LOCAL_SUBMODE);
        } else {
            ASSERT(fpga_meter_submode_is_valid(submode));
            supported_submodes_seen |= (uint16_t)(1U << submode);
        }
    }

    for (uint8_t source = 0; source < FPGA_METER_LOCAL_SUBMODE_COUNT; source++) {
        uint8_t source_frame[12];

        build_valid_frame_for_mode(source_frame, source);
        for (uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest++) {
            fpga_meter_transition_plan_t plan =
                fpga_meter_transition_plan_for_submode(dest);
            uint8_t discard = plan.discard_frames;
            uint32_t transition_skips = 0;

            stale_transition_sources_seen |= (uint16_t)(1U << source);
            stale_transition_dests_seen |= (uint16_t)(1U << dest);
            stale_transition_pairs++;

            meter_data_init();
            process_frame(source_frame, source);
            ASSERT(meter_reading.valid);

            meter_data_invalidate(dest);
            transition_phases_seen |= PHASE_INVALIDATE;
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);
            ASSERT(expect_payload_cleared("---"));

            ASSERT(!fpga_meter_rx_frame_should_parse(true, &discard,
                                                     &transition_skips));
            transition_phases_seen |= PHASE_BUSY_DROP;
            ASSERT(discard == plan.discard_frames);
            ASSERT(transition_skips == 1);

            for (uint8_t i = 0; i < plan.discard_frames; i++) {
                ASSERT(!fpga_meter_rx_frame_should_parse(false, &discard,
                                                         &transition_skips));
                transition_phases_seen |= PHASE_DISCARD_DROP;
            }
            ASSERT(discard == 0);
            ASSERT(fpga_meter_rx_frame_should_parse(false, &discard,
                                                    &transition_skips));
            transition_phases_seen |= PHASE_STABLE_ACCEPT;
        }
    }

    for (uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode++) {
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(mode);

        local_submodes_seen |= (uint16_t)(1U << mode);
        expected_families_seen |= (uint8_t)(1U << plan.frame_family);

        build_valid_frame_for_mode(frame, mode);
        meter_data_init();
        process_frame(frame, mode);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.expected_frame_family == plan.frame_family);
        ASSERT(meter_reading.observed_frame_family == plan.frame_family);
        ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
        if (!test_frame_has_voltage_payload_marker(frame) &&
            !test_raw_digits_are_continuity_marker(meter_reading.dbg_raw_digits)) {
            unclassified_active_policy_seen |= (uint8_t)(1U << plan.frame_family);
        }

        process_frame(unresolved_low_dcv_frame, mode);
        if (plan.frame_family == FPGA_METER_FRAME_FAMILY_VOLTAGE) {
            ASSERT(!meter_reading.valid);
            ASSERT(expect_payload_cleared("---"));
            if (mode == 0) {
                ASSERT(meter_reading.reject_reason ==
                       METER_REJECT_WRONG_FRAME_FAMILY);
            } else {
                ASSERT(meter_reading.reject_reason ==
                       METER_REJECT_MISSING_AC_EVIDENCE);
                ac_evidence_seen |= 1U << 0;
            }
        } else {
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.reject_reason ==
                   METER_REJECT_WRONG_FRAME_FAMILY);
            ASSERT(expect_payload_cleared("---"));
            marker_mismatch_seen |= (uint8_t)(1U << plan.frame_family);
        }

        meter_data_init();
        process_frame(continuity_marker, mode);
        if (plan.frame_family == FPGA_METER_FRAME_FAMILY_CONTINUITY) {
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.result_class == METER_RESULT_CONTINUITY);
        } else {
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.reject_reason ==
                   METER_REJECT_WRONG_FRAME_FAMILY);
            marker_mismatch_seen |= (uint8_t)(1U << plan.frame_family);
        }

        process_frame(frame, FPGA_METER_LOCAL_SUBMODE_COUNT);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.reject_reason == METER_REJECT_INVALID_SUBMODE);
        invalid_submode_seen = 1;
    }

    for (uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode++) {
        if (mode == 1 || mode == 4 || mode == 5) {
            uint8_t missing_ac_frame[12];
            uint8_t present_ac_frame[12];

            if (mode == 1) {
                build_segment_frame(missing_ac_frame, 2, 2, 8, 2,
                                    0x00, 0x24, 0x02, 0x00, 0);
                build_segment_frame(present_ac_frame, 2, 2, 8, 2,
                                    0x00, 0x00, 0x02, 0x00, 0x0031);
            } else {
                build_segment_frame(missing_ac_frame, 2, 2, 6, 1,
                                    0x00, 0x24, 0x00, 0x00, 0);
                build_segment_frame(present_ac_frame, 2, 2, 6, 1,
                                    0x00, 0x00, 0x00, 0x00, 0x0031);
            }

            meter_data_init();
            process_frame(missing_ac_frame, mode);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.reject_reason ==
                   METER_REJECT_MISSING_AC_EVIDENCE);
            ac_evidence_seen |= 1U << 1;

            process_frame(present_ac_frame, mode);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
            ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
            ac_evidence_seen |= 1U << 2;
        }
    }

    for (uint8_t bits = 0; bits < 16; bits++) {
        uint8_t expected_class =
            (bits & 0x8U) ? 4U :
            (bits & 0x4U) ? 3U :
            (bits & 0x2U) ? 2U :
            (bits & 0x1U) ? 1U : 0U;

        build_segment_frame(frame, 1, 2, 3, 4,
                            0x00, 0x00, 0x02, 0x00, 0x0031);
        if (bits & 0x1U) frame[5] |= 0x10U;
        if (bits & 0x2U) frame[4] |= 0x10U;
        if (bits & 0x4U) frame[3] |= 0x10U;
        if (bits & 0x8U) frame[8] |= 0x80U;

        meter_data_init();
        process_frame(frame, 0);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
        range_classes_seen |= (uint8_t)(1U << expected_class);
    }

    ASSERT(logical_seen == all_logical_functions);
    ASSERT(unresolved_seen == unresolved_ua_functions);
    ASSERT(supported_submodes_seen == all_local_submodes);
    ASSERT(local_submodes_seen == all_local_submodes);
    ASSERT(stale_transition_sources_seen == all_local_submodes);
    ASSERT(stale_transition_dests_seen == all_local_submodes);
    ASSERT(stale_transition_pairs ==
           FPGA_METER_LOCAL_SUBMODE_COUNT * FPGA_METER_LOCAL_SUBMODE_COUNT);
    ASSERT(expected_families_seen ==
           ((1U << FPGA_METER_FRAME_FAMILY_VOLTAGE) |
            (1U << FPGA_METER_FRAME_FAMILY_CURRENT) |
            (1U << FPGA_METER_FRAME_FAMILY_RESISTANCE) |
            (1U << FPGA_METER_FRAME_FAMILY_CONTINUITY) |
            (1U << FPGA_METER_FRAME_FAMILY_DIODE) |
            (1U << FPGA_METER_FRAME_FAMILY_EXTENDED)));
    ASSERT(ac_evidence_seen == 0x07U);
    ASSERT(range_classes_seen == 0x1FU);
    ASSERT(transition_phases_seen ==
           (PHASE_INVALIDATE | PHASE_BUSY_DROP |
            PHASE_DISCARD_DROP | PHASE_STABLE_ACCEPT));
    ASSERT(marker_mismatch_seen ==
           ((1U << FPGA_METER_FRAME_FAMILY_VOLTAGE) |
            (1U << FPGA_METER_FRAME_FAMILY_CURRENT) |
            (1U << FPGA_METER_FRAME_FAMILY_RESISTANCE) |
            (1U << FPGA_METER_FRAME_FAMILY_CONTINUITY) |
            (1U << FPGA_METER_FRAME_FAMILY_DIODE) |
            (1U << FPGA_METER_FRAME_FAMILY_EXTENDED)));
    ASSERT(unclassified_active_policy_seen ==
           ((1U << FPGA_METER_FRAME_FAMILY_CURRENT) |
            (1U << FPGA_METER_FRAME_FAMILY_RESISTANCE) |
            (1U << FPGA_METER_FRAME_FAMILY_DIODE) |
            (1U << FPGA_METER_FRAME_FAMILY_EXTENDED)));
    ASSERT(invalid_submode_seen == 1);
    return 1;
}

static int test_invalidate_clears_stale_payload_for_every_ordered_mode_transition(void)
{
    uint8_t source_frame[12];

    for (uint8_t source = 0; source < FPGA_METER_LOCAL_SUBMODE_COUNT; source++) {
        for (uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest++) {
            uint32_t updates_after_source;
            uint32_t display_updates_after_source;
            fpga_meter_transition_plan_t dest_plan =
                fpga_meter_transition_plan_for_submode(dest);

            meter_data_init();
            build_valid_frame_for_mode(source_frame, source);
            process_frame(source_frame, source);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.submode == source);
            ASSERT(meter_reading.result_class == METER_RESULT_NORMAL ||
                   meter_reading.result_class == METER_RESULT_CONTINUITY);
            updates_after_source = meter_reading.update_count;
            display_updates_after_source = meter_reading.display_update_count;

            meter_data_invalidate(dest);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);
            ASSERT(meter_reading.result_class == METER_RESULT_NONE);
            ASSERT(expect_payload_cleared("---"));
            ASSERT(meter_reading.stock_mode == dest_plan.stock_mode);
            ASSERT(meter_reading.expected_frame_family == dest_plan.frame_family);
            ASSERT(meter_reading.observed_frame_family == dest_plan.frame_family);
            ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
            ASSERT(meter_reading.update_count == updates_after_source + 1);
            ASSERT(meter_reading.display_update_count ==
                   display_updates_after_source + 1);
        }
    }
    return 1;
}

static int test_transport_gate_blocks_source_frames_during_every_transition(void)
{
    uint8_t source_frame[12];

    for (uint8_t source = 0; source < FPGA_METER_LOCAL_SUBMODE_COUNT; source++) {
        for (uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest++) {
            fpga_meter_transition_plan_t dest_plan =
                fpga_meter_transition_plan_for_submode(dest);
            uint8_t discard = dest_plan.discard_frames;
            uint32_t transition_skips = 0;

            meter_data_init();
            build_valid_frame_for_mode(source_frame, source);
            process_frame(source_frame, source);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.submode == source);

            meter_data_invalidate(dest);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);
            ASSERT(meter_reading.stock_mode == dest_plan.stock_mode);
            ASSERT(meter_reading.expected_frame_family == dest_plan.frame_family);
            ASSERT(expect_payload_cleared("---"));

            ASSERT(!fpga_meter_rx_frame_should_parse(true, &discard,
                                                     &transition_skips));
            ASSERT(discard == dest_plan.discard_frames);
            ASSERT(transition_skips == 1);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);
            ASSERT(expect_payload_cleared("---"));

            for (uint8_t i = 0; i < dest_plan.discard_frames; i++) {
                ASSERT(!fpga_meter_rx_frame_should_parse(false, &discard,
                                                         &transition_skips));
                ASSERT(!meter_reading.valid);
                ASSERT(meter_reading.submode == dest);
                ASSERT(expect_payload_cleared("---"));
            }
            ASSERT(discard == 0);
            ASSERT(transition_skips == 1);
            ASSERT(fpga_meter_rx_frame_should_parse(false, &discard,
                                                    &transition_skips));
        }
    }
    return 1;
}

static int test_transition_phase_marker_frames_follow_destination_state(void)
{
    struct marker_case {
        uint8_t family;
        const uint8_t *frame;
    };
    uint8_t voltage_frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };
    uint8_t continuity_frame[12];
    const struct marker_case markers[] = {
        { FPGA_METER_FRAME_FAMILY_VOLTAGE, voltage_frame },
        { FPGA_METER_FRAME_FAMILY_CONTINUITY, continuity_frame },
    };

    build_segment_frame(continuity_frame, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);

    for (uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest++) {
        for (unsigned m = 0; m < sizeof(markers) / sizeof(markers[0]); m++) {
            fpga_meter_transition_plan_t dest_plan =
                fpga_meter_transition_plan_for_submode(dest);
            uint8_t discard = dest_plan.discard_frames;
            uint32_t transition_skips = 0;
            uint8_t seed[12];

            meter_data_init();
            build_valid_frame_for_mode(seed, 0);
            process_frame(seed, 0);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.submode == 0);

            meter_data_invalidate(dest);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);
            ASSERT(meter_reading.expected_frame_family == dest_plan.frame_family);
            ASSERT(expect_payload_cleared("---"));

            ASSERT(!fpga_meter_rx_frame_should_parse(true, &discard,
                                                     &transition_skips));
            ASSERT(discard == dest_plan.discard_frames);
            ASSERT(transition_skips == 1);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == dest);

            for (uint8_t i = 0; i < dest_plan.discard_frames; i++) {
                ASSERT(!fpga_meter_rx_frame_should_parse(false, &discard,
                                                         &transition_skips));
                ASSERT(!meter_reading.valid);
                ASSERT(meter_reading.submode == dest);
            }
            ASSERT(discard == 0);
            ASSERT(fpga_meter_rx_frame_should_parse(false, &discard,
                                                    &transition_skips));

            process_frame(markers[m].frame, dest);
            ASSERT(meter_reading.submode == dest);
            ASSERT(meter_reading.expected_frame_family == dest_plan.frame_family);
            ASSERT(meter_reading.observed_frame_family == markers[m].family);
            if ((dest == 1 || dest == 4 || dest == 5) &&
                dest_plan.frame_family == markers[m].family) {
                ASSERT(!meter_reading.valid);
                ASSERT(meter_reading.reject_reason ==
                       METER_REJECT_MISSING_AC_EVIDENCE);
                ASSERT(expect_payload_cleared("---"));
            } else if (dest_plan.frame_family == markers[m].family) {
                ASSERT(meter_reading.valid);
                ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
                ASSERT(meter_reading.result_class == METER_RESULT_NORMAL ||
                       meter_reading.result_class == METER_RESULT_CONTINUITY);
            } else {
                ASSERT(!meter_reading.valid);
                ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
                ASSERT(expect_payload_cleared("---"));
            }
        }
    }
    return 1;
}

static int test_marker_visible_family_mismatch_matrix_clears_stale_payload(void)
{
    struct marker_case {
        uint8_t family;
        const char *name;
        const uint8_t *frame;
    };
    uint8_t voltage_frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };
    uint8_t continuity_frame[12];
    const struct marker_case markers[] = {
        {
            FPGA_METER_FRAME_FAMILY_VOLTAGE,
            "voltage-marker",
            voltage_frame,
        },
        {
            FPGA_METER_FRAME_FAMILY_CONTINUITY,
            "continuity-segment",
            continuity_frame,
        },
    };
    uint8_t good[12];

    build_segment_frame(continuity_frame, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);

    for (uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode++) {
        uint8_t expected =
            (uint8_t)fpga_meter_frame_family_for_submode(mode);

        for (unsigned m = 0; m < sizeof(markers) / sizeof(markers[0]); m++) {
            const uint8_t *foreign = markers[m].frame;
            uint32_t display_updates_after_good;

            if (expected == markers[m].family) {
                continue;
            }

            meter_data_init();
            build_valid_frame_for_mode(good, mode);
            process_frame(good, mode);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.result_class == METER_RESULT_NORMAL ||
                   meter_reading.result_class == METER_RESULT_CONTINUITY);
            display_updates_after_good = meter_reading.display_update_count;

            process_frame(foreign, mode);
            (void)markers[m].name;
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == mode);
            ASSERT(meter_reading.result_class == METER_RESULT_NONE);
            ASSERT(meter_reading.display_update_count == display_updates_after_good + 1);
            ASSERT(expect_payload_cleared("---"));
            ASSERT(expect_family_debug(expected, markers[m].family,
                                       METER_REJECT_WRONG_FRAME_FAMILY));
        }
    }
    return 1;
}

static int test_unclassified_normal_frames_follow_active_family_only(void)
{
    /*
     * Stock-visible cross-family markers are currently narrow: voltage payload
     * metadata in frame[8]/[9] and the continuity segment pattern. A normal
     * digit frame with neither marker is an unclassified normal frame for the
     * still-unmarked current/passive gaps: it must not grow a synthetic current,
     * resistance, diode, or extended "looks like" classifier. Voltage is no
     * longer allowed to use that fallback because its recovered stock marker is
     * visible in frame[8]/frame[9]; live frame[8]=00 DCV traces produced
     * confident nonsense readings when accepted as active voltage.
     */
    static const uint8_t modes[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    };
    uint8_t frame[12];

    for (unsigned i = 0; i < sizeof(modes); i++) {
        uint8_t mode = modes[i];
        uint16_t extra = (mode == 1 || mode == 4 || mode == 5) ? 0x0031 : 0;

        if (mode == 6 || mode == 7) {
            build_segment_frame(frame, 3, 3, 0, 0,
                                0x40, 0x00, 0x00, 0x00, extra);
        } else {
            build_segment_frame(frame, 1, 2, 3, 4,
                                0x00, 0x00, 0x00, 0x00, extra);
        }

        meter_data_init();
        process_frame(frame, mode);
        if (fpga_meter_frame_family_for_submode(mode) ==
            FPGA_METER_FRAME_FAMILY_VOLTAGE) {
            ASSERT(!meter_reading.valid);
            ASSERT(expect_payload_cleared("---"));
            ASSERT(expect_family_debug(FPGA_METER_FRAME_FAMILY_VOLTAGE,
                                       FPGA_METER_FRAME_FAMILY_VOLTAGE,
                                       METER_REJECT_WRONG_FRAME_FAMILY));
        } else {
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
            ASSERT(expect_family_debug(
                (uint8_t)fpga_meter_frame_family_for_submode(mode),
                (uint8_t)fpga_meter_frame_family_for_submode(mode),
                METER_REJECT_NONE));
        }
    }
    return 1;
}

static int test_dcv_rejects_live_unmarked_frame8_00_regression(void)
{
    uint8_t frame[12] = {
        0x5A, 0xA5, 0xCC, 0x47, 0xFE, 0xEB,
        0x47, 0x20, 0x00, 0x00, 0x01, 0x3C,
    };

    meter_data_init();
    process_frame(frame, 0);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug(FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               METER_REJECT_WRONG_FRAME_FAMILY));
    return 1;
}

static int test_dcv_rejects_live_status20_class_bit_wrong_family(void)
{
    /*
     * Live low-input regression, captured after the 0.05 V / source-off reports:
     * the producer emitted a stock class-bit frame, but with status 0x20 and
     * frame[6]=0x4x. The stock-shaped debug formatter moved to unit index 5, and
     * displaying it as a confident 0.7 V was a frame-family leak, not a decimal
     * exponent issue.
     */
    uint8_t frame[12] = {
        0x5A, 0xA5, 0x84, 0x0A, 0x0A, 0xEA,
        0x47, 0x20, 0x80, 0x00, 0x01, 0x3F,
    };

    meter_data_init();
    process_frame(frame, 0);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(expect_family_debug(FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               FPGA_METER_FRAME_FAMILY_VOLTAGE,
                               METER_REJECT_WRONG_FRAME_FAMILY));
    return 1;
}

static int test_dcv_rejects_live_status20_marker_frame_04366_regression(void)
{
    /*
     * The same low-input live failure with the 0x82 marker still has status
     * 0x24. Stock only proves that bit 5 participates in DCV formatter state;
     * it is not proof that the upstream producer has delivered a settled DCV
     * payload. This frame must fail closed until the writer/apply path is
     * recovered from stock/runtime evidence.
     */
    uint8_t frame[12] = {
        0x5A, 0xA5, 0x44, 0x8E, 0xEF, 0xE7,
        0x07, 0x24, 0x82, 0x00, 0x01, 0x89,
    };

    meter_data_init();
    process_frame(frame, 0);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
    ASSERT(expect_payload_cleared("---"));
    ASSERT(meter_reading.dbg_frame[8] == 0x82);
    return 1;
}

static int test_dcv_rejects_reported_low_input_numeric_shapes_without_marker(void)
{
    static const uint8_t frames[][12] = {
        /* Reported 0.1 V -> about 2.23 V: BCD-like producer data, no marker. */
        { 0x5A, 0xA5, 0xA4, 0xBD, 0x8D, 0xAF,
          0x4D, 0x20, 0x00, 0x00, 0x01, 0x36 },
        /* Reported 0.4 V -> about 153/154 V: unmarked status20 producer data. */
        { 0x5A, 0xA5, 0xCC, 0x47, 0xFE, 0xEB,
          0x47, 0x20, 0x00, 0x00, 0x01, 0x3C },
    };

    for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        meter_data_init();
        process_frame(frames[i], 0);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.reject_reason == METER_REJECT_WRONG_FRAME_FAMILY);
        ASSERT(expect_payload_cleared("---"));
    }
    return 1;
}

static int test_dcv_zero_detect_short_frame_reports_zero_not_low_input_number(void)
{
    uint8_t frame[12] = {
        0x5A, 0xA5, 0x04, 0xE0, 0x5B, 0x8E,
        0x0A, 0x28, 0x00, 0x00, 0x01, 0x43,
    };

    meter_data_init();
    process_frame(frame, 0);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.000");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    ASSERT(meter_reading.dbg_raw_digits[0] == 0x10);
    ASSERT(meter_reading.dbg_raw_digits[1] == 0x00);
    ASSERT(meter_reading.dbg_raw_digits[2] == 0x04);
    ASSERT(meter_reading.dbg_raw_digits[3] == 0x07);
    return 1;
}

static int test_passive_zero_detect_short_frames_report_short_state(void)
{
    uint8_t frame[12] = {
        0x5A, 0xA5, 0x04, 0xE0, 0x9B, 0xEA,
        0x07, 0x28, 0x00, 0x00, 0x01, 0x4B,
    };

    meter_data_init();
    process_frame(frame, 6);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.0");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "Ohm");
    ASSERT(!meter_reading.continuity_beep);
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));

    meter_data_init();
    process_frame(frame, 7);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_CONTINUITY);
    ASSERT_STR_EQ(meter_reading.display_str, "CONT");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "Ohm");
    ASSERT(meter_reading.continuity_beep);
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    return 1;
}

static int test_zero_detect_short_frames_report_terminal_zero_modes(void)
{
    uint8_t frame[12] = {
        0x5A, 0xA5, 0x04, 0xE0, 0x9B, 0xEA,
        0x07, 0x28, 0x00, 0x00, 0x01, 0x4B,
    };

    meter_data_init();
    process_frame(frame, 2);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.00");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "mA");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));

    meter_data_init();
    process_frame(frame, 3);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.000");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "A");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));

    meter_data_init();
    process_frame(frame, 8);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.000");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));

    meter_data_init();
    process_frame(frame, 9);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.reject_reason == METER_REJECT_NONE);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT_STR_EQ(meter_reading.display_str, "0.0");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "nF");
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    return 1;
}

static int test_mixed_special_voltage_digits_do_not_become_numeric_dcv(void)
{
    uint8_t frame[12];

    /*
     * The segment lookup's 0x0C..0x14 outputs are not decimal digits. A mixed
     * special/digit frame can otherwise look plausibly numeric after the old
     * clamp-to-zero path, especially in DCV where frame[8] may legitimately
     * carry voltage metadata. This guards the read function itself: only 0..9
     * enter the normal BCD assembly path after the explicit OL/blank/continuity
     * terminal cases above it.
     */
    build_segment_frame(frame, 1, 2, 0x0C, 3,
                        0x00, 0x00, 0x82, 0x00, 0);

    meter_data_init();
    process_frame(frame, 0);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_INVALID);
    ASSERT_STR_EQ(meter_reading.display_str, "ERR");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "");
    ASSERT(meter_reading.bcd_value == 0);
    ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
    ASSERT(meter_reading.dbg_raw_digits[0] == 1);
    ASSERT(meter_reading.dbg_raw_digits[1] == 2);
    ASSERT(meter_reading.dbg_raw_digits[2] == 0x0C);
    ASSERT(meter_reading.dbg_raw_digits[3] == 3);
    return 1;
}

static int test_frame6_0x40_is_not_a_global_resistance_family_marker(void)
{
    /*
     * Resistance kOhm frames use frame[6] upper nibble 4 in the current RE
     * notes, but that byte is not a standalone cross-mode family marker:
     * current-family fixtures also use 0x4x as a status/hold-bearing frame.
     * Guard this explicitly so future work does not "solve" wrong-family
     * leakage by turning one frame byte into a magnitude/range classifier.
     */
    uint8_t current_frame[12];

    build_segment_frame(current_frame, 1, 8, 6, 3,
                        0x40, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(current_frame, 2);

    ASSERT(expect_normal_reading("18.63", "mA", 18.63f, 0.001f));
    ASSERT(meter_reading.is_hold);
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                               METER_REJECT_NONE));
    return 1;
}

static int test_voltage_mode_mains_frame_uses_stock_range_hint(void)
{
    static const uint8_t frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };

    meter_data_init();
    process_frame(frame, 0);

    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.bcd_value == 2283);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT_STR_EQ(meter_reading.display_str, "228.3");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 228.3f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));
    return 1;
}

static int test_dcv_high_range_frame_stays_voltage_across_current_transition(void)
{
    static const uint8_t dcv_high_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    uint8_t current_frame[12];

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00,
                        0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(dcv_high_frame, 0);
    ASSERT(meter_reading.valid);
    ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
    ASSERT(meter_reading.bcd_value == 2283);
    ASSERT(meter_reading.decimal_pos == 3);
    ASSERT_STR_EQ(meter_reading.display_str, "228.3");
    ASSERT_STR_EQ(meter_reading.unit_suffix, "V");
    ASSERT(close_to(meter_reading.value, 228.3f, 0.05f));
    ASSERT(close_to(meter_reading.aux_freq_hz, 49.0f, 0.1f));

    process_frame(current_frame, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
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
    ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_CONTINUITY,
                               (uint8_t)FPGA_METER_FRAME_FAMILY_CONTINUITY,
                               METER_REJECT_NONE));
    return 1;
}

static int test_continuity_marker_rejected_outside_continuity_mode(void)
{
    uint8_t continuity[12];
    uint8_t normal[12];
    uint8_t ac_normal[12];
    uint8_t ac_current_normal[12];
    static const uint8_t modes[] = { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10 };

    build_segment_frame(continuity, 0, 0x12, 0x0A, 5,
                        0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(normal, 1, 2, 3, 4,
                        0x00, 0x00, 0x02, 0x00, 0);
    build_segment_frame(ac_normal, 1, 2, 3, 4,
                        0x00, 0x00, 0x02, 0x00, 0x0031);
    build_segment_frame(ac_current_normal, 1, 2, 3, 4,
                        0x00, 0x00, 0x00, 0x00, 0x0031);

    for (unsigned i = 0; i < sizeof(modes); i++) {
        uint8_t mode = modes[i];

        meter_data_init();
        if (mode == 6) {
            build_segment_frame(normal, 1, 2, 3, 4,
                                0x40, 0x00, 0x00, 0x00, 0);
        } else if (mode == 0) {
            build_segment_frame(normal, 1, 2, 3, 4,
                                0x00, 0x00, 0x02, 0x00, 0);
        } else {
            build_segment_frame(normal, 1, 2, 3, 4,
                                0x00, 0x00, 0x00, 0x00, 0);
        }
        process_frame((mode == 1) ? ac_normal :
                      (mode == 4 || mode == 5) ? ac_current_normal :
                      normal, mode);
        ASSERT(meter_reading.valid);
        ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);

        process_frame(continuity, mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT(expect_payload_cleared("---"));
        ASSERT(expect_family_debug(
            (uint8_t)fpga_meter_frame_family_for_submode(mode),
            (uint8_t)FPGA_METER_FRAME_FAMILY_CONTINUITY,
            METER_REJECT_WRONG_FRAME_FAMILY));
    }
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
    build_segment_frame(normal, 0, 0, 1, 0, 0x40, 0x00, 0x00, 0x00, 0);

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

    build_segment_frame(partial_blank, 0x10, 0x11, 0x10, 0x10, 0x00, 0x00, 0x02, 0x00, 0);
    build_segment_frame(invalid, 0xFF, 0, 0, 0, 0x00, 0x00, 0x02, 0x00, 0);
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

static int test_special_frames_clear_stale_payload_fields(void)
{
    uint8_t normal[12];
    uint8_t overload[12];
    uint8_t blank[12];
    uint8_t partial_blank[12];
    uint8_t invalid[12];

    build_segment_frame(normal, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(overload, 0x0A, 0x0B, 0, 0, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(blank, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(partial_blank, 0x10, 0x11, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(invalid, 0xFF, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(normal, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    process_frame(overload, 2);
    ASSERT(meter_reading.result_class == METER_RESULT_OVERLOAD);
    ASSERT(meter_reading.bar_fraction == 1.0f);
    ASSERT(expect_payload_cleared("OL"));

    process_frame(normal, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    process_frame(blank, 2);
    ASSERT(meter_reading.result_class == METER_RESULT_BLANK);
    ASSERT(expect_payload_cleared("---"));

    process_frame(normal, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    process_frame(partial_blank, 2);
    ASSERT(meter_reading.result_class == METER_RESULT_BLANK);
    ASSERT(expect_payload_cleared("---"));

    process_frame(normal, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    process_frame(invalid, 2);
    ASSERT(meter_reading.result_class == METER_RESULT_INVALID);
    ASSERT(expect_payload_cleared("ERR"));
    return 1;
}

static int test_non_voltage_modes_reject_voltage_payloads(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };
    static const uint8_t dcv_high_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    static const uint8_t dcv_low_frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };
    static const uint8_t wrong_family_modes[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    const uint8_t *voltage_frames[] = {
        mains_frame, dcv_frame, dcv_high_frame, dcv_low_frame
    };

    for (unsigned f = 0; f < sizeof(voltage_frames) / sizeof(voltage_frames[0]); f++) {
        for (unsigned i = 0; i < sizeof(wrong_family_modes); i++) {
            meter_data_init();
            process_frame(voltage_frames[f], wrong_family_modes[i]);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == wrong_family_modes[i]);
            ASSERT(meter_reading.result_class == METER_RESULT_NONE);
            ASSERT_STR_EQ(meter_reading.display_str, "---");
            ASSERT_STR_EQ(meter_reading.unit_suffix, "");
            ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
            ASSERT(close_to(meter_reading.aux_freq_hz, 0.0f, 0.001f));
            ASSERT(expect_family_debug(
                (uint8_t)fpga_meter_frame_family_for_submode(wrong_family_modes[i]),
                (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                METER_REJECT_WRONG_FRAME_FAMILY));
        }
    }
    return 1;
}

static int test_voltage_payload_clears_stale_reading_in_all_non_voltage_modes(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    static const uint8_t dcv_frame[12] = {
        0x5A, 0xA5, 0xC6, 0xF7, 0xEB, 0xEB,
        0x0F, 0x00, 0x02, 0x00, 0x01, 0x4E,
    };
    static const uint8_t dcv_high_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0x9F,
        0x0F, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    static const uint8_t dcv_low_frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };
    static const uint8_t modes[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    const uint8_t *voltage_frames[] = {
        mains_frame, dcv_frame, dcv_high_frame, dcv_low_frame
    };
    uint8_t normal[12];

    for (unsigned m = 0; m < sizeof(modes); m++) {
        for (unsigned f = 0; f < sizeof(voltage_frames) / sizeof(voltage_frames[0]); f++) {
            meter_data_init();

            if (modes[m] == 6 || modes[m] == 7) {
                build_segment_frame(normal, 3, 3, 0, 0, 0x40, 0x00, 0x00, 0x00, 0);
            } else if (modes[m] == 4 || modes[m] == 5) {
                build_segment_frame(normal, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0x0031);
            } else {
                build_segment_frame(normal, 1, 2, 3, 4, 0x00, 0x00, 0x00, 0x00, 0);
            }

            process_frame(normal, modes[m]);
            ASSERT(meter_reading.valid);
            ASSERT(meter_reading.result_class == METER_RESULT_NORMAL);
            ASSERT(meter_reading.bcd_value != 0);
            ASSERT_STR_EQ(meter_reading.display_str,
                          (modes[m] == 6 || modes[m] == 7) ? "3.300" :
                          (modes[m] == 8 || modes[m] == 9 || modes[m] == 10) ?
                          "123.4" :
                          (modes[m] == 3 || modes[m] == 5) ? "1.234" : "12.34");

            process_frame(voltage_frames[f], modes[m]);
            ASSERT(!meter_reading.valid);
            ASSERT(meter_reading.submode == modes[m]);
            ASSERT(meter_reading.result_class == METER_RESULT_NONE);
            ASSERT(expect_payload_cleared("---"));
            ASSERT(expect_family_debug(
                (uint8_t)fpga_meter_frame_family_for_submode(modes[m]),
                (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                METER_REJECT_WRONG_FRAME_FAMILY));
        }
    }
    return 1;
}

static int test_special_voltage_family_frames_are_rejected_outside_voltage(void)
{
    uint8_t ol_voltage_frame[12];
    static const uint8_t special_voltage_reject_modes[] = {
        2, 3, 4, 5, 6, 7, 8, 9, 10
    };

    build_segment_frame(ol_voltage_frame, 0x0A, 0x0B, 0, 0, 0x00,
                        0x00, 0x02, 0x00, 0x0031);

    for (unsigned i = 0; i < sizeof(special_voltage_reject_modes); i++) {
        uint8_t mode = special_voltage_reject_modes[i];

        meter_data_init();
        process_frame(ol_voltage_frame, mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT_STR_EQ(meter_reading.display_str, "---");
        ASSERT_STR_EQ(meter_reading.unit_suffix, "");
        ASSERT(expect_family_debug(
            (uint8_t)fpga_meter_frame_family_for_submode(mode),
            (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
            METER_REJECT_WRONG_FRAME_FAMILY));
    }
    return 1;
}

static int test_voltage_payload_clears_stale_current_reading(void)
{
    static const uint8_t mains_frame[12] = {
        0x5A, 0xA5, 0xA5, 0xAD, 0xED, 0xBF,
        0x0D, 0x00, 0x02, 0x00, 0x00, 0x31,
    };
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];
    static const uint8_t current_modes[] = { 2, 3, 4, 5 };

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0x0031);

    for (unsigned i = 0; i < sizeof(current_modes); i++) {
        uint8_t mode = current_modes[i];
        const uint8_t *active_current =
            (mode == 4 || mode == 5) ? ac_current_frame : current_frame;
        uint32_t updates_after_current;
        uint32_t display_updates_after_current;

        meter_data_init();
        process_frame(active_current, mode);
        ASSERT(expect_normal_reading((mode == 2 || mode == 4) ? "22.61" : "2.261",
                                     (mode == 2 || mode == 4) ? "mA" : "A",
                                     (mode == 2 || mode == 4) ? 22.61f : 2.261f,
                                     0.001f));
        updates_after_current = meter_reading.update_count;
        display_updates_after_current = meter_reading.display_update_count;

        process_frame(active_current, mode);
        ASSERT(expect_normal_reading((mode == 2 || mode == 4) ? "22.61" : "2.261",
                                     (mode == 2 || mode == 4) ? "mA" : "A",
                                     (mode == 2 || mode == 4) ? 22.61f : 2.261f,
                                     0.001f));
        ASSERT(meter_reading.update_count == updates_after_current + 1);
        ASSERT(meter_reading.display_update_count == display_updates_after_current);
        updates_after_current = meter_reading.update_count;
        display_updates_after_current = meter_reading.display_update_count;

        process_frame(mains_frame, mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.update_count == updates_after_current + 1);
        ASSERT(meter_reading.display_update_count == display_updates_after_current + 1);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT_STR_EQ(meter_reading.display_str, "---");
        ASSERT_STR_EQ(meter_reading.unit_suffix, "");
        ASSERT(close_to(meter_reading.value, 0.0f, 0.001f));
        ASSERT(expect_family_debug(
            (uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
            (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
            METER_REJECT_WRONG_FRAME_FAMILY));

        updates_after_current = meter_reading.update_count;
        display_updates_after_current = meter_reading.display_update_count;
        process_frame(mains_frame, mode);
        ASSERT(meter_reading.update_count == updates_after_current + 1);
        ASSERT(meter_reading.display_update_count == display_updates_after_current);
        ASSERT_STR_EQ(meter_reading.display_str, "---");
    }
    return 1;
}

static int test_low_dcv_voltage_payload_clears_stale_current_reading(void)
{
    static const uint8_t low_dcv_frame[12] = {
        0x5A, 0xA5, 0x4E, 0xCE, 0x8F, 0x8A,
        0x0A, 0x00, 0x82, 0x00, 0x01, 0x7F,
    };
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];
    static const uint8_t low_dcv_current_modes[] = { 2, 3, 4, 5 };

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0x0031);

    for (unsigned i = 0; i < sizeof(low_dcv_current_modes); i++) {
        uint8_t mode = low_dcv_current_modes[i];
        const uint8_t *active_current =
            (mode == 4 || mode == 5) ? ac_current_frame : current_frame;
        uint32_t updates_after_current;
        uint32_t display_updates_after_current;

        meter_data_init();
        process_frame(active_current, mode);
        ASSERT(expect_normal_reading((mode == 2 || mode == 4) ? "22.61" : "2.261",
                                     (mode == 2 || mode == 4) ? "mA" : "A",
                                     (mode == 2 || mode == 4) ? 22.61f : 2.261f,
                                     0.001f));
        updates_after_current = meter_reading.update_count;
        display_updates_after_current = meter_reading.display_update_count;

        process_frame(low_dcv_frame, mode);
        ASSERT(!meter_reading.valid);
        ASSERT(meter_reading.update_count == updates_after_current + 1);
        ASSERT(meter_reading.display_update_count == display_updates_after_current + 1);
        ASSERT(meter_reading.submode == mode);
        ASSERT(meter_reading.result_class == METER_RESULT_NONE);
        ASSERT(expect_payload_cleared("---"));
        ASSERT(expect_family_debug((uint8_t)FPGA_METER_FRAME_FAMILY_CURRENT,
                                   (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE,
                                   METER_REJECT_WRONG_FRAME_FAMILY));
    }
    return 1;
}

static int test_stock_fsm_debug_fields_follow_mode_and_frames(void)
{
    uint8_t current_frame[12];
    uint8_t voltage_payload[12];

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(voltage_payload, 5, 0, 0, 8, 0x00, 0x00, 0x02, 0x00, 0x014E);

    meter_data_init();
    meter_data_invalidate(2);
    ASSERT(meter_reading.stock_mode == 2);
    ASSERT(meter_reading.stock_variant == 1);
    ASSERT(meter_reading.stock_dc_state == 0);

    process_frame(current_frame, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    ASSERT(meter_reading.stock_mode == 2);
    ASSERT(meter_reading.stock_variant == 1);
    ASSERT(meter_reading.stock_format == 1);
    ASSERT(meter_reading.stock_display_cmd == 2);
    ASSERT(meter_reading.stock_unit_index == 4);
    ASSERT(meter_reading.stock_composite_index == 1);

    process_frame(voltage_payload, 2);
    ASSERT(!meter_reading.valid);
    ASSERT(meter_reading.stock_mode == 2);
    ASSERT(meter_reading.stock_variant == 1);
    ASSERT(meter_reading.stock_dc_state == 0);
    return 1;
}

static int test_large_current_submodes_use_active_local_range_state(void)
{
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00,
                        0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1, 0x00,
                        0x00, 0x00, 0x00, 0x0031);

    meter_data_init();
    process_frame(current_frame, 2);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(meter_reading.stock_variant == 1);
    ASSERT(meter_reading.stock_unit_index == 4);

    process_frame(current_frame, 3);
    ASSERT(expect_normal_reading("2.261", "A", 2.261f, 0.001f));
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(meter_reading.stock_mode == 2);
    ASSERT(meter_reading.stock_variant == 2);
    ASSERT(meter_reading.stock_unit_index == 3);

    meter_data_invalidate(4);
    process_frame(ac_current_frame, 4);
    ASSERT(expect_normal_reading("22.61", "mA", 22.61f, 0.001f));
    ASSERT(meter_reading.decimal_pos == 2);
    ASSERT(meter_reading.stock_variant == 1);
    ASSERT(meter_reading.stock_unit_index == 5);

    process_frame(ac_current_frame, 5);
    ASSERT(expect_normal_reading("2.261", "A", 2.261f, 0.001f));
    ASSERT(meter_reading.decimal_pos == 1);
    ASSERT(meter_reading.stock_mode == 3);
    ASSERT(meter_reading.stock_variant == 2);
    ASSERT(meter_reading.stock_unit_index == 5);
    return 1;
}

static int test_current_submodes_do_not_expose_unproven_microamp_unit(void)
{
    uint8_t current_frame[12];
    uint8_t ac_current_frame[12];
    static const uint8_t modes[] = { 2, 3, 4, 5 };

    build_segment_frame(current_frame, 2, 2, 6, 1, 0x00,
                        0x00, 0x00, 0x00, 0);
    build_segment_frame(ac_current_frame, 2, 2, 6, 1, 0x00,
                        0x00, 0x00, 0x00, 0x0031);

    for (unsigned i = 0; i < sizeof(modes); i++) {
        uint8_t mode = modes[i];
        const uint8_t *frame = (mode == 4 || mode == 5) ?
                               ac_current_frame : current_frame;

        meter_data_init();
        process_frame(frame, mode);
        ASSERT(meter_reading.valid);
        ASSERT_STR_EQ(meter_reading.unit_suffix,
                      (mode == 2 || mode == 4) ? "mA" : "A");
        ASSERT(strcmp(meter_reading.unit_suffix, "uA") != 0);
    }
    return 1;
}

static int test_snapshot_returns_coherent_latest_completed_reading(void)
{
    uint8_t ol_frame[12];
    uint8_t current_frame[12];
    meter_reading_t snap;

    build_segment_frame(ol_frame, 0x0A, 0x0B, 0, 0, 0x00, 0x00, 0x00, 0x00, 0);
    build_segment_frame(current_frame, 1, 8, 6, 3, 0x40, 0x00, 0x00, 0x00, 0);

    meter_data_init();
    process_frame(ol_frame, 2);
    ASSERT(meter_data_snapshot(&snap));
    ASSERT(snap.valid);
    ASSERT(snap.result_class == METER_RESULT_OVERLOAD);
    ASSERT_STR_EQ(snap.display_str, "OL");
    ASSERT_STR_EQ(snap.unit_suffix, "");

    process_frame(current_frame, 2);
    ASSERT(meter_data_snapshot(&snap));
    ASSERT(snap.valid);
    ASSERT(snap.result_class == METER_RESULT_NORMAL);
    ASSERT(snap.submode == 2);
    ASSERT_STR_EQ(snap.display_str, "18.63");
    ASSERT_STR_EQ(snap.unit_suffix, "mA");
    ASSERT(close_to(snap.value, 18.63f, 0.001f));
    ASSERT(snap.display_update_count == meter_reading.display_update_count);
    return 1;
}

int main(void)
{
    printf("Meter data frame tests\n");

    TEST(segment_frame_builder_exercises_cross_byte_lookup);
    TEST(dcv_5v_frame_keeps_verified_decimal_and_unit);
    TEST(dcv_live_5v_frame_uses_stock_range_hint);
    TEST(dcv_live_32v_frame_uses_stock_range_hint);
    TEST(dcv_1v2949_frame_uses_stock_extended_raw_and_class4);
    TEST(dcv_1v4979_frame_uses_stock_extended_raw_and_class4);
    TEST(dcv_live_1v5_frame_uses_stock_extended_raw_and_class4);
    TEST(dcv_live_1v5_rotating_frames_keep_stock_class4);
    TEST(dcv_live_0200_bare_class_bit_fails_closed);
    TEST(dcv_stock_range_class_priority_table);
    TEST(dcv_stock_range_class_priority_all_bit_combinations);
    TEST(dcv_class4_priority_requires_frame8_bit7);
    TEST(dcv_synthetic_5008_without_class_bits_stays_class0);
    TEST(dcv_extra_frequency_hint_does_not_set_voltage_range);
    TEST(dcv_aux_extra_bytes_do_not_change_stock_range_class);
    TEST(dcv_7005_without_class_bits_stays_class0);
    TEST(dcv_range_frames_are_not_latched_from_acv_mains);
    TEST(acv_mains_frame_uses_high_voltage_scale_and_frequency);
    TEST(acv_rejects_dc_voltage_without_ac_evidence);
    TEST(acv_rejects_live_low_dcv_status24_without_frequency_hint);
    TEST(ac_current_rejects_current_frame_without_ac_evidence);
    TEST(ac_modes_require_frequency_hint_boundaries);
    TEST(stock_formatter_families_have_regression_fixtures);
    TEST(passive_formatter_debug_fields_cover_diode_and_extended_splits);
    TEST(resistance_low_ohm_fails_closed_without_factory_cal);
    TEST(invalidate_clears_stale_reading_before_mode_transition);
    TEST(invalidate_clears_stale_reading_for_every_submode);
    TEST(parser_stock_mode_tracks_transition_plan_for_every_submode);
    TEST(invalid_submode_rejects_without_becoming_dcv);
    TEST(invalid_submode_rejects_every_frame_family_corpus);
    TEST(state_machine_property_matrix_covers_all_submodes);
    TEST(goal_surface_property_enumerates_dmm_state_machine);
    TEST(invalidate_clears_stale_payload_for_every_ordered_mode_transition);
    TEST(transport_gate_blocks_source_frames_during_every_transition);
    TEST(transition_phase_marker_frames_follow_destination_state);
    TEST(marker_visible_family_mismatch_matrix_clears_stale_payload);
    TEST(unclassified_normal_frames_follow_active_family_only);
    TEST(dcv_rejects_live_unmarked_frame8_00_regression);
    TEST(dcv_rejects_live_status20_class_bit_wrong_family);
    TEST(dcv_rejects_live_status20_marker_frame_04366_regression);
    TEST(dcv_rejects_reported_low_input_numeric_shapes_without_marker);
    TEST(dcv_zero_detect_short_frame_reports_zero_not_low_input_number);
    TEST(passive_zero_detect_short_frames_report_short_state);
    TEST(zero_detect_short_frames_report_terminal_zero_modes);
    TEST(mixed_special_voltage_digits_do_not_become_numeric_dcv);
    TEST(frame6_0x40_is_not_a_global_resistance_family_marker);
    TEST(voltage_mode_mains_frame_uses_stock_range_hint);
    TEST(dcv_high_range_frame_stays_voltage_across_current_transition);
    TEST(voltage_mode_mains_rotating_frames_stay_high_voltage);
    TEST(acv_repeated_rotating_range_frames_do_not_drop_to_2v);
    TEST(continuity_frame_sets_beep_from_segment_pattern);
    TEST(continuity_marker_rejected_outside_continuity_mode);
    TEST(non_continuity_terminal_frames_clear_stale_beep);
    TEST(special_frames_clear_stale_aux_frequency);
    TEST(special_frames_clear_stale_payload_fields);
    TEST(non_voltage_modes_reject_voltage_payloads);
    TEST(voltage_payload_clears_stale_reading_in_all_non_voltage_modes);
    TEST(special_voltage_family_frames_are_rejected_outside_voltage);
    TEST(voltage_payload_clears_stale_current_reading);
    TEST(low_dcv_voltage_payload_clears_stale_current_reading);
    TEST(stock_fsm_debug_fields_follow_mode_and_frames);
    TEST(large_current_submodes_use_active_local_range_state);
    TEST(current_submodes_do_not_expose_unproven_microamp_unit);
    TEST(snapshot_returns_coherent_latest_completed_reading);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
