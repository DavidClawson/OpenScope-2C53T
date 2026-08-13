/*
 * Waterfall blit equivalence + draw-call census (host test)
 *
 * Build and run:
 *   gcc -O2 -Wall -Wextra -o test_waterfall_blit test_waterfall_blit.c
 *   ./test_waterfall_blit
 *
 * Old vs new waterfall blit, on a stub LCD.
 *
 * Proves two things without hardware:
 *   1. the new row-blit writes the SAME colour to the SAME pixel as the old
 *      per-column fill_rect loop, over the whole plot rectangle, and leaves
 *      no pixel unwritten;
 *   2. how many LCD driver calls each one costs.
 *
 * The stub reproduces lcd.c semantics exactly: lcd_set_window() latches a
 * rect and resets a cursor; lcd_write_data()/lcd_write_pixels() advance it
 * left-to-right, top-to-bottom with wrap; lcd_fill_rect() clips to the panel
 * then does its own set_window + w*h data writes.
 *
 * Both paths use the SAME intensity->colour function, so this isolates the
 * B1 (render performance) claim from the B3 (colour scale) change.
 *
 * Caveat, stated because this project has been bitten by instruments that
 * could not see what they claimed to measure: both loops are TRANSCRIBED
 * from scope_ui.c rather than compiled from it — draw_waterfall_screen()
 * pulls in the FFT, the shared pool, the fonts and the AT32 headers. This
 * validates the ALGORITHM and the call counts, not the shipped translation
 * unit. If the blit in scope_ui.c is edited, blit_new() below must be
 * updated to match or this test proves nothing about it.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define LCD_WIDTH  320
#define LCD_HEIGHT 240
#define COLOR_BLACK 0x0000
#define RGB565(r, g, b) ((uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | ((b) >> 3)))

#define UNWRITTEN 0xDEADu

static uint16_t fb[LCD_HEIGHT][LCD_WIDTH];
static uint16_t win_x, win_y, win_w, win_h;
static uint32_t win_cursor;

static struct {
    uint32_t set_window;
    uint32_t fill_rect;
    uint32_t write_pixels;
    uint32_t pixel_writes;
} ctr;

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ctr.set_window++;
    win_x = x; win_y = y; win_w = w; win_h = h; win_cursor = 0;
}

static void lcd_write_data(uint16_t color)
{
    ctr.pixel_writes++;
    if (win_w == 0 || win_h == 0) return;
    uint32_t total = (uint32_t)win_w * win_h;
    if (win_cursor >= total) { fprintf(stderr, "window overrun\n"); exit(2); }
    uint16_t px = (uint16_t)(win_x + win_cursor % win_w);
    uint16_t py = (uint16_t)(win_y + win_cursor / win_w);
    if (px < LCD_WIDTH && py < LCD_HEIGHT) fb[py][px] = color;
    win_cursor++;
}

static void lcd_write_pixels(const uint16_t *pixels, uint32_t count)
{
    ctr.write_pixels++;
    while (count--) lcd_write_data(*pixels++);
}

static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ctr.fill_rect++;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    lcd_set_window(x, y, w, h);
    uint32_t total = (uint32_t)w * h;
    for (uint32_t i = 0; i < total; i++) lcd_write_data(color);
}

/* ── geometry, matching scope_ui.c ───────────────────────────────── */
static int SCOPE_TOP = 18;
static int SCOPE_BOT = LCD_HEIGHT - 16;
#define SCOPE_H (SCOPE_BOT - SCOPE_TOP)

static int WATERFALL_ROWS = 64;
#define WATERFALL_COLS 320

static uint8_t wbuf[256][WATERFALL_COLS];

/* one shared palette so this test isolates geometry, not colour */
static uint16_t intensity_to_color(uint8_t intensity)
{
    uint16_t level = (uint16_t)(255u - intensity);
    uint8_t  t = (uint8_t)((level & 63u) * 4u);
    if (level < 64)  return RGB565(0, 0, t);
    if (level < 128) return RGB565(0, t, 255);
    if (level < 192) return RGB565(t, 255, (uint8_t)(255 - t));
    return RGB565(255, (uint8_t)(255 - t), 0);
}

/* ── OLD: verbatim from scope_ui.c before the change ─────────────── */
static void blit_old(uint8_t newest_row)
{
    int x;
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);

    int row_height = SCOPE_H / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    int r;
    for (r = 0; r < WATERFALL_ROWS; r++) {
        int buf_row = (newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS;
        int y = SCOPE_TOP + r * row_height;
        if (y + row_height > SCOPE_BOT) break;
        for (x = 0; x < WATERFALL_COLS; x++)
            lcd_fill_rect((uint16_t)x, (uint16_t)y, 1, (uint16_t)row_height,
                          intensity_to_color(wbuf[buf_row][x]));
    }
}

/* ── NEW: verbatim from scope_ui.c after the change ──────────────── */
static uint16_t waterfall_line[WATERFALL_COLS];

static void blit_new(uint8_t newest_row)
{
    int x;
    int row_height = SCOPE_H / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    int rows_drawn = WATERFALL_ROWS;
    if (rows_drawn * row_height > SCOPE_H)
        rows_drawn = SCOPE_H / row_height;
    int block_h = rows_drawn * row_height;

    if (block_h < SCOPE_H)
        lcd_fill_rect(0, (uint16_t)(SCOPE_TOP + block_h), LCD_WIDTH,
                      (uint16_t)(SCOPE_H - block_h), COLOR_BLACK);

    (void)block_h;
    int r;
    for (r = 0; r < rows_drawn; r++) {
        const uint8_t *src = wbuf[(newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS];
        for (x = 0; x < WATERFALL_COLS; x++)
            waterfall_line[x] = intensity_to_color(src[x]);
        lcd_set_window(0, (uint16_t)(SCOPE_TOP + r * row_height),
                       WATERFALL_COLS, (uint16_t)row_height);
        int rep;
        for (rep = 0; rep < row_height; rep++)
            lcd_write_pixels(waterfall_line, WATERFALL_COLS);
    }
}

static void fb_reset(void)
{
    for (int y = 0; y < LCD_HEIGHT; y++)
        for (int x = 0; x < LCD_WIDTH; x++)
            fb[y][x] = UNWRITTEN;
}

static int run_case(const char *name, int rows, int top, int bot, int verbose)
{
    WATERFALL_ROWS = rows;
    SCOPE_TOP = top;
    SCOPE_BOT = bot;

    srand(12345);
    for (int r = 0; r < rows; r++)
        for (int x = 0; x < WATERFALL_COLS; x++)
            wbuf[r][x] = (uint8_t)(rand() & 0xFF);

    uint8_t newest = (uint8_t)(rows > 7 ? 7 : 0);

    static uint16_t fb_old[LCD_HEIGHT][LCD_WIDTH];

    fb_reset();
    memset(&ctr, 0, sizeof ctr);
    blit_old(newest);
    memcpy(fb_old, fb, sizeof fb);
    uint32_t old_sw = ctr.set_window, old_fr = ctr.fill_rect, old_pw = ctr.pixel_writes;

    fb_reset();
    memset(&ctr, 0, sizeof ctr);
    blit_new(newest);

    int diffs = 0, unwritten_old = 0, unwritten_new = 0;
    for (int y = SCOPE_TOP; y < SCOPE_BOT; y++)
        for (int x = 0; x < LCD_WIDTH; x++) {
            if (fb_old[y][x] == UNWRITTEN) unwritten_old++;
            if (fb[y][x] == UNWRITTEN) unwritten_new++;
            if (fb_old[y][x] != fb[y][x]) diffs++;
        }

    printf("%-42s rows=%-3d plot=%dx%d\n", name, rows, LCD_WIDTH, SCOPE_BOT - SCOPE_TOP);
    printf("   OLD  set_window=%-7u fill_rect=%-7u pixel_writes=%-7u"
           "  cmd+data8 bus xfers=%u\n",
           old_sw, old_fr, old_pw, old_sw * 11);
    printf("   NEW  set_window=%-7u fill_rect=%-7u pixel_writes=%-7u"
           "  cmd+data8 bus xfers=%u   (write_pixels=%u)\n",
           ctr.set_window, ctr.fill_rect, ctr.pixel_writes,
           ctr.set_window * 11, ctr.write_pixels);
    printf("   pixels differing in plot area: %d   unwritten old=%d new=%d  -> %s\n\n",
           diffs, unwritten_old, unwritten_new,
           (diffs == 0 && unwritten_new == 0) ? "IDENTICAL" : "*** MISMATCH ***");
    (void)verbose;
    return (diffs == 0 && unwritten_new == 0) ? 0 : 1;
}

int main(void)
{
    int bad = 0;
    /* shipping geometry: SCOPE_TOP 18, SCOPE_BOT 224, 64 rows -> rh 3, 14px residue */
    bad |= run_case("shipping geometry (206px plot, 64 rows)", 64, 18, 224, 1);
    /* exact multiple: no residual strip */
    bad |= run_case("exact multiple (192px plot, 64 rows)", 64, 18, 210, 1);
    /* fewer rows than pixels, large row_height */
    bad |= run_case("16 rows (rh=12, 14px residue)", 16, 18, 224, 1);
    /* degenerate: more rows than plot lines -> row_height clamps to 1 */
    bad |= run_case("more rows than lines (rh clamps to 1)", 250, 18, 224, 1);

    /* NEGATIVE CONTROL: the comparison must be able to fail. Corrupt one
     * history byte between the two runs and confirm a mismatch is reported. */
    printf("negative control (one byte perturbed between runs):\n");
    {
        WATERFALL_ROWS = 64; SCOPE_TOP = 18; SCOPE_BOT = 224;
        srand(12345);
        for (int r = 0; r < 64; r++)
            for (int x = 0; x < WATERFALL_COLS; x++)
                wbuf[r][x] = (uint8_t)(rand() & 0xFF);
        static uint16_t fb_old[LCD_HEIGHT][LCD_WIDTH];
        fb_reset(); memset(&ctr, 0, sizeof ctr);
        blit_old(7);
        memcpy(fb_old, fb, sizeof fb);
        wbuf[7][100] ^= 0xFF;                    /* perturb */
        fb_reset(); memset(&ctr, 0, sizeof ctr);
        blit_new(7);
        int diffs = 0;
        for (int y = SCOPE_TOP; y < SCOPE_BOT; y++)
            for (int x = 0; x < LCD_WIDTH; x++)
                if (fb_old[y][x] != fb[y][x]) diffs++;
        printf("   pixels differing: %d  -> %s\n\n", diffs,
               diffs > 0 ? "control OK (test can fail)" : "*** CONTROL FAILED ***");
        if (diffs == 0) bad = 1;
    }

    printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return bad;
}
