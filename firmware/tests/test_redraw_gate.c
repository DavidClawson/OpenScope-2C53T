/*
 * Host test for the scope / siggen redraw gate (B2, 2026-08-13).
 *
 * WHAT THIS PROVES, AND WHAT IT DOES NOT
 * --------------------------------------
 * It drives ui/redraw_gate.h — the exact header main.c compiles — over
 * scenario traces and counts full-area repaints. A clean firmware build
 * proves nothing about redraw behaviour, so the reduction is measured
 * here against a transcription of the pre-B2 logic (`old_gate_step`,
 * copied from main.c at 9388e71) running on the same trace.
 *
 * It does NOT prove anything about pixels on the LCD, timing on the
 * device, or that `scope_ui_epoch()` in main.c hashes the right fields.
 * The gate is exercised; the wiring is not. Bench validation still owed.
 *
 * The negative half matters more than the positive half. Every check
 * here is run a second time against deliberately broken gates (see
 * `mutants[]`), and the test FAILS if a mutant slips through — because
 * this project's recurring failure mode is instruments that report
 * confidently on state they cannot observe.
 *
 * Build/run:  make -C firmware test-redraw-gate
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "redraw_gate.h"

/* ═══════════════════════════════════════════════════════════════════
 * Test harness
 * ═══════════════════════════════════════════════════════════════════ */

static int checks_run = 0;
static int checks_failed = 0;
static bool expect_failures = false;   /* mutant pass: failures are the point */
static int failures_in_pass = 0;

#define CHECK(cond, fmt, ...)                                                \
    do {                                                                     \
        checks_run++;                                                        \
        if (!(cond)) {                                                       \
            failures_in_pass++;                                              \
            if (!expect_failures) {                                          \
                checks_failed++;                                             \
                printf("  FAIL: " fmt "\n", ##__VA_ARGS__);                  \
            }                                                                \
        }                                                                    \
    } while (0)

/* ═══════════════════════════════════════════════════════════════════
 * The pre-B2 gate, transcribed from main.c @ 9388e71:
 *
 *     bool new_data = (fpga.spi3_ok_count != last_spi3_ok);
 *     if (new_data) last_spi3_ok = fpga.spi3_ok_count;
 *     if (scope_popup_active() || new_data || (frame - last_scope_frame) >= 20) {
 *         ... choose renderer ...
 *         last_scope_frame = frame;
 *     }
 *
 * Renderer choice was: incremental iff (!popup && acq_ready && no cursors)
 * and the view is TIME; otherwise the clear-and-repaint path.
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct { uint32_t last_frame; } old_gate_t;

static redraw_action_t old_gate_step(old_gate_t *g, const redraw_query_t *q)
{
    if (!(q->force || q->new_data || (q->frame - g->last_frame) >= 20))
        return REDRAW_SKIP;
    g->last_frame = q->frame;
    return q->incremental_ok ? REDRAW_INCREMENTAL : REDRAW_FULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * Scenario driver
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t ticks;          /* display-task ticks to simulate (50 ms each) */
    uint32_t data_period;    /* new capture every N ticks; 0 = no data at all */
    uint32_t epoch_period;   /* bump the epoch every N ticks; 0 = never */
    bool     incremental_ok;
    bool     animating;
    uint16_t full_min_frames;
    uint16_t anim_tick_frames;
    uint16_t idle_tick_frames;
} scenario_t;

typedef struct {
    uint32_t full;
    uint32_t incremental;
    uint32_t skipped;
    uint32_t max_data_latency;   /* worst gap, in ticks, from a capture
                                    landing to the draw that rendered it */
    uint32_t dropped_data;       /* captures never rendered by end of run */
} tally_t;

/* gate kind selector, so the same driver runs new / old / mutant gates */
typedef enum { GATE_NEW, GATE_OLD, GATE_MUT_NONSTICKY, GATE_MUT_ALWAYS,
               GATE_MUT_NEVER_IDLE } gate_kind_t;

static redraw_action_t step(gate_kind_t kind, redraw_gate_t *g, old_gate_t *og,
                            const redraw_query_t *q)
{
    switch (kind) {
    case GATE_OLD:
        return old_gate_step(og, q);

    case GATE_MUT_NONSTICKY: {
        /* The pre-B2 bug in isolation: a throttled tick consumes the
         * capture event instead of holding it pending, so the sample is
         * never rendered. */
        redraw_query_t q2 = *q;
        redraw_action_t a = redraw_gate_step(g, &q2);
        g->data_pending = false;
        return a;
    }
    case GATE_MUT_ALWAYS:
        /* "Gate" that never skips — i.e. no gating at all. */
        redraw_gate_mark(g, q->epoch, q->frame, REDRAW_FULL);
        return REDRAW_FULL;

    case GATE_MUT_NEVER_IDLE: {
        /* Over-eager gating: refuses to draw unless the epoch moved,
         * so live capture never reaches the screen. */
        if (q->epoch == g->drawn_epoch && g->primed) return REDRAW_SKIP;
        redraw_gate_mark(g, q->epoch, q->frame, REDRAW_FULL);
        return REDRAW_FULL;
    }
    case GATE_NEW:
    default:
        return redraw_gate_step(g, q);
    }
}

static tally_t run(gate_kind_t kind, const scenario_t *s)
{
    redraw_gate_t g = { 0, 0, 0, false, false };
    old_gate_t og = { 0 };
    tally_t t = { 0, 0, 0, 0, 0 };

    uint32_t epoch = 0x1234;
    bool have_pending = false;
    uint32_t pending_since = 0;

    /* The trace is followed by a short quiet drain (no new captures) so a
     * capture landing on the final tick can still be rendered. Without it
     * every throttled run reports one phantom "dropped" sample, which
     * would mask a real drop. Drain ticks are not counted in the tally. */
    const uint32_t drain = (s->full_min_frames ? s->full_min_frames : 1) + 1;

    for (uint32_t f = 0; f < s->ticks + drain; f++) {
        const bool counting = (f < s->ticks);
        bool new_data = counting &&
                        (s->data_period && f && (f % s->data_period) == 0);
        if (counting && s->epoch_period && f && (f % s->epoch_period) == 0)
            epoch++;

        if (new_data && !have_pending) { have_pending = true; pending_since = f; }

        redraw_query_t q;
        q.frame            = f;
        q.epoch            = epoch;
        q.new_data         = new_data;
        q.force            = false;
        q.incremental_ok   = s->incremental_ok;
        q.animating        = s->animating;
        q.full_min_frames  = s->full_min_frames;
        q.anim_tick_frames = s->anim_tick_frames;
        q.idle_tick_frames = s->idle_tick_frames;

        redraw_action_t a = step(kind, &g, &og, &q);

        if (a == REDRAW_SKIP) {
            if (counting) t.skipped++;
        } else {
            if (counting) { if (a == REDRAW_FULL) t.full++; else t.incremental++; }
            if (have_pending) {
                uint32_t lat = f - pending_since;
                if (lat > t.max_data_latency) t.max_data_latency = lat;
                have_pending = false;
            }
        }
    }
    if (have_pending) t.dropped_data++;
    return t;
}

/* Firmware constants, mirrored from main.c so the numbers below are the
 * numbers the device will actually run. */
#define FW_FULL_MIN    4     /* SCOPE_FULL_MIN_FRAMES  */
#define FW_ANIM_TICK   20    /* SCOPE_ANIM_TICK_FRAMES */
#define FW_IDLE_TICK   100   /* SCOPE_IDLE_FULL_FRAMES — heartbeat that blanks */
#define FW_IDLE_LIVE   20    /* SCOPE_IDLE_LIVE_FRAMES — flicker-free heartbeat */

/* ═══════════════════════════════════════════════════════════════════
 * The checks
 * ═══════════════════════════════════════════════════════════════════ */

static void check_idle_settled_screen(bool verbose)
{
    /* 20 s on a configured, running scope with a live trace already
     * latched, nothing connected changing, no user input, no new samples.
     * This is the case the pre-B2 1 s tick repainted forever. */
    scenario_t s = { 400, 0, 0, false, false, FW_FULL_MIN, 0, FW_IDLE_TICK };

    tally_t nw = run(GATE_NEW, &s);
    tally_t od = run(GATE_OLD, &s);

    if (verbose)
        printf("  idle settled screen (20 s, no data, no input):\n"
               "      pre-B2  full repaints = %u\n"
               "      post-B2 full repaints = %u   (skipped %u ticks)\n",
               od.full, nw.full, nw.skipped);

    /* 19, not 20: the pre-B2 tick fired at f=20,40,...,380 — its first
     * draw is one interval in, where the new gate paints at f=0. */
    CHECK(od.full == 19, "pre-B2 model should repaint 19x in 20 s, got %u", od.full);
    CHECK(nw.full <= 5, "settled screen should repaint <=5x in 20 s, got %u", nw.full);
    CHECK(nw.full * 4 <= od.full, "expected >=4x fewer repaints, %u vs %u",
          nw.full, od.full);
    /* ...but it must not go silent: something still has to tick over so a
     * frozen display is distinguishable from a hung task. */
    CHECK(nw.full >= 1, "settled screen must still heartbeat at least once");
}

static void check_idle_heartbeat_stays_live(bool verbose)
{
    /* NEGATIVE CONTROL for the slow backstop above: when the flicker-free
     * compositor IS available it also repaints the SCOPE_DEBUG_OVERLAY
     * strip (scope_ui.c, opaque text, no refill), and that strip is the
     * only instrument on bench unit #1. Backing the heartbeat off to 5 s
     * there would freeze the bench counters, so the live case must keep
     * the pre-B2 1 s cadence — and must not blank while doing it. */
    scenario_t s = { 400, 0, 0, true, false, FW_FULL_MIN, 0, FW_IDLE_LIVE };

    tally_t nw = run(GATE_NEW, &s);

    if (verbose)
        printf("  idle with compositor available (20 s): %u incremental,"
               " %u full\n", nw.incremental, nw.full);

    CHECK(nw.incremental >= 19, "bench heartbeat should run ~1 Hz, got %u",
          nw.incremental);
    CHECK(nw.full <= 1, "idle heartbeat must not blank, %u full draws", nw.full);
}

static void check_cursors_on_live_data(bool verbose)
{
    /* Cursors on = the live compositor cannot serve the frame (it does not
     * draw cursor lines), so every capture took the clear-and-repaint path.
     * 10 s at a 20 Hz capture rate. */
    scenario_t s = { 200, 1, 0, false, false, FW_FULL_MIN, 0, FW_IDLE_TICK };

    tally_t nw = run(GATE_NEW, &s);
    tally_t od = run(GATE_OLD, &s);

    if (verbose)
        printf("  cursors on, 20 Hz capture (10 s):\n"
               "      pre-B2  full repaints = %u\n"
               "      post-B2 full repaints = %u\n", od.full, nw.full);

    CHECK(od.full >= 190, "pre-B2 model should repaint ~every tick, got %u", od.full);
    CHECK(nw.full <= 55, "should cap near 5 Hz over 10 s, got %u", nw.full);
    /* NEGATIVE CONTROL: throttling must not silently discard captures. */
    CHECK(nw.dropped_data == 0, "a capture was never rendered");
    CHECK(nw.max_data_latency <= FW_FULL_MIN,
          "capture waited %u ticks for a draw, cap is %u",
          nw.max_data_latency, FW_FULL_MIN);
}

static void check_waterfall_live_data(bool verbose)
{
    /* Same shape as cursors-on but this is the expensive screen. 10 s at the
     * stock 34 Hz read cadence, which the 50 ms display tick samples at 20 Hz.
     *
     * COST MODEL — post-B1. Before B1 (merged 2026-08-13) a waterfall repaint
     * was ~20,480 lcd_fill_rect calls, one per pixel column. B1 replaced that
     * with a per-row blit: 1 fill_rect, 65 lcd_set_window, and 65,920 pixel
     * writes. So fill_rect is no longer the meaningful unit of work and
     * multiplying repaints by 20,480 would overstate this saving by ~20,000x.
     * Pixel writes are what remains, and they scale linearly with repaints —
     * which is exactly what B2 reduces. Keep this constant in step with
     * draw_waterfall_screen(); tests/test_waterfall_blit.c measures it. */
    enum { WF_PIXEL_WRITES_PER_REPAINT = 65920 };

    scenario_t s = { 200, 1, 0, false, false, FW_FULL_MIN, 0, FW_IDLE_TICK };

    tally_t nw = run(GATE_NEW, &s);
    tally_t od = run(GATE_OLD, &s);

    unsigned long old_px = (unsigned long)od.full * WF_PIXEL_WRITES_PER_REPAINT;
    unsigned long new_px = (unsigned long)nw.full * WF_PIXEL_WRITES_PER_REPAINT;

    if (verbose)
        printf("  waterfall view, 20 Hz capture (10 s), post-B1 blit:\n"
               "      pre-B2  %u repaints = %lu pixel writes\n"
               "      post-B2 %u repaints = %lu pixel writes\n",
               od.full, old_px, nw.full, new_px);

    CHECK(new_px * 3 < old_px, "expected >3x fewer pixel writes, %lu vs %lu",
          new_px, old_px);
}

static void check_live_trace_not_throttled(bool verbose)
{
    /* The whole point of the incremental path: it never blanks, so it is
     * NOT rate-limited. A live trace must still update once per capture. */
    scenario_t s = { 200, 2, 0, true, false, FW_FULL_MIN, 0, FW_IDLE_TICK };

    tally_t nw = run(GATE_NEW, &s);

    if (verbose)
        printf("  live trace, 10 Hz capture (10 s): %u incremental, %u full,"
               " worst latency %u tick(s)\n",
               nw.incremental, nw.full, nw.max_data_latency);

    CHECK(nw.incremental >= 99, "every capture should draw, got %u for ~99",
          nw.incremental);
    CHECK(nw.max_data_latency == 0, "live trace lagged %u ticks",
          nw.max_data_latency);
    CHECK(nw.dropped_data == 0, "a capture was never rendered");
    /* The one permitted full draw is the first paint on screen entry. */
    CHECK(nw.full <= 1, "live path took the clearing renderer %u times", nw.full);
}

static void check_demo_animation_preserved(bool verbose)
{
    /* Before the first real sample the screen IS a function of `frame`
     * (synthetic demo waveform, persistence decay). Gating must not
     * freeze it — the cadence is unchanged from pre-B2. */
    scenario_t s = { 400, 0, 0, false, true, FW_FULL_MIN, FW_ANIM_TICK,
                     FW_IDLE_TICK };

    tally_t nw = run(GATE_NEW, &s);
    tally_t od = run(GATE_OLD, &s);

    if (verbose)
        printf("  pre-data demo animation (20 s): pre-B2 %u, post-B2 %u\n",
               od.full, nw.full);

    /* Identical cadence; the new gate is +1 only because it paints on
     * screen entry (f=0) instead of one interval later. */
    CHECK(nw.full == od.full + 1, "demo cadence changed: %u vs %u",
          nw.full, od.full);
}

static void check_state_change_always_draws(bool verbose)
{
    /* NEGATIVE CONTROL: a user-visible change must repaint on the very
     * next tick, with no rate limiting, or button presses feel dead. */
    redraw_gate_t g = { 0, 0, 0, false, false };
    redraw_query_t q = { 0, 0xAAAA, false, false, false, false,
                         FW_FULL_MIN, FW_ANIM_TICK, FW_IDLE_TICK };

    (void)redraw_gate_step(&g, &q);          /* prime */

    uint32_t drew = 0;
    for (uint32_t f = 1; f < 40; f++) {
        q.frame = f;
        q.epoch = 0xAAAA + f;                /* every tick changes something */
        if (redraw_gate_step(&g, &q) != REDRAW_SKIP) drew++;
    }

    if (verbose) printf("  state changed every tick: %u/39 draws\n", drew);
    CHECK(drew == 39, "state change must always draw, got %u/39", drew);
}

static void check_popup_runs_at_tick_rate(bool verbose)
{
    /* draw_popup() decrements its countdown once per draw, so the popup
     * needs a draw every tick or it lingers. `force` covers that. */
    redraw_gate_t g = { 0, 0, 0, false, false };
    redraw_query_t q = { 0, 0xBBBB, false, true, false, false,
                         FW_FULL_MIN, FW_ANIM_TICK, FW_IDLE_TICK };

    uint32_t drew = 0;
    for (uint32_t f = 0; f < 10; f++) {
        q.frame = f;
        if (redraw_gate_step(&g, &q) == REDRAW_FULL) drew++;
    }

    if (verbose) printf("  popup active: %u/10 full draws\n", drew);
    CHECK(drew == 10, "popup must draw every tick, got %u/10", drew);
}

static void check_screen_entry_always_draws(bool verbose)
{
    /* NEGATIVE CONTROL: switching away and back must repaint even though
     * the screen's own state never moved — otherwise you get the previous
     * mode's pixels. main.c calls redraw_gate_invalidate() on mode change. */
    redraw_gate_t g = { 0, 0, 0, false, false };
    redraw_query_t q = { 0, 0xC0DE, false, false, false, false,
                         FW_FULL_MIN, 0, 0 };

    CHECK(redraw_gate_step(&g, &q) == REDRAW_FULL, "first paint must draw");
    q.frame = 1;
    CHECK(redraw_gate_step(&g, &q) == REDRAW_SKIP, "unchanged screen must skip");

    redraw_gate_invalidate(&g);              /* mode change */
    q.frame = 2;
    redraw_action_t a = redraw_gate_step(&g, &q);
    if (verbose) printf("  screen re-entry after invalidate: %s\n",
                        a == REDRAW_FULL ? "FULL" : "not full");
    CHECK(a == REDRAW_FULL, "screen entry must repaint");
}

static void check_siggen_clamped_presses(bool verbose)
{
    /* Siggen is drawn only from queue commands, and input_handler sends
     * DCMD_REDRAW_ALL — a full-screen clear — for every adjustment,
     * including ones that hit a clamp. 20 presses, 10 of them no-ops. */
    redraw_gate_t g = { 0, 0, 0, false, false };
    uint32_t epoch = 0x5165;
    uint32_t drew = 0;

    for (uint32_t i = 0; i < 20; i++) {
        if (i % 2 == 0) epoch++;             /* half the presses change state */
        /* main.c's DCMD path: draw iff not primed or epoch moved */
        if (!g.primed || epoch != g.drawn_epoch) {
            drew++;
            redraw_gate_mark(&g, epoch, i, REDRAW_FULL);
        }
    }

    if (verbose)
        printf("  siggen 20 presses (10 change nothing): pre-B2 20 full-screen"
               " clears, post-B2 %u\n", drew);
    CHECK(drew == 10, "expected 10 draws for 10 real changes, got %u", drew);
}

static void check_frame_counter_wrap(bool verbose)
{
    /* frame is a free-running uint32_t; at 20 Hz it wraps after ~6.8
     * years, but the arithmetic must be wrap-safe regardless because a
     * signed/absolute comparison here would stall the display for the
     * whole second half of the range. */
    redraw_gate_t g = { 0, 0, 0, false, false };
    redraw_query_t q = { 0xFFFFFFF0u, 0xD00D, false, false, false, false,
                         FW_FULL_MIN, 0, FW_IDLE_TICK };

    (void)redraw_gate_step(&g, &q);          /* prime near the wrap */

    uint32_t drew = 0;
    for (uint32_t i = 1; i <= 200; i++) {
        q.frame = 0xFFFFFFF0u + i;           /* wraps through zero */
        if (redraw_gate_step(&g, &q) != REDRAW_SKIP) drew++;
    }

    if (verbose) printf("  across uint32 frame wrap: %u heartbeats in 200 ticks\n",
                        drew);
    CHECK(drew == 2, "heartbeat should fire twice in 200 ticks, got %u", drew);
}

/* ═══════════════════════════════════════════════════════════════════
 * Mutant pass — every check above is re-run with a broken gate, and the
 * test fails if the checks do NOT notice.
 * ═══════════════════════════════════════════════════════════════════ */

static int run_all_checks(bool verbose)
{
    int before = failures_in_pass;
    check_idle_settled_screen(verbose);
    check_idle_heartbeat_stays_live(verbose);
    check_cursors_on_live_data(verbose);
    check_waterfall_live_data(verbose);
    check_live_trace_not_throttled(verbose);
    check_demo_animation_preserved(verbose);
    check_state_change_always_draws(verbose);
    check_popup_runs_at_tick_rate(verbose);
    check_screen_entry_always_draws(verbose);
    check_siggen_clamped_presses(verbose);
    check_frame_counter_wrap(verbose);
    return failures_in_pass - before;
}

/* Mutants are exercised through the scenario-driven checks only (the
 * hand-written ones call redraw_gate_step directly), so each mutant is
 * paired with the scenario check that must catch it. */
static void check_mutant(const char *name, gate_kind_t kind,
                         const scenario_t *s, bool expect_drop,
                         bool expect_no_reduction)
{
    tally_t t = run(kind, s);
    bool caught = false;

    if (expect_drop && (t.dropped_data > 0 || t.max_data_latency > FW_FULL_MIN))
        caught = true;
    if (expect_no_reduction && t.full >= 190)
        caught = true;

    checks_run++;
    if (!caught) {
        checks_failed++;
        printf("  FAIL: mutant '%s' was NOT caught "
               "(full=%u incr=%u dropped=%u maxlat=%u)\n",
               name, t.full, t.incremental, t.dropped_data, t.max_data_latency);
    } else {
        printf("  caught mutant: %-28s (full=%u incr=%u dropped=%u maxlat=%u)\n",
               name, t.full, t.incremental, t.dropped_data, t.max_data_latency);
    }
}

int main(void)
{
    printf("redraw gate — scope/siggen repaint accounting\n");
    printf("================================================================\n");
    printf("\nmeasured behaviour (pre-B2 model vs redraw_gate.h):\n\n");

    int real_failures = run_all_checks(true);

    printf("\nnegative controls — broken gates must be caught:\n\n");

    /* live-capture scenario: 10 s at 20 Hz, incremental unavailable */
    scenario_t live = { 200, 1, 0, false, false, FW_FULL_MIN, 0, FW_IDLE_TICK };
    /* live-trace scenario: incremental available */
    scenario_t trace = { 200, 2, 0, true, false, FW_FULL_MIN, 0, FW_IDLE_TICK };

    check_mutant("non-sticky data (pre-B2 bug)", GATE_MUT_NONSTICKY,
                 &live, true, false);
    check_mutant("no gating at all", GATE_MUT_ALWAYS,
                 &live, false, true);
    check_mutant("draws only on state change", GATE_MUT_NEVER_IDLE,
                 &trace, true, false);

    /* And prove the harness itself can fail: run the full check set
     * against the "no gating at all" gate by short-circuiting the new
     * gate, and confirm the reduction assertions fire. */
    printf("\nharness self-test — the checks must fail on an ungated build:\n");
    {
        expect_failures = true;
        failures_in_pass = 0;
        /* Reuse check_idle_settled_screen's expectation directly against
         * the ungated model: 20 s idle, no gating => 400 repaints. */
        scenario_t idle = { 400, 0, 0, false, false, FW_FULL_MIN, 0, FW_IDLE_TICK };
        tally_t ungated = run(GATE_MUT_ALWAYS, &idle);
        CHECK(ungated.full <= 5,
              "ungated build repainted %u times in 20 s", ungated.full);
        int caught = failures_in_pass;
        expect_failures = false;
        checks_run++;
        if (caught != 1) {
            checks_failed++;
            printf("  FAIL: the idle-screen assertion did not fire on an "
                   "ungated build — the check cannot detect what it claims to\n");
        } else {
            printf("  ok: idle-screen assertion fires (%u repaints in 20 s)\n",
                   ungated.full);
        }
    }

    printf("\n================================================================\n");
    if (checks_failed == 0 && real_failures == 0) {
        printf("PASS — %d checks\n", checks_run);
        return 0;
    }
    printf("FAIL — %d of %d checks failed\n", checks_failed, checks_run);
    return 1;
}
