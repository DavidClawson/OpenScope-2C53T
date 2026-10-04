/*
 * mask_pf — waveform pass/fail ("mask") testing on committed scope records.
 *
 * Teach a mask from a few known-good records, then judge every later record
 * against it: PASS, FAIL, or SKIP-with-a-reason. Design note:
 * docs/specs/scope/mask-pass-fail.md.
 *
 * Three rules the design rests on, each learned the hard way elsewhere in
 * this project:
 *
 *  1. Aligned on the TRIGGER, not the screen. The mask covers MASK_PF_SPAN
 *     samples starting `pre` samples before the hardware trigger crossing
 *     (trig_edge_anchor) of a time-ordered record. Where the window sits on
 *     the glass does not move what is tested.
 *
 *  2. Units are the record's own: ADC counts vertically, samples
 *     horizontally. Volts and seconds are a display concern (scope_cal,
 *     scope_timebase) and never enter the verdict, so a calibration change
 *     cannot silently move a mask.
 *
 *  3. REFUSE rather than guess. A record that is not strobed, not time-
 *     ordered, has no trigger crossing near index 512, or was taken under
 *     different settings from the mask (range, timebase, trigger level...)
 *     is SKIPPED and counted by reason -- never scored. A pass rate is only
 *     worth something if every frame in it was actually comparable.
 *
 * Pure C, no RTOS or hardware dependency: host-tested in
 * tests/test_mask_pf.c on real records from unit #1.
 */
#ifndef MASK_PF_H
#define MASK_PF_H

#include <stdint.h>
#include <stdbool.h>

#define MASK_PF_SPAN   320u    /* samples tested = one per screen column */
#define MASK_PF_NCH    2u

/* Largest horizontal tolerance. The envelope is taught over the span PLUS
 * this margin on each side, so a dilated bound at column 0 sees the real
 * samples before the span instead of an edge it cannot look past. */
#define MASK_PF_HMAX   16u
#define MASK_PF_ENV    (MASK_PF_SPAN + 2u * MASK_PF_HMAX)

/* The envelope lives in a CALLER-PROVIDED buffer of this many bytes (lo and
 * hi per channel). The firmware's static RAM is full in the coldtrace image,
 * so scope_mask.c takes it from the heap on the first teach; the bounds are
 * derived from it per record rather than stored (~100 us at tol_h 16 in the
 * acquisition task, far less at the default). */
#define MASK_PF_BUF_BYTES (MASK_PF_NCH * 2u * MASK_PF_ENV)

/* Defaults, in record units. Vertical: 8 counts, a quarter of a 32-count
 * division. Horizontal: 2 samples -- deliberately small. Horizontal slack
 * widens the vertical bound on every slope by slope x tol_h, so on a 201 Hz
 * sine at 12.5 kS/s (62 samples/period, 5 counts/sample at the steepest)
 * 8 samples let a 20-count glitch and a -30% amplitude change through
 * (tests/test_mask_pf.c, first run). The data justify 2: the hardware anchor
 * jitters by about one sample (508..510 across 33 records), and with no
 * horizontal slack at all the same signal re-captured minutes later stays
 * within 4 counts of a 6-record envelope. */
#define MASK_PF_TOL_V_DEFAULT       8u
#define MASK_PF_TOL_H_DEFAULT       2u
#define MASK_PF_TEACH_DEFAULT       8u
#define MASK_PF_TEACH_MAX          64u

typedef enum {
    MASK_PF_EMPTY = 0,      /* no mask */
    MASK_PF_TEACHING,       /* collecting known-good records */
    MASK_PF_READY           /* testing */
} mask_pf_state_t;

typedef enum {
    MASK_PF_V_NONE = 0,     /* nothing evaluated yet */
    MASK_PF_V_PASS,
    MASK_PF_V_FAIL,
    MASK_PF_V_SKIP
} mask_pf_verdict_t;

/* Why a record was not scored (SKIP) or not taught from. Order is the order
 * of the checks, so a record carries the FIRST reason it failed. */
typedef enum {
    MASK_PF_R_OK = 0,
    MASK_PF_R_NO_MASK,       /* state is EMPTY */
    MASK_PF_R_STALE,         /* captured before a stop-on-fail hold ended (see below) */
    MASK_PF_R_UNTRIGGERED,   /* AUTO free-run commit, not a strobed record */
    MASK_PF_R_NOT_ORDERED,   /* seam not recovered: index 512 is not the trigger */
    MASK_PF_R_NO_ANCHOR,     /* no crossing of the level near index 512 */
    MASK_PF_R_SETTINGS,      /* range / timebase / trigger differ from the mask */
    MASK_PF_R_SPAN,          /* the span (+ margin, when teaching) runs off the record */
    MASK_PF_R_CLIPPED,       /* teach only: a sample sits on an ADC rail */
    MASK_PF_R_COUNT
} mask_pf_reason_t;

/* Everything a record's meaning depends on. If any field differs between the
 * mask and a record, the two are not comparable. Filled by the caller from
 * the settings IN FORCE (the codes written to the hardware), not from UI
 * intent -- the timebase-button lesson (EXP-17). */
typedef struct {
    uint8_t range[MASK_PF_NCH];     /* frontend range index per channel */
    uint8_t coupling[MASK_PF_NCH];
    uint8_t timebase;               /* reg 0x01 code in force */
    uint8_t trig_code;              /* reg 0x08 code in force */
    uint8_t trig_edge;              /* 0 rising, 1 falling */
    uint8_t trig_src;               /* 0 CH1, 1 CH2 */
} mask_pf_cond_t;

/* Bit per field of mask_pf_cond_t, for mask_pf_cond_diff(). */
#define MASK_PF_C_RANGE1     0x01u
#define MASK_PF_C_RANGE2     0x02u
#define MASK_PF_C_COUPLING   0x04u
#define MASK_PF_C_TIMEBASE   0x08u
#define MASK_PF_C_TRIG_LEVEL 0x10u
#define MASK_PF_C_TRIG_EDGE  0x20u
#define MASK_PF_C_TRIG_SRC   0x40u

typedef struct {
    const volatile uint8_t *rec[MASK_PF_NCH];  /* NULL = channel absent */
    uint16_t len;                   /* samples per channel record */
    bool     triggered;             /* PC0-strobed, not an AUTO free-run commit */
    bool     time_ordered;          /* un-rotated at its seam (trigger at 512) */
    /* The FPGA holds a completed record until it is read (EXP-53/54). While a
     * stop-on-fail hold stops the reads, it keeps the record it captured just
     * after the failing one, so the FIRST read after release is from the
     * fault period, not from after the user pressed OK. Bench, unit #1,
     * 2026-10-03: fail gen 748 -> hold -> amplitude restored 2 s -> release ->
     * gen 750 FAILED again and re-held; gens 752+ passed. Such a record is
     * marked stale and SKIPPED: not silently dropped, not scored. */
    bool     stale;
    int16_t  anchor;                /* trig_edge_anchor() index, -1 = none */
    uint32_t gen;                   /* frame generation, carried into the result */
    mask_pf_cond_t cond;
} mask_pf_frame_t;

typedef struct {
    mask_pf_verdict_t verdict;
    mask_pf_reason_t  reason;       /* SKIP / teach-reject reason, else OK */
    uint16_t violations;            /* samples outside the mask, all channels */
    int8_t   first_ch;              /* channel of the earliest violation, -1 */
    int16_t  first_col;             /* its column in the span, -1 */
    int16_t  worst;                 /* largest excursion beyond a bound, counts */
    uint8_t  cond_diff;             /* MASK_PF_C_* bits when reason == SETTINGS */
    uint32_t gen;
} mask_pf_result_t;

typedef struct {
    mask_pf_state_t state;
    uint8_t  chans;                 /* bit0 CH1, bit1 CH2: channels in the mask */
    int16_t  pre;                   /* span starts this many samples before the anchor */
    mask_pf_cond_t cond;            /* settings the mask was taught under */
    uint8_t  tol_v, tol_h;
    bool     rail_limited;          /* some bound was clamped at an ADC rail */

    uint16_t teach_target, teach_got;
    uint32_t teach_rejects[MASK_PF_R_COUNT];

    /* taught envelope over the span plus MASK_PF_HMAX either side (span
     * column i is envelope index i + MASK_PF_HMAX), in the caller's buffer:
     * channel c's lows at env + 2c*MASK_PF_ENV, highs right after. NULL = no
     * buffer attached, and teaching is refused. */
    uint8_t *env;

    /* statistics since the mask was finalised (or last reset) */
    uint32_t tested, passed, failed;
    uint32_t skipped[MASK_PF_R_COUNT];
    uint32_t consec_fail, max_consec_fail;

    mask_pf_result_t last;          /* the most recent mask_pf_frame() outcome */
    mask_pf_result_t last_fail;     /* the most recent FAIL */
    uint8_t  fail_cols[MASK_PF_NCH][MASK_PF_SPAN / 8u];  /* violating columns, last FAIL */
} mask_pf_t;

/* Initialise; `buf` (MASK_PF_BUF_BYTES, may be NULL for now) holds the
 * envelope. mask_pf_attach() supplies it later. */
void mask_pf_init(mask_pf_t *m, uint8_t *buf);
void mask_pf_attach(mask_pf_t *m, uint8_t *buf);
void mask_pf_clear(mask_pf_t *m);

/* Start teaching: the next `frames` acceptable records build the envelope.
 * `chans` selects the channels (bit0 CH1, bit1 CH2), `pre` the samples kept
 * before the anchor (the screen column the trigger sits on, so the mask
 * covers what was on the glass). The settings are taken from the first
 * accepted record; later records must match them. */
bool mask_pf_teach_begin(mask_pf_t *m, uint8_t chans, int16_t pre, uint16_t frames);
/* false (and state EMPTY) when no buffer is attached or no channel given. */

/* Change tolerances (tol_h is capped at MASK_PF_HMAX). Re-derives the bounds from the stored envelope (no
 * re-teach needed) and resets the statistics, since the verdicts so far were
 * against a different mask. */
void mask_pf_set_tol(mask_pf_t *m, uint8_t tol_v, uint8_t tol_h);

void mask_pf_reset_stats(mask_pf_t *m);

/* Feed one committed record. TEACHING: folds it into the envelope (or
 * rejects it: result.verdict SKIP + reason) and finalises when enough have
 * been taken. READY: tests it. Every call updates m->last; returns it. */
mask_pf_result_t mask_pf_frame(mask_pf_t *m, const mask_pf_frame_t *f);

/* Which MASK_PF_C_* fields differ between two condition sets. */
uint8_t mask_pf_cond_diff(const mask_pf_cond_t *a, const mask_pf_cond_t *b);

/* The bounds at span column `col` of channel `ch` under the current
 * tolerances. false if the mask is not READY or the channel is not in it. */
bool mask_pf_bounds(const mask_pf_t *m, uint8_t ch, uint16_t col,
                    uint8_t *lo, uint8_t *hi);

/* Was column `col` of channel `ch` outside the mask in the last FAIL? */
bool mask_pf_fail_col(const mask_pf_t *m, uint8_t ch, uint16_t col);

/* Largest (env_hi - env_lo) over the taught channels: how much the known-good
 * records disagreed among themselves. A large spread means a loose mask. */
uint8_t mask_pf_teach_spread(const mask_pf_t *m);

const char *mask_pf_reason_str(mask_pf_reason_t r);

#endif /* MASK_PF_H */
