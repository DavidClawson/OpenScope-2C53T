/* See scope_mask.h. */
#include "scope_mask.h"
#include "scope_state.h"
#include "trig_edge.h"
#include "fpga.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* The whole mask -- bookkeeping AND envelope, sizeof(mask_pf_t) +
 * MASK_PF_BUF_BYTES, ~1.6 KB -- is one FreeRTOS heap block taken on the first
 * teach. Static RAM in the coldtrace image was within ~110 bytes of full when
 * this feature was written (the first build overflowed it by 2,960 B, the
 * second, with only the bookkeeping static, by 152 B). Allocated once and
 * NEVER freed, so a reader holding the pointer can never see it vanish; NULL
 * until then, which every accessor treats as "no mask". */
static mask_pf_t *g_m;
static uint8_t    g_tol_v = MASK_PF_TOL_V_DEFAULT;   /* tolerances set before */
static uint8_t    g_tol_h = MASK_PF_TOL_H_DEFAULT;   /* the first teach */

/* Request mailbox: one slot, sequence-numbered. Posting overwrites an
 * unapplied request -- the latest intent wins, and the overwritten caller's
 * scope_mask_acked() still turns true when the newer one lands. */
static volatile uint32_t req_seq, ack_seq;
static volatile uint8_t  req_kind, req_a, req_b;
static volatile uint16_t req_n;

static volatile bool     g_hold;
static bool              g_stale_next;   /* first commit after a hold release */
static volatile bool     g_stop_on_fail = true;
static volatile uint32_t g_missed;       /* commits not judged (gen gaps) */
static volatile uint32_t g_epoch;
static uint32_t          g_last_gen;
static const char       *g_teach_refusal;
static mask_pf_reason_t  g_teach_last_reject;

/* Allocate on first use; false if the heap could not supply it. */
static bool ensure_alloc(void)
{
    if (g_m != NULL) return true;
    uint8_t *blk = (uint8_t *)pvPortMalloc(sizeof(mask_pf_t) + MASK_PF_BUF_BYTES);
    if (blk == NULL) return false;
    mask_pf_t *m = (mask_pf_t *)blk;
    mask_pf_init(m, blk + sizeof(mask_pf_t));
    mask_pf_set_tol(m, g_tol_v, g_tol_h);
    g_m = m;                       /* publish last: readers see NULL or a ready mask */
    return true;
}

uint32_t scope_mask_post(scope_mask_req_t kind, uint8_t a, uint8_t b, uint16_t n)
{
    uint32_t seq;
    taskENTER_CRITICAL();
    req_kind = (uint8_t)kind;
    req_a = a;
    req_b = b;
    req_n = n;
    seq = ++req_seq;
    taskEXIT_CRITICAL();
    return seq;
}

bool scope_mask_acked(uint32_t seq)
{
    return (int32_t)(ack_seq - seq) >= 0;
}

const char *scope_mask_teach_refusal(void) { return g_teach_refusal; }
bool scope_mask_hold_active(void)           { return g_hold; }
uint32_t scope_mask_epoch(void)             { return g_epoch; }
const mask_pf_t *scope_mask_raw(void)       { return g_m; }
mask_pf_state_t scope_mask_state(void)      { return g_m ? g_m->state : MASK_PF_EMPTY; }

static void start_teach(uint8_t chans, uint16_t frames)
{
    const scope_state_t *ss = scope_state_get();
    g_teach_refusal = NULL;
    /* The hardware anchor -- the only alignment a mask can trust -- is
     * measured on CH1 (EXP-55/56), so v1 masks need a CH1 trigger. */
    if (ss->trigger.source == TRIG_SRC_CH2) {
        g_teach_refusal = "trigger source is CH2; masks need a CH1 trigger";
        return;
    }
    if (chans == 0u)
        chans = (uint8_t)((ss->ch1.enabled ? 1u : 0u) | (ss->ch2.enabled ? 2u : 0u));
    if (chans == 0u) {
        g_teach_refusal = "no channel enabled";
        return;
    }
    if (!ensure_alloc()) {
        g_teach_refusal = "out of heap for the mask (needs ~1.6 KB)";
        return;
    }
    int16_t pre = ss->trig_x;            /* cover what is on the glass */
    if (pre < 8) pre = 8;
    if (pre > (int16_t)MASK_PF_SPAN - 8) pre = (int16_t)MASK_PF_SPAN - 8;
    g_hold = false;
    g_teach_last_reject = MASK_PF_R_OK;
    if (!mask_pf_teach_begin(g_m, chans, pre, frames))
        g_teach_refusal = "teach refused by the mask core";
}

void scope_mask_service(void)
{
    if (req_seq == ack_seq)
        return;
    uint8_t kind, a, b;
    uint16_t n;
    uint32_t seq;
    taskENTER_CRITICAL();
    kind = req_kind; a = req_a; b = req_b; n = req_n; seq = req_seq;
    taskEXIT_CRITICAL();

    switch ((scope_mask_req_t)kind) {
    case SCOPE_MASK_REQ_TEACH:    start_teach(a, n); break;
    case SCOPE_MASK_REQ_TOL:
        g_tol_v = a;
        g_tol_h = (b > MASK_PF_HMAX) ? (uint8_t)MASK_PF_HMAX : b;
        if (g_m) mask_pf_set_tol(g_m, a, b);
        break;
    case SCOPE_MASK_REQ_CLEAR:
        if (g_m) mask_pf_clear(g_m);
        g_hold = false;
        break;
    case SCOPE_MASK_REQ_RESET:
        if (g_m) mask_pf_reset_stats(g_m);
        g_missed = 0;
        break;
    case SCOPE_MASK_REQ_RELEASE:
        if (g_hold) g_stale_next = true;   /* see mask_pf_frame_t.stale */
        g_hold = false;
        if (g_m) g_m->consec_fail = 0;
        break;
    case SCOPE_MASK_REQ_STOPFAIL:
        g_stop_on_fail = (a != 0u);
        if (!g_stop_on_fail) g_hold = false;
        break;
    default: break;
    }
    g_epoch++;
    ack_seq = seq;
}

/* Settings IN FORCE: the codes written to the hardware where fpga.c records
 * them (timebase, trigger level), the frontend state elsewhere. Reading the
 * UI's intent instead is the EXP-17 timebase-button bug. */
static mask_pf_cond_t cond_now(void)
{
    const scope_state_t *ss = scope_state_get();
    mask_pf_cond_t c;
    memset(&c, 0, sizeof(c));
    c.range[0]    = ss->ch1.vdiv_idx;
    c.range[1]    = ss->ch2.vdiv_idx;
    c.coupling[0] = (uint8_t)ss->ch1.coupling;
    c.coupling[1] = (uint8_t)ss->ch2.coupling;
    c.timebase    = fpga_acq_rate_idx_get();
    c.trig_code   = fpga_acq_trig_code_get();
    c.trig_edge   = (ss->trigger.edge == TRIG_RISING) ? 0u : 1u;
    c.trig_src    = (ss->trigger.source == TRIG_SRC_CH2) ? 1u : 0u;
    return c;
}

void scope_mask_on_commit(const volatile uint8_t *ch1, const volatile uint8_t *ch2,
                          bool triggered, bool time_ordered, uint32_t gen)
{
    /* Each commit advances the generation by 2; a larger step means a
     * commit this hook never saw (it should not happen -- the hook runs in
     * the committing task -- so a non-zero count is itself a finding). */
    if (g_last_gen != 0u && gen - g_last_gen > 2u)
        g_missed += (gen - g_last_gen) / 2u - 1u;
    g_last_gen = gen;

    mask_pf_t *m = g_m;
    if (m == NULL || m->state == MASK_PF_EMPTY || g_hold)
        return;

    const scope_state_t *ss = scope_state_get();
    mask_pf_frame_t f;
    memset(&f, 0, sizeof(f));
    f.rec[0] = ch1;
    f.rec[1] = ch2;
    f.len = FPGA_ADC_BUF_SIZE;
    f.triggered = triggered;
    f.time_ordered = time_ordered;
    f.stale = g_stale_next;
    g_stale_next = false;
    f.gen = gen;
    f.cond = cond_now();
    f.anchor = -1;
    if (ch1 != NULL && ss->trigger.source != TRIG_SRC_CH2)
        f.anchor = (int16_t)trig_edge_anchor(ch1,
                        (int)fpga_acq_trig_code_get() + (int)FPGA_ADC_OFFSET,
                        ss->trigger.edge == TRIG_RISING);

    mask_pf_state_t before = m->state;
    mask_pf_result_t r = mask_pf_frame(m, &f);
    if (before == MASK_PF_TEACHING && r.verdict == MASK_PF_V_SKIP)
        g_teach_last_reject = r.reason;
    if (r.verdict == MASK_PF_V_FAIL && g_stop_on_fail)
        g_hold = true;
    g_epoch++;
}

void scope_mask_summary(scope_mask_summary_t *s)
{
    memset(s, 0, sizeof(*s));
    s->hold = g_hold;
    s->stop_on_fail = g_stop_on_fail;
    s->missed = g_missed;
    s->tol_v = g_tol_v;
    s->tol_h = g_tol_h;
    s->teach_last_reject = g_teach_last_reject;
    const mask_pf_t *m = g_m;
    if (m == NULL) {
        s->state = MASK_PF_EMPTY;
        s->last.first_ch = s->last_fail.first_ch = -1;
        s->last.first_col = s->last_fail.first_col = -1;
        return;
    }
    s->state = m->state;
    s->chans = m->chans;
    s->pre = m->pre;
    s->tol_v = m->tol_v;
    s->tol_h = m->tol_h;
    s->rail_limited = m->rail_limited;
    s->teach_got = m->teach_got;
    s->teach_target = m->teach_target;
    for (unsigned i = 0; i < MASK_PF_R_COUNT; i++) {
        s->teach_rejects += m->teach_rejects[i];
        s->skipped += m->skipped[i];
    }
    s->spread = (m->state == MASK_PF_READY) ? mask_pf_teach_spread(m) : 0u;
    s->tested = m->tested;
    s->passed = m->passed;
    s->failed = m->failed;
    s->max_consec_fail = m->max_consec_fail;
    s->last = m->last;
    s->last_fail = m->last_fail;
    s->cond = m->cond;
}

scope_mask_col_t scope_mask_column(uint8_t ch, int x, int trig_x, uint32_t gen)
{
    const mask_pf_t *mk = g_m;
    if (mk == NULL || mk->state == MASK_PF_EMPTY || trig_x < 0 || ch >= MASK_PF_NCH)
        return SCOPE_MASK_COL_NONE;
    if (!(mk->chans & (1u << ch)))
        return SCOPE_MASK_COL_NONE;
    int m = x - trig_x + (int)mk->pre;
    if (m < 0 || m >= (int)MASK_PF_SPAN)
        return SCOPE_MASK_COL_NONE;
    /* Only colour the frame that was actually judged. */
    if (mk->last.gen != gen)
        return SCOPE_MASK_COL_NONE;
    if (mk->state == MASK_PF_TEACHING)
        return SCOPE_MASK_COL_TEACH;
    switch (mk->last.verdict) {
    case MASK_PF_V_PASS: return SCOPE_MASK_COL_PASS;
    case MASK_PF_V_FAIL:
        return mask_pf_fail_col(mk, ch, (uint16_t)m) ? SCOPE_MASK_COL_FAIL
                                                     : SCOPE_MASK_COL_FAIL_FRAME;
    case MASK_PF_V_SKIP: return SCOPE_MASK_COL_SKIP;
    default:             return SCOPE_MASK_COL_NONE;
    }
}

bool scope_mask_extent(uint8_t ch, uint8_t *lo, uint8_t *hi)
{
    const mask_pf_t *mk = g_m;
    if (mk == NULL || mk->state != MASK_PF_READY || ch >= MASK_PF_NCH ||
        !(mk->chans & (1u << ch)))
        return false;
    uint8_t mn = 255, mx = 0;
    for (uint16_t i = 0; i < MASK_PF_SPAN; i++) {
        uint8_t l, h;
        if (!mask_pf_bounds(mk, ch, i, &l, &h)) return false;
        if (l < mn) mn = l;
        if (h > mx) mx = h;
    }
    *lo = mn;
    *hi = mx;
    return true;
}

bool scope_mask_bound_at(uint8_t ch, int x, int trig_x, uint8_t *lo, uint8_t *hi)
{
    const mask_pf_t *mk = g_m;
    if (mk == NULL || trig_x < 0) return false;
    int m = x - trig_x + (int)mk->pre;
    if (m < 0 || m >= (int)MASK_PF_SPAN) return false;
    return mask_pf_bounds(mk, ch, (uint16_t)m, lo, hi);
}
