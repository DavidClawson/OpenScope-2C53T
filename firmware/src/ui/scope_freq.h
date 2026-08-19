/*
 * OpenScope 2C53T — frequency measurement from a capture record
 *
 * WHY THIS EXISTS
 * ---------------
 * The measurement badge used to derive frequency by counting rising Schmitt
 * crossings and dividing. Against a known drive that read **+1.6% to +112%
 * wrong** (EXP-13), always high, because the estimator found more crossings
 * than the signal contained. The Freq badge was reverted to blank rather than
 * ship it.
 *
 * EXP-16 established why, and it was not the arithmetic. Three unrelated
 * estimators — edge counting, autocorrelation, and a spectral peak — all failed
 * on the SAME records and all succeeded on the same others. That is not an
 * algorithm problem. **Roughly a fifth to a half of capture records are torn**:
 * their spectral energy is smeared over ~6 adjacent bins instead of
 * concentrated, and one such record was measured at 0.380 coherence over its
 * full length while each 512-sample half scored 0.98. The record is stitched,
 * not noisy.
 *
 * Tearing does not correlate with signal amplitude — records at span 11 measure
 * correctly and records at span 107 are unusable — so it cannot be screened out
 * by a quality gate on the input. It has to be DETECTED PER RECORD.
 *
 * WHAT THIS MODULE DOES
 * ---------------------
 *   1. Spectral peak with parabolic interpolation, on a Hanning-windowed
 *      record — the same method that measured the sample rate to 0.1%.
 *   2. A **sharpness** figure: the fraction of spectral power sitting in the
 *      peak bin and its two neighbours. Clean records score 0.83-1.00, torn
 *      ones 0.54-0.75. The separation is clean and it is what makes a
 *      per-record decision possible at all.
 *   3. If the full record is not sharp, retry on its LAST HALF, which is the
 *      fresher end of a rolling buffer, and keep whichever is better.
 *   4. **Refuse** if the result is still not sharp, or if the peak sits below
 *      bin 6 — where a Hanning mainlobe is not separable from DC and a
 *      quantisation harmonic outranks the fundamental. Three of the four
 *      confidently-wrong answers in the bench set were exactly that.
 *
 * THE THRESHOLDS ARE MEASURED, NOT CHOSEN
 * ----------------------------------------
 * Swept against 72 bench records (two ranges, three timebase codes, five
 * frequencies, seven amplitudes, bench unit #1, 2026-08-19):
 *
 *     floor  min_bin   correct  WRONG  refused   worst error
 *      0.80        2        53      5       14        69.5%
 *      0.85        6        43      1       28         7.5%
 *      0.90        6        39      0       33         3.4%   <-- shipped
 *      0.92        6        32      0       40         3.4%
 *
 * The shipped point answers 54% of the time and is within **3.4%** when it
 * does. Loosening the floor to 0.85 buys four more answers and one 7.5% error;
 * that trade was declined. EXP-13's conclusion stands: dressing a wrong number
 * in real units makes it more convincing, not more correct.
 *
 * A refusal is not a failure. It is the module correctly reporting that this
 * particular record cannot support a frequency, and the caller should hold the
 * previous reading or print nothing.
 *
 * WHY NOT CMSIS-DSP
 * -----------------
 * arm_rfft_fast_f32 is linked into every build and would be faster. This module
 * carries its own radix-2 FFT so that tests/test_scope_freq.c exercises **the
 * code that ships**, on the real bench records, rather than a numpy stand-in
 * agreeing with itself. On this target a 1024-point transform costs well under
 * a millisecond, which is nothing for a badge that updates a few times a
 * second. If that ever stops being true, swap the transform and keep the test.
 */

#ifndef SCOPE_FREQ_H
#define SCOPE_FREQ_H

#include <stdint.h>
#include <stdbool.h>

/* Longest record this module will transform. Must be a power of two. */
#define SCOPE_FREQ_MAX_N        1024u

/* Minimum fraction of spectral power in the peak bin +/- 1 for a record to be
 * believed. Measured, see the sweep above. */
#define SCOPE_FREQ_MIN_SHARP    0.90f

/* Below this bin (in FULL-RECORD terms, so the rule means the same thing
 * whichever window was used) a Hanning mainlobe overlaps DC and a quantisation
 * harmonic can outrank the fundamental. */
#define SCOPE_FREQ_MIN_BIN      6.0f

typedef struct {
    float    hz;         /* 0.0f when the module declines                    */
    float    bin;        /* interpolated peak, in full-record bins           */
    float    sharpness;  /* 0..1, power in peak +/- 1 over total (ex-DC)     */
    uint16_t window;     /* samples actually used: the full n, or n/2        */
} scope_freq_t;

/*
 * Estimate the dominant frequency in a capture record.
 *
 * `samples` is `n` unsigned ADC counts; `n` must be a power of two, at least
 * 64, and at most SCOPE_FREQ_MAX_N. `sample_rate_hz` comes from
 * scope_timebase_sample_rate() and MUST be > 0 — a code with no trustworthy
 * rate has no frequency either, and passing 0 makes this refuse rather than
 * invent one.
 *
 * Returns true and fills `*out` when the record supports a frequency. Returns
 * false otherwise, with `out->hz` set to 0.0f and the diagnostics still filled
 * in so a caller can report WHY it declined.
 */
bool scope_freq_estimate(const uint8_t *samples, uint16_t n,
                         float sample_rate_hz, scope_freq_t *out);

#endif /* SCOPE_FREQ_H */
