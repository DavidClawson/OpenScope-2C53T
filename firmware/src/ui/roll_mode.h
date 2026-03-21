/*
 * FNIRSI 2C53T Roll Mode Display
 *
 * Continuous scrolling waveform for slow signals.
 * No trigger — new samples appear at right edge and scroll left.
 */

#ifndef ROLL_MODE_H
#define ROLL_MODE_H

#include <stdint.h>
#include <stdbool.h>

#define ROLL_BUFFER_SIZE 320  /* One sample per pixel column */

typedef struct {
    int16_t  ch1[ROLL_BUFFER_SIZE];
    int16_t  ch2[ROLL_BUFFER_SIZE];
    uint16_t write_pos;       /* Circular buffer write position */
    uint16_t count;           /* Number of valid samples (up to ROLL_BUFFER_SIZE) */
    bool     ch1_enabled;
    bool     ch2_enabled;
    float    time_per_pixel;  /* Seconds per pixel (determines scroll speed) */
} roll_state_t;

/* Initialize roll mode state */
void roll_init(roll_state_t *state, float time_per_pixel);

/* Add a new sample pair (called at sample rate / decimation) */
void roll_add_sample(roll_state_t *state, int16_t ch1_val, int16_t ch2_val);

/* Render the roll display.
 * Draws scrolling waveform in the given display area.
 * framebuf/fb_width are for line drawing; pass NULL to use direct LCD calls. */
void roll_render(const roll_state_t *state,
                 uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
                 uint16_t ch1_color, uint16_t ch2_color,
                 uint16_t *framebuf, uint16_t fb_width);

/* Get the displayed time span in seconds */
float roll_get_time_span(const roll_state_t *state);

#endif /* ROLL_MODE_H */
