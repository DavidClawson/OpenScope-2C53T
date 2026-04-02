/*
 * FFT Engine for OpenScope 2C53T
 *
 * Phase 2: 4096-point FFT with 5 window functions, averaging, max hold
 *
 * Fixes every stock firmware FFT problem:
 *   - Proper windowing (5 types including Blackman-Harris)
 *   - Correct amplitude scaling (2/N normalization)
 *   - dB magnitude display (20*log10)
 *   - Peak detection with frequency labels
 *   - Exponential moving average for noise reduction
 *   - Max hold for peak envelope tracking
 */

#ifndef FFT_H
#define FFT_H

#include <stdint.h>
#include <stdbool.h>

/* 4096-point FFT: 10.77 Hz resolution at 44.1kHz sample rate */
#define FFT_SIZE        4096
#define FFT_BINS        (FFT_SIZE / 2)

/* Maximum number of peaks to detect */
#define FFT_MAX_PEAKS   8

/* Window function types */
typedef enum {
    FFT_WINDOW_RECTANGULAR = 0,
    FFT_WINDOW_HANNING,
    FFT_WINDOW_HAMMING,
    FFT_WINDOW_BLACKMAN_HARRIS,
    FFT_WINDOW_FLAT_TOP,
    FFT_WINDOW_COUNT
} fft_window_t;

/* Detected frequency peak */
typedef struct {
    uint16_t bin;
    float    freq_hz;
    float    magnitude_db;
    char     label[6];      /* "Fund", "H2", "H3", etc. or "" */
} fft_peak_t;

/* FFT configuration */
typedef struct {
    fft_window_t window;
    float        sample_rate_hz;
    float        ref_level_db;      /* Top of display (dBFS) */
    float        db_range;          /* Visible dB range (e.g. 80.0) */
    uint8_t      peak_count;        /* Number of peaks to detect (0=off) */
    uint8_t      avg_count;         /* EMA averaging: 0=off, 2/4/8/16/32 */
    bool         max_hold;          /* Max hold mode active */
    uint16_t     zoom_start_bin;    /* First visible bin (default: 1) */
    uint16_t     zoom_end_bin;      /* Last visible bin (default: FFT_BINS-1) */
} fft_config_t;

/* FFT computation result — pointers to internal static buffers (no copy) */
typedef struct {
    const float *magnitude_db;      /* Current frame magnitude [FFT_BINS] */
    const float *avg_db;            /* Averaged magnitude (NULL if off) */
    const float *max_hold_db;       /* Max hold envelope (NULL if off) */
    float       bin_width_hz;
    uint16_t    num_bins;
    fft_peak_t  peaks[FFT_MAX_PEAKS];
    uint8_t     num_peaks;
    float       peak_freq_hz;       /* Frequency of strongest peak */
    float       peak_mag_db;        /* Magnitude of strongest peak */
} fft_result_t;

/* Initialize FFT engine. Claims shared memory pool and precomputes
 * window coefficients and twiddle factors. */
void fft_init(const fft_config_t *cfg);

/* Deinitialize FFT engine. Releases shared memory pool. */
void fft_deinit(void);

/* Check if FFT engine is initialized (has pool ownership). */
bool fft_is_initialized(void);

/* Get the FFT sample scratch buffer (FFT_SIZE int16s, lives in shared pool).
 * Returns NULL if FFT is not initialized. Fill this, then pass to fft_process(). */
int16_t *fft_get_sample_buf(void);

/* Process int16 ADC samples into frequency domain result. */
void fft_process(const int16_t *samples, uint16_t num_samples,
                 fft_result_t *result);

/* Get current configuration (for UI display). */
const fft_config_t *fft_get_config(void);

/* Change window type (recomputes coefficients). */
void fft_set_window(fft_window_t window);

/* Cycle to next window type. Returns new window type. */
fft_window_t fft_cycle_window(void);

/* Set averaging count (0=off, 2/4/8/16/32). */
void fft_set_averaging(uint8_t count);

/* Enable/disable max hold. Resets buffer when enabling. */
void fft_set_max_hold(bool enable);

/* Reset max hold buffer. */
void fft_reset_max_hold(void);

/* Adjust reference level by delta dB. Returns new ref_level_db. */
float fft_adjust_ref_level(float delta_db);

/* Zoom in (halve visible range around center). */
void fft_zoom_in(void);

/* Zoom out (double visible range, clamped to full span). */
void fft_zoom_out(void);

/* Auto-configure: estimate fundamental from zero crossings, set zoom/scale. */
void fft_auto_configure(const int16_t *samples, uint16_t num_samples);

#endif /* FFT_H */
