/* See mask_pf.h. Integer-only. Per record: one pass over MASK_PF_SPAN samples
 * per masked channel, each deriving its bound from a (2*tol_h+1)-wide window
 * of the envelope. */
#include "mask_pf.h"
#include <string.h>

#define ENV_LO(m, c) ((m)->env + (2u * (c)) * MASK_PF_ENV)
#define ENV_HI(m, c) ((m)->env + (2u * (c) + 1u) * MASK_PF_ENV)

static void stats_clear(mask_pf_t *m)
{
    m->tested = m->passed = m->failed = 0;
    memset(m->skipped, 0, sizeof(m->skipped));
    m->consec_fail = m->max_consec_fail = 0;
    memset(&m->last_fail, 0, sizeof(m->last_fail));
    m->last_fail.first_ch = -1;
    m->last_fail.first_col = -1;
    memset(m->fail_cols, 0, sizeof(m->fail_cols));
}

void mask_pf_init(mask_pf_t *m, uint8_t *buf)
{
    memset(m, 0, sizeof(*m));
    m->env = buf;
    m->tol_v = MASK_PF_TOL_V_DEFAULT;
    m->tol_h = MASK_PF_TOL_H_DEFAULT;
    m->last.first_ch = -1;
    m->last.first_col = -1;
    stats_clear(m);
}

void mask_pf_attach(mask_pf_t *m, uint8_t *buf)
{
    m->env = buf;
}

void mask_pf_clear(mask_pf_t *m)
{
    uint8_t v = m->tol_v, h = m->tol_h;        /* tolerances are preferences */
    uint8_t *buf = m->env;
    mask_pf_init(m, buf);
    m->tol_v = v;
    m->tol_h = h;
}

bool mask_pf_teach_begin(mask_pf_t *m, uint8_t chans, int16_t pre, uint16_t frames)
{
    mask_pf_clear(m);
    chans &= 0x03u;
    if (m->env == 0 || chans == 0u)
        return false;
    if (frames == 0u) frames = MASK_PF_TEACH_DEFAULT;
    if (frames > MASK_PF_TEACH_MAX) frames = MASK_PF_TEACH_MAX;
    if (pre < 0) pre = 0;
    if (pre > (int16_t)MASK_PF_SPAN) pre = (int16_t)MASK_PF_SPAN;
    m->chans = chans;
    m->pre = pre;
    m->teach_target = frames;
    m->state = MASK_PF_TEACHING;
    return true;
}

uint8_t mask_pf_cond_diff(const mask_pf_cond_t *a, const mask_pf_cond_t *b)
{
    uint8_t d = 0;
    if (a->range[0] != b->range[0])       d |= MASK_PF_C_RANGE1;
    if (a->range[1] != b->range[1])       d |= MASK_PF_C_RANGE2;
    if (a->coupling[0] != b->coupling[0] ||
        a->coupling[1] != b->coupling[1]) d |= MASK_PF_C_COUPLING;
    if (a->timebase != b->timebase)       d |= MASK_PF_C_TIMEBASE;
    if (a->trig_code != b->trig_code)     d |= MASK_PF_C_TRIG_LEVEL;
    if (a->trig_edge != b->trig_edge)     d |= MASK_PF_C_TRIG_EDGE;
    if (a->trig_src != b->trig_src)       d |= MASK_PF_C_TRIG_SRC;
    return d;
}

/* Channel-relevant part of a cond diff: a range change on a channel that is
 * not in the mask does not make the record incomparable. */
static uint8_t relevant_diff(uint8_t d, uint8_t chans)
{
    if (!(chans & 1u)) d &= (uint8_t)~MASK_PF_C_RANGE1;
    if (!(chans & 2u)) d &= (uint8_t)~MASK_PF_C_RANGE2;
    return d;
}

/* The bound at span column i: the extreme of the envelope anywhere within
 * +/-tol_h samples (timing jitter of that size passes), widened by tol_v
 * counts. Returns the UNCLAMPED values so callers can tell a rail clamp. */
static void bound_at(const mask_pf_t *m, unsigned c, int i, int *lo_out, int *hi_out)
{
    const int h = (int)m->tol_h;               /* <= MASK_PF_HMAX: never off the envelope */
    const uint8_t *el = ENV_LO(m, c), *eh = ENV_HI(m, c);
    int a = i + (int)MASK_PF_HMAX - h, b = i + (int)MASK_PF_HMAX + h;
    int lo = 255, hi = 0;
    for (int j = a; j <= b; j++) {
        if (el[j] < lo) lo = el[j];
        if (eh[j] > hi) hi = eh[j];
    }
    *lo_out = lo - (int)m->tol_v;
    *hi_out = hi + (int)m->tol_v;
}

/* A bound at or past a rail means excursions beyond that rail cannot be
 * seen; recorded so the UI can say so instead of implying full coverage. */
static void update_rail_limited(mask_pf_t *m)
{
    m->rail_limited = false;
    for (unsigned c = 0; c < MASK_PF_NCH; c++) {
        if (!(m->chans & (1u << c))) continue;
        for (int i = 0; i < (int)MASK_PF_SPAN; i++) {
            int lo, hi;
            bound_at(m, c, i, &lo, &hi);
            if (hi >= 255 || lo <= 0) { m->rail_limited = true; return; }
        }
    }
}

bool mask_pf_bounds(const mask_pf_t *m, uint8_t ch, uint16_t col,
                    uint8_t *lo, uint8_t *hi)
{
    if (m->state != MASK_PF_READY || ch >= MASK_PF_NCH || col >= MASK_PF_SPAN ||
        !(m->chans & (1u << ch)))
        return false;
    int l, h;
    bound_at(m, ch, (int)col, &l, &h);
    *lo = (uint8_t)(l < 0 ? 0 : l);
    *hi = (uint8_t)(h > 255 ? 255 : h);
    return true;
}

void mask_pf_set_tol(mask_pf_t *m, uint8_t tol_v, uint8_t tol_h)
{
    if (tol_h > MASK_PF_HMAX) tol_h = MASK_PF_HMAX;
    m->tol_v = tol_v;
    m->tol_h = tol_h;
    if (m->state == MASK_PF_READY) {
        update_rail_limited(m);
        stats_clear(m);
    }
}

void mask_pf_reset_stats(mask_pf_t *m)
{
    stats_clear(m);
}

/* Checks shared by teach and test, in the order of mask_pf_reason_t. On
 * success *start is the record index of span column 0. */
static mask_pf_reason_t frame_usable(const mask_pf_t *m, const mask_pf_frame_t *f,
                                     bool check_cond, int32_t margin,
                                     uint8_t *diff, int32_t *start)
{
    *diff = 0;
    if (f->stale)         return MASK_PF_R_STALE;
    if (!f->triggered)    return MASK_PF_R_UNTRIGGERED;
    if (!f->time_ordered) return MASK_PF_R_NOT_ORDERED;
    if (f->anchor < 0)    return MASK_PF_R_NO_ANCHOR;
    if (check_cond) {
        *diff = relevant_diff(mask_pf_cond_diff(&m->cond, &f->cond), m->chans);
        if (*diff) return MASK_PF_R_SETTINGS;
    }
    int32_t s = (int32_t)f->anchor - (int32_t)m->pre;
    if (s - margin < 0 || s + (int32_t)MASK_PF_SPAN + margin > (int32_t)f->len)
        return MASK_PF_R_SPAN;
    for (unsigned c = 0; c < MASK_PF_NCH; c++)
        if ((m->chans & (1u << c)) && f->rec[c] == 0)
            return MASK_PF_R_SPAN;           /* a masked channel has no data */
    *start = s;
    return MASK_PF_R_OK;
}

static mask_pf_result_t result_init(const mask_pf_frame_t *f)
{
    mask_pf_result_t r;
    memset(&r, 0, sizeof(r));
    r.first_ch = -1;
    r.first_col = -1;
    r.gen = f->gen;
    return r;
}

static mask_pf_result_t teach(mask_pf_t *m, const mask_pf_frame_t *f)
{
    mask_pf_result_t r = result_init(f);
    int32_t s = 0;
    uint8_t diff = 0;
    const int32_t H = (int32_t)MASK_PF_HMAX;
    /* The first accepted record fixes the settings; the rest must match. */
    mask_pf_reason_t why = frame_usable(m, f, m->teach_got > 0u, H, &diff, &s);
    if (why == MASK_PF_R_OK) {
        for (unsigned c = 0; c < MASK_PF_NCH && why == MASK_PF_R_OK; c++) {
            if (!(m->chans & (1u << c))) continue;
            for (unsigned i = 0; i < MASK_PF_ENV; i++) {
                uint8_t v = f->rec[c][s - H + (int32_t)i];
                if (v == 0u || v == 255u) { why = MASK_PF_R_CLIPPED; break; }
            }
        }
    }
    if (why != MASK_PF_R_OK) {
        m->teach_rejects[why]++;
        r.verdict = MASK_PF_V_SKIP;
        r.reason = why;
        r.cond_diff = diff;
        return r;
    }

    if (m->teach_got == 0u)
        m->cond = f->cond;
    for (unsigned c = 0; c < MASK_PF_NCH; c++) {
        if (!(m->chans & (1u << c))) continue;
        uint8_t *el = ENV_LO(m, c), *eh = ENV_HI(m, c);
        for (unsigned i = 0; i < MASK_PF_ENV; i++) {
            uint8_t v = f->rec[c][s - H + (int32_t)i];
            if (m->teach_got == 0u) {
                el[i] = v;
                eh[i] = v;
            } else {
                if (v < el[i]) el[i] = v;
                if (v > eh[i]) eh[i] = v;
            }
        }
    }
    m->teach_got++;
    if (m->teach_got >= m->teach_target) {
        m->state = MASK_PF_READY;
        update_rail_limited(m);
        stats_clear(m);
    }
    r.verdict = MASK_PF_V_NONE;               /* taught from, not judged */
    return r;
}

static mask_pf_result_t test(mask_pf_t *m, const mask_pf_frame_t *f)
{
    mask_pf_result_t r = result_init(f);
    int32_t s = 0;
    uint8_t diff = 0;
    mask_pf_reason_t why = frame_usable(m, f, true, 0, &diff, &s);
    if (why != MASK_PF_R_OK) {
        m->skipped[why]++;
        r.verdict = MASK_PF_V_SKIP;
        r.reason = why;
        r.cond_diff = diff;
        return r;
    }

    uint8_t cols[MASK_PF_NCH][MASK_PF_SPAN / 8u];
    memset(cols, 0, sizeof(cols));
    for (unsigned c = 0; c < MASK_PF_NCH; c++) {
        if (!(m->chans & (1u << c))) continue;
        for (unsigned i = 0; i < MASK_PF_SPAN; i++) {
            int lo, hi;
            bound_at(m, c, (int)i, &lo, &hi);
            int v = (int)f->rec[c][s + (int32_t)i];
            int ex = 0;
            if (v > hi)      ex = v - hi;
            else if (v < lo) ex = lo - v;
            if (ex == 0) continue;
            r.violations++;
            cols[c][i >> 3] |= (uint8_t)(1u << (i & 7u));
            if (ex > r.worst) r.worst = (int16_t)ex;
            if (r.first_col < 0 || (int16_t)i < r.first_col) {
                r.first_col = (int16_t)i;
                r.first_ch = (int8_t)c;
            }
        }
    }

    m->tested++;
    if (r.violations == 0u) {
        r.verdict = MASK_PF_V_PASS;
        m->passed++;
        m->consec_fail = 0;
    } else {
        r.verdict = MASK_PF_V_FAIL;
        m->failed++;
        m->consec_fail++;
        if (m->consec_fail > m->max_consec_fail)
            m->max_consec_fail = m->consec_fail;
        m->last_fail = r;
        memcpy(m->fail_cols, cols, sizeof(cols));
    }
    return r;
}

mask_pf_result_t mask_pf_frame(mask_pf_t *m, const mask_pf_frame_t *f)
{
    mask_pf_result_t r;
    if (m->state == MASK_PF_TEACHING) {
        r = teach(m, f);
    } else if (m->state == MASK_PF_READY) {
        r = test(m, f);
    } else {
        r = result_init(f);
        r.verdict = MASK_PF_V_SKIP;
        r.reason = MASK_PF_R_NO_MASK;
    }
    m->last = r;
    return r;
}

bool mask_pf_fail_col(const mask_pf_t *m, uint8_t ch, uint16_t col)
{
    if (ch >= MASK_PF_NCH || col >= MASK_PF_SPAN) return false;
    return (m->fail_cols[ch][col >> 3] >> (col & 7u)) & 1u;
}

uint8_t mask_pf_teach_spread(const mask_pf_t *m)
{
    uint8_t w = 0;
    if (m->env == 0 || m->state == MASK_PF_EMPTY) return 0;
    for (unsigned c = 0; c < MASK_PF_NCH; c++) {
        if (!(m->chans & (1u << c))) continue;
        const uint8_t *el = ENV_LO(m, c), *eh = ENV_HI(m, c);
        for (unsigned i = MASK_PF_HMAX; i < MASK_PF_HMAX + MASK_PF_SPAN; i++) {
            uint8_t d = (uint8_t)(eh[i] - el[i]);
            if (d > w) w = d;
        }
    }
    return w;
}

const char *mask_pf_reason_str(mask_pf_reason_t r)
{
    switch (r) {
    case MASK_PF_R_OK:          return "ok";
    case MASK_PF_R_NO_MASK:     return "no mask";
    case MASK_PF_R_STALE:       return "stale (held across a stop)";
    case MASK_PF_R_UNTRIGGERED: return "untriggered (free-run)";
    case MASK_PF_R_NOT_ORDERED: return "not time-ordered";
    case MASK_PF_R_NO_ANCHOR:   return "no trigger crossing";
    case MASK_PF_R_SETTINGS:    return "settings changed";
    case MASK_PF_R_SPAN:        return "span off record";
    case MASK_PF_R_CLIPPED:     return "clipped at ADC rail";
    default:                    return "?";
    }
}
