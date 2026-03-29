#ifndef SIGNAL_GEN_H
#define SIGNAL_GEN_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SIGGEN_SINE = 0,
    SIGGEN_SQUARE,
    SIGGEN_TRIANGLE,
    SIGGEN_SAWTOOTH,
    SIGGEN_FULLRECT_SINE,   /* |sin(x)| -- full-wave rectified */
    SIGGEN_HALFRECT_SINE,   /* max(sin(x), 0) -- half-wave rectified */
    SIGGEN_PULSE,           /* narrow pulse (duty cycle adjustable) */
    SIGGEN_NOISE,           /* pseudo-random noise */
    SIGGEN_WAVEFORM_COUNT
} siggen_waveform_t;

typedef struct {
    siggen_waveform_t waveform;
    float frequency_hz;     /* 0.1 Hz to 25 kHz */
    float amplitude_vpp;    /* 0.0 to 3.3V */
    float offset_v;         /* DC offset */
    uint8_t duty_cycle_pct; /* Duty cycle 10-90%, used by square and pulse */
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
void siggen_set_duty_cycle(uint8_t pct);
siggen_waveform_t siggen_cycle_waveform(void);
const siggen_config_t *siggen_get_config(void);

/* Step amplitude up/down through predefined levels. Returns new value. */
float siggen_amplitude_up(void);
float siggen_amplitude_down(void);

/* Step duty cycle up/down by 10%. Returns new value. */
uint8_t siggen_duty_cycle_up(void);
uint8_t siggen_duty_cycle_down(void);

#endif
