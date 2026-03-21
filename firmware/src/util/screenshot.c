/*
 * OpenScope 2C53T - Screenshot Capture
 *
 * See screenshot.h for interface documentation.
 *
 * All functionality is behind FEATURE_SCREENSHOT to avoid allocating
 * 150KB of shadow framebuffer on memory-constrained builds.
 */

#include "screenshot.h"
#include "shared_mem.h"
#include <string.h>

#ifdef FEATURE_SCREENSHOT

/* Shadow framebuffer uses shared memory pool (150KB = full pool) */
static uint8_t *shadow_fb = 0;
static bool initialized = false;

/* ========================================================================
 * Little-endian write helpers (BMP is always LE)
 * ======================================================================== */

static void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void write_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

/* ========================================================================
 * Public API
 * ======================================================================== */

void screenshot_init(void)
{
    shadow_fb = shared_mem_acquire(SHMEM_OWNER_SCREENSHOT);
    memset(shadow_fb, 0, SCREENSHOT_SIZE);
    initialized = true;
}

void screenshot_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (!initialized) return;
    if (x >= SCREENSHOT_WIDTH || y >= SCREENSHOT_HEIGHT) return;

    uint32_t offset = ((uint32_t)y * SCREENSHOT_WIDTH + x) * SCREENSHOT_BPP;
    shadow_fb[offset]     = (uint8_t)(color & 0xFF);
    shadow_fb[offset + 1] = (uint8_t)((color >> 8) & 0xFF);
}

void screenshot_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!initialized) return;

    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t hi = (uint8_t)((color >> 8) & 0xFF);

    for (uint16_t row = y; row < y + h && row < SCREENSHOT_HEIGHT; row++) {
        for (uint16_t col = x; col < x + w && col < SCREENSHOT_WIDTH; col++) {
            uint32_t offset = ((uint32_t)row * SCREENSHOT_WIDTH + col) * SCREENSHOT_BPP;
            shadow_fb[offset]     = lo;
            shadow_fb[offset + 1] = hi;
        }
    }
}

void screenshot_clear(uint16_t color)
{
    if (!initialized) {
        screenshot_init();
    }

    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t hi = (uint8_t)((color >> 8) & 0xFF);

    for (uint32_t i = 0; i < SCREENSHOT_SIZE; i += 2) {
        shadow_fb[i]     = lo;
        shadow_fb[i + 1] = hi;
    }
}

uint32_t screenshot_capture_bmp(uint8_t *buf, uint32_t buf_size)
{
    if (!buf || buf_size < BMP_FILE_SIZE) return 0;
    if (!initialized) return 0;

    uint32_t file_size = BMP_FILE_SIZE;

    /* Zero out the header area */
    memset(buf, 0, BMP_HEADER_SIZE);

    /* ---- BMP file header (14 bytes) ---- */
    buf[0] = 'B';
    buf[1] = 'M';
    write_le32(buf + 2, file_size);       /* File size */
    /* buf[6..9] = 0 (reserved) */
    write_le32(buf + 10, BMP_HEADER_SIZE); /* Pixel data offset */

    /* ---- DIB header: BITMAPINFOHEADER (40 bytes) ---- */
    write_le32(buf + 14, 40);             /* Header size */
    write_le32(buf + 18, SCREENSHOT_WIDTH);
    /* Negative height = top-down row order (matches our framebuffer layout) */
    write_le32(buf + 22, (uint32_t)(-(int32_t)SCREENSHOT_HEIGHT));
    write_le16(buf + 26, 1);              /* Planes */
    write_le16(buf + 28, 16);             /* Bits per pixel */
    write_le32(buf + 30, 3);              /* Compression: BI_BITFIELDS */
    write_le32(buf + 34, SCREENSHOT_SIZE); /* Image data size */
    /* buf[38..41] = 0 (X pixels per meter) */
    /* buf[42..45] = 0 (Y pixels per meter) */
    /* buf[46..49] = 0 (colors used) */
    /* buf[50..53] = 0 (important colors) */

    /* ---- Color masks for RGB565 (16 bytes) ---- */
    write_le32(buf + 54, 0xF800);         /* Red mask:   5 bits */
    write_le32(buf + 58, 0x07E0);         /* Green mask: 6 bits */
    write_le32(buf + 62, 0x001F);         /* Blue mask:  5 bits */
    write_le32(buf + 66, 0x0000);         /* Alpha mask: none */

    /* ---- Pixel data (direct copy of RGB565 framebuffer) ---- */
    memcpy(buf + BMP_HEADER_SIZE, shadow_fb, SCREENSHOT_SIZE);

    return file_size;
}

const uint8_t *screenshot_get_framebuffer(void)
{
    if (!initialized) return NULL;
    return shadow_fb;
}

#else /* !FEATURE_SCREENSHOT */

/* Stub implementations when screenshot feature is disabled */

void screenshot_init(void) {}
void screenshot_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    (void)x; (void)y; (void)color;
}
void screenshot_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void screenshot_clear(uint16_t color)
{
    (void)color;
}
uint32_t screenshot_capture_bmp(uint8_t *buf, uint32_t buf_size)
{
    (void)buf; (void)buf_size;
    return 0;
}
const uint8_t *screenshot_get_framebuffer(void)
{
    return (const uint8_t *)0;
}

#endif /* FEATURE_SCREENSHOT */
