/*
 * OpenScope 2C53T — per-channel, per-range vertical calibration
 *
 * See scope_cal.h for provenance, tiers, and the source-scale contract.
 * This file is deliberately free of hardware and RTOS dependencies so it can
 * be exercised on the host — see tests/test_scope_cal.c.
 */

#include "scope_cal.h"

#include <stdio.h>

/*
 * mV per ADC count, bench unit #1, 2026-08-17.
 * Source: docs/experiments/2026-08-17-08-per-range-gain-ladders.md
 *
 * 0.0f means "this range produced no usable span at any drive amplitude" —
 * it is not a missing measurement, it is a measured refusal.
 *
 * WITHDRAWN 2026-08-18: the two values this table replaces, which lived as
 * bare constants in scope_ui.c's scope_cal_volts_per_count():
 *     range 2 -> 2.882 mV/count ("20mV/div", 347 codes per volt)
 *     range 8 -> 6.494 mV/count ("2V/div",   154 codes per volt)
 * Both are contradicted by the five-amplitude sweep: range 2 rails on both
 * channels and yields no span, and range 8 measures 279.05 (CH1) / 223.71
 * (CH2), a factor of 43 away. Kept here in writing rather than deleted,
 * because the failure mode — a plausible stable number nobody could tell was
 * wrong — is one this project has now hit four separate times.
 */
static const float mv_per_count[2][SCOPE_CAL_RANGE_COUNT] = {
    /*        r0     r1     r2     r3      r4      r5      r6      r7       r8       r9 */
    /* CH1 */ { 0.0f, 0.0f, 0.0f, 0.0f,  14.08f, 21.83f, 42.95f, 88.42f, 279.05f, 352.17f },
    /* CH2 */ { 0.0f, 0.0f, 0.0f, 0.0f,   9.49f, 20.96f, 41.71f, 83.79f, 223.71f, 425.00f },
};

/*
 * Confidence per range. Identical for both channels by construction: the
 * tier is a statement about how well the two channels AGREE, so it cannot
 * differ between them.
 *
 *   r0-r3  NONE         railed on both channels at every amplitude
 *   r4     PROVISIONAL  channels differ by 48% (14.08 vs 9.49)
 *   r5     MEASURED     channels differ by 4.1%
 *   r6     MEASURED     channels differ by 2.9%
 *   r7     MEASURED     channels differ by 5.5%
 *   r8     PROVISIONAL  channels differ by 25%
 *   r9     PROVISIONAL  channels differ by 21%, and in the opposite direction
 *                       to r8 — CH2 reads HIGHER here and lower there, which
 *                       is not a fixed per-channel gain error and is the
 *                       clearest sign the sweep amplitudes were wrong for
 *                       these rows rather than the hardware being odd.
 */
static const scope_cal_tier_t tier_by_range[SCOPE_CAL_RANGE_COUNT] = {
    SCOPE_CAL_NONE,         /* 0 */
    SCOPE_CAL_NONE,         /* 1 */
    SCOPE_CAL_NONE,         /* 2 */
    SCOPE_CAL_NONE,         /* 3 */
    SCOPE_CAL_PROVISIONAL,  /* 4 */
    SCOPE_CAL_MEASURED,     /* 5 */
    SCOPE_CAL_MEASURED,     /* 6 */
    SCOPE_CAL_MEASURED,     /* 7 */
    SCOPE_CAL_PROVISIONAL,  /* 8 */
    SCOPE_CAL_PROVISIONAL,  /* 9 */
};

/* Channel 1/2 -> row 0/1, or -1 for anything else. Out-of-domain arguments
 * return "no calibration" rather than clamping into a neighbouring channel:
 * silently addressing the wrong channel is exactly how `fpga scope range
 * <n> 0` came to address both of them. */
static int channel_row(uint8_t ch)
{
    if (ch == 1u) return 0;
    if (ch == 2u) return 1;
    return -1;
}

float scope_cal_mv_per_count(uint8_t ch, uint8_t range_idx)
{
    const int row = channel_row(ch);

    if (row < 0 || range_idx >= SCOPE_CAL_RANGE_COUNT)
        return 0.0f;

    return mv_per_count[row][range_idx] * SCOPE_CAL_SOURCE_SCALE;
}

float scope_cal_volts_per_count(uint8_t ch, uint8_t range_idx)
{
    return scope_cal_mv_per_count(ch, range_idx) / 1000.0f;
}

scope_cal_tier_t scope_cal_get_tier(uint8_t ch, uint8_t range_idx)
{
    if (channel_row(ch) < 0 || range_idx >= SCOPE_CAL_RANGE_COUNT)
        return SCOPE_CAL_NONE;

    /* An entry with no gain cannot be anything but NONE, whatever the tier
     * table says. Keeping the two in sync is a maintenance hazard, so derive
     * the guard rather than trusting agreement. */
    if (mv_per_count[channel_row(ch)][range_idx] <= 0.0f)
        return SCOPE_CAL_NONE;

    return tier_by_range[range_idx];
}

float scope_cal_volts_per_div(uint8_t ch, uint8_t range_idx)
{
    return scope_cal_volts_per_count(ch, range_idx) * SCOPE_CAL_COUNTS_PER_DIV;
}

void scope_cal_range_label(uint8_t ch, uint8_t range_idx, char *out, uint32_t n)
{
    if (out == NULL || n == 0u)
        return;

    const float v = scope_cal_volts_per_div(ch, range_idx);

    if (v <= 0.0f) {
        /* No cal. The grid has no volts meaning on this range, and saying so
         * is the point — a nominal label here would be the defect this
         * module exists to remove. */
        snprintf(out, n, "--");
        return;
    }

    /* Provisional entries carry a leading '~' so the face of the instrument
     * distinguishes "measured" from "roughly". It costs one character and it
     * is the only place a user can see the difference. */
    const char *mark =
        (scope_cal_get_tier(ch, range_idx) == SCOPE_CAL_PROVISIONAL) ? "~" : "";

    if (v < 1.0f) {
        snprintf(out, n, "%s%umV", mark, (unsigned)(v * 1000.0f + 0.5f));
    } else {
        const unsigned centivolts = (unsigned)(v * 100.0f + 0.5f);
        snprintf(out, n, "%s%u.%02uV", mark, centivolts / 100u, centivolts % 100u);
    }
}
