/*
 * Shared Memory Pool — Lifecycle-Managed
 *
 * 150KB buffer in BSS, shared between on-demand features.
 * Tracks ownership, transition count, and provides diagnostics.
 */

#include "shared_mem.h"
#include <string.h>

/* The shared buffer — 150KB in .bss */
static uint8_t pool[SHMEM_POOL_SIZE] __attribute__((aligned(4)));
static shmem_owner_t current_owner = SHMEM_OWNER_NONE;
static uint32_t transitions = 0;

/* Owner names for diagnostics */
static const char *owner_names[SHMEM_OWNER_COUNT] = {
    "Free",
    "FFT",
    "Persistence",
    "Screenshot",
    "Decode",
    "Component",
    "Bode",
    "Module",
};

/* RAM needs per owner */
static const uint32_t owner_needs[SHMEM_OWNER_COUNT] = {
    0,
    SHMEM_NEED_FFT,
    SHMEM_NEED_PERSISTENCE,
    SHMEM_NEED_SCREENSHOT,
    SHMEM_NEED_DECODE,
    SHMEM_NEED_COMPONENT,
    SHMEM_NEED_BODE,
    0, /* Module — variable, not known at compile time */
};

/* ═══════════════════════════════════════════════════════════════════
 * Core API
 * ═══════════════════════════════════════════════════════════════════ */

uint8_t *shared_mem_acquire(shmem_owner_t owner)
{
    if (current_owner != owner) {
        /* New owner — zero the pool so stale data doesn't cause issues */
        memset(pool, 0, SHMEM_POOL_SIZE);
        current_owner = owner;
        transitions++;
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
    return (uint8_t *)0;
}

uint32_t shared_mem_size(void)
{
    return SHMEM_POOL_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════
 * Lifecycle helpers
 * ═══════════════════════════════════════════════════════════════════ */

bool shared_mem_is_free(void)
{
    return current_owner == SHMEM_OWNER_NONE;
}

const char *shared_mem_owner_name(void)
{
    if (current_owner < SHMEM_OWNER_COUNT)
        return owner_names[current_owner];
    return "Unknown";
}

uint32_t shared_mem_owner_need(void)
{
    if (current_owner < SHMEM_OWNER_COUNT)
        return owner_needs[current_owner];
    return 0;
}

uint32_t shared_mem_transition_count(void)
{
    return transitions;
}
