/*
 * Shared Memory Pool — Lifecycle-Managed
 *
 * 96KB buffer shared between features that never run simultaneously.
 * Features must explicitly claim the pool before use and release it
 * when done. The lifecycle manager tracks what's loaded and provides
 * diagnostic info (for the About screen and health monitoring).
 *
 * Design rules:
 *   1. Core signal paths (scope time-domain, DMM sampling, siggen) do not
 *      depend on the pool.
 *   2. On-demand features (FFT, persistence, screenshot, decode) claim it.
 *   3. Display code may opportunistically claim it for an off-screen tile, but
 *      must fall back to direct drawing when another feature owns the pool.
 *   4. Only one feature holds the pool at a time.
 *   5. Claiming auto-releases the previous holder (with notification).
 *   6. Screenshot is a brief claim-use-release cycle (~500ms).
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
    SHMEM_OWNER_DISPLAY,
    SHMEM_OWNER_COUNT
} shmem_owner_t;

/* Size of the shared pool. Keep this above the largest enabled consumer.
 * The old RGB565 screenshot utility needs 150KB and must fail closed unless
 * this pool is deliberately restored to that size. */
#ifndef SHMEM_POOL_SIZE
#define SHMEM_POOL_SIZE  118784
#endif

/* RAM sizes each feature actually needs (for diagnostics/budgeting) */
#define SHMEM_NEED_FFT          90112   /* 88 KB (radix-2) */
#define SHMEM_NEED_PERSISTENCE  65920   /* 320 * 206 = 64.4 KB */
#define SHMEM_NEED_SCREENSHOT   153600  /* legacy RGB565 screenshot */
#define SHMEM_NEED_DECODE       32768   /* 32 KB */
#define SHMEM_NEED_COMPONENT    8192    /* 8 KB */
#define SHMEM_NEED_BODE         4096    /* 4 KB */
#define SHMEM_NEED_DISPLAY      44400   /* Meter waveform panel: 300 * 74 * RGB565 */

/* ── FFT sub-tenants ────────────────────────────────────────────────
 *
 * The waterfall is not a pool owner in its own right: draw_waterfall_screen()
 * calls fft_process(), so it can only ever run while SHMEM_OWNER_FFT holds
 * the pool. Claiming it separately would evict the very FFT it depends on.
 * Instead its history buffer is a sub-tenant of the FFT region, placed above
 * everything fft_setup_pointers() lays down.
 *
 * ⚠ The offset is NOT SHMEM_NEED_FFT. That constant says 90112 (88 KB), but
 * the radix-2 layout actually runs to 98304 (96 KB):
 *
 *     CMSIS   : FFT_SIZE*14 + FFT_BINS*12 = 81920
 *     radix-2 : FFT_SIZE*18 + FFT_BINS*12 = 98304   <-- the real high-water mark
 *
 * SHMEM_NEED_FFT understating the radix-2 path is a pre-existing inaccuracy;
 * it is harmless today only because every build defines USE_CMSIS_DSP. Placing
 * the waterfall at 90112 would have silently overlapped the twiddle and sample
 * buffers the moment anyone built the radix-2 path. fft.c carries a
 * _Static_assert tying its real layout to this offset so the build breaks
 * instead of corrupting, e.g. if FFT_SIZE is ever raised.
 *
 * Lifetime rules for a sub-tenant:
 *   - only valid while shared_mem_owner() == SHMEM_OWNER_FFT;
 *   - shared_mem_acquire() zeroes the pool on an owner *change*, so history
 *     is wiped automatically after an eviction — callers must notice via
 *     shared_mem_transition_count() and reset their own indices.
 */
#define SHMEM_FFT_WATERFALL_OFFSET  98304
#define SHMEM_FFT_WATERFALL_ROWS    64
#define SHMEM_FFT_WATERFALL_COLS    320
#define SHMEM_FFT_WATERFALL_SIZE    (SHMEM_FFT_WATERFALL_ROWS * SHMEM_FFT_WATERFALL_COLS)

/* Total the FFT owner actually occupies, including sub-tenants. This is the
 * figure the About/health screens should report, not SHMEM_NEED_FFT. */
#define SHMEM_NEED_FFT_TOTAL \
    (SHMEM_FFT_WATERFALL_OFFSET + SHMEM_FFT_WATERFALL_SIZE)

_Static_assert(SHMEM_NEED_FFT_TOTAL <= SHMEM_POOL_SIZE,
               "FFT region + waterfall sub-tenant overflows the shared pool");

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
