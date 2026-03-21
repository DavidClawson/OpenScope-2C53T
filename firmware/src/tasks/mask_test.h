/*
 * FNIRSI 2C53T Mask (Pass/Fail) Testing
 *
 * Compares live waveforms against upper/lower boundaries defined by
 * a reference waveform plus tolerance. Used for production testing
 * and compliance verification.
 */

#ifndef MASK_TEST_H
#define MASK_TEST_H

#include <stdint.h>
#include <stdbool.h>

#define MASK_WIDTH 320       /* One boundary point per pixel column */

typedef struct {
    int16_t upper[MASK_WIDTH];  /* Upper boundary (sample units) */
    int16_t lower[MASK_WIDTH];  /* Lower boundary (sample units) */
    bool    defined[MASK_WIDTH]; /* Whether this column has a boundary */
    float   tolerance_pct;       /* How much margin around the mask (%) */
    uint32_t total_tests;        /* Number of waveforms tested */
    uint32_t pass_count;
    uint32_t fail_count;
    bool     enabled;
} mask_state_t;

/* Initialize mask from a "known good" waveform with tolerance */
void mask_create_from_waveform(mask_state_t *mask, const int16_t *reference,
                               uint16_t num_samples, float tolerance_pct);

/* Test a waveform against the mask. Returns true if PASS (within bounds). */
bool mask_test(mask_state_t *mask, const int16_t *samples, uint16_t num_samples);

/* Reset pass/fail counters */
void mask_reset_counts(mask_state_t *mask);

/* Clear mask */
void mask_clear(mask_state_t *mask);

/* Get pass rate as percentage */
float mask_pass_rate(const mask_state_t *mask);

/* Render mask boundaries on display */
void mask_render(const mask_state_t *mask,
                 uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                 int16_t y_min, int16_t y_max,
                 uint16_t pass_color, uint16_t fail_color);

#endif /* MASK_TEST_H */
