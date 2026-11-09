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
/*
 * REPLACED 2026-11-09 (EXP-74, docs/experiments/2026-11-09-74-stock-fast-timebase-path.md).
 *
 * Every code 0x00-0x14 is now fold-tested against a CRYSTAL-LOCKED source: a
 * divider off the PlutoSky PS IO PLL (FCLK0 = 100 MHz) driven onto the JP5
 * header, tones 50 MHz / k, so every drive frequency is known to the board
 * crystal's ppm and nothing about it is commanded-but-unverified (the EXP-14
 * failure mode). Per code: 3-10 tones from 0.1 fs to 2.6 fs, three records
 * each, Hann + 16x zero-pad + parabolic peak, then fs fitted by minimising the
 * ALIAS error over +-30 % of the candidate. Result on every code 0x04-0x14:
 * fit equal to the round value within the grid step (1e-5 relative), worst
 * per-tone miss 0.02 bins of 1024, read-to-read spread 0.00-0.02 bins.
 *
 * So the round ladder IS the measurement now. The earlier rule ("do not write
 * the round numbers, they are expectations") was right for an uncalibrated
 * ESP32 source; it is retired for these rows because the source is no longer
 * the weak link. The superseded values (EXP-17/18: 49930.1, 24979.1, 12490.0,
 * 123662.7, 4990.8, 2494.9, 1250.4, 500.2) were within 0.2 % except 0x0D (-1.07 %),
 * which matches the quantisation limits stated for them.
 *
 * The fast codes were never INCOHERENT. Their records looked like it because
 * they were read through `spi3 opread`, whose ~17.5 ms /256 burst laps a
 * 205 us (0x08) record ~170 times (see the EXP-12 resolution note). Read
 * through the acquisition task (the remote-protocol waveform), every fast code
 * gives coherent, reproducible records. The ladder is the one in stock's own
 * per-index table at 0x080465E0, times 0.4 (EXP-74 sec. 2.2).
 *
 * 0x00-0x03: stock treats these as 250 MS/s (single channel, the two ADC
 * halves interleaved). On our configuration they measure 125 MS/s on CH1, the
 * same as 0x04: the op-0x05 record carries CH2, not a second phase of CH1. The
 * rate entered is what the hardware delivers to us, not stock's figure.
 */
static const float sample_rate[SCOPE_TIMEBASE_CODE_COUNT] = {
    /* 0x00 */  125000000.0f,   /* EXP-74, same as 0x04 here (stock: 250M)    */
    /* 0x01 */  125000000.0f,   /* EXP-74, ditto                              */
    /* 0x02 */  125000000.0f,   /* EXP-74, ditto                              */
    /* 0x03 */  125000000.0f,   /* EXP-74, ditto                              */
    /* 0x04 */  125000000.0f,   /* EXP-74, in-band tones 12.5/25/50 MHz       */
    /* 0x05 */   50000000.0f,   /* EXP-74 fold fit                            */
    /* 0x06 */   25000000.0f,   /* EXP-74 fold fit                            */
    /* 0x07 */   12500000.0f,   /* EXP-74 fold fit                            */
    /* 0x08 */    5000000.0f,   /* EXP-74 fold fit (= Stlkv, EXP-12 note)     */
    /* 0x09 */    2500000.0f,   /* EXP-74 fold fit                            */
    /* 0x0A */    1250000.0f,   /* EXP-74 fold fit                            */
    /* 0x0B */     500000.0f,   /* EXP-74 fold fit                            */
    /* 0x0C */     250000.0f,   /* EXP-74 fold fit                            */
    /* 0x0D */     125000.0f,   /* EXP-74 fold fit                            */
    /* 0x0E */      50000.0f,   /* EXP-74 fold fit                            */
    /* 0x0F */      25000.0f,   /* EXP-74 fold fit                            */
    /* 0x10 */      12500.0f,   /* EXP-74 fold fit                            */
    /* 0x11 */       5000.0f,   /* EXP-74 fold fit                            */
    /* 0x12 */       2500.0f,   /* EXP-74 fold fit                            */
    /* 0x13 */       1250.0f,   /* EXP-74 fold fit                            */
    /* 0x14 */        500.0f,   /* EXP-74 fold fit                            */
};

/*
 * REVISED 2026-08-19 (EXP-17). The three measured rates moved by +1.78% /
 * -2.94% / -0.68%. The revision came out of validating the frequency
 * estimator, not out of looking for it: applied to a KNOWN drive, the
 * estimator's error was not scattered, it was constant within each timebase
 * code and different BETWEEN codes.
 *
 * That pattern is what assigns blame. A source-scale error -- the EXP-14 bug,
 * where the generator delivered 0.825x what it was told -- multiplies every
 * commanded frequency by one factor, so it moves every code the same
 * direction by the same amount. Here 0x0E needed +1.8% and 0x0F needed -2.9%,
 * opposite signs, which no single source factor can produce. And within one
 * code the error held constant from bin 6.8 to bin 246, a 36:1 span, so it is
 * not an interpolation artifact either (that varies with fractional bin).
 *
 * Method: 66 records (two ranges, five drive frequencies, two reps), peak
 * located by the shipped estimator, then fs from a least-squares fit of bin
 * against drive frequency. The fit is deliberately free-intercept rather than
 * through the origin, because the intercept turns out to be real: -0.045 bins,
 * the SAME on all three codes, a small fixed bias in parabolic interpolation
 * over a Hanning magnitude spectrum. It is worth only 0.03-0.11% here, but
 * fitting through the origin would have folded it into the slope.
 *
 * Four checks, all independent of the fit itself:
 *   - two DISJOINT drive sets (100/250/500/1000/2000 and 150/330/700/1500/
 *     3000 Hz) agree to 0.00%, 0.03% and 0.24%;
 *   - all three land within 0.14% of the round 1-2-5 ladder 12.5k/25k/50k;
 *   - averaging per-point ratios instead of fitting agrees to 0.11%;
 *   - 0x10 against Stlkv's independent rig (12,437 S/s, different unit,
 *     generator, firmware and method) improves from +1.11% to +0.43%.
 *
 * The last one is the one that matters: it is out-of-sample. Nothing in this
 * fit knows about his number, and the correction moved toward it.
 *
 * The round ladder is STILL NOT WRITTEN HERE, for the reason it was not
 * written in EXP-14: 12,490 is what was measured and 12,500 is what was
 * expected, and a table that records expectations is not evidence. The
 * agreement is now close enough that the ladder is almost certainly the
 * design intent -- but "almost certainly" belongs in a comment, not in a
 * float that later work will treat as data.
 *
 * Why the first ladder was wrong is NOT established. The leading candidate is
 * that it was fitted partly on torn records: it swept through `spi3 opread`,
 * whose ~17.5 ms /256 burst lets the engine lap the buffer, and which still
 * tears 6 reads in 12 (EXP-16). This fit used `spi3 read`, which after the
 * 2026-08-19 snapshot fix tears 0 in 12. That is a plausible mechanism and
 * nothing more; it has not been tested by re-fitting the old data.
 *
 * 0x0D (119,678 S/s) was NOT re-measured -- the bench source cannot reach far
 * enough above it to place a peak usefully. It very likely carries a similar
 * error and stays PROVISIONAL.
 */

/*
 * EXTENDED 2026-08-19 (EXP-18). The calibrated ladder went from 4 codes to 8.
 *
 * Codes 0x11-0x14 had never been swept. Every earlier session worked downward
 * from 0x10 and treated the unmeasured codes as a block, but the ones ABOVE
 * 0x10 are SLOWER, which makes them EASIER to measure than anything already
 * done -- more cycles per record, higher bins, better resolved. They were not
 * hard. Nobody had looked.
 *
 *   0x11  4,990.8   R2 1.0000
 *   0x12  2,494.9   R2 1.0000, fold-tested
 *   0x13  1,250.4   R2 1.0000
 *   0x14    500.2   R2 1.0000
 *
 * THE LADDER IS 1-2.5-5, NOT UNIFORM x2. Predicted 6,250 for 0x11 by assuming
 * each step doubles; measured 4,990.8. The real sequence is
 * 500 / 1250 / 2500 / 5000 / 12500 / 25000 / 50000 / 125000 -- ratios
 * 2.5, 2, 2, 2.5, 2, 2, 2.5. Every measured code lands within 0.2% of it,
 * except 0x0D (see below). Prediction beaten by measurement, again, and in
 * the direction that matters: the wrong prediction was the round-looking one.
 *
 * FOLD TEST at 0x12 (Nyquist 1,247 Hz): five tones from 1.4 to 3.0 kHz land
 * where 2,494.9 S/s says they alias to, worst miss 2.4 bins of 512. Aliasing
 * depends only on f/fs, so this is scale-invariant -- it tests the rate, not
 * the arithmetic that produced it.
 *
 * 0x0D RE-MEASURED, and it is the first evidence for EXP-17's untested
 * mechanism. The old 119,678 came from a sweep through `spi3 opread`, whose
 * slow /256 burst lets the engine lap the buffer (6 reads in 12 torn). Read
 * instead through `spi3 read` -- 0 in 12 torn since the snapshot fix -- the
 * same code gives 123,662.7 with R2 going 0.947 -> 0.9997, and its distance
 * from the round 125,000 drops from -4.26% to -1.07%. EXP-17 could only offer
 * torn records as a plausible story; this is a controlled-ish test of it (same
 * code, same rig, different read path) and it went the predicted way.
 *
 * It stays PROVISIONAL regardless: at ~124 kS/s the bench source only reaches
 * bin 25, so this is the least-resolved code in the table and -1.07% is a
 * bigger miss than every other code by a factor of five.
 *
 * NOT CHASED, worth recording: the INCOHERENT band 0x06-0x09 fitted to
 * 1,231 / 1,595 / 1,350 / 1,550 S/s -- all close to 0x13's 1,250. If the rate
 * field is narrower than the codes we write, or those codes select roll mode
 * rather than a rate, that would explain both the clustering and why the
 * numbers do not reproduce. Untested.
 */

/*
 * Why each code is where it is. Three distinct kinds of "no":
 *
 *   0x06-0x09     INCOHERENT. Measured, and the records do not reproduce.
 *                 EXP-15 (2026-08-19) widened this from 0x08 alone: fitting all
 *                 four codes gives 1,231 / 1,595 / 1,350 / 1,550 S/s — a 30%
 *                 band — where a real ladder step is 2-2.5x, and 40 Hz lands on
 *                 bin 28 in ALL FOUR. Read-to-read spread is 144-309 bins on a
 *                 512-bin spectrum. A number that does not change when the code
 *                 changes is not the device's rate; it is ours. Stlkv predicted
 *                 exactly this on issue #18 before it was run.
 *                 (His proposed MECHANISM — the fit returns our read cadence —
 *                 is NOT established: a round-trip opread takes ~800 ms, but
 *                 that is dominated by the hex dump over serial, and the SPI
 *                 burst at /256 is ~17.5 ms, which would predict ~58 kS/s.
 *                 Prediction confirmed, mechanism open.)
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
    /* All MEASURED since EXP-74 (2026-11-09). The commentary above this table
     * is history: what the tiers USED to be and why. EXP-74 superseded every
     * NONE and PROVISIONAL in it. */
    SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED,
    SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED,
    SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED,
    SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED,
    SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED, SCOPE_TB_MEASURED,
    SCOPE_TB_MEASURED,
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
    /* Sub-microsecond and low-microsecond steps exist since EXP-74 (0x04 is
     * 256 ns/div), so the microsecond range keeps significant digits. */
    if (s < 1e-6f) {
        snprintf(out, n, "%s%uns", mark, (unsigned)(s * 1e9f + 0.5f));
    } else if (s < 1e-5f) {
        const unsigned c = (unsigned)(s * 1e8f + 0.5f);
        snprintf(out, n, "%s%u.%02uus", mark, c / 100u, c % 100u);
    } else if (s < 1e-4f) {
        const unsigned d = (unsigned)(s * 1e7f + 0.5f);
        snprintf(out, n, "%s%u.%uus", mark, d / 10u, d % 10u);
    } else if (s < 1e-3f) {
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
