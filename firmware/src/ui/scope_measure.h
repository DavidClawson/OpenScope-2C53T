/*
 * OpenScope 2C53T — honest measurements over a raw capture record
 *
 * WHAT THIS IS FOR
 * ----------------
 * The scope UI needs numbers to put in its measurement badges. This module
 * computes ONLY the quantities that are genuinely derivable from the samples
 * we actually have, and nothing else. Everything here is expressed in the
 * units the instrument really observes today:
 *
 *   - ADC COUNTS for anything vertical. Counts -> volts needs the per-range
 *     gain/offset calibration this firmware does not have yet (dev plan §F2:
 *     "per-range scope calibration ... needs known signals at known
 *     amplitudes"). Until then a volt figure would be invented, so none is
 *     produced.
 *   - SAMPLES / RECORD for anything horizontal. Samples -> seconds needs a
 *     known sample rate, i.e. a timebase; this firmware has no timebase
 *     control at all (dev plan §F4: TMR3 is stock's acquisition pacer and
 *     ours is owned by the 500 Hz button scan). So no frequency, no period,
 *     no rise time is produced.
 *
 * What survives that filter is real:
 *   min / max / peak-to-peak / mean / AC RMS  in counts,
 *   duty cycle                                as a pure ratio,
 *   cycles per record                         as a pure count,
 *   period                                    in samples.
 *
 * Duty and cycle count are invariant under ANY affine counts->volts mapping
 * (y = a*x + b, a > 0): the mid-level threshold, the sample ordering and the
 * crossing instants all move together. That is why they are correct despite
 * the missing vertical calibration, and it is asserted directly by
 * tests/test_scope_measure.c.
 *
 * WHY NOT measurement_compute() (src/tasks/measurement.c)?
 * It is unit-tested and correct on its own terms, but its terms are not this
 * hardware's: it scales every voltage by 3.3/32768 (a 16-bit ADC over a 3.3 V
 * span — ours is 8-bit through an analog frontend of unknown gain) and its
 * frequency/period/rise-time outputs are all multiplied by a caller-supplied
 * sample_rate we do not know. Feeding it counts and unwinding its scale
 * factor afterwards would produce the same numbers this file produces, but
 * coupled to a constant in another module that is wrong for this board — a
 * silent-breakage hazard. When a real timebase and real cal exist, the right
 * move is to fix that constant, pass the true sample rate, and call it.
 */

#ifndef SCOPE_MEASURE_H
#define SCOPE_MEASURE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * A record whose peak-to-peak is below this is treated as "no discernible
 * signal": the mid-level threshold would be sitting inside the noise, so duty
 * and cycle count would be counting noise, not the waveform. 6 counts of
 * 255 ~ 2.4% of the ADC span.
 */
#define SCOPE_MEASURE_MIN_PP  6u

typedef struct {
    bool     valid;         /* a usable record was supplied at all           */

    /* Vertical — ADC COUNTS, never volts (see header comment). */
    uint8_t  min;
    uint8_t  max;
    uint8_t  pp;            /* max - min                                     */
    /* pp with 0.5% of samples trimmed from each end of the DISTRIBUTION
     * before taking the extremes. `pp` is an extreme-value statistic: over a
     * 1024-sample noisy 8-bit record the noise tails inflate both ends, and
     * EXP-19 measured that inflation at +4..+10% of commanded amplitude,
     * shrinking as amplitude grows — the additive signature. Trimming is
     * shape-safe for the waveforms this instrument shows: sines (arcsine
     * density, mass AT the extremes) and squares (mass at the levels) lose
     * nothing; a pure triangle loses ~1% by construction. Use THIS for any
     * voltage the user reads; keep `pp` for validity thresholds. */
    uint8_t  pp_robust;
    float    mean;          /* DC level                                      */
    float    ac_rms;        /* RMS about the mean (mean removed)             */

    /* True once pp >= SCOPE_MEASURE_MIN_PP. When false, duty_pct and cycles
     * are meaningless and MUST NOT be displayed. */
    bool     level_valid;

    /* Horizontal — in SAMPLES, or dimensionless. No sample rate involved. */
    float    duty_pct;      /* % of samples above the mid-level              */
    uint16_t cycles;        /* rising mid-level crossings within the record  */
    bool     cycles_valid;  /* level_valid && >= 2 crossings                 */
    float    period_samples;/* samples per cycle, first->last crossing       */
    bool     period_valid;
} scope_measure_t;

/*
 * Analyse one capture record of unsigned 8-bit samples.
 *
 * samples/n may be NULL/0 — the result is then simply invalid, all fields
 * zero. Never reads outside [0, n).
 */
void scope_measure_record(const uint8_t *samples, uint16_t n,
                          scope_measure_t *out);

#endif /* SCOPE_MEASURE_H */
