/*
 * OpenScope 2C53T — per-channel, per-range vertical calibration
 *
 * WHAT THIS IS
 * ------------
 * One number per (channel, range): how many millivolts at the probe tip one
 * ADC count is worth. Everything vertical the instrument claims — Vpp, Vrms,
 * the volts/div in the status bar, cursor deltas — is that number times a
 * count. There is nothing else in the vertical path.
 *
 * WHERE THE NUMBERS COME FROM
 * ---------------------------
 * docs/experiments/2026-08-17-08-per-range-gain-ladders.md, bench unit #1.
 * Slope method: span measured at five drive amplitudes per range, mVpp
 * regressed against counts, so the constant noise-floor term lands in the
 * intercept instead of inflating the gain. n = 5 on every entry below.
 *
 * WHY THERE ARE TIERS
 * -------------------
 * The measurement is not equally good everywhere, and the previous version of
 * this table (two hardcoded constants in scope_ui.c) is the reason the
 * distinction is now structural rather than a comment. Those two constants
 * claimed 2.882 mV/count on range 2 and 6.494 mV/count on range 8. Range 2
 * rails on both channels and produces no usable span at all, and range 8
 * measures 279 mV/count — a factor of 43 out. They were plausible, stable,
 * and wrong, and they had been on the instrument face for days.
 *
 *   MEASURED     ranges 5/6/7. The two channels agree within 6%, the ladder
 *                is a clean 1-2-4 doubling, and the numbers reproduce BOTH an
 *                independent mux-code sweep on this unit (EXP-06: 21.23 /
 *                42.04 / 91.41) and Stlkv's measurements on a DIFFERENT unit
 *                with a different rig (20 / 40 / 80). Three methods, two
 *                units, one answer.
 *   PROVISIONAL  ranges 4/8/9. Measured the same way, but the two channels
 *                disagree by 25-48%, which is the signature of one fixed
 *                amplitude set under-serving both ends of the sweep: too
 *                small to move range 9, big enough to clip range 4. The
 *                number is the right order of magnitude and nothing more.
 *   NONE         ranges 0-3. Both channels rail. EXP-11 split this into two
 *                different faults: 0/1/2 sit at 255 even with NO input and
 *                after centring, so they are pinned rather than overdriven;
 *                range 3 is a genuine sensitive row (~5.6 mV/count) that
 *                clips because its offset cannot be centred — the injector
 *                runs out of travel. Range 3 is therefore a firmware problem,
 *                not a dead tap, and is the one worth chasing.
 *
 * A PROVISIONAL entry still returns a number, because "roughly 280 mV per
 * count" beats a raw count for a user trying to read a trace. A NONE entry
 * returns 0.0f and the caller MUST fall back to honest ADC counts. That
 * contract is the whole point: a confident wrong number is worse than a raw
 * count, and callers are not allowed to invent one.
 *
 * THE ABSOLUTE SCALE IS ONE MULTIPLY AWAY FROM CORRECT
 * ----------------------------------------------------
 * Every number here traces back to an amplitude *commanded* from the ESP32
 * signal generator, which has never been checked against a calibrated source.
 * Its frequency readback is already known to be off by ~0.82x, so assuming
 * its amplitude is exact would be optimistic. If it turns out to be off by a
 * scale factor, every entry in this table is off by that same factor —
 * uniformly, because they were all taken through the same source on the same
 * evening.
 *
 * That is why SCOPE_CAL_SOURCE_SCALE exists and is applied to every entry at
 * lookup time rather than baked into the table. When a trusted source arrives
 * on the bench, measuring ONE range recovers the factor and correcting it
 * here rescales the whole instrument. Do not "fix" individual rows by hand
 * to make them agree with a reference; that would break the one property
 * that makes this recoverable.
 *
 * WHAT THIS TABLE IS NOT
 * ----------------------
 * - Not per-unit. These are bench unit #1's numbers compiled into the image.
 *   Factory per-device cal exists in MCU flash at 0x08006000 (see
 *   docs/experiments and the factory-cal memory note) and our app overwrites
 *   it. Wiring that up is a separate job.
 * - Not offset. This is gain only. The zero point is set by the per-channel
 *   vertical-offset reference (DAC1/PA4 for CH1, TMR13/PA6 for CH2) and is
 *   re-centered per range by `fpga scope center`.
 * - Not horizontal. There is no sample-rate table here yet. (The claim that
 *   "the capture does not resolve frequency at all" was WITHDRAWN by EXP-10 on
 *   2026-08-18 — it was a double FFT in the measuring script, not a hardware
 *   defect. The time axis is linear and measurable: R^2 = 0.9990 over
 *   100 Hz - 3.5 kHz, fs = 14,890 S/s at reg 0x01 = 0x10.) Seconds need the
 *   same treatment volts just got: measure the rate per reg-0x01 code, tier
 *   it, and refuse to print when there is no entry.
 */

#ifndef SCOPE_CAL_H
#define SCOPE_CAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of frontend range indices. Matches VDIV_COUNT in scope_state.h; the
 * index passed here IS the frontend range index written to the attenuator
 * bank (`fpga_set_ch1_frontend_range`), not a display-only index. */
#define SCOPE_CAL_RANGE_COUNT   10u

/*
 * Counts per screen division, fixed by the renderer, NOT by the hardware.
 *
 * scope_ui.c maps a sample to y as SCOPE_MID_Y - (sample-128)*SCOPE_H/256 and
 * rules a horizontal grid line every 26 px, with SCOPE_H = 206. So one
 * division is 26 * 256/206 = 32.3 counts. 32 is that rounded, and it is also
 * 256/8 — a full ADC span across eight divisions, which is the convention the
 * grid spacing was chosen to produce.
 *
 * scope_ui.c carries a compile-time assertion that its geometry still agrees
 * with this constant, so changing the grid without changing this is a build
 * error rather than a silently wrong volts/div label.
 */
#define SCOPE_CAL_COUNTS_PER_DIV   32.0f

/*
 * Correction for the calibration source's true-vs-commanded amplitude.
 * 1.0f = "we are taking the ESP32 siggen at its word", which is the current
 * state and is NOT a verified claim. See the header comment above before
 * changing this, and change ONLY this rather than individual table rows.
 */
#define SCOPE_CAL_SOURCE_SCALE     1.0f

typedef enum {
    SCOPE_CAL_NONE = 0,     /* unusable range — caller must show ADC counts */
    SCOPE_CAL_PROVISIONAL,  /* right order of magnitude, channels disagree   */
    SCOPE_CAL_MEASURED,     /* cross-validated three ways on two units       */
} scope_cal_tier_t;

/*
 * Millivolts per ADC count for one channel on one range, source-scale
 * applied. `ch` is 1 or 2. Returns 0.0f for an unusable range or an
 * out-of-domain argument — callers MUST treat 0.0f as "no calibration" and
 * fall back to counts.
 */
float scope_cal_mv_per_count(uint8_t ch, uint8_t range_idx);

/* Volts per ADC count — the form the measurement badges want. 0.0f as above. */
float scope_cal_volts_per_count(uint8_t ch, uint8_t range_idx);

/* How much to trust the above. */
scope_cal_tier_t scope_cal_get_tier(uint8_t ch, uint8_t range_idx);

/*
 * Volts per screen division, derived from the measured gain and the
 * renderer's counts-per-division. This is the number the status bar should
 * show, because it is the number the grid actually means. Returns 0.0f when
 * uncalibrated.
 */
float scope_cal_volts_per_div(uint8_t ch, uint8_t range_idx);

/*
 * Status-bar label for a range, e.g. "1.37V", "699mV", "~8.9V" (provisional,
 * leading tilde) or "--" (uncalibrated). Always NUL-terminated. Writes at
 * most `n` bytes; 8 is enough for every entry.
 *
 * Deriving the label from the measurement is deliberate. The nominal
 * vdiv_table labels ("5mV" ... "5V") were never derived from anything: on the
 * three ranges we trust they are out by 2.7x to 3.5x, so a range labelled
 * "2V" was showing a 2.8 V division. A label the instrument cannot support is
 * the same class of defect as an invented measurement.
 */
void scope_cal_range_label(uint8_t ch, uint8_t range_idx, char *out, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* SCOPE_CAL_H */
