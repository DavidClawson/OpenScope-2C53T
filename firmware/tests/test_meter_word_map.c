/*
 * test_meter_word_map.c — the executable specification for the meter
 * function-word table.
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │  THIS TEST IS EXPECTED TO FAIL RIGHT NOW.  IT IS NOT A BROKEN BUILD. │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * `stock_meter_cmd_low[]` in src/drivers/fpga_meter_plan.c selects the WRONG
 * meter function for 10 of our 11 submodes.  Only Temperature is right, and
 * that by coincidence.  Submode 0 "DCV" sends 0x0514, which is the meter SoC's
 * *Auto* word.  This test encodes what the table should be, so that when the
 * correction lands the test flipping to green IS the verification.
 *
 * Deliberately NOT wired into `make test-meter` or any other aggregate target:
 * a red aggregate teaches people to ignore aggregates.  Run it by name:
 *
 *     make test-meter-word-map
 *
 * ── Provenance of the reference map ──────────────────────────────────────
 *
 * GitHub issue #15, DavidClawson/OpenScope-2C53T — comment by Stlkv
 * (contributor, 2026-09-07), MEASURED on bench unit #2.  Not reverse
 * engineered: he patched a logger into stock V1.2.0 itself (a code cave hooked
 * into the dvom TX task and SysTick, writing every TX frame into an SRAM block
 * above stock's stack, read back through a MENU+Power IAP round trip), then
 * walked stock's own meter menu and recorded one word per function.
 *
 * Cross-checks that this file must not be edited without repeating:
 *   - Every one of the eight low bytes in our own `stock_meter_cmd_low[]`
 *     appears somewhere in these twelve.  The bytes were recovered correctly;
 *     it was the function ASSIGNMENT that was wrong.
 *   - `scripts/test_stock_meter_literals.py` pins those eight bytes against the
 *     stock binary itself, so the byte set has independent binary grounding.
 *   - EXP-25 (docs/experiments/2026-09-12-25-meter-tx-header-aa55.md) commanded
 *     0x050C, 0x0514, 0x050B and 0x050A on bench unit #1 and got distinct,
 *     correctly-shaped frame families back, which is cross-unit agreement on
 *     four of the twelve rows.
 *
 * NOTHING IN THIS FILE WAS MEASURED BY THE AUTHOR OF THIS FILE.  The reference
 * is a transcription of a published third-party measurement.  If it is ever
 * found to disagree with the bench, the bench wins and this file is the thing
 * that changes.
 *
 * Two words appear in stock's accepted set but are NOT function selectors, and
 * must never be used as one:
 *   0x0513 — the SoC's LIVE screen; its frame is seven-segment text "L1uE",
 *            not a value.
 *   0x050F — accepted by the SoC but never sent by stock; meaning unknown.
 * HOLD sends no word at all.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fpga_meter_plan.h"

#define ISSUE_URL "https://github.com/DavidClawson/OpenScope-2C53T/issues/15"

static int failures;
static int mismatched_submodes;

/* ───────────────────────── the reference map ───────────────────────────── */

typedef struct {
    uint16_t word;
    const char *stock_function;
} stock_word_t;

/*
 * The twelve words stock sends, exactly as published in issue #15.
 * Order here is the order of the table in that comment (left column top to
 * bottom, then right column), so it can be diffed against the source by eye.
 */
static const stock_word_t stock_word_map[] = {
    { 0x0514u, "Auto" },
    { 0x050Cu, "DC Voltage" },
    { 0x050Du, "AC Voltage" },
    { 0x0517u, "Continuity" },
    { 0x050Bu, "Resistance" },
    { 0x050Au, "Capacitance" },
    { 0x050Eu, "Diode" },
    { 0x0512u, "Temperature" },
    { 0x0511u, "Small DC current" },
    { 0x0516u, "Small AC current" },
    { 0x0510u, "Large DC current" },
    { 0x0515u, "Large AC current" },
};
#define STOCK_WORD_MAP_COUNT \
    (sizeof(stock_word_map) / sizeof(stock_word_map[0]))

/* Accepted by the SoC, but not function selectors. Never a valid answer. */
static const stock_word_t stock_non_function_words[] = {
    { 0x0513u, "LIVE screen (text \"L1uE\", not a value)" },
    { 0x050Fu, "accepted but never sent by stock; meaning unknown" },
};
#define STOCK_NON_FUNCTION_WORD_COUNT \
    (sizeof(stock_non_function_words) / sizeof(stock_non_function_words[0]))

/*
 * Our eleven UI submodes against that map.
 *
 * This is the whole point of the file: it is the mapping, not the byte set,
 * that is wrong today.  Each row names our submode and the stock function it
 * must select.
 */
typedef struct {
    uint8_t submode;
    const char *ui_name;
    uint16_t want_word;
    const char *want_stock_function;
} submode_expectation_t;

static const submode_expectation_t submode_expectations[] = {
    {  0, "DCV",         0x050Cu, "DC Voltage"       },
    {  1, "ACV",         0x050Du, "AC Voltage"       },
    {  2, "DC mA",       0x0511u, "Small DC current" },
    {  3, "DC A",        0x0510u, "Large DC current" },
    {  4, "AC mA",       0x0516u, "Small AC current" },
    {  5, "AC A",        0x0515u, "Large AC current" },
    {  6, "Resistance",  0x050Bu, "Resistance"       },
    {  7, "Continuity",  0x0517u, "Continuity"       },
    {  8, "Diode",       0x050Eu, "Diode"            },
    {  9, "Capacitance", 0x050Au, "Capacitance"      },
    { 10, "Temperature", 0x0512u, "Temperature"      },
};
#define SUBMODE_EXPECTATION_COUNT \
    (sizeof(submode_expectations) / sizeof(submode_expectations[0]))

/* ───────────────────────────── helpers ─────────────────────────────────── */

static const char *stock_function_for_word(uint16_t word)
{
    size_t i;

    for (i = 0; i < STOCK_WORD_MAP_COUNT; i++) {
        if (stock_word_map[i].word == word) {
            return stock_word_map[i].stock_function;
        }
    }
    for (i = 0; i < STOCK_NON_FUNCTION_WORD_COUNT; i++) {
        if (stock_non_function_words[i].word == word) {
            return stock_non_function_words[i].stock_function;
        }
    }
    return "NOT IN THE STOCK MAP";
}

static void fail(const char *line)
{
    printf("  FAIL  %s\n", line);
    failures++;
}

/* ───────────────────────────── the tests ───────────────────────────────── */

/*
 * Guard the reference itself. A reference table nobody checks is how this
 * project has repeatedly ended up with a stable, plausible, wrong number.
 */
static void test_reference_map_is_self_consistent(void)
{
    size_t i;
    size_t j;
    char line[192];

    printf("[1] reference map integrity (issue #15 transcription)\n");

    if (STOCK_WORD_MAP_COUNT != 12u) {
        snprintf(line, sizeof(line),
                 "reference map has %u rows, issue #15 published 12",
                 (unsigned)STOCK_WORD_MAP_COUNT);
        fail(line);
    }

    for (i = 0; i < STOCK_WORD_MAP_COUNT; i++) {
        uint16_t w = stock_word_map[i].word;

        if ((w & 0xFF00u) != 0x0500u) {
            snprintf(line, sizeof(line),
                     "%s word 0x%04X is not in the 0x05xx raw-selector family",
                     stock_word_map[i].stock_function, w);
            fail(line);
        }
        for (j = i + 1; j < STOCK_WORD_MAP_COUNT; j++) {
            if (stock_word_map[j].word == w) {
                snprintf(line, sizeof(line),
                         "duplicate word 0x%04X (%s / %s)", w,
                         stock_word_map[i].stock_function,
                         stock_word_map[j].stock_function);
                fail(line);
            }
        }
        for (j = 0; j < STOCK_NON_FUNCTION_WORD_COUNT; j++) {
            if (stock_non_function_words[j].word == w) {
                snprintf(line, sizeof(line),
                         "0x%04X is listed as a function selector but issue #15 "
                         "records it as %s", w,
                         stock_non_function_words[j].stock_function);
                fail(line);
            }
        }
    }

    /* Every expectation row must name a word that is actually in the map. */
    for (i = 0; i < SUBMODE_EXPECTATION_COUNT; i++) {
        const submode_expectation_t *e = &submode_expectations[i];
        bool found = false;

        for (j = 0; j < STOCK_WORD_MAP_COUNT; j++) {
            if (stock_word_map[j].word == e->want_word) {
                found = true;
                break;
            }
        }
        if (!found) {
            snprintf(line, sizeof(line),
                     "submode %u (%s) expects 0x%04X, which is not in the "
                     "twelve-word map", (unsigned)e->submode, e->ui_name,
                     e->want_word);
            fail(line);
        }
    }

    if (SUBMODE_EXPECTATION_COUNT != (size_t)FPGA_METER_LOCAL_SUBMODE_COUNT) {
        snprintf(line, sizeof(line),
                 "expectation rows (%u) != FPGA_METER_LOCAL_SUBMODE_COUNT (%u)",
                 (unsigned)SUBMODE_EXPECTATION_COUNT,
                 (unsigned)FPGA_METER_LOCAL_SUBMODE_COUNT);
        fail(line);
    }
    printf("      %u function words + %u documented non-function words\n",
           (unsigned)STOCK_WORD_MAP_COUNT,
           (unsigned)STOCK_NON_FUNCTION_WORD_COUNT);
}

/*
 * THE ONE THAT MATTERS. fpga_meter_stock_cmd_word_for_submode() is what ends
 * up on the wire once the AA 55 header is on.
 */
static void test_every_submode_sends_its_own_function_word(void)
{
    size_t i;

    printf("[2] submode -> wire word  (fpga_meter_stock_cmd_word_for_submode)\n");
    printf("       %-3s %-12s %-7s %-20s %-7s %s\n",
           "sm", "our UI mode", "sends", "which really means",
           "wants", "which is");

    for (i = 0; i < SUBMODE_EXPECTATION_COUNT; i++) {
        const submode_expectation_t *e = &submode_expectations[i];
        uint16_t got = fpga_meter_stock_cmd_word_for_submode(e->submode);
        bool ok = (got == e->want_word);

        printf("  %s %-3u %-12s 0x%04X  %-20s 0x%04X  %s\n",
               ok ? " ok " : "FAIL", (unsigned)e->submode, e->ui_name,
               got, stock_function_for_word(got),
               e->want_word, e->want_stock_function);
        if (!ok) {
            failures++;
            mismatched_submodes++;
        }
    }
}

/*
 * The low-byte accessor the plan builder reads through. Same claim, one layer
 * down, so a correction that fixes only the submode switch and leaves the
 * table unable to express a word still gets caught.
 */
static void test_every_required_low_byte_is_reachable(void)
{
    size_t i;
    size_t j;
    uint8_t mode;
    char line[192];

    printf("[3] required low bytes reachable through "
           "fpga_meter_stock_cmd_low_for_mode()\n");

    for (i = 0; i < SUBMODE_EXPECTATION_COUNT; i++) {
        const submode_expectation_t *e = &submode_expectations[i];
        uint8_t want_low = (uint8_t)(e->want_word & 0xFFu);
        bool reachable = false;

        for (mode = 0; mode < FPGA_METER_STOCK_MODE_COUNT; mode++) {
            if (fpga_meter_stock_cmd_low_for_mode(mode) == want_low) {
                reachable = true;
                break;
            }
        }
        if (!reachable) {
            snprintf(line, sizeof(line),
                     "0x%02X (%s, for submode %u %s) is in no stock slot at "
                     "all -- the table cannot express it",
                     want_low, e->want_stock_function, (unsigned)e->submode,
                     e->ui_name);
            fail(line);
        }
    }

    /*
     * And the converse, which PASSES today and is worth keeping: every byte the
     * table can emit is a word stock is known to send. This is what says the
     * reverse engineering was sound and only the assignment was wrong -- and it
     * is what would catch an invented selector byte.
     */
    for (mode = 0; mode < FPGA_METER_STOCK_MODE_COUNT; mode++) {
        uint16_t word = (uint16_t)(0x0500u |
                                   fpga_meter_stock_cmd_low_for_mode(mode));
        bool known = false;

        for (j = 0; j < STOCK_WORD_MAP_COUNT; j++) {
            if (stock_word_map[j].word == word) {
                known = true;
                break;
            }
        }
        if (!known) {
            snprintf(line, sizeof(line),
                     "stock slot %u emits 0x%04X, which stock never sends (%s)",
                     (unsigned)mode, word, stock_function_for_word(word));
            fail(line);
        }
    }
}

/*
 * Eleven submodes are eleven distinct meter functions. The shared-slot
 * fallbacks in fpga_meter_plan.c existed only because our table lacked 0x0D,
 * 0x0E, 0x15 and 0x16; issue #15 supplies all four, so the constraint that
 * forced the sharing is gone and no two submodes may collide.
 */
static void test_no_two_submodes_share_a_word(void)
{
    size_t i;
    size_t j;
    char line[192];

    printf("[4] no two submodes share a function word\n");

    for (i = 0; i < SUBMODE_EXPECTATION_COUNT; i++) {
        for (j = i + 1; j < SUBMODE_EXPECTATION_COUNT; j++) {
            uint16_t a = fpga_meter_stock_cmd_word_for_submode(
                submode_expectations[i].submode);
            uint16_t b = fpga_meter_stock_cmd_word_for_submode(
                submode_expectations[j].submode);

            if (a == b) {
                snprintf(line, sizeof(line),
                         "submodes %u (%s) and %u (%s) both send 0x%04X (%s)",
                         (unsigned)submode_expectations[i].submode,
                         submode_expectations[i].ui_name,
                         (unsigned)submode_expectations[j].submode,
                         submode_expectations[j].ui_name,
                         a, stock_function_for_word(a));
                fail(line);
            }
        }
    }
}

/*
 * Out-of-range submodes must refuse rather than fall through to a word. A
 * correction that widens the table must not widen this.
 */
static void test_invalid_submode_still_refuses(void)
{
    char line[192];
    uint16_t got;

    printf("[5] out-of-range submode refuses\n");

    got = fpga_meter_stock_cmd_word_for_submode(FPGA_METER_LOCAL_SUBMODE_COUNT);
    if (got != FPGA_METER_INVALID_SELECTOR_WORD) {
        snprintf(line, sizeof(line),
                 "submode %u returned 0x%04X, want the invalid marker 0x%04X",
                 (unsigned)FPGA_METER_LOCAL_SUBMODE_COUNT, got,
                 (unsigned)FPGA_METER_INVALID_SELECTOR_WORD);
        fail(line);
    }
    if (fpga_meter_stock_cmd_word_for_submode(0xFFu) !=
        FPGA_METER_INVALID_SELECTOR_WORD) {
        fail("submode 0xFF did not return the invalid marker");
    }
}

/* ───────────────────────────── reporting ───────────────────────────────── */

static void print_banner(void)
{
    printf("\n");
    printf("=========================================================================\n");
    printf(" meter function-word map  --  executable spec for issue #15\n");
    printf(" %s\n", ISSUE_URL);
    printf("\n");
    printf(" THIS TEST IS EXPECTED TO FAIL until the corrected word table lands.\n");
    printf(" A red result here is a DOCUMENTED, KNOWN DEFECT, not a broken build.\n");
    printf(" It is deliberately excluded from `make test-meter` for that reason.\n");
    printf("\n");
    printf(" Reference: twelve stock meter words measured on bench unit #2 by\n");
    printf(" contributor Stlkv with a logger patched into stock V1.2.0, published\n");
    printf(" in issue #15. Transcribed here, not measured here.\n");
    printf("=========================================================================\n");
    printf("\n");
}

static void print_expected_failure_summary(void)
{
    printf("\n");
    printf("-------------------------------------------------------------------------\n");
    printf(" RESULT: FAIL  --  %d check(s); %d of %u submodes send the wrong word.\n",
           failures, mismatched_submodes, (unsigned)SUBMODE_EXPECTATION_COUNT);
    printf("\n");
    printf(" THIS IS THE EXPECTED RESULT ON main TODAY. Nothing is broken.\n");
    printf("\n");
    printf(" Cause: stock_meter_cmd_low[] in src/drivers/fpga_meter_plan.c ties the\n");
    printf(" right bytes to the wrong functions, and lacks 0x0D / 0x0E / 0x15 / 0x16\n");
    printf(" entirely -- which is why the shared-slot fallbacks in that file exist.\n");
    printf("\n");
    printf(" Fix: the correction is contributor work, inbound as a PR on issue #15.\n");
    printf(" Do NOT patch the table to silence this test as a side errand, and do\n");
    printf(" NOT flip the AA 55 TX header default (fpga.c) while it is still red:\n");
    printf(" with the header on and the table wrong the meter OBEYS wrong function\n");
    printf(" words and drives its relays into the wrong mode.\n");
    printf("\n");
    printf(" When the PR lands, this test going green IS the verification.\n");
    printf(" See %s\n", ISSUE_URL);
    printf("-------------------------------------------------------------------------\n");
}

static void print_pass_summary(void)
{
    printf("\n");
    printf("-------------------------------------------------------------------------\n");
    printf(" RESULT: PASS -- every submode selects its own stock function word.\n");
    printf("\n");
    printf(" The issue #15 correction has landed. Two follow-ups this test cannot\n");
    printf(" cover, both bench work:\n");
    printf("   * eight of the twelve words have never been on our wire at all:\n");
    printf("     0x050D ACV, 0x050E diode, 0x0510 large DC A, 0x0511 small DC mA,\n");
    printf("     0x0512 temperature, 0x0515 large AC A, 0x0516 small AC mA,\n");
    printf("     0x0517 continuity. Needs a capacitor, a diode and a thermocouple.\n");
    printf("   * a correct word selected is not a correct value read.\n");
    printf("-------------------------------------------------------------------------\n");
}

int main(void)
{
    print_banner();

    test_reference_map_is_self_consistent();
    test_every_submode_sends_its_own_function_word();
    test_every_required_low_byte_is_reachable();
    test_no_two_submodes_share_a_word();
    test_invalid_submode_still_refuses();

    if (failures) {
        print_expected_failure_summary();
        return 1;
    }

    print_pass_summary();
    return 0;
}
