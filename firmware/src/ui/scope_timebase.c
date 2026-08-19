/*
 * OpenScope 2C53T — horizontal (time) calibration
 *
 * See scope_timebase.h for provenance, the tier rules, and why an R^2 alone is
 * not sufficient evidence for a sample rate. Free of hardware and RTOS
 * dependencies so it can be exercised on the host — see
 * tests/test_scope_timebase.c.
 */

#include "scope_timebase.h"

#include <stdio.h>

/*
 * Capture sample rate in S/s, indexed by reg-0x01 code, bench unit #1.
 * Source: docs/experiments/2026-08-18-10-time-axis.md and -12-.
 *
 * 0.0f means "no trustworthy rate", and the three reasons for that are
 * genuinely different — see the tier table below. Do not collapse them.
 *
 * RESCALED 2026-08-19 (EXP-14). Every rate here was previously 1.21x too high,
 * because the fits used the frequency COMMANDED from the ESP32 source while the
 * source delivered 0.8250x that. Its DDS loop reschedules from `now` after the
 * work is done, so its real rate is whatever two dacWrite() calls cost — 32,999
 * Hz against an assumed 40,000, measured against the ESP32's own crystal. The
 * generator now measures and reports its loop rate, and the fits divide by the
 * measured value. Consequences worth keeping in mind:
 *
 *   - the error was purely multiplicative, so the 1-2-5 SHAPE of this ladder,
 *     every R^2, and every fold check were untouched. Only the scale moved.
 *   - the corrected 0x10 lands 1.1% from Stlkv's independent figure (12,437),
 *     measured on a different unit with a different generator and a completely
 *     different method. That is this project's first cross-rig agreement on an
 *     ABSOLUTE quantity.
 *   - the round 1-2-5 ladder 12.5k / 25k / 50k / 125k sits inside our
 *     run-to-run spread on all four codes and is very likely the truth, but it
 *     is NOT written here. These are the measurements; fitting them to the
 *     round numbers we suspect would be inventing data.
 *
 * The AMPLITUDE calibration in scope_cal.c is untouched by this — the loop-rate
 * error moves frequency only. Absolute volts remain unverified.
 *
 * WITHDRAWN 2026-08-18 (EXP-12): code 0x08 was published at 1.07 kS/s, then at
 * 1,660 S/s (EXP-10), then fitted at 1,414 and 1,526 S/s in two passes minutes
 * apart. All are artifacts. The record at 0x08 does not reproduce between
 * reads — one unchanged tone gave peak bins [171, 132, 104] — and the fitted
 * rate missed its own fold predictions by up to 227 bins. It is entered here
 * as 0.0f, and the reason is INCOHERENT rather than UNMEASURED.
 */
static const float sample_rate[SCOPE_TIMEBASE_CODE_COUNT] = {
    /* 0x00-0x07 */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    /* 0x08 */      0.0f,        /* INCOHERENT — measured, no rate exists */
    /* 0x09 */      0.0f,        /* never measured */
    /* 0x0A */      0.0f,        /* source too slow to measure (R2 -1.03)   */
    /* 0x0B */      0.0f,        /* source too slow to measure (R2 -0.05)   */
    /* 0x0C */      0.0f,        /* source too slow to measure (R2  0.60)   */
    /* 0x0D */      119678.0f,   /* R2 0.947 — provisional                  */
    /* 0x0E */       49056.0f,   /* R2 0.993                                */
    /* 0x0F */       25736.0f,   /* R2 0.999                                */
    /* 0x10 */       12575.0f,   /* R2 0.999, fold-checked, cross-rig 1.1%  */
    /* 0x11-0x14 */ 0.0f, 0.0f, 0.0f, 0.0f,
};

/*
 * Why each code is where it is. Three distinct kinds of "no":
 *
 *   0x08          INCOHERENT. Measured, and the record does not reproduce.
 *                 A statement about the device.
 *   0x0A-0x0C     UNMEASURABLE WITH OUR SOURCE. The ESP32 tops out near
 *                 4.5 kHz, which at these rates puts the tone in bins 1-15,
 *                 so the fit measures quantisation rather than rate. A
 *                 statement about our instrument — the device is probably
 *                 fine. A faster generator settles these.
 *   everything
 *   else at 0.0f  NEVER MEASURED.
 *
 * Conflating the three would be the same error as treating a floating pin and
 * a pin driven low as one thing, which cost this project weeks.
 */
static const scope_tb_tier_t tier_by_code[SCOPE_TIMEBASE_CODE_COUNT] = {
    SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE,
    SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE,
    SCOPE_TB_NONE,          /* 0x08 incoherent   */
    SCOPE_TB_NONE,          /* 0x09 unmeasured   */
    SCOPE_TB_NONE,          /* 0x0A unmeasurable */
    SCOPE_TB_NONE,          /* 0x0B unmeasurable */
    SCOPE_TB_NONE,          /* 0x0C unmeasurable */
    SCOPE_TB_PROVISIONAL,   /* 0x0D              */
    SCOPE_TB_MEASURED,      /* 0x0E              */
    SCOPE_TB_MEASURED,      /* 0x0F              */
    SCOPE_TB_MEASURED,      /* 0x10              */
    SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE, SCOPE_TB_NONE,
};

float scope_timebase_sample_rate(uint8_t code)
{
    if (code >= SCOPE_TIMEBASE_CODE_COUNT)
        return 0.0f;
    return sample_rate[code];
}

scope_tb_tier_t scope_timebase_get_tier(uint8_t code)
{
    if (code >= SCOPE_TIMEBASE_CODE_COUNT)
        return SCOPE_TB_NONE;

    /* A code with no rate cannot be anything but NONE, whatever the tier
     * table says. Derive the guard rather than trusting the two to agree. */
    if (sample_rate[code] <= 0.0f)
        return SCOPE_TB_NONE;

    return tier_by_code[code];
}

float scope_timebase_seconds_per_sample(uint8_t code)
{
    const float fs = scope_timebase_sample_rate(code);
    return (fs > 0.0f) ? (1.0f / fs) : 0.0f;
}

float scope_timebase_seconds_per_div(uint8_t code)
{
    return scope_timebase_seconds_per_sample(code) *
           SCOPE_TIMEBASE_SAMPLES_PER_DIV;
}

float scope_timebase_hz_from_period(uint8_t code, float period_samples)
{
    const float fs = scope_timebase_sample_rate(code);

    if (fs <= 0.0f || period_samples <= 0.0f)
        return 0.0f;

    return fs / period_samples;
}

void scope_timebase_label(uint8_t code, char *out, uint32_t n)
{
    if (out == NULL || n == 0u)
        return;

    const float s = scope_timebase_seconds_per_div(code);

    if (s <= 0.0f) {
        /* No rate. The grid has no time meaning on this code, and saying so is
         * the point — a nominal label here would be the defect this module
         * exists to remove. */
        snprintf(out, n, "--");
        return;
    }

    const char *mark =
        (scope_timebase_get_tier(code) == SCOPE_TB_PROVISIONAL) ? "~" : "";

    /* Integer formatting throughout: the shell and the LCD both build against
     * nano.specs, which has no float printf. */
    if (s < 1e-3f) {
        snprintf(out, n, "%s%uus", mark, (unsigned)(s * 1e6f + 0.5f));
    } else if (s < 1.0f) {
        const unsigned hundredths_ms = (unsigned)(s * 1e5f + 0.5f);
        snprintf(out, n, "%s%u.%02ums", mark,
                 hundredths_ms / 100u, hundredths_ms % 100u);
    } else {
        const unsigned hundredths_s = (unsigned)(s * 100.0f + 0.5f);
        snprintf(out, n, "%s%u.%02us", mark,
                 hundredths_s / 100u, hundredths_s % 100u);
    }
}
