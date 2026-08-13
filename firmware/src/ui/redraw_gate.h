/*
 * OpenScope 2C53T — redraw gate (B2, 2026-08-13)
 *
 * WHY THIS EXISTS
 * ---------------
 * The display task wakes every 50 ms. Before this, the scope branch
 * repainted whenever *any* of three things was true — a popup was
 * counting down, the SPI3 acquisition counter had moved, or one second
 * had elapsed — and the one-second arm was unconditional: with nothing
 * connected, nothing changing and no user input, the scope screen still
 * ran `lcd_fill_rect(0, SCOPE_TOP, 320, SCOPE_H)` + a full repaint once
 * a second, forever. Worse, in the FFT / split / waterfall views and in
 * the cursors-on case, the *data* arm also took the clear-and-repaint
 * path, at the capture rate (~34 Hz on a live bench unit). That is the
 * "2-3 Hz full-repaint flash" recorded on 2026-08-12.
 *
 * The multimeter had exactly this bug and it was fixed on 2026-04-04 by
 * gating its redraw on `meter_reading.display_update_count` changing,
 * with a slow safety tick. That worked, so this is the same shape —
 * generalised, because the meter had one integer that summarised
 * everything it renders and the scope has ~20 fields. Here the caller
 * folds those fields into an "epoch" (any cheap hash; see
 * `redraw_epoch_*` below) and the gate compares it against the epoch of
 * the last completed draw. Same epoch + no new samples => the next draw
 * would produce identical pixels, so it is pure flicker and is skipped.
 *
 * DELIBERATELY NOT A DIRTY FLAG SET BY THE UI. The scope screen's inputs
 * are spread across scope_state, main.c's feature toggles, the theme and
 * the FFT config, and (as of 2026-08-13) the measurement badges and
 * FFT/waterfall content are being rewired to real capture data. A hash
 * over observable state cannot go stale when someone adds a renderer
 * input they forget to mark dirty in one of the twenty places that
 * mutate it; a hand-maintained flag can. The cost is one pass over ~20
 * scalars per 50 ms tick.
 *
 * PURE C99, NO FIRMWARE DEPENDENCIES — so `tests/test_redraw_gate.c`
 * can drive it on the host and assert real redraw counts. See
 * `make -C firmware test-redraw-gate`.
 */

#ifndef REDRAW_GATE_H
#define REDRAW_GATE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Epoch helpers (FNV-1a). Any hash works; this one is 3 instructions
 * per field and has no table.
 * ═══════════════════════════════════════════════════════════════════ */

#define REDRAW_EPOCH_SEED  2166136261u

static inline uint32_t redraw_epoch_mix(uint32_t h, uint32_t v)
{
    h ^= v;
    h *= 16777619u;
    return h;
}

/* Floats are mixed by bit pattern: we only ever ask "is this the same
 * value as last frame", never "is it close to". memcpy keeps it legal
 * under strict aliasing and compiles to a single move. */
static inline uint32_t redraw_epoch_mix_f(uint32_t h, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return redraw_epoch_mix(h, bits);
}

/* ═══════════════════════════════════════════════════════════════════
 * Gate
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    REDRAW_SKIP = 0,     /* nothing observable changed — do not touch the LCD */
    REDRAW_INCREMENTAL,  /* flicker-free update (scope: draw_scope_live_frame) */
    REDRAW_FULL          /* clear-and-repaint (scope: draw_scope_screen) */
} redraw_action_t;

typedef struct {
    uint32_t drawn_epoch;      /* epoch rendered by the last completed draw */
    uint32_t last_draw_frame;  /* tick index of the last completed draw */
    uint32_t last_full_frame;  /* tick index of the last FULL draw */
    bool     data_pending;     /* samples arrived that no draw has rendered yet */
    bool     primed;           /* this screen has been painted at least once */
} redraw_gate_t;

typedef struct {
    uint32_t frame;            /* display-task tick counter (50 ms/tick) */
    uint32_t epoch;            /* hash of everything this screen renders */
    bool     new_data;         /* a new capture landed since the last query */
    bool     force;            /* explicit invalidate (popup, mode entry) */
    bool     incremental_ok;   /* the flicker-free renderer can serve this frame */
    bool     animating;        /* content is a function of `frame` (demo trace) */

    /* All three are in display-task ticks; 0 disables that arm. */
    uint16_t full_min_frames;  /* floor between FULL repaints driven by data */
    uint16_t anim_tick_frames; /* cadence while `animating` */
    uint16_t idle_tick_frames; /* heartbeat when nothing else fires */
} redraw_query_t;

/* Force the next query to draw. Use on screen entry / hard invalidate. */
static inline void redraw_gate_invalidate(redraw_gate_t *g)
{
    g->primed = false;
}

/* Record a draw that happened outside the gate (a queued DCMD), so the
 * periodic path does not immediately repeat it. */
static inline void redraw_gate_mark(redraw_gate_t *g, uint32_t epoch,
                                    uint32_t frame, redraw_action_t action)
{
    if (action == REDRAW_SKIP) return;
    g->primed          = true;
    g->drawn_epoch     = epoch;
    g->last_draw_frame = frame;
    if (action == REDRAW_FULL) g->last_full_frame = frame;
    g->data_pending    = false;
}

/*
 * Decide what this tick owes the LCD, and latch the decision.
 *
 * Arms, in priority order:
 *   1. not primed / force        -> FULL. Screen entry, popups (whose
 *                                   countdown advances one step per draw).
 *   2. epoch moved               -> FULL. A user-visible setting changed;
 *                                   badges, markers and labels must repaint.
 *   3. data + incremental_ok     -> INCREMENTAL. The live trace, at the
 *                                   capture rate, with no clear. Unthrottled
 *                                   on purpose: this path never blanks.
 *   4. data, no incremental path -> FULL, but no more often than
 *                                   full_min_frames. FFT/waterfall/cursors
 *                                   still track the data; they just stop
 *                                   flashing at 34 Hz.
 *   5. animating                 -> FULL on anim_tick_frames. The synthetic
 *                                   demo waveform is a function of `frame`,
 *                                   so it genuinely has new pixels each tick.
 *   6. idle heartbeat            -> the slow backstop for anything a hash
 *                                   cannot see (bench overlay counters).
 *
 * `new_data` is sticky: if arm 4 throttles it, the pending flag survives
 * to the next tick instead of being silently dropped. (The pre-B2 code
 * consumed the counter outside the draw decision, so a skipped frame lost
 * the event entirely.)
 */
static inline redraw_action_t redraw_gate_step(redraw_gate_t *g,
                                               const redraw_query_t *q)
{
    if (q->new_data) g->data_pending = true;

    const uint32_t since_draw = q->frame - g->last_draw_frame;
    const uint32_t since_full = q->frame - g->last_full_frame;

    redraw_action_t action = REDRAW_SKIP;

    if (!g->primed || q->force) {
        action = REDRAW_FULL;
    } else if (q->epoch != g->drawn_epoch) {
        action = REDRAW_FULL;
    } else if (g->data_pending && q->incremental_ok) {
        action = REDRAW_INCREMENTAL;
    } else if (g->data_pending &&
               (q->full_min_frames == 0 || since_full >= q->full_min_frames)) {
        action = REDRAW_FULL;
    } else if (q->animating && q->anim_tick_frames != 0 &&
               since_draw >= q->anim_tick_frames) {
        action = REDRAW_FULL;
    } else if (q->idle_tick_frames != 0 && since_draw >= q->idle_tick_frames) {
        action = q->incremental_ok ? REDRAW_INCREMENTAL : REDRAW_FULL;
    }

    redraw_gate_mark(g, q->epoch, q->frame, action);
    return action;
}

#endif /* REDRAW_GATE_H */
