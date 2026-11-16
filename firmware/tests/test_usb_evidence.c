/*
 * Host tests for the CDC wedge evidence record (src/util/usb_evidence.c,
 * issue #39).
 *
 * The record sits in .noinit so it survives the reset that recovers a wedged
 * port. The price is that after a cold power-up the same bytes hold SRAM
 * noise, and the one thing that must never happen is noise being printed as
 * "previous session: stall pending". So most of what is tested here is the
 * trust rule — magic AND checksum, over every byte — and then the session
 * lifecycle the shell builds on it (pending set by a stall, cleared by a
 * completed send, carried across exactly one boot).
 *
 * Build: make -C firmware test-usb-evidence
 *   (or: cc -std=c11 -Wall -Wextra -Werror -Isrc/util \
 *         tests/test_usb_evidence.c src/util/usb_evidence.c)
 *
 * Also the target of a mutation check in scripts/test_usb_evidence.py: each
 * guard in the unit is deleted in turn and these tests are required to fail.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "usb_evidence.h"

static int tests_run = 0;
static int tests_failed = 0;
static int current_failed = 0;

#define CHECK(cond, ...) do {                       \
    if (!(cond)) {                                  \
        printf("  FAIL (line %d): ", __LINE__);     \
        printf(__VA_ARGS__);                        \
        printf("\n");                               \
        current_failed++;                           \
    }                                               \
} while (0)

#define RUN(fn) do {                                \
    current_failed = 0;                             \
    printf("%s\n", #fn);                            \
    fn();                                           \
    tests_run++;                                    \
    if (current_failed) { tests_failed++; printf("  -> FAILED\n"); }   \
    else { printf("  ok\n"); }                      \
} while (0)

/* Deterministic "power-up SRAM": xorshift32, so a failure is reproducible. */
static uint32_t rng_state = 0x2C53u;
static uint32_t rng(void)
{
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

static void fill_noise(usb_evidence_t *ev)
{
    uint8_t *p = (uint8_t *)ev;
    for (size_t i = 0; i < sizeof(*ev); i++) p[i] = (uint8_t)rng();
}

/* A record as a session in progress would leave it: begun, one stall. */
static void make_session(usb_evidence_t *ev, bool pending)
{
    usb_evidence_t prev;
    fill_noise(ev);
    (void)usb_ev_begin_session(ev, &prev);
    usb_ev_alive(ev, 1000u, 10u, 1u, 2u, 3u);
    usb_ev_stall(ev, 1234u, 0x00003021u, false, true, false);
    if (!pending) usb_ev_send_completed(ev);
}

/* ── layout ──────────────────────────────────────────────────────────── */
static void test_layout_is_tiny_and_check_is_last(void)
{
    CHECK(sizeof(usb_evidence_t) <= 64u, "record is %zu B (> 64)", sizeof(usb_evidence_t));
    CHECK(offsetof(usb_evidence_t, check) == sizeof(usb_evidence_t) - 4u,
          "check is not the last 4 bytes");
}

/* ── trust rule: cold power-up never reads as a previous session ─────── */
static void test_cold_noise_is_not_a_session(void)
{
    for (int round = 0; round < 1000; round++) {
        usb_evidence_t ev, prev;
        fill_noise(&ev);
        memset(&prev, 0xA5, sizeof(prev));
        usb_ev_prev_t k = usb_ev_begin_session(&ev, &prev);
        CHECK(k == USB_EV_PREV_NONE, "round %d: noise accepted as a session", round);
        CHECK(prev.magic == 0 && prev.seq == 0 && prev.flags == 0,
              "round %d: invalid record copied into prev", round);
        CHECK(ev.seq == 1u, "round %d: cold power-up must start at session 1, got %u",
              round, (unsigned)ev.seq);
        if (current_failed) return;
    }
}

static void test_blank_ram_is_not_a_session(void)
{
    usb_evidence_t ev, prev;
    memset(&ev, 0x00, sizeof(ev));
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE, "all-zero RAM accepted");
    memset(&ev, 0xFF, sizeof(ev));
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE, "all-0xFF RAM accepted");
}

/* The magic alone is not enough: noise that happens to carry it (or a
 * record torn by a reset mid-update) must still be refused. This is the test
 * that goes red when the checksum comparison is removed. */
static void test_magic_without_checksum_is_refused(void)
{
    for (int round = 0; round < 1000; round++) {
        usb_evidence_t ev;
        fill_noise(&ev);
        ev.magic = USB_EV_MAGIC;
        ev.flags |= USB_EV_FLAG_PENDING;
        if (ev.check == usb_ev_checksum(&ev)) ev.check ^= 1u;   /* not by luck */
        CHECK(!usb_ev_valid(&ev), "round %d: magic + wrong checksum accepted", round);
        usb_evidence_t prev;
        CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE,
              "round %d: noise with the magic reported as a previous session", round);
        if (current_failed) return;
    }
}

/* Every byte before `check` is covered: flip any one bit and the record is
 * refused. Catches a checksum that only covers part of the record. */
static void test_any_single_bit_flip_is_refused(void)
{
    usb_evidence_t good;
    make_session(&good, true);
    CHECK(usb_ev_valid(&good), "baseline record invalid");

    for (size_t i = 0; i < sizeof(good); i++) {
        for (unsigned bit = 0; bit < 8; bit++) {
            usb_evidence_t ev = good;
            ((uint8_t *)&ev)[i] ^= (uint8_t)(1u << bit);
            CHECK(!usb_ev_valid(&ev), "byte %zu bit %u flipped, still valid", i, bit);
            if (current_failed) return;
        }
    }
}

/* The checksum alone is not enough either: a sealed record carrying another
 * magic (another firmware's struct, an older layout) is not ours. */
static void test_checksum_without_magic_is_refused(void)
{
    usb_evidence_t ev;
    make_session(&ev, true);
    ev.magic = 0xFA17ED00u;          /* fault.c's magic: a neighbour in .noinit */
    usb_ev_seal(&ev);
    CHECK(!usb_ev_valid(&ev), "foreign magic with a matching checksum accepted");
    usb_evidence_t prev;
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE,
          "foreign record reported as a previous session");
}

/* ── session lifecycle ───────────────────────────────────────────────── */
static void test_stall_then_reset_reports_pending(void)
{
    usb_evidence_t ev, prev;
    make_session(&ev, true);
    uint16_t seq = ev.seq;

    usb_ev_prev_t k = usb_ev_begin_session(&ev, &prev);
    CHECK(k == USB_EV_PREV_PENDING, "stall then reset not reported as pending (%d)", (int)k);
    CHECK(prev.seq == seq, "prev.seq %u != %u", (unsigned)prev.seq, (unsigned)seq);
    CHECK(prev.stall_tick == 1234u, "stall tick lost: %lu", (unsigned long)prev.stall_tick);
    CHECK(prev.stall_ept == 0x00003021u, "EPT1 snapshot lost: 0x%08lx",
          (unsigned long)prev.stall_ept);
    CHECK((prev.flags & USB_EV_FLAG_STALL_DTR) != 0, "DTR at stall lost");
    CHECK((prev.flags & USB_EV_FLAG_STALL_TXC) == 0, "tx_completed=0 recorded as 1");
    CHECK(prev.tx_stalls == 1u && prev.tx_host_slow == 0u, "stall counters wrong");
    CHECK(prev.alive_tick == 1000u, "alive tick lost");
    CHECK(prev.rx_frames_ok == 10u && prev.rx_bad_checksum == 1u &&
          prev.rx_bad_length == 2u && prev.rx_gap_timeouts == 3u, "rx stats lost");

    /* ...and the new session starts clean. */
    CHECK(usb_ev_valid(&ev), "new session record not sealed");
    CHECK(ev.seq == (uint16_t)(seq + 1u), "seq not advanced: %u", (unsigned)ev.seq);
    CHECK(ev.flags == 0 && ev.tx_stalls == 0 && ev.stall_tick == 0 && ev.alive_tick == 0,
          "new session inherited the previous session's evidence");
}

static void test_completed_send_clears_pending(void)
{
    usb_evidence_t ev, prev;
    make_session(&ev, false);
    CHECK(usb_ev_valid(&ev), "record invalid after send_completed");
    CHECK((ev.flags & USB_EV_FLAG_PENDING) == 0, "pending survived a completed send");
    CHECK(ev.stall_tick == 1234u, "clearing pending must keep the last stall snapshot");
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_CLEAN,
          "stall followed by a completed send reported as pending");
}

static void test_heal_keeps_its_own_stall_snapshot(void)
{
    /* EXP-66: the wedge that fired the heal was a lost completion
     * (ept NAK), and the host_slow stalls that followed (the host had
     * abandoned the port) overwrote the last-stall snapshot. The heal's own
     * fields must survive them and the reset. */
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    CHECK(ev.heal_stall_tick == 0u && ev.heal_stall_ept == 0u && ev.heals == 0u,
          "a new session must start with no heal");
    usb_ev_stall(&ev, 5000u, 0x00003021u, false, true, false);   /* lost completion #1 */
    usb_ev_stall(&ev, 6000u, 0x00003021u, false, true, false);   /* lost completion #2 */
    usb_ev_heal(&ev);
    usb_ev_stall(&ev, 9000u, 0x00003031u, false, false, true);   /* host_slow after the heal */
    usb_ev_stall(&ev, 10000u, 0x00003031u, false, false, true);
    CHECK(ev.heals == 1u, "heal not counted");
    CHECK(ev.heal_stall_tick == 6000u, "heal snapshot tick %lu != 6000", (unsigned long)ev.heal_stall_tick);
    CHECK(ev.heal_stall_ept == 0x00003021u, "heal snapshot ept 0x%08lx overwritten",
          (unsigned long)ev.heal_stall_ept);
    CHECK(ev.stall_tick == 10000u && ev.stall_ept == 0x00003031u, "last-stall snapshot must still move");
    CHECK(usb_ev_valid(&ev), "record not sealed after the heal");
    usb_evidence_t next = ev;
    CHECK(usb_ev_begin_session(&next, &prev) == USB_EV_PREV_PENDING, "reset after the heal");
    CHECK(prev.heal_stall_tick == 6000u && prev.heal_stall_ept == 0x00003021u,
          "heal snapshot lost across the reset");
}

static void test_completed_send_without_stall_is_free(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    uint32_t check = ev.check;
    for (int i = 0; i < 100; i++) usb_ev_send_completed(&ev);
    CHECK(ev.check == check && usb_ev_valid(&ev), "idle completions changed the record");
}

static void test_every_update_reseals(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    CHECK(usb_ev_valid(&ev), "begin_session left the record unsealed");
    usb_ev_alive(&ev, 5u, 1u, 0u, 0u, 0u);
    CHECK(usb_ev_valid(&ev), "alive left the record unsealed");
    usb_ev_stall(&ev, 6u, 0x30u, false, false, true);
    CHECK(usb_ev_valid(&ev), "stall left the record unsealed");
    usb_ev_heal(&ev);
    CHECK(usb_ev_valid(&ev), "heal left the record unsealed");
    usb_ev_send_completed(&ev);
    CHECK(usb_ev_valid(&ev), "send_completed left the record unsealed");
}

static void test_host_slow_and_snapshot_flags(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    usb_ev_stall(&ev, 10u, 0x00003031u, false, false, true);   /* VALID: host not reading */
    usb_ev_stall(&ev, 20u, 0x00003021u, true, true, false);    /* last stall wins */
    CHECK(ev.tx_stalls == 2u && ev.tx_host_slow == 1u, "stalls=%lu host_slow=%lu",
          (unsigned long)ev.tx_stalls, (unsigned long)ev.tx_host_slow);
    CHECK(ev.stall_tick == 20u && ev.stall_ept == 0x00003021u, "snapshot is not the last stall");
    CHECK(ev.flags == (USB_EV_FLAG_PENDING | USB_EV_FLAG_STALL_DTR | USB_EV_FLAG_STALL_TXC),
          "flags 0x%02x", (unsigned)ev.flags);
}

static void test_heals_saturate(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    ev.heals = 0xFFFEu;
    usb_ev_seal(&ev);
    usb_ev_heal(&ev);
    usb_ev_heal(&ev);
    CHECK(ev.heals == 0xFFFFu, "heals wrapped: %u", (unsigned)ev.heals);
}

static void test_alive_copies_and_saturates_rx(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    usb_ev_alive(&ev, 77u, 0x12345678u, 0x10000u, 0xFFFFu, 0xFFFFFFFFu);
    CHECK(ev.alive_tick == 77u, "alive tick not stored");
    CHECK(ev.rx_frames_ok == 0x12345678u, "frames_ok truncated");
    CHECK(ev.rx_bad_checksum == 0xFFFFu && ev.rx_bad_length == 0xFFFFu &&
          ev.rx_gap_timeouts == 0xFFFFu, "16-bit rx counters wrapped instead of saturating");
    uint32_t check = ev.check;
    usb_ev_alive(&ev, 77u, 0x12345678u, 0x10000u, 0xFFFFu, 0xFFFFFFFFu);
    CHECK(ev.check == check, "unchanged alive pass rewrote the record");
}

/* A reset can land between a field write and the reseal: that session's
 * evidence is then lost — never reported as something it is not. */
static void test_torn_update_is_dropped_not_misreported(void)
{
    usb_evidence_t ev, prev;
    make_session(&ev, false);
    ev.flags |= USB_EV_FLAG_PENDING;     /* field written, reset before the seal */
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE,
          "torn record trusted");
    CHECK(ev.seq == 1u, "torn record must restart the session count");
}

static void test_seq_counts_resets_and_skips_zero(void)
{
    usb_evidence_t ev, prev;
    fill_noise(&ev);
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_NONE, "noise accepted");
    CHECK(ev.seq == 1u, "first session %u", (unsigned)ev.seq);
    CHECK(usb_ev_begin_session(&ev, &prev) == USB_EV_PREV_CLEAN, "clean reset not recognised");
    CHECK(prev.seq == 1u && ev.seq == 2u, "prev %u new %u", (unsigned)prev.seq, (unsigned)ev.seq);

    ev.seq = 0xFFFFu;
    usb_ev_seal(&ev);
    (void)usb_ev_begin_session(&ev, &prev);
    CHECK(ev.seq == 1u, "seq wrapped to %u (0 never names a session)", (unsigned)ev.seq);
}

/* The previous session is reported for a whole session, while the record
 * keeps changing underneath: the copy must not alias it. */
static void test_prev_is_a_copy(void)
{
    usb_evidence_t ev, prev;
    make_session(&ev, true);
    (void)usb_ev_begin_session(&ev, &prev);
    usb_ev_stall(&ev, 99u, 0u, false, false, false);
    CHECK(prev.stall_tick == 1234u, "prev changed with the live record");
}

int main(void)
{
    printf("=== usb_evidence host tests ===\n");
    RUN(test_layout_is_tiny_and_check_is_last);
    RUN(test_cold_noise_is_not_a_session);
    RUN(test_blank_ram_is_not_a_session);
    RUN(test_magic_without_checksum_is_refused);
    RUN(test_any_single_bit_flip_is_refused);
    RUN(test_checksum_without_magic_is_refused);
    RUN(test_stall_then_reset_reports_pending);
    RUN(test_completed_send_clears_pending);
    RUN(test_heal_keeps_its_own_stall_snapshot);
    RUN(test_completed_send_without_stall_is_free);
    RUN(test_every_update_reseals);
    RUN(test_host_slow_and_snapshot_flags);
    RUN(test_heals_saturate);
    RUN(test_alive_copies_and_saturates_rx);
    RUN(test_torn_update_is_dropped_not_misreported);
    RUN(test_seq_counts_resets_and_skips_zero);
    RUN(test_prev_is_a_copy);
    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
