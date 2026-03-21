/*
 * FFT Test Signal Generator
 *
 * Produces known waveforms so the FFT pipeline can be validated
 * entirely in the emulator before real ADC data is available.
 */

#include "fft_test_signals.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Simple LFSR for pseudorandom noise */
static uint32_t lfsr_state = 0xACE1u;

static int16_t lfsr_next(void)
{
    uint32_t bit = ((lfsr_state >> 0) ^ (lfsr_state >> 2) ^
                    (lfsr_state >> 3) ^ (lfsr_state >> 5)) & 1u;
    lfsr_state = (lfsr_state >> 1) | (bit << 15);
    return (int16_t)(lfsr_state & 0xFFFF) - 16384;
}

void test_signal_generate(test_signal_t type, int16_t *buffer,
                          uint16_t num_samples, float sample_rate,
                          float freq1_hz, float freq2_hz,
                          float amplitude)
{
    float scale = amplitude * 32767.0f;
    float dt = 1.0f / sample_rate;
    uint16_t i;

    switch (type) {
    case TEST_SIG_SINE:
        for (i = 0; i < num_samples; i++) {
            float t = (float)i * dt;
            buffer[i] = (int16_t)(sinf(2.0f * M_PI * freq1_hz * t) * scale);
        }
        break;

    case TEST_SIG_DUAL_SINE:
        for (i = 0; i < num_samples; i++) {
            float t = (float)i * dt;
            float val = sinf(2.0f * M_PI * freq1_hz * t) * 0.7f
                      + sinf(2.0f * M_PI * freq2_hz * t) * 0.3f;
            buffer[i] = (int16_t)(val * scale);
        }
        break;

    case TEST_SIG_SQUARE:
        for (i = 0; i < num_samples; i++) {
            float t = (float)i * dt;
            float phase = fmodf(t * freq1_hz, 1.0f);
            buffer[i] = (int16_t)((phase < 0.5f ? 1.0f : -1.0f) * scale);
        }
        break;

    case TEST_SIG_NOISE:
        lfsr_state = 0xACE1u;  /* Reset for reproducibility */
        for (i = 0; i < num_samples; i++) {
            buffer[i] = (int16_t)((float)lfsr_next() * amplitude);
        }
        break;
    }
}
