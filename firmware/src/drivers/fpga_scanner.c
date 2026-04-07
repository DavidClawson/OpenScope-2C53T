/*
 * OpenScope 2C53T - FPGA SPI Activation Scanner
 *
 * Systematically sweeps SPI parameters and USART commands to find
 * what activates the FPGA's SPI slave data output.
 *
 * Tier 1 (~5 sec):   SPI mode × first MOSI byte (4 × 256 = 1024 combos)
 * Tier 2 (~30 sec):  USART cmd_lo sweep with cmd_hi=0x00 (256 combos)
 * Tier 3 (~8 min):   USART cmd_lo sweep with cmd_hi=0x00-0x0F (4096 combos)
 * Tier 4 (~2 hours):  Full USART cmd_hi × cmd_lo (256 × 256 = 65536 combos)
 *
 * Detection: for each probe, send 4 bytes over SPI3, check if any
 * response byte is not 0xFF. Also samples all GPIO ports for any
 * activity during the transfer.
 *
 * Display shows real-time progress, elapsed time, and any hits.
 * POWER button aborts at any time.
 */

#include "fpga_scanner.h"
#include "fpga.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "button_scan.h"
#include "watchdog.h"
#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 direct register access (same as fpga.c)
 * ═══════════════════════════════════════════════════════════════════ */

#define SCAN_SPI       ((spi_type *)SPI3_BASE)

/* CS control */
#define SCAN_CS_LO()   (GPIOB->clr = (1 << 6))
#define SCAN_CS_HI()   (GPIOB->scr = (1 << 6))

/* ═══════════════════════════════════════════════════════════════════
 * Scanner state
 * ═══════════════════════════════════════════════════════════════════ */

static scanner_hit_t hits[SCANNER_MAX_HITS];
static uint16_t hit_count;
static uint32_t total_probes;
static uint32_t start_tick;

/* ═══════════════════════════════════════════════════════════════════
 * Low-level SPI3 transfer (bypasses fpga.c's spi3_xfer)
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t scan_spi_xfer(uint8_t tx)
{
    volatile uint32_t timeout = 50000;
    while (!(SCAN_SPI->sts & (1 << 1))) {  /* TDBE */
        if (--timeout == 0) return 0xFF;
    }
    SCAN_SPI->dt = tx;

    timeout = 50000;
    while (!(SCAN_SPI->sts & (1 << 0))) {  /* RDBF */
        if (--timeout == 0) return 0xFF;
    }
    return (uint8_t)SCAN_SPI->dt;
}

/* Reconfigure SPI3 mode (CPOL/CPHA) without changing other settings */
static void scan_set_spi_mode(uint8_t mode)
{
    /* Disable SPI */
    SCAN_SPI->ctrl1 &= ~(1 << 6);  /* SPE off */

    uint32_t ctrl1 = SCAN_SPI->ctrl1;
    ctrl1 &= ~0x03;          /* Clear CPHA and CPOL */
    ctrl1 |= (mode & 0x03);  /* Set new mode */
    SCAN_SPI->ctrl1 = ctrl1;

    /* Re-enable SPI */
    SCAN_SPI->ctrl1 |= (1 << 6);  /* SPE on */

    /* Flush any stale data */
    (void)SCAN_SPI->dt;
}

/* Restore SPI3 to Mode 3 (normal operating mode) */
static void scan_restore_spi(void)
{
    scan_set_spi_mode(3);  /* CPOL=1, CPHA=1 */
}

/* ═══════════════════════════════════════════════════════════════════
 * USART2 command send (standalone, no RTOS queue)
 * ═══════════════════════════════════════════════════════════════════ */

static void scan_usart_cmd(uint8_t cmd_hi, uint8_t cmd_lo)
{
    uint8_t frame[10] = {0};
    frame[2] = cmd_hi;
    frame[3] = cmd_lo;
    frame[8] = 0xAA;
    frame[9] = (cmd_hi + cmd_lo) & 0xFF;

    for (int i = 0; i < 10; i++) {
        volatile uint32_t timeout = 100000;
        while (!(USART2->sts & (1 << 7))) {  /* TDBE */
            if (--timeout == 0) return;
        }
        USART2->dt = frame[i];
    }

    /* Wait for transmit complete */
    volatile uint32_t timeout = 100000;
    while (!(USART2->sts & (1 << 6))) {  /* TDC */
        if (--timeout == 0) break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * GPIO snapshot for detecting any pin activity
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t a, b, c, d, e;
} gpio_snap_t;

static void gpio_snapshot(gpio_snap_t *s)
{
    s->a = GPIOA->idt;
    s->b = GPIOB->idt;
    s->c = GPIOC->idt;
    s->d = GPIOD->idt;
    s->e = GPIOE->idt;
}

/* ═══════════════════════════════════════════════════════════════════
 * Core probe function
 *
 * Sends 4 bytes over SPI3, returns true if any response != 0xFF
 * or if any GPIO pin changed during the transfer.
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_probe(uint8_t first_byte, uint8_t response[4],
                        uint32_t *gpio_delta_out)
{
    gpio_snap_t pre, post;

    /* Snapshot GPIO state before transfer */
    gpio_snapshot(&pre);

    /* SPI3 transfer: 1 command byte + 3 data bytes */
    SCAN_CS_LO();

    /* Small delay for CS setup time */
    for (volatile int d = 0; d < 20; d++) {}

    response[0] = scan_spi_xfer(first_byte);
    response[1] = scan_spi_xfer(0xFF);
    response[2] = scan_spi_xfer(0xFF);
    response[3] = scan_spi_xfer(0xFF);

    SCAN_CS_HI();

    /* Snapshot GPIO state after transfer */
    gpio_snapshot(&post);

    /* Compute delta: any pin that changed */
    uint32_t da = pre.a ^ post.a;
    uint32_t db = pre.b ^ post.b;
    uint32_t dc = pre.c ^ post.c;
    uint32_t dd = pre.d ^ post.d;

    /* Pack into 32 bits: A[7:0] | B[15:8] | C[23:16] | D[31:24]
     * (just low 8 bits of each port for compact display) */
    *gpio_delta_out = (da & 0xFF) | ((db & 0xFF) << 8)
                    | ((dc & 0xFF) << 16) | ((dd & 0xFF) << 24);

    /* Mask out known-toggling pins to avoid false positives:
     * - PB3 (SCK) = bit 3 of port B → byte 1 bit 3
     * - PB5 (MOSI) = bit 5 of port B → byte 1 bit 5
     * - PB6 (CS) = bit 6 of port B → byte 1 bit 6
     * - PA1 (~0.6Hz from FPGA) = bit 1 of port A → byte 0 bit 1
     * - PA3 (USART2 RX) = bit 3 of port A → byte 0 bit 3
     */
    uint32_t mask = 0x0A          /* PA1, PA3 */
                  | (0x68 << 8);  /* PB3, PB5, PB6 */
    uint32_t meaningful_delta = *gpio_delta_out & ~mask;

    /* Hit if: any response byte is not 0xFF, OR meaningful GPIO change */
    bool hit = (response[0] != 0xFF || response[1] != 0xFF
             || response[2] != 0xFF || response[3] != 0xFF);

    /* Also check PB4 specifically (expected MISO pin) */
    if (db & (1 << 4)) hit = true;

    /* Any other unexpected GPIO activity */
    if (meaningful_delta) hit = true;

    total_probes++;
    return hit;
}

/* ═══════════════════════════════════════════════════════════════════
 * Record a hit
 * ═══════════════════════════════════════════════════════════════════ */

static void record_hit(uint8_t tier, uint8_t spi_mode, uint8_t first_byte,
                        uint8_t cmd_hi, uint8_t cmd_lo,
                        const uint8_t response[4], uint32_t gpio_delta)
{
    if (hit_count >= SCANNER_MAX_HITS) return;

    scanner_hit_t *h = &hits[hit_count++];
    h->tier = tier;
    h->spi_mode = spi_mode;
    h->first_byte = first_byte;
    h->cmd_hi = cmd_hi;
    h->cmd_lo = cmd_lo;
    memcpy(h->response, response, 4);
    h->gpio_delta = gpio_delta;
}

/* ═══════════════════════════════════════════════════════════════════
 * Check if user pressed POWER to abort
 * ═══════════════════════════════════════════════════════════════════ */

static bool user_abort(void)
{
    /* PC8 = POWER button, active LOW */
    return !(GPIOC->idt & (1 << 8));
}

/* Feed watchdog every call — prevents health monitor timeout.
 * The scanner blocks the input task for minutes, so the health
 * monitor would flag it as stalled and starve the WDT. */
static void scan_keepalive(void)
{
    wdt_counter_reload();
}

/* ═══════════════════════════════════════════════════════════════════
 * Display helpers
 * ═══════════════════════════════════════════════════════════════════ */

static void scan_draw_header(const char *tier_name)
{
    const theme_t *th = theme_get();
    lcd_fill_rect(0, 0, 320, 240, th->background);

    font_draw_string(4, 2, "FPGA SPI SCANNER", th->highlight, th->background,
                     &font_medium);
    font_draw_string(4, 20, tier_name, th->text_primary, th->background,
                     &font_small);
}

static void scan_draw_progress(uint32_t current, uint32_t total,
                                uint8_t spi_mode, uint8_t first_byte,
                                uint8_t cmd_hi, uint8_t cmd_lo)
{
    const theme_t *th = theme_get();
    char buf[48];
    uint16_t bg = th->background;

    /* Progress bar */
    uint16_t bar_x = 4, bar_y = 38, bar_w = 312, bar_h = 14;
    lcd_fill_rect(bar_x, bar_y, bar_w, bar_h, th->grid);
    uint32_t fill_w = (total > 0) ? (uint32_t)bar_w * current / total : 0;
    if (fill_w > bar_w) fill_w = bar_w;
    if (fill_w > 0)
        lcd_fill_rect(bar_x, bar_y, (uint16_t)fill_w, bar_h, th->ch1);

    /* Percentage */
    uint32_t pct = (total > 0) ? (current * 100) / total : 0;
    snprintf(buf, sizeof(buf), "%lu/%lu (%lu%%)",
             (unsigned long)current, (unsigned long)total,
             (unsigned long)pct);
    lcd_fill_rect(4, 56, 312, 14, bg);
    font_draw_string(4, 56, buf, th->text_secondary, bg, &font_small);

    /* Current test parameters */
    snprintf(buf, sizeof(buf), "M%d MOSI:0x%02X CMD:%02X/%02X",
             spi_mode, first_byte, cmd_hi, cmd_lo);
    lcd_fill_rect(4, 72, 312, 14, bg);
    font_draw_string(4, 72, buf, th->text_secondary, bg, &font_small);

    /* Hit count */
    snprintf(buf, sizeof(buf), "Hits: %u", hit_count);
    lcd_fill_rect(4, 88, 160, 14, bg);
    font_draw_string(4, 88, buf,
                     hit_count > 0 ? th->ch1 : th->text_secondary,
                     bg, &font_small);

    /* Elapsed time */
    uint32_t elapsed_ms = (xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS;
    uint32_t secs = elapsed_ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    snprintf(buf, sizeof(buf), "%luh %02lum %02lus",
             (unsigned long)hours, (unsigned long)(mins % 60),
             (unsigned long)(secs % 60));
    lcd_fill_rect(164, 88, 152, 14, bg);
    font_draw_string(164, 88, buf, th->text_secondary, bg, &font_small);
}

static void scan_draw_hit(uint16_t idx)
{
    if (idx >= hit_count) return;
    const theme_t *th = theme_get();
    const scanner_hit_t *h = &hits[idx];
    char buf[48];
    uint16_t bg = th->background;

    /* Show latest hit at fixed position */
    uint16_t y = 106;

    snprintf(buf, sizeof(buf), "HIT #%u: T%d M%d B:0x%02X C:%02X/%02X",
             idx + 1, h->tier, h->spi_mode, h->first_byte,
             h->cmd_hi, h->cmd_lo);
    lcd_fill_rect(4, y, 312, 14, bg);
    font_draw_string(4, y, buf, th->highlight, bg, &font_small);

    snprintf(buf, sizeof(buf), "  R: %02X %02X %02X %02X  G:%08lX",
             h->response[0], h->response[1], h->response[2], h->response[3],
             (unsigned long)h->gpio_delta);
    lcd_fill_rect(4, y + 14, 312, 14, bg);
    font_draw_string(4, y + 14, buf, th->ch1, bg, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 0: Deep Probe — investigate Mode 0 + 0x13 discovery
 *
 * First run found PB4 (MISO) toggled in SPI Mode 0 with MOSI=0x13.
 * This tier does an intensive investigation:
 *   - Mode 0 with bytes 0x10-0x1F (neighborhood of 0x13)
 *   - Extended reads (32 bytes instead of 4)
 *   - Two-phase: send command byte, then read separately
 *   - Slow SPI (/256) with GPIO sampling during transfer
 *   - All 4 modes with 0x12, 0x13, 0x15 (the handshake bytes)
 * ═══════════════════════════════════════════════════════════════════ */

/* WDT-safe delay: feeds watchdog during long waits */
static void scan_delay_ms(uint32_t ms)
{
    uint32_t chunks = ms / 100;
    for (uint32_t i = 0; i < chunks; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        scan_keepalive();
    }
    uint32_t remainder = ms % 100;
    if (remainder > 0)
        vTaskDelay(pdMS_TO_TICKS(remainder));
    scan_keepalive();
}

/* Static buffers to avoid stack overflow in the input task */
static uint8_t ext_resp[64];

static bool scan_tier0(void)
{
    const theme_t *th = theme_get();
    char buf[52];
    uint16_t bg = th->background;

    /* ─── Phase A: Mode 0, full command sweep 0x00-0xFF, 64-byte reads ─── */
    scan_draw_header("Tier 0A: Mode 0 cmd sweep (256x64B)");
    font_draw_string(4, 38, "Cmd  Bytes 0-3    Bytes 4-7    PB4 Data?",
                     th->text_secondary, bg, &font_small);

    scan_set_spi_mode(0);

    uint8_t display_row = 0;
    for (uint16_t fb = 0; fb < 256; fb++) {
        if (user_abort()) { scan_restore_spi(); return true; }
        scan_keepalive();

        gpio_snap_t pre, post;
        gpio_snapshot(&pre);

        SCAN_CS_LO();
        for (volatile int d = 0; d < 20; d++) {}
        ext_resp[0] = scan_spi_xfer((uint8_t)fb);
        for (int i = 1; i < 64; i++)
            ext_resp[i] = scan_spi_xfer(0xFF);
        SCAN_CS_HI();

        gpio_snapshot(&post);
        uint32_t db = pre.b ^ post.b;

        /* Check for any non-FF byte */
        bool has_data = false;
        int first_data_idx = -1;
        for (int i = 0; i < 64; i++) {
            if (ext_resp[i] != 0xFF) {
                has_data = true;
                if (first_data_idx < 0) first_data_idx = i;
            }
        }

        bool pb4_hit = (db & (1 << 4)) != 0;

        if (has_data || pb4_hit) {
            record_hit(0, 0, (uint8_t)fb, 0, 0, ext_resp, db);

            /* Show on display (up to 14 rows fit) */
            if (display_row < 14) {
                snprintf(buf, sizeof(buf),
                         "%02X: %02X%02X%02X%02X %02X%02X%02X%02X %c @%d",
                         (uint8_t)fb,
                         ext_resp[0], ext_resp[1], ext_resp[2], ext_resp[3],
                         ext_resp[4], ext_resp[5], ext_resp[6], ext_resp[7],
                         pb4_hit ? 'Y' : 'n',
                         first_data_idx);
                uint16_t y = 52 + display_row * 12;
                lcd_fill_rect(4, y, 312, 12, bg);
                font_draw_string(4, y, buf, th->ch1, bg, &font_small);
                display_row++;
            }
        }

        /* Update progress every 16 commands */
        if ((fb & 0x0F) == 0) {
            snprintf(buf, sizeof(buf), "Scanning: 0x%02X/FF  Hits:%u",
                     (uint8_t)fb, hit_count);
            lcd_fill_rect(200, 20, 116, 14, bg);
            font_draw_string(200, 20, buf, th->text_secondary, bg, &font_small);
        }
    }

    scan_delay_ms(3000);

    /* ─── Phase B: Deep read for hits — read 64 bytes, show hex dump ─── */
    /* Focus on 0x11 which returned 0120681B */
    scan_draw_header("Tier 0B: 0x11 hex dump (64 bytes M0)");

    scan_set_spi_mode(0);

    SCAN_CS_LO();
    for (volatile int d = 0; d < 20; d++) {}
    ext_resp[0] = scan_spi_xfer(0x11);
    for (int i = 1; i < 64; i++)
        ext_resp[i] = scan_spi_xfer(0xFF);
    SCAN_CS_HI();
    scan_keepalive();

    /* Display as hex dump: 8 rows of 8 bytes each */
    for (int row = 0; row < 8; row++) {
        int off = row * 8;
        snprintf(buf, sizeof(buf), "%02d: %02X %02X %02X %02X %02X %02X %02X %02X",
                 off,
                 ext_resp[off+0], ext_resp[off+1], ext_resp[off+2], ext_resp[off+3],
                 ext_resp[off+4], ext_resp[off+5], ext_resp[off+6], ext_resp[off+7]);
        font_draw_string(4, 40 + row * 14, buf, th->text_primary, bg, &font_small);
    }

    /* Second read of 0x11 to check if data is consistent */
    SCAN_CS_LO();
    for (volatile int d = 0; d < 20; d++) {}
    ext_resp[0] = scan_spi_xfer(0x11);
    for (int i = 1; i < 64; i++)
        ext_resp[i] = scan_spi_xfer(0xFF);
    SCAN_CS_HI();
    scan_keepalive();

    font_draw_string(4, 156, "2nd read:", th->highlight, bg, &font_small);
    for (int row = 0; row < 4; row++) {
        int off = row * 8;
        snprintf(buf, sizeof(buf), "%02d: %02X %02X %02X %02X %02X %02X %02X %02X",
                 off,
                 ext_resp[off+0], ext_resp[off+1], ext_resp[off+2], ext_resp[off+3],
                 ext_resp[off+4], ext_resp[off+5], ext_resp[off+6], ext_resp[off+7]);
        font_draw_string(4, 170 + row * 14, buf, th->ch2, bg, &font_small);
    }

    scan_delay_ms(5000);

    /* ─── Phase C: Try all modes with 0x11, compare data ─── */
    scan_draw_header("Tier 0C: 0x11 in all modes (M0-M3)");

    for (uint8_t mode = 0; mode < 4; mode++) {
        scan_set_spi_mode(mode);
        scan_keepalive();

        SCAN_CS_LO();
        for (volatile int d = 0; d < 20; d++) {}
        ext_resp[0] = scan_spi_xfer(0x11);
        for (int i = 1; i < 32; i++)
            ext_resp[i] = scan_spi_xfer(0xFF);
        SCAN_CS_HI();

        snprintf(buf, sizeof(buf), "M%d: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                 mode,
                 ext_resp[0], ext_resp[1], ext_resp[2], ext_resp[3],
                 ext_resp[4], ext_resp[5], ext_resp[6], ext_resp[7],
                 ext_resp[8], ext_resp[9], ext_resp[10], ext_resp[11]);
        font_draw_string(4, 40 + mode * 28, buf, th->text_primary, bg, &font_small);

        snprintf(buf, sizeof(buf), "    %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                 ext_resp[12], ext_resp[13], ext_resp[14], ext_resp[15],
                 ext_resp[16], ext_resp[17], ext_resp[18], ext_resp[19],
                 ext_resp[20], ext_resp[21], ext_resp[22], ext_resp[23]);
        font_draw_string(4, 40 + mode * 28 + 14, buf, th->text_secondary, bg, &font_small);
    }

    scan_delay_ms(5000);

    /* ─── Phase D: Variable lead-in bytes before reading ─── */
    scan_draw_header("Tier 0D: Lead-in bytes before data");
    font_draw_string(4, 38, "Skip N dummy bytes, then read 8",
                     th->text_secondary, bg, &font_small);

    scan_set_spi_mode(0);

    /* Try different numbers of lead-in 0xFF before 0x11 command */
    for (int lead = 0; lead < 12; lead++) {
        scan_keepalive();

        SCAN_CS_LO();
        for (volatile int d = 0; d < 20; d++) {}

        /* Send lead-in dummy bytes */
        for (int i = 0; i < lead; i++)
            (void)scan_spi_xfer(0xFF);

        /* Send command 0x11 */
        ext_resp[0] = scan_spi_xfer(0x11);

        /* Read 8 bytes after command */
        for (int i = 1; i < 9; i++)
            ext_resp[i] = scan_spi_xfer(0xFF);

        SCAN_CS_HI();

        snprintf(buf, sizeof(buf), "L%02d: %02X %02X%02X%02X%02X %02X%02X%02X%02X",
                 lead,
                 ext_resp[0],
                 ext_resp[1], ext_resp[2], ext_resp[3], ext_resp[4],
                 ext_resp[5], ext_resp[6], ext_resp[7], ext_resp[8]);

        bool has = false;
        for (int i = 0; i < 9; i++) if (ext_resp[i] != 0xFF) has = true;

        font_draw_string(4, 52 + lead * 13, buf,
                         has ? th->ch1 : th->text_secondary, bg, &font_small);
    }

    scan_delay_ms(5000);

    scan_restore_spi();
    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 1: SPI parameter sweep (no USART commands)
 *
 * For each SPI mode (0-3), try all 256 first-byte values.
 * Total: 1024 probes, takes ~5 seconds.
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier1(void)
{
    scan_draw_header("Tier 1: SPI Mode x First Byte (1024)");

    uint32_t combo = 0;
    const uint32_t total = 4 * 256;

    for (uint8_t mode = 0; mode < 4; mode++) {
        scan_set_spi_mode(mode);

        for (uint16_t fb = 0; fb < 256; fb++) {
            if (user_abort()) return true;

            uint8_t response[4];
            uint32_t gpio_delta;

            if (scan_probe((uint8_t)fb, response, &gpio_delta)) {
                record_hit(1, mode, (uint8_t)fb, 0, 0, response, gpio_delta);
                scan_draw_hit(hit_count - 1);
            }

            combo++;
            scan_keepalive();
            if ((combo & 0x1F) == 0)
                scan_draw_progress(combo, total, mode, (uint8_t)fb, 0, 0);
        }
    }

    scan_restore_spi();
    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 2: USART cmd_lo sweep (cmd_hi = 0x00)
 *
 * For each USART command 0x00-0xFF with cmd_hi=0x00:
 *   Send command, wait 100ms, probe SPI3 with bytes 0x00-0x09
 * Total: 256 commands × 10 trigger bytes = 2560 probes, ~30 sec.
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier2(void)
{
    scan_draw_header("Tier 2: USART cmd_lo x trigger (2560)");

    uint32_t combo = 0;
    const uint32_t total = 256 * 10;

    for (uint16_t cmd_lo = 0; cmd_lo < 256; cmd_lo++) {
        if (user_abort()) return true;

        /* Send USART command */
        scan_usart_cmd(0x00, (uint8_t)cmd_lo);
        vTaskDelay(pdMS_TO_TICKS(50));

        /* Try trigger bytes 0x00-0x09 */
        for (uint8_t tb = 0; tb < 10; tb++) {
            uint8_t response[4];
            uint32_t gpio_delta;

            if (scan_probe(tb, response, &gpio_delta)) {
                record_hit(2, 3, tb, 0x00, (uint8_t)cmd_lo,
                           response, gpio_delta);
                scan_draw_hit(hit_count - 1);
            }
            combo++;
        }

        scan_keepalive();
        if ((cmd_lo & 0x07) == 0)
            scan_draw_progress(combo, total, 3, 0, 0x00, (uint8_t)cmd_lo);
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 3: USART cmd_hi sweep (0x00-0x0F × cmd_lo 0x00-0xFF)
 *
 * Focuses on cmd_hi values 0x00-0x0F (the most likely range).
 * For each: send command, wait 50ms, probe with 0xFF.
 * Total: 16 × 256 = 4096 probes, ~8 minutes.
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier3(void)
{
    scan_draw_header("Tier 3: cmd_hi(0-F) x cmd_lo (4096)");

    uint32_t combo = 0;
    const uint32_t total = 16 * 256;

    for (uint8_t cmd_hi = 0; cmd_hi < 16; cmd_hi++) {
        for (uint16_t cmd_lo = 0; cmd_lo < 256; cmd_lo++) {
            if (user_abort()) return true;

            /* Send USART command */
            scan_usart_cmd(cmd_hi, (uint8_t)cmd_lo);
            vTaskDelay(pdMS_TO_TICKS(50));

            /* Probe SPI3 */
            uint8_t response[4];
            uint32_t gpio_delta;

            if (scan_probe(0xFF, response, &gpio_delta)) {
                record_hit(3, 3, 0xFF, cmd_hi, (uint8_t)cmd_lo,
                           response, gpio_delta);
                scan_draw_hit(hit_count - 1);
            }

            combo++;
            scan_keepalive();
            if ((combo & 0x1F) == 0)
                scan_draw_progress(combo, total, 3, 0xFF, cmd_hi,
                                   (uint8_t)cmd_lo);
        }
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 4: Full USART sweep (cmd_hi 0x00-0xFF × cmd_lo 0x00-0xFF)
 *
 * The big one. 65536 probes at ~50ms each = ~55 minutes.
 * Skips cmd_hi 0x00-0x0F (already covered by Tier 3).
 * Total: 240 × 256 = 61440 probes, ~51 minutes.
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier4(void)
{
    scan_draw_header("Tier 4: Full USART sweep (61440)");

    uint32_t combo = 0;
    const uint32_t total = 240 * 256;  /* cmd_hi 0x10-0xFF */

    for (uint16_t cmd_hi = 0x10; cmd_hi < 256; cmd_hi++) {
        for (uint16_t cmd_lo = 0; cmd_lo < 256; cmd_lo++) {
            if (user_abort()) return true;

            scan_usart_cmd((uint8_t)cmd_hi, (uint8_t)cmd_lo);
            vTaskDelay(pdMS_TO_TICKS(50));

            uint8_t response[4];
            uint32_t gpio_delta;

            if (scan_probe(0xFF, response, &gpio_delta)) {
                record_hit(4, 3, 0xFF, (uint8_t)cmd_hi, (uint8_t)cmd_lo,
                           response, gpio_delta);
                scan_draw_hit(hit_count - 1);
            }

            combo++;
            scan_keepalive();
            if ((combo & 0x3F) == 0)
                scan_draw_progress(combo, total, 3, 0xFF,
                                   (uint8_t)cmd_hi, (uint8_t)cmd_lo);
        }
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 5: USART command pairs (most promising combos)
 *
 * Try sending TWO USART commands before SPI probe.
 * Focus on known-important commands as the first command,
 * then sweep cmd_lo for the second command.
 *
 * First commands: 0x00, 0x01, 0x02, 0x06, 0x07, 0x08, 0x09,
 *                 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
 * Second: full 0x00-0xFF sweep
 * Total: 14 × 256 = 3584 probes at ~100ms = ~6 minutes
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier5(void)
{
    scan_draw_header("Tier 5: USART cmd pairs (3584)");

    static const uint8_t first_cmds[] = {
        0x00, 0x01, 0x02, 0x06, 0x07, 0x08, 0x09,
        0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
    };
    const uint8_t num_first = sizeof(first_cmds) / sizeof(first_cmds[0]);

    uint32_t combo = 0;
    const uint32_t total = num_first * 256;

    for (uint8_t fi = 0; fi < num_first; fi++) {
        for (uint16_t cmd_lo = 0; cmd_lo < 256; cmd_lo++) {
            if (user_abort()) return true;

            /* Send first command (known important) */
            scan_usart_cmd(0x00, first_cmds[fi]);
            vTaskDelay(pdMS_TO_TICKS(30));

            /* Send second command */
            scan_usart_cmd(0x00, (uint8_t)cmd_lo);
            vTaskDelay(pdMS_TO_TICKS(50));

            /* Probe SPI3 */
            uint8_t response[4];
            uint32_t gpio_delta;

            if (scan_probe(0xFF, response, &gpio_delta)) {
                record_hit(5, 3, 0xFF, first_cmds[fi], (uint8_t)cmd_lo,
                           response, gpio_delta);
                scan_draw_hit(hit_count - 1);
            }

            combo++;
            scan_keepalive();
            if ((combo & 0x1F) == 0)
                scan_draw_progress(combo, total, 3, 0xFF,
                                   first_cmds[fi], (uint8_t)cmd_lo);
        }
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════
 * Tier 6: Control pin combinations
 *
 * Toggle PC6 (SPI enable) and PB11 (active mode) between probes.
 * Try all 4 combinations × trigger bytes 0-9 × SPI first bytes.
 * Total: 4 × 10 × 16 = 640 probes, ~1 minute
 * ═══════════════════════════════════════════════════════════════════ */

static bool scan_tier6(void)
{
    scan_draw_header("Tier 6: Control pins x triggers (640)");

    uint32_t combo = 0;
    const uint32_t total = 4 * 10 * 16;

    /* Key first-bytes to try: 0x00-0x09 plus some common ones */
    static const uint8_t test_bytes[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x3B, 0x3A, 0x80, 0xAA, 0x55, 0xFF
    };

    for (uint8_t pins = 0; pins < 4; pins++) {
        /* Set control pins */
        if (pins & 1) GPIOC->scr = (1 << 6);   /* PC6 HIGH */
        else          GPIOC->clr = (1 << 6);   /* PC6 LOW */

        if (pins & 2) GPIOB->scr = (1 << 11);  /* PB11 HIGH */
        else          GPIOB->clr = (1 << 11);  /* PB11 LOW */

        vTaskDelay(pdMS_TO_TICKS(100));

        for (uint8_t tb = 0; tb < 10; tb++) {
            for (uint8_t bi = 0; bi < 16; bi++) {
                if (user_abort()) goto restore_pins;

                uint8_t response[4];
                uint32_t gpio_delta;

                if (scan_probe(test_bytes[bi], response, &gpio_delta)) {
                    record_hit(6, 3, test_bytes[bi], pins, tb,
                               response, gpio_delta);
                    scan_draw_hit(hit_count - 1);
                }

                combo++;
                scan_keepalive();
                if ((combo & 0x1F) == 0)
                    scan_draw_progress(combo, total, 3, test_bytes[bi],
                                       pins, tb);
            }
        }
    }

restore_pins:
    /* Restore normal pin state */
    GPIOC->scr = (1 << 6);   /* PC6 HIGH */
    GPIOB->scr = (1 << 11);  /* PB11 HIGH */

    return user_abort();
}

/* ═══════════════════════════════════════════════════════════════════
 * Final results screen
 * ═══════════════════════════════════════════════════════════════════ */

static void scan_draw_results(bool aborted)
{
    const theme_t *th = theme_get();
    char buf[48];
    uint16_t bg = th->background;

    lcd_fill_rect(0, 0, 320, 240, bg);

    font_draw_string(4, 2, aborted ? "SCAN ABORTED" : "SCAN COMPLETE",
                     th->highlight, bg, &font_medium);

    /* Elapsed time */
    uint32_t elapsed_ms = (xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS;
    uint32_t secs = elapsed_ms / 1000;
    snprintf(buf, sizeof(buf), "Time: %luh %02lum %02lus   Probes: %lu",
             (unsigned long)(secs / 3600),
             (unsigned long)((secs / 60) % 60),
             (unsigned long)(secs % 60),
             (unsigned long)total_probes);
    font_draw_string(4, 22, buf, th->text_primary, bg, &font_small);

    snprintf(buf, sizeof(buf), "Total hits: %u", hit_count);
    font_draw_string(4, 38, buf,
                     hit_count > 0 ? th->ch1 : th->text_secondary,
                     bg, &font_medium);

    if (hit_count == 0) {
        font_draw_string(4, 60, "No FPGA SPI response detected.",
                         th->text_secondary, bg, &font_small);
        font_draw_string(4, 76, "FPGA SPI slave may need hardware",
                         th->text_secondary, bg, &font_small);
        font_draw_string(4, 92, "probing or bitstream analysis.",
                         th->text_secondary, bg, &font_small);
    } else {
        /* Show all hits (up to 8) */
        uint16_t show = (hit_count < 8) ? hit_count : 8;
        for (uint16_t i = 0; i < show; i++) {
            const scanner_hit_t *h = &hits[i];
            uint16_t y = 60 + i * 22;

            snprintf(buf, sizeof(buf),
                     "#%u T%d M%d 0x%02X CMD:%02X/%02X",
                     i + 1, h->tier, h->spi_mode, h->first_byte,
                     h->cmd_hi, h->cmd_lo);
            font_draw_string(4, y, buf, th->text_primary, bg, &font_small);

            snprintf(buf, sizeof(buf),
                     "   -> %02X %02X %02X %02X  GPIO:%08lX",
                     h->response[0], h->response[1],
                     h->response[2], h->response[3],
                     (unsigned long)h->gpio_delta);
            font_draw_string(4, y + 11, buf, th->ch1, bg, &font_small);
        }
    }

    font_draw_string(4, 224, "Press any button to exit",
                     th->text_secondary, bg, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main scanner entry point
 * ═══════════════════════════════════════════════════════════════════ */

void fpga_scanner_run(void)
{
    /* Initialize state */
    memset(hits, 0, sizeof(hits));
    hit_count = 0;
    total_probes = 0;
    start_tick = xTaskGetTickCount();

    /* Pause the acquisition task so we have exclusive SPI3 access.
     * We use the fpga.spi3_active flag — the acq task checks this
     * and skips transfers when false. We also set acq_mode to 0
     * which the trigger code won't fire on. */
    fpga.spi3_active = false;
    /* Give acquisition task time to finish any in-progress transfer */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Run tiers sequentially. Each returns true if user aborted. */
    bool aborted = false;

    if (!aborted) aborted = scan_tier0();   /* Deep probe: Mode 0 + 0x13 */
    if (!aborted) aborted = scan_tier1();   /* ~5 sec: SPI mode × byte */
    if (!aborted) aborted = scan_tier2();   /* ~30 sec: USART cmd_lo */
    if (!aborted) aborted = scan_tier5();   /* ~6 min: cmd pairs */
    if (!aborted) aborted = scan_tier6();   /* ~1 min: control pins */
    if (!aborted) aborted = scan_tier3();   /* ~8 min: cmd_hi sweep */
    if (!aborted) aborted = scan_tier4();   /* ~51 min: full USART */

    /* Show final results */
    scan_draw_results(aborted);

    /* Wait for any button press to exit */
    for (;;) {
        wdt_counter_reload();
        vTaskDelay(pdMS_TO_TICKS(100));

        /* Check all buttons — any press exits */
        if (user_abort()) break;  /* POWER */

        /* Also check matrix buttons via a simple GPIO read.
         * Drive one column low and check if any row goes low. */
        /* Simplified: just wait for POWER button */
        static uint32_t wait_count = 0;
        wait_count++;
        /* After 30 seconds of waiting, auto-exit */
        if (wait_count > 300) break;
    }

    /* Resume acquisition */
    fpga.spi3_active = true;

    /* Restore SPI3 to normal mode */
    scan_restore_spi();
}
