/* See trig_edge.h. Integer-only; ~3 passes over 1024 bytes. */
#include "trig_edge.h"

#define N          TRIG_EDGE_REC_N
#define MASK       (N - 1u)
#define TRIG_POS   512
#define SEARCH     24      /* crossing searched within +/-24 of the trigger
                              index: EXP-53 placed it 501..520 */
#define SEAM_RATIO 2       /* seam jump must be >= 2x every other step   */
#define SEAM_MIN   6       /* and at least 6 counts                      */
#define DMIN       4       /* slope must clear 4 counts at some scale    */

static inline int at(const volatile uint8_t *rec, uint32_t k, int i)
{
    return (int)rec[(k + (uint32_t)i) & MASK];
}

int trig_edge_find_seam(const volatile uint8_t *rec, uint32_t *k_out)
{
    if (rec == 0)
        return 0;
    /* The largest circular step. k = its newer side = oldest sample. */
    uint32_t k = 0;
    int j1 = -1;
    for (uint32_t i = 0; i < N; i++) {
        int d = (int)rec[i] - (int)rec[(i + MASK) & MASK];
        if (d < 0) d = -d;
        if (d > j1) { j1 = d; k = i; }
    }
    int j2 = 0;
    for (uint32_t i = 0; i < N; i++) {
        uint32_t off = (i - k) & MASK;           /* skip k-2 .. k+2 */
        if (off <= 2u || off >= N - 2u) continue;
        int d = (int)rec[i] - (int)rec[(i + MASK) & MASK];
        if (d < 0) d = -d;
        if (d > j2) j2 = d;
    }
    if (j1 < SEAM_MIN || j1 < SEAM_RATIO * j2)
        return 0;
    *k_out = k;
    return 1;
}

trig_edge_class_t trig_edge_classify(const volatile uint8_t *rec, int crossing)
{
    /* 1. The seam (see trig_edge_find_seam). */
    uint32_t k = 0;
    if (!trig_edge_find_seam(rec, &k))
        return TRIG_EDGE_CLASS_UNKNOWN;

    /* 2. The crossing nearest the trigger index, on a 4-sample running sum
     *    (r = the un-rotated record, r[0] oldest). */
    int c4 = 4 * crossing;
    int best = -1;
    for (int j = TRIG_POS - SEARCH; j < TRIG_POS + SEARCH; j++) {
        int a = at(rec, k, j - 1) + at(rec, k, j) + at(rec, k, j + 1) + at(rec, k, j + 2);
        int b = at(rec, k, j)     + at(rec, k, j + 1) + at(rec, k, j + 2) + at(rec, k, j + 3);
        if ((a < c4 && c4 <= b) || (a >= c4 && c4 > b)) {
            int dj = j - TRIG_POS; if (dj < 0) dj = -dj;
            int db = best - TRIG_POS; if (db < 0) db = -db;
            if (best < 0 || dj < db) best = j;
        }
    }
    if (best < 0)
        return TRIG_EDGE_CLASS_UNKNOWN;          /* no trigger at this level */

    /* 3. The slope at the smallest scale that clears the noise: a fast edge
     *    is read locally, a slow ramp over a wider window. */
    static const int scales[] = { 4, 8, 16, 32, 64 };
    for (unsigned s = 0; s < sizeof scales / sizeof scales[0]; s++) {
        int w = scales[s];
        int after = 0, before = 0;
        for (int i = 1; i <= w; i++) {
            after  += at(rec, k, best + i);
            before += at(rec, k, best + 1 - i);
        }
        int d = after - before;                  /* = w x (mean difference) */
        if (d >= DMIN * w)  return TRIG_EDGE_CLASS_RISING;
        if (d <= -DMIN * w) return TRIG_EDGE_CLASS_FALLING;
    }
    return TRIG_EDGE_CLASS_UNKNOWN;
}

/* ── Linear-prediction seam finder (see trig_edge.h) ─────────────────── */
#define LPC_P 8

static float lpc_err(const volatile uint8_t *rec, const float *a, float mean, uint32_t n)
{
    float e = 0.0f;
    for (int j = 0; j <= LPC_P; j++)
        e += a[j] * ((float)rec[(n - (uint32_t)j) & MASK] - mean);
    return e < 0.0f ? -e : e;
}

int trig_edge_find_seam_lpc(const volatile uint8_t *rec, uint32_t *k_out)
{
    if (rec == 0)
        return 0;
    float mean = 0.0f;
    for (uint32_t i = 0; i < N; i++) mean += (float)rec[i];
    mean /= (float)N;

    /* Circular autocorrelation r[0..P]. */
    float r[LPC_P + 1];
    for (int l = 0; l <= LPC_P; l++) {
        float acc = 0.0f;
        for (uint32_t i = 0; i < N; i++)
            acc += ((float)rec[i] - mean) * ((float)rec[(i + MASK + 1u - (uint32_t)l) & MASK] - mean);
        r[l] = acc;
    }
    if (r[0] <= 0.0f)
        return 0;

    /* Levinson-Durbin: a[0] = 1, e = prediction error power. */
    float a[LPC_P + 1] = { 1.0f };
    float tmp[LPC_P + 1];
    float e = r[0];
    for (int i = 1; i <= LPC_P; i++) {
        float acc = r[i];
        for (int j = 1; j < i; j++) acc += a[j] * r[i - j];
        float kk = -acc / e;
        for (int j = 0; j <= i; j++) tmp[j] = a[j];
        for (int j = 1; j < i; j++) a[j] = tmp[j] + kk * tmp[i - j];
        a[i] = kk;
        e *= (1.0f - kk * kk);
        if (e <= 0.0f)
            return 0;
    }

    /* Pass 1: the peak prediction error. */
    uint32_t m = 0;
    float peak = -1.0f;
    for (uint32_t n = 0; n < N; n++) {
        float v = lpc_err(rec, a, mean, n);
        if (v > peak) { peak = v; m = n; }
    }
    /* Pass 2: the largest error more than P samples from the peak. */
    float second = 0.0f;
    for (uint32_t n = 0; n < N; n++) {
        uint32_t d = (n - m) & MASK;
        if (d <= (uint32_t)LPC_P || d >= N - (uint32_t)LPC_P) continue;
        float v = lpc_err(rec, a, mean, n);
        if (v > second) second = v;
    }
    if (peak < 3.0f * second)
        return 0;
    /* Pass 3: the first prediction that reaches across the seam is AT the
     * seam; walk back to the earliest index within P whose error is >= 35%
     * of the peak. */
    uint32_t k = m;
    for (uint32_t o = 1; o <= (uint32_t)LPC_P; o++)
        if (lpc_err(rec, a, mean, (m - o) & MASK) >= 0.35f * peak)
            k = (m - o) & MASK;
    *k_out = k;
    return 1;
}

int trig_edge_find_seam_any(const volatile uint8_t *rec, uint32_t *k)
{
    return trig_edge_find_seam(rec, k) || trig_edge_find_seam_lpc(rec, k);
}
