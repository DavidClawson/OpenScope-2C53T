/*
 * Relative Compression Test — Automotive Diagnostic Module
 *
 * Analyzes starter motor cranking current waveform to detect weak cylinders.
 * During cranking, each cylinder's compression stroke causes a distinct current
 * peak. A weak cylinder (bad rings, valve, head gasket) draws noticeably less
 * current. The pattern repeats every 720 degrees (2 engine revolutions) for
 * 4-stroke engines.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "compression_test.h"
#include <string.h>
#include <math.h>

/* Module-level configuration */
static compression_config_t s_cfg;

void compression_init(const compression_config_t *cfg)
{
    if (!cfg || cfg->num_cylinders == 0 || cfg->num_cylinders > COMP_MAX_CYLINDERS) {
        /* Default: 4-cylinder, 85% threshold, average 3 cycles */
        s_cfg.num_cylinders    = 4;
        s_cfg.firing_order[0]  = 1;
        s_cfg.firing_order[1]  = 3;
        s_cfg.firing_order[2]  = 4;
        s_cfg.firing_order[3]  = 2;
        s_cfg.pass_threshold_pct = 85.0f;
        s_cfg.cranks_to_average  = 3;
    } else {
        memcpy(&s_cfg, cfg, sizeof(s_cfg));
    }
}

const compression_config_t *compression_get_config(void)
{
    return &s_cfg;
}

uint16_t compression_find_peaks(const int16_t *samples, uint32_t num_samples,
                                uint16_t *peak_indices, uint16_t max_peaks,
                                int16_t min_peak_height)
{
    if (!samples || !peak_indices || num_samples < 3 || max_peaks == 0) {
        return 0;
    }

    /* Minimum separation between peaks: sample_rate / 100 is passed via the
     * caller context. Here we use a simple heuristic: at least num_samples /
     * (max_peaks * 4) separation to reject noise while still finding all real
     * peaks. Minimum 3 samples separation as an absolute floor. */
    uint32_t min_sep = num_samples / (max_peaks * 4);
    if (min_sep < 3) {
        min_sep = 3;
    }

    uint16_t count = 0;
    uint32_t last_peak_pos = 0;

    for (uint32_t i = 1; i < num_samples - 1 && count < max_peaks; i++) {
        /* Local maximum check */
        if (samples[i] > samples[i - 1] &&
            samples[i] >= samples[i + 1] &&
            samples[i] >= min_peak_height) {

            /* Enforce minimum separation from previous peak */
            if (count == 0 || (i - last_peak_pos) >= min_sep) {
                peak_indices[count] = (uint16_t)i;
                count++;
                last_peak_pos = i;
            } else if (samples[i] > samples[peak_indices[count - 1]]) {
                /* This peak is higher and within min_sep — replace previous */
                peak_indices[count - 1] = (uint16_t)i;
                last_peak_pos = i;
            }
        }
    }

    return count;
}

void compression_analyze(const int16_t *current_samples, uint32_t num_samples,
                         float sample_rate, compression_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));

    if (!current_samples || num_samples < 10 || sample_rate <= 0.0f) {
        result->valid = false;
        return;
    }

    uint8_t ncyl = s_cfg.num_cylinders;
    if (ncyl == 0 || ncyl > COMP_MAX_CYLINDERS) {
        result->valid = false;
        return;
    }

    /* Find a reasonable minimum peak height: use 25% of the global max */
    int16_t global_max = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (current_samples[i] > global_max) {
            global_max = current_samples[i];
        }
    }
    int16_t min_peak = (int16_t)(global_max / 4);
    if (min_peak < 1) min_peak = 1;

    /* Find peaks — allow up to cranks_to_average * ncyl + some extra */
    uint16_t max_expected = (uint16_t)(s_cfg.cranks_to_average * ncyl + ncyl);
    if (max_expected > 256) max_expected = 256;

    uint16_t peak_idx[256];
    uint16_t num_peaks = compression_find_peaks(current_samples, num_samples,
                                                 peak_idx, max_expected,
                                                 min_peak);

    if (num_peaks < ncyl) {
        /* Not enough peaks for even one crank cycle */
        result->valid = false;
        return;
    }

    /* Determine how many complete crank cycles we have */
    uint16_t full_cycles = num_peaks / ncyl;
    if (full_cycles > s_cfg.cranks_to_average) {
        full_cycles = s_cfg.cranks_to_average;
    }
    if (full_cycles == 0) {
        result->valid = false;
        return;
    }

    uint16_t peaks_used = full_cycles * ncyl;

    /* Average peak current per cylinder position across crank cycles */
    float cyl_sum[COMP_MAX_CYLINDERS];
    memset(cyl_sum, 0, sizeof(cyl_sum));

    for (uint16_t cycle = 0; cycle < full_cycles; cycle++) {
        for (uint8_t cyl = 0; cyl < ncyl; cyl++) {
            uint16_t idx = peak_idx[cycle * ncyl + cyl];
            cyl_sum[cyl] += (float)current_samples[idx];
        }
    }

    float max_peak_current = 0.0f;
    float min_peak_current = 1e9f;
    float total_current = 0.0f;
    uint8_t weakest = 0;

    for (uint8_t cyl = 0; cyl < ncyl; cyl++) {
        result->peak_current[cyl] = cyl_sum[cyl] / (float)full_cycles;
        total_current += result->peak_current[cyl];

        if (result->peak_current[cyl] > max_peak_current) {
            max_peak_current = result->peak_current[cyl];
        }
        if (result->peak_current[cyl] < min_peak_current) {
            min_peak_current = result->peak_current[cyl];
            weakest = cyl;
        }
    }

    result->average_current = total_current / (float)ncyl;

    /* Calculate relative compression percentage and pass/fail */
    for (uint8_t cyl = 0; cyl < ncyl; cyl++) {
        if (max_peak_current > 0.0f) {
            result->relative_pct[cyl] =
                (result->peak_current[cyl] / max_peak_current) * 100.0f;
        } else {
            result->relative_pct[cyl] = 0.0f;
        }
        result->pass[cyl] = (result->relative_pct[cyl] >= s_cfg.pass_threshold_pct);
    }

    /* Variation: difference between strongest and weakest as % of strongest */
    if (max_peak_current > 0.0f) {
        result->variation_pct =
            ((max_peak_current - min_peak_current) / max_peak_current) * 100.0f;
    }

    result->weakest_cylinder = weakest;
    result->num_cylinders    = ncyl;
    result->cranks_analyzed  = full_cycles;
    result->valid            = true;

    (void)peaks_used;
}
