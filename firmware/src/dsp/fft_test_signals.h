/*
 * FFT Test Signal Generator
 *
 * Generates known test waveforms for validating the FFT pipeline
 * without real ADC data. Useful for emulator-first development
 * and on-hardware self-test.
 */

#ifndef FFT_TEST_SIGNALS_H
#define FFT_TEST_SIGNALS_H

#include <stdint.h>

/* Available test signal types */
typedef enum {
    TEST_SIG_SINE,          /* Single sine wave */
    TEST_SIG_DUAL_SINE,     /* Two sines (tests frequency resolution) */
    TEST_SIG_SQUARE,        /* Square wave (tests harmonic detection) */
    TEST_SIG_NOISE,         /* Pseudorandom noise (tests noise floor) */
} test_signal_t;

/*
 * Generate a test signal into an int16 buffer.
 *
 * type:        Waveform type
 * buffer:      Output buffer (int16_t samples)
 * num_samples: Number of samples to generate
 * sample_rate: Sample rate in Hz
 * freq1_hz:    Primary frequency (used by all types)
 * freq2_hz:    Secondary frequency (DUAL_SINE only, ignored otherwise)
 * amplitude:   Peak amplitude as fraction of int16 range (0.0 - 1.0)
 */
void test_signal_generate(test_signal_t type, int16_t *buffer,
                          uint16_t num_samples, float sample_rate,
                          float freq1_hz, float freq2_hz,
                          float amplitude);

#endif /* FFT_TEST_SIGNALS_H */
