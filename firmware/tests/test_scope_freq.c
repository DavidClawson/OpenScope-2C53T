/*
 * Host tests for the frequency estimator, driven by REAL BENCH RECORDS.
 *
 * This is the acceptance test EXP-13 asked for. It does not check the
 * estimator against a synthetic sine — a synthetic sine is exactly the case
 * that always worked. It runs the shipped code over 72 captures taken from
 * bench unit #1 on 2026-08-19, across two vertical ranges, three timebase
 * codes, five frequencies and seven amplitudes, with the signal generator
 * corrected (EXP-14) so the recorded drive frequency is DELIVERED rather than
 * merely commanded.
 *
 * The contract being tested is not "always answers". It is:
 *
 *   - when it answers, it is within 5% of the true drive;
 *   - it NEVER answers wrongly, which is what got the previous badge reverted;
 *   - it refuses on torn records rather than guessing.
 *
 * A refusal rate is reported, not asserted, so that improving coverage is
 * visible without loosening correctness.
 */

#include "../src/ui/scope_freq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAXN 1024

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

/* ── synthetic sanity, before touching real data ──────────────────────── */

static void test_synthetic(void)
{
    uint8_t s[MAXN];
    scope_freq_t r;

    /* A clean 500 Hz sine at 12,575 S/s: bin 40.7, unambiguous. */
    for (int i = 0; i < MAXN; i++)
        s[i] = (uint8_t)(128.0 + 90.0 * sin(2.0 * M_PI * 500.0 * i / 12575.0));

    CHECK(scope_freq_estimate(s, MAXN, 12575.0f, &r),
          "a clean synthetic sine must be accepted");
    CHECK(fabsf(r.hz - 500.0f) < 5.0f,
          "clean 500 Hz sine read as %.1f Hz", (double)r.hz);
    CHECK(r.sharpness > 0.95f, "a clean sine should be very sharp, got %.3f",
          (double)r.sharpness);
    CHECK(r.window == MAXN, "a clean record must not need the half-window");

    /* DC: no frequency exists, and inventing one is the failure mode this
     * whole module is a response to. */
    memset(s, 128, sizeof(s));
    CHECK(!scope_freq_estimate(s, MAXN, 12575.0f, &r), "flat DC must be refused");
    CHECK(r.hz == 0.0f, "a refusal must leave hz at exactly 0");

    /* No sample rate -> no frequency. */
    for (int i = 0; i < MAXN; i++)
        s[i] = (uint8_t)(128.0 + 90.0 * sin(2.0 * M_PI * 500.0 * i / 12575.0));
    CHECK(!scope_freq_estimate(s, MAXN, 0.0f, &r),
          "a timebase code with no rate must yield no frequency");

    /* Bad geometry is refused rather than clamped. */
    CHECK(!scope_freq_estimate(s, 1000, 12575.0f, &r), "non-power-of-two refused");
    CHECK(!scope_freq_estimate(s, 32, 12575.0f, &r),   "too-short refused");
    CHECK(!scope_freq_estimate(NULL, MAXN, 12575.0f, &r), "NULL refused");
    CHECK(!scope_freq_estimate(s, MAXN, 12575.0f, NULL), "NULL out refused");
}

/* ── the real records ─────────────────────────────────────────────────── */

static void test_bench_records(const char *path)
{
    FILE *fh = fopen(path, "r");
    if (fh == NULL) {
        printf("FAIL: cannot open %s\n", path);
        failures++;
        return;
    }

    int c;
    while ((c = fgetc(fh)) == '#') {          /* skip comment lines */
        while ((c = fgetc(fh)) != '\n' && c != EOF) { }
    }
    ungetc(c, fh);

    int count = 0;
    if (fscanf(fh, "%d", &count) != 1 || count <= 0) {
        printf("FAIL: %s has no record count\n", path);
        failures++;
        fclose(fh);
        return;
    }

    uint8_t s[MAXN];
    int answered = 0, refused = 0, wrong = 0;
    float worst = 0.0f;

    for (int rec = 0; rec < count; rec++) {
        int range, code, drive, span;
        float fs;
        if (fscanf(fh, "%d %d %f %d %d", &range, &code, &fs, &drive, &span) != 5) {
            printf("FAIL: %s truncated at record %d\n", path, rec);
            failures++;
            break;
        }
        for (int i = 0; i < MAXN; i++) {
            int v = 0;
            if (fscanf(fh, "%d", &v) != 1) v = 0;
            s[i] = (uint8_t)v;
        }

        scope_freq_t r;
        if (!scope_freq_estimate(s, MAXN, fs, &r)) {
            refused++;
            CHECK(r.hz == 0.0f,
                  "record %d refused but hz = %.1f — a refusal must be silent, "
                  "not quiet", rec, (double)r.hz);
            continue;
        }

        answered++;
        const float err = fabsf(r.hz - (float)drive) / (float)drive * 100.0f;
        if (err > worst) worst = err;
        if (err > 5.0f) {
            wrong++;
            printf("  WRONG: range %d code 0x%02X span %d, %d Hz -> %.1f Hz "
                   "(%+.1f%%, sharpness %.2f, window %u)\n",
                   range, code, span, drive, (double)r.hz, (double)err,
                   (double)r.sharpness, r.window);
        }
    }
    fclose(fh);

    printf("  bench records: %d answered, %d refused, %d wrong, worst %.1f%%\n",
           answered, refused, wrong, (double)worst);

    /* THE contract. The previous badge was reverted for producing confident
     * wrong numbers; producing none is acceptable, producing wrong ones is
     * not. */
    CHECK(wrong == 0, "%d record(s) answered with more than 5%% error", wrong);
    CHECK(worst <= 5.0f, "worst error %.1f%% exceeds the 5%% contract",
          (double)worst);

    /* Coverage is a quality target, not a correctness one — but a collapse to
     * near-zero answers would mean a regression that the `wrong == 0` check
     * alone would happily pass. Measured coverage was 39/72; allow slack. */
    CHECK(answered >= 30,
          "only %d of %d records answered — coverage regressed (was 39)",
          answered, count);
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1]
                                  : "tests/fixtures/scope_records.txt";
    test_synthetic();
    test_bench_records(path);

    if (failures == 0) {
        printf("test_scope_freq: all checks passed\n");
        return 0;
    }
    printf("test_scope_freq: %d check(s) FAILED\n", failures);
    return 1;
}
