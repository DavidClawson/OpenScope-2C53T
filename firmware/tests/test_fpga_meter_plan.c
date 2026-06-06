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

static void expect_mux_state(const char *label,
                             const fpga_meter_mux_gpio_state_t *got,
                             const fpga_meter_mux_gpio_state_t *want)
{
    char name[96];

    snprintf(name, sizeof(name), "%s pc12", label);
    EXPECT_EQ_U8(name, got->pc12, want->pc12);
    snprintf(name, sizeof(name), "%s pe4", label);
    EXPECT_EQ_U8(name, got->pe4, want->pe4);
    snprintf(name, sizeof(name), "%s pe5", label);
    EXPECT_EQ_U8(name, got->pe5, want->pe5);
    snprintf(name, sizeof(name), "%s pe6", label);
    EXPECT_EQ_U8(name, got->pe6, want->pe6);
    snprintf(name, sizeof(name), "%s pa15", label);
    EXPECT_EQ_U8(name, got->pa15, want->pa15);
    snprintf(name, sizeof(name), "%s pa10", label);
    EXPECT_EQ_U8(name, got->pa10, want->pa10);
    snprintf(name, sizeof(name), "%s pb10", label);
    EXPECT_EQ_U8(name, got->pb10, want->pb10);
    snprintf(name, sizeof(name), "%s pb11", label);
    EXPECT_EQ_U8(name, got->pb11, want->pb11);
    snprintf(name, sizeof(name), "%s pb9", label);
    EXPECT_EQ_U8(name, got->pb9, want->pb9);
    snprintf(name, sizeof(name), "%s pa6", label);
    EXPECT_EQ_U8(name, got->pa6, want->pa6);
}

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

static void test_logical_function_capability_matrix_covers_all_dmm_modes(void)
{
    static const uint8_t expected_submode[FPGA_METER_LOGICAL_FUNCTION_COUNT] = {
        0,
        1,
        FPGA_METER_INVALID_LOCAL_SUBMODE,
        2,
        3,
        FPGA_METER_INVALID_LOCAL_SUBMODE,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
    };
    uint16_t seen_functions = 0;
    uint16_t seen_supported_submodes = 0;

    for (uint8_t fn = 0; fn < FPGA_METER_LOGICAL_FUNCTION_COUNT; fn++) {
        char name[72];
        uint8_t submode = fpga_meter_submode_for_logical_function(fn);

        seen_functions |= (uint16_t)(1U << fn);
        if (fpga_meter_submode_is_valid(submode)) {
            seen_supported_submodes |= (uint16_t)(1U << submode);
        }

        snprintf(name, sizeof(name), "logical function valid %u", (unsigned)fn);
        EXPECT_EQ_U8(name,
                     fpga_meter_logical_function_is_valid(fn) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "logical function submode %u", (unsigned)fn);
        EXPECT_EQ_U8(name, submode, expected_submode[fn]);
        snprintf(name, sizeof(name), "logical function supported %u", (unsigned)fn);
        EXPECT_EQ_U8(name,
                     fpga_meter_logical_function_is_supported(fn) ? 1U : 0U,
                     expected_submode[fn] != FPGA_METER_INVALID_LOCAL_SUBMODE);
        snprintf(name, sizeof(name), "logical function unresolved %u", (unsigned)fn);
        EXPECT_EQ_U8(name,
                     fpga_meter_logical_function_is_unresolved(fn) ? 1U : 0U,
                     expected_submode[fn] == FPGA_METER_INVALID_LOCAL_SUBMODE);
    }

    EXPECT_EQ_U16("all logical DMM functions covered", seen_functions,
                  (uint16_t)((1U << FPGA_METER_LOGICAL_FUNCTION_COUNT) - 1U));
    EXPECT_EQ_U16("all supported local submodes represented", seen_supported_submodes,
                  (uint16_t)((1U << FPGA_METER_LOCAL_SUBMODE_COUNT) - 1U));
    EXPECT_EQ_U8("DC uA is unresolved",
                 fpga_meter_logical_function_is_unresolved(FPGA_METER_FUNCTION_DC_UA) ? 1U : 0U,
                 1U);
    EXPECT_EQ_U8("AC uA is unresolved",
                 fpga_meter_logical_function_is_unresolved(FPGA_METER_FUNCTION_AC_UA) ? 1U : 0U,
                 1U);
    EXPECT_EQ_U8("invalid logical function rejected",
                 fpga_meter_logical_function_is_valid(FPGA_METER_LOGICAL_FUNCTION_COUNT) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("invalid logical function maps invalid submode",
                 fpga_meter_submode_for_logical_function(FPGA_METER_LOGICAL_FUNCTION_COUNT),
                 FPGA_METER_INVALID_LOCAL_SUBMODE);
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
    /*
     * Stock dynamic raw-word helper at 0x08006120 chooses only these
     * selector/apply low-byte pairs for runtime family-side switching:
     * ACV 0x0C/0x0D, DCA 0x17/0x0E, continuity 0x11/0x16, diode 0x10/0x15.
     * Keep the local apply table as a subset of that recovered pair set; do
     * not add an apply word for a surprising range without new stock xrefs.
     */
    static const uint16_t expected_apply[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        0x0000, 0x050D, 0x050E, 0x050E, 0x0000,
        0x0000, 0x0000, 0x0516, 0x0515, 0x0000,
        0x0000
    };
    static const uint16_t stock_dynamic_apply_words[] = {
        0x050D, 0x050E, 0x0516, 0x0515
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
            {
                uint8_t found = 0;
                for (uint8_t j = 0;
                     j < sizeof(stock_dynamic_apply_words) /
                         sizeof(stock_dynamic_apply_words[0]);
                     j++) {
                    if (word == stock_dynamic_apply_words[j]) {
                        found = 1;
                    }
                }
                snprintf(name, sizeof(name), "apply stock pair %u", (unsigned)i);
                EXPECT_EQ_U8(name, found, 1U);
            }
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
        snprintf(name, sizeof(name), "plan probe detect %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.has_probe_detect ? 1U : 0U, 1U);
        snprintf(name, sizeof(name), "plan start word %u", (unsigned)i);
        EXPECT_EQ_U16(name, plan.start_word, FPGA_METER_START_WORD);
        snprintf(name, sizeof(name), "plan voltage axis %u", (unsigned)i);
        EXPECT_EQ_U8(name, plan.voltage_function_axis ? 1U : 0U,
                     expected_family[i] == FPGA_METER_FRAME_FAMILY_VOLTAGE ?
                     1U : 0U);
    }
}

static void test_mux_gpio_state_matches_stock_projection_for_every_submode(void)
{
    static const fpga_meter_mux_gpio_state_t expected[FPGA_METER_LOCAL_SUBMODE_COUNT] = {
        { 1, 1, 0, 1, 1, 1, 0, 1, 0, 0 },
        { 1, 1, 0, 1, 1, 1, 1, 1, 0, 0 },
        { 1, 1, 1, 0, 1, 0, 1, 0, 0, 0 },
        { 1, 1, 1, 0, 1, 0, 1, 0, 0, 0 },
        { 1, 1, 0, 0, 1, 0, 0, 1, 0, 0 },
        { 1, 1, 0, 0, 1, 0, 0, 1, 0, 0 },
        { 1, 1, 1, 0, 1, 0, 1, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 1, 0, 1, 1, 0, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 0 },
    };

    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[48];
        fpga_meter_mux_gpio_state_t state;

        snprintf(name, sizeof(name), "mux gpio submode %u", (unsigned)i);
        EXPECT_EQ_U8(name,
                     fpga_meter_mux_gpio_state_for_submode(i, &state) ? 1U : 0U,
                     1U);
        expect_mux_state(name, &state, &expected[i]);
    }
}

static void test_mux_writer_stock_arm_truth_table_covers_all_10_switch_arms(void)
{
    /*
     * The stock mux writers are 10-way switch bodies.  Local DMM submodes only
     * project the recovered stock slots 0..7 today, but arms 8/9 are still
     * recovered hardware states and must stay explicit unresolved evidence,
     * not become hidden low-DCV/current fixes by folklore.
     */
    static const fpga_meter_mux_gpio_state_t expected[10] = {
        { 1, 1, 0, 1, 1, 1, 0, 1, 0, 0 },
        { 1, 1, 0, 1, 1, 1, 1, 1, 0, 0 },
        { 1, 1, 1, 0, 1, 0, 1, 0, 0, 0 },
        { 1, 1, 0, 0, 1, 0, 0, 1, 0, 0 },
        { 1, 1, 1, 0, 1, 0, 1, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 1, 0, 1, 1, 0, 0, 0 },
        { 0, 1, 0, 0, 0, 0, 0, 1, 0, 0 },
        { 0, 1, 1, 0, 0, 0, 1, 1, 0, 0 },
    };

    for (uint8_t arm = 0; arm < 10; arm++) {
        char name[56];
        fpga_meter_mux_gpio_state_t state;

        snprintf(name, sizeof(name), "stock mux arm %u", (unsigned)arm);
        EXPECT_EQ_U8(name,
                     fpga_meter_mux_gpio_state_for_stock_mux_arms(
                         arm, arm, &state) ? 1U : 0U,
                     1U);
        expect_mux_state(name, &state, &expected[arm]);
    }

    {
        fpga_meter_mux_gpio_state_t state;

        EXPECT_EQ_U8("invalid stock mux arm rejected",
                     fpga_meter_mux_gpio_state_for_stock_mux_arms(
                         10, 0, &state) ? 1U : 0U,
                     0U);
        EXPECT_EQ_U8("invalid stock mux arm keeps baseline pc12",
                     state.pc12, 1U);
        EXPECT_EQ_U8("invalid stock mux arm keeps baseline pb9",
                     state.pb9, 0U);
        EXPECT_EQ_U8("null stock mux arm output is rejected",
                     fpga_meter_mux_gpio_state_for_stock_mux_arms(
                         0, 0, NULL) ? 1U : 0U,
                     0U);
    }
}

static void test_transition_settle_discard_policy_is_explicit_for_every_submode(void)
{
    for (uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i++) {
        char name[72];
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(i);

        snprintf(name, sizeof(name), "uniform local settle/discard valid %u",
                 (unsigned)i);
        EXPECT_EQ_U8(name, fpga_meter_submode_is_valid(i) ? 1U : 0U, 1U);
        snprintf(name, sizeof(name), "uniform local discard policy %u",
                 (unsigned)i);
        EXPECT_EQ_U8(name, plan.discard_frames,
                     FPGA_METER_TRANSITION_DISCARD_FRAMES);
        snprintf(name, sizeof(name), "uniform local settle policy %u",
                 (unsigned)i);
        EXPECT_EQ_U16(name, plan.settle_ms,
                      FPGA_METER_TRANSITION_SETTLE_MS);
    }

    {
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(FPGA_METER_LOCAL_SUBMODE_COUNT);

        EXPECT_EQ_U8("invalid submodes emit no settle/discard valid",
                     fpga_meter_submode_is_valid(FPGA_METER_LOCAL_SUBMODE_COUNT) ? 1U : 0U,
                     0U);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard stock",
                     plan.stock_mode, FPGA_METER_INVALID_STOCK_MODE);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard mux",
                     plan.mux_index, FPGA_METER_INVALID_STOCK_MODE);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard frame family",
                     plan.frame_family, FPGA_METER_FRAME_FAMILY_INVALID);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard discard",
                     plan.discard_frames, 0U);
        EXPECT_EQ_U16("invalid submodes emit no settle/discard settle",
                      plan.settle_ms, 0U);
        EXPECT_EQ_U16("invalid submodes emit no settle/discard selector",
                      plan.selector_word, FPGA_METER_INVALID_SELECTOR_WORD);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard apply",
                     plan.has_apply_word ? 1U : 0U, 0U);
        EXPECT_EQ_U8("invalid submodes emit no settle/discard probe",
                     plan.has_probe_detect ? 1U : 0U, 0U);
        EXPECT_EQ_U16("invalid submodes emit no settle/discard start",
                      plan.start_word, 0U);
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

static void test_frame_family_mismatch_policy_matrix_is_exhaustive(void)
{
    static const uint8_t families[] = {
        FPGA_METER_FRAME_FAMILY_VOLTAGE,
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_RESISTANCE,
        FPGA_METER_FRAME_FAMILY_CONTINUITY,
        FPGA_METER_FRAME_FAMILY_DIODE,
        FPGA_METER_FRAME_FAMILY_EXTENDED,
    };

    for (unsigned e = 0; e < sizeof(families); e++) {
        char name[80];

        snprintf(name, sizeof(name), "family %u recovered",
                 (unsigned)families[e]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_is_recovered(families[e]) ? 1U : 0U,
                     1U);

        for (unsigned o = 0; o < sizeof(families); o++) {
            snprintf(name, sizeof(name), "expected %u observed %u",
                     (unsigned)families[e], (unsigned)families[o]);
            EXPECT_EQ_U8(name,
                         fpga_meter_frame_family_is_acceptable(families[e],
                                                               families[o]) ? 1U : 0U,
                         families[e] == families[o] ? 1U : 0U);
        }

        snprintf(name, sizeof(name), "expected %u observed invalid",
                 (unsigned)families[e]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_is_acceptable(
                         families[e], FPGA_METER_FRAME_FAMILY_INVALID) ? 1U : 0U,
                     0U);
    }

    EXPECT_EQ_U8("invalid family not recovered",
                 fpga_meter_frame_family_is_recovered(
                     FPGA_METER_FRAME_FAMILY_INVALID) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("invalid expected rejected",
                 fpga_meter_frame_family_is_acceptable(
                     FPGA_METER_FRAME_FAMILY_INVALID,
                     FPGA_METER_FRAME_FAMILY_VOLTAGE) ? 1U : 0U,
                 0U);
}

static void test_frame_family_marker_visibility_documents_observed_gaps(void)
{
    static const uint8_t marker_visible[] = {
        FPGA_METER_FRAME_FAMILY_VOLTAGE,
        FPGA_METER_FRAME_FAMILY_CONTINUITY,
    };
    static const uint8_t active_plan_only[] = {
        FPGA_METER_FRAME_FAMILY_CURRENT,
        FPGA_METER_FRAME_FAMILY_RESISTANCE,
        FPGA_METER_FRAME_FAMILY_DIODE,
        FPGA_METER_FRAME_FAMILY_EXTENDED,
    };

    for (unsigned i = 0; i < sizeof(marker_visible); i++) {
        char name[80];

        snprintf(name, sizeof(name), "marker-visible family %u recovered",
                 (unsigned)marker_visible[i]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_is_recovered(marker_visible[i]) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "marker-visible family %u has stock marker",
                 (unsigned)marker_visible[i]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_has_stock_marker(marker_visible[i]) ? 1U : 0U,
                     1U);
    }

    for (unsigned i = 0; i < sizeof(active_plan_only); i++) {
        char name[80];

        snprintf(name, sizeof(name), "active-plan family %u recovered",
                 (unsigned)active_plan_only[i]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_is_recovered(active_plan_only[i]) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "active-plan family %u has no stock marker",
                 (unsigned)active_plan_only[i]);
        EXPECT_EQ_U8(name,
                     fpga_meter_frame_family_has_stock_marker(active_plan_only[i]) ? 1U : 0U,
                     0U);
    }

    EXPECT_EQ_U8("invalid family has no stock marker",
                 fpga_meter_frame_family_has_stock_marker(
                     FPGA_METER_FRAME_FAMILY_INVALID) ? 1U : 0U,
                 0U);
}

static void test_local_splits_do_not_invent_extra_stock_selectors(void)
{
    static const struct {
        uint8_t a;
        uint8_t b;
        const char *label;
    } shared_slots[] = {
        { 2, 3, "DC current small/A" },
        { 4, 5, "AC current small/A" },
        { 9, 10, "capacitance/temperature" },
    };

    for (unsigned i = 0; i < sizeof(shared_slots) / sizeof(shared_slots[0]); i++) {
        char name[80];
        fpga_meter_transition_plan_t a =
            fpga_meter_transition_plan_for_submode(shared_slots[i].a);
        fpga_meter_transition_plan_t b =
            fpga_meter_transition_plan_for_submode(shared_slots[i].b);

        snprintf(name, sizeof(name), "%s stock slot", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.stock_mode, b.stock_mode);
        snprintf(name, sizeof(name), "%s selector", shared_slots[i].label);
        EXPECT_EQ_U16(name, a.selector_word, b.selector_word);
        snprintf(name, sizeof(name), "%s Port C/E mux", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.portc_porte_mux, b.portc_porte_mux);
        snprintf(name, sizeof(name), "%s Port A/B mux", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.porta_portb_mux, b.porta_portb_mux);
        snprintf(name, sizeof(name), "%s frame family", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.frame_family, b.frame_family);
        snprintf(name, sizeof(name), "%s apply presence", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.has_apply_word ? 1U : 0U,
                     b.has_apply_word ? 1U : 0U);
        snprintf(name, sizeof(name), "%s apply word", shared_slots[i].label);
        EXPECT_EQ_U16(name, a.apply_word, b.apply_word);
        snprintf(name, sizeof(name), "%s probe tail", shared_slots[i].label);
        EXPECT_EQ_U8(name, a.has_probe_detect ? 1U : 0U,
                     b.has_probe_detect ? 1U : 0U);
        snprintf(name, sizeof(name), "%s start word", shared_slots[i].label);
        EXPECT_EQ_U16(name, a.start_word, b.start_word);
    }
}

static void test_local_splits_share_mux_gpio_state(void)
{
    static const struct {
        uint8_t a;
        uint8_t b;
        const char *label;
    } shared_slots[] = {
        { 2, 3, "DC current small/A" },
        { 4, 5, "AC current small/A" },
        { 9, 10, "capacitance/temperature" },
    };

    for (unsigned i = 0; i < sizeof(shared_slots) / sizeof(shared_slots[0]); i++) {
        fpga_meter_mux_gpio_state_t a;
        fpga_meter_mux_gpio_state_t b;
        char name[80];

        snprintf(name, sizeof(name), "%s mux gpio A", shared_slots[i].label);
        EXPECT_EQ_U8(name,
                     fpga_meter_mux_gpio_state_for_submode(shared_slots[i].a, &a) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "%s mux gpio B", shared_slots[i].label);
        EXPECT_EQ_U8(name,
                     fpga_meter_mux_gpio_state_for_submode(shared_slots[i].b, &b) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "%s mux gpio shared", shared_slots[i].label);
        expect_mux_state(name, &a, &b);
    }
}

static void test_fallbacks(void)
{
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(99);
    fpga_meter_mux_gpio_state_t state = { 0 };

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
    EXPECT_EQ_U8("bad plan has no probe", plan.has_probe_detect ? 1U : 0U, 0U);
    EXPECT_EQ_U16("bad plan has no start", plan.start_word, 0U);
    EXPECT_EQ_U8("bad plan discard", plan.discard_frames, 0U);
    EXPECT_EQ_U16("bad plan settle", plan.settle_ms, 0U);
    EXPECT_EQ_U8("bad plan voltage axis",
                 plan.voltage_function_axis ? 1U : 0U, 0U);
    EXPECT_EQ_U8("bad submode no mux gpio state",
                 fpga_meter_mux_gpio_state_for_submode(99, &state) ? 1U : 0U,
                 0U);
    EXPECT_EQ_U8("bad submode keeps baseline pc12", state.pc12, 1U);
    EXPECT_EQ_U8("null mux gpio state is rejected",
                 fpga_meter_mux_gpio_state_for_submode(0, NULL) ? 1U : 0U,
                 0U);
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

static void test_every_submode_transition_drains_before_accepting_frames(void)
{
    for (uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode++) {
        char name[96];
        fpga_meter_transition_plan_t plan =
            fpga_meter_transition_plan_for_submode(mode);
        uint8_t discard = plan.discard_frames;
        uint32_t transition_skips = 0;

        snprintf(name, sizeof(name), "mode %u busy frame rejected", (unsigned)mode);
        EXPECT_EQ_U8(name,
                     fpga_meter_rx_frame_should_parse(true, &discard, &transition_skips) ? 1U : 0U,
                     0U);
        snprintf(name, sizeof(name), "mode %u busy keeps full discard", (unsigned)mode);
        EXPECT_EQ_U8(name, discard, plan.discard_frames);
        snprintf(name, sizeof(name), "mode %u busy skip counted", (unsigned)mode);
        EXPECT_EQ_U8(name, transition_skips, 1U);

        for (uint8_t i = 0; i < plan.discard_frames; i++) {
            snprintf(name, sizeof(name), "mode %u discard frame %u",
                     (unsigned)mode, (unsigned)i);
            EXPECT_EQ_U8(name,
                         fpga_meter_rx_frame_should_parse(false, &discard, &transition_skips) ? 1U : 0U,
                         0U);
        }
        snprintf(name, sizeof(name), "mode %u discard drained", (unsigned)mode);
        EXPECT_EQ_U8(name, discard, 0U);
        snprintf(name, sizeof(name), "mode %u stable frame accepted", (unsigned)mode);
        EXPECT_EQ_U8(name,
                     fpga_meter_rx_frame_should_parse(false, &discard, &transition_skips) ? 1U : 0U,
                     1U);
        snprintf(name, sizeof(name), "mode %u stable keeps busy skip count", (unsigned)mode);
        EXPECT_EQ_U8(name, transition_skips, 1U);
    }
}

int main(void)
{
    test_stock_table_bytes();
    test_local_submode_mapping();
    test_logical_function_capability_matrix_covers_all_dmm_modes();
    test_wire_words_are_raw_05_family();
    test_stock_apply_words_for_runtime_family_switch();
    test_transition_plan_covers_mux_family_and_settle_policy();
    test_mux_gpio_state_matches_stock_projection_for_every_submode();
    test_mux_writer_stock_arm_truth_table_covers_all_10_switch_arms();
    test_transition_settle_discard_policy_is_explicit_for_every_submode();
    test_state_machine_contract_is_exhaustive();
    test_frame_family_mismatch_policy_matrix_is_exhaustive();
    test_frame_family_marker_visibility_documents_observed_gaps();
    test_local_splits_do_not_invent_extra_stock_selectors();
    test_local_splits_share_mux_gpio_state();
    test_fallbacks();
    test_rx_frame_gate_preserves_discard_budget_while_busy();
    test_every_submode_transition_drains_before_accepting_frames();

    if (failures) {
        printf("%d fpga meter plan test(s) failed\n", failures);
        return 1;
    }

    printf("fpga meter plan tests passed\n");
    return 0;
}
