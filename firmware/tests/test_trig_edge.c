/*
 * trig_edge_classify(): the MCU-side trigger edge filter (EXP-55 follow-up).
 *
 * [1] real records (EXP-55/56, unit #1): never wrong, and classifies most.
 * [2] negative control: the same records with the level moved off the signal
 *     must come back UNKNOWN -- a classifier that ignores the level would pass
 *     [1] and still be wrong about what triggered.
 * [3] synthetic sweep, sines and squares from 0.2 to 31 periods per record,
 *     both edges, random rotation: never wrong; integer periods (seam
 *     invisible by construction) must be refused, not guessed.
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "trig_edge.h"
#include "fixtures/trig_edge_records.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (cond) { printf("  PASS: "); } else { printf("  FAIL: "); fails++; } \
                              printf(__VA_ARGS__); printf("\n"); } while (0)

static int label_of(trig_edge_class_t c)
{
    return c == TRIG_EDGE_CLASS_RISING ? 1 : c == TRIG_EDGE_CLASS_FALLING ? -1 : 0;
}

static uint32_t lcg = 12345u;
static double urand(void) { lcg = lcg * 1664525u + 1013904223u; return (lcg >> 8) / 16777216.0; }

static uint32_t last_rot;
static void synth(uint8_t *rec, double periods, int want, int square, double amp)
{
    const double pi = 3.14159265358979;
    double per = 1024.0 / periods;
    double ph = -2.0 * pi * 512.0 / per + (want > 0 ? 0.0 : pi) + (urand() - 0.5) * 0.1;
    uint32_t rot = (uint32_t)(urand() * 1024.0) & 1023u;
    last_rot = rot;
    for (int n = 0; n < 1024; n++) {
        double b = sin(2.0 * pi * n / per + ph);
        if (square) b = tanh(8.0 * b);
        double v = 128.0 + amp * b + (urand() - 0.5) * 4.0;
        if (v < 0) v = 0;
        if (v > 227) v = 227;
        rec[(n + rot) & 1023u] = (uint8_t)(v + 0.5);
    }
}

int main(void)
{
    printf("[1] real records, unit #1 (EXP-55/56)\n");
    int ok = 0, wrong = 0, unk = 0;
    for (unsigned i = 0; i < EDGE_FIXTURE_N; i++) {
        int g = label_of(trig_edge_classify(EDGE_FIXTURES[i].rec, EDGE_FIXTURES[i].crossing));
        if (g == 0) unk++;
        else if (g == EDGE_FIXTURES[i].label) ok++;
        else { wrong++; printf("    wrong: %s\n", EDGE_FIXTURES[i].name); }
    }
    CHECK(wrong == 0, "%u records: %d correct, %d wrong, %d unknown", (unsigned)EDGE_FIXTURE_N, ok, wrong, unk);
    CHECK(ok * 10 >= (int)EDGE_FIXTURE_N * 8, "classifies >= 80%% of real records (%d/%u)", ok, (unsigned)EDGE_FIXTURE_N);

    printf("[2] negative control: level moved off the signal\n");
    int classified = 0;
    for (unsigned i = 0; i < EDGE_FIXTURE_N; i++)
        if (trig_edge_classify(EDGE_FIXTURES[i].rec, EDGE_FIXTURES[i].crossing + 150) != TRIG_EDGE_CLASS_UNKNOWN)
            classified++;
    CHECK(classified == 0, "no record classified at a level it never crosses (%d)", classified);

    printf("[3] synthetic sweep\n");
    static const double frac[] = { 0.2, 0.3, 0.4, 0.7, 1.3, 1.8, 2.7, 4.4, 6.6, 9.1, 15.6, 31.3 };
    int s_ok = 0, s_wrong = 0, s_unk = 0;
    uint8_t rec[1024];
    for (int sq = 0; sq < 2; sq++)
        for (unsigned f = 0; f < sizeof frac / sizeof frac[0]; f++)
            for (int i = 0; i < 100; i++) {
                int want = (i & 1) ? -1 : 1;
                synth(rec, frac[f], want, sq, 25.0 + urand() * 35.0);
                int g = label_of(trig_edge_classify(rec, 128));
                if (g == 0) s_unk++; else if (g == want) s_ok++; else s_wrong++;
            }
    CHECK(s_wrong == 0, "2400 fractional-period records: %d correct, %d wrong, %d unknown", s_ok, s_wrong, s_unk);
    CHECK(s_ok >= 1800, "classifies >= 75%% of them (%d)", s_ok);
    int int_classified = 0;
    for (int i = 0; i < 200; i++) {
        synth(rec, (double)(1 + (i % 8)), (i & 1) ? -1 : 1, i & 2, 40.0);
        if (trig_edge_classify(rec, 128) != TRIG_EDGE_CLASS_UNKNOWN) int_classified++;
    }
    CHECK(int_classified == 0, "integer periods (seam invisible) refused, not guessed (%d classified)", int_classified);

    printf("[4] seam finder recovers the rotation (un-rotation, dev plan 2.3)\n");
    int found = 0, exact = 0, off = 0;
    for (int sq = 0; sq < 2; sq++)
        for (unsigned f = 0; f < sizeof frac / sizeof frac[0]; f++)
            for (int i = 0; i < 50; i++) {
                synth(rec, frac[f], (i & 1) ? -1 : 1, sq, 25.0 + urand() * 35.0);
                uint32_t k = 9999u;
                if (!trig_edge_find_seam(rec, &k)) continue;
                found++;
                /* synth() writes sample n at (n + rot): the oldest sample
                 * (n = 0) sits at index rot. */
                if (k == last_rot) exact++; else { off++; if (off <= 3) printf("    k %u vs rot %u (periods %.1f)\n", (unsigned)k, (unsigned)last_rot, frac[f]); }
            }
    CHECK(off == 0, "every confident seam is the true rotation (%d found, %d exact, %d off)", found, exact, off);
    CHECK(found >= 900, "finds the seam on >= 75%% of 1200 fractional-period records (%d)", found);

    printf("\n%s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
