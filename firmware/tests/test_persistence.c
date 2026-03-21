/*
 * Unit tests for persistence display module.
 * Build: gcc -o tests/test_persistence tests/test_persistence.c src/ui/persistence.c -lm -Isrc/ui
 * Run:   ./tests/test_persistence
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "persistence.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s — expected %d, got %d (line %d)\n", msg, (int)(b), (int)(a), __LINE__); \
        tests_failed++; \
        return; \
    } \
} while (0)

/* ------------------------------------------------------------------ */
/* Test 1: Init clears all pixels                                      */
/* ------------------------------------------------------------------ */
static void test_init(void)
{
    printf("Test 1: Init clears all pixels\n");
    persist_init();

    const uint8_t *buf = persist_get_buffer();
    for (uint32_t i = 0; i < PERSIST_WIDTH * PERSIST_HEIGHT; i++) {
        ASSERT(buf[i] == 0, "pixel should be 0 after init");
    }
    ASSERT_EQ(persist_get_mode(), PERSIST_OFF, "mode should be OFF after init");
    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: Add trace sets intensity                                    */
/* ------------------------------------------------------------------ */
static void test_add_trace(void)
{
    printf("Test 2: Add horizontal trace at y=100\n");
    persist_init();
    persist_set_mode(PERSIST_INFINITE);

    uint16_t y_values[PERSIST_WIDTH];
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = 100;
    }
    persist_add_trace(y_values, 0);

    /* Check that the trace line has full intensity */
    for (int x = 0; x < PERSIST_WIDTH; x++) {
        ASSERT_EQ(persist_get_intensity(x, 100), 255, "trace pixel should be 255");
    }
    /* Check neighboring rows have thickness intensity */
    for (int x = 0; x < PERSIST_WIDTH; x++) {
        ASSERT_EQ(persist_get_intensity(x, 99), 192, "row above should be 192");
        ASSERT_EQ(persist_get_intensity(x, 101), 192, "row below should be 192");
    }
    /* Check that a distant row is still 0 */
    ASSERT_EQ(persist_get_intensity(0, 50), 0, "distant pixel should be 0");

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: Decay LOW clears pixels within 20 frames                    */
/* ------------------------------------------------------------------ */
static void test_decay_low(void)
{
    printf("Test 3: Decay LOW clears pixels within 20 frames\n");
    persist_init();
    persist_set_mode(PERSIST_LOW);

    uint16_t y_values[PERSIST_WIDTH];
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = 100;
    }
    persist_add_trace(y_values, 0);

    /* Decay 20 times (255 / 16 = ~16 needed, 20 gives margin) */
    for (int i = 0; i < 20; i++) {
        persist_decay();
    }

    for (int x = 0; x < PERSIST_WIDTH; x++) {
        ASSERT_EQ(persist_get_intensity(x, 100), 0, "pixel should be 0 after 20 LOW decays");
        ASSERT_EQ(persist_get_intensity(x, 99), 0, "neighbor should be 0 after 20 LOW decays");
    }

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: Decay INFINITE preserves pixels                             */
/* ------------------------------------------------------------------ */
static void test_decay_infinite(void)
{
    printf("Test 4: Decay INFINITE preserves pixels after 100 frames\n");
    persist_init();
    persist_set_mode(PERSIST_INFINITE);

    uint16_t y_values[PERSIST_WIDTH];
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = 100;
    }
    persist_add_trace(y_values, 0);

    for (int i = 0; i < 100; i++) {
        persist_decay();
    }

    for (int x = 0; x < PERSIST_WIDTH; x++) {
        ASSERT_EQ(persist_get_intensity(x, 100), 255, "pixel should remain 255 in INFINITE mode");
    }

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: Multiple traces at different y positions                    */
/* ------------------------------------------------------------------ */
static void test_multiple_traces(void)
{
    printf("Test 5: Multiple traces at different y positions\n");
    persist_init();
    persist_set_mode(PERSIST_INFINITE);

    /* Add 10 traces at y=90 through y=99 */
    for (int t = 0; t < 10; t++) {
        uint16_t y_values[PERSIST_WIDTH];
        for (int i = 0; i < PERSIST_WIDTH; i++) {
            y_values[i] = 90 + t;
        }
        persist_add_trace(y_values, 0);
    }

    /* All 10 trace rows should have intensity 255 */
    for (int t = 0; t < 10; t++) {
        ASSERT_EQ(persist_get_intensity(0, 90 + t), 255, "each trace row should be 255");
    }
    /* Row 89 should have thickness from the y=90 trace */
    ASSERT_EQ(persist_get_intensity(0, 89), 192, "row above first trace should be 192");

    /* Row 50 should be untouched */
    ASSERT_EQ(persist_get_intensity(0, 50), 0, "distant row should be 0");

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: Color mapping                                               */
/* ------------------------------------------------------------------ */
static void test_color_mapping(void)
{
    printf("Test 6: Color mapping\n");

    /* CH1 yellow: intensity 255 -> full yellow 0xFFE0 */
    uint16_t c1_full = persist_intensity_to_color_ch1(255);
    ASSERT_EQ(c1_full, 0xFFE0, "CH1 intensity 255 should be 0xFFE0");

    /* CH1 yellow: intensity 0 -> black */
    uint16_t c1_zero = persist_intensity_to_color_ch1(0);
    ASSERT_EQ(c1_zero, 0x0000, "CH1 intensity 0 should be 0x0000");

    /* CH2 cyan: intensity 255 -> full cyan 0x07FF */
    uint16_t c2_full = persist_intensity_to_color_ch2(255);
    ASSERT_EQ(c2_full, 0x07FF, "CH2 intensity 255 should be 0x07FF");

    /* CH2 cyan: intensity 0 -> black */
    uint16_t c2_zero = persist_intensity_to_color_ch2(0);
    ASSERT_EQ(c2_zero, 0x0000, "CH2 intensity 0 should be 0x0000");

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: Out-of-bounds y values are ignored                          */
/* ------------------------------------------------------------------ */
static void test_bounds(void)
{
    printf("Test 7: Out-of-bounds y values are ignored\n");
    persist_init();
    persist_set_mode(PERSIST_INFINITE);

    uint16_t y_values[PERSIST_WIDTH];
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = PERSIST_HEIGHT + 10; /* out of bounds */
    }
    persist_add_trace(y_values, 0);

    /* Buffer should still be all zeros */
    const uint8_t *buf = persist_get_buffer();
    for (uint32_t i = 0; i < PERSIST_WIDTH * PERSIST_HEIGHT; i++) {
        ASSERT(buf[i] == 0, "pixel should remain 0 for OOB trace");
    }

    /* Also test y=0 edge (should not write to y=-1) */
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = 0;
    }
    persist_add_trace(y_values, 0);
    ASSERT_EQ(persist_get_intensity(0, 0), 255, "y=0 trace should work");
    ASSERT_EQ(persist_get_intensity(0, 1), 192, "y=1 neighbor should be 192");

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: Clear resets buffer                                         */
/* ------------------------------------------------------------------ */
static void test_clear(void)
{
    printf("Test 8: Clear resets buffer\n");
    persist_init();
    persist_set_mode(PERSIST_INFINITE);

    uint16_t y_values[PERSIST_WIDTH];
    for (int i = 0; i < PERSIST_WIDTH; i++) {
        y_values[i] = 100;
    }
    persist_add_trace(y_values, 0);

    /* Verify something is there */
    ASSERT_EQ(persist_get_intensity(0, 100), 255, "pre-clear should have data");

    persist_clear();

    const uint8_t *buf = persist_get_buffer();
    for (uint32_t i = 0; i < PERSIST_WIDTH * PERSIST_HEIGHT; i++) {
        ASSERT(buf[i] == 0, "pixel should be 0 after clear");
    }

    tests_passed++;
    printf("  PASS\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("=== Persistence Display Module Tests ===\n\n");

    test_init();
    test_add_trace();
    test_decay_low();
    test_decay_infinite();
    test_multiple_traces();
    test_color_mapping();
    test_bounds();
    test_clear();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
