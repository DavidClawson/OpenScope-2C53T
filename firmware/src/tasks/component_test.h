#ifndef COMPONENT_TEST_H
#define COMPONENT_TEST_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    COMP_RESISTOR = 0,
    COMP_CAPACITOR,
    COMP_DIODE,
    COMP_CONTINUITY,
    COMP_COUNT
} component_type_t;

typedef enum {
    COMP_RESULT_PASS = 0,
    COMP_RESULT_FAIL,
    COMP_RESULT_OPEN,
    COMP_RESULT_SHORT,
    COMP_RESULT_MEASURING,
} comp_result_status_t;

typedef struct {
    component_type_t type;
    /* Resistor settings */
    float nominal_ohms;      /* Expected value */
    float tolerance_pct;     /* Tolerance band (1, 2, 5, 10, 20%) */
    /* Capacitor settings */
    float nominal_farads;    /* Expected capacitance */
    /* Diode settings */
    float max_vf;            /* Maximum forward voltage */
} comp_test_config_t;

typedef struct {
    comp_result_status_t status;
    component_type_t type;
    /* Measured values */
    float measured_ohms;     /* Resistor: measured resistance */
    float measured_farads;   /* Capacitor: measured capacitance */
    float measured_esr;      /* Capacitor: ESR in ohms */
    float measured_vf;       /* Diode: forward voltage */
    float deviation_pct;     /* How far from nominal (%) */
    bool  is_pass;           /* Within tolerance? */
} comp_test_result_t;

/* Initialize component tester */
void comp_test_init(void);

/* Set test configuration */
void comp_test_set_config(const comp_test_config_t *cfg);

/* Run a component test on sample data.
 * voltage_samples: ADC voltage across component
 * current_samples: ADC current through component (or derived)
 * For resistor: R = V/I from known test current
 * For capacitor: measure charge/discharge time constant
 * For diode: measure forward voltage drop */
void comp_test_measure(const int16_t *voltage_samples, const int16_t *current_samples,
                       uint16_t num_samples, float sample_rate,
                       comp_test_result_t *result);

/* Convenience: resistor pass/fail test */
bool comp_test_resistor(float measured_ohms, float nominal_ohms, float tolerance_pct);

/* Convenience: decode resistor color bands from value */
const char *comp_test_resistor_bands(float ohms);

/* Get config */
const comp_test_config_t *comp_test_get_config(void);

/* Cycle component type */
component_type_t comp_test_cycle_type(void);

#endif
