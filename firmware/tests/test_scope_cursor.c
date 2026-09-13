/*
 * Host tests for the cursor delta readouts.
 *
 * The cursor is the one feature whose entire purpose is measurement, and
 * until 2026-09-12 its units came from two floats that nothing ever updated:
 * 10 ms across the screen and 8 V down it, seeded once at init. Every dt and
 * dV the instrument printed was a function of the cursor positions alone.
 *
 * So these tests are not about arithmetic. They are about the two properties
 * that stop that happening again:
 *
 *   1. THE ANSWER MUST MOVE WITH THE MEASURED TABLES. Change the timebase
 *      code and dt must change by exactly the ratio scope_timebase.c says;
 *      change the range and dV must change by exactly the ratio scope_cal.c
 *      says. Any implementation carrying its own constant fails this, because
 *      a constant cannot depend on a code or a range.
 *   2. WHERE THERE IS NO CALIBRATION, THERE MUST BE NO UNIT. An unmeasured
 *      timebase code gives samples, an uncalibrated range gives ADC counts,
 *      an unknown vertical transform gives pixels, and 1/dt with no rate
 *      gives "--". Never a plausible number.
 *
 * NEGATIVE CONTROLS are at the bottom and are the point of the file. This
 * project has repeatedly shipped tests that passed on broken code, so each
 * control transcribes the WRONG implementation — the historical constants —
 * and asserts the suite would reject it. A test that only confirms the
 * expected answer has no evidence value here.
 */

#include "../src/ui/scope_cursor.h"
#include "../src/ui/scope_cal.h"
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

/* The codes and ranges the tables call MEASURED, stated here independently of
 * the tables so that retiering a row without revisiting these tests shows up
 * as a failure rather than as a silently weaker test. */
static const uint8_t measured_codes[]  = { 0x0Eu, 0x0Fu, 0x10u };
static const uint8_t measured_ranges[] = { 5u, 6u, 7u };
static const uint8_t provisional_ranges[] = { 4u, 8u, 9u };

/* A believable vertical transform: the fixed ("true volts/div") render path
 * maps 256 counts across the 206-px plot. Autofit produces a different number
 * every frame, which is exactly why the renderer reports it rather than this
 * module assuming it. */
#define FIXED_COUNTS_PER_PIXEL   (256.0f / 206.0f)

static int close_rel(float a, float b, float tol)
{
    const float m = fabsf(b) > 1e-12f ? fabsf(b) : 1.0f;
    return fabsf(a - b) / m <= tol;
}

static scope_cursor_vmap_t vmap(uint8_t ch, uint8_t range, float cpp)
{
    scope_cursor_vmap_t m;
    m.counts_per_pixel = cpp;
    m.channel = ch;
    m.range_idx = range;
    return m;
}

/* ── 1. calibrated axes derive from the tables ───────────────────────── */

static void test_time_comes_from_the_rate_table(void)
{
    for (unsigned i = 0; i < sizeof(measured_codes); i++) {
        const uint8_t code = measured_codes[i];
        const float fs = scope_timebase_sample_rate(code);

        CHECK(fs > 0.0f, "code 0x%02X was supposed to be MEASURED", code);
        CHECK(scope_timebase_get_tier(code) == SCOPE_TB_MEASURED,
              "code 0x%02X tier changed — update this test with the evidence",
              code);

        const scope_cursor_reading_t r = scope_cursor_delta_t(code, 100);
        CHECK(r.unit == SCOPE_CURSOR_UNIT_SECONDS,
              "code 0x%02X: a measured rate must yield seconds", code);
        CHECK(r.confidence == SCOPE_CURSOR_MEASURED,
              "code 0x%02X: confidence must follow the table tier", code);
        CHECK(close_rel(r.value, 100.0f / fs, 1e-5f),
              "code 0x%02X: dt %g != 100 samples / %g S/s",
              code, (double)r.value, (double)fs);
    }
}

static void test_volts_come_from_the_gain_table(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (unsigned i = 0; i < sizeof(measured_ranges); i++) {
            const uint8_t rg = measured_ranges[i];
            const float k = scope_cal_volts_per_count(ch, rg);
            const scope_cursor_vmap_t m = vmap(ch, rg, FIXED_COUNTS_PER_PIXEL);

            const scope_cursor_reading_t r = scope_cursor_delta_v(&m, 40);
            CHECK(r.unit == SCOPE_CURSOR_UNIT_VOLTS,
                  "ch%u r%u: a measured gain must yield volts", ch, rg);
            CHECK(r.confidence == SCOPE_CURSOR_MEASURED,
                  "ch%u r%u: confidence must follow the table tier", ch, rg);
            CHECK(close_rel(r.value, 40.0f * FIXED_COUNTS_PER_PIXEL * k, 1e-5f),
                  "ch%u r%u: dV %g != 40 px * %g cnt/px * %g V/cnt",
                  ch, rg, (double)r.value,
                  (double)FIXED_COUNTS_PER_PIXEL, (double)k);
        }
    }
}

static void test_the_vertical_transform_is_the_renderers(void)
{
    /*
     * The same cursor separation over the same range must give a different
     * voltage when the renderer used a different scale — autofit rescales
     * every frame. An implementation that assumes 256/SCOPE_H passes every
     * other test in this file and fails this one.
     */
    const scope_cursor_vmap_t fixed  = vmap(1u, 6u, FIXED_COUNTS_PER_PIXEL);
    const scope_cursor_vmap_t zoomed = vmap(1u, 6u, FIXED_COUNTS_PER_PIXEL / 4.0f);

    const float a = scope_cursor_delta_v(&fixed, 30).value;
    const float b = scope_cursor_delta_v(&zoomed, 30).value;

    CHECK(close_rel(a, b * 4.0f, 1e-5f),
          "dV must scale with the renderer's counts/pixel: %g vs %g",
          (double)a, (double)b);
}

static void test_one_over_dt(void)
{
    const uint8_t code = 0x10u;
    const float fs = scope_timebase_sample_rate(code);

    scope_cursor_reading_t r = scope_cursor_one_over_dt(code, 100);
    CHECK(r.unit == SCOPE_CURSOR_UNIT_HERTZ, "measured code must give Hz");
    CHECK(close_rel(r.value, fs / 100.0f, 1e-5f),
          "1/dt %g != %g / 100", (double)r.value, (double)fs);

    /* Sign must not survive into a frequency. */
    r = scope_cursor_one_over_dt(code, -100);
    CHECK(r.value > 0.0f, "1/dt must be positive for a negative dx");

    /* No interval => no frequency, and that is arithmetic, not calibration. */
    r = scope_cursor_one_over_dt(code, 0);
    CHECK(r.unit == SCOPE_CURSOR_UNIT_NONE, "dx=0 must refuse");

    /* No rate => no Hz either; 1/samples has no unit to fall back to. */
    r = scope_cursor_one_over_dt(0x08u, 100);
    CHECK(r.unit == SCOPE_CURSOR_UNIT_NONE,
          "an INCOHERENT code must not produce a frequency");
}

/* ── 2. refusal where there is no calibration ────────────────────────── */

static void test_unmeasured_timebase_falls_back_to_samples(void)
{
    /* 0x08 is the power-on default and is INCOHERENT — measured, and the
     * records do not reproduce. It is the code most likely to be on screen. */
    static const uint8_t none_codes[] = { 0x00u, 0x06u, 0x08u, 0x0Au, 0x0Cu };

    for (unsigned i = 0; i < sizeof(none_codes); i++) {
        const uint8_t code = none_codes[i];
        CHECK(scope_timebase_sample_rate(code) == 0.0f,
              "code 0x%02X was supposed to have no rate", code);

        const scope_cursor_reading_t r = scope_cursor_delta_t(code, 137);
        CHECK(r.unit == SCOPE_CURSOR_UNIT_SAMPLES,
              "code 0x%02X must fall back to samples, not seconds", code);
        CHECK(r.confidence == SCOPE_CURSOR_RAW, "samples are raw, not tiered");
        CHECK(r.value == 137.0f,
              "code 0x%02X: the sample count must be exact, got %g",
              code, (double)r.value);

        char s[16];
        scope_cursor_format(&r, s, sizeof(s));
        CHECK(strcmp(s, "137smp") == 0, "expected 137smp, got %s", s);
    }
}

static void test_uncalibrated_range_falls_back_to_counts(void)
{
    for (uint8_t ch = 1; ch <= 2; ch++) {
        for (uint8_t rg = 0; rg <= 3; rg++) {
            CHECK(scope_cal_volts_per_count(ch, rg) == 0.0f,
                  "ch%u r%u was supposed to be uncalibrated", ch, rg);

            const scope_cursor_vmap_t m = vmap(ch, rg, 1.0f);
            const scope_cursor_reading_t r = scope_cursor_delta_v(&m, 50);

            CHECK(r.unit == SCOPE_CURSOR_UNIT_COUNTS,
                  "ch%u r%u must fall back to counts, not volts", ch, rg);
            CHECK(r.confidence == SCOPE_CURSOR_RAW, "counts are raw");
            CHECK(r.value == 50.0f, "ch%u r%u: counts must be exact", ch, rg);

            char s[16];
            scope_cursor_format(&r, s, sizeof(s));
            CHECK(strcmp(s, "50cnt") == 0, "expected 50cnt, got %s", s);
        }
    }
}

static void test_unknown_transform_falls_back_to_pixels(void)
{
    /* No trace drawn, the demo waveform, or the cursors straddling two
     * differently-scaled bands. Pixels are the only exact statement. */
    const scope_cursor_vmap_t none = vmap(1u, 6u, 0.0f);
    scope_cursor_reading_t r = scope_cursor_delta_v(&none, 20);
    CHECK(r.unit == SCOPE_CURSOR_UNIT_PIXELS,
          "no renderer transform must give pixels, not counts or volts");
    CHECK(r.value == 20.0f, "pixel delta must be exact");

    r = scope_cursor_delta_v(NULL, 20);
    CHECK(r.unit == SCOPE_CURSOR_UNIT_PIXELS, "NULL map must not crash or convert");

    char s[16];
    scope_cursor_format(&r, s, sizeof(s));
    CHECK(strcmp(s, "20px") == 0, "expected 20px, got %s", s);
}

static void test_out_of_domain_channel_does_not_resolve(void)
{
    /* scope_cal returns 0.0f for channel 0 and 3 rather than clamping into a
     * neighbour; the cursor must inherit that refusal, not paper over it. */
    for (uint8_t ch = 0; ch <= 4; ch += 3) {
        const scope_cursor_vmap_t m = vmap(ch, 6u, 1.0f);
        const scope_cursor_reading_t r = scope_cursor_delta_v(&m, 10);
        CHECK(r.unit == SCOPE_CURSOR_UNIT_COUNTS,
              "channel %u must not resolve to a calibrated channel", ch);
    }
}

/* ── 3. provisional entries are marked ───────────────────────────────── */

static void test_provisional_ranges_are_marked(void)
{
    char s[16];

    for (unsigned i = 0; i < sizeof(provisional_ranges); i++) {
        const uint8_t rg = provisional_ranges[i];
        CHECK(scope_cal_get_tier(1u, rg) == SCOPE_CAL_PROVISIONAL,
              "r%u tier changed — update this test with the evidence", rg);

        const scope_cursor_vmap_t m = vmap(1u, rg, FIXED_COUNTS_PER_PIXEL);
        const scope_cursor_reading_t r = scope_cursor_delta_v(&m, 20);

        CHECK(r.confidence == SCOPE_CURSOR_PROVISIONAL,
              "r%u must be reported as provisional", rg);
        scope_cursor_format(&r, s, sizeof(s));
        CHECK(s[0] == '~', "r%u must carry the tilde, got %s", rg, s);
    }

    /* And a MEASURED one must NOT, or the marker says nothing. */
    const scope_cursor_vmap_t m6 = vmap(1u, 6u, FIXED_COUNTS_PER_PIXEL);
    const scope_cursor_reading_t r6 = scope_cursor_delta_v(&m6, 20);
    scope_cursor_format(&r6, s, sizeof(s));
    CHECK(s[0] != '~', "a measured range must not be marked provisional: %s", s);
}

static void test_provisional_timebase_is_marked(void)
{
    /* 0x0D: R^2 0.9997 but only bins 4-25 of resolution, and never
     * fold-tested at the corrected rate. */
    CHECK(scope_timebase_get_tier(0x0Du) == SCOPE_TB_PROVISIONAL,
          "0x0D tier changed — update this test with the evidence");

    const scope_cursor_reading_t r = scope_cursor_delta_t(0x0Du, 100);
    CHECK(r.confidence == SCOPE_CURSOR_PROVISIONAL, "0x0D must be provisional");

    char s[16];
    scope_cursor_format(&r, s, sizeof(s));
    CHECK(s[0] == '~', "0x0D must carry the tilde, got %s", s);
}

/* ── 4. formatting ──────────────────────────────────────────────────── */

static void test_formatting(void)
{
    char s[16];

    /* NONE is the "--" every other part of the scope UI uses. */
    scope_cursor_reading_t r = scope_cursor_one_over_dt(0x10u, 0);
    scope_cursor_format(&r, s, sizeof(s));
    CHECK(strcmp(s, "--") == 0, "a refusal must render as --, got %s", s);

    /* Sign survives into the readout: a cursor dragged backwards reads
     * negative rather than silently taking an absolute value. */
    r = scope_cursor_delta_t(0x10u, -100);
    CHECK(r.value < 0.0f, "negative dx must give negative dt");
    scope_cursor_format(&r, s, sizeof(s));
    CHECK(s[0] == '-', "negative dt must print a sign, got %s", s);

    /* 100 samples at 12,490 S/s = 8.0064 ms. */
    r = scope_cursor_delta_t(0x10u, 100);
    scope_cursor_format(&r, s, sizeof(s));
    CHECK(strcmp(s, "8.01ms") == 0, "expected 8.01ms, got %s", s);

    /* A tiny buffer must truncate, not overflow. */
    char tiny[4];
    scope_cursor_format(&r, tiny, sizeof(tiny));
    CHECK(tiny[3] == '\0', "format must NUL-terminate within n");
}

/* ── 5. NEGATIVE CONTROLS ───────────────────────────────────────────── */

/*
 * The implementation this work replaced, transcribed exactly.
 * scope_state_init() seeded cursor.time_per_pixel with 10 ms across a 320-px
 * screen and cursor.volts_per_pixel with 8 V down a 206-px plot, and nothing
 * ever changed either.
 */
#define OLD_TIME_PER_PIXEL    (10.0e-3f / 320.0f)
#define OLD_VOLTS_PER_PIXEL   (8.0f / 206.0f)

static void negctl_old_constants_would_be_caught(void)
{
    /*
     * If someone "simplifies" this module back to a fixed scale, the
     * positive tests above must reject it. Assert that here explicitly, so
     * the suite's sensitivity is itself tested rather than assumed.
     */
    for (unsigned i = 0; i < sizeof(measured_codes); i++) {
        const uint8_t code = measured_codes[i];
        const float real = scope_cursor_delta_t(code, 100).value;
        const float old  = 100.0f * OLD_TIME_PER_PIXEL;

        CHECK(!close_rel(old, real, 0.05f),
              "code 0x%02X: the old fixed time/pixel (%g s) is within 5%% of "
              "the measured answer (%g s) — this control can no longer detect "
              "a regression", code, (double)old, (double)real);
    }

    for (unsigned i = 0; i < sizeof(measured_ranges); i++) {
        const uint8_t rg = measured_ranges[i];
        const scope_cursor_vmap_t m = vmap(1u, rg, FIXED_COUNTS_PER_PIXEL);
        const float real = scope_cursor_delta_v(&m, 40).value;
        const float old  = 40.0f * OLD_VOLTS_PER_PIXEL;

        CHECK(!close_rel(old, real, 0.05f),
              "r%u: the old fixed volts/pixel (%g V) is within 5%% of the "
              "measured answer (%g V) — this control can no longer detect a "
              "regression", rg, (double)old, (double)real);
    }
}

static void negctl_answer_must_depend_on_the_code(void)
{
    /*
     * The structural control, and the one that cannot be satisfied by any
     * hardcoded constant whatsoever: the same pixel delta must produce
     * DIFFERENT answers on different timebase codes, in exactly the ratio the
     * rate table gives. A constant produces the same answer for all three.
     */
    const float a = scope_cursor_delta_t(0x0Eu, 100).value;
    const float b = scope_cursor_delta_t(0x0Fu, 100).value;
    const float c = scope_cursor_delta_t(0x10u, 100).value;

    CHECK(!close_rel(a, b, 0.01f) && !close_rel(b, c, 0.01f),
          "dt did not change with the timebase code (%g / %g / %g) — the "
          "readout is not reading the rate table", (double)a, (double)b, (double)c);

    const float fs_e = scope_timebase_sample_rate(0x0Eu);
    const float fs_f = scope_timebase_sample_rate(0x0Fu);
    CHECK(close_rel(a / b, fs_f / fs_e, 1e-4f),
          "dt ratio %g does not match the table's rate ratio %g",
          (double)(a / b), (double)(fs_f / fs_e));
}

static void negctl_answer_must_depend_on_the_range(void)
{
    /* Same structural control on the vertical axis. Ranges 5/6/7 are a clean
     * doubling ladder, so a range-independent implementation is obvious. */
    const scope_cursor_vmap_t m5 = vmap(1u, 5u, FIXED_COUNTS_PER_PIXEL);
    const scope_cursor_vmap_t m6 = vmap(1u, 6u, FIXED_COUNTS_PER_PIXEL);
    const scope_cursor_vmap_t m7 = vmap(1u, 7u, FIXED_COUNTS_PER_PIXEL);

    const float a = scope_cursor_delta_v(&m5, 40).value;
    const float b = scope_cursor_delta_v(&m6, 40).value;
    const float c = scope_cursor_delta_v(&m7, 40).value;

    CHECK(!close_rel(a, b, 0.01f) && !close_rel(b, c, 0.01f),
          "dV did not change with the range (%g / %g / %g) — the readout is "
          "not reading the gain table", (double)a, (double)b, (double)c);

    const float k5 = scope_cal_volts_per_count(1u, 5u);
    const float k6 = scope_cal_volts_per_count(1u, 6u);
    CHECK(close_rel(a / b, k5 / k6, 1e-4f),
          "dV ratio %g does not match the table's gain ratio %g",
          (double)(a / b), (double)(k5 / k6));
}

static void negctl_source_scale_reaches_the_cursor(void)
{
    /*
     * The vertical table is recoverable with one constant only if every
     * consumer goes through scope_cal's lookup. If the cursor ever grew its
     * own copy of a raw mV/count number, correcting SCOPE_CAL_SOURCE_SCALE
     * would fix the badges and leave the cursor wrong — a divergence nobody
     * would see. The raw bench figure is stated here independently of the
     * table for the same reason test_scope_cal.c states them.
     */
    const float raw_ch1_r6_mv_per_count = 42.95f;   /* EXP-08, bench unit #1 */
    const scope_cursor_vmap_t m = vmap(1u, 6u, 1.0f);
    const float got = scope_cursor_delta_v(&m, 1).value;

    CHECK(close_rel(got,
                    raw_ch1_r6_mv_per_count * SCOPE_CAL_SOURCE_SCALE / 1000.0f,
                    1e-4f),
          "cursor dV (%g V/count) does not equal the raw table entry times "
          "SCOPE_CAL_SOURCE_SCALE — the cursor is not going through "
          "scope_cal_volts_per_count()", (double)got);
}

int main(void)
{
    test_time_comes_from_the_rate_table();
    test_volts_come_from_the_gain_table();
    test_the_vertical_transform_is_the_renderers();
    test_one_over_dt();

    test_unmeasured_timebase_falls_back_to_samples();
    test_uncalibrated_range_falls_back_to_counts();
    test_unknown_transform_falls_back_to_pixels();
    test_out_of_domain_channel_does_not_resolve();

    test_provisional_ranges_are_marked();
    test_provisional_timebase_is_marked();
    test_formatting();

    negctl_old_constants_would_be_caught();
    negctl_answer_must_depend_on_the_code();
    negctl_answer_must_depend_on_the_range();
    negctl_source_scale_reaches_the_cursor();

    if (failures == 0) {
        printf("test_scope_cursor: PASS\n");
        return 0;
    }
    printf("test_scope_cursor: %d FAILURES\n", failures);
    return 1;
}
