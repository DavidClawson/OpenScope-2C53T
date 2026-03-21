/*
 * Unit tests for XY Mode, Roll Mode, and Trend Plot
 *
 * Build:
 *   gcc -o tests/test_xy_roll_trend tests/test_xy_roll_trend.c \
 *       src/ui/xy_mode.c src/ui/roll_mode.c src/ui/trend_plot.c \
 *       -lm -Isrc/ui -DTEST_BUILD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "xy_mode.h"
#include "roll_mode.h"
#include "trend_plot.h"

/* --------------------------------------------------------------------------
 * Test framework
 * -------------------------------------------------------------------------- */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", #name); \
    if (test_##name()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAIL\n"); } \
} while(0)

#define ASSERT(cond) do { if (!(cond)) { printf("[line %d: %s] ", __LINE__, #cond); return 0; } } while(0)
#define ASSERT_FLOAT_EQ(a, b, eps) ASSERT(fabsf((a) - (b)) < (eps))

/* --------------------------------------------------------------------------
 * LCD stubs shared by roll_mode.c and trend_plot.c (xy_mode.c has its own)
 * -------------------------------------------------------------------------- */

/* We need a shared pixel buffer for roll and trend tests */
#define FB_W 320
#define FB_H 240
/* Framebuffer used by lcd_set_pixel stub in xy_mode.c (via xy_test_get_fb) */

/* These are the lcd stubs that roll_mode.c and trend_plot.c will link against.
 * xy_mode.c has its own internal stubs via TEST_BUILD. However, since we're
 * compiling all together, we need a single definition. The xy_mode.c stubs
 * are static (internal), so they don't conflict. Actually, xy_mode.c defines
 * lcd_set_pixel non-static... We'll handle this by having a single definition. */

/* xy_mode.c's test stub defines lcd_set_pixel. We rely on that one. */
/* roll_mode.c and trend_plot.c declare lcd_set_pixel as extern in TEST_BUILD. */
/* So xy_mode.c's definition satisfies all three. */

/* lcd_draw_string stub for trend_plot.c */
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    (void)x; (void)y; (void)str; (void)fg; (void)bg;
}

/* Get access to xy_mode.c's test framebuffer */
extern uint16_t *xy_test_get_fb(void);
extern void xy_test_clear_fb(void);

/* ========================================================================
 * XY Mode Tests
 * ======================================================================== */

/* Same signal on both channels -> points along diagonal (y=x) */
static int test_xy_diagonal(void)
{
    int16_t ch1[100], ch2[100];
    /* Both channels have the same ramp */
    for (int i = 0; i < 100; i++) {
        int16_t val = (int16_t)(-32768 + i * 65535 / 99);
        ch1[i] = val;
        ch2[i] = val;
    }

    xy_test_clear_fb();
    xy_render(ch1, ch2, 100, 10, 10, 200, 200, 0xFFFF);

    /* Check that pixels are set along or near the diagonal.
     * For same input on both axes, the Y is inverted so:
     * ch1=ch2=-32768 -> bottom-left, ch1=ch2=32767 -> top-right
     * This means py = y_off + height - 1 - px_relative, i.e. anti-diagonal. */
    uint16_t *fb = xy_test_get_fb();
    int diagonal_hits = 0;
    for (int i = 0; i < 200; i++) {
        /* Expected: px = 10 + i, py = 10 + 199 - i (anti-diagonal due to Y inversion) */
        uint16_t px = 10 + i;
        uint16_t py = 10 + 199 - i;
        /* Check a small neighborhood (2x2 dot rendering) */
        if (fb[py * FB_W + px] != 0 ||
            (px + 1 < FB_W && fb[py * FB_W + px + 1] != 0) ||
            (py > 0 && fb[(py - 1) * FB_W + px] != 0))
            diagonal_hits++;
    }
    /* At least 80% of diagonal points should be hit */
    ASSERT(diagonal_hits > 80);
    return 1;
}

/* 90 degree phase shift -> circle/ellipse */
static int test_xy_circle(void)
{
    int16_t ch1[360], ch2[360];
    for (int i = 0; i < 360; i++) {
        double angle = (double)i * 3.14159265 / 180.0;
        ch1[i] = (int16_t)(16000.0 * cos(angle));
        ch2[i] = (int16_t)(16000.0 * sin(angle));
    }

    xy_test_clear_fb();
    xy_render(ch1, ch2, 360, 0, 0, 200, 200, 0xFFFF);

    /* Check that pixels appear in all 4 quadrants of the display area */
    uint16_t *fb = xy_test_get_fb();
    int q1 = 0, q2 = 0, q3 = 0, q4 = 0;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 200; x++) {
            if (fb[y * FB_W + x] != 0) {
                if (x < 100 && y < 100) q1++;
                if (x >= 100 && y < 100) q2++;
                if (x < 100 && y >= 100) q3++;
                if (x >= 100 && y >= 100) q4++;
            }
        }
    }
    ASSERT(q1 > 0 && q2 > 0 && q3 > 0 && q4 > 0);
    return 1;
}

/* Bounds checking: output coordinates stay within display area */
static int test_xy_bounds(void)
{
    int16_t ch1[10], ch2[10];
    /* Extreme values */
    ch1[0] = -32768; ch2[0] = -32768;
    ch1[1] =  32767; ch2[1] =  32767;
    ch1[2] = -32768; ch2[2] =  32767;
    ch1[3] =  32767; ch2[3] = -32768;
    ch1[4] =  0;     ch2[4] =  0;
    /* Fill rest */
    for (int i = 5; i < 10; i++) { ch1[i] = 0; ch2[i] = 0; }

    uint16_t x_off = 20, y_off = 30, w = 100, h = 80;
    xy_test_clear_fb();
    xy_render(ch1, ch2, 10, x_off, y_off, w, h, 0xFFFF);

    /* Verify no pixels are set outside the display area */
    uint16_t *fb = xy_test_get_fb();
    for (int y = 0; y < FB_H; y++) {
        for (int x = 0; x < FB_W; x++) {
            if (fb[y * FB_W + x] != 0) {
                ASSERT(x >= x_off && x < x_off + w);
                ASSERT(y >= y_off && y < y_off + h);
            }
        }
    }
    return 1;
}

/* ========================================================================
 * Roll Mode Tests
 * ======================================================================== */

/* Add 320 samples -> buffer full */
static int test_roll_fill_buffer(void)
{
    roll_state_t state;
    roll_init(&state, 0.001f);

    for (int i = 0; i < 320; i++) {
        roll_add_sample(&state, (int16_t)i, (int16_t)(-i));
    }

    ASSERT(state.count == 320);
    ASSERT(state.write_pos == 0);  /* Wrapped around */
    /* Verify last sample written correctly */
    ASSERT(state.ch1[319] == 319);
    ASSERT(state.ch2[319] == -319);
    return 1;
}

/* Add 321st sample -> wraps correctly, oldest dropped */
static int test_roll_wrap(void)
{
    roll_state_t state;
    roll_init(&state, 0.001f);

    for (int i = 0; i < 321; i++) {
        roll_add_sample(&state, (int16_t)(i * 10), (int16_t)(i * 5));
    }

    ASSERT(state.count == 320);        /* Saturated at buffer size */
    ASSERT(state.write_pos == 1);      /* One past the wrap point */
    /* Position 0 holds the 321st sample (index 320) */
    ASSERT(state.ch1[0] == (int16_t)(320 * 10));
    ASSERT(state.ch2[0] == (int16_t)(320 * 5));
    /* Position 1 holds the 2nd sample (index 1), now the oldest */
    ASSERT(state.ch1[1] == (int16_t)(1 * 10));
    return 1;
}

/* Time span = 320 * time_per_pixel */
static int test_roll_time_span(void)
{
    roll_state_t state;
    roll_init(&state, 0.005f);

    float span = roll_get_time_span(&state);
    ASSERT_FLOAT_EQ(span, 320.0f * 0.005f, 0.0001f);
    return 1;
}

/* ========================================================================
 * Trend Plot Tests
 * ======================================================================== */

/* Add constant value -> min=max=value */
static int test_trend_constant(void)
{
    trend_state_t state;
    trend_init(&state, TREND_FREQUENCY, 1.0f);

    for (int i = 0; i < 50; i++) {
        trend_add_point(&state, 100.0f);
    }

    ASSERT_FLOAT_EQ(trend_get_min(&state), 100.0f, 0.001f);
    ASSERT_FLOAT_EQ(trend_get_max(&state), 100.0f, 0.001f);
    ASSERT_FLOAT_EQ(trend_get_avg(&state), 100.0f, 0.001f);
    return 1;
}

/* Add increasing values -> auto-scale expands */
static int test_trend_autoscale(void)
{
    trend_state_t state;
    trend_init(&state, TREND_VPP, 0.5f);

    /* Add values 0..99 */
    for (int i = 0; i < 100; i++) {
        trend_add_point(&state, (float)i);
    }

    /* min_val should be below 0, max_val should be above 99 (10% margin) */
    ASSERT(state.min_val < 0.0f);
    ASSERT(state.max_val > 99.0f);

    /* Raw min/max should be exact */
    ASSERT_FLOAT_EQ(trend_get_min(&state), 0.0f, 0.001f);
    ASSERT_FLOAT_EQ(trend_get_max(&state), 99.0f, 0.001f);
    return 1;
}

/* Statistics: min/max/avg correct */
static int test_trend_statistics(void)
{
    trend_state_t state;
    trend_init(&state, TREND_VRMS, 1.0f);

    trend_add_point(&state, 10.0f);
    trend_add_point(&state, 20.0f);
    trend_add_point(&state, 30.0f);

    ASSERT_FLOAT_EQ(trend_get_min(&state), 10.0f, 0.001f);
    ASSERT_FLOAT_EQ(trend_get_max(&state), 30.0f, 0.001f);
    ASSERT_FLOAT_EQ(trend_get_avg(&state), 20.0f, 0.001f);
    return 1;
}

/* Source names */
static int test_trend_source_names(void)
{
    ASSERT(strcmp(trend_source_name(TREND_FREQUENCY),  "Freq") == 0);
    ASSERT(strcmp(trend_source_name(TREND_VPP),        "Vpp")  == 0);
    ASSERT(strcmp(trend_source_name(TREND_VRMS),       "Vrms") == 0);
    ASSERT(strcmp(trend_source_name(TREND_DUTY_CYCLE), "Duty") == 0);
    return 1;
}

/* ========================================================================
 * Main
 * ======================================================================== */
int main(void)
{
    printf("\n=== XY Mode Tests ===\n");
    TEST(xy_diagonal);
    TEST(xy_circle);
    TEST(xy_bounds);

    printf("\n=== Roll Mode Tests ===\n");
    TEST(roll_fill_buffer);
    TEST(roll_wrap);
    TEST(roll_time_span);

    printf("\n=== Trend Plot Tests ===\n");
    TEST(trend_constant);
    TEST(trend_autoscale);
    TEST(trend_statistics);
    TEST(trend_source_names);

    printf("\n=== Results: %d / %d passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
