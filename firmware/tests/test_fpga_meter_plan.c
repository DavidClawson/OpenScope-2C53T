#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fpga_meter_plan.h"

static int failures;

#define EXPECT_EQ_U8(name, got, want) \
    do { \
        uint8_t g_ = (uint8_t)(got); \
        uint8_t w_ = (uint8_t)(want); \
        if (g_ != w_) { \
            printf("FAIL %s: got 0x%02X want 0x%02X\n", name, g_, w_); \
            failures++; \
        } \
    } while (0)

#define EXPECT_EQ_U16(name, got, want) \
    do { \
        uint16_t g_ = (uint16_t)(got); \
        uint16_t w_ = (uint16_t)(want); \
        if (g_ != w_) { \
            printf("FAIL %s: got 0x%04X want 0x%04X\n", name, g_, w_); \
            failures++; \
        } \
    } while (0)

static void test_stock_table_bytes(void)
{
    static const uint8_t expected[FPGA_METER_STOCK_MODE_COUNT] = {
        0x14, 0x0C, 0x17, 0x0B, 0x0A, 0x12, 0x11, 0x10
    };

    for (uint8_t i = 0; i < FPGA_METER_STOCK_MODE_COUNT; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stock low %u", (unsigned)i);
        EXPECT_EQ_U8(name, fpga_meter_stock_cmd_low_for_mode(i), expected[i]);
    }
}

static void test_local_submode_mapping(void)
{
    static const uint8_t expected_stock_mode[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0, 1, 4, 4, 3, 3, 2, 6, 7, 5
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[32];
        snprintf(name, sizeof(name), "submode stock %u", (unsigned)i);
        EXPECT_EQ_U8(name, fpga_meter_stock_mode_for_submode(i),
                     expected_stock_mode[i]);
    }
}

static void test_wire_words_are_raw_05_family(void)
{
    static const uint16_t expected_words[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0x0514, 0x050C, 0x050A, 0x050A, 0x050B,
        0x050B, 0x0517, 0x0511, 0x0510, 0x0512
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[32];
        uint16_t word = fpga_meter_stock_cmd_word_for_submode(i);
        snprintf(name, sizeof(name), "submode word %u", (unsigned)i);
        EXPECT_EQ_U16(name, word, expected_words[i]);
        EXPECT_EQ_U8("raw family", (uint8_t)(word >> 8), 0x05);
    }
}

static void test_fallbacks(void)
{
    EXPECT_EQ_U8("bad stock mode falls back", fpga_meter_stock_cmd_low_for_mode(99), 0x14);
    EXPECT_EQ_U8("bad submode stock mode", fpga_meter_stock_mode_for_submode(99), 0);
    EXPECT_EQ_U16("bad submode word", fpga_meter_stock_cmd_word_for_submode(99), 0x0514);
}

int main(void)
{
    test_stock_table_bytes();
    test_local_submode_mapping();
    test_wire_words_are_raw_05_family();
    test_fallbacks();

    if (failures) {
        printf("%d fpga meter plan test(s) failed\n", failures);
        return 1;
    }

    printf("fpga meter plan tests passed\n");
    return 0;
}
