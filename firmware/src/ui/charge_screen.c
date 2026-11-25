/*
 * OpenScope 2C53T - charge screen. See charge_screen.h.
 *
 * Charger status, decoded from stock V1.2.0, not yet bench-confirmed:
 *   PC7  LOW = charging. Stock's charge loop (0x0802B39A) draws the
 *        animation frames 163..167.jpg when PC7 is LOW; its status-bar
 *        battery icon (0x0801F93C) and battery monitor (0x08009CAE) read
 *        PC7 the same way.
 *   PC14 LOW = charge complete. With PC7 HIGH, stock draws the static
 *        162.jpg when PC14 is LOW and nothing when it is HIGH.
 * Both look like the open-drain CHRG/STDBY pair of a 4056-class charger
 * (the TC4056A on the unlabelled board), read with pull-ups, which is the
 * posture stock leaves them in (swd_chargemode_baseline.txt).
 */

#include "charge_screen.h"
#include "power.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "at32f403a_407.h"

#define TICK_MS       50u
#define ANIM_TICKS    8u          /* 400 ms per animation step */
#define HOLD_TICKS    ((CHARGE_SCREEN_HOLD_MS + TICK_MS - 1u) / TICK_MS)

/* Battery outline and its five segments */
#define BODY_X   100
#define BODY_Y   70
#define BODY_W   110
#define BODY_H   60
#define BORDER   4
#define NUB_W    8
#define NUB_H    24
#define SEGS     5
#define SEG_W    16
#define SEG_GAP  3
#define SEG_X0   (BODY_X + BORDER + 5)
#define SEG_Y    (BODY_Y + BORDER + 4)
#define SEG_H    (BODY_H - 2 * (BORDER + 4))

typedef enum { CHG_NONE, CHG_CHARGING, CHG_FULL } chg_state_t;

static chg_state_t read_state(void)
{
    uint32_t idt = GPIOC->idt;
    if (!(idt & (1u << 7)))  return CHG_CHARGING;
    if (!(idt & (1u << 14))) return CHG_FULL;
    return CHG_NONE;
}

static void draw_frame(const theme_t *th)
{
    uint16_t bg = th->background;
    uint16_t fg = th->text_secondary;

    lcd_clear(bg);
    lcd_fill_rect(BODY_X, BODY_Y, BODY_W, BORDER, fg);
    lcd_fill_rect(BODY_X, BODY_Y + BODY_H - BORDER, BODY_W, BORDER, fg);
    lcd_fill_rect(BODY_X, BODY_Y, BORDER, BODY_H, fg);
    lcd_fill_rect(BODY_X + BODY_W - BORDER, BODY_Y, BORDER, BODY_H, fg);
    lcd_fill_rect(BODY_X + BODY_W, BODY_Y + (BODY_H - NUB_H) / 2,
                  NUB_W, NUB_H, fg);
    font_draw_string_center(160, 200, "Hold POWER to turn on",
                            th->text_secondary, bg, &font_medium);
}

static void draw_segments(const theme_t *th, int lit)
{
    for (int i = 0; i < SEGS; i++) {
        lcd_fill_rect(SEG_X0 + i * (SEG_W + SEG_GAP), SEG_Y, SEG_W, SEG_H,
                      i < lit ? th->success : th->background);
    }
}

static void draw_label(const theme_t *th, chg_state_t s)
{
    const char *txt = (s == CHG_CHARGING) ? "Charging" :
                      (s == CHG_FULL)     ? "Charged"  : "USB power";
    lcd_fill_rect(0, 145, LCD_WIDTH, 34, th->background);
    font_draw_string_center(160, 150, txt, th->text_primary,
                            th->background, &font_large);
}

void charge_screen_run(bool need_release)
{
    const theme_t *th = theme_get();

    /* PC7 / PC14 as inputs with pull-up for the duration, then put back:
     * nothing else in this firmware configures them, but fpga.c reads PC7
     * for the meter command tail, and that behaviour must not change
     * depending on whether the boot went through this screen. */
    uint32_t saved_l   = GPIOC->cfglr & (0xFu << 28);
    uint32_t saved_h   = GPIOC->cfghr & (0xFu << 24);
    uint32_t saved_odt = GPIOC->odt & ((1u << 7) | (1u << 14));
    GPIOC->cfglr = (GPIOC->cfglr & ~(0xFu << 28)) | (0x8u << 28);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 24)) | (0x8u << 24);
    GPIOC->scr   = (1u << 7) | (1u << 14);

    draw_frame(th);
    chg_state_t shown = (chg_state_t)-1;
    int shown_lit = -1;
    unsigned step = 0, tick = 0, held = 0;
    bool lit_bl = false;

    for (;;) {
        wdt_counter_reload();

        chg_state_t s = read_state();
        if (s != shown) {
            draw_label(th, s);
            shown = s;
            step = 0;
        }
        int lit = (s == CHG_CHARGING) ? (int)(step % SEGS) + 1 :
                  (s == CHG_FULL)     ? SEGS : 0;
        if (lit != shown_lit) {
            draw_segments(th, lit);
            shown_lit = lit;
        }
        if (!lit_bl) {
            GPIOB->scr = (1u << 8);        /* backlight after the first frame */
            lit_bl = true;
        }

        if (power_button_held()) {
            if (!need_release && ++held >= HOLD_TICKS)
                break;
        } else {
            need_release = false;
            held = 0;
        }

        delay_ms(TICK_MS);
        if (++tick >= ANIM_TICKS) {
            tick = 0;
            step++;
        }
    }

    GPIOC->cfglr = (GPIOC->cfglr & ~(0xFu << 28)) | saved_l;
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 24)) | saved_h;
    GPIOC->odt   = (GPIOC->odt & ~((1u << 7) | (1u << 14))) | saved_odt;
}
