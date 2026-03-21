/*
 * FNIRSI 2C53T Trend Plot Display
 *
 * Graphs a measurement value (frequency, Vpp, Vrms, duty cycle) over time.
 * Shows how the selected parameter changes, with auto-scaled Y axis.
 */

#ifndef TREND_PLOT_H
#define TREND_PLOT_H

#include <stdint.h>

#define TREND_MAX_POINTS 320  /* One point per pixel column */

typedef enum {
    TREND_FREQUENCY = 0,
    TREND_VPP,
    TREND_VRMS,
    TREND_DUTY_CYCLE,
    TREND_COUNT
} trend_source_t;

typedef struct {
    float    values[TREND_MAX_POINTS];
    uint16_t num_points;          /* Total points added (saturates at TREND_MAX_POINTS) */
    uint16_t write_pos;           /* Circular buffer write position */
    float    min_val;             /* Auto-scaled Y axis minimum */
    float    max_val;             /* Auto-scaled Y axis maximum */
    trend_source_t source;        /* Which measurement is being tracked */
    float    sample_interval_s;   /* Time between trend points in seconds */
} trend_state_t;

/* Initialize trend plot state */
void trend_init(trend_state_t *state, trend_source_t source, float interval_s);

/* Add a new measurement data point */
void trend_add_point(trend_state_t *state, float value);

/* Render the trend plot into the given display area */
void trend_render(const trend_state_t *state,
                  uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                  uint16_t color, uint16_t grid_color);

/* Statistics across stored points */
float trend_get_min(const trend_state_t *state);
float trend_get_max(const trend_state_t *state);
float trend_get_avg(const trend_state_t *state);

/* Get human-readable name for a trend source */
const char *trend_source_name(trend_source_t source);

#endif /* TREND_PLOT_H */
