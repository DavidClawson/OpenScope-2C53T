#ifndef ALTERNATOR_TEST_H
#define ALTERNATOR_TEST_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ALT_GOOD = 0,          /* Normal ripple pattern */
    ALT_DIODE_FAULT,       /* Missing phase = bad diode */
    ALT_BEARING_NOISE,     /* Irregular pattern */
    ALT_WEAK_OUTPUT,       /* Low overall output */
    ALT_NOT_CHARGING,      /* No AC component */
} alternator_status_t;

typedef struct {
    alternator_status_t status;
    float    ripple_mv;          /* Peak-to-peak AC ripple in mV */
    float    dc_voltage;         /* Average DC level */
    float    frequency_hz;       /* Ripple frequency (should be 3x RPM for 3-phase) */
    uint8_t  diodes_working;     /* Estimated: 0-6 */
    bool     is_charging;        /* DC > battery voltage */
    char     diagnosis[32];      /* Human-readable diagnosis */
} alternator_result_t;

/* Analyze battery voltage samples for alternator health.
 * samples: AC-coupled battery voltage (after high-pass filter)
 * or raw battery voltage (DC + ripple) */
void alternator_analyze(const int16_t *samples, uint32_t num_samples,
                        float sample_rate, float voltage_scale,
                        alternator_result_t *result);

#endif
