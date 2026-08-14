/*
 * cal_dump.c — read-only report on MCU flash 0x08006000..0x08006FFF.
 *
 * See cal_dump.h for why this exists and why it is safe. Short version: it
 * reads 4 KB and draws them. It writes nothing, anywhere, ever.
 *
 * Runs BEFORE the scheduler, so: no FreeRTOS calls, no queues, no tasks. Just
 * the LCD, the font renderer, and a busy-wait that feeds the watchdog.
 */

#include "cal_dump.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "theme.h"

#ifndef EMULATOR_BUILD
#include "at32f403a_407.h"
#endif

/* ── colours: fixed, not themed. A diagnostic should look the same for
 * everyone reporting results, so screenshots are comparable. ── */
#define BG        0x0000  /* black  */
#define FG        0xFFFF  /* white  */
#define DIM       0x8410  /* grey   */
#define GOOD      0x07E0  /* green  */
#define WARN      0xFD20  /* orange */
#define HOT       0xF800  /* red    */

#define LINE_H    13
#define PAGE_MS   5000u

/* ────────────────────────────────────────────────────────────────────
 * Pre-scheduler delay. The IWDG may already be running (it cannot be
 * stopped once started and survives a system reset), so a long wait must
 * feed it or the device resets mid-report.
 * ──────────────────────────────────────────────────────────────────── */
static void diag_delay_ms(uint32_t ms)
{
    while (ms--) {
#ifndef EMULATOR_BUILD
        wdt_counter_reload();
        /* ~240 MHz, ~4 cycles per iteration of this empty volatile loop. */
        for (volatile uint32_t i = 0; i < 60000u; i++) { }
#else
        (void)ms;
        break;
#endif
    }
}

/* ── CRC32 (IEEE 802.3, reflected) — computed bitwise to avoid a 1 KB table
 * in a diagnostic image. 4 KB at 240 MHz is imperceptible. ── */
static uint32_t crc32_of(const volatile uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
    return ~crc;
}

typedef struct {
    uint32_t non_ff;        /* bytes that are not 0xFF (erased state)      */
    uint32_t non_00;        /* bytes that are not 0x00                     */
    int32_t  first_non_ff;  /* offset of first non-erased byte, -1 if none */
    int32_t  last_non_ff;
    uint32_t crc;
    bool     chunk_used[16];/* per 256-byte chunk: contains a non-FF byte  */
} cal_stats_t;

static void cal_scan(const volatile uint8_t *p, cal_stats_t *s)
{
    s->non_ff = 0;
    s->non_00 = 0;
    s->first_non_ff = -1;
    s->last_non_ff = -1;
    for (int i = 0; i < 16; i++) s->chunk_used[i] = false;

    for (uint32_t i = 0; i < CAL_DUMP_LEN; i++) {
        uint8_t v = p[i];
        if (v != 0xFFu) {
            s->non_ff++;
            if (s->first_non_ff < 0) s->first_non_ff = (int32_t)i;
            s->last_non_ff = (int32_t)i;
            s->chunk_used[i >> 8] = true;
        }
        if (v != 0x00u) s->non_00++;
    }
    s->crc = crc32_of(p, CAL_DUMP_LEN);
}

/* ────────────────────────────────────────────────────────────────────
 * Page 0 — the verdict. This is the page a user photographs and posts.
 * ──────────────────────────────────────────────────────────────────── */
static void draw_summary(const cal_stats_t *s)
{
    char buf[64];
    uint16_t y = 4;

    lcd_clear(BG);
    font_draw_string(4, y, "MCU saved-config sector", FG, BG, &font_medium);
    y += 18;
    snprintf(buf, sizeof(buf), "0x%08lX  +%lu bytes",
             (unsigned long)CAL_DUMP_BASE, (unsigned long)CAL_DUMP_LEN);
    font_draw_string(4, y, buf, DIM, BG, &font_small);
    y += LINE_H + 4;

    /* Verdict. "blank" is a real result, not a failure — it says either this
     * platform has no per-device cal, or ours was erased before anyone looked. */
    const char *verdict;
    uint16_t vc;
    if (s->non_ff == 0) {
        verdict = "BLANK (all 0xFF, erased)";
        vc = WARN;
    } else if (s->non_00 == 0) {
        verdict = "ALL ZERO (written, not erased)";
        vc = WARN;
    } else {
        verdict = "POPULATED - data present";
        vc = GOOD;
    }
    font_draw_string(4, y, verdict, vc, BG, &font_medium);
    y += 20;

    snprintf(buf, sizeof(buf), "non-FF: %lu / %lu bytes",
             (unsigned long)s->non_ff, (unsigned long)CAL_DUMP_LEN);
    font_draw_string(4, y, buf, FG, BG, &font_small); y += LINE_H;

    if (s->first_non_ff >= 0) {
        snprintf(buf, sizeof(buf), "range:  0x%03lX .. 0x%03lX",
                 (unsigned long)s->first_non_ff, (unsigned long)s->last_non_ff);
        font_draw_string(4, y, buf, FG, BG, &font_small); y += LINE_H;
    }

    /* The number to report. One value identifies the sector's content, so
     * "do different units differ?" is answerable without shipping dumps. */
    snprintf(buf, sizeof(buf), "CRC32:  %08lX", (unsigned long)s->crc);
    font_draw_string(4, y, buf, GOOD, BG, &font_small);
    y += LINE_H + 6;

    font_draw_string(4, y, "256B chunks (# used, . erased):", DIM, BG, &font_small);
    y += LINE_H;
    char map[17];
    for (int i = 0; i < 16; i++) map[i] = s->chunk_used[i] ? '#' : '.';
    map[16] = '\0';
    font_draw_string(4, y, map, FG, BG, &font_medium);
    y += 20;

    font_draw_string(4, y, "READ-ONLY. Nothing was written.", DIM, BG, &font_small);
    y += LINE_H;
    font_draw_string(4, y, "Report the CRC32 on issue #12.", DIM, BG, &font_small);
}

/* ────────────────────────────────────────────────────────────────────
 * Hex pages — only for chunks that contain data. Paging through 15 screens
 * of 0xFF helps nobody.
 * ──────────────────────────────────────────────────────────────────── */
static void draw_hex_chunk(const volatile uint8_t *p, int chunk)
{
    char buf[80];
    uint16_t y = 4;

    lcd_clear(BG);
    snprintf(buf, sizeof(buf), "0x%08lX  chunk %d/16",
             (unsigned long)(CAL_DUMP_BASE + (uint32_t)chunk * 256u), chunk);
    font_draw_string(4, y, buf, WARN, BG, &font_small);
    y += LINE_H + 2;

    /* 256 bytes as 16 rows of 16. */
    for (int row = 0; row < 16; row++) {
        uint32_t off = (uint32_t)chunk * 256u + (uint32_t)row * 16u;
        int n = snprintf(buf, sizeof(buf), "%03lX ", (unsigned long)off);
        for (int col = 0; col < 16; col++)
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%02X", p[off + (uint32_t)col]);
        font_draw_string(2, y, buf, FG, BG, &font_small);
        y += LINE_H;
    }
}

void cal_dump_run(void)
{
    /* const volatile: the compiler may not cache it, and cannot be talked into
     * writing through it. */
    const volatile uint8_t *p = (const volatile uint8_t *)CAL_DUMP_BASE;

    cal_stats_t s;
    cal_scan(p, &s);

    for (;;) {
        draw_summary(&s);
        diag_delay_ms(PAGE_MS);

        for (int c = 0; c < 16; c++) {
            if (!s.chunk_used[c]) continue;   /* skip erased chunks */
            draw_hex_chunk(p, c);
            diag_delay_ms(PAGE_MS);
        }
    }
}
