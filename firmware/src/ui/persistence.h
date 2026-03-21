#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdint.h>
#include <stdbool.h>

#define PERSIST_WIDTH  320
#define PERSIST_HEIGHT 206   /* Display area between status bars (y=18 to y=223) */

typedef enum {
    PERSIST_OFF = 0,
    PERSIST_LOW,        /* Fast decay -- shows recent history */
    PERSIST_MEDIUM,     /* Moderate decay */
    PERSIST_HIGH,       /* Slow decay -- long history */
    PERSIST_INFINITE,   /* No decay -- accumulate forever */
    PERSIST_COUNT
} persist_mode_t;

/* Initialize persistence buffer */
void persist_init(void);

/* Clear persistence buffer */
void persist_clear(void);

/* Set persistence mode */
void persist_set_mode(persist_mode_t mode);

/* Get current mode */
persist_mode_t persist_get_mode(void);

/* Add a waveform trace to the persistence buffer.
 * y_values: array of PERSIST_WIDTH y-coordinates (0 to PERSIST_HEIGHT-1)
 * color_index: 0=CH1(yellow), 1=CH2(cyan) */
void persist_add_trace(const uint16_t *y_values, uint8_t color_index);

/* Apply decay to the persistence buffer (call once per frame) */
void persist_decay(void);

/* Get intensity at a pixel (0-255) */
uint8_t persist_get_intensity(uint16_t x, uint16_t y);

/* Get the persistence buffer pointer (for direct rendering) */
const uint8_t *persist_get_buffer(void);

/* Map intensity (0-255) to RGB565 color for CH1 (yellow) */
uint16_t persist_intensity_to_color_ch1(uint8_t intensity);

/* Map intensity (0-255) to RGB565 color for CH2 (cyan) */
uint16_t persist_intensity_to_color_ch2(uint8_t intensity);

#endif
