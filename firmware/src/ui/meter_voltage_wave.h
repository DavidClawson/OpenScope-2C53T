/*
 * OpenScope 2C53T - DMM Voltage Waveform
 *
 * Processes raw samples from the multimeter voltage path. This is not the
 * oscilloscope CH1/CH2 path: callers feed samples captured while the DMM
 * frontend is routed to the COM + V/Ohm/C jacks.
 */

#ifndef METER_VOLTAGE_WAVE_H
#define METER_VOLTAGE_WAVE_H

#include <stdint.h>
#include <stdbool.h>

#define METER_VOLTAGE_WAVE_CAPACITY      512
#define METER_VOLTAGE_WAVE_RENDER_POINTS 300
#define METER_VOLTAGE_WAVE_SAMPLE_HZ     1000.0f

typedef struct {
    uint16_t count;
    uint8_t  y[METER_VOLTAGE_WAVE_RENDER_POINTS];
    uint8_t  env_min[METER_VOLTAGE_WAVE_RENDER_POINTS];
    uint8_t  env_max[METER_VOLTAGE_WAVE_RENDER_POINTS];
    uint8_t  raw_min;
    uint8_t  raw_max;
    uint8_t  raw_last;
    uint16_t peak_to_peak_raw;
    float    mean_raw;
    float    rms_raw;
    float    freq_hz;
    bool     synced;
} meter_voltage_wave_snapshot_t;

typedef struct {
    bool  valid;
    float volts_per_raw_rms;
} meter_voltage_wave_scale_t;

void meter_voltage_wave_init(void);
void meter_voltage_wave_reset(void);
void meter_voltage_wave_add_sample(uint8_t raw);
uint32_t meter_voltage_wave_sample_count(void);

void meter_voltage_wave_snapshot(meter_voltage_wave_snapshot_t *out,
                                 uint16_t render_points,
                                 float freq_hint_hz);

meter_voltage_wave_scale_t
meter_voltage_wave_scale_from_dmm_rms(const meter_voltage_wave_snapshot_t *snap,
                                      float dmm_rms_volts);

float meter_voltage_wave_peak_to_peak_volts(const meter_voltage_wave_snapshot_t *snap,
                                            meter_voltage_wave_scale_t scale);

#endif /* METER_VOLTAGE_WAVE_H */
