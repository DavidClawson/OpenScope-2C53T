/*
 * scope_measure host test — what the badges are allowed to say
 *
 * Build and run:
 *   make -C firmware test-scope-measure
 * or by hand:
 *   gcc -std=gnu11 -Wall -Wextra -Ifirmware/src/ui \
 *       firmware/tests/test_scope_measure.c firmware/src/ui/scope_measure.c \
 *       -lm -o /tmp/t && /tmp/t
 *
 * This compiles the SHIPPED src/ui/scope_measure.c — not a transcription of
 * it (contrast test_waterfall_blit.c, which has to copy its two blit loops
 * because they are entangled with the FFT, the fonts and the AT32 headers).
 * scope_measure.c was deliberately written free of all of that so this test
 * exercises the same object the firmware links.
 *
 * What is asserted, and why each assertion is the interesting one:
 *
 *  1. Known synthetic records in, known numbers out: peak-to-peak, AC RMS,
 *     duty, cycles and period must match closed-form values computed by hand
 *     from the generator parameters.
 *
 *  2. AFFINE INVARIANCE — the load-bearing one. Duty, cycle count and period
 *     must be BIT-IDENTICAL after the record is passed through y = a*x + b
 *     (a > 0), because that is exactly what the missing vertical calibration
 *     would do to these samples. If they survive it, the UI is entitled to
 *     print them today; if they did not, they would be as fake as the
 *     hardcoded "50.0%" they replaced.
 *
 *  3. Silence where there is nothing to say: a flat record and a
 *     noise-only record must come back level_valid = false, so the UI shows
 *     "--" instead of a duty cycle computed from dither.
 *
 * NEGATIVE CONTROL: the same battery is run against two deliberately broken
 * implementations (fixed 128 threshold; no hysteresis). The test only passes
 * if BOTH are caught. That is the proof the assertions have teeth — a check
 * that cannot fail is this project's signature failure mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "scope_measure.h"

#define N 1024

typedef void (*measure_fn)(const uint8_t *, uint16_t, scope_measure_t *);

/* ── Broken implementations, for the negative control ─────────────────── */

/* Variant A: threshold hardwired at mid-scale (128) instead of the record's
 * own mid-level. Correct for a signal that happens to sit centred on the ADC
 * midpoint; wrong for everything else — precisely the error an uncalibrated
 * vertical path would introduce. */
static void broken_fixed_threshold(const uint8_t *s, uint16_t n,
                                   scope_measure_t *o)
{
    scope_measure_record(s, n, o);
    if (!o->valid || !o->level_valid) return;

    uint32_t above = 0;
    int state = 0;
    uint16_t rising = 0, first = 0, last = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (s[i] > 128) above++;
        if (state == 0 && s[i] > 128) {
            state = 1;
            if (rising == 0) first = i;
            last = i; rising++;
        } else if (state == 1 && s[i] <= 128) {
            state = 0;
        }
    }
    o->duty_pct = (float)above * 100.0f / (float)n;
    o->cycles = rising;
    o->cycles_valid = (rising >= 2);
    o->period_valid = o->cycles_valid;
    o->period_samples = o->cycles_valid
                        ? (float)(last - first) / (float)(rising - 1) : 0.0f;
}

/* Variant B: correct threshold, but no hysteresis — every noise excursion
 * across the mid-level counts as another cycle. */
static void broken_no_hysteresis(const uint8_t *s, uint16_t n,
                                 scope_measure_t *o)
{
    scope_measure_record(s, n, o);
    if (!o->valid || !o->level_valid) return;

    uint16_t mid2 = (uint16_t)o->min + (uint16_t)o->max;
    int state = 0;
    uint16_t rising = 0, first = 0, last = 0;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t s2 = (uint16_t)(2u * (uint16_t)s[i]);
        if (state == 0 && s2 > mid2) {
            state = 1;
            if (rising == 0) first = i;
            last = i; rising++;
        } else if (state == 1 && s2 <= mid2) {
            state = 0;
        }
    }
    o->cycles = rising;
    o->cycles_valid = (rising >= 2);
    o->period_valid = o->cycles_valid;
    o->period_samples = o->cycles_valid
                        ? (float)(last - first) / (float)(rising - 1) : 0.0f;
}

/* ── Signal generators ────────────────────────────────────────────────── */

static uint8_t clamp8(float v)
{
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (uint8_t)(v + 0.5f);
}

/* Square wave: `period` samples per cycle, high for duty% of each cycle,
 * starting at the beginning of a high phase. */
static void gen_square(uint8_t *b, uint16_t n, uint16_t period,
                       float duty, uint8_t lo, uint8_t hi)
{
    uint16_t high_len = (uint16_t)(period * duty / 100.0f + 0.5f);
    for (uint16_t i = 0; i < n; i++)
        b[i] = ((i % period) < high_len) ? hi : lo;
}

static void gen_sine(uint8_t *b, uint16_t n, uint16_t period,
                     float amp, float offset, float noise_amp)
{
    uint32_t lcg = 12345u;
    for (uint16_t i = 0; i < n; i++) {
        float v = offset + amp * sinf(2.0f * (float)M_PI * (float)i / (float)period);
        if (noise_amp > 0.0f) {
            lcg = lcg * 1103515245u + 12345u;
            float r = (float)((lcg >> 16) & 0xFFFF) / 65535.0f - 0.5f;
            v += 2.0f * noise_amp * r;
        }
        b[i] = clamp8(v);
    }
}

static void gen_const(uint8_t *b, uint16_t n, uint8_t v)
{
    memset(b, v, n);
}

/* Apply the affine map an uncalibrated->calibrated vertical path would. */
static void affine(const uint8_t *src, uint8_t *dst, uint16_t n,
                   float a, float b)
{
    for (uint16_t i = 0; i < n; i++)
        dst[i] = clamp8(a * (float)src[i] + b);
}

/* ── Assertion battery ────────────────────────────────────────────────── */

static int g_verbose = 0;

static int chk(const char *what, int ok)
{
    if (g_verbose || !ok)
        printf("      %-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int near(float got, float want, float tol)
{
    return fabsf(got - want) <= tol;
}

/*
 * Runs every case against `f` and returns the number of failed checks.
 * The real implementation must score 0; each broken one must score > 0.
 */
static int battery(measure_fn f)
{
    static uint8_t buf[N], buf2[N];
    scope_measure_t m, m2;
    int bad = 0;

    /* --- Case 1: 25% duty square, 8 cycles, lo=88 hi=168 --------------
     * mean = 0.25*168 + 0.75*88 = 108
     * var  = 0.25*60^2 + 0.75*20^2 = 1200  ->  ac_rms = 34.64 */
    gen_square(buf, N, 128, 25.0f, 88, 168);
    f(buf, N, &m);
    bad += chk("square: valid", m.valid);
    bad += chk("square: pp == 80", m.pp == 80);
    bad += chk("square: min/max == 88/168", m.min == 88 && m.max == 168);
    bad += chk("square: mean ~= 108", near(m.mean, 108.0f, 0.5f));
    bad += chk("square: ac_rms ~= 34.6", near(m.ac_rms, 34.64f, 0.5f));
    bad += chk("square: duty ~= 25%", near(m.duty_pct, 25.0f, 0.5f));
    bad += chk("square: 8 cycles", m.cycles == 8);
    bad += chk("square: period ~= 128 samples",
               m.period_valid && near(m.period_samples, 128.0f, 0.5f));

    /* --- Case 2: affine-mapped copy of case 1 ------------------------
     * y = 0.4x + 60 maps 88 -> 95, 168 -> 127. Nothing about the shape
     * changed, so the shape measurements must not change either. Note the
     * fixed-128-threshold variant sees NOTHING above threshold here. */
    affine(buf, buf2, N, 0.4f, 60.0f);
    f(buf2, N, &m2);
    bad += chk("affine: still level_valid", m2.level_valid);
    bad += chk("affine: duty identical", m2.duty_pct == m.duty_pct);
    bad += chk("affine: cycles identical", m2.cycles == m.cycles);
    bad += chk("affine: period identical",
               m2.period_valid && m2.period_samples == m.period_samples);
    bad += chk("affine: pp scaled to 32", m2.pp == 32);

    /* --- Case 3: clean sine, 4 cycles, amp 50 about 128 ---------------
     * ac_rms = 50/sqrt(2) = 35.36, duty = 50% */
    gen_sine(buf, N, 256, 50.0f, 128.0f, 0.0f);
    f(buf, N, &m);
    bad += chk("sine: ac_rms ~= 35.4", near(m.ac_rms, 35.36f, 1.0f));
    bad += chk("sine: duty ~= 50%", near(m.duty_pct, 50.0f, 1.5f));
    bad += chk("sine: 4 cycles", m.cycles == 4);
    bad += chk("sine: period ~= 256 samples",
               m.period_valid && near(m.period_samples, 256.0f, 2.0f));

    /* --- Case 4: same sine with +/-4 count noise ----------------------
     * Hysteresis is pp/8 ~ 12 counts, comfortably above the noise, so the
     * cycle count must be unchanged. Without hysteresis it explodes. */
    gen_sine(buf, N, 256, 50.0f, 128.0f, 4.0f);
    f(buf, N, &m);
    bad += chk("noisy sine: still 4 cycles", m.cycles == 4);
    bad += chk("noisy sine: period ~= 256 samples",
               m.period_valid && near(m.period_samples, 256.0f, 4.0f));

    /* --- Case 5: flat DC ---------------------------------------------- */
    gen_const(buf, N, 200);
    f(buf, N, &m);
    bad += chk("flat: valid record", m.valid);
    bad += chk("flat: pp == 0", m.pp == 0);
    bad += chk("flat: ac_rms == 0", m.ac_rms == 0.0f);
    bad += chk("flat: NOT level_valid", !m.level_valid);
    bad += chk("flat: no cycles claimed", !m.cycles_valid && !m.period_valid);

    /* --- Case 6: dither only, pp below the floor ---------------------- */
    gen_sine(buf, N, 37, 2.0f, 128.0f, 0.0f);
    f(buf, N, &m);
    bad += chk("dither: pp < floor", m.pp < SCOPE_MEASURE_MIN_PP);
    bad += chk("dither: NOT level_valid", !m.level_valid);
    bad += chk("dither: no duty claimed", m.duty_pct == 0.0f);

    /* --- Case 6b: robust pp vs impulse noise (EXP-19) ------------------
     * The bench measured raw max-min inflated +4..+10% of the commanded
     * amplitude by noise tails. Model it: a clean square (mass AT the
     * levels, so trimming must cost nothing) plus 4 impulse outliers.
     * Raw pp must see the outliers (that is what raw means) and pp_robust
     * must not — and on the CLEAN record the two must agree exactly. */
    gen_square(buf, N, 25, 50.0f, 88, 168);
    f(buf, N, &m);
    bad += chk("clean square: pp_robust == pp", m.pp_robust == m.pp);
    gen_sine(buf2, N, 256, 50.0f, 128.0f, 0.0f);
    f(buf2, N, &m2);
    bad += chk("clean sine: pp_robust within 2 of pp",
               m2.pp >= m2.pp_robust && (m2.pp - m2.pp_robust) <= 2);
    gen_square(buf, N, 25, 50.0f, 88, 168);
    buf[13] = 255; buf[400] = 0; buf[700] = 250; buf[901] = 3;
    f(buf, N, &m);
    bad += chk("impulses: raw pp inflated to 255", m.pp == 255);
    bad += chk("impulses: pp_robust holds at 80", m.pp_robust == 80);

    /* --- Case 7: degenerate inputs are refused, not guessed ----------- */
    f(NULL, N, &m);
    bad += chk("NULL record: invalid", !m.valid);
    f(buf, 0, &m);
    bad += chk("empty record: invalid", !m.valid);
    f(buf, 1, &m);
    bad += chk("1-sample record: invalid", !m.valid);

    return bad;
}

int main(int argc, char **argv)
{
    g_verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);
    int fail = 0;

    printf("scope_measure: shipped implementation\n");
    int real_bad = battery(scope_measure_record);
    printf("   %d failed check(s) -> %s\n\n", real_bad,
           real_bad == 0 ? "PASS" : "*** FAIL ***");
    if (real_bad) fail = 1;

    printf("negative control A: threshold hardwired at 128\n");
    int a_bad = battery(broken_fixed_threshold);
    printf("   %d failed check(s) -> %s\n\n", a_bad,
           a_bad > 0 ? "control OK (test can fail)" : "*** CONTROL FAILED ***");
    if (a_bad == 0) fail = 1;

    printf("negative control B: no hysteresis on the cycle counter\n");
    int b_bad = battery(broken_no_hysteresis);
    printf("   %d failed check(s) -> %s\n\n", b_bad,
           b_bad > 0 ? "control OK (test can fail)" : "*** CONTROL FAILED ***");
    if (b_bad == 0) fail = 1;

    printf(fail ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fail;
}
