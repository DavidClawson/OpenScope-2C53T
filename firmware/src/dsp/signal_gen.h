#ifndef SIGNAL_GEN_H
#define SIGNAL_GEN_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SIGGEN_SINE = 0,
    SIGGEN_SQUARE,
    SIGGEN_TRIANGLE,
    SIGGEN_SAWTOOTH,
    SIGGEN_WAVEFORM_COUNT
} siggen_waveform_t;

typedef struct {
    siggen_waveform_t waveform;
    float frequency_hz;     /* 0.1 Hz to 25 kHz */
    float amplitude_vpp;    /* 0.0 to 3.3V */
    float offset_v;         /* DC offset */
    bool  output_enabled;
} siggen_config_t;

/* Initialize signal generator with given config */
void siggen_init(const siggen_config_t *cfg);

/* Generate num_samples into a 16-bit DAC buffer (for preview or actual output) */
void siggen_fill_buffer(int16_t *buffer, uint16_t num_samples, float sample_rate);

/* Configuration API */
void siggen_set_frequency(float freq_hz);
void siggen_set_waveform(siggen_waveform_t wave);
void siggen_set_amplitude(float vpp);
void siggen_set_offset(float offset_v);
void siggen_enable(bool enable);
siggen_waveform_t siggen_cycle_waveform(void);
const siggen_config_t *siggen_get_config(void);

#endif
