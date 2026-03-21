/*
 * Shared Memory Pool for OpenScope 2C53T
 *
 * Large buffers that are never used simultaneously share the same
 * physical RAM via a union. This saves ~150KB of RAM compared to
 * allocating all buffers independently.
 *
 * Usage: Call shared_mem_acquire() before using a pool. If another
 * module holds the pool, its data is invalidated.
 *
 * Pool contents:
 *   FFT:         88KB (fft_buf + window + magnitude + avg + max_hold)
 *   PERSISTENCE: 64KB (320x206 intensity buffer)
 *   SCREENSHOT: 150KB (320x240 RGB565 shadow framebuffer)
 *   DECODE:      32KB (sample buffer for protocol decoding)
 */

#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stdbool.h>

/* Which module currently owns the shared pool */
typedef enum {
    SHMEM_OWNER_NONE = 0,
    SHMEM_OWNER_FFT,
    SHMEM_OWNER_PERSISTENCE,
    SHMEM_OWNER_SCREENSHOT,
    SHMEM_OWNER_DECODE,
} shmem_owner_t;

/* Size of the shared pool (largest consumer = screenshot at 150KB) */
#define SHMEM_POOL_SIZE  153600

/* Acquire the shared memory pool for a module.
 * If another module held it, their data is lost.
 * Returns pointer to the pool. */
uint8_t *shared_mem_acquire(shmem_owner_t owner);

/* Release the pool (optional — acquire auto-releases previous owner) */
void shared_mem_release(void);

/* Check who currently owns the pool */
shmem_owner_t shared_mem_owner(void);

/* Get pool pointer without changing ownership (NULL if not owned by caller) */
uint8_t *shared_mem_get(shmem_owner_t expected_owner);

/* Get pool size */
uint32_t shared_mem_size(void);

#endif /* SHARED_MEM_H */
