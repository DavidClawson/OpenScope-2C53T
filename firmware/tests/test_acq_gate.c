/*
 * acq_frame_live(): a railed input is live data, a dead bus is not (EXP-82).
 */
#include <stdio.h>
#include <string.h>
#include "acq_gate.h"

static int fails, checks;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; printf("FAIL: %s\n", msg); } } while (0)

#define N 1024
static uint8_t ch1[N], ch2[N];

static void fill(uint8_t *b, uint8_t v) { memset(b, v, N); }

int main(void)
{
    /* Dead bus: every byte 0xFF, samples a flat 227 after the -28 offset. */
    fill(ch1, 227); fill(ch2, 227);
    CHECK(!acq_frame_live(0xFF, 0xFF, ch1, ch2, N), "dead bus is rejected");

    /* Railed CH1 on a live bus (EXP-82: status byte 0x00, CH1 flat 227),
     * CH2 open and noisy. */
    fill(ch1, 227); fill(ch2, 101); ch2[500] = 102;
    CHECK(acq_frame_live(0x00, 0x00, ch1, ch2, N), "railed CH1, noisy CH2, live status");

    /* Both channels railed on a live bus: only the status bytes say so. */
    fill(ch1, 227); fill(ch2, 227);
    CHECK(acq_frame_live(0x00, 0x00, ch1, ch2, N), "both railed, live status");

    /* Both railed at the bottom (raw <= 28 clamps to 0). */
    fill(ch1, 0); fill(ch2, 0);
    CHECK(acq_frame_live(0x00, 0x00, ch1, ch2, N), "both railed low, live status");

    /* One status byte 0xFF and flat samples: not enough evidence. */
    fill(ch1, 227); fill(ch2, 227);
    CHECK(!acq_frame_live(0x00, 0xFF, ch1, ch2, N), "one 0xFF status, flat: rejected");
    CHECK(!acq_frame_live(0xFF, 0x00, ch1, ch2, N), "other 0xFF status, flat: rejected");

    /* The data-ready marker alone is enough (unchanged behaviour). */
    CHECK(acq_frame_live(0x80, 0xFF, ch1, ch2, N), "marker on CH1 read");
    CHECK(acq_frame_live(0xFF, 0x80, ch1, ch2, N), "marker on CH2 read");

    /* Varying CH1 alone is enough (unchanged behaviour), even with 0xFF status. */
    fill(ch1, 120); ch1[3] = 121; fill(ch2, 227);
    CHECK(acq_frame_live(0xFF, 0xFF, ch1, ch2, N), "varying CH1");

    /* Varying CH2 proves the bus is live too. */
    fill(ch1, 227); fill(ch2, 90); ch2[1023] = 91;
    CHECK(acq_frame_live(0xFF, 0xFF, ch1, ch2, N), "varying CH2");

    /* acq_samples_vary looks at the whole record, last sample included. */
    fill(ch1, 5); ch1[N - 1] = 6;
    CHECK(acq_samples_vary(ch1, N), "difference in the last sample");
    fill(ch1, 5);
    CHECK(!acq_samples_vary(ch1, N), "flat record");

    printf("test_acq_gate: %d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
