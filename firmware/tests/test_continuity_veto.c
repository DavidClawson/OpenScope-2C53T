/*
 * cont_veto_step(): when the decoded reading may mute the PC3 tone
 * (continuity_veto.h, review of #102).
 *
 * A millisecond simulation of the probes, the SoC's tone and the meter's
 * frames: one frame per 163 ms, each showing the probes as they were `lag`
 * ms before it arrived. Every scenario runs at five lags (300 ms to the
 * longest measured, 1012 ms) and four frame phases.
 *
 * Negative controls, each of which must get a scenario wrong:
 *   - the gate as #102 first posted it (9d1d135): the first new frame at or
 *     above the threshold mutes until the tone stops;
 *   - the latest frame decides from the onset, no ignore window;
 *   - the ignore window, but latching.
 *
 * What it does not see: the real SoC threshold (60 Ohm here, only "above
 * 47 Ohm" is measured), frames that arrive irregularly, readings between
 * the probes' states (EXP-93 saw none), and the buzzer itself.
 */
#include <stdio.h>
#include "continuity_veto.h"

#define FRAME_MS    163U
#define HW_OHM      60.0       /* the SoC sounds its tone below this */
#define USER_OHM    30.0       /* "Beep below 30 Ohm", the default */
#define OPEN        1e12
#define HOLD_MS     105U       /* CONT_DETECT_HOLD_MS */

static const uint32_t lags[]   = { 300, 475, 676, 850, 1012 };
static const uint32_t phases[] = { 0, 41, 82, 122 };
#define N_LAG   (int)(sizeof lags / sizeof lags[0])
#define N_PHASE (int)(sizeof phases / sizeof phases[0])

typedef struct { uint32_t t; double ohm; } seg_t;          /* from t on */
typedef struct { uint32_t from, to; bool beep; } want_t;   /* [from, to) */

typedef struct {
    const char *what;
    uint32_t end;
    seg_t seg[8];
    int nseg;
    want_t want[4];
    int nwant;
} scen_t;

static const scen_t scen[] = {
    { "short from open", 6000,
      { {0, OPEN}, {2000, 0.2}, {5000, OPEN} }, 3,
      { {0, 2000, false}, {2000, 5000, true}, {5000 + HOLD_MS, 6000, false} }, 3 },
    /* The review's case: the frames still show the 1 kOhm part for up to a
     * second after the tone starts. */
    { "1 kOhm part, slid onto a short", 7000,
      { {0, OPEN}, {500, 1000.0}, {3000, 0.2}, {6000, OPEN} }, 4,
      { {500, 3000, false}, {3000, 6000, true} }, 2 },
    { "1 kOhm part, lifted, tapped a short", 6000,
      { {0, OPEN}, {500, 1000.0}, {3000, OPEN}, {3200, 0.2}, {5000, OPEN} }, 5,
      { {3200, 5000, true} }, 1 },
    /* Between the thresholds: may beep through the ignore window, then quiet. */
    { "47 Ohm part from open", 6000,
      { {0, OPEN}, {2000, 47.0}, {6000, OPEN} }, 3,
      { {2000 + 1400, 6000, false} }, 1 },
    /* The tone never stops, so only a veto that follows the frames clears. */
    { "47 Ohm part, slid onto a short", 8000,
      { {0, OPEN}, {1000, 47.0}, {4000, 0.2}, {7000, OPEN} }, 4,
      { {1000 + 1400, 4000, false}, {4000 + 1200, 7000, true} }, 2 },
    { "short, slid onto a 47 Ohm part", 8000,
      { {0, OPEN}, {1000, 0.2}, {4000, 47.0}, {7000, OPEN} }, 4,
      { {1000, 4000, true}, {4000 + 1200, 7000, false} }, 2 },
    { "three quick taps on a short", 3000,
      { {0, OPEN}, {1000, 0.2}, {1300, OPEN}, {1600, 0.2}, {1900, OPEN},
        {2200, 0.2}, {2500, OPEN} }, 7,
      { {1000, 1300, true}, {1600, 1900, true}, {2200, 2500, true} }, 3 },
};
#define N_SCEN (int)(sizeof scen / sizeof scen[0])

/* A gate under test: started at the tone's onset, stepped every ms. */
typedef struct { cont_veto_t v; uint32_t upd0; } gstate_t;
typedef struct {
    const char *name;
    void (*start)(gstate_t *, uint32_t upd);
    bool (*step)(gstate_t *, uint32_t ms, uint32_t upd, bool above);
} gate_t;

static void real_start(gstate_t *g, uint32_t upd) { cont_veto_start(&g->v, upd); }
static bool real_step(gstate_t *g, uint32_t ms, uint32_t upd, bool above)
{
    return cont_veto_step(&g->v, ms, upd, above);
}

/* 9d1d135: the first new frame above the threshold mutes, until release. */
static void old_start(gstate_t *g, uint32_t upd) { g->v.muted = false; g->upd0 = upd; }
static bool old_step(gstate_t *g, uint32_t ms, uint32_t upd, bool above)
{
    (void)ms;
    if (!g->v.muted && upd != g->upd0 && above)
        g->v.muted = true;
    return g->v.muted;
}

/* Follows the latest frame from the onset on. */
static bool nowin_step(gstate_t *g, uint32_t ms, uint32_t upd, bool above)
{
    (void)ms;
    if (upd != g->v.last_upd) {
        g->v.last_upd = upd;
        g->v.muted = above;
    }
    return g->v.muted;
}

/* Ignores the window, then latches. */
static bool latch_step(gstate_t *g, uint32_t ms, uint32_t upd, bool above)
{
    if (upd != g->v.last_upd) {
        g->v.last_upd = upd;
        if (ms >= CONT_VETO_IGNORE_MS && above)
            g->v.muted = true;
    }
    return g->v.muted;
}

static const gate_t gates[] = {
    { "cont_veto_step",                 real_start, real_step  },
    { "control: as first posted",       old_start,  old_step   },
    { "control: no ignore window",      real_start, nowin_step },
    { "control: window, but latching",  real_start, latch_step },
};

static double ohm_at(const scen_t *s, long t)
{
    double r = OPEN;
    for (int i = 0; i < s->nseg; i++)
        if (t >= (long)s->seg[i].t)
            r = s->seg[i].ohm;
    return r;
}

/* A frame is above the threshold if it shows a resistance at or above it;
 * open probes read OL, which never is. */
static bool frame_above(double r) { return r < OPEN && r >= USER_OHM; }

/* One run; returns the number of milliseconds that broke an expectation. */
static int run_one(const gate_t *g, const scen_t *s, uint32_t lag,
                   uint32_t phase, uint32_t *first_bad)
{
    gstate_t st = { { false, 0 }, 0 };
    bool is_short = false;
    uint32_t last_tone = 0, onset = 0;
    int bad = 0;

    for (uint32_t t = 0; t < s->end; t++) {
        /* Frames that have arrived by t; the latest shows the probes at
         * its arrival minus the lag. */
        uint32_t upd = t >= phase ? (t - phase) / FRAME_MS + 1 : 0;
        long shown_at = upd ? (long)(phase + (upd - 1) * FRAME_MS) - (long)lag : -1;
        bool above = upd && frame_above(ohm_at(s, shown_at));

        bool tone = ohm_at(s, t) < HW_OHM;
        if (tone)
            last_tone = t;
        if (!is_short && tone) {
            is_short = true;
            onset = t;
            g->start(&st, upd);
        } else if (is_short && !tone && t - last_tone >= HOLD_MS) {
            is_short = false;
        }

        bool beep = is_short && !g->step(&st, t - onset, upd, above);

        for (int w = 0; w < s->nwant; w++)
            if (t >= s->want[w].from && t < s->want[w].to &&
                beep != s->want[w].beep) {
                if (!bad)
                    *first_bad = t;
                bad++;
            }
    }
    return bad;
}

/* Runs every scenario at every lag and phase; returns the failed runs. */
static int run_gate(const gate_t *g, bool verbose)
{
    int failed = 0, runs = 0;
    for (int i = 0; i < N_SCEN; i++) {
        int scen_failed = 0;
        for (int l = 0; l < N_LAG; l++)
            for (int p = 0; p < N_PHASE; p++) {
                uint32_t first = 0;
                runs++;
                int bad = run_one(g, &scen[i], lags[l], phases[p], &first);
                if (bad) {
                    failed++;
                    if (verbose && scen_failed++ == 0)
                        printf("  FAIL %s: lag %u phase %u, %d ms wrong from %u ms\n",
                               scen[i].what, (unsigned)lags[l],
                               (unsigned)phases[p], bad, (unsigned)first);
                }
            }
    }
    printf("%-32s %d/%d runs as expected\n", g->name, runs - failed, runs);
    return failed;
}

static int fails, checks;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    checks++;
    if (run_gate(&gates[0], true) != 0)
        fails++;

    for (int i = 1; i < (int)(sizeof gates / sizeof gates[0]); i++) {
        checks++;
        if (run_gate(&gates[i], false) == 0) {
            fails++;
            printf("  FAIL control passed: the scenarios do not catch it\n");
        }
    }

    /* The step rule by itself. */
    cont_veto_t v;
    cont_veto_start(&v, 5);
    CHECK(!cont_veto_step(&v, 1300, 5, true),  "the frame on hand at the onset never counts");
    cont_veto_start(&v, 5);
    CHECK(!cont_veto_step(&v, 100, 6, true),   "a new frame inside the window is ignored");
    CHECK(!cont_veto_step(&v, 1300, 6, true),  "and stays ignored once the window is over");
    CHECK(cont_veto_step(&v, 1300, 7, true),   "a new frame after the window mutes");
    CHECK(cont_veto_step(&v, 1350, 7, false),  "no new frame: the mute holds");
    CHECK(!cont_veto_step(&v, 1400, 8, false), "a new frame below the threshold unmutes");
    cont_veto_start(&v, 0);
    CHECK(!cont_veto_step(&v, CONT_VETO_IGNORE_MS - 1, 1, true), "1 ms before the window ends: ignored");
    CHECK(cont_veto_step(&v, CONT_VETO_IGNORE_MS, 2, true),      "at the window's end: counts");

    printf("test_continuity_veto: %d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
