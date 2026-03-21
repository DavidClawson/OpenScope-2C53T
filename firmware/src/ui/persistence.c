#include "persistence.h"
#include "shared_mem.h"
#include <string.h>

/* Persistence buffer uses shared memory pool (64KB of 150KB available) */
static uint8_t *persist_buf = 0;
static persist_mode_t current_mode = PERSIST_OFF;

void persist_init(void)
{
    persist_buf = shared_mem_acquire(SHMEM_OWNER_PERSISTENCE);
    memset(persist_buf, 0, PERSIST_WIDTH * PERSIST_HEIGHT);
    current_mode = PERSIST_OFF;
}

void persist_clear(void)
{
    if (persist_buf)
        memset(persist_buf, 0, PERSIST_WIDTH * PERSIST_HEIGHT);
}

void persist_set_mode(persist_mode_t mode)
{
    if (mode < PERSIST_COUNT) {
        current_mode = mode;
    }
}

persist_mode_t persist_get_mode(void)
{
    return current_mode;
}

void persist_add_trace(const uint16_t *y_values, uint8_t color_index)
{
    uint16_t x, y;
    (void)color_index; /* Color is applied at render time, not stored in buffer */

    for (x = 0; x < PERSIST_WIDTH; x++) {
        y = y_values[x];
        if (y >= PERSIST_HEIGHT) {
            continue;
        }

        /* Full intensity at the trace point */
        persist_buf[y * PERSIST_WIDTH + x] = 255;

        /* Anti-aliased thickness: neighboring rows at reduced intensity */
        if (y > 0) {
            uint8_t *p = &persist_buf[(y - 1) * PERSIST_WIDTH + x];
            if (*p < 192) {
                *p = 192;
            }
        }
        if (y + 1 < PERSIST_HEIGHT) {
            uint8_t *p = &persist_buf[(y + 1) * PERSIST_WIDTH + x];
            if (*p < 192) {
                *p = 192;
            }
        }
    }
}

void persist_decay(void)
{
    uint8_t decay;
    uint32_t i;
    uint32_t total = PERSIST_WIDTH * PERSIST_HEIGHT;

    switch (current_mode) {
    case PERSIST_OFF:
        /* Clear everything each frame */
        memset(persist_buf, 0, PERSIST_WIDTH * PERSIST_HEIGHT);
        return;
    case PERSIST_LOW:
        decay = 16;
        break;
    case PERSIST_MEDIUM:
        decay = 4;
        break;
    case PERSIST_HIGH:
        decay = 1;
        break;
    case PERSIST_INFINITE:
        /* No decay */
        return;
    default:
        return;
    }

    for (i = 0; i < total; i++) {
        if (persist_buf[i] > decay) {
            persist_buf[i] -= decay;
        } else {
            persist_buf[i] = 0;
        }
    }
}

uint8_t persist_get_intensity(uint16_t x, uint16_t y)
{
    if (x >= PERSIST_WIDTH || y >= PERSIST_HEIGHT) {
        return 0;
    }
    return persist_buf[y * PERSIST_WIDTH + x];
}

const uint8_t *persist_get_buffer(void)
{
    return persist_buf;
}

uint16_t persist_intensity_to_color_ch1(uint8_t intensity)
{
    if (intensity == 0) {
        return 0x0000; /* Black */
    }
    /* Dim yellow to bright yellow */
    uint8_t r = intensity >> 3;  /* 0-31 */
    uint8_t g = intensity >> 2;  /* 0-63 */
    return (uint16_t)(r << 11) | (uint16_t)(g << 5); /* RGB565 yellow */
}

uint16_t persist_intensity_to_color_ch2(uint8_t intensity)
{
    if (intensity == 0) {
        return 0x0000; /* Black */
    }
    /* Dim cyan to bright cyan */
    uint8_t g = intensity >> 2;  /* 0-63 */
    uint8_t b = intensity >> 3;  /* 0-31 */
    return (uint16_t)(g << 5) | b; /* RGB565 cyan */
}
