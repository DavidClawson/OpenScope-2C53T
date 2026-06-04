/*
 * Unit tests for DMM voltage waveform processing.
 *
 * Build:
 *   gcc -o tests/test_meter_voltage_wave tests/test_meter_voltage_wave.c \
 *       src/ui/meter_voltage_wave.c -lm -Isrc/ui
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "meter_voltage_wave.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-44s ", #name); \
    if (test_##name()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAIL\n"); } \
} while (0)

#define ASSERT(cond) do { \
    if (!(cond)) { printf("[line %d: %s] ", __LINE__, #cond); return 0; } \
} while (0)

static void feed_sine(float hz, uint16_t count, float amplitude)
{
    for (uint16_t i = 0; i < count; i++) {
        float phase = 2.0f * 3.1415926535f * hz *
                      ((float)i / METER_VOLTAGE_WAVE_SAMPLE_HZ);
        int raw = 128 + (int)(sinf(phase) * amplitude);
        if (raw < 0) raw = 0;
        if (raw > 255) raw = 255;
        meter_voltage_wave_add_sample((uint8_t)raw);
    }
}

static void feed_stepped_inverter_50hz(uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        uint16_t phase = i % 20;  /* 50 Hz at 1 ksample/s */
        int raw;

        if (phase < 2 || (phase >= 9 && phase < 12) || phase >= 19) {
            raw = 128;       /* zero segment */
        } else if (phase < 9) {
            raw = 210;       /* positive step */
        } else {
            raw = 46;        /* negative step */
        }
        meter_voltage_wave_add_sample((uint8_t)raw);
    }
}

static void feed_chopped_50hz(uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        uint16_t phase = i % 20;  /* 50 Hz at 1 ksample/s */
        int raw;
        if (phase < 3 || (phase >= 10 && phase < 13)) {
            raw = 128;  /* triac-style dead/chopped segment */
        } else if (phase < 10) {
            raw = 128 + (int)(90.0f * sinf((float)phase * 3.1415926535f / 10.0f));
        } else {
            raw = 128 - (int)(90.0f * sinf((float)(phase - 10) * 3.1415926535f / 10.0f));
        }
        if (raw < 0) raw = 0;
        if (raw > 255) raw = 255;
        meter_voltage_wave_add_sample((uint8_t)raw);
    }
}

static void feed_noisy_sine_50hz(uint16_t count)
{
    uint32_t noise_state = 0x12345678U;

    for (uint16_t i = 0; i < count; i++) {
        float phase = 2.0f * 3.1415926535f * 50.0f *
                      ((float)i / METER_VOLTAGE_WAVE_SAMPLE_HZ);
        noise_state = noise_state * 1664525U + 1013904223U;
        int noise = (int)((noise_state >> 24) & 0x1F) - 16;
        int raw = 128 + (int)(sinf(phase) * 70.0f) + noise;
        if (raw < 0) raw = 0;
        if (raw > 255) raw = 255;
        meter_voltage_wave_add_sample((uint8_t)raw);
    }
}

static void feed_dc_with_ripple(uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        float phase = 2.0f * 3.1415926535f * 100.0f *
                      ((float)i / METER_VOLTAGE_WAVE_SAMPLE_HZ);
        int raw = 170 + (int)(sinf(phase) * 12.0f);
        if (raw < 0) raw = 0;
        if (raw > 255) raw = 255;
        meter_voltage_wave_add_sample((uint8_t)raw);
    }
}

static int test_sine_frequency_estimate(void)
{
    meter_voltage_wave_reset();
    feed_sine(50.0f, 500, 80.0f);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 0.0f);

    ASSERT(snap.count > 20);
    ASSERT(snap.freq_hz > 47.0f && snap.freq_hz < 53.0f);
    ASSERT(snap.rms_raw > 50.0f && snap.rms_raw < 60.0f);
    ASSERT(snap.raw_max > 200);
    ASSERT(snap.raw_min < 60);
    ASSERT(snap.peak_to_peak_raw == (uint16_t)snap.raw_max - (uint16_t)snap.raw_min);
    return 1;
}

static int test_dmm_rms_scale_estimates_sine_peak_to_peak(void)
{
    meter_voltage_wave_reset();
    feed_sine(50.0f, 500, 80.0f);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 50.0f);

    meter_voltage_wave_scale_t scale =
        meter_voltage_wave_scale_from_dmm_rms(&snap, 12.0f);
    float pp = meter_voltage_wave_peak_to_peak_volts(&snap, scale);

    ASSERT(scale.valid);
    ASSERT(pp > 33.0f && pp < 35.0f);
    return 1;
}

static int test_sync_hint_stabilizes_start(void)
{
    meter_voltage_wave_reset();
    feed_sine(50.0f, 500, 80.0f);

    meter_voltage_wave_snapshot_t a, b;
    meter_voltage_wave_snapshot(&a, 300, 50.0f);
    feed_sine(50.0f, 20, 80.0f);
    meter_voltage_wave_snapshot(&b, 300, 50.0f);

    ASSERT(a.synced);
    ASSERT(b.synced);
    ASSERT(a.count > 20 && b.count > 20);

    int total_delta = 0;
    for (uint16_t i = 0; i < 16; i++) {
        total_delta += abs((int)a.y[i] - (int)b.y[i]);
    }
    ASSERT(total_delta < 16 * 20);
    return 1;
}

static int test_flat_dc_has_no_fake_frequency(void)
{
    meter_voltage_wave_reset();
    for (uint16_t i = 0; i < 200; i++) {
        meter_voltage_wave_add_sample(160);
    }

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 0.0f);

    ASSERT(snap.count > 2);
    ASSERT(snap.freq_hz == 0.0f);
    ASSERT(snap.rms_raw == 0.0f);
    ASSERT(snap.raw_min == 160 && snap.raw_max == 160);
    return 1;
}

static int test_chopped_wave_preserves_envelope(void)
{
    meter_voltage_wave_reset();
    feed_chopped_50hz(500);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 50.0f);

    ASSERT(snap.synced);
    ASSERT(snap.raw_max > 200);
    ASSERT(snap.raw_min < 60);

    int envelope_spread_bins = 0;
    for (uint16_t i = 0; i < snap.count; i++) {
        if ((int)snap.env_max[i] - (int)snap.env_min[i] > 10) {
            envelope_spread_bins++;
        }
    }
    ASSERT(envelope_spread_bins > 0);
    return 1;
}

static int test_stepped_inverter_keeps_square_edges(void)
{
    meter_voltage_wave_reset();
    feed_stepped_inverter_50hz(500);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 50.0f);

    ASSERT(snap.synced);
    ASSERT(snap.raw_max == 210);
    ASSERT(snap.raw_min == 46);
    ASSERT(snap.rms_raw > 60.0f && snap.rms_raw < 90.0f);

    int high_points = 0;
    int mid_points = 0;
    int low_points = 0;
    for (uint16_t i = 0; i < snap.count; i++) {
        if (snap.y[i] > 220) high_points++;
        else if (snap.y[i] < 35) low_points++;
        else if (snap.y[i] > 110 && snap.y[i] < 145) mid_points++;
    }
    ASSERT(high_points > 5);
    ASSERT(mid_points > 3);
    ASSERT(low_points > 5);
    return 1;
}

static int test_noisy_sine_envelope_spreads(void)
{
    meter_voltage_wave_reset();
    feed_noisy_sine_50hz(512);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 50.0f);

    ASSERT(snap.synced);
    ASSERT(snap.freq_hz > 45.0f && snap.freq_hz < 55.0f);

    int spread_bins = 0;
    for (uint16_t i = 0; i < 300; i++) {
        if ((int)snap.env_max[i] - (int)snap.env_min[i] > 8) {
            spread_bins++;
        }
    }
    ASSERT(spread_bins > 10);
    return 1;
}

static int test_dc_with_ripple_keeps_dc_mean_and_ac_shape(void)
{
    meter_voltage_wave_reset();
    feed_dc_with_ripple(500);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 0.0f);

    ASSERT(snap.count > 20);
    ASSERT(snap.mean_raw > 165.0f && snap.mean_raw < 175.0f);
    ASSERT(snap.rms_raw > 7.0f && snap.rms_raw < 10.0f);
    ASSERT(snap.raw_min < 160);
    ASSERT(snap.raw_max > 180);
    ASSERT(snap.freq_hz > 90.0f && snap.freq_hz < 110.0f);
    return 1;
}

static int test_ring_buffer_keeps_recent_window_only(void)
{
    meter_voltage_wave_reset();

    for (uint16_t i = 0; i < 120; i++) {
        meter_voltage_wave_add_sample(10);
    }
    for (uint16_t i = 0; i < METER_VOLTAGE_WAVE_CAPACITY; i++) {
        meter_voltage_wave_add_sample((uint8_t)(100 + (i % 50)));
    }

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 0.0f);

    ASSERT(meter_voltage_wave_sample_count() ==
           120U + (uint32_t)METER_VOLTAGE_WAVE_CAPACITY);
    ASSERT(snap.raw_min >= 100);
    ASSERT(snap.raw_max <= 149);
    ASSERT(snap.count > 2);
    return 1;
}

static int test_scale_rejects_flat_or_zero_dmm_reference(void)
{
    meter_voltage_wave_reset();
    for (uint16_t i = 0; i < 200; i++) {
        meter_voltage_wave_add_sample(160);
    }

    meter_voltage_wave_snapshot_t flat;
    meter_voltage_wave_snapshot(&flat, 300, 0.0f);

    meter_voltage_wave_scale_t flat_scale =
        meter_voltage_wave_scale_from_dmm_rms(&flat, 12.0f);
    ASSERT(!flat_scale.valid);

    meter_voltage_wave_reset();
    feed_sine(50.0f, 500, 80.0f);

    meter_voltage_wave_snapshot_t sine;
    meter_voltage_wave_snapshot(&sine, 300, 50.0f);

    meter_voltage_wave_scale_t zero_ref_scale =
        meter_voltage_wave_scale_from_dmm_rms(&sine, 0.0f);
    ASSERT(!zero_ref_scale.valid);
    ASSERT(meter_voltage_wave_peak_to_peak_volts(&sine, zero_ref_scale) == 0.0f);
    return 1;
}

static int test_frequency_hint_is_preferred_when_available(void)
{
    meter_voltage_wave_reset();
    feed_sine(50.0f, 500, 80.0f);

    meter_voltage_wave_snapshot_t snap;
    meter_voltage_wave_snapshot(&snap, 300, 60.0f);

    ASSERT(snap.synced);
    ASSERT(snap.freq_hz == 60.0f);
    ASSERT(snap.count > 20);
    return 1;
}

int main(void)
{
    printf("DMM Voltage Waveform Tests\n");

    TEST(sine_frequency_estimate);
    TEST(dmm_rms_scale_estimates_sine_peak_to_peak);
    TEST(sync_hint_stabilizes_start);
    TEST(flat_dc_has_no_fake_frequency);
    TEST(chopped_wave_preserves_envelope);
    TEST(stepped_inverter_keeps_square_edges);
    TEST(noisy_sine_envelope_spreads);
    TEST(dc_with_ripple_keeps_dc_mean_and_ac_shape);
    TEST(ring_buffer_keeps_recent_window_only);
    TEST(scale_rejects_flat_or_zero_dmm_reference);
    TEST(frequency_hint_is_preferred_when_available);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
