/*
 * Host tests for the scope vertical calibration table.
 *
 * These are not numeric-accuracy tests — the numbers came off a bench and no
 * unit test can second-guess them. What is tested is the set of PROPERTIES
 * that stop this table failing the way its predecessor failed:
 *
 *   1. "No calibration" returns exactly 0.0f, so a caller that forgets to
 *      check gets an obviously-zero volt reading rather than a plausible
 *      small one. Since 2026-11-27 every range has a gain, so this is
 *      exercised through out-of-domain arguments.
 *   2. Out-of-domain channel/range arguments return "no calibration" instead
 *      of clamping into a neighbour.
 *   3. Every row is the rule in docs/calibration/scope_gain.md applied to
 *      the per-unit numbers stated here, times the one source scale. A row
 *      edited on its own to "fix" a reading fails.
 *   4. The claims the tiers rest on hold: the units agree, the channels
 *      agree, and the ranges form the 1-2-5 ladder the header states.
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

/*
 * The per-unit numbers (mV per count at the BNC), stated independently of
 * the table: docs/calibration/scope_gain.md. 0 = not measured, or not at
 * usable resolution (s00500's ranges 8-9; see the doc).
 */
static const float unit_s00500[2][SCOPE_CAL_RANGE_COUNT] = {
    { 0.4028f, 0.7981f, 1.9872f, 3.927f, 7.866f, 20.01f, 39.79f, 78.90f, 0.0f, 0.0f },
    { 0.3978f, 0.7928f, 1.9732f, 3.906f, 7.783f, 19.86f, 39.39f, 77.11f, 0.0f, 0.0f },
};
static const float unit_sparsematrix[2][SCOPE_CAL_RANGE_COUNT] = {   /* #95 */
    { 0.0f, 0.0f, 0.0f, 0.0f, 7.77f, 19.83f, 39.46f, 78.19f, 193.5f, 384.2f },
    { 0.0f, 0.0f, 0.0f, 0.0f, 7.93f, 19.98f, 39.95f, 79.18f, 196.0f, 391.1f },
};

/* The rule: the mean of the units that measured an entry. */
static float rule(uint8_t row, uint8_t r)
{
    const float a = unit_s00500[row][r], b = unit_sparsematrix[row][r];
    if (a > 0.0f && b > 0.0f) return (a + b) * 0.5f;
    return (a > 0.0f) ? a : b;
}

static void test_every_range_has_a_gain(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
            CHECK(scope_cal_mv_per_count(ch, r) > 0.0f,
                  "ch%u r%u: every range has a measured gain since 2026-11-27", ch, r);
            CHECK(scope_cal_get_tier(ch, r) != SCOPE_CAL_NONE,
                  "ch%u r%u: a range with a gain is not tier NONE", ch, r);
        }
    }
    /* "No calibration" still reads exactly 0.0f where it applies. */
    CHECK(scope_cal_volts_per_div(1, SCOPE_CAL_RANGE_COUNT) == 0.0f,
          "a range past the end must have no volts/div");
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

static void test_rows_follow_the_rule(void)
{
    /*
     * Every entry must be the rule applied to the per-unit numbers above,
     * times exactly one shared factor. A row hand-edited to match one
     * reference fails here, which is the intent: the way to add a reference
     * is to add its unit to the doc and to the arrays above, and re-derive.
     * The table is rounded to the units' precision, hence the tolerance.
     */
    CHECK(SCOPE_CAL_SOURCE_SCALE == 1.0f,
          "both units are absolutely referenced: the source scale must be 1.0");

    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
            const float want = rule((uint8_t)(ch - 1u), r) * SCOPE_CAL_SOURCE_SCALE;
            const float got  = scope_cal_mv_per_count(ch, r);
            CHECK(want > 0.0f, "ch%u r%u: no unit measured this entry", ch, r);
            CHECK(fabsf(got - want) <= 0.006f + want * 1e-4f,
                  "ch%u r%u: table %f, the rule gives %f", ch, r,
                  (double)got, (double)want);
        }
    }
}

static void test_the_units_agree(void)
{
    /* Ranges 4-7 are MEASURED because the two units agree; the doc says
     * within 2.6 %, the tier needs within 3 %. */
    for (uint8_t row = 0; row < 2; row++) {
        for (uint8_t r = 4; r <= 7; r++) {
            const float a = unit_s00500[row][r], b = unit_sparsematrix[row][r];
            const float d = fabsf(a - b) / ((a + b) * 0.5f);
            CHECK(d < 0.03f, "CH%u r%u: the units disagree by %.1f%%",
                  row + 1u, r, (double)(d * 100.0f));
        }
    }
}

static void test_measured_tier_claims_hold(void)
{
    /*
     * The header claims ranges 0-7 are MEASURED, that the channels agree
     * within 2 % on every range, and that the ranges form the 1-2-5 ladder
     * of the attenuator. Those are asserted rather than asserted-in-a-comment.
     */
    for (uint8_t r = 0; r <= 7; r++) {
        CHECK(scope_cal_get_tier(1, r) == SCOPE_CAL_MEASURED,
              "r%u should be MEASURED on ch1", r);
        CHECK(scope_cal_get_tier(2, r) == SCOPE_CAL_MEASURED,
              "r%u should be MEASURED on ch2", r);
    }

    for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
        const float a = scope_cal_mv_per_count(1, r);
        const float b = scope_cal_mv_per_count(2, r);
        const float disagreement = fabsf(a - b) / ((a + b) * 0.5f);

        CHECK(disagreement < 0.02f,
              "r%u: channels disagree by %.1f%%", r, (double)(disagreement * 100.0f));
    }

    /* 1-2-5 ladder from range 0: x2, x2.5, x2, x2, x2.5, x2, x2, x2.5, x2. */
    static const float step[SCOPE_CAL_RANGE_COUNT - 1u] = {
        2.0f, 2.5f, 2.0f, 2.0f, 2.5f, 2.0f, 2.0f, 2.5f, 2.0f };
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r + 1u < SCOPE_CAL_RANGE_COUNT; r++) {
            const float ratio = scope_cal_mv_per_count(ch, (uint8_t)(r + 1u)) /
                                scope_cal_mv_per_count(ch, r);
            CHECK(fabsf(ratio / step[r] - 1.0f) < 0.03f,
                  "ch%u r%u->r%u: ratio %.3f is not the ladder's x%.1f",
                  ch, r, r + 1, (double)ratio, (double)step[r]);
        }
    }
}

static void test_provisional_ranges_still_produce_a_number(void)
{
    /* Provisional is not "unusable": a rough volt figure beats a raw count,
     * and the tier plus the label marker is how the user is told which is
     * which. */
    const uint8_t provisional[] = { 8, 9 };

    for (uint8_t i = 0; i < 2; i++) {
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

    /* Uncalibrated -> an explicit nothing, never a nominal number. No range
     * is uncalibrated since 2026-11-27, so this goes through one past the end. */
    scope_cal_range_label(1, SCOPE_CAL_RANGE_COUNT, buf, sizeof(buf));
    CHECK(strcmp(buf, "--") == 0, "no-cal label should be \"--\", got \"%s\"", buf);

    /* Measured -> a plain number, no marker. */
    scope_cal_range_label(1, 6, buf, sizeof(buf));
    CHECK(buf[0] != '~', "r6 is MEASURED and must not be marked, got \"%s\"", buf);
    CHECK(strchr(buf, 'V') != NULL, "r6 label should carry units, got \"%s\"", buf);

    /* 39.63 mV/count * 1.0 source scale * 32 counts/div = 1.268 V/div. */
    CHECK(strcmp(buf, "1.27V") == 0,
          "r6 label should be derived as 1.27V, got \"%s\"", buf);

    /* Provisional -> same number, marked. */
    scope_cal_range_label(1, 9, buf, sizeof(buf));
    CHECK(buf[0] == '~', "r9 is PROVISIONAL and must be marked, got \"%s\"", buf);

    /* Sub-volt divisions read in mV. 19.92 * 32 = 637.4 mV. */
    scope_cal_range_label(1, 5, buf, sizeof(buf));
    CHECK(strcmp(buf, "637mV") == 0,
          "r5 label should be derived as 637mV, got \"%s\"", buf);

    /* The most sensitive range. 0.4028 * 32 = 12.9 mV. */
    scope_cal_range_label(1, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "13mV") == 0,
          "r0 label should be derived as 13mV, got \"%s\"", buf);

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

/* The true-scale render gate (M3) must agree, range for range, with whether a
 * volts/div exists: calibrated -> render true scale; NONE / out-of-domain ->
 * autofit. If these two ever disagree, the grid would claim a volts/div the
 * badges refuse to print (or vice versa) — the exact class of inconsistency
 * scope_cal exists to prevent. */
static void test_true_scale_gate_matches_calibration(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
            int ok   = scope_cal_true_scale_ok(ch, r);
            int cal  = scope_cal_volts_per_count(ch, r) > 0.0f;
            CHECK(!!ok == cal,
                  "ch%u r%u: true_scale_ok(%d) must match has-cal(%d)",
                  ch, r, ok, cal);
        }
        /* Every range has a gain since 2026-11-27 -> true scale everywhere. */
        for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++)
            CHECK(scope_cal_true_scale_ok(ch, r),
                  "ch%u r%u: a calibrated range must render true scale", ch, r);
    }
    /* Out-of-domain is a refusal, same as everywhere else in this module. */
    CHECK(!scope_cal_true_scale_ok(0, 6), "ch0 must not render true scale");
    CHECK(!scope_cal_true_scale_ok(1, SCOPE_CAL_RANGE_COUNT),
          "range past the end must not render true scale");
}

/* Probe factor (2026-10-03): the readout scales, the calibration table does
 * not, and only the channel with the 10x probe moves. Control: with no
 * source registered every number is exactly what it was. */
static float probe_ch2_x10(uint8_t ch) { return ch == 2u ? 10.0f : 1.0f; }
static float probe_bad(uint8_t ch)     { (void)ch; return -3.0f; }

static void test_probe_factor(void)
{
    const uint8_t r = 6;                       /* a MEASURED range */
    scope_cal_set_probe_source(NULL);
    const float v1 = scope_cal_volts_per_count(1, r), v2 = scope_cal_volts_per_count(2, r);
    const float m2 = scope_cal_mv_per_count(2, r), d2 = scope_cal_volts_per_div(2, r);
    char l_before[12], l_after[12];
    scope_cal_range_label(2, r, l_before, sizeof(l_before));
    CHECK(v2 > 0.0f && scope_cal_probe_factor(2) == 1.0f, "no source: factor 1");

    scope_cal_set_probe_source(probe_ch2_x10);
    CHECK(scope_cal_volts_per_count(2, r) == v2 * 10.0f, "CH2 10x: volts/count x10");
    CHECK(scope_cal_volts_per_div(2, r) == d2 * 10.0f, "CH2 10x: volts/div x10");
    CHECK(scope_cal_mv_per_count(2, r) == m2, "CH2 10x: the cal table must not move");
    CHECK(scope_cal_volts_per_count(1, r) == v1, "CH1 1x: unchanged");
    scope_cal_range_label(2, r, l_after, sizeof(l_after));
    CHECK(strcmp(l_before, l_after) != 0, "CH2 label must change (%s -> %s)", l_before, l_after);
    {
        char bnc[12];
        scope_cal_range_label_bnc(2, r, bnc, sizeof(bnc));
        CHECK(strcmp(bnc, l_before) == 0, "BNC label ignores the probe (%s vs %s)", bnc, l_before);
    }
    CHECK(scope_cal_volts_per_count(2, SCOPE_CAL_RANGE_COUNT) == 0.0f,
          "no calibration stays 0 at 10x");
    CHECK(scope_cal_true_scale_ok(2, r), "true-scale gate unaffected by the probe");

    scope_cal_set_probe_source(probe_bad);
    CHECK(scope_cal_volts_per_count(2, r) == v2, "a nonsense factor falls back to 1");
    scope_cal_set_probe_source(NULL);
    CHECK(scope_cal_volts_per_count(2, r) == v2, "unregistering restores 1x");
}

int main(void)
{
    test_every_range_has_a_gain();
    test_out_of_domain_arguments();
    test_true_scale_gate_matches_calibration();
    test_rows_follow_the_rule();
    test_the_units_agree();
    test_measured_tier_claims_hold();
    test_provisional_ranges_still_produce_a_number();
    test_volts_per_div_is_derived();
    test_labels();
    test_probe_factor();

    if (failures == 0) {
        printf("test_scope_cal: all checks passed\n");
        return 0;
    }
    printf("test_scope_cal: %d check(s) FAILED\n", failures);
    return 1;
}
