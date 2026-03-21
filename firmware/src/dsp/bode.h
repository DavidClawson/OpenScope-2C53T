/*
 * FNIRSI 2C53T Bode Plot Analysis
 *
 * Sweeps the signal generator frequency and measures amplitude/phase
 * response of a device under test (DUT) connected between CH1 (reference)
 * and CH2 (response).
 */

#ifndef BODE_H
#define BODE_H

#include <stdint.h>
#include <stdbool.h>

#define BODE_MAX_POINTS 200  /* Max frequency points in sweep */

typedef struct {
    float start_freq_hz;     /* Sweep start (e.g., 10 Hz) */
    float stop_freq_hz;      /* Sweep stop (e.g., 20 kHz) */
    uint16_t num_points;     /* Number of frequency steps */
    float amplitude_vpp;     /* Signal gen output amplitude */
    bool  log_sweep;         /* true=logarithmic, false=linear spacing */
} bode_config_t;

typedef struct {
    float frequency_hz;
    float gain_db;           /* 20*log10(Vout/Vin) */
    float phase_deg;         /* Phase difference in degrees */
} bode_point_t;

typedef struct {
    bode_point_t points[BODE_MAX_POINTS];
    uint16_t     num_points;
    uint16_t     current_step;  /* Progress during sweep */
    bool         sweep_done;
    /* Summary */
    float        bandwidth_hz;   /* -3dB point */
    float        peak_gain_db;
    float        peak_freq_hz;
} bode_result_t;

/* Initialize Bode measurement */
void bode_init(const bode_config_t *cfg);

/* Calculate the frequency for a given sweep step */
float bode_step_frequency(const bode_config_t *cfg, uint16_t step);

/* Process one frequency point: given input and output sample buffers,
 * compute gain and phase at the current frequency.
 * input_samples: signal generator reference (CH1)
 * output_samples: system response (CH2) */
void bode_process_point(const int16_t *input_samples, const int16_t *output_samples,
                        uint16_t num_samples, float sample_rate, float freq_hz,
                        bode_point_t *point);

/* Find -3dB bandwidth from sweep data */
float bode_find_bandwidth(const bode_result_t *result);

/* Render Bode plot (gain + phase vs frequency) */
void bode_render_gain(const bode_result_t *result,
                      uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                      uint16_t color);

void bode_render_phase(const bode_result_t *result,
                       uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                       uint16_t color);

#endif /* BODE_H */
