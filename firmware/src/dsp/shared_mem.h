/*
 * Shared Memory Pool — Lifecycle-Managed
 *
 * 150KB buffer shared between features that never run simultaneously.
 * Features must explicitly claim the pool before use and release it
 * when done. The lifecycle manager tracks what's loaded and provides
 * diagnostic info (for the About screen and health monitoring).
 *
 * Design rules:
 *   1. Core features (scope time-domain, meter, siggen) never need the pool.
 *   2. On-demand features (FFT, persistence, screenshot, decode) claim it.
 *   3. Only one feature holds the pool at a time.
 *   4. Claiming auto-releases the previous holder (with notification).
 *   5. Screenshot is a brief claim-use-release cycle (~500ms).
 */

#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stdbool.h>

/* Which feature currently owns the shared pool */
typedef enum {
    SHMEM_OWNER_NONE = 0,
    SHMEM_OWNER_FFT,
    SHMEM_OWNER_PERSISTENCE,
    SHMEM_OWNER_SCREENSHOT,
    SHMEM_OWNER_DECODE,
    SHMEM_OWNER_COMPONENT,
    SHMEM_OWNER_BODE,
    SHMEM_OWNER_MODULE,      /* External loaded module */
    SHMEM_OWNER_COUNT
} shmem_owner_t;

/* Size of the shared pool (largest consumer = screenshot at 150KB) */
#define SHMEM_POOL_SIZE  153600

/* RAM sizes each feature actually needs (for diagnostics/budgeting) */
#define SHMEM_NEED_FFT          90112   /* 88 KB (radix-2) */
#define SHMEM_NEED_PERSISTENCE  65920   /* 320 * 206 = 64.4 KB */
#define SHMEM_NEED_SCREENSHOT   153600  /* 320 * 240 * 2 = 150 KB */
#define SHMEM_NEED_DECODE       32768   /* 32 KB */
#define SHMEM_NEED_COMPONENT    8192    /* 8 KB */
#define SHMEM_NEED_BODE         4096    /* 4 KB */

/* ═══════════════════════════════════════════════════════════════════
 * Core API
 * ═══════════════════════════════════════════════════════════════════ */

/* Acquire the pool for a feature. Previous owner's data is invalidated.
 * Returns pointer to the pool buffer. */
uint8_t *shared_mem_acquire(shmem_owner_t owner);

/* Release the pool. Safe to call if not owned. */
void shared_mem_release(void);

/* Check current owner */
shmem_owner_t shared_mem_owner(void);

/* Get pool pointer only if caller is the current owner (NULL otherwise) */
uint8_t *shared_mem_get(shmem_owner_t expected_owner);

/* Total pool size */
uint32_t shared_mem_size(void);

/* ═══════════════════════════════════════════════════════════════════
 * Lifecycle helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Check if the pool is currently free */
bool shared_mem_is_free(void);

/* Get the display name of the current owner (for diagnostics) */
const char *shared_mem_owner_name(void);

/* Get how many bytes the current owner needs (0 if free) */
uint32_t shared_mem_owner_need(void);

/* Get number of transitions (claims) since boot — useful for diagnostics */
uint32_t shared_mem_transition_count(void);

#endif /* SHARED_MEM_H */
