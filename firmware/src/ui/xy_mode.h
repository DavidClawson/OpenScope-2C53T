/*
 * FNIRSI 2C53T XY Mode (Lissajous) Display
 *
 * Plots CH1 samples on X-axis vs CH2 samples on Y-axis.
 * Used for phase relationship visualization and Lissajous figures.
 */

#ifndef XY_MODE_H
#define XY_MODE_H

#include <stdint.h>

/* Render XY plot from two sample buffers.
 * ch1 = X axis, ch2 = Y axis.
 * Draws into display area (x_off, y_off) with given width/height.
 * Samples are int16_t (-32768 to 32767), mapped to display coordinates. */
void xy_render(const int16_t *ch1, const int16_t *ch2, uint16_t num_samples,
               uint16_t x_off, uint16_t y_off, uint16_t width, uint16_t height,
               uint16_t color);

/* Render XY with persistence (intensity accumulation).
 * persist_buf must be width * height bytes, caller-owned.
 * Increments hit pixels, applies decay, then renders intensity-mapped colors. */
void xy_render_persist(const int16_t *ch1, const int16_t *ch2, uint16_t num_samples,
                       uint8_t *persist_buf, uint16_t width, uint16_t height);

#endif /* XY_MODE_H */
