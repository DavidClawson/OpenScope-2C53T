/*
 * Unit tests for the FNIRSI 2C53T color theme system.
 * Build: gcc -o tests/test_theme tests/test_theme.c src/ui/theme.c -Isrc/ui -Isrc/drivers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theme.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s", name); \
} while (0)

#define PASS() do { tests_passed++; printf("[PASS]\n"); } while (0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); } while (0)

/* Test 1: Init sets correct theme */
static void test_init_sets_theme(void)
{
    TEST("init sets correct theme");

    theme_init(THEME_CLASSIC_GREEN);
    if (theme_get_id() != THEME_CLASSIC_GREEN) {
        FAIL("expected THEME_CLASSIC_GREEN after init");
        return;
    }

    theme_init(THEME_DARK_BLUE);
    if (theme_get_id() != THEME_DARK_BLUE) {
        FAIL("expected THEME_DARK_BLUE after init");
        return;
    }

    /* Out-of-range should clamp to THEME_DARK_BLUE */
    theme_init(THEME_COUNT);
    if (theme_get_id() != THEME_DARK_BLUE) {
        FAIL("expected THEME_DARK_BLUE for out-of-range id");
        return;
    }

    PASS();
}

/* Test 2: Cycle goes through all 4 and wraps */
static void test_cycle_wraps(void)
{
    TEST("cycle goes through all 4 and wraps");

    theme_init(THEME_DARK_BLUE);

    if (theme_cycle() != THEME_CLASSIC_GREEN) {
        FAIL("expected CLASSIC_GREEN after first cycle");
        return;
    }
    if (theme_cycle() != THEME_HIGH_CONTRAST) {
        FAIL("expected HIGH_CONTRAST after second cycle");
        return;
    }
    if (theme_cycle() != THEME_NIGHT_RED) {
        FAIL("expected NIGHT_RED after third cycle");
        return;
    }
    if (theme_cycle() != THEME_DARK_BLUE) {
        FAIL("expected DARK_BLUE after fourth cycle (wrap)");
        return;
    }

    PASS();
}

/* Test 3: Each theme has non-null name */
static void test_names_non_null(void)
{
    TEST("each theme has non-null name");

    for (int i = 0; i < THEME_COUNT; i++) {
        theme_set((theme_id_t)i);
        const theme_t *t = theme_get();
        if (t->name == NULL) {
            char buf[64];
            snprintf(buf, sizeof(buf), "theme %d has NULL name", i);
            FAIL(buf);
            return;
        }
        if (strlen(t->name) == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "theme %d has empty name", i);
            FAIL(buf);
            return;
        }
    }

    PASS();
}

/* Test 4: Get returns current theme */
static void test_get_returns_current(void)
{
    TEST("get returns current theme");

    theme_set(THEME_NIGHT_RED);
    const theme_t *t = theme_get();
    if (t == NULL) {
        FAIL("theme_get returned NULL");
        return;
    }
    if (strcmp(t->name, "Night Red") != 0) {
        FAIL("expected 'Night Red' name");
        return;
    }

    theme_set(THEME_HIGH_CONTRAST);
    t = theme_get();
    if (t->background != 0xFFFF) {
        FAIL("expected white background for high contrast");
        return;
    }

    PASS();
}

/* Test 5: Colors are different between themes */
static void test_colors_differ(void)
{
    TEST("colors are different between themes");

    theme_set(THEME_DARK_BLUE);
    const theme_t *dark = theme_get();
    uint16_t dark_ch1 = dark->ch1;
    uint16_t dark_bg = dark->background;

    theme_set(THEME_HIGH_CONTRAST);
    const theme_t *hc = theme_get();

    if (dark_ch1 == hc->ch1) {
        FAIL("ch1 should differ between dark blue and high contrast");
        return;
    }
    if (dark_bg == hc->background) {
        FAIL("background should differ between dark blue and high contrast");
        return;
    }

    /* Verify night red ch1 is different from classic green ch1 */
    theme_set(THEME_NIGHT_RED);
    uint16_t night_ch1 = theme_get()->ch1;
    theme_set(THEME_CLASSIC_GREEN);
    uint16_t green_ch1 = theme_get()->ch1;

    if (night_ch1 == green_ch1) {
        FAIL("ch1 should differ between night red and classic green");
        return;
    }

    PASS();
}

int main(void)
{
    printf("Running theme tests...\n\n");

    test_init_sets_theme();
    test_cycle_wraps();
    test_names_non_null();
    test_get_returns_current();
    test_colors_differ();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
