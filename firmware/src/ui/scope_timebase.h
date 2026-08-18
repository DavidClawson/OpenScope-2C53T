/*
 * OpenScope 2C53T — horizontal (time) calibration
 *
 * WHAT THIS IS
 * ------------
 * One number per reg-0x01 timebase code: the capture sample rate in samples
 * per second. Everything horizontal the instrument could claim — seconds per
 * division, frequency, period, rise time — is derived from that rate and a
 * sample count. There is nothing else in the horizontal path.
 *
 * This is the exact counterpart of scope_cal.h for the vertical axis, and it
 * is deliberately built the same way, including the tiers and the refusal to
 * print when there is no entry.
 *
 * WHERE THE NUMBERS COME FROM
 * ---------------------------
 * docs/experiments/2026-08-18-10-time-axis.md (the ladder) and
 * docs/experiments/2026-08-18-12-code-08-incoherent.md (what a rate must
 * survive before it is believed), bench unit #1.
 *
 * Method: drive tones of known frequency, find the spectral peak with a
 * SEARCH, and fit peak-bin against frequency through the origin —
 * bin = f * 1024/fs, so fs = 1024/slope.
 *
 * AN R^2 IS NOT ENOUGH, AND THAT IS NOT A THEORETICAL WORRY
 * ---------------------------------------------------------
 * At code 0x08 a 14-tone in-band sweep returned **R^2 = 0.9304** — which reads
 * as a decent fit — on a record that does not reproduce itself between reads.
 * Three consecutive reads of one unchanged tone gave peak bins [171, 132, 104],
 * and two passes of the identical sweep disagreed by 7.6%.
 *
 * What exposed it is the FOLD test: above Nyquist a tone aliases to
 * |f - round(f/fs)*fs|, a bin position far more sensitive to fs than the
 * in-band slope is. A line through noise that happens to trend passes the R^2
 * test and fails this one. At 0x08 the fitted rate missed its own fold
 * predictions by up to 227 bins; at 0x10, where fs is independently known, the
 * same test missed by at most 8.
 *
 * So a rate reaches MEASURED here only if it has survived the fold test, and
 * `scripts/measure_sample_rate.py` runs that check as part of the measurement.
 *
 * THE TIERS
 * ---------
 *   MEASURED     0x0E / 0x0F / 0x10. In-band R^2 of 0.98 or better, reads
 *                that reproduce exactly between passes. 0x10 is confirmed
 *                FIVE independent ways (four in-band fits across two read
 *                paths, plus the fold check).
 *   PROVISIONAL  0x0D. R^2 0.904 — the tone set is getting short relative to
 *                the rate, so the bins are small and quantisation dominates.
 *                Right order of magnitude, no better.
 *   NONE         everything else, for two DIFFERENT reasons that must not be
 *                conflated:
 *                  - 0x0A / 0x0B / 0x0C: not measurable with our current
 *                    source. It tops out near 4.5 kHz, which at 300 kS/s is
 *                    bin 15 and at 600 kS/s is bin 7, so the fit measures
 *                    quantisation. The device is probably fine here; our
 *                    instrument is not. A faster generator settles it.
 *                  - 0x08: measured and found INCOHERENT. Reads do not
 *                    reproduce. This is a statement about the device.
 *                  - all other codes: never measured at all.
 *
 * A NONE entry returns 0.0f and callers MUST fall back to samples — the same
 * contract scope_cal.h imposes for counts. Printing a frequency derived from
 * an assumed rate is exactly the invention this module exists to prevent.
 *
 * SECONDS PER DIVISION IS A PROPERTY OF OUR RENDERER, NOT OF THE HARDWARE
 * -----------------------------------------------------------------------
 * scope_ui.c draws one sample per screen column and rules a vertical grid line
 * every 32 px, so a division is 32 samples and s/div = 32/fs. That is what our
 * grid means, and the label must say so.
 *
 * Worth recording, because it is a genuinely different situation from the
 * volts case: the nominal `timebase_table` labels ("5ns" ... "20ms") are a
 * CONSTANT 2.12-2.15x away from that on every code we have measured, and at
 * 16 samples per division they land within 6-8%. Unlike `vdiv_table` — which
 * was 2.7-3.5x out with no consistent factor and was simply invented — this
 * table appears to be real, and derived from a design that draws 16 samples
 * per division. That is evidence about STOCK's geometry, not a defect in ours;
 * 10 divisions across 320 px with one sample per column is a perfectly
 * ordinary layout. If the renderer's geometry is ever changed, change
 * SCOPE_TIMEBASE_SAMPLES_PER_DIV with it and the labels follow.
 */

#ifndef SCOPE_TIMEBASE_H
#define SCOPE_TIMEBASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Timebase codes are the UI's timebase_idx written straight to SPI reg 0x01
 * (see fpga_stock_timebase_byte()), so this must match TIMEBASE_COUNT. */
#define SCOPE_TIMEBASE_CODE_COUNT   21u

/*
 * Samples per screen division — set by the renderer, not the hardware.
 * scope_ui.c: one sample per column, vertical grid every 32 px.
 * scope_ui.c carries a compile-time assertion tying this to its geometry.
 */
#define SCOPE_TIMEBASE_SAMPLES_PER_DIV   32.0f

typedef enum {
    SCOPE_TB_NONE = 0,      /* no rate — caller must fall back to samples */
    SCOPE_TB_PROVISIONAL,   /* right order of magnitude, quantisation-limited */
    SCOPE_TB_MEASURED,      /* fitted AND fold-checked */
} scope_tb_tier_t;

/* Capture sample rate in samples/second, or 0.0f when unknown. */
float scope_timebase_sample_rate(uint8_t code);

/* How much to trust it. */
scope_tb_tier_t scope_timebase_get_tier(uint8_t code);

/* Seconds between adjacent samples, or 0.0f when unknown. */
float scope_timebase_seconds_per_sample(uint8_t code);

/* Seconds per screen division, derived from the rate and the renderer's
 * samples-per-division. 0.0f when unknown. */
float scope_timebase_seconds_per_div(uint8_t code);

/*
 * Status-bar label, e.g. "2.15ms", "~213us" (provisional, leading tilde) or
 * "--" (no rate). Always NUL-terminated; 10 bytes is enough for every entry.
 */
void scope_timebase_label(uint8_t code, char *out, uint32_t n);

/*
 * Hz from a measured period in SAMPLES, or 0.0f when the code has no rate or
 * the period is not positive. The caller is expected to print samples instead
 * when this returns 0.
 */
float scope_timebase_hz_from_period(uint8_t code, float period_samples);

#ifdef __cplusplus
}
#endif

#endif /* SCOPE_TIMEBASE_H */
