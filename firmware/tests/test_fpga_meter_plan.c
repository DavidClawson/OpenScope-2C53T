#include <stdint.h>
#include <stdbool.h>
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
        0, 1, 2, 2, 3, 3, 4, 6, 7, 5, 5
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
        0x0514, 0x050C, 0x0517, 0x0517, 0x050B,
        0x050B, 0x050A, 0x0511, 0x0510, 0x0512,
        0x0512
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[32];
        uint16_t word = fpga_meter_stock_cmd_word_for_submode(i);
        snprintf(name, sizeof(name), "submode word %u", (unsigned)i);
        EXPECT_EQ_U16(name, word, expected_words[i]);
        EXPECT_EQ_U8("raw family", (uint8_t)(word >> 8), 0x05);
    }
}

static void test_stock_apply_words_for_runtime_family_switch(void)
{
    static const uint16_t expected_apply[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0x0000, 0x050D, 0x050E, 0x050E, 0x0000,
        0x0000, 0x0000, 0x0516, 0x0515, 0x0000,
        0x0000
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[32];
        uint16_t word = 0xAAAA;
        bool have_word = fpga_meter_stock_apply_cmd_word_for_submode(i, &word);

        snprintf(name, sizeof(name), "apply exists %u", (unsigned)i);
        EXPECT_EQ_U8(name, have_word ? 1U : 0U,
                     expected_apply[i] != 0 ? 1U : 0U);
        if (expected_apply[i] != 0) {
            snprintf(name, sizeof(name), "apply word %u", (unsigned)i);
            EXPECT_EQ_U16(name, word, expected_apply[i]);
            EXPECT_EQ_U8("apply raw family", (uint8_t)(word >> 8), 0x05);
        } else {
            snprintf(name, sizeof(name), "apply untouched %u", (unsigned)i);
            EXPECT_EQ_U16(name, word, 0xAAAA);
        }
    }
}

static void test_transition_plan_covers_mux_family_and_settle_policy(void)
{
    static const uint8_t expected_stock_mode[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0, 1, 2, 2, 3, 3, 4, 6, 7, 5, 5
    };
    static const uint8_t expected_family[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        FPGA_METER_FRAME_FAMILY_VOLTAGE,
        FPGA_METER_FRAME_FAMILY_VOLTAGE,
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_RESISTANCE,
        FPGA_METER_FRAME_FAMILY_CONTINUITY,
        FPGA_METER_FRAME_FAMILY_DIODE,
        FPGA_METER_FRAME_FAMILY_EXTENDED,
        FPGA_METER_FRAME_FAMILY_EXTENDED,
    };
    static const uint16_t expected_selector[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0x0514, 0x050C, 0x0517, 0x0517, 0x050B,
        0x050B, 0x050A, 0x0511, 0x0510, 0x0512,
        0x0512
    };
    static const uint16_t expected_apply[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0x0000, 0x050D, 0x050E, 0x050E, 0x0000,
        0x0000, 0x0000, 0x0516, 0x0515, 0x0000,
        0x0000
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[48];
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(i);

        snprintf(name, sizeof(name), "plan submode %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.submode, i);
        snprintf(name, sizeof(name), "plan stock %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.stock_mode, expected_stock_mode[i]);
        snprintf(name, sizeof(name), "plan mux %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.mux_index, expected_stock_mode[i]);
        snprintf(name, sizeof(name), "plan portc/porte mux %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.portc_porte_mux, expected_stock_mode[i]);
        snprintf(name, sizeof(name), "plan porta/portb mux %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.porta_portb_mux, expected_stock_mode[i]);
        snprintf(name, sizeof(name), "plan family %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.frame_family, expected_family[i]);
        snprintf(name, sizeof(name), "plan discard %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.discard_frames,
                     FPGA_METER_TRANSITION_DISCARD_FRAMES);
        snprintf(name, sizeof(name), "plan settle %u", (unsigned)i);
        EXPECT_EQ_U16(name, plan.settle_ms, FPGA_METER_TRANSITION_SETTLE_MS);
        snprintf(name, sizeof(name), "plan selector %u", (unsigned)i);
        EXPECT_EQ_U16(name, plan.selector_word, expected_selector[i]);
        snprintf(name, sizeof(name), "plan apply exists %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.has_apply_word ? 1U : 0U,
                     expected_apply[i] != 0 ? 1U : 0U);
        snprintf(name, sizeof(name), "plan apply word %u", (unsigned)i);
        EXPECT_EQ_U16(name, plan.apply_word, expected_apply[i]);
        snprintf(name, sizeof(name), "plan voltage axis %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.voltage_function_axis ? 1U : 0U,
                     expected_family[i] == FPGA_METER_FRAME_FAMILY_VOLTAGE ?
                     1U : 0U);
    }
}

static void test_state_machine_contract_is_exhaustive(void)
{
    uint16_t seen_local_submodes = 0;
    uint16_t seen_stock_modes = 0;

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[56];
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(i);

        seen_local_submodes |= (uint16_t)(1U << i);
        if (plan.stock_mode < FPGA_METER_STOCK_MODE_COUNT) {
            seen_stock_modes |= (uint16_t)(1U << plan.stock_mode);
        }

        snprintf(name, sizeof(name), "contract valid submode %u", (unsigned)i);
        EXPECT_EQ_U8(name, fpga_meter_submode_is_valid(i) ? 1U : 0U, 1U);
        snprintf(name, sizeof(name), "contract stock mode valid %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.stock_mode < FPGA_METER_STOCK_MODE_COUNT ? 1U : 0U, 1U);
        snprintf(name, sizeof(name), "contract frame family valid %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.frame_family != FPGA_METER_FRAME_FAMILY_INVALID ? 1U : 0U, 1U);
        snprintf(name, sizeof(name), "contract selector valid %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.selector_word != FPGA_METER_INVALID_SELECTOR_WORD ? 1U : 0U, 1U);
    }

    EXPECT_EQ_U16("all local submodes covered", seen_local_submodes,
                  (uint16_t)((1U << FPGA_METER_LOCAL_SUBMODE_COUNT) - 1U));
    EXPECT_EQ_U16("all recovered stock slots covered", seen_stock_modes,
                  (uint16_t)((1U << FPGA_METER_STOCK_MODE_COUNT) - 1U));
}

static void test_fallbacks(void)
{
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(99);

    EXPECT_EQ_U8("bad stock mode is invalid",
                 fpga_meter_stock_cmd_low_for_mode(99), 0);
    EXPECT_EQ_U8("bad submode valid",
                 fpga_meter_submode_is_valid(99) ? 1U : 0U, 0U);
    EXPECT_EQ_U8("good submode valid",
                 fpga_meter_submode_is_valid(10) ? 1U : 0U, 1U);
    EXPECT_EQ_U8("bad submode stock mode",
                 fpga_meter_stock_mode_for_submode(99),
                 FPGA_METER_INVALID_STOCK_MODE);
    EXPECT_EQ_U16("bad submode word",
                  fpga_meter_stock_cmd_word_for_submode(99),
                  FPGA_METER_INVALID_SELECTOR_WORD);
    EXPECT_EQ_U8("bad submode no apply word",
                 fpga_meter_stock_apply_cmd_word_for_submode(99, NULL) ? 1U : 0U, 0U);
    EXPECT_EQ_U8("bad plan stock", plan.stock_mode,
                 FPGA_METER_INVALID_STOCK_MODE);
    EXPECT_EQ_U8("bad plan mux", plan.mux_index,
                 FPGA_METER_INVALID_STOCK_MODE);
    EXPECT_EQ_U8("bad plan portc/porte mux", plan.portc_porte_mux,
                 FPGA_METER_INVALID_STOCK_MODE);
    EXPECT_EQ_U8("bad plan porta/portb mux", plan.porta_portb_mux,
                 FPGA_METER_INVALID_STOCK_MODE);
    EXPECT_EQ_U8("bad plan family", plan.frame_family,
                 FPGA_METER_FRAME_FAMILY_INVALID);
    EXPECT_EQ_U16("bad plan selector", plan.selector_word,
                  FPGA_METER_INVALID_SELECTOR_WORD);
    EXPECT_EQ_U8("bad plan has no apply", plan.has_apply_word ? 1U : 0U, 0U);
    EXPECT_EQ_U8("bad plan discard", plan.discard_frames, 0U);
    EXPECT_EQ_U16("bad plan settle", plan.settle_ms, 0U);
    EXPECT_EQ_U8("bad plan voltage axis",
                 plan.voltage_function_axis ? 1U : 0U, 0U);
}

static void test_rx_frame_gate_preserves_discard_budget_while_busy(void)
{
    uint8_t discard = FPGA_METER_TRANSITION_DISCARD_FRAMES;
    uint32_t transition_skips = 0;

    EXPECT_EQ_U8("busy frame does not parse",
                 fpga_meter_rx_frame_should_parse(true, &discard, &transition_skips) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("busy frame keeps discard budget",
                 discard, FPGA_METER_TRANSITION_DISCARD_FRAMES);
    EXPECT_EQ_U8("busy frame increments transition skips",
                 transition_skips, 1U);

    EXPECT_EQ_U8("first post-transition frame is discarded",
                 fpga_meter_rx_frame_should_parse(false, &discard, &transition_skips) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("first discard consumed", discard, 1U);
    EXPECT_EQ_U8("discard does not increment transition skips",
                 transition_skips, 1U);

    EXPECT_EQ_U8("second post-transition frame is discarded",
                 fpga_meter_rx_frame_should_parse(false, &discard, &transition_skips) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("discard budget drained", discard, 0U);

    EXPECT_EQ_U8("next stable frame parses",
                 fpga_meter_rx_frame_should_parse(false, &discard, &transition_skips) ? 1U : 0U,
                 1U);
    EXPECT_EQ_U8("stable frame keeps discard drained", discard, 0U);
    EXPECT_EQ_U8("stable frame keeps transition skips",
                 transition_skips, 1U);
}

int main(void)
{
    test_stock_table_bytes();
    test_local_submode_mapping();
    test_wire_words_are_raw_05_family();
    test_stock_apply_words_for_runtime_family_switch();
    test_transition_plan_covers_mux_family_and_settle_policy();
    test_state_machine_contract_is_exhaustive();
    test_fallbacks();
    test_rx_frame_gate_preserves_discard_budget_while_busy();

    if (failures) {
        printf("%d fpga meter plan test(s) failed\n", failures);
        return 1;
    }

    printf("fpga meter plan tests passed\n");
    return 0;
}
