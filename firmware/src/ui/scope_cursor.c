/*
 * OpenScope 2C53T — cursor delta readouts
 *
 * See scope_cursor.h for why this module owns no constants of its own.
 * Deliberately free of hardware and RTOS dependencies so it can be exercised
 * on the host — see tests/test_scope_cursor.c.
 */

#include "scope_cursor.h"

#include "scope_cal.h"
#include "scope_timebase.h"

#include <stdio.h>
#include <stddef.h>

/* The string every other part of the scope UI uses for "cannot measure". */
#define CURSOR_NA   "--"

static scope_cursor_reading_t make(scope_cursor_unit_t u,
                                   scope_cursor_conf_t c,
                                   float v)
{
    scope_cursor_reading_t r;
    r.unit = u;
    r.confidence = c;
    r.value = v;
    return r;
}

/* scope_cal / scope_timebase tiers -> cursor confidence. The mapping is
 * deliberately total: a tier this module does not recognise must not silently
 * become MEASURED. */
static scope_cursor_conf_t conf_from_cal(scope_cal_tier_t t)
{
    if (t == SCOPE_CAL_MEASURED)    return SCOPE_CURSOR_MEASURED;
    if (t == SCOPE_CAL_PROVISIONAL) return SCOPE_CURSOR_PROVISIONAL;
    return SCOPE_CURSOR_RAW;
}

static scope_cursor_conf_t conf_from_tb(scope_tb_tier_t t)
{
    if (t == SCOPE_TB_MEASURED)    return SCOPE_CURSOR_MEASURED;
    if (t == SCOPE_TB_PROVISIONAL) return SCOPE_CURSOR_PROVISIONAL;
    return SCOPE_CURSOR_RAW;
}

scope_cursor_reading_t scope_cursor_delta_t(uint8_t tb_code, int32_t dx_px)
{
    /* Exact, and true whether or not a rate exists: the plot draws one sample
     * per column. */
    const float samples = (float)dx_px * SCOPE_CURSOR_SAMPLES_PER_PIXEL;

    const float fs = scope_timebase_sample_rate(tb_code);
    if (fs <= 0.0f) {
        /* No measured rate for this code. Samples are what we have, and a
         * sample count is not a duration however confidently it is printed. */
        return make(SCOPE_CURSOR_UNIT_SAMPLES, SCOPE_CURSOR_RAW, samples);
    }

    return make(SCOPE_CURSOR_UNIT_SECONDS,
                conf_from_tb(scope_timebase_get_tier(tb_code)),
                samples / fs);
}

scope_cursor_reading_t scope_cursor_one_over_dt(uint8_t tb_code, int32_t dx_px)
{
    const int32_t adx = (dx_px < 0) ? -dx_px : dx_px;

    /* Cursors on the same column: there is no interval, so there is no
     * frequency. Not a calibration problem — an arithmetic one. */
    if (adx == 0)
        return make(SCOPE_CURSOR_UNIT_NONE, SCOPE_CURSOR_RAW, 0.0f);

    const float fs = scope_timebase_sample_rate(tb_code);
    if (fs <= 0.0f) {
        /* 1/samples has no unit to fall back to. "--" beats inventing a rate,
         * which is exactly what the old fixed time_per_pixel did. */
        return make(SCOPE_CURSOR_UNIT_NONE, SCOPE_CURSOR_RAW, 0.0f);
    }

    const float dt = ((float)adx * SCOPE_CURSOR_SAMPLES_PER_PIXEL) / fs;
    return make(SCOPE_CURSOR_UNIT_HERTZ,
                conf_from_tb(scope_timebase_get_tier(tb_code)),
                1.0f / dt);
}

scope_cursor_reading_t scope_cursor_delta_v(const scope_cursor_vmap_t *map,
                                            int32_t dy_px)
{
    if (map == NULL || map->counts_per_pixel <= 0.0f) {
        /* The renderer's vertical transform is unknown (demo trace, no
         * capture, or cursors straddling two autofit bands with different
         * scales). Pixels are the only exact statement left. */
        return make(SCOPE_CURSOR_UNIT_PIXELS, SCOPE_CURSOR_RAW, (float)dy_px);
    }

    const float counts = (float)dy_px * map->counts_per_pixel;

    const float k = scope_cal_volts_per_count(map->channel, map->range_idx);
    if (k <= 0.0f) {
        /* Uncalibrated range (0-3 rail), or an out-of-domain channel.
         * scope_cal returns exactly 0.0f for both, and the contract is that
         * callers fall back to counts rather than inventing a gain. */
        return make(SCOPE_CURSOR_UNIT_COUNTS, SCOPE_CURSOR_RAW, counts);
    }

    return make(SCOPE_CURSOR_UNIT_VOLTS,
                conf_from_cal(scope_cal_get_tier(map->channel, map->range_idx)),
                counts * k);
}

/* ── formatting ─────────────────────────────────────────────────────────
 *
 * Magnitude only; the caller below prepends the marker and the sign. The
 * unit steps mirror fmt_seconds()/fmt_volts()/fmt_hz() in scope_ui.c so the
 * cursor readout and the measurement badges cannot disagree about how the
 * same quantity is written.
 */

static void fmt_seconds_mag(char *b, uint32_t n, float s)
{
    if (s < 1e-6f) {
        snprintf(b, n, "%uns", (unsigned)(s * 1e9f + 0.5f));
    } else if (s < 1e-3f) {
        snprintf(b, n, "%uus", (unsigned)(s * 1e6f + 0.5f));
    } else if (s < 1.0f) {
        const unsigned hundredths_ms = (unsigned)(s * 1e5f + 0.5f);
        snprintf(b, n, "%u.%02ums", hundredths_ms / 100u, hundredths_ms % 100u);
    } else {
        const unsigned hundredths = (unsigned)(s * 100.0f + 0.5f);
        snprintf(b, n, "%u.%02us", hundredths / 100u, hundredths % 100u);
    }
}

static void fmt_volts_mag(char *b, uint32_t n, float v)
{
    const unsigned mv = (unsigned)(v * 1000.0f + 0.5f);
    if (mv < 1000u)
        snprintf(b, n, "%umV", mv);
    else
        snprintf(b, n, "%u.%02uV", mv / 1000u, (mv % 1000u) / 10u);
}

static void fmt_hz_mag(char *b, uint32_t n, float hz)
{
    if (hz < 1000.0f) {
        const unsigned tenths = (unsigned)(hz * 10.0f + 0.5f);
        snprintf(b, n, "%u.%uHz", tenths / 10u, tenths % 10u);
    } else if (hz < 1000000.0f) {
        const unsigned hundredths = (unsigned)(hz / 10.0f + 0.5f);
        snprintf(b, n, "%u.%02ukHz", hundredths / 100u, hundredths % 100u);
    } else {
        const unsigned hundredths = (unsigned)(hz / 10000.0f + 0.5f);
        snprintf(b, n, "%u.%02uMHz", hundredths / 100u, hundredths % 100u);
    }
}

void scope_cursor_format(const scope_cursor_reading_t *r, char *out, uint32_t n)
{
    if (out == NULL || n == 0u)
        return;

    if (r == NULL || r->unit == SCOPE_CURSOR_UNIT_NONE) {
        snprintf(out, n, CURSOR_NA);
        return;
    }

    /* Leading '~' for a value converted through a PROVISIONAL table row —
     * the same one-character warning scope_cal_range_label() puts on the
     * status bar, for the same reason: it is the only place a user can see
     * the difference between "measured" and "roughly". */
    const char *mark = (r->confidence == SCOPE_CURSOR_PROVISIONAL) ? "~" : "";
    const char *sign = (r->value < 0.0f) ? "-" : "";
    const float mag  = (r->value < 0.0f) ? -r->value : r->value;

    char body[16];
    body[0] = '\0';

    switch (r->unit) {
    case SCOPE_CURSOR_UNIT_SECONDS:
        fmt_seconds_mag(body, sizeof(body), mag);
        break;
    case SCOPE_CURSOR_UNIT_VOLTS:
        fmt_volts_mag(body, sizeof(body), mag);
        break;
    case SCOPE_CURSOR_UNIT_HERTZ:
        fmt_hz_mag(body, sizeof(body), mag);
        break;
    case SCOPE_CURSOR_UNIT_SAMPLES:
        snprintf(body, sizeof(body), "%usmp", (unsigned)(mag + 0.5f));
        break;
    case SCOPE_CURSOR_UNIT_COUNTS:
        snprintf(body, sizeof(body), "%ucnt", (unsigned)(mag + 0.5f));
        break;
    case SCOPE_CURSOR_UNIT_PIXELS:
        snprintf(body, sizeof(body), "%upx", (unsigned)(mag + 0.5f));
        break;
    default:
        snprintf(out, n, CURSOR_NA);
        return;
    }

    snprintf(out, n, "%s%s%s", mark, sign, body);
}
