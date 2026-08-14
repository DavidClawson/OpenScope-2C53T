/*
 * OpenScope 2C53T — honest measurements over a raw capture record.
 * See scope_measure.h for what is computed and, more importantly, what is
 * deliberately not.
 *
 * Pure C, no hardware, no floats-in-printf: host-testable as-is, and the host
 * test (tests/test_scope_measure.c) compiles THIS file rather than a
 * transcription of it.
 */

#include "scope_measure.h"

#include <math.h>
#include <string.h>

void scope_measure_record(const uint8_t *samples, uint16_t n,
                          scope_measure_t *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (samples == NULL || n < 2)
        return;                     /* out->valid stays false */

    /* ── Pass 1: extrema and the two integer accumulators ──────────────
     *
     * Integer accumulation is exact here and cheap: with n <= 65535 and
     * samples <= 255, sum <= 1.6e7 (fits uint32) and sum_sq <= 4.3e9, which
     * needs the uint64. Doing this in float would lose bits at the top of
     * the range for long records (24-bit mantissa vs sum_sq up to 2^32). */
    uint8_t  smin = samples[0];
    uint8_t  smax = samples[0];
    uint64_t sum = 0;
    uint64_t sum_sq = 0;

    for (uint16_t i = 0; i < n; i++) {
        uint8_t s = samples[i];
        if (s < smin) smin = s;
        if (s > smax) smax = s;
        sum    += s;
        sum_sq += (uint64_t)s * (uint64_t)s;
    }

    out->valid = true;
    out->min   = smin;
    out->max   = smax;
    out->pp    = (uint8_t)(smax - smin);
    out->mean  = (float)sum / (float)n;

    /* Variance about the mean, computed as E[x^2] - E[x]^2 in integer form
     * (n*sum_sq - sum^2) / n^2 so the subtraction happens before any
     * rounding. Clamped at 0 against a -0.0 from a perfectly flat record. */
    {
        double num = (double)n * (double)sum_sq - (double)sum * (double)sum;
        double var = num / ((double)n * (double)n);
        if (var < 0.0) var = 0.0;
        out->ac_rms = sqrtf((float)var);
    }

    out->level_valid = (out->pp >= SCOPE_MEASURE_MIN_PP);
    if (!out->level_valid)
        return;                     /* duty/cycles would be counting noise */

    /* ── Pass 2: duty cycle ────────────────────────────────────────────
     *
     * Fraction of the record spent above the mid-level, mid = (min+max)/2.
     * Both the threshold and the comparison are order-preserving under any
     * affine counts->volts mapping, so this number is already correct in the
     * calibrated world — it does not need the calibration to exist.
     *
     * The mid-level is computed in halves (mid2 = min+max, compared against
     * 2*s) so no rounding is introduced for odd min+max. */
    {
        uint16_t mid2 = (uint16_t)smin + (uint16_t)smax;
        uint32_t above = 0;
        for (uint16_t i = 0; i < n; i++) {
            if ((uint16_t)(2u * (uint16_t)samples[i]) > mid2)
                above++;
        }
        out->duty_pct = (float)above * 100.0f / (float)n;
    }

    /* ── Pass 3: cycles within the record ──────────────────────────────
     *
     * Count low->high transitions of a Schmitt trigger centred on the
     * mid-level, with hysteresis of pp/8 (>= 1 count) so a noisy trace does
     * not chatter across the threshold. Each such transition is one full
     * cycle of whatever is on screen.
     *
     * This is a COUNT, not a frequency. Turning it into Hz needs the sample
     * rate; when a timebase exists, frequency = cycles * rate / n and this
     * field is what to multiply. */
    {
        uint16_t mid2  = (uint16_t)smin + (uint16_t)smax;
        uint16_t hyst2 = (uint16_t)(out->pp / 4u);   /* 2 * (pp/8)          */
        if (hyst2 < 2u) hyst2 = 2u;                  /* 2 * 1 count minimum */

        uint16_t hi_thr2 = (uint16_t)(mid2 + hyst2);
        uint16_t lo_thr2 = (mid2 > hyst2) ? (uint16_t)(mid2 - hyst2) : 0u;

        int state = 0;              /* 0 = below/unknown, 1 = above */
        uint16_t rising = 0;
        uint16_t first_rise = 0, last_rise = 0;

        for (uint16_t i = 0; i < n; i++) {
            uint16_t s2 = (uint16_t)(2u * (uint16_t)samples[i]);
            if (state == 0) {
                if (s2 >= hi_thr2) {
                    state = 1;
                    if (rising == 0) first_rise = i;
                    last_rise = i;
                    rising++;
                }
            } else {
                if (s2 <= lo_thr2)
                    state = 0;
            }
        }

        /* One crossing only tells you where the record happened to start
         * relative to the waveform. A period is the interval BETWEEN
         * crossings, so two are the minimum for any cycle statement. */
        out->cycles = rising;
        out->cycles_valid = (rising >= 2u);

        if (out->cycles_valid) {
            /* Period measured the accurate way: the span from the first to
             * the last rising crossing, divided by the number of whole
             * periods in that span. Units are SAMPLES — the only horizontal
             * unit this instrument can honestly quote. Multiply by the
             * sample interval to get seconds the day a timebase exists;
             * that one multiplication is the entire wiring job. */
            out->period_samples = (float)(last_rise - first_rise) /
                                  (float)(rising - 1u);
            out->period_valid = (out->period_samples > 0.0f);

            /* ── Duty, recomputed over WHOLE CYCLES ────────────────────
             *
             * BENCH 2026-08-14. Pass 2 measures duty across the entire
             * record, which is only unbiased when the record holds an
             * integer number of cycles. It does not: a 2 Hz square at the
             * current acquisition rate gives ~600 samples/cycle in a 1024
             * sample buffer = ~1.7 cycles, so the leftover 0.7 cycle drags
             * the answer around with the window phase. Measured on a
             * commanded 24.7% square, the badge wandered across roughly
             * 20-40% — and the predicted phase-bias range for that record
             * is 14-29%, which matches.
             *
             * [first_rise, last_rise) spans exactly (rising-1) whole
             * cycles by construction, so duty over that span is
             * phase-independent. Still affine-invariant, so it remains
             * correct before any calibration exists.
             *
             * NOTE this does not explain readings near 50% on that signal,
             * which phase bias alone cannot produce. That, and the cycle
             * detector intermittently finding nothing, point at the record
             * not being a uniform-rate time series — the open timebase
             * question (dev plan F4), not an arithmetic problem here. */
            uint32_t above_c = 0;
            for (uint16_t i = first_rise; i < last_rise; i++) {
                if ((uint16_t)(2u * (uint16_t)samples[i]) > mid2)
                    above_c++;
            }
            uint16_t span = (uint16_t)(last_rise - first_rise);
            if (span > 0u)
                out->duty_pct = (float)above_c * 100.0f / (float)span;
        }
    }
}
