/*
 * OpenScope 2C53T - DMM Voltage Waveform
 */

#include "meter_voltage_wave.h"
#include <string.h>
#include <math.h>

#ifdef __arm__
#include "FreeRTOS.h"
#include "task.h"
#define METER_WAVE_LOCK()   taskENTER_CRITICAL()
#define METER_WAVE_UNLOCK() taskEXIT_CRITICAL()
#else
#define METER_WAVE_LOCK()   do {} while (0)
#define METER_WAVE_UNLOCK() do {} while (0)
#endif

static uint8_t samples[METER_VOLTAGE_WAVE_CAPACITY];
static uint8_t snapshot_samples[METER_VOLTAGE_WAVE_CAPACITY];
static uint16_t write_pos;
static uint16_t sample_count;
static uint32_t total_samples;

static void reset_unlocked(void)
{
    memset(samples, 0, sizeof(samples));
    write_pos = 0;
    sample_count = 0;
    total_samples = 0;
}

void meter_voltage_wave_init(void)
{
    reset_unlocked();
}

void meter_voltage_wave_reset(void)
{
    METER_WAVE_LOCK();
    reset_unlocked();
    METER_WAVE_UNLOCK();
}

void meter_voltage_wave_add_sample(uint8_t raw)
{
    METER_WAVE_LOCK();
    samples[write_pos] = raw;
    write_pos++;
    if (write_pos >= METER_VOLTAGE_WAVE_CAPACITY) write_pos = 0;
    if (sample_count < METER_VOLTAGE_WAVE_CAPACITY) sample_count++;
    total_samples++;
    METER_WAVE_UNLOCK();
}

uint32_t meter_voltage_wave_sample_count(void)
{
    uint32_t count;

    METER_WAVE_LOCK();
    count = total_samples;
    METER_WAVE_UNLOCK();

    return count;
}

static uint16_t ring_index_from_oldest(uint16_t oldest, uint16_t offset)
{
    uint16_t idx = oldest + offset;
    while (idx >= METER_VOLTAGE_WAVE_CAPACITY) {
        idx -= METER_VOLTAGE_WAVE_CAPACITY;
    }
    return idx;
}

static uint16_t find_sync_offset(const uint8_t *buf, uint16_t oldest,
                                 uint16_t count, float mean)
{
    if (count < 8) return 0;

    for (uint16_t i = 1; i < count; i++) {
        float prev = (float)buf[ring_index_from_oldest(oldest, i - 1)] - mean;
        float cur  = (float)buf[ring_index_from_oldest(oldest, i)] - mean;
        if (prev < 0.0f && cur >= 0.0f) {
            return i;
        }
    }
    return 0;
}

static uint16_t sync_window_from_hint(uint16_t count, float freq_hint_hz)
{
    if (freq_hint_hz < 10.0f || freq_hint_hz > 1000.0f) {
        return count;
    }

    float period = METER_VOLTAGE_WAVE_SAMPLE_HZ / freq_hint_hz;
    uint16_t want = (uint16_t)(period * 2.5f);
    if (want < 64) want = 64;
    if (want > count) want = count;
    return want;
}

static float estimate_frequency(const uint8_t *buf, uint16_t oldest,
                                uint16_t count, float mean)
{
    uint16_t first = 0xFFFF;
    uint16_t last = 0;
    uint16_t crossings = 0;

    for (uint16_t i = 1; i < count; i++) {
        float prev = (float)buf[ring_index_from_oldest(oldest, i - 1)] - mean;
        float cur  = (float)buf[ring_index_from_oldest(oldest, i)] - mean;
        if (prev < 0.0f && cur >= 0.0f) {
            if (first == 0xFFFF) first = i;
            last = i;
            crossings++;
        }
    }

    if (crossings < 2 || last <= first) return 0.0f;
    float cycles = (float)(crossings - 1);
    float seconds = (float)(last - first) / METER_VOLTAGE_WAVE_SAMPLE_HZ;
    if (seconds <= 0.0f) return 0.0f;
    return cycles / seconds;
}

void meter_voltage_wave_snapshot(meter_voltage_wave_snapshot_t *out,
                                 uint16_t render_points,
                                 float freq_hint_hz)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    METER_WAVE_LOCK();
    uint16_t available = sample_count;
    uint16_t snapshot_write_pos = write_pos;
    memcpy(snapshot_samples, samples, sizeof(snapshot_samples));
    METER_WAVE_UNLOCK();

    if (render_points > METER_VOLTAGE_WAVE_RENDER_POINTS) {
        render_points = METER_VOLTAGE_WAVE_RENDER_POINTS;
    }
    if (available < 2 || render_points < 2) return;

    uint16_t oldest = (snapshot_write_pos + METER_VOLTAGE_WAVE_CAPACITY - available)
                      % METER_VOLTAGE_WAVE_CAPACITY;

    uint32_t sum = 0;
    uint8_t raw_min = 255;
    uint8_t raw_max = 0;
    for (uint16_t i = 0; i < available; i++) {
        uint8_t v = snapshot_samples[ring_index_from_oldest(oldest, i)];
        sum += v;
        if (v < raw_min) raw_min = v;
        if (v > raw_max) raw_max = v;
    }

    float mean = (float)sum / (float)available;
    float sum_sq = 0.0f;
    for (uint16_t i = 0; i < available; i++) {
        float centered = (float)snapshot_samples[ring_index_from_oldest(oldest, i)] - mean;
        sum_sq += centered * centered;
    }

    uint16_t window = sync_window_from_hint(available, freq_hint_hz);
    uint16_t window_oldest = ring_index_from_oldest(oldest, available - window);
    uint16_t sync_offset = find_sync_offset(snapshot_samples, window_oldest, window, mean);
    uint16_t render_start = ring_index_from_oldest(window_oldest, sync_offset);
    uint16_t render_count = window - sync_offset;
    if (render_count > render_points) render_count = render_points;
    if (render_count < 2) {
        render_count = available < render_points ? available : render_points;
        render_start = ring_index_from_oldest(oldest, available - render_count);
    }

    uint16_t peak_to_peak = (uint16_t)raw_max - (uint16_t)raw_min;
    bool has_signal = peak_to_peak >= 4;
    uint16_t range = peak_to_peak;
    if (range < 4) range = 4;

    for (uint16_t i = 0; i < render_points; i++) {
        out->env_min[i] = 255;
        out->env_max[i] = 0;
    }

    for (uint16_t i = 0; i < render_count; i++) {
        uint8_t v = snapshot_samples[ring_index_from_oldest(render_start, i)];
        out->y[i] = (uint8_t)(((uint16_t)v - raw_min) * 255U / range);
    }

    for (uint16_t i = 0; i < available; i++) {
        uint16_t bin = (uint16_t)((uint32_t)i * render_points / available);
        if (bin >= render_points) bin = render_points - 1;
        uint8_t v = snapshot_samples[ring_index_from_oldest(oldest, i)];
        uint8_t y = (uint8_t)(((uint16_t)v - raw_min) * 255U / range);
        if (y < out->env_min[bin]) out->env_min[bin] = y;
        if (y > out->env_max[bin]) out->env_max[bin] = y;
    }

    for (uint16_t i = 0; i < render_points; i++) {
        if (out->env_min[i] == 255 && out->env_max[i] == 0) {
            out->env_min[i] = out->y[i < render_count ? i : render_count - 1];
            out->env_max[i] = out->env_min[i];
        }
    }

    out->count = render_count;
    out->raw_min = raw_min;
    out->raw_max = raw_max;
    out->peak_to_peak_raw = peak_to_peak;
    out->has_signal = has_signal;
    out->stuck_high = (raw_min == 0xFF && raw_max == 0xFF);
    out->raw_last = snapshot_samples[(snapshot_write_pos + METER_VOLTAGE_WAVE_CAPACITY - 1)
                                     % METER_VOLTAGE_WAVE_CAPACITY];
    out->mean_raw = mean;
    out->rms_raw = sqrtf(sum_sq / (float)available);
    if (has_signal) {
        out->freq_hz = (freq_hint_hz >= 10.0f) ? freq_hint_hz
                                               : estimate_frequency(snapshot_samples, oldest,
                                                                    available, mean);
        out->synced = (sync_offset != 0 || out->freq_hz >= 10.0f);
    }
}

meter_voltage_wave_scale_t
meter_voltage_wave_scale_from_dmm_rms(const meter_voltage_wave_snapshot_t *snap,
                                      float dmm_rms_volts)
{
    meter_voltage_wave_scale_t scale = { false, 0.0f };

    if (snap == NULL) return scale;
    if (snap->rms_raw <= 0.5f) return scale;
    if (dmm_rms_volts <= 0.0f) return scale;

    scale.valid = true;
    scale.volts_per_raw_rms = dmm_rms_volts / snap->rms_raw;
    return scale;
}

float meter_voltage_wave_peak_to_peak_volts(const meter_voltage_wave_snapshot_t *snap,
                                            meter_voltage_wave_scale_t scale)
{
    if (snap == NULL || !scale.valid) return 0.0f;
    return (float)snap->peak_to_peak_raw * scale.volts_per_raw_rms;
}
