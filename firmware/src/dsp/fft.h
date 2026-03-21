/*
 * FFT Engine for OpenScope 2C53T
 *
 * Phase 1: Custom radix-2 DIT FFT (no external dependencies)
 * Phase 2: Drop-in replacement with CMSIS-DSP arm_rfft_fast_f32()
 *
 * Fixes every stock firmware FFT problem:
 *   - Proper windowing (Hanning/Hamming/Rectangular)
 *   - Correct amplitude scaling (2/N normalization)
 *   - dB magnitude display (20*log10)
 *   - Peak detection with frequency labels
 */

#ifndef FFT_H
#define FFT_H

#include <stdint.h>
#include <stdbool.h>

/* Phase 1: 1024-point (14KB RAM). Phase 2 scales to 4096. */
#define FFT_SIZE        1024
#define FFT_BINS        (FFT_SIZE / 2)

/* Maximum number of peaks to detect */
#define FFT_MAX_PEAKS   8

/* Window function types */
typedef enum {
    FFT_WINDOW_RECTANGULAR = 0,
    FFT_WINDOW_HANNING,
    FFT_WINDOW_HAMMING,
    FFT_WINDOW_COUNT
} fft_window_t;

/* Detected frequency peak */
typedef struct {
    uint16_t bin;
    float    freq_hz;
    float    magnitude_db;
} fft_peak_t;

/* FFT configuration */
typedef struct {
    fft_window_t window;
    float        sample_rate_hz;
    float        ref_level_db;      /* Top of display (dBFS) */
    float        db_range;          /* Visible dB range (e.g. 80.0) */
    uint8_t      peak_count;        /* Number of peaks to detect (0=off) */
} fft_config_t;

/* FFT computation result */
typedef struct {
    float       magnitude_db[FFT_BINS];
    float       bin_width_hz;
    uint16_t    num_bins;
    fft_peak_t  peaks[FFT_MAX_PEAKS];
    uint8_t     num_peaks;
    float       peak_freq_hz;       /* Frequency of strongest peak */
    float       peak_mag_db;        /* Magnitude of strongest peak */
} fft_result_t;

/* Initialize FFT engine. Precomputes window coefficients and twiddle factors. */
void fft_init(const fft_config_t *cfg);

/* Process int16 ADC samples into frequency domain result. */
void fft_process(const int16_t *samples, uint16_t num_samples,
                 fft_result_t *result);

/* Get current configuration (for UI display). */
const fft_config_t *fft_get_config(void);

/* Change window type (recomputes coefficients). */
void fft_set_window(fft_window_t window);

/* Cycle to next window type. Returns new window type. */
fft_window_t fft_cycle_window(void);

#endif /* FFT_H */
