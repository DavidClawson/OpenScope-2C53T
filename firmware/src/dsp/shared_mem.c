/*
 * Shared Memory Pool
 *
 * 150KB buffer shared between FFT, persistence, screenshot, and decode.
 * Only one module can use it at a time. Acquiring for a new module
 * invalidates the previous owner's data.
 *
 * Before shared pool: FFT(88KB) + Persistence(64KB) + Screenshot(150KB) = 302KB
 * After shared pool:  max(88KB, 64KB, 150KB) = 150KB — saves 152KB of RAM.
 */

#include "shared_mem.h"
#include <string.h>

/* The shared buffer — 150KB in .bss */
static uint8_t pool[SHMEM_POOL_SIZE] __attribute__((aligned(4)));
static shmem_owner_t current_owner = SHMEM_OWNER_NONE;

uint8_t *shared_mem_acquire(shmem_owner_t owner)
{
    if (current_owner != owner) {
        /* New owner — zero the pool so stale data doesn't cause issues */
        memset(pool, 0, SHMEM_POOL_SIZE);
        current_owner = owner;
    }
    return pool;
}

void shared_mem_release(void)
{
    current_owner = SHMEM_OWNER_NONE;
}

shmem_owner_t shared_mem_owner(void)
{
    return current_owner;
}

uint8_t *shared_mem_get(shmem_owner_t expected_owner)
{
    if (current_owner == expected_owner)
        return pool;
    return (uint8_t *)0;  /* NULL — caller doesn't own the pool */
}

uint32_t shared_mem_size(void)
{
    return SHMEM_POOL_SIZE;
}
