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

/* FFT_SIZE / FFT_BINS — SHMEM_NEED_FFT is derived from them below so the
 * diagnostic figure cannot drift from the layout fft.c actually lays down.
 * fft.h is macros + typedefs only; it does not include this header back. */
#include "fft.h"

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
 * this pool is deliberately restored to that size.
 *
 * Since the 2026-11-11 RAM diet the default is DERIVED: exactly the FFT
 * owner's footprint (SHMEM_NEED_FFT_TOTAL, below), which is the largest
 * consumer that actually claims the pool (FFT 100 KB under CMSIS, persistence
 * 64.4 KB; screenshot fails closed). It used to be a flat 118784, the radix-2
 * footprint, which under USE_CMSIS_DSP (every build) left 16 KB that nothing
 * could use. Static asserts at the end of this block keep every claimant
 * inside it. */
#ifndef SHMEM_POOL_SIZE
#define SHMEM_POOL_SIZE  SHMEM_NEED_FFT_TOTAL
#endif

/* RAM sizes each feature actually needs (for diagnostics/budgeting) */

/* FFT need — DERIVED, not a hand-maintained number.
 *
 * This constant used to read a flat 90112 ("88 KB (radix-2)") and was wrong for
 * BOTH layouts: it was the radix-2 layout minus the 8 KB sample_buf that
 * fft_setup_pointers() places at the top. The real high-water marks are
 *
 *     CMSIS   : FFT_SIZE*14 + FFT_BINS*12 = 81920  (80 KB)
 *     radix-2 : FFT_SIZE*18 + FFT_BINS*12 = 98304  (96 KB)
 *
 * decomposed as: fft_buf FFT_SIZE*8, window FFT_SIZE*4, magnitude+avg+max_hold
 * FFT_BINS*12, sample_buf FFT_SIZE*2, and (radix-2 only) twiddle_re+im
 * FFT_SIZE*4. fft.c carries a _Static_assert pinning FFT_POOL_LAYOUT_END ==
 * SHMEM_NEED_FFT, so changing the layout without changing this breaks the build
 * rather than silently making the About/health screens report fiction. */
#ifdef USE_CMSIS_DSP
#define SHMEM_NEED_FFT          (FFT_SIZE * 14 + FFT_BINS * 12)   /* 80 KB */
#else
#define SHMEM_NEED_FFT          (FFT_SIZE * 18 + FFT_BINS * 12)   /* 96 KB */
#endif
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
 * The offset is SHMEM_NEED_FFT: the waterfall starts where the FFT layout
 * ends (81920 under USE_CMSIS_DSP, 98304 radix-2). It used to be a FIXED 98304
 * so the address "would not change with the DSP backend", which bought 16 KB
 * of permanently unused slack in every CMSIS build. That reason does not hold:
 * the backend is a compile-time choice, every translation unit sees the same
 * USE_CMSIS_DSP (Makefile C_DEFS), and the pool is .bss that is zeroed on
 * every owner change and never outlives a reflash, so no stored history can
 * ever be read back at a different offset. fft.c's _Static_asserts tie the
 * real layout end to this offset (FFT_POOL_LAYOUT_END == SHMEM_NEED_FFT and
 * <= the offset), so raising FFT_SIZE still breaks the build instead of
 * overlapping the sample buffer with the waterfall (RAM diet, 2026-11-11).
 *
 * Lifetime rules for a sub-tenant:
 *   - only valid while shared_mem_owner() == SHMEM_OWNER_FFT;
 *   - shared_mem_acquire() zeroes the pool on an owner *change*, so history
 *     is wiped automatically after an eviction — callers must notice via
 *     shared_mem_transition_count() and reset their own indices.
 */
#define SHMEM_FFT_WATERFALL_OFFSET  SHMEM_NEED_FFT
#define SHMEM_FFT_WATERFALL_ROWS    64
#define SHMEM_FFT_WATERFALL_COLS    320
#define SHMEM_FFT_WATERFALL_SIZE    (SHMEM_FFT_WATERFALL_ROWS * SHMEM_FFT_WATERFALL_COLS)

/* Total pool FOOTPRINT of the FFT owner, including the waterfall sub-tenant:
 * 102400 under CMSIS, 118784 radix-2. This is the figure the About/health
 * screens report. */
#define SHMEM_NEED_FFT_TOTAL \
    (SHMEM_FFT_WATERFALL_OFFSET + SHMEM_FFT_WATERFALL_SIZE)

_Static_assert(SHMEM_NEED_FFT_TOTAL <= SHMEM_POOL_SIZE,
               "FFT region + waterfall sub-tenant overflows the shared pool");
_Static_assert((SHMEM_FFT_WATERFALL_OFFSET % 4) == 0,
               "waterfall sub-tenant must stay word-aligned in the pool");
/* The other claimants must fit too, now that the pool is no bigger than FFT
 * needs. Screenshot (153600) is NOT asserted: it checks shared_mem_size() and
 * fails closed, by design (see above). */
_Static_assert(SHMEM_NEED_PERSISTENCE <= SHMEM_POOL_SIZE,
               "persistence buffer no longer fits the shared pool");
_Static_assert(SHMEM_NEED_DISPLAY <= SHMEM_POOL_SIZE &&
               SHMEM_NEED_DECODE <= SHMEM_POOL_SIZE &&
               SHMEM_NEED_COMPONENT <= SHMEM_POOL_SIZE &&
               SHMEM_NEED_BODE <= SHMEM_POOL_SIZE,
               "a declared pool claimant no longer fits the shared pool");

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
