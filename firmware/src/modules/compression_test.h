#ifndef COMPRESSION_TEST_H
#define COMPRESSION_TEST_H
#include <stdint.h>
#include <stdbool.h>

#define COMP_MAX_CYLINDERS 12

typedef struct {
    uint8_t  num_cylinders;              /* 1-12 */
    uint8_t  firing_order[COMP_MAX_CYLINDERS]; /* e.g., {1,2,3,4,5,6} */
    float    pass_threshold_pct;         /* e.g., 85% = fail below this */
    uint8_t  cranks_to_average;          /* Number of crank cycles to average */
} compression_config_t;

typedef struct {
    float    relative_pct[COMP_MAX_CYLINDERS];  /* Relative compression % per cyl */
    float    peak_current[COMP_MAX_CYLINDERS];  /* Peak current per cyl (amps) */
    bool     pass[COMP_MAX_CYLINDERS];          /* Per-cylinder pass/fail */
    float    average_current;                    /* Average peak across all syls */
    uint8_t  weakest_cylinder;                   /* Index of weakest (0-based) */
    float    variation_pct;                      /* Max variation between cylinders */
    uint8_t  num_cylinders;
    uint16_t cranks_analyzed;
    bool     valid;
} compression_result_t;

/* Initialize compression test */
void compression_init(const compression_config_t *cfg);

/* Analyze cranking current waveform.
 * current_samples: current clamp ADC data (int16_t)
 * The algorithm detects compression peaks and assigns them to cylinders. */
void compression_analyze(const int16_t *current_samples, uint32_t num_samples,
                         float sample_rate, compression_result_t *result);

/* Detect peak positions in cranking current waveform.
 * Returns number of peaks found. Peak positions stored in peak_indices[]. */
uint16_t compression_find_peaks(const int16_t *samples, uint32_t num_samples,
                                uint16_t *peak_indices, uint16_t max_peaks,
                                int16_t min_peak_height);

/* Get config */
const compression_config_t *compression_get_config(void);

#endif
