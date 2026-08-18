/*
 * Host tests for the scope vertical calibration table.
 *
 * These are not numeric-accuracy tests — the numbers came off a bench and no
 * unit test can second-guess them. What is tested is the set of PROPERTIES
 * that stop this table failing the way its predecessor failed:
 *
 *   1. An uncalibrated range returns exactly 0.0f, so a caller that forgets
 *      to check gets an obviously-zero volt reading rather than a plausible
 *      small one.
 *   2. Out-of-domain channel/range arguments return "no calibration" instead
 *      of clamping into a neighbour.
 *   3. The source-scale factor reaches every entry uniformly, which is the
 *      property that makes one calibrated measurement able to rescale the
 *      whole instrument later.
 *   4. The MEASURED ranges really do agree between channels and really do
 *      form the doubling ladder claimed in the header, so if someone edits a
 *      row to "fix" a reading, the claim in the comment stops being true and
 *      the build says so.
 *   5. Labels are derived from the gain rather than hardcoded, and mark
 *      provisional entries.
 */

#include "../src/ui/scope_cal.h"

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

static void test_uncalibrated_ranges_return_zero(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r <= 3; r++) {
            CHECK(scope_cal_mv_per_count(ch, r) == 0.0f,
                  "ch%u r%u: railed range must return exactly 0.0f, got %f",
                  ch, r, (double)scope_cal_mv_per_count(ch, r));
            CHECK(scope_cal_get_tier(ch, r) == SCOPE_CAL_NONE,
                  "ch%u r%u: railed range must be tier NONE", ch, r);
            CHECK(scope_cal_volts_per_div(ch, r) == 0.0f,
                  "ch%u r%u: railed range must have no volts/div", ch, r);
        }
    }
}

static void test_out_of_domain_arguments(void)
{
    /* Channel 0 is the trap: fpga_scope_set_range_diag_ch() uses 0-based
     * channels while the shell argument is 1-based, and that mismatch has
     * already caused one silently-wrong bench result. Here 0 must be a
     * refusal, not "CH1". */
    CHECK(scope_cal_mv_per_count(0, 6) == 0.0f, "ch0 must not resolve to CH1");
    CHECK(scope_cal_mv_per_count(3, 6) == 0.0f, "ch3 must not resolve");
    CHECK(scope_cal_mv_per_count(1, SCOPE_CAL_RANGE_COUNT) == 0.0f,
          "range past the end must not resolve");
    CHECK(scope_cal_mv_per_count(1, 255) == 0.0f, "range 255 must not resolve");
    CHECK(scope_cal_get_tier(0, 6) == SCOPE_CAL_NONE, "ch0 tier must be NONE");
}

static void test_source_scale_is_uniform(void)
{
    /*
     * Every calibrated entry must be the raw bench number times exactly one
     * shared factor. The test states the raw numbers independently of the
     * table, so if someone hand-edits a single row to match a reference
     * instrument, this fails — which is the intent. The recovery path when a
     * trusted source arrives is to change SCOPE_CAL_SOURCE_SCALE, and that
     * only works if no row has been individually nudged.
     */
    static const float raw_ch1[] = { 14.08f, 21.83f, 42.95f, 88.42f, 279.05f, 352.17f };
    static const float raw_ch2[] = {  9.49f, 20.96f, 41.71f, 83.79f, 223.71f, 425.00f };

    for (uint8_t i = 0; i < 6; i++) {
        const uint8_t r = (uint8_t)(i + 4u);

        const float got1 = scope_cal_mv_per_count(1, r);
        const float got2 = scope_cal_mv_per_count(2, r);
        const float want1 = raw_ch1[i] * SCOPE_CAL_SOURCE_SCALE;
        const float want2 = raw_ch2[i] * SCOPE_CAL_SOURCE_SCALE;

        CHECK(fabsf(got1 - want1) < 0.01f,
              "ch1 r%u: expected %f (raw * source scale), got %f",
              r, (double)want1, (double)got1);
        CHECK(fabsf(got2 - want2) < 0.01f,
              "ch2 r%u: expected %f (raw * source scale), got %f",
              r, (double)want2, (double)got2);
    }
}

static void test_measured_tier_claims_hold(void)
{
    /*
     * The header claims ranges 5/6/7 agree between channels within 6% and
     * form a doubling ladder. Those are the grounds for calling them
     * MEASURED, so they are asserted rather than asserted-in-a-comment.
     */
    for (uint8_t r = 5; r <= 7; r++) {
        CHECK(scope_cal_get_tier(1, r) == SCOPE_CAL_MEASURED,
              "r%u should be MEASURED on ch1", r);
        CHECK(scope_cal_get_tier(2, r) == SCOPE_CAL_MEASURED,
              "r%u should be MEASURED on ch2", r);

        const float a = scope_cal_mv_per_count(1, r);
        const float b = scope_cal_mv_per_count(2, r);
        const float disagreement = fabsf(a - b) / ((a + b) * 0.5f);

        CHECK(disagreement < 0.06f,
              "r%u: channels disagree by %.1f%%, too much for tier MEASURED",
              r, (double)(disagreement * 100.0f));
    }

    /* Doubling ladder: each step up should roughly double the mV/count. */
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 5; r <= 6; r++) {
            const float ratio = scope_cal_mv_per_count(ch, (uint8_t)(r + 1u)) /
                                scope_cal_mv_per_count(ch, r);
            CHECK(ratio > 1.8f && ratio < 2.2f,
                  "ch%u r%u->r%u: ratio %.2f is not the claimed doubling",
                  ch, r, r + 1, (double)ratio);
        }
    }
}

static void test_provisional_ranges_still_produce_a_number(void)
{
    /* Provisional is not "unusable": a rough volt figure beats a raw count,
     * and the tier plus the label marker is how the user is told which is
     * which. */
    const uint8_t provisional[] = { 4, 8, 9 };

    for (uint8_t i = 0; i < 3; i++) {
        const uint8_t r = provisional[i];
        for (uint8_t ch = 1; ch <= 2; ch++) {
            CHECK(scope_cal_mv_per_count(ch, r) > 0.0f,
                  "ch%u r%u: provisional range must still return a gain", ch, r);
            CHECK(scope_cal_get_tier(ch, r) == SCOPE_CAL_PROVISIONAL,
                  "ch%u r%u: expected tier PROVISIONAL", ch, r);
        }
    }
}

static void test_volts_per_div_is_derived(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
            const float want = scope_cal_volts_per_count(ch, r) *
                               SCOPE_CAL_COUNTS_PER_DIV;
            CHECK(fabsf(scope_cal_volts_per_div(ch, r) - want) < 1e-6f,
                  "ch%u r%u: volts/div must be gain * counts-per-div", ch, r);
        }
    }
}

static void test_labels(void)
{
    char buf[16];

    /* Uncalibrated -> an explicit nothing, never a nominal number. */
    scope_cal_range_label(1, 2, buf, sizeof(buf));
    CHECK(strcmp(buf, "--") == 0, "r2 label should be \"--\", got \"%s\"", buf);

    /* Measured -> a plain number, no marker. */
    scope_cal_range_label(1, 6, buf, sizeof(buf));
    CHECK(buf[0] != '~', "r6 is MEASURED and must not be marked, got \"%s\"", buf);
    CHECK(strchr(buf, 'V') != NULL, "r6 label should carry units, got \"%s\"", buf);

    /* 42.95 mV/count * 32 counts/div = 1.3744 V/div. */
    CHECK(strcmp(buf, "1.37V") == 0,
          "r6 label should be derived as 1.37V, got \"%s\"", buf);

    /* Provisional -> same number, marked. */
    scope_cal_range_label(1, 9, buf, sizeof(buf));
    CHECK(buf[0] == '~', "r9 is PROVISIONAL and must be marked, got \"%s\"", buf);

    /* Sub-volt divisions read in mV. 21.83 * 32 = 698.6 mV. */
    scope_cal_range_label(1, 5, buf, sizeof(buf));
    CHECK(strcmp(buf, "699mV") == 0,
          "r5 label should be derived as 699mV, got \"%s\"", buf);

    /*
     * And the point of the whole exercise: the derived label must NOT equal
     * the nominal vdiv_table label it replaces. r5's nominal label was
     * "200mV" and r6's was "500mV", both roughly 3x out.
     */
    scope_cal_range_label(1, 5, buf, sizeof(buf));
    CHECK(strcmp(buf, "200mV") != 0, "r5 must not still read as the nominal 200mV");
    scope_cal_range_label(1, 6, buf, sizeof(buf));
    CHECK(strcmp(buf, "500mV") != 0, "r6 must not still read as the nominal 500mV");

    /* A tiny buffer must truncate safely, not overrun. */
    char small[3];
    scope_cal_range_label(1, 9, small, sizeof(small));
    CHECK(small[2] == '\0', "label must NUL-terminate within a 3-byte buffer");
}

int main(void)
{
    test_uncalibrated_ranges_return_zero();
    test_out_of_domain_arguments();
    test_source_scale_is_uniform();
    test_measured_tier_claims_hold();
    test_provisional_ranges_still_produce_a_number();
    test_volts_per_div_is_derived();
    test_labels();

    if (failures == 0) {
        printf("test_scope_cal: all checks passed\n");
        return 0;
    }
    printf("test_scope_cal: %d check(s) FAILED\n", failures);
    return 1;
}
