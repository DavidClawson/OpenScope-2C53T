/*
 * Test: Font system - struct integrity and string width calculation
 */

#include <stdio.h>
#include <string.h>

/* Stub out LCD functions since we're testing font logic only */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
/* The stubs count what reaches the panel so a test can tell "drew the whole
 * string" from "returned a plausible width". Opaque draws go through
 * lcd_set_window + lcd_write_data; transparent ones through lcd_set_pixel. */
static unsigned long px_writes = 0;
static unsigned short px_max_x = 0;
static unsigned short win_x = 0, win_w = 0;
void lcd_set_pixel(unsigned short x, unsigned short y, unsigned short c) { (void)y; (void)c; px_writes++; if (x > px_max_x) px_max_x = x; }
void lcd_set_window(unsigned short x, unsigned short y, unsigned short w, unsigned short h) { (void)y; (void)h; win_x = x; win_w = w; if (w && x + w - 1 > px_max_x) px_max_x = x + w - 1; }
void lcd_write_data(unsigned short d) { (void)d; px_writes++; }
static void px_reset(void) { px_writes = 0; px_max_x = 0; win_x = win_w = 0; }

#include "font.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-40s ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) printf("FAIL: %s\n", msg)
#define ASSERT(cond, msg) do { if (cond) { PASS(); } else { FAIL(msg); } } while(0)

static void test_font_struct(const font_t *f, const char *name, int expected_height)
{
    char buf[80];

    snprintf(buf, sizeof(buf), "%s: height = %d", name, expected_height);
    TEST(buf);
    ASSERT(f->height == expected_height, "wrong height");

    snprintf(buf, sizeof(buf), "%s: has glyph data", name);
    TEST(buf);
    ASSERT(f->data != NULL, "null data pointer");

    snprintf(buf, sizeof(buf), "%s: has charmap", name);
    TEST(buf);
    ASSERT(f->charmap != NULL, "null charmap");

    snprintf(buf, sizeof(buf), "%s: num_glyphs > 0", name);
    TEST(buf);
    ASSERT(f->num_glyphs > 0, "no glyphs");

    snprintf(buf, sizeof(buf), "%s: first_char <= last_char", name);
    TEST(buf);
    ASSERT(f->first_char <= f->last_char, "char range inverted");
}

static void test_string_width(void)
{
    TEST("font_small: string width > 0");
    uint16_t w = font_string_width("Hello", &font_small);
    ASSERT(w > 0, "zero width");

    TEST("font_small: empty string width = 0");
    w = font_string_width("", &font_small);
    ASSERT(w == 0, "non-zero width for empty string");

    TEST("font_xlarge: digit width > 0");
    w = font_string_width("13.82", &font_xlarge);
    ASSERT(w > 0, "zero width for digits");

    TEST("font_xlarge: wider string = more pixels");
    uint16_t w1 = font_string_width("1", &font_xlarge);
    uint16_t w5 = font_string_width("12345", &font_xlarge);
    ASSERT(w5 > w1, "5 digits not wider than 1");

    TEST("font_large: 'W' wider than 'i'");
    uint16_t wW = font_string_width("W", &font_large);
    uint16_t wi = font_string_width("i", &font_large);
    ASSERT(wW > wi, "W not wider than i (variable width broken?)");
}

static void test_draw_returns_width(void)
{
    TEST("font_draw_string returns width");
    uint16_t w = font_draw_string(0, 0, "Test", 0xFFFF, 0x0000, &font_medium);
    ASSERT(w > 0, "returned zero");

    TEST("font_draw_char returns advance");
    uint8_t adv = font_draw_char(0, 0, 'A', 0xFFFF, 0x0000, &font_medium);
    ASSERT(adv > 0, "returned zero advance");
}

/* Right-aligned labels at the screen edge must keep their last glyph.
 *
 * Bench, 2026-09-22: the FFT axis label right-aligned to x = 318 read
 * `24.9kH` — font_draw_string() stopped when x + font height passed the
 * edge, pricing a 5 px 'z' at 12 px. The reference is the same string drawn
 * well inside the screen: the pixel count must match, and the rightmost
 * pixel must land inside the panel. The negative control reproduces the old
 * rule on the same glyph advances and shows it would have dropped the 'z'. */
static void test_right_aligned_edge_label(void)
{
    const char *lbl = "24.9kHz";

    px_reset();
    font_draw_string(100, 0, lbl, 0xFFFF, 0x0000, &font_small);
    unsigned long inside = px_writes;

    px_reset();
    font_draw_string_right(LCD_WIDTH - 2, 0, lbl, 0xFFFF, 0x0000, &font_small);
    unsigned long at_edge = px_writes;
    unsigned short max_x = px_max_x;

    TEST("right-aligned label at x=318 draws every glyph");
    ASSERT(at_edge == inside && inside > 0, "pixel count differs from the same string drawn mid-screen");

    TEST("right-aligned label stays inside the panel");
    ASSERT(max_x <= LCD_WIDTH - 2, "rightmost pixel past x_right");

    /* Negative control: the old rule on the same metrics. */
    uint16_t w = font_string_width(lbl, &font_small);
    uint16_t x = (uint16_t)(LCD_WIDTH - 2 - w);
    int dropped = 0;
    for (const char *c = lbl; *c; c++) {
        if (x + font_small.height > LCD_WIDTH) { dropped++; }
        x += font_small.advances[font_small.charmap[*c - font_small.first_char]];
    }
    TEST("negative control: old height-based rule drops the 'z'");
    ASSERT(dropped >= 1, "old rule would not have reproduced the bench defect");
}

int main(void)
{
    printf("=== Font System Tests ===\n\n");

    printf("[Font struct integrity]\n");
    test_font_struct(&font_small,  "font_small",  12);
    test_font_struct(&font_medium, "font_medium", 16);
    test_font_struct(&font_large,  "font_large",  24);
    test_font_struct(&font_xlarge, "font_xlarge", 39);

    printf("\n[String width measurement]\n");
    test_string_width();

    printf("\n[Drawing return values]\n");
    test_draw_returns_width();

    printf("\n[Edge clipping]\n");
    test_right_aligned_edge_label();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
