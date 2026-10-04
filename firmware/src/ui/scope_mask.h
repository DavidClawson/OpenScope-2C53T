/*
 * scope_mask — the firmware side of waveform pass/fail (mask) testing.
 *
 * Owns the one mask_pf_t, feeds it every committed acquisition record, and
 * holds the acquisition on a failure when asked to. Design note:
 * docs/specs/scope/mask-pass-fail.md.
 *
 * THREADING. Every mutation of the mask happens in the acquisition task:
 * scope_mask_service() (called once per loop iteration) applies requests
 * posted by the shell or the buttons, and scope_mask_on_commit() (called
 * right after each commit) teaches or tests the record just committed. That
 * makes each record judged exactly once, from the published buffers while no
 * other writer exists, and keeps all analysis off the display task's
 * capture-rate path -- inline analysis there killed acquisition once
 * (scope_ui.c, the 2026-08-14 badge revert note). Readers (display, shell)
 * take scope_mask_summary() copies; a torn counter costs one stale digit.
 *
 * In builds without the live acquisition loop nothing calls the service, so
 * requests are never acknowledged; scope_mask_post() callers see that.
 */
#ifndef SCOPE_MASK_H
#define SCOPE_MASK_H

#include <stdint.h>
#include <stdbool.h>
#include "mask_pf.h"

typedef enum {
    SCOPE_MASK_REQ_TEACH = 1,   /* a = channel bits (0 = enabled channels), n = frames */
    SCOPE_MASK_REQ_TOL,         /* a = tol_v counts, b = tol_h samples */
    SCOPE_MASK_REQ_CLEAR,
    SCOPE_MASK_REQ_RESET,       /* statistics only */
    SCOPE_MASK_REQ_RELEASE,     /* leave a stop-on-fail hold */
    SCOPE_MASK_REQ_STOPFAIL,    /* a = 0/1 */
} scope_mask_req_t;

/* Post a request; returns its sequence number. Applied by the acquisition
 * task within one loop iteration (~30 ms; ~10 ms while holding). */
uint32_t scope_mask_post(scope_mask_req_t kind, uint8_t a, uint8_t b, uint16_t n);
/* true once request `seq` has been applied. */
bool     scope_mask_acked(uint32_t seq);

/* Acquisition-task hooks. */
void scope_mask_service(void);
void scope_mask_on_commit(const volatile uint8_t *ch1, const volatile uint8_t *ch2,
                          bool triggered, bool time_ordered, uint32_t gen);
/* true while a failed record is being held (stop-on-fail): the acquisition
 * loop must not commit, so the failing record stays on the glass. */
bool scope_mask_hold_active(void);

/* Why the last TEACH request was refused before it began, or NULL. */
const char *scope_mask_teach_refusal(void);

typedef struct {
    mask_pf_state_t state;
    uint8_t  chans;
    int16_t  pre;
    uint8_t  tol_v, tol_h;
    bool     rail_limited, hold, stop_on_fail;
    uint16_t teach_got, teach_target;
    uint32_t teach_rejects;          /* total, all reasons */
    mask_pf_reason_t teach_last_reject;
    uint8_t  spread;
    uint32_t tested, passed, failed, skipped, missed;
    uint32_t max_consec_fail;
    mask_pf_result_t last, last_fail;
    mask_pf_cond_t cond;
} scope_mask_summary_t;

void scope_mask_summary(scope_mask_summary_t *s);
/* The mask itself, or NULL before the first teach (it is heap-allocated
 * then). Shell use only. */
const mask_pf_t *scope_mask_raw(void);
mask_pf_state_t  scope_mask_state(void);   /* EMPTY when not allocated */

/* Changes whenever something the on-screen readout shows changes. */
uint32_t scope_mask_epoch(void);

/* Per-column screen state for the overlay, for the frame currently in the
 * buffers. `x` is the screen column, `trig_x` where the hardware anchor
 * landed on the glass (scope_ui_trig_x_actual(), with anchor kind 2), `gen`
 * the frame generation being drawn. */
typedef enum {
    SCOPE_MASK_COL_NONE = 0,    /* no mask here / frame not judged */
    SCOPE_MASK_COL_PASS,        /* covered, frame passed */
    SCOPE_MASK_COL_FAIL_FRAME,  /* covered, frame failed elsewhere */
    SCOPE_MASK_COL_FAIL,        /* this column was outside the mask */
    SCOPE_MASK_COL_SKIP,        /* covered, frame refused */
    SCOPE_MASK_COL_TEACH        /* covered, teaching */
} scope_mask_col_t;
scope_mask_col_t scope_mask_column(uint8_t ch, int x, int trig_x, uint32_t gen);

/* Display support for drawing the mask itself. Both are false unless the
 * mask is READY and covers channel `ch`.
 *  - extent: lowest lo / highest hi over the whole span, so the autofit can
 *    keep the bounds on screen. Independent of the frame, so the scale does
 *    not jump when a record is refused.
 *  - bound_at: the bounds at screen column x when the trigger landed at
 *    column trig_x (the hardware anchor). */
bool scope_mask_extent(uint8_t ch, uint8_t *lo, uint8_t *hi);
bool scope_mask_bound_at(uint8_t ch, int x, int trig_x, uint8_t *lo, uint8_t *hi);

#endif /* SCOPE_MASK_H */
