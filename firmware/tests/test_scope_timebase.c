/*
 * Host tests for the scope horizontal (time) calibration.
 *
 * As with test_scope_cal.c, these do not second-guess bench numbers. They test
 * the PROPERTIES that keep this table from failing the way rate figures have
 * already failed on this project:
 *
 *   1. A code with no trustworthy rate returns exactly 0.0f, so a caller that
 *      forgets to check gets an obviously-zero frequency rather than a
 *      plausible one.
 *   2. Code 0x08 in particular returns nothing. Three different rates have
 *      been published for it and all three were artifacts; if someone
 *      re-enters one, this fails.
 *   3. Out-of-range codes refuse rather than wrap.
 *   4. Derived quantities really are derived — s/div from the rate and the
 *      renderer's samples-per-division, Hz from the rate and a period.
 *   5. Labels come from the measurement and are NOT the nominal timebase_table
 *      strings, which are a constant ~2.13x away.
 */

#include "../src/ui/scope_timebase.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                       \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void test_measured_codes(void)
{
    const struct { uint8_t code; float fs; } want[] = {
        { 0x0E, 62958.0f }, { 0x0F, 30235.0f }, { 0x10, 14853.6f },
    };

    for (unsigned i = 0; i < 3; i++) {
        CHECK(fabsf(scope_timebase_sample_rate(want[i].code) - want[i].fs) < 1.0f,
              "code 0x%02X: expected %.1f S/s, got %.1f", want[i].code,
              (double)want[i].fs,
              (double)scope_timebase_sample_rate(want[i].code));
        CHECK(scope_timebase_get_tier(want[i].code) == SCOPE_TB_MEASURED,
              "code 0x%02X should be MEASURED", want[i].code);
    }

    /* The ladder roughly doubles downward; a transcription slip that swapped
     * two rows would break this. */
    CHECK(scope_timebase_sample_rate(0x0F) > 1.8f * scope_timebase_sample_rate(0x10),
          "0x0F should be about double 0x10");
    CHECK(scope_timebase_sample_rate(0x0E) > 1.8f * scope_timebase_sample_rate(0x0F),
          "0x0E should be about double 0x0F");
}

static void test_code_08_stays_withdrawn(void)
{
    /*
     * 0x08 has had three published rates — 1.07 kS/s, 1,660 S/s, and a
     * two-pass 1,414/1,526 — and every one was fitted to a record that does
     * not reproduce between reads. This test exists so that re-entering any of
     * them fails the build.
     */
    CHECK(scope_timebase_sample_rate(0x08) == 0.0f,
          "code 0x08 is INCOHERENT and must return exactly 0.0f, got %.1f",
          (double)scope_timebase_sample_rate(0x08));
    CHECK(scope_timebase_get_tier(0x08) == SCOPE_TB_NONE,
          "code 0x08 must be tier NONE");
    CHECK(scope_timebase_seconds_per_div(0x08) == 0.0f,
          "code 0x08 must have no seconds/div");
    CHECK(scope_timebase_hz_from_period(0x08, 50.0f) == 0.0f,
          "code 0x08 must not yield a frequency");
}

static void test_unmeasured_codes_return_zero(void)
{
    const uint8_t none[] = { 0, 1, 5, 7, 9, 0x0A, 0x0B, 0x0C, 0x11, 0x14 };

    for (unsigned i = 0; i < sizeof(none) / sizeof(none[0]); i++) {
        CHECK(scope_timebase_sample_rate(none[i]) == 0.0f,
              "code 0x%02X has no measured rate and must return 0.0f", none[i]);
        CHECK(scope_timebase_get_tier(none[i]) == SCOPE_TB_NONE,
              "code 0x%02X must be tier NONE", none[i]);
    }
}

static void test_out_of_range(void)
{
    CHECK(scope_timebase_sample_rate(SCOPE_TIMEBASE_CODE_COUNT) == 0.0f,
          "a code past the end must not resolve");
    CHECK(scope_timebase_sample_rate(255) == 0.0f, "code 255 must not resolve");
    CHECK(scope_timebase_get_tier(200) == SCOPE_TB_NONE,
          "an out-of-range tier must be NONE");
}

static void test_derived_quantities(void)
{
    for (uint8_t c = 0; c < SCOPE_TIMEBASE_CODE_COUNT; c++) {
        const float fs = scope_timebase_sample_rate(c);
        if (fs <= 0.0f) continue;

        CHECK(fabsf(scope_timebase_seconds_per_sample(c) - 1.0f / fs) < 1e-12f,
              "code 0x%02X: s/sample must be 1/fs", c);
        CHECK(fabsf(scope_timebase_seconds_per_div(c) -
                    (SCOPE_TIMEBASE_SAMPLES_PER_DIV / fs)) < 1e-12f,
              "code 0x%02X: s/div must be samples-per-div / fs", c);
        CHECK(fabsf(scope_timebase_hz_from_period(c, 100.0f) - fs / 100.0f) < 1e-6f,
              "code 0x%02X: Hz must be fs / period_samples", c);
    }

    /* A non-positive period is not a measurement. */
    CHECK(scope_timebase_hz_from_period(0x10, 0.0f) == 0.0f,
          "zero period must not yield a frequency");
    CHECK(scope_timebase_hz_from_period(0x10, -5.0f) == 0.0f,
          "negative period must not yield a frequency");
}

static void test_labels(void)
{
    char buf[16];

    /* 14853.6 S/s, 32 samples/div -> 2.1544 ms/div. */
    scope_timebase_label(0x10, buf, sizeof(buf));
    CHECK(strcmp(buf, "2.15ms") == 0,
          "0x10 label should be derived as 2.15ms, got \"%s\"", buf);

    /* And it must NOT be the nominal table's "1ms" — that is the whole point.
     * The nominal labels are a constant ~2.13x away on every measured code. */
    CHECK(strcmp(buf, "1ms") != 0, "0x10 must not still read as the nominal 1ms");

    /* 30235 S/s -> 1.058 ms/div. */
    scope_timebase_label(0x0F, buf, sizeof(buf));
    CHECK(strcmp(buf, "1.06ms") == 0,
          "0x0F label should be 1.06ms, got \"%s\"", buf);

    /* 62958 S/s -> 508 us/div, sub-millisecond so microseconds. */
    scope_timebase_label(0x0E, buf, sizeof(buf));
    CHECK(strcmp(buf, "508us") == 0,
          "0x0E label should be 508us, got \"%s\"", buf);

    /* Provisional carries the marker. */
    scope_timebase_label(0x0D, buf, sizeof(buf));
    CHECK(buf[0] == '~', "0x0D is PROVISIONAL and must be marked, got \"%s\"", buf);

    /* No rate -> explicit nothing, never a nominal number. */
    scope_timebase_label(0x08, buf, sizeof(buf));
    CHECK(strcmp(buf, "--") == 0, "0x08 label should be \"--\", got \"%s\"", buf);
    scope_timebase_label(0x00, buf, sizeof(buf));
    CHECK(strcmp(buf, "--") == 0, "0x00 label should be \"--\", got \"%s\"", buf);

    /* Truncation must be safe. */
    char small[3];
    scope_timebase_label(0x10, small, sizeof(small));
    CHECK(small[2] == '\0', "label must NUL-terminate within a 3-byte buffer");
}

int main(void)
{
    test_measured_codes();
    test_code_08_stays_withdrawn();
    test_unmeasured_codes_return_zero();
    test_out_of_range();
    test_derived_quantities();
    test_labels();

    if (failures == 0) {
        printf("test_scope_timebase: all checks passed\n");
        return 0;
    }
    printf("test_scope_timebase: %d check(s) FAILED\n", failures);
    return 1;
}
