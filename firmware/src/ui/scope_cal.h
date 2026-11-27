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
 * Two units since 2026-11-27, combined by the rule in
 * docs/calibration/scope_gain.md (which lists every unit's numbers):
 *   - s00500's unit: ranges 0-9, both channels, 4-9-level slope fits against
 *     the 2C53T's own generator, whose scale was set by a Rigol DS1104Z after
 *     Self-Cal (absolute to about +/-1-2 %).
 *   - sparsematrix's unit (#95): ranges 4-9, both channels, one level per
 *     point against a 4-digit DMM on the same node (about +/-0.5 %).
 * Ranges 4-7 are the mean of the two (they agree within 2.6 %), ranges 0-3
 * are s00500's, ranges 8-9 sparsematrix's (s00500's fits there span too few
 * counts to use). Both channels agree within 1.8 % on every range, and every
 * step is within 2 % of the 1-2-5 attenuator ladder (x2, x2.5, x2, ...).
 *
 * Until 2026-11-27 the table was bench unit #1's (2026-08-17, EXP-08, slope
 * method at five amplitudes), scaled by 0.92 against a JDS6600; withdrawn in
 * scope_cal.c, with how far each row was off.
 *
 * WHY THERE ARE TIERS
 * -------------------
 * The measurement is not equally good everywhere, and the version of this
 * table before unit #1's (two hardcoded constants in scope_ui.c) is the reason
 * the distinction is structural rather than a comment. Those two constants
 * claimed 2.882 mV/count on range 2 and 6.494 mV/count on range 8. Range 8
 * measures about 195 mV/count, a factor of 30 out. They were plausible,
 * stable, and wrong, and they had been on the instrument face for days.
 *
 *   MEASURED     ranges 0-7. Ranges 4-7: two units within 2.6 %. Ranges 0-3:
 *                one unit, 7-9-level fits with residuals under one count,
 *                both channels within 1.3 %.
 *   PROVISIONAL  ranges 8-9. One unit at usable resolution. The label carries
 *                a leading '~'.
 *   NONE         no range at present. Kept for out-of-domain arguments and
 *                for any future range without a number.
 *
 * A PROVISIONAL entry still returns a number, because "roughly 195 mV per
 * count" beats a raw count for a user trying to read a trace. A NONE entry
 * returns 0.0f and the caller MUST fall back to honest ADC counts. That
 * contract is the whole point: a confident wrong number is worse than a raw
 * count, and callers are not allowed to invent one.
 *
 * ONE SCALE FOR EVERY ENTRY
 * -------------------------
 * SCOPE_CAL_SOURCE_SCALE multiplies every entry at lookup time. Unit #1's
 * table needed 0.92 (its generator over-read by about 8 %); the current
 * table comes from absolute references and needs 1.0. The knob stays so that
 * a correction common to the whole table remains one constant. Do not "fix"
 * individual rows by hand to make them agree with a reference: add the
 * reference's unit to docs/calibration/scope_gain.md and re-derive.
 *
 * WHAT THIS TABLE IS NOT
 * ----------------------
 * - Not per-unit. These are numbers shared by every image, from the units in
 *   docs/calibration/scope_gain.md.
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
 * Correction for the calibration source's true-vs-commanded amplitude,
 * applied to every entry.
 *
 * 1.0f since 2026-11-27: the table comes from two absolutely referenced
 * units (a self-calibrated Rigol behind one, a DMM behind the other; see
 * docs/calibration/scope_gain.md), so there is nothing to correct.
 *
 * WITHDRAWN 2026-11-27: 0.92f, which corrected bench unit #1's table. That
 * table was taken through the ESP32 siggen at commanded amplitudes, which
 * over-read gain about 8 %. Measured against a JDS6600 (~1-2 % amplitude
 * accuracy): EXP-20 (2026-08-21) CH1 r6 0.919, r7 0.884, DC r7 0.874;
 * EXP-21 (2026-08-28) one JDS channel into both inputs, raw r6 slopes CH1
 * 25.2 / CH2 25.3 counts per Vpp, per-channel scale r6 0.924 / 0.948,
 * r5 0.943 / 0.966. Against the two units now in the table, unit #1's rows
 * times 0.92 were within 3.6 % on ranges 5-7: the correction was right.
 */
#define SCOPE_CAL_SOURCE_SCALE     1.0f

typedef enum {
    SCOPE_CAL_NONE = 0,     /* unusable range — caller must show ADC counts */
    SCOPE_CAL_PROVISIONAL,  /* one unit at usable resolution: read with '~' */
    SCOPE_CAL_MEASURED,     /* two units agree, or one unit's multi-level fit */
} scope_cal_tier_t;

/*
 * Millivolts per ADC count for one channel on one range, source-scale
 * applied. `ch` is 1 or 2. Returns 0.0f for an unusable range or an
 * out-of-domain argument — callers MUST treat 0.0f as "no calibration" and
 * fall back to counts.
 */
float scope_cal_mv_per_count(uint8_t ch, uint8_t range_idx);

/* Volts per ADC count AT THE PROBE TIP — the form the badges, cursor and
 * volts/div labels want. = mV/count x the channel's probe factor / 1000.
 * 0.0f as above. scope_cal_mv_per_count() above stays BNC-referred: it is the
 * calibration table (what `fpga scope cal` prints and the bench scripts
 * verify), and a probe setting must never look like a calibration change. */
float scope_cal_volts_per_count(uint8_t ch, uint8_t range_idx);

/* Probe attenuation, read through a registered source so the probe setting
 * has ONE home (scope_state) and ONE application point (here). Until
 * 2026-10-03 the 1X/10X setting reached nothing but an unobeyed USART bit:
 * a user on a 10x probe saw volts 10x too small. No source registered (host
 * tests, early boot) or a factor <= 0 means 1.0. */
typedef float (*scope_cal_probe_fn)(uint8_t ch);
void  scope_cal_set_probe_source(scope_cal_probe_fn fn);
float scope_cal_probe_factor(uint8_t ch);

/* How much to trust the above. */
scope_cal_tier_t scope_cal_get_tier(uint8_t ch, uint8_t range_idx);

/*
 * May the renderer draw this (channel, range) at TRUE vertical scale — i.e.
 * make one grid division mean exactly scope_cal_volts_per_div()?
 *
 * True-scale rendering fixes the sample->pixel slope at SCOPE_CAL_COUNTS_PER_DIV
 * counts per division; that is only honest when a volts/div actually exists for
 * the range. This returns nonzero exactly when it does (MEASURED or
 * PROVISIONAL). On a NONE range it returns 0 and the renderer MUST fall back to
 * its own-min/max autofit, whose graticule has no volts meaning — the same
 * counts-vs-volts contract every other consumer of this module honours.
 */
int scope_cal_true_scale_ok(uint8_t ch, uint8_t range_idx);

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

/* The same label at the BNC, ignoring the probe factor: for the calibration
 * dump (`fpga scope cal`), whose table must not change with a UI setting
 * (EXP-70 addendum: the first probe build moved that column). */
void scope_cal_range_label_bnc(uint8_t ch, uint8_t range_idx, char *out, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* SCOPE_CAL_H */
