/*
 * Host tests for the scope horizontal (time) calibration.
 *
 * As with test_scope_cal.c, these do not second-guess bench numbers. They test
 * the PROPERTIES that keep this table from failing the way rate figures have
 * already failed on this project:
 *
 *   1. The table holds the EXP-74 ladder (2026-11-09): every code fold-tested
 *      against a crystal-locked source. A slip back to any superseded figure
 *      fails here.
 *   2. Code 0x08 is 5 MS/s. The three kS/s figures once published for it were
 *      read-path artifacts; re-entering any of them fails the build.
 *   3. Out-of-range codes refuse rather than wrap.
 *   4. Derived quantities really are derived: s/div from the rate and the
 *      renderer's samples-per-division, Hz from the rate and a period.
 *   5. Labels come from the measurement, keep significant digits down to the
 *      256 ns/div of 0x04, and are NOT the nominal timebase_table strings.
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

/* EXP-74: fold fit equal to these within 1e-5 relative on 0x04-0x13; 0x00-0x03
 * measured 125 MS/s on CH1 (stock's 250 MS/s interleave is not reproduced). */
static const struct { uint8_t code; float fs; } exp74[] = {
    { 0x00, 125e6f }, { 0x01, 125e6f }, { 0x02, 125e6f }, { 0x03, 125e6f },
    { 0x04, 125e6f }, { 0x05, 50e6f },  { 0x06, 25e6f },  { 0x07, 12.5e6f },
    { 0x08, 5e6f },   { 0x09, 2.5e6f }, { 0x0A, 1.25e6f },{ 0x0B, 500e3f },
    { 0x0C, 250e3f }, { 0x0D, 125e3f }, { 0x0E, 50e3f },  { 0x0F, 25e3f },
    { 0x10, 12.5e3f },{ 0x11, 5e3f },   { 0x12, 2.5e3f }, { 0x13, 1.25e3f }, { 0x14, 500.0f },
};

static void test_measured_codes(void)
{
    for (unsigned i = 0; i < sizeof(exp74) / sizeof(exp74[0]); i++) {
        const float got = scope_timebase_sample_rate(exp74[i].code);
        CHECK(fabsf(got / exp74[i].fs - 1.0f) < 1e-5f,
              "code 0x%02X: expected %.1f S/s (EXP-74), got %.1f",
              exp74[i].code, (double)exp74[i].fs, (double)got);
        CHECK(scope_timebase_get_tier(exp74[i].code) == SCOPE_TB_MEASURED,
              "code 0x%02X should be MEASURED", exp74[i].code);
    }

    /* 0x14 was fold-tested last, in its own EXP-74 run. */
    CHECK(fabsf(scope_timebase_sample_rate(0x14) - 500.0f) < 1.0f,
          "code 0x14 should be ~500 S/s, got %.1f",
          (double)scope_timebase_sample_rate(0x14));

    /*
     * The ladder is 1-2.5-5 (x2, x2, x2.5), NOT uniform x2. Encode the real
     * shape over the whole fast range so a transcription slip or a "uniform
     * doubling" correction fails here.
     */
    for (uint8_t c = 0x04; c < 0x14; c++) /* 0x04..0x13 vs next */ {
        const float r = scope_timebase_sample_rate(c) /
                        scope_timebase_sample_rate((uint8_t)(c + 1));
        CHECK(fabsf(r - 2.0f) < 0.01f || fabsf(r - 2.5f) < 0.01f,
              "0x%02X/0x%02X steps by %.3fx, not 2 or 2.5", c, c + 1, (double)r);
    }
}

static void test_code_08_is_5msps(void)
{
    /*
     * 0x08 once carried three kS/s figures (1.07 kS/s, 1,660 S/s, a two-pass
     * 1,414/1,526), each fitted to records that `spi3 opread` had lapped ~170
     * times. Stlkv's 5.00 MS/s (issue #18) and EXP-74's fold test agree.
     */
    CHECK(fabsf(scope_timebase_sample_rate(0x08) - 5e6f) < 1.0f,
          "code 0x08 must be 5 MS/s, got %.1f",
          (double)scope_timebase_sample_rate(0x08));
    const float withdrawn[] = { 1070.0f, 1660.0f, 1414.0f, 1526.0f };
    for (unsigned i = 0; i < 4; i++)
        CHECK(fabsf(scope_timebase_sample_rate(0x08) - withdrawn[i]) > 100.0f,
              "code 0x08 is back at a withdrawn artifact rate %.0f",
              (double)withdrawn[i]);
}

static void test_superseded_rates_stay_out(void)
{
    /*
     * EXP-14: the first published ladder was fitted against frequencies
     * COMMANDED from a source that delivered 0.8250x them, so every rate was
     * 1.21x too high. EXP-17 then moved three rows by up to 2.9 %, and EXP-18
     * left 0x0D at -1.07 %. None of these may come back on a merge.
     */
    const struct { uint8_t code; float bad; } gone[] = {
        { 0x0D, 150706.0f }, { 0x0E, 62958.0f }, { 0x0F, 30235.0f },
        { 0x10, 14853.6f },  { 0x0E, 49056.0f }, { 0x0F, 25736.0f },
        { 0x10, 12575.0f },  { 0x0D, 123662.7f }, { 0x0D, 119678.0f },
        { 0x0E, 49930.1f },  { 0x0F, 24979.1f }, { 0x10, 12490.0f },
        { 0x11, 4990.8f },   { 0x12, 2494.9f },
    };
    for (unsigned i = 0; i < sizeof(gone) / sizeof(gone[0]); i++) {
        CHECK(fabsf(scope_timebase_sample_rate(gone[i].code) - gone[i].bad) > 1.0f,
              "code 0x%02X is back at superseded rate %.1f", gone[i].code,
              (double)gone[i].bad);
    }
}

static void test_every_code_has_a_rate(void)
{
    for (uint8_t c = 0; c < SCOPE_TIMEBASE_CODE_COUNT; c++) {
        CHECK(scope_timebase_sample_rate(c) > 0.0f,
              "code 0x%02X has no rate, but every code was measured", c);
        CHECK(scope_timebase_get_tier(c) == SCOPE_TB_MEASURED,
              "code 0x%02X should be MEASURED", c);
    }
}

static void test_out_of_range(void)
{
    CHECK(scope_timebase_sample_rate(SCOPE_TIMEBASE_CODE_COUNT) == 0.0f,
          "a code past the end must not resolve");
    CHECK(scope_timebase_sample_rate(255) == 0.0f, "code 255 must not resolve");
    CHECK(scope_timebase_get_tier(200) == SCOPE_TB_NONE,
          "an out-of-range tier must be NONE");

    char buf[16];
    scope_timebase_label(SCOPE_TIMEBASE_CODE_COUNT, buf, sizeof(buf));
    CHECK(strcmp(buf, "--") == 0, "out-of-range label should be \"--\", got \"%s\"", buf);
}

static void test_derived_quantities(void)
{
    for (uint8_t c = 0; c < SCOPE_TIMEBASE_CODE_COUNT; c++) {
        const float fs = scope_timebase_sample_rate(c);
        if (fs <= 0.0f) continue;

        CHECK(fabsf(scope_timebase_seconds_per_sample(c) * fs - 1.0f) < 1e-6f,
              "code 0x%02X: s/sample must be 1/fs", c);
        CHECK(fabsf(scope_timebase_seconds_per_div(c) * fs /
                    SCOPE_TIMEBASE_SAMPLES_PER_DIV - 1.0f) < 1e-6f,
              "code 0x%02X: s/div must be samples-per-div / fs", c);
        CHECK(fabsf(scope_timebase_hz_from_period(c, 100.0f) / (fs / 100.0f) - 1.0f) < 1e-6f,
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
    const struct { uint8_t code; const char *want; } lbl[] = {
        { 0x04, "256ns" },   /* 32 / 125 MS/s */
        { 0x05, "640ns" },
        { 0x06, "1.28us" },
        { 0x07, "2.56us" },
        { 0x08, "6.40us" },
        { 0x09, "12.8us" },
        { 0x0A, "25.6us" },
        { 0x0B, "64.0us" },
        { 0x0C, "128us" },
        { 0x0E, "640us" },
        { 0x0F, "1.28ms" },
        { 0x10, "2.56ms" },
    };
    char buf[16];
    for (unsigned i = 0; i < sizeof(lbl) / sizeof(lbl[0]); i++) {
        scope_timebase_label(lbl[i].code, buf, sizeof(buf));
        CHECK(strcmp(buf, lbl[i].want) == 0,
              "0x%02X label should be \"%s\", got \"%s\"", lbl[i].code,
              lbl[i].want, buf);
    }

    /* Not the nominal table's "1ms": the nominal labels are a constant 2.5x
     * away (stock draws a different number of samples per division). */
    scope_timebase_label(0x10, buf, sizeof(buf));
    CHECK(strcmp(buf, "1ms") != 0, "0x10 must not read as the nominal 1ms");

    /* Nothing is provisional any more, so nothing carries the marker. */
    scope_timebase_label(0x0D, buf, sizeof(buf));
    CHECK(buf[0] != '~', "0x0D is MEASURED and must not be marked, got \"%s\"", buf);

    /* Truncation must be safe. */
    char small[3];
    scope_timebase_label(0x10, small, sizeof(small));
    CHECK(small[2] == '\0', "label must NUL-terminate within a 3-byte buffer");
}

int main(void)
{
    test_measured_codes();
    test_code_08_is_5msps();
    test_superseded_rates_stay_out();
    test_every_code_has_a_rate();
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
