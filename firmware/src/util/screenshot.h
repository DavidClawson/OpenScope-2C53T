/*
 * OpenScope 2C53T - Screenshot Capture
 *
 * Maintains a shadow framebuffer that mirrors LCD pixel writes.
 * Can capture the display contents as a 16-bit BMP file (RGB565).
 *
 * NOTE: The shadow framebuffer uses 150KB of RAM (59% of the GD32F307's
 * 256KB SRAM). All screenshot functionality is behind FEATURE_SCREENSHOT
 * so it can be excluded from memory-constrained builds.
 */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>
#include <stdbool.h>

#define SCREENSHOT_WIDTH   320
#define SCREENSHOT_HEIGHT  240
#define SCREENSHOT_BPP     2     /* RGB565 = 2 bytes per pixel */
#define SCREENSHOT_SIZE    (SCREENSHOT_WIDTH * SCREENSHOT_HEIGHT * SCREENSHOT_BPP)

/* BMP file header size for 16-bit BMP with BI_BITFIELDS */
#define BMP_HEADER_SIZE    70    /* 14 file header + 40 info header + 16 masks */

/* Total BMP file size */
#define BMP_FILE_SIZE      (BMP_HEADER_SIZE + SCREENSHOT_SIZE)

/* Initialize screenshot subsystem (clears shadow framebuffer) */
void screenshot_init(void);

/* Update shadow framebuffer pixel (called from LCD driver) */
void screenshot_set_pixel(uint16_t x, uint16_t y, uint16_t color);

/* Fill rectangle in shadow framebuffer */
void screenshot_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* Clear shadow framebuffer to a single color */
void screenshot_clear(uint16_t color);

/* Capture current shadow framebuffer to a BMP file buffer.
 * Returns total size of BMP data written to buf, or 0 on failure.
 * buf must be at least BMP_FILE_SIZE bytes. */
uint32_t screenshot_capture_bmp(uint8_t *buf, uint32_t buf_size);

/* Get pointer to raw RGB565 framebuffer (for saving or streaming) */
const uint8_t *screenshot_get_framebuffer(void);

#endif /* SCREENSHOT_H */
