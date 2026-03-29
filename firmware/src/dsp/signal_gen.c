/*
 * DDS (Direct Digital Synthesis) Signal Generator Engine
 *
 * Uses a 32-bit phase accumulator for precise frequency generation
 * from 0.1 Hz to 25 kHz. 8 waveform types with duty cycle control.
 *
 * Target: GD32F307 (ARM Cortex-M4F @ 120MHz)
 */

#include "signal_gen.h"
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
 * 256-entry sine lookup table: sin(2*PI*i/256) * 32767
 * Stored in flash (.rodata) to save SRAM
 * ═══════════════════════════════════════════════════════════════════ */
static const int16_t sine_lut[256] = {
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
     -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

/* ═══════════════════════════════════════════════════════════════════
 * DDS State
 * ═══════════════════════════════════════════════════════════════════ */

static siggen_config_t g_config;
static uint32_t g_phase_acc;    /* 32-bit phase accumulator */
static uint32_t g_tuning_word;  /* phase increment per sample */
static uint32_t g_noise_state = 0xDEADBEEF; /* LFSR state for noise */

/* Predefined amplitude steps (Vpp) */
static const float amplitude_steps[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 3.3f };
#define NUM_AMP_STEPS (sizeof(amplitude_steps) / sizeof(amplitude_steps[0]))

/* ═══════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════ */

static uint32_t compute_tuning_word(float freq_hz, float sample_rate)
{
    return (uint32_t)((double)freq_hz / (double)sample_rate * 4294967296.0);
}

static int32_t generate_raw_sample(uint32_t phase)
{
    switch (g_config.waveform) {
    case SIGGEN_SINE: {
        uint8_t idx = (uint8_t)(phase >> 24);
        uint8_t frac = (uint8_t)(phase >> 16);
        int32_t s0 = sine_lut[idx];
        int32_t s1 = sine_lut[(uint8_t)(idx + 1)];
        return s0 + (((s1 - s0) * frac) >> 8);
    }

    case SIGGEN_SQUARE: {
        uint32_t threshold = (uint32_t)((uint64_t)g_config.duty_cycle_pct * 4294967295ULL / 100);
        return (phase < threshold) ? 32767 : -32767;
    }

    case SIGGEN_TRIANGLE: {
        uint32_t p = phase;
        int32_t ramp;
        if (p < 0x80000000UL) {
            ramp = (int32_t)(p >> 15);
            if (ramp > 32767) ramp = 65535 - ramp;
        } else {
            p -= 0x80000000UL;
            ramp = (int32_t)(p >> 15);
            if (ramp > 32767) ramp = 65535 - ramp;
            ramp = -ramp;
        }
        if (ramp > 32767) ramp = 32767;
        if (ramp < -32767) ramp = -32767;
        return ramp;
    }

    case SIGGEN_SAWTOOTH:
        return (int32_t)(phase >> 16) - 32768;

    case SIGGEN_FULLRECT_SINE: {
        uint8_t idx = (uint8_t)(phase >> 24);
        uint8_t frac = (uint8_t)(phase >> 16);
        int32_t s0 = sine_lut[idx];
        int32_t s1 = sine_lut[(uint8_t)(idx + 1)];
        int32_t val = s0 + (((s1 - s0) * frac) >> 8);
        return (val < 0) ? -val : val;
    }

    case SIGGEN_HALFRECT_SINE: {
        uint8_t idx = (uint8_t)(phase >> 24);
        uint8_t frac = (uint8_t)(phase >> 16);
        int32_t s0 = sine_lut[idx];
        int32_t s1 = sine_lut[(uint8_t)(idx + 1)];
        int32_t val = s0 + (((s1 - s0) * frac) >> 8);
        return (val > 0) ? val : 0;
    }

    case SIGGEN_PULSE: {
        uint32_t threshold = (uint32_t)((uint64_t)g_config.duty_cycle_pct * 4294967295ULL / 100);
        return (phase < threshold) ? 32767 : -32767;
    }

    case SIGGEN_NOISE: {
        g_noise_state ^= g_noise_state << 13;
        g_noise_state ^= g_noise_state >> 17;
        g_noise_state ^= g_noise_state << 5;
        return (int32_t)(g_noise_state & 0xFFFF) - 32768;
    }

    default:
        return 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void siggen_init(const siggen_config_t *cfg)
{
    if (cfg) {
        g_config = *cfg;
    } else {
        g_config.waveform       = SIGGEN_SINE;
        g_config.frequency_hz   = 1000.0f;
        g_config.amplitude_vpp  = 3.3f;
        g_config.offset_v       = 0.0f;
        g_config.duty_cycle_pct = 50;
        g_config.output_enabled = false;
    }
    g_phase_acc = 0;
    g_tuning_word = 0;
}

void siggen_fill_buffer(int16_t *buffer, uint16_t num_samples, float sample_rate)
{
    if (!buffer || num_samples == 0 || sample_rate <= 0.0f) return;

    g_tuning_word = compute_tuning_word(g_config.frequency_hz, sample_rate);

    float amp_scale = g_config.amplitude_vpp / 3.3f;
    if (amp_scale > 1.0f) amp_scale = 1.0f;
    if (amp_scale < 0.0f) amp_scale = 0.0f;

    int32_t offset_counts = (int32_t)(g_config.offset_v / 3.3f * 32767.0f);

    for (uint16_t i = 0; i < num_samples; i++) {
        int32_t raw = generate_raw_sample(g_phase_acc);
        int32_t scaled = (int32_t)((float)raw * amp_scale);
        scaled += offset_counts;
        if (scaled > 32767) scaled = 32767;
        if (scaled < -32767) scaled = -32767;
        buffer[i] = (int16_t)scaled;
        g_phase_acc += g_tuning_word;
    }
}

void siggen_set_frequency(float freq_hz)
{
    if (freq_hz < 0.1f) freq_hz = 0.1f;
    if (freq_hz > 25000.0f) freq_hz = 25000.0f;
    g_config.frequency_hz = freq_hz;
}

void siggen_set_waveform(siggen_waveform_t wave)
{
    if (wave < SIGGEN_WAVEFORM_COUNT) {
        g_config.waveform = wave;
        g_phase_acc = 0;
    }
}

void siggen_set_amplitude(float vpp)
{
    if (vpp < 0.0f) vpp = 0.0f;
    if (vpp > 3.3f) vpp = 3.3f;
    g_config.amplitude_vpp = vpp;
}

void siggen_set_offset(float offset_v)
{
    g_config.offset_v = offset_v;
}

void siggen_enable(bool enable)
{
    g_config.output_enabled = enable;
}

siggen_waveform_t siggen_cycle_waveform(void)
{
    g_config.waveform = (siggen_waveform_t)((g_config.waveform + 1) % SIGGEN_WAVEFORM_COUNT);
    g_phase_acc = 0;
    return g_config.waveform;
}

void siggen_set_duty_cycle(uint8_t pct)
{
    if (pct < 10) pct = 10;
    if (pct > 90) pct = 90;
    g_config.duty_cycle_pct = pct;
}

float siggen_amplitude_up(void)
{
    for (uint8_t i = 0; i < NUM_AMP_STEPS; i++) {
        if (amplitude_steps[i] > g_config.amplitude_vpp + 0.01f) {
            g_config.amplitude_vpp = amplitude_steps[i];
            return g_config.amplitude_vpp;
        }
    }
    g_config.amplitude_vpp = amplitude_steps[NUM_AMP_STEPS - 1];
    return g_config.amplitude_vpp;
}

float siggen_amplitude_down(void)
{
    for (int i = (int)NUM_AMP_STEPS - 1; i >= 0; i--) {
        if (amplitude_steps[i] < g_config.amplitude_vpp - 0.01f) {
            g_config.amplitude_vpp = amplitude_steps[i];
            return g_config.amplitude_vpp;
        }
    }
    g_config.amplitude_vpp = amplitude_steps[0];
    return g_config.amplitude_vpp;
}

uint8_t siggen_duty_cycle_up(void)
{
    if (g_config.duty_cycle_pct < 90)
        g_config.duty_cycle_pct += 10;
    return g_config.duty_cycle_pct;
}

uint8_t siggen_duty_cycle_down(void)
{
    if (g_config.duty_cycle_pct > 10)
        g_config.duty_cycle_pct -= 10;
    return g_config.duty_cycle_pct;
}

const siggen_config_t *siggen_get_config(void)
{
    return &g_config;
}
