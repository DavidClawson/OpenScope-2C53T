/*
 * component_test.c — Component tester module for FNIRSI 2C53T
 *
 * Supports resistor, capacitor, diode, and continuity testing
 * using ADC voltage/current sample data.
 *
 * Voltage scale: 3.3V / 32768 for int16 to volts conversion
 */

#include "component_test.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ADC conversion: int16 → volts (3.3V range, signed 16-bit) */
#define ADC_SCALE (3.3f / 32768.0f)

/* Continuity thresholds (ohms) */
#define CONTINUITY_THRESHOLD   50.0f
#define SHORT_THRESHOLD         1.0f
#define OPEN_THRESHOLD    10000000.0f  /* 10M ohm */

/* Known test resistance for capacitor measurement (ohms) */
#define CAP_TEST_R           1000.0f

/* Fraction of peak for time constant (1 - 1/e ≈ 0.632) */
#define TAU_FRACTION          0.632f

/* Module state */
static comp_test_config_t s_config;
static bool s_initialized = false;

/* Color band lookup table */
static const char *color_names[] = {
    "Blk", "Brn", "Red", "Org", "Yel",
    "Grn", "Blu", "Vio", "Gry", "Wht"
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static float samples_to_volts_avg(const int16_t *samples, uint16_t n)
{
    if (n == 0) return 0.0f;
    float sum = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        sum += (float)samples[i];
    }
    return (sum / (float)n) * ADC_SCALE;
}

static float fabsf_safe(float x)
{
    return x < 0.0f ? -x : x;
}

/* ------------------------------------------------------------------ */
/* Resistor measurement                                                */
/* ------------------------------------------------------------------ */

static void measure_resistor(const int16_t *voltage_samples,
                             const int16_t *current_samples,
                             uint16_t num_samples,
                             comp_test_result_t *result)
{
    float v_avg = samples_to_volts_avg(voltage_samples, num_samples);
    float i_avg = samples_to_volts_avg(current_samples, num_samples);

    result->type = COMP_RESISTOR;

    /* Guard against division by zero / open circuit */
    if (fabsf_safe(i_avg) < 1e-9f) {
        result->measured_ohms = OPEN_THRESHOLD;
        result->status = COMP_RESULT_OPEN;
        result->is_pass = false;
        result->deviation_pct = 100.0f;
        return;
    }

    float r = fabsf_safe(v_avg / i_avg);
    result->measured_ohms = r;

    /* Classify */
    if (r < SHORT_THRESHOLD) {
        result->status = COMP_RESULT_SHORT;
        result->is_pass = false;
        result->deviation_pct = 100.0f;
        return;
    }
    if (r >= OPEN_THRESHOLD) {
        result->status = COMP_RESULT_OPEN;
        result->is_pass = false;
        result->deviation_pct = 100.0f;
        return;
    }

    /* Pass/fail against nominal */
    if (s_config.nominal_ohms > 0.0f) {
        result->deviation_pct = fabsf_safe(r - s_config.nominal_ohms)
                                / s_config.nominal_ohms * 100.0f;
        result->is_pass = (result->deviation_pct <= s_config.tolerance_pct);
        result->status = result->is_pass ? COMP_RESULT_PASS : COMP_RESULT_FAIL;
    } else {
        result->deviation_pct = 0.0f;
        result->is_pass = true;
        result->status = COMP_RESULT_PASS;
    }
}

/* ------------------------------------------------------------------ */
/* Capacitor measurement                                               */
/* ------------------------------------------------------------------ */

static void measure_capacitor(const int16_t *voltage_samples,
                              const int16_t *current_samples,
                              uint16_t num_samples,
                              float sample_rate,
                              comp_test_result_t *result)
{
    result->type = COMP_CAPACITOR;

    if (num_samples < 2 || sample_rate <= 0.0f) {
        result->status = COMP_RESULT_FAIL;
        result->is_pass = false;
        return;
    }

    /* Find peak voltage in the charge/discharge curve */
    float v_peak = 0.0f;
    for (uint16_t i = 0; i < num_samples; i++) {
        float v = (float)voltage_samples[i] * ADC_SCALE;
        if (v > v_peak) v_peak = v;
    }

    if (v_peak < 1e-6f) {
        result->status = COMP_RESULT_SHORT;
        result->measured_farads = 0.0f;
        result->is_pass = false;
        return;
    }

    /* Find time constant: time for voltage to reach ~63.2% of peak */
    float tau_level = v_peak * TAU_FRACTION;
    float tau = 0.0f;
    for (uint16_t i = 0; i < num_samples; i++) {
        float v = (float)voltage_samples[i] * ADC_SCALE;
        if (v >= tau_level) {
            tau = (float)i / sample_rate;
            break;
        }
    }

    if (tau <= 0.0f) {
        result->status = COMP_RESULT_FAIL;
        result->is_pass = false;
        return;
    }

    /* C = tau / R_test */
    result->measured_farads = tau / CAP_TEST_R;

    /* ESR estimation: voltage step at start divided by current */
    float v0 = (float)voltage_samples[0] * ADC_SCALE;
    float i0 = (float)current_samples[0] * ADC_SCALE;
    if (fabsf_safe(i0) > 1e-9f) {
        result->measured_esr = fabsf_safe(v0 / i0);
    } else {
        result->measured_esr = 0.0f;
    }

    /* Compare to nominal if set */
    if (s_config.nominal_farads > 0.0f) {
        result->deviation_pct = fabsf_safe(result->measured_farads - s_config.nominal_farads)
                                / s_config.nominal_farads * 100.0f;
        result->is_pass = (result->deviation_pct <= 20.0f); /* Caps typically ±20% */
        result->status = result->is_pass ? COMP_RESULT_PASS : COMP_RESULT_FAIL;
    } else {
        result->deviation_pct = 0.0f;
        result->is_pass = true;
        result->status = COMP_RESULT_PASS;
    }
}

/* ------------------------------------------------------------------ */
/* Diode measurement                                                   */
/* ------------------------------------------------------------------ */

static void measure_diode(const int16_t *voltage_samples,
                          uint16_t num_samples,
                          comp_test_result_t *result)
{
    result->type = COMP_DIODE;

    float v_avg = samples_to_volts_avg(voltage_samples, num_samples);

    /* Auto-detect polarity */
    result->measured_vf = fabsf_safe(v_avg);

    /* Open / short classification */
    if (result->measured_vf < 0.01f) {
        result->status = COMP_RESULT_SHORT;
        result->is_pass = false;
        return;
    }
    if (result->measured_vf > 5.0f) {
        result->status = COMP_RESULT_OPEN;
        result->is_pass = false;
        return;
    }

    /* Check against max forward voltage */
    if (s_config.max_vf > 0.0f) {
        result->is_pass = (result->measured_vf <= s_config.max_vf);
        result->status = result->is_pass ? COMP_RESULT_PASS : COMP_RESULT_FAIL;
    } else {
        /* Default: silicon diode range 0.1 - 3.5V is reasonable */
        result->is_pass = (result->measured_vf >= 0.1f && result->measured_vf <= 3.5f);
        result->status = result->is_pass ? COMP_RESULT_PASS : COMP_RESULT_FAIL;
    }

    result->deviation_pct = 0.0f;
}

/* ------------------------------------------------------------------ */
/* Continuity measurement                                              */
/* ------------------------------------------------------------------ */

static void measure_continuity(const int16_t *voltage_samples,
                               const int16_t *current_samples,
                               uint16_t num_samples,
                               comp_test_result_t *result)
{
    result->type = COMP_CONTINUITY;

    float v_avg = samples_to_volts_avg(voltage_samples, num_samples);
    float i_avg = samples_to_volts_avg(current_samples, num_samples);

    if (fabsf_safe(i_avg) < 1e-9f) {
        result->measured_ohms = OPEN_THRESHOLD;
        result->status = COMP_RESULT_OPEN;
        result->is_pass = false;
        return;
    }

    float r = fabsf_safe(v_avg / i_avg);
    result->measured_ohms = r;

    if (r < SHORT_THRESHOLD) {
        result->status = COMP_RESULT_SHORT;
        result->is_pass = true; /* short = continuity */
    } else if (r >= OPEN_THRESHOLD) {
        result->status = COMP_RESULT_OPEN;
        result->is_pass = false;
    } else if (r <= CONTINUITY_THRESHOLD) {
        result->status = COMP_RESULT_PASS;
        result->is_pass = true;
    } else {
        result->status = COMP_RESULT_FAIL;
        result->is_pass = false;
    }

    result->deviation_pct = 0.0f;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void comp_test_init(void)
{
    memset(&s_config, 0, sizeof(s_config));
    s_config.type = COMP_RESISTOR;
    s_config.tolerance_pct = 5.0f;
    s_config.max_vf = 3.5f;
    s_initialized = true;
}

void comp_test_set_config(const comp_test_config_t *cfg)
{
    if (cfg) {
        s_config = *cfg;
    }
}

const comp_test_config_t *comp_test_get_config(void)
{
    return &s_config;
}

component_type_t comp_test_cycle_type(void)
{
    s_config.type = (component_type_t)((s_config.type + 1) % COMP_COUNT);
    return s_config.type;
}

void comp_test_measure(const int16_t *voltage_samples, const int16_t *current_samples,
                       uint16_t num_samples, float sample_rate,
                       comp_test_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->status = COMP_RESULT_MEASURING;

    if (!voltage_samples || num_samples == 0) {
        result->status = COMP_RESULT_FAIL;
        return;
    }

    switch (s_config.type) {
    case COMP_RESISTOR:
        measure_resistor(voltage_samples, current_samples, num_samples, result);
        break;
    case COMP_CAPACITOR:
        measure_capacitor(voltage_samples, current_samples, num_samples,
                          sample_rate, result);
        break;
    case COMP_DIODE:
        measure_diode(voltage_samples, num_samples, result);
        break;
    case COMP_CONTINUITY:
        measure_continuity(voltage_samples, current_samples, num_samples, result);
        break;
    default:
        result->status = COMP_RESULT_FAIL;
        break;
    }
}

bool comp_test_resistor(float measured_ohms, float nominal_ohms, float tolerance_pct)
{
    if (nominal_ohms <= 0.0f) return false;
    float dev = fabsf_safe(measured_ohms - nominal_ohms) / nominal_ohms * 100.0f;
    return dev <= tolerance_pct;
}

const char *comp_test_resistor_bands(float ohms)
{
    static char band_str[32];

    if (ohms <= 0.0f) {
        snprintf(band_str, sizeof(band_str), "---");
        return band_str;
    }

    /* Normalize to 2 significant digits + multiplier */
    int multiplier = 0;
    float val = ohms;

    while (val >= 100.0f) {
        val /= 10.0f;
        multiplier++;
    }
    while (val < 10.0f && multiplier > 0) {
        val *= 10.0f;
        multiplier--;
    }

    /* Round to nearest integer for 2-digit value */
    int digits = (int)(val + 0.5f);
    if (digits >= 100) {
        digits /= 10;
        multiplier++;
    }

    int d1 = digits / 10;
    int d2 = digits % 10;

    if (d1 < 0) d1 = 0;
    if (d1 > 9) d1 = 9;
    if (d2 < 0) d2 = 0;
    if (d2 > 9) d2 = 9;
    if (multiplier < 0) multiplier = 0;
    if (multiplier > 9) multiplier = 9;

    snprintf(band_str, sizeof(band_str), "%s-%s-%s",
             color_names[d1], color_names[d2], color_names[multiplier]);

    return band_str;
}
