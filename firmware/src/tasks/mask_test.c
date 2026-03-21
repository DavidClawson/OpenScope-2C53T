/*
 * FNIRSI 2C53T Mask (Pass/Fail) Testing
 *
 * Compares live waveforms against upper/lower boundaries. A reference
 * waveform defines the expected shape; tolerance defines the mask width.
 */

#include "mask_test.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Minimum mask margin in sample units, prevents zero-width mask at
 * zero-crossings where the reference signal amplitude is near zero. */
#define MASK_MIN_MARGIN 100

/* ========================================================================
 * mask_create_from_waveform — Build mask from a reference waveform
 *
 * For each sample mapped to a display column:
 *   upper = reference + max(|reference| * tolerance/100, MIN_MARGIN)
 *   lower = reference - max(|reference| * tolerance/100, MIN_MARGIN)
 * ======================================================================== */
void mask_create_from_waveform(mask_state_t *mask, const int16_t *reference,
                               uint16_t num_samples, float tolerance_pct)
{
    memset(mask, 0, sizeof(mask_state_t));
    mask->tolerance_pct = tolerance_pct;
    mask->enabled = true;

    uint16_t cols = num_samples;
    if (cols > MASK_WIDTH) {
        cols = MASK_WIDTH;
    }

    for (uint16_t i = 0; i < cols; i++) {
        /* Map sample index to display column */
        uint16_t col;
        if (cols == num_samples) {
            col = i;
        } else {
            col = (uint16_t)((uint32_t)i * MASK_WIDTH / num_samples);
        }

        if (col >= MASK_WIDTH) col = MASK_WIDTH - 1;

        int16_t ref = reference[i];
        int16_t margin = (int16_t)(abs(ref) * tolerance_pct / 100.0f);
        if (margin < MASK_MIN_MARGIN) {
            margin = MASK_MIN_MARGIN;
        }

        int32_t upper = (int32_t)ref + margin;
        int32_t lower = (int32_t)ref - margin;

        /* Clamp to int16_t range */
        if (upper > 32767) upper = 32767;
        if (lower < -32768) lower = -32768;

        /* If multiple samples map to the same column, widen the mask */
        if (mask->defined[col]) {
            if ((int16_t)upper > mask->upper[col]) mask->upper[col] = (int16_t)upper;
            if ((int16_t)lower < mask->lower[col]) mask->lower[col] = (int16_t)lower;
        } else {
            mask->upper[col] = (int16_t)upper;
            mask->lower[col] = (int16_t)lower;
            mask->defined[col] = true;
        }
    }
}

/* ========================================================================
 * mask_test — Test a waveform against the mask boundaries
 *
 * Returns true (PASS) if all samples are within bounds.
 * ======================================================================== */
bool mask_test(mask_state_t *mask, const int16_t *samples, uint16_t num_samples)
{
    if (!mask->enabled) {
        return true;
    }

    bool pass = true;

    for (uint16_t i = 0; i < num_samples; i++) {
        /* Map sample index to display column */
        uint16_t col;
        if (num_samples <= MASK_WIDTH) {
            col = i;
        } else {
            col = (uint16_t)((uint32_t)i * MASK_WIDTH / num_samples);
        }

        if (col >= MASK_WIDTH) col = MASK_WIDTH - 1;

        if (!mask->defined[col]) {
            continue;
        }

        if (samples[i] > mask->upper[col] || samples[i] < mask->lower[col]) {
            pass = false;
            break;
        }
    }

    mask->total_tests++;
    if (pass) {
        mask->pass_count++;
    } else {
        mask->fail_count++;
    }

    return pass;
}

/* ========================================================================
 * mask_reset_counts — Reset pass/fail counters
 * ======================================================================== */
void mask_reset_counts(mask_state_t *mask)
{
    mask->total_tests = 0;
    mask->pass_count = 0;
    mask->fail_count = 0;
}

/* ========================================================================
 * mask_clear — Clear all mask data
 * ======================================================================== */
void mask_clear(mask_state_t *mask)
{
    memset(mask->upper, 0, sizeof(mask->upper));
    memset(mask->lower, 0, sizeof(mask->lower));
    memset(mask->defined, 0, sizeof(mask->defined));
    mask->enabled = false;
    mask->total_tests = 0;
    mask->pass_count = 0;
    mask->fail_count = 0;
}

/* ========================================================================
 * mask_pass_rate — Get pass rate as percentage
 * ======================================================================== */
float mask_pass_rate(const mask_state_t *mask)
{
    if (mask->total_tests == 0) {
        return 0.0f;
    }
    return (float)mask->pass_count / (float)mask->total_tests * 100.0f;
}

/* ========================================================================
 * Rendering — Mask boundaries on display
 * ======================================================================== */

#ifndef TEST_BUILD

#include "../drivers/lcd.h"

void mask_render(const mask_state_t *mask,
                 uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                 int16_t y_min, int16_t y_max,
                 uint16_t pass_color, uint16_t fail_color)
{
    if (!mask->enabled) return;

    float y_range = (float)(y_max - y_min);
    if (y_range <= 0.0f) return;

    for (uint16_t col = 0; col < width && col < MASK_WIDTH; col++) {
        if (!mask->defined[col]) continue;

        uint16_t px = x_off + col;

        /* Map upper boundary to pixel Y (inverted: higher value = lower Y) */
        float upper_norm = (float)(mask->upper[col] - y_min) / y_range;
        float lower_norm = (float)(mask->lower[col] - y_min) / y_range;

        int16_t upper_py = (int16_t)(y_off + height - 1 - upper_norm * (height - 1));
        int16_t lower_py = (int16_t)(y_off + height - 1 - lower_norm * (height - 1));

        /* Clamp to drawing area */
        if (upper_py < (int16_t)y_off) upper_py = (int16_t)y_off;
        if (lower_py >= (int16_t)(y_off + height)) lower_py = (int16_t)(y_off + height - 1);

        /* Draw upper boundary (dashed: every other pair of pixels) */
        if ((col & 3) < 2) {
            if (upper_py >= (int16_t)y_off && upper_py < (int16_t)(y_off + height)) {
                lcd_set_pixel(px, (uint16_t)upper_py, pass_color);
            }
            if (lower_py >= (int16_t)y_off && lower_py < (int16_t)(y_off + height)) {
                lcd_set_pixel(px, (uint16_t)lower_py, pass_color);
            }
        }

        /* Fill forbidden zones (outside mask) with fail_color */
        /* Above upper boundary */
        for (int16_t yy = (int16_t)y_off; yy < upper_py; yy++) {
            /* Every other pixel for translucent effect */
            if ((px + yy) & 1) {
                lcd_set_pixel(px, (uint16_t)yy, fail_color);
            }
        }
        /* Below lower boundary */
        for (int16_t yy = lower_py + 1; yy < (int16_t)(y_off + height); yy++) {
            if ((px + yy) & 1) {
                lcd_set_pixel(px, (uint16_t)yy, fail_color);
            }
        }
    }
}

#else /* TEST_BUILD */

void mask_render(const mask_state_t *mask,
                 uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                 int16_t y_min, int16_t y_max,
                 uint16_t pass_color, uint16_t fail_color)
{
    (void)mask; (void)x_off; (void)y_off; (void)width; (void)height;
    (void)y_min; (void)y_max; (void)pass_color; (void)fail_color;
}

#endif /* TEST_BUILD */
