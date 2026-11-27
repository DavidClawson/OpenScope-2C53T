/*
 * soundlight.c - Sound and Light settings (menu item 1).
 *
 * Brightness: PB8 is the LCD backlight. Stock drives it AF push-pull and
 * PWM-dims it (fpga.c PB9 note; docs/stock_vs_openscope.md). OpenScope
 * configures it as a plain output and sets it HIGH once (main.c ~1039).
 * Here PB8 becomes TMR4_CH3 PWM (~24 kHz, above audible) and duty maps
 * the 0-100 percent setting. TMR4 is otherwise unused by this firmware
 * (TMR3 = button scan, TMR13 = CH2 trigger reference); PB9, TMR4_CH4 in
 * stock, stays with the continuity buzzer driver.
 *
 * Register access follows the TMR13 idiom in scope_trigger.c (direct
 * volatile pointers, offsets against at32f403a_407_tmr.h) so the file
 * stays host-test-linkable: hardware paths compile only when SL_HW.
 *
 * Sound: the continuity buzzer (PB9) already has a task and a force
 * path (continuity_buzzer.c). Sound off = that task stays silent; the
 * forced-preview beep and continuity beeps both pass through
 * soundlight_beep_ok() so one switch kills both.
 */

#include "soundlight.h"

/* Host builds (test_config_persist) link this file for the state
 * round-trip only; the PWM/buzzer hardware paths stay on the device. */
#ifdef SOUNDLIGHT_HOST_TESTS
#define SL_HW 0
#else
#define SL_HW 1
#endif

#if SL_HW
#include "continuity_buzzer.h"
#include "at32f403a_407.h"
#endif

/* ── TMR4 CH3 (PB8, backlight) register map ──────────────────────────────
 * TMR4 base 0x40000800 (APB1PERIPH_BASE + 0x0800, at32f403a_407.h).
 * Offsets per at32f403a_407_tmr.h: CTRL1 0x00, SWEVT 0x14, CVAL 0x24,
 * DIV 0x28, PR 0x2C, CM1 0x18 (CH1/CH2), CM2 0x1C (CH3/CH4: OCM3 bits
 * [6:4], CCSEL3 [9:8]), CCTRL 0x20 (C3EN bit8, C3P bit9), C3DT 0x3C.
 * (C4DT is 0x40 - writing there dims the buzzer pin, not the backlight.) */
#if SL_HW
#define TMR4_CTRL1  (*(volatile uint32_t *)0x40000800)  /* CEN = bit0 */
#define TMR4_SWEVT  (*(volatile uint32_t *)0x40000814)  /* OVFSWTR/UG = bit0 */
#define TMR4_DIV    (*(volatile uint32_t *)0x40000828)  /* prescaler */
#define TMR4_PR     (*(volatile uint32_t *)0x4000082C)  /* period / ARR */
#define TMR4_CM2    (*(volatile uint32_t *)0x4000081C)  /* CH3/CH4 mode */
#define TMR4_CCTRL  (*(volatile uint32_t *)0x40000820)  /* C3EN bit8, C3P bit9 */
#define TMR4_C3DT   (*(volatile uint32_t *)0x4000083C)  /* CH3 compare (duty) */
#endif

/* APB1 timer clock is 120 MHz (button_scan.c TMR3 comment). PSC=4 ->
 * 24 MHz counter; ARR=999 -> 24 kHz PWM, above audible. */
#define SL_TMR4_DIV_VAL   4u
#define SL_TMR4_ARR       999u

/* One-time hardware bring-up. Idempotent. Host builds never call it
 * (SL_HW 0), so it is compiled out entirely there. */
#if SL_HW
static bool sl_pwm_ready;

static void sl_pwm_init(void)
{
    if (sl_pwm_ready) return;

    crm_periph_clock_enable(CRM_TMR4_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    /* PB8 = TMR4_CH3 output, AF push-pull (same shape as the TMR13/PA6
     * init in scope_trigger.c). tmr4_mux left 0: stock never writes the
     * IOMUX remap for TMR4, so CH3 stays on PB8. */
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_8;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    TMR4_DIV = SL_TMR4_DIV_VAL;
    TMR4_PR  = SL_TMR4_ARR;
    /* CH3 PWM mode 0b110 (OCM3 = 0b110), CC3S = 00 (output). Preserve
     * other bits - CH4 shares CM2 if anything ever claims it. */
    TMR4_CM2 = (TMR4_CM2 & ~0x7u) | (0x6u << 4);
    /* C3EN, active-high (C3P = 0: duty = C3DT/(ARR+1) high time). */
    TMR4_CCTRL = (TMR4_CCTRL & ~0x300u) | (1u << 8);
    /* Latch PSC/ARR via an update event, then run. */
    TMR4_SWEVT |= 1u;
    TMR4_CTRL1 |= 1u;

    sl_pwm_ready = true;
}
#endif /* SL_HW */

/* ── Live state ────────────────────────────────────────────────────────── */

static uint8_t sl_brightness;   /* 0-100 */
static bool    sl_sound;

static void sl_push_brightness(void)
{
#if SL_HW
    if (sl_brightness == 0u) {
        /* Dark: plain output LOW, timer keeps running. Keeps the panel off
         * path identical to the boot-time dark panel (main.c PB8 LOW). */
        gpio_init_type gpio_cfg;
        gpio_default_para_init(&gpio_cfg);
        gpio_cfg.gpio_pins = GPIO_PINS_8;
        gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
        gpio_init(GPIOB, &gpio_cfg);
        GPIOB->clr = GPIO_PINS_8;
        return;
    }
    sl_pwm_init();
    /* Linear duty: percent -> C3DT. 100% = ARR+1 counts of high. */
    uint32_t cdt = ((uint32_t)sl_brightness * (SL_TMR4_ARR + 1u)) / 100u;
    if (cdt > SL_TMR4_ARR) cdt = SL_TMR4_ARR;
    TMR4_C3DT = cdt;
#endif
}

void soundlight_set_brightness(uint8_t percent)
{
    if (percent > 100u) percent = 100u;
    sl_brightness = percent;
    sl_push_brightness();
}

void soundlight_set_sound(bool on)
{
    sl_sound = on;
}

uint8_t soundlight_get_brightness(void) { return sl_brightness; }
bool soundlight_get_sound(void) { return sl_sound; }

bool soundlight_beep_ok(void)
{
    return sl_sound;
}

void soundlight_beep(void)
{
    if (!sl_sound) return;
#if SL_HW
    continuity_buzzer_force_ms(60);
#endif
}

void soundlight_adjust(int row, int dir)
{
    if (row == 0) {
        /* Brightness steps of 20, wrapping 100 -> 20. */
        uint8_t b = sl_brightness;
        if (b == 0u || b >= 100u) {
            b = (dir >= 0) ? 20u : 100u;
        } else {
            int nb = (int)b + 20 * dir;
            if (nb < 0) nb = 0;
            if (nb > 100) nb = 100;
            b = (uint8_t)nb;
        }
        soundlight_set_brightness(b);
    } else if (row == 1) {
        /* Sound toggle; audible the moment it lands on. */
        bool on = !sl_sound;
        soundlight_set_sound(on);
        if (on) soundlight_beep();
    }
}

/* ── Settings store hooks ──────────────────────────────────────────────── */

void soundlight_apply(const device_config_t *cfg)
{
    if (!cfg) return;

    uint8_t b = cfg->display_brightness;
    if (b > 100u) b = 100u;
    soundlight_set_brightness(b);
    soundlight_set_sound(cfg->sound_enabled != 0u);
}

void soundlight_capture(device_config_t *cfg)
{
    if (!cfg) return;
    cfg->display_brightness = sl_brightness;
    cfg->sound_enabled      = sl_sound ? 1u : 0u;
}
