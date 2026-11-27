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
 * mV per ADC count at the BNC: the shared table, derived from two units.
 * docs/calibration/scope_gain.md has every unit's numbers and the rule;
 * tests/test_scope_cal.c holds the same per-unit numbers and checks each row
 * against the rule, so a row edited on its own fails the build.
 *
 *   r0-r3  s00500's unit (2026-11-26): 7-9-level slope fits against the
 *          2C53T's generator, its scale set by a Rigol DS1104Z after
 *          Self-Cal (0.7937 mV per DAC code). Nobody else has these ranges.
 *   r4-r7  the mean of s00500's unit and sparsematrix's (#95, DMM on the
 *          same node). They agree within 2.6 % on every entry.
 *   r8-r9  sparsematrix's unit only. s00500's fits there span 7-15 counts
 *          and put the generator's 0 V at -55 / -71 mV, against +7..+10 mV
 *          on every other range; they are a cross-check, not data.
 *
 * Both references are absolute, so SCOPE_CAL_SOURCE_SCALE is 1.0.
 *
 * WITHDRAWN 2026-11-27: bench unit #1's table (2026-08-17, EXP-08), read
 * through the 0.92 source scale it needed:
 *     CH1 { 0, 0, 0, 0, 14.08, 21.83, 42.95, 88.42, 279.05, 352.17 }
 *     CH2 { 0, 0, 0, 0,  9.49, 20.96, 41.71, 83.79, 223.71, 425.00 }
 * Against the two units above its MEASURED ranges 5-7 held within 3.6 %. Its
 * PROVISIONAL ranges were the ones off: range 4 by +66 % (CH1) / +11 % (CH2),
 * range 8, the power-on default, by +33 % on CH1, range 9 by -16 % on CH1.
 * Ranges 0-3 were 0.0f because in August they sat at 255 with a quiet input
 * (EXP-11). With the offset references armed as main does now (DAC1 2048,
 * TMR13 2544) a shorted input sits at 44-49 counts on CH1 and 101-106 on CH2
 * on every range (s00500's EXP-89), and the four ranges measure cleanly.
 *
 * WITHDRAWN 2026-08-18: the two values that table replaced, which lived as
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
    /*        r0       r1       r2       r3      r4     r5      r6      r7      r8      r9 */
    /* CH1 */ { 0.4028f, 0.7981f, 1.9872f, 3.927f, 7.82f, 19.92f, 39.63f, 78.55f, 193.5f, 384.2f },
    /* CH2 */ { 0.3978f, 0.7928f, 1.9732f, 3.906f, 7.86f, 19.92f, 39.67f, 78.15f, 196.0f, 391.1f },
};

/*
 * Confidence per range, the same for both channels (they agree within 1.8 %
 * on every range).
 *
 *   r0-r3  MEASURED     one unit, 7-9-level fits, channels within 1.3 %
 *   r4-r7  MEASURED     two units within 2.6 %
 *   r8-r9  PROVISIONAL  one unit at usable resolution; the label carries '~'
 *
 * Until 2026-11-27: r0-r3 NONE (railed, EXP-11), r4/r8/r9 PROVISIONAL
 * (bench unit #1's channels 21-48 % apart there), r5-r7 MEASURED.
 */
static const scope_cal_tier_t tier_by_range[SCOPE_CAL_RANGE_COUNT] = {
    SCOPE_CAL_MEASURED,     /* 0 */
    SCOPE_CAL_MEASURED,     /* 1 */
    SCOPE_CAL_MEASURED,     /* 2 */
    SCOPE_CAL_MEASURED,     /* 3 */
    SCOPE_CAL_MEASURED,     /* 4 */
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

static scope_cal_probe_fn probe_src;

void scope_cal_set_probe_source(scope_cal_probe_fn fn)
{
    probe_src = fn;
}

float scope_cal_probe_factor(uint8_t ch)
{
    if (probe_src == NULL || channel_row(ch) < 0)
        return 1.0f;
    const float f = probe_src(ch);
    return (f > 0.0f) ? f : 1.0f;
}

float scope_cal_volts_per_count(uint8_t ch, uint8_t range_idx)
{
    return scope_cal_mv_per_count(ch, range_idx) / 1000.0f * scope_cal_probe_factor(ch);
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

int scope_cal_true_scale_ok(uint8_t ch, uint8_t range_idx)
{
    /* Same gate the labels and badges use: a range has a volts meaning exactly
     * when its gain is > 0. Deriving it from the gain (not the tier table)
     * keeps the two from drifting apart, for the same reason
     * scope_cal_get_tier() does. */
    return scope_cal_volts_per_count(ch, range_idx) > 0.0f;
}

static void format_range_label(uint8_t ch, uint8_t range_idx, float v,
                               char *out, uint32_t n)
{
    if (out == NULL || n == 0u)
        return;

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

void scope_cal_range_label(uint8_t ch, uint8_t range_idx, char *out, uint32_t n)
{
    format_range_label(ch, range_idx, scope_cal_volts_per_div(ch, range_idx), out, n);
}

void scope_cal_range_label_bnc(uint8_t ch, uint8_t range_idx, char *out, uint32_t n)
{
    format_range_label(ch, range_idx,
                       scope_cal_mv_per_count(ch, range_idx) / 1000.0f * SCOPE_CAL_COUNTS_PER_DIV,
                       out, n);
}
