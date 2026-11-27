/*
 * acq_frame_live(): a railed input is live data, a dead bus is not (EXP-82),
 * and neither is an unconfigured part's open config port (review of #109).
 *
 * Negative controls: the gate before EXP-82 must fail the railed cases, and
 * the first EXP-82 gate (opcode phase trusted unconditionally) must fail the
 * open-port case, or these checks do not discriminate.
 */
#include <stdio.h>
#include <string.h>
#include "acq_gate.h"

static int fails, checks;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; printf("FAIL: %s\n", msg); } } while (0)

#define N 1024
static uint8_t ch1[N], ch2[N];

static void fill(uint8_t *b, uint8_t v) { memset(b, v, N); }

typedef bool (*gate_t)(uint8_t, uint8_t, const volatile uint8_t *,
                       const volatile uint8_t *, int, bool);

/* The gate before EXP-82: marker, or CH1 varies. */
static bool gate_pre_exp82(uint8_t s1, uint8_t s2, const volatile uint8_t *a,
                           const volatile uint8_t *b, int n, bool t)
{
    (void)b; (void)t;
    return s1 == 0x80 || s2 == 0x80 || acq_samples_vary(a, n);
}

/* The first EXP-82 gate (6e500b6): the opcode phase trusted on every boot. */
static bool gate_untrusting(uint8_t s1, uint8_t s2, const volatile uint8_t *a,
                            const volatile uint8_t *b, int n, bool t)
{
    (void)t;
    return acq_frame_live(s1, s2, a, b, n, true);
}

/* Each scenario: what the frame looks like, whether this boot's config left
 * the port closed (trusted), and what the gate must say. */
typedef struct {
    const char *what;
    uint8_t s1, s2, v1, v2;      /* flat sample values; vary1/2 add a step */
    bool vary1, vary2, trusted, want;
} scen_t;

static const scen_t scen[] = {
    /* Dead bus: every byte 0xFF, samples a flat 227 after the -28 offset. */
    { "dead bus, trusted",                     0xFF, 0xFF, 227, 227, false, false, true,  false },
    { "dead bus, untrusted",                   0xFF, 0xFF, 227, 227, false, false, false, false },
    /* Railed CH1 on a configured part (EXP-82: status 0x00, CH1 flat 227),
     * CH2 open and noisy: CH2's variation carries it, trusted or not. */
    { "railed CH1, noisy CH2",                 0x00, 0x00, 227, 101, false, true,  true,  true  },
    /* Both channels railed on a configured part: only the opcode phase says so. */
    { "both railed high, port closed",         0x00, 0x00, 227, 227, false, false, true,  true  },
    { "both railed low, port closed",          0x00, 0x00, 0,   0,   false, false, true,  true  },
    /* Failed config (port open, EXP-31: 0x00 in the opcode phase), flat
     * frames: never committed, so the demo trace stays. */
    { "open port, flat zero",                  0x00, 0x00, 0,   0,   false, false, false, false },
    { "open port, flat 227",                   0x00, 0x00, 227, 227, false, false, false, false },
    /* One opcode-phase byte 0xFF and flat samples: not enough evidence. */
    { "one 0xFF status, flat",                 0x00, 0xFF, 227, 227, false, false, true,  false },
    { "other 0xFF status, flat",               0xFF, 0x00, 227, 227, false, false, true,  false },
    /* The data-ready marker alone is enough (unchanged behaviour). */
    { "marker on CH1 read",                    0x80, 0xFF, 227, 227, false, false, false, true  },
    { "marker on CH2 read",                    0xFF, 0x80, 227, 227, false, false, false, true  },
    /* Varying samples alone are enough (unchanged behaviour). */
    { "varying CH1, 0xFF status",              0xFF, 0xFF, 120, 227, true,  false, false, true  },
    { "varying CH2, 0xFF status",              0xFF, 0xFF, 227, 90,  false, true,  false, true  },
};
#define N_SCEN (int)(sizeof(scen) / sizeof(scen[0]))

static int run(const char *name, gate_t g, bool verbose)
{
    int bad = 0;
    for (int i = 0; i < N_SCEN; i++) {
        const scen_t *s = &scen[i];
        fill(ch1, s->v1); fill(ch2, s->v2);
        if (s->vary1) ch1[3] = (uint8_t)(s->v1 + 1);
        if (s->vary2) ch2[N - 1] = (uint8_t)(s->v2 + 1);   /* last sample counts */
        bool got = g(s->s1, s->s2, ch1, ch2, N, s->trusted);
        if (got != s->want) {
            bad++;
            if (verbose)
                printf("FAIL: %s: want %d got %d\n", s->what, s->want, got);
        }
    }
    printf("%-26s %d/%d scenarios as expected\n", name, N_SCEN - bad, N_SCEN);
    return bad;
}

int main(void)
{
    checks++;
    if (run("acq_frame_live", acq_frame_live, true) != 0)
        fails++;

    /* Negative controls: each must get at least one scenario wrong. */
    checks++;
    if (run("control: pre-EXP-82 gate", gate_pre_exp82, false) == 0) {
        fails++;
        printf("FAIL: control passed: scenarios do not catch the railed freeze\n");
    }
    checks++;
    if (run("control: untrusting gate", gate_untrusting, false) == 0) {
        fails++;
        printf("FAIL: control passed: scenarios do not catch the open config port\n");
    }

    /* The trust decision: only a read that ran and found no IDCODE. */
    CHECK(acq_opphase_trusted(true, -1),   "anchor read, silent: trusted (configured, cold or warm)");
    CHECK(!acq_opphase_trusted(true, 0),   "anchor read, IDCODE at bit 0: open port, not trusted");
    CHECK(!acq_opphase_trusted(true, 17),  "anchor read, IDCODE at bit 17: open port, not trusted");
    CHECK(!acq_opphase_trusted(false, -1), "no anchor read (preset -1): not trusted");
    CHECK(!acq_opphase_trusted(false, 0),  "no anchor read, zeroed field: not trusted");

    /* acq_samples_vary looks at the whole record, last sample included. */
    fill(ch1, 5); ch1[N - 1] = 6;
    CHECK(acq_samples_vary(ch1, N), "difference in the last sample");
    fill(ch1, 5);
    CHECK(!acq_samples_vary(ch1, N), "flat record");

    printf("test_acq_gate: %d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
