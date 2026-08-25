/*
 * OpenScope 2C53T - Oscilloscope UI
 *
 * Waveform display with:
 *   - Trigger level arrow + dotted line
 *   - Ground reference markers (CH1/CH2 zero-volt arrows)
 *   - Trigger status badge (Auto/Trig'd/Ready/Stop)
 *   - Run/Stop indicator
 *   - Auto-measurement badges (Freq, Vpp, Vrms, Duty, Period, Rise)
 *   - Quick-change popup overlay
 */

#include "ui.h"
#include "lcd.h"
#include "font.h"
#include "theme.h"
#include "scope_state.h"
#include "scope_measure.h"
#include "scope_cal.h"
#include "scope_timebase.h"
#include "scope_freq.h"
#include "math_channel.h"
#include "persistence.h"
#include "fpga.h"
#include "scope_trigger.h"
#include "at32f403a_407.h"  /* GPIO port reads in the debug overlay */
#include <stdio.h>
#include <math.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Layout constants
 * ═══════════════════════════════════════════════════════════════════ */

#define SCOPE_TOP       18      /* Below status bar */
#define SCOPE_BOT       (LCD_HEIGHT - 16)  /* Above info bar */
#define SCOPE_H         (SCOPE_BOT - SCOPE_TOP)
#define SCOPE_MID_Y     (SCOPE_TOP + SCOPE_H / 2)
#ifdef SCOPE_DEBUG_OVERLAY
#define SCOPE_DBG_Y     (LCD_HEIGHT - 60)  /* top of the debug strip */
#endif

/* Measurement badge layout */
#define BADGE_W         76
#define BADGE_H         14
#define BADGE_PAD       2
/* Badge rows.
 *
 * BENCH 2026-08-14: with SCOPE_DEBUG_OVERLAY the badges were INVISIBLE, and had
 * been in every build — `-DSCOPE_DEBUG_OVERLAY` is unconditional in the
 * Makefile's C_DEFS, so there was no configuration in which they showed. The
 * overlay's four lines start at SCOPE_DBG_Y (=180) and run to ~219, painting
 * straight over rows that sat at 193 and 208.
 *
 * Debug builds therefore stack the badges ABOVE the debug strip. That costs
 * waveform height, which is the right trade: an overlay build is a bench build,
 * and a measurement you cannot see is worth less than the pixels it hides. */
#ifdef SCOPE_DEBUG_OVERLAY
#define BADGE_ROW_Y     (SCOPE_DBG_Y - BADGE_H - 2)
#define BADGE_ROW2_Y    (BADGE_ROW_Y - BADGE_H - 1)
#else
#define BADGE_ROW_Y     (SCOPE_BOT - BADGE_H - 2)
#define BADGE_ROW2_Y    (BADGE_ROW_Y - BADGE_H - 1)
#endif

/*
 * How many consecutive refusals the Freq badge holds its last reading for.
 * At the measured ~46% refusal rate a hold of 8 frames makes a dropout
 * visible only when the signal genuinely goes away, while still expiring
 * fast enough that a removed probe blanks the badge within a second.
 */
#define FREQ_HOLD_FRAMES  8u

/* Quick-change popup */
#define POPUP_W         200
#define POPUP_H         36
#define POPUP_X         ((LCD_WIDTH - POPUP_W) / 2)
#define POPUP_Y         ((LCD_HEIGHT - POPUP_H) / 2)

/* ═══════════════════════════════════════════════════════════════════
 * Quick-change popup state (shared with main.c via extern)
 * ═══════════════════════════════════════════════════════════════════ */

static char popup_text[32] = "";
static uint8_t popup_frames = 0;  /* Countdown: show for N frames then dismiss */

/* Has the box been painted for THIS popup? The incremental path paints it
 * once and then leaves it alone (see draw_scope_live_frame's hole); the full
 * path repaints it every frame anyway. Without this, every live frame would
 * repaint the box and the text would strobe at the capture rate. */
static bool popup_painted = false;

#define POPUP_DURATION 10  /* ~500ms at 20fps */

void scope_show_popup(const char *text)
{
    int i = 0;
    while (text[i] && i < (int)sizeof(popup_text) - 1) {
        popup_text[i] = text[i];
        i++;
    }
    popup_text[i] = '\0';
    popup_frames = POPUP_DURATION;
    popup_painted = false;
}

bool scope_popup_active(void)
{
    return popup_frames > 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Grid
 * ═══════════════════════════════════════════════════════════════════ */

void draw_scope_grid(void)
{
    const theme_t *th = theme_get();
    uint16_t x, y;

    /* Vertical grid lines (dotted) */
    for (x = 0; x < LCD_WIDTH; x += 32) {
        for (y = SCOPE_TOP; y < SCOPE_BOT; y += 2)
            lcd_set_pixel(x, y, th->grid);
    }
    /* Horizontal grid lines (dotted) */
    for (y = SCOPE_TOP; y < SCOPE_BOT; y += 26) {
        for (x = 0; x < LCD_WIDTH; x += 2)
            lcd_set_pixel(x, y, th->grid);
    }

    /* Center crosshair (solid) */
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, SCOPE_MID_Y, th->grid_center);
    for (y = SCOPE_TOP; y < SCOPE_BOT; y++)
        lcd_set_pixel(LCD_WIDTH / 2, y, th->grid_center);
}

/* ═══════════════════════════════════════════════════════════════════
 * Trigger level indicator
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_trigger_indicator(const scope_state_t *ss, const theme_t *th)
{
    /* Trigger level as pixel Y position */
    int16_t trig_y = SCOPE_MID_Y - ss->trigger.level;
    if (trig_y < SCOPE_TOP + 2) trig_y = SCOPE_TOP + 2;
    if (trig_y > SCOPE_BOT - 3) trig_y = SCOPE_BOT - 3;

    /* Right-edge arrow: small triangle pointing left */
    uint16_t ax = LCD_WIDTH - 1;
    lcd_set_pixel(ax,     (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 3, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 4, (uint16_t)trig_y, th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)(trig_y - 1), th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)(trig_y - 2), th->trigger);
    lcd_set_pixel(ax - 1, (uint16_t)(trig_y + 1), th->trigger);
    lcd_set_pixel(ax - 2, (uint16_t)(trig_y + 2), th->trigger);

    /* Dotted horizontal line at trigger level */
    for (uint16_t x = 0; x < LCD_WIDTH - 6; x += 4)
        lcd_set_pixel(x, (uint16_t)trig_y, th->trigger);

    /* Trigger edge indicator (small arrow next to the trigger arrow) */
    if (ss->trigger.edge == TRIG_RISING) {
        /* Up arrow: rising edge */
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 2), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 1), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 1), th->trigger);
        lcd_set_pixel(ax - 7, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 5, (uint16_t)trig_y,       th->trigger);
    } else {
        /* Down arrow: falling edge */
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 2), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y - 1), th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 6, (uint16_t)(trig_y + 1), th->trigger);
        lcd_set_pixel(ax - 7, (uint16_t)trig_y,       th->trigger);
        lcd_set_pixel(ax - 5, (uint16_t)trig_y,       th->trigger);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Ground reference markers (left edge)
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_ground_markers(const scope_state_t *ss, const theme_t *th)
{
    /* CH1 ground marker — small right-pointing arrow with "1" */
    if (ss->ch1.enabled) {
        int16_t ch1_y = SCOPE_MID_Y - ss->ch1.position;
        if (ch1_y >= SCOPE_TOP + 2 && ch1_y <= SCOPE_BOT - 3) {
            lcd_set_pixel(0, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(1, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(2, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(3, (uint16_t)ch1_y, th->ch1);
            lcd_set_pixel(1, (uint16_t)(ch1_y - 1), th->ch1);
            lcd_set_pixel(2, (uint16_t)(ch1_y - 2), th->ch1);
            lcd_set_pixel(1, (uint16_t)(ch1_y + 1), th->ch1);
            lcd_set_pixel(2, (uint16_t)(ch1_y + 2), th->ch1);
            font_draw_string(5, (uint16_t)(ch1_y - 5), "1",
                             th->ch1, th->ch1, &font_small);
        }
    }

    /* CH2 ground marker */
    if (ss->ch2.enabled) {
        int16_t ch2_y = SCOPE_MID_Y - ss->ch2.position;
        if (ch2_y >= SCOPE_TOP + 2 && ch2_y <= SCOPE_BOT - 3) {
            lcd_set_pixel(0, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(1, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(2, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(3, (uint16_t)ch2_y, th->ch2);
            lcd_set_pixel(1, (uint16_t)(ch2_y - 1), th->ch2);
            lcd_set_pixel(2, (uint16_t)(ch2_y - 2), th->ch2);
            lcd_set_pixel(1, (uint16_t)(ch2_y + 1), th->ch2);
            lcd_set_pixel(2, (uint16_t)(ch2_y + 2), th->ch2);
            font_draw_string(5, (uint16_t)(ch2_y - 5), "2",
                             th->ch2, th->ch2, &font_small);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Trigger status badge
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_trigger_status(const scope_state_t *ss, const theme_t *th)
{
    const char *label;
    uint16_t color;

    if (!ss->running) {
        label = "STOP";
        color = th->warning;
    } else if (ss->trigger.mode == TRIG_AUTO) {
        label = "Auto";
        color = th->success;
    } else if (ss->trigger.mode == TRIG_SINGLE) {
        label = "Ready";
        color = th->highlight;
    } else {
        label = "Trig'd";
        color = th->success;
    }

    /* Draw badge at top-right of waveform area */
    uint16_t bw = 44;
    uint16_t bx = LCD_WIDTH - bw - 2;
    uint16_t by = SCOPE_TOP + 2;
    lcd_fill_rect(bx, by, bw, 14, th->background);
    font_draw_string_right(LCD_WIDTH - 4, by + 1, label,
                           color, th->background, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Run/Stop indicator
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_run_stop(const scope_state_t *ss, const theme_t *th)
{
    if (!ss->running) {
        /* Prominent STOP badge top-center */
        uint16_t bw = 40;
        uint16_t bx = (LCD_WIDTH - bw) / 2;
        lcd_fill_rect(bx, SCOPE_TOP + 2, bw, 16, th->warning);
        font_draw_string_center(LCD_WIDTH / 2, SCOPE_TOP + 4, "STOP",
                                th->text_primary, th->warning, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Measurement badges
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_one_badge(uint16_t x, uint16_t y, const char *label,
                           const char *value, uint16_t color, const theme_t *th)
{
    /* Semi-transparent background (just use dim background) */
    lcd_fill_rect(x, y, BADGE_W, BADGE_H, th->background);

    /* Label in dim color, value in bright channel color */
    font_draw_string(x + BADGE_PAD, y + 1, label,
                     th->text_secondary, th->background, &font_small);
    font_draw_string_right(x + BADGE_W - BADGE_PAD, y + 1, value,
                           color, th->background, &font_small);
}

/* Shown wherever the instrument cannot measure the quantity today. */
#define MEAS_NA  "--"

/*
 * Measure one channel's live capture record.
 *
 * Returns false — and leaves *m zeroed — unless the FPGA has actually
 * delivered samples. fpga_data_ready() is the same gate the waveform plot
 * uses (see draw_demo_waveform), so the badges and the trace can never
 * disagree about whether what is on screen is real.
 */
static bool measure_live_channel(const volatile uint8_t *buf,
                                 scope_measure_t *m)
{
    memset(m, 0, sizeof(*m));
    if (!fpga_data_ready() || buf == NULL)
        return false;

    /* Tear-checked read (audit 2026-08-20, P0.2). fpga.c now stages each
     * capture and commits it in a ~10 µs window bracketed by the frame
     * generation counter (odd = commit in progress); a commit landing inside
     * our analysis pass would splice two frames through every statistic, so
     * re-run the pass when the generation moved. Commits arrive at ≤ ~34 Hz,
     * so the second attempt virtually always stands; after three, accept the
     * torn result (pre-fix behavior) rather than spin. The volatile cast is
     * safe because the generation check is what detects concurrent writes. */
    for (int attempt = 0; attempt < 3; attempt++) {
        uint32_t g0 = fpga_acq_frame_generation();
        scope_measure_record((const uint8_t *)buf, FPGA_ADC_BUF_SIZE, m);
        if ((g0 & 1u) == 0u && fpga_acq_frame_generation() == g0)
            break;
    }
    return m->valid;
}

/* No float formatting: the firmware links newlib-nano without
 * -u _printf_float, so "%f" prints nothing. Tenths are assembled by hand,
 * exactly as format_si()/format_freq() do below. */
static void fmt_counts(char *b, size_t n, unsigned v)
{
    snprintf(b, n, "%ucnt", v);
}

static void fmt_tenths(char *b, size_t n, float v, const char *unit)
{
    if (v < 0.0f) v = 0.0f;
    unsigned t = (unsigned)(v * 10.0f + 0.5f);
    snprintf(b, n, "%u.%u%s", t / 10u, t % 10u, unit);
}

/*
 * Frequency with automatic Hz/kHz/MHz ranging, integer math only.
 * Same constraint as fmt_volts: newlib-nano's "%f" prints nothing.
 */
__attribute__((unused))
static void fmt_hz(char *b, size_t n, float hz)
{
    if (hz < 0.0f) hz = 0.0f;
    if (hz < 1000.0f) {
        const unsigned tenths = (unsigned)(hz * 10.0f + 0.5f);
        snprintf(b, n, "%u.%uHz", tenths / 10u, tenths % 10u);
    } else if (hz < 1000000.0f) {
        const unsigned hundredths = (unsigned)(hz / 10.0f + 0.5f);
        snprintf(b, n, "%u.%02ukHz", hundredths / 100u, hundredths % 100u);
    } else {
        const unsigned hundredths = (unsigned)(hz / 10000.0f + 0.5f);
        snprintf(b, n, "%u.%02uMHz", hundredths / 100u, hundredths % 100u);
    }
}

/*
 * A duration in seconds, ranged us/ms/s. Integer math, as above.
 */
static void fmt_seconds(char *b, size_t n, float s)
{
    if (s < 0.0f) s = 0.0f;
    if (s < 1e-3f) {
        snprintf(b, n, "%uus", (unsigned)(s * 1e6f + 0.5f));
    } else if (s < 1.0f) {
        const unsigned hundredths_ms = (unsigned)(s * 1e5f + 0.5f);
        snprintf(b, n, "%u.%02ums", hundredths_ms / 100u, hundredths_ms % 100u);
    } else {
        const unsigned hundredths = (unsigned)(s * 100.0f + 0.5f);
        snprintf(b, n, "%u.%02us", hundredths / 100u, hundredths % 100u);
    }
}

/*
 * Format a voltage with automatic mV/V ranging, integer math only (same
 * reason as fmt_tenths: newlib-nano's "%f" prints nothing). Sub-volt values
 * read as whole millivolts ("154mV"); at/above 1 V, two decimals ("1.66V").
 * Rounding is done once, in millivolts, so 0.9995 V shows "1.00V" rather than
 * the nonsense "1000mV".
 */
static void fmt_volts(char *b, size_t n, float v)
{
    if (v < 0.0f) v = 0.0f;
    unsigned mv = (unsigned)(v * 1000.0f + 0.5f);   /* millivolts, rounded */
    if (mv < 1000u)
        snprintf(b, n, "%umV", mv);
    else
        snprintf(b, n, "%u.%02uV", mv / 1000u, (mv % 1000u) / 10u);
}

/*
 * Vertical calibration now lives in scope_cal.c, per CHANNEL as well as per
 * range, because the two channels have different frontends and measure
 * different gains on the same range index.
 *
 * WITHDRAWN 2026-08-18 — what used to be here was a two-case switch:
 *     case 2: 1.0f/347.0f   ("20mV/div")
 *     case 8: 1.0f/154.0f   ("2V/div")
 * The five-amplitude sweep of 2026-08-17 contradicts both. Range 2 rails on
 * both channels and yields no span at all; range 8 measures 279 mV/count, not
 * 6.49. The full provenance and the reason the replacement is tiered rather
 * than a flat table is in scope_cal.h.
 *
 * The compile-time check below is what keeps the volts/div label honest.
 * SCOPE_CAL_COUNTS_PER_DIV is a claim about THIS renderer's geometry — the
 * 26-pixel grid pitch against a 206-pixel plot showing 256 counts. If either
 * changes, the label silently starts meaning something else, so make that a
 * build failure instead.
 */
_Static_assert((26 * 256 + SCOPE_H / 2) / SCOPE_H == (int)SCOPE_CAL_COUNTS_PER_DIV,
               "grid pitch no longer matches SCOPE_CAL_COUNTS_PER_DIV — "
               "the volts/div labels in the status bar would be wrong");

/* The cal table is indexed by the frontend range index, so its length and
 * the UI's range count are the same quantity counted twice. */
_Static_assert(SCOPE_CAL_RANGE_COUNT == VDIV_COUNT,
               "scope_cal range table length must match VDIV_COUNT");

/* The horizontal counterpart: one sample per screen column, vertical grid
 * every 32 px, so a division is 32 samples. If either changes, the seconds/div
 * label and every frequency silently start meaning something else. */
_Static_assert(SCOPE_TIMEBASE_SAMPLES_PER_DIV == 32.0f,
               "grid pitch no longer matches SCOPE_TIMEBASE_SAMPLES_PER_DIV");
_Static_assert(SCOPE_TIMEBASE_CODE_COUNT == TIMEBASE_COUNT,
               "timebase rate table length must match TIMEBASE_COUNT");

/*
 * Measurement badges.
 *
 * Until 2026-08-13 this function printed the string literals "1.00kHz",
 * "707mV", "50.0%" and "1.00ms" — four numbers that had never touched an
 * ADC sample, on a screen a user reads as measurements. They are gone.
 *
 * What is printed now is measured from the live record, in the units this
 * instrument can actually defend (see scope_measure.h):
 *
 *   Vpp / Vrms   VOLTS wherever scope_cal.c has a bench-measured
 *                volts-per-count for that CHANNEL on that range: ranges 4-9,
 *                with 5/6/7 cross-validated three ways and 4/8/9 provisional.
 *                Ranges 0-3 rail on both channels and have no gain at all, so
 *                they still show honest ADC COUNTS ("Ncnt") rather than a
 *                plausible-but-invented voltage.
 *   Duty         a pure ratio; invariant under any affine counts->volts
 *                mapping, so it is already correct and stays correct once
 *                calibration lands. Asserted in tests/test_scope_measure.c.
 *   Freq         Hz from the SPECTRAL estimator (scope_freq.c) on measured
 *                timebase codes; declines rather than guess, with a short
 *                hold (see below). "--" on unmeasured codes.
 *   Per          SECONDS as 1/Freq while the spectral estimate stands;
 *                otherwise SAMPLES between mid-level crossings, never
 *                converted, because that crossing period is 5-50% out
 *                (EXP-13) and a bad number in real units reads as true.
 *
 * The wiring is Vpp = pp * k, Vrms = ac_rms * k, with k from
 * scope_cal_volts_per_count(ch, vdiv_idx), k == 0 meaning "show counts";
 * Hz needs the measured rate table in scope_timebase.c (EXP-17/18) and the
 * reg-0x01 code actually in force. `fpga scope measure` prints these same
 * quantities raw for the bench, from the same sources, so what EXP-19
 * validates is what the screen shows.
 */
static void draw_measurement_badges(const scope_state_t *ss, const theme_t *th)
{
    char buf[16];
    scope_measure_t m1, m2;

    const bool have1 = ss->ch1.enabled &&
                       measure_live_channel(fpga_get_ch1_buf(), &m1);
    const bool have2 = ss->ch2.enabled &&
                       measure_live_channel(fpga_get_ch2_buf(), &m2);
    const uint16_t na = th->text_secondary;   /* dim: nothing to report */

    /* ── Row 1 — CH1 ─────────────────────────────────────────────── */
    uint16_t x = 2;
    uint16_t y1 = BADGE_ROW_Y;

    /*
     * Freq — spectral, not edge-counted, and it declines rather than guess.
     *
     * History, because it is the whole justification for the shape of this:
     * Freq = fs / period_samples was wired up on 2026-08-18 and removed the
     * same evening, measuring +1.6% to +112% against a known drive (EXP-13).
     * The sample rate was never the problem; the rising-crossing period
     * estimator found spurious crossings.
     *
     * EXP-16 then showed it was not the estimator either. Edge counting,
     * autocorrelation and a spectral peak all failed on the SAME bench records
     * and all succeeded on the same others: roughly a fifth to a half of
     * capture records are TORN, with their spectral energy smeared over ~6
     * bins. One record scored 0.380 coherence whole and 0.999 over its last
     * half. Tearing does not track signal amplitude, so it cannot be screened
     * out in advance — it has to be detected per record, which is exactly what
     * scope_freq does. Against 72 bench captures it answers 54% of the time,
     * is within 3.4% when it does, and is never wrong.
     *
     * The reading is HELD across a short run of refusals so the badge does not
     * strobe at the tearing rate; after FREQ_HOLD_FRAMES consecutive refusals
     * it blanks, because a stale number with no expiry is just a wrong number
     * that used to be right.
     */
    static float    freq_hz;
    static uint16_t freq_stale;
    {
        /* timebase_idx IS the reg-0x01 code — fpga_stock_timebase_byte()
         * returns it unchanged, and status_bar.c reads it the same way. */
        const float fs = scope_timebase_sample_rate(ss->timebase_idx);
        scope_freq_t fr;

        /* The acquisition buffer is `volatile` because the acq task fills it;
         * the estimator only reads, and its coherence gate refuses torn
         * records anyway — so an MCU-side tear was never able to produce a
         * WRONG number here, only a refusal. The generation retry (P0.2)
         * recovers those refusals: a commit landing mid-estimate re-runs the
         * estimate on the settled frame instead of burning a hold frame. */
        _Static_assert(FPGA_ADC_BUF_SIZE == SCOPE_FREQ_MAX_N,
                       "estimator window must match the capture buffer");

        bool fe = false;
        if (have1) {
            for (int attempt = 0; attempt < 3; attempt++) {
                uint32_t g0 = fpga_acq_frame_generation();
                fe = scope_freq_estimate((const uint8_t *)fpga_get_ch1_buf(),
                                         FPGA_ADC_BUF_SIZE, fs, &fr);
                if ((g0 & 1u) == 0u && fpga_acq_frame_generation() == g0)
                    break;
            }
        }
        if (fe) {
            freq_hz = fr.hz;
            freq_stale = 0u;
        } else if (freq_stale < FREQ_HOLD_FRAMES) {
            freq_stale++;
        } else {
            freq_hz = 0.0f;
        }

        if (freq_hz > 0.0f) {
            fmt_hz(buf, sizeof(buf), freq_hz);
            draw_one_badge(x, y1, "Freq", buf, th->ch1, th);
        } else {
            draw_one_badge(x, y1, "Freq", MEAS_NA, na, th);
        }
    }
    x += BADGE_W + 2;

    /* Calibrated ranges show volts; all others fall back to ADC counts. */
    const float k1 = scope_cal_volts_per_count(1u, ss->ch1.vdiv_idx);

    if (have1) {
        /* pp_robust, not pp: raw max-min is noise-inflated by +4..+10% of
         * the commanded amplitude (EXP-19) — see scope_measure.h. */
        if (k1 > 0.0f) fmt_volts(buf, sizeof(buf), (float)m1.pp_robust * k1);
        else           fmt_counts(buf, sizeof(buf), m1.pp_robust);
        draw_one_badge(x, y1, "Vpp", buf, th->ch1, th);
    } else {
        draw_one_badge(x, y1, "Vpp", MEAS_NA, na, th);
    }
    x += BADGE_W + 2;

    if (have1) {
        if (k1 > 0.0f) fmt_volts(buf, sizeof(buf), m1.ac_rms * k1);
        else           fmt_counts(buf, sizeof(buf), (unsigned)(m1.ac_rms + 0.5f));
        draw_one_badge(x, y1, "Vrms", buf, th->ch1, th);
    } else {
        draw_one_badge(x, y1, "Vrms", MEAS_NA, na, th);
    }
    x += BADGE_W + 2;

    if (have1 && m1.level_valid) {
        fmt_tenths(buf, sizeof(buf), m1.duty_pct, "%");
        draw_one_badge(x, y1, "Duty", buf, th->ch1, th);
    } else {
        /* Flat/near-flat record: the mid-level threshold would be sitting
         * inside the noise, so a duty figure would be measuring dither. */
        draw_one_badge(x, y1, "Duty", MEAS_NA, na, th);
    }

    /* ── Row 2 — period, CH2, and the legend ─────────────────────── */
    x = 2;
    uint16_t y2 = BADGE_ROW2_Y;

    if (freq_hz > 0.0f) {
        /*
         * SECONDS, derived as 1/Freq from the SPECTRAL estimate above — the
         * one estimator this project trusts (S3: held-out fixture, never
         * wrong on 72 bench records). NOT from period_samples: the
         * mid-level-crossing period is 5-50% out (EXP-13), which is why it
         * was never dressed in real units. Same hold/expiry as the Freq
         * badge, so the two can never disagree on the screen.
         */
        fmt_seconds(buf, sizeof(buf), 1.0f / freq_hz);
        draw_one_badge(x, y2, "Per", buf, th->ch1, th);
    } else if (have1 && m1.period_valid) {
        /* No trustworthy Hz (unmeasured code, or the estimator declined and
         * the hold expired): fall back to the crossing count in honest
         * SAMPLES, never converted (EXP-13). */
        snprintf(buf, sizeof(buf), "%usmp",
                 (unsigned)(m1.period_samples + 0.5f));
        draw_one_badge(x, y2, "Per", buf, th->ch1, th);
    } else {
        draw_one_badge(x, y2, "Per", MEAS_NA, na, th);
    }
    x += BADGE_W + 2;

    if (have2) {
        const float k2 = scope_cal_volts_per_count(2u, ss->ch2.vdiv_idx);
        if (k2 > 0.0f) fmt_volts(buf, sizeof(buf), (float)m2.pp_robust * k2);
        else           fmt_counts(buf, sizeof(buf), m2.pp_robust);
        draw_one_badge(x, y2, "CH2pp", buf, th->ch2, th);
    } else {
        draw_one_badge(x, y2, "CH2pp", MEAS_NA, na, th);
    }
    x += BADGE_W + 2;

    /*
     * The legend is what makes the "--" and the "cnt" suffix readable
     * without the source: it says, on the instrument itself, whether the
     * trace is real and why the missing values are missing.
     */
    {
        const char *note = (have1 || have2) ? "cnt=ADC raw  --=no tb/cal"
                                            : "DEMO trace - no capture";
        lcd_fill_rect(x, y2, (uint16_t)(LCD_WIDTH - x), BADGE_H,
                      th->background);
        if (font_string_width(note, &font_small) > LCD_WIDTH - x)
            note = (have1 || have2) ? "--=no tb/cal" : "DEMO trace";
        font_draw_string(x, y2 + 1, note, na, th->background, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Quick-change popup overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_popup_box(const theme_t *th)
{
    /* Dark box with border */
    lcd_fill_rect(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, th->background);
    /* Top/bottom border */
    lcd_fill_rect(POPUP_X, POPUP_Y, POPUP_W, 1, th->highlight);
    lcd_fill_rect(POPUP_X, POPUP_Y + POPUP_H - 1, POPUP_W, 1, th->highlight);
    /* Left/right border */
    lcd_fill_rect(POPUP_X, POPUP_Y, 1, POPUP_H, th->highlight);
    lcd_fill_rect(POPUP_X + POPUP_W - 1, POPUP_Y, 1, POPUP_H, th->highlight);

    /* Centered text */
    font_draw_string_center(LCD_WIDTH / 2, POPUP_Y + 10, popup_text,
                            th->text_primary, th->background, &font_large);
    popup_painted = true;
}

static void draw_popup(const theme_t *th)
{
    if (popup_frames == 0) return;
    popup_frames--;
    draw_popup_box(th);
}

/*
 * Popup upkeep for the flicker-free path.
 *
 * Until 2026-08-19 an active popup set `q.force`, which pinned the renderer
 * to the full clear-then-redraw for the popup's whole ~500 ms life — so the
 * cost of showing a two-word box was ten full-screen blanks, and the box
 * itself strobed along with the trace. The popup was not flickering despite
 * the redraw; it was causing it.
 *
 * Now the compositor treats the popup rectangle as a hole (it is the one
 * layer it cannot recompute per pixel, because of the glyphs), paints the box
 * exactly once, and repaints the band underneath on the first frame after the
 * countdown expires. No full redraw is needed at either end.
 */
void scope_popup_overlay_tick(void)
{
    if (popup_frames == 0) return;
    if (!popup_painted)
        draw_popup_box(theme_get());
    popup_frames--;
}

/* ═══════════════════════════════════════════════════════════════════
 * Demo waveform (sine + square)
 * ═══════════════════════════════════════════════════════════════════ */

/* Render one channel's live ADC buffer into the vertical band [y_top, y_bot),
 * AUTO-FITTING the trace to that band from its own min/max (2026-08-14). This
 * makes any signal fill the screen centred, regardless of the frontend gain,
 * the DC offset (DAC1), or amplitude — no clipping, no manual centring. It is
 * a visualisation autoscale, NOT a volts reference; the Vpp/Vrms badges carry
 * the calibrated volts separately. Consecutive samples are connected with a
 * vertical span so the trace reads as a continuous line, not dots. */
/*
 * THE vertical transform. Both renderers go through this.
 *
 * They did not, until 2026-08-19. The full path autoscaled from the buffer's
 * own min/max while the compositor used a fixed (v-128)/256 about mid-screen,
 * over a shorter band, with the channel position offset applied in one and
 * ignored in the other. Same samples, two different places on the glass — so
 * the trace JUMPED whenever the renderer changed, which on the bench looked
 * like "the trace goes back to the bottom after the popup". It hid for a week
 * because a popup or a cursor forced the full path and everything else was
 * the demo waveform.
 *
 * Two copies of a transform that are supposed to agree will not stay agreeing.
 * One copy, two callers.
 */
typedef struct {
    bool    on;
    int16_t y_top, y_bot;
    int     h, span;
    uint8_t mn;
} autofit_t;

static void autofit_prep(autofit_t *a, const volatile uint8_t *buf,
                         int16_t y_top, int16_t y_bot)
{
    uint16_t n = (LCD_WIDTH < 512u) ? (uint16_t)LCD_WIDTH : 512u;
    uint8_t mn = 255, mx = 0;
    for (uint16_t x = 0; x < n; x++) {
        uint8_t sv = buf[x];
        if (sv < mn) mn = sv;
        if (sv > mx) mx = sv;
    }
    int span = (int)mx - (int)mn;
    if (span < 8) span = 8;                 /* don't zoom pure noise to full band */
    int h = (int)(y_bot - y_top) - 3;       /* usable height, small margin */
    if (h < 4) h = 4;

    a->on    = true;
    a->y_top = y_top;
    a->y_bot = y_bot;
    a->h     = h;
    a->span  = span;
    a->mn    = mn;
}

static inline int16_t autofit_y(const autofit_t *a, uint8_t v)
{
    int yy = (int)(a->y_bot - 2) - ((int)((int)v - a->mn) * a->h) / a->span;
    if (yy < a->y_top) yy = a->y_top;
    if (yy >= a->y_bot) yy = a->y_bot - 1;
    return (int16_t)yy;
}

static void draw_channel_autofit(const volatile uint8_t *buf, uint16_t color,
                                 int16_t y_top, int16_t y_bot)
{
    uint16_t n = (LCD_WIDTH < 512u) ? (uint16_t)LCD_WIDTH : 512u;
    autofit_t a;
    autofit_prep(&a, buf, y_top, y_bot);

    int16_t prev_y = -1;
    for (uint16_t x = 0; x < n; x++) {
        int16_t y = autofit_y(&a, buf[x]);
        if (prev_y >= 0) {                  /* connect prev..cur vertically */
            int16_t a = prev_y < y ? prev_y : y;
            int16_t b = prev_y < y ? y : prev_y;
            for (int16_t v = a; v <= b; v++)
                lcd_set_pixel(x, (uint16_t)v, color);
        } else {
            lcd_set_pixel(x, (uint16_t)y, color);
        }
        prev_y = y;
    }
}

/* Fixed-scale ("true volts/div") render — the graticule-honesty path (M3).
 *
 * Maps sample -> y at a CONSTANT 256/SCOPE_H counts per pixel, the exact
 * inverse of the geometry the grid and the status bar already assume:
 * SCOPE_CAL_COUNTS_PER_DIV counts per 26 px division (the _Static_assert below
 * ties the two together, so they cannot drift). Unlike autofit this does NOT
 * rescale to the band height, so one division on the glass is worth exactly the
 * volts/div scope_cal_volts_per_div() prints — that equality is the entire
 * point.
 *
 * The trace is positioned by the real ADC value about `center` (ADC mid-scale,
 * where `fpga scope center` parks a centred baseline). An uncentred DC offset
 * therefore shifts the whole trace and can push it past the band; that is the
 * honest behaviour of a real scope at a fixed volts/div, and it is why this
 * path is opt-in (scope_state.true_scale, default off) rather than the default.
 * In split (both-channel) mode the slope still uses the full SCOPE_H, so the
 * volts/div is identical to single-channel mode and a large signal clips at the
 * half-band edge exactly as it should. */
static void draw_channel_fixed(const volatile uint8_t *buf, uint16_t color,
                               int16_t y_top, int16_t y_bot, uint8_t center)
{
    uint16_t n = (LCD_WIDTH < 512u) ? (uint16_t)LCD_WIDTH : 512u;
    int16_t  y_mid = (int16_t)((y_top + y_bot) / 2);

    int16_t prev_y = -1;
    for (uint16_t x = 0; x < n; x++) {
        int yy = (int)y_mid - ((int)buf[x] - (int)center) * SCOPE_H / 256;
        if (yy < y_top)   yy = y_top;
        if (yy >= y_bot)  yy = y_bot - 1;
        int16_t y = (int16_t)yy;
        if (prev_y >= 0) {
            int16_t a = prev_y < y ? prev_y : y;
            int16_t b = prev_y < y ? y : prev_y;
            for (int16_t v = a; v <= b; v++)
                lcd_set_pixel(x, (uint16_t)v, color);
        } else {
            lcd_set_pixel(x, (uint16_t)y, color);
        }
        prev_y = y;
    }
}

void draw_demo_waveform(uint32_t frame)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();

    /* Freeze waveform when stopped — hold the last running frame */
    static uint32_t frozen_frame = 0;
    if (ss->running) {
        frozen_frame = frame;
    }
    uint32_t draw_frame = ss->running ? frame : frozen_frame;

    /*
     * If FPGA has delivered real ADC data, render from the sample buffers.
     * Each buffer holds 512 samples (normal mode) as unsigned 8-bit values
     * centered around 128 (after ADC offset calibration).
     * We map the 0-255 range into the waveform display area.
     */
    if (fpga_data_ready()) {
        /* Latch: once we've ever seen real ADC data, disable the
         * synthetic demo fallback permanently. */
        scope_mark_acquisition_ready();

        const volatile uint8_t *ch1_buf = fpga_get_ch1_buf();
        const volatile uint8_t *ch2_buf = fpga_get_ch2_buf();

        /* Re-triggering DISABLED — see note in demo waveform fallback.
         * TODO: re-enable once SPI3 protocol is confirmed working. */

        /* Auto-fit render. If both channels are live, split the scope area
         * into top (CH1) and bottom (CH2) bands so the traces don't overlap;
         * a single live channel gets the whole area. */
        bool c1 = ss->ch1.enabled && ch1_buf != NULL;
        bool c2 = ss->ch2.enabled && ch2_buf != NULL;

        /* True-scale only where the range has a real volts/div; a NONE range
         * always autofits (its grid has no volts meaning to honour). Default
         * ss->true_scale == false keeps every build on autofit unless toggled
         * from the shell. */
        bool fx1 = ss->true_scale && scope_cal_true_scale_ok(1u, ss->ch1.vdiv_idx);
        bool fx2 = ss->true_scale && scope_cal_true_scale_ok(2u, ss->ch2.vdiv_idx);

        if (c1 && c2) {
            if (fx1) draw_channel_fixed(ch1_buf, th->ch1, SCOPE_TOP, SCOPE_MID_Y - 1, 128u);
            else     draw_channel_autofit(ch1_buf, th->ch1, SCOPE_TOP, SCOPE_MID_Y - 1);
            if (fx2) draw_channel_fixed(ch2_buf, th->ch2, SCOPE_MID_Y + 1, SCOPE_BOT, 128u);
            else     draw_channel_autofit(ch2_buf, th->ch2, SCOPE_MID_Y + 1, SCOPE_BOT);
        } else if (c1) {
            if (fx1) draw_channel_fixed(ch1_buf, th->ch1, SCOPE_TOP, SCOPE_BOT, 128u);
            else     draw_channel_autofit(ch1_buf, th->ch1, SCOPE_TOP, SCOPE_BOT);
        } else if (c2) {
            if (fx2) draw_channel_fixed(ch2_buf, th->ch2, SCOPE_TOP, SCOPE_BOT, 128u);
            else     draw_channel_autofit(ch2_buf, th->ch2, SCOPE_TOP, SCOPE_BOT);
        }
        return;
    }

    /* ── Fallback: synthetic demo waveform (no FPGA data yet) ──────
     *
     * Only rendered if acquisition has NEVER produced real data. Once
     * the latch flips, we leave the grid empty instead — otherwise a
     * transient FPGA stall would flash fake waveforms on top of a
     * stopped trace, which is worse than an empty screen. */
    if (scope_acquisition_ready()) {
        return;
    }

    /* SPI3 acquisition triggers are now fired from main.c display loop
     * with 500ms warmup delay and early-abort safety. See main.c and
     * fpga_acquisition_task() for the crash-protection logic. */

    /* Mark the trace as synthetic ON the trace. The badge legend says the
     * same thing at the bottom of the screen, but the waveform is what
     * catches the eye and it is the thing that is not real. */
    font_draw_string(4, SCOPE_TOP + 14, "DEMO", th->warning, th->warning,
                     &font_small);

    static const int8_t sin_lut[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    /* CH1: sine wave — scale based on vdiv index */
    if (ss->ch1.enabled) {
        int16_t ch1_center = SCOPE_MID_Y - ss->ch1.position;
        int16_t amplitude = 40 - (int16_t)(ss->ch1.vdiv_idx * 2);
        if (amplitude < 10) amplitude = 10;

        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            uint8_t idx = (uint8_t)((x * 4 + draw_frame) & 0x3F);
            int16_t y = ch1_center - (sin_lut[idx] * amplitude / 100);
            if (y >= SCOPE_TOP && y < SCOPE_BOT) {
                lcd_set_pixel(x, (uint16_t)y, th->ch1);
                if (y + 1 < SCOPE_BOT)
                    lcd_set_pixel(x, (uint16_t)(y + 1), th->ch1);
            }
        }
    }

    /* CH2: square wave */
    if (ss->ch2.enabled) {
        int16_t ch2_center = SCOPE_MID_Y + 50 - ss->ch2.position;

        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            uint8_t phase = (uint8_t)((x * 4 + draw_frame) & 0x3F);
            int16_t y = ch2_center + (phase < 32 ? -25 : 25);
            if (y >= SCOPE_TOP && y < SCOPE_BOT)
                lcd_set_pixel(x, (uint16_t)y, th->ch2);

            /* Vertical edges */
            if (x > 0) {
                uint8_t prev = (uint8_t)(((x - 1) * 4 + draw_frame) & 0x3F);
                if ((prev < 32) != (phase < 32)) {
                    int16_t y1 = ch2_center - 25;
                    int16_t y2 = ch2_center + 25;
                    for (int16_t yy = y1; yy <= y2; yy++) {
                        if (yy >= SCOPE_TOP && yy < SCOPE_BOT)
                            lcd_set_pixel(x, (uint16_t)yy, th->ch2);
                    }
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Cursor measurement drawing
 * ═══════════════════════════════════════════════════════════════════ */

static void format_si(float val, const char *unit, char *buf, int bufsize)
{
    const char *prefix;
    float abs_val = val < 0.0f ? -val : val;
    int pos = 0;

    if (abs_val == 0.0f) {
        prefix = "";
    } else if (abs_val >= 1.0f) {
        prefix = "";
    } else if (abs_val >= 1.0e-3f) {
        val *= 1.0e3f;
        prefix = "m";
    } else if (abs_val >= 1.0e-6f) {
        val *= 1.0e6f;
        prefix = "u";
    } else {
        val *= 1.0e9f;
        prefix = "n";
    }

    if (val < 0.0f && pos < bufsize - 1) {
        buf[pos++] = '-';
        val = -val;
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 100.0f + 0.5f);
    if (frac >= 100) { integer++; frac -= 100; }

    if (integer >= 1000 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 1000) % 10);
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 100) % 10);
    if (integer >= 10 && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac / 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac % 10);

    while (*prefix && pos < bufsize - 1) buf[pos++] = *prefix++;
    while (*unit && pos < bufsize - 1) buf[pos++] = *unit++;
    buf[pos] = '\0';
}

static void draw_vline_dashed(uint16_t x, uint16_t y_top, uint16_t y_bot,
                              uint16_t color, bool is_active)
{
    for (uint16_t y = y_top; y <= y_bot; y++) {
        if (is_active || ((y & 7) < 4))
            lcd_set_pixel(x, y, color);
    }
}

static void draw_hline_dashed(uint16_t y, uint16_t x_left, uint16_t x_right,
                              uint16_t color, bool is_active)
{
    for (uint16_t x = x_left; x <= x_right; x++) {
        if (is_active || ((x & 7) < 4))
            lcd_set_pixel(x, y, color);
    }
}

static void draw_cursors(void)
{
    const scope_state_t *ss = scope_state_get();
    const cursor_state_t *c = &ss->cursor;
    const theme_t *th = theme_get();

    if (c->mode == CURSOR_OFF) return;

    uint16_t color_active   = th->highlight;
    uint16_t color_inactive = th->text_secondary;

    if (c->mode == CURSOR_VERTICAL || c->mode == CURSOR_BOTH) {
        bool v1_active = (c->active == CURSOR_SEL_V1);
        bool v2_active = (c->active == CURSOR_SEL_V2);

        draw_vline_dashed(c->v1_x, SCOPE_TOP, SCOPE_BOT,
                          v1_active ? color_active : color_inactive, v1_active);
        draw_vline_dashed(c->v2_x, SCOPE_TOP, SCOPE_BOT,
                          v2_active ? color_active : color_inactive, v2_active);

        font_draw_string(c->v1_x > 12 ? c->v1_x - 10 : 0, SCOPE_TOP,
                         "V1", v1_active ? color_active : color_inactive,
                         v1_active ? color_active : color_inactive, &font_small);
        font_draw_string(c->v2_x > 12 ? c->v2_x - 10 : 0, SCOPE_TOP,
                         "V2", v2_active ? color_active : color_inactive,
                         v2_active ? color_active : color_inactive, &font_small);
    }

    if (c->mode == CURSOR_HORIZONTAL || c->mode == CURSOR_BOTH) {
        bool h1_active = (c->active == CURSOR_SEL_H1);
        bool h2_active = (c->active == CURSOR_SEL_H2);

        draw_hline_dashed(c->h1_y, 0, LCD_WIDTH - 1,
                          h1_active ? color_active : color_inactive, h1_active);
        draw_hline_dashed(c->h2_y, 0, LCD_WIDTH - 1,
                          h2_active ? color_active : color_inactive, h2_active);

        font_draw_string_right(LCD_WIDTH - 1,
                               c->h1_y > SCOPE_TOP + 2 ? c->h1_y - 13 : c->h1_y + 2,
                               "H1", h1_active ? color_active : color_inactive,
                               h1_active ? color_active : color_inactive, &font_small);
        font_draw_string_right(LCD_WIDTH - 1,
                               c->h2_y > SCOPE_TOP + 2 ? c->h2_y - 13 : c->h2_y + 2,
                               "H2", h2_active ? color_active : color_inactive,
                               h2_active ? color_active : color_inactive, &font_small);
    }

    /*
     * Delta readout.
     *
     * Both cursor axes have an EXACT relationship to the capture, because
     * the waveform plot defines one:
     *
     *   horizontal — draw_demo_waveform()/draw_scope_live_frame() plot
     *                buf[x] at column x, so one screen column IS one sample.
     *                dx pixels = dx samples, exactly, no calibration.
     *   vertical   — the same plots use
     *                  y = SCOPE_MID_Y - (sample - 128) * SCOPE_H / 256,
     *                so dy pixels = dy * 256 / SCOPE_H ADC counts, exactly.
     *
     * Seconds and volts are a different matter: they need a sample rate and
     * a per-range gain, neither of which exists yet (dev plan §F2/§F4).
     * So the readouts are in samples and counts, and the s/V forms appear
     * only once cursor.time_per_pixel / volts_per_pixel are non-zero — which
     * scope_state.c documents as "unknown" and sets to 0 until a timebase
     * and a calibration are wired. That is the whole switch-over.
     */
    uint16_t badge_y = SCOPE_BOT - 28;
    uint16_t badge_x = 4;
    char buf[24];
    char label[40];

    if (c->mode == CURSOR_VERTICAL || c->mode == CURSOR_BOTH) {
        int16_t dx = (int16_t)c->v2_x - (int16_t)c->v1_x;
        int16_t adx = (dx < 0) ? (int16_t)-dx : dx;

        lcd_fill_rect(badge_x, badge_y, 100, 13, th->background);
        if (c->time_per_pixel > 0.0f) {
            float dt = (float)adx * c->time_per_pixel;
            format_si(dt, "s", buf, sizeof(buf));
            snprintf(label, sizeof(label), "dt=%s%s", dx < 0 ? "-" : "", buf);
        } else {
            snprintf(label, sizeof(label), "dt=%s%dsmp", dx < 0 ? "-" : "", adx);
        }
        font_draw_string(badge_x, badge_y, label,
                         th->highlight, th->highlight, &font_small);

        lcd_fill_rect(badge_x, badge_y + 14, 100, 13, th->background);
        if (c->time_per_pixel > 0.0f && adx != 0) {
            float freq = 1.0f / ((float)adx * c->time_per_pixel);
            format_si(freq, "Hz", buf, sizeof(buf));
            snprintf(label, sizeof(label), "1/dt=%s", buf);
        } else {
            /* No sample rate => no Hz. Saying "1/dt=--" beats printing a
             * number derived from a placeholder time base. */
            snprintf(label, sizeof(label), "1/dt=%s", MEAS_NA);
        }
        font_draw_string(badge_x, badge_y + 14, label,
                         th->highlight, th->highlight, &font_small);
    }

    if (c->mode == CURSOR_HORIZONTAL || c->mode == CURSOR_BOTH) {
        int16_t dy = (int16_t)c->h1_y - (int16_t)c->h2_y;
        int16_t ady = (dy < 0) ? (int16_t)-dy : dy;

        uint16_t vbadge_x = (c->mode == CURSOR_BOTH) ? 120 : badge_x;
        lcd_fill_rect(vbadge_x, badge_y, 100, 13, th->background);
        if (c->volts_per_pixel > 0.0f) {
            float dv = (float)ady * c->volts_per_pixel;
            format_si(dv, "V", buf, sizeof(buf));
            snprintf(label, sizeof(label), "dV=%s%s", dy < 0 ? "-" : "", buf);
        } else {
            /* Pixels -> counts is the plot's own transform, inverted. */
            unsigned counts = (unsigned)(((uint32_t)ady * 256u + SCOPE_H / 2u)
                                         / (uint32_t)SCOPE_H);
            snprintf(label, sizeof(label), "dV=%s%ucnt",
                     dy < 0 ? "-" : "", counts);
        }
        font_draw_string(vbadge_x, badge_y, label,
                         th->highlight, th->highlight, &font_small);
    }

    /* Cursor mode indicator */
    {
        const char *mode_str;
        switch (c->mode) {
        case CURSOR_VERTICAL:    mode_str = "CUR:T"; break;
        case CURSOR_HORIZONTAL:  mode_str = "CUR:V"; break;
        case CURSOR_BOTH:        mode_str = "CUR:TV"; break;
        default:                 mode_str = ""; break;
        }
        font_draw_string_right(LCD_WIDTH - 4, SCOPE_BOT - 13,
                               mode_str, th->highlight, th->highlight, &font_small);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Persistence overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_persistence_overlay(uint32_t frame)
{
    if (!persist_enabled) return;

    /* Feed demo waveform y-values into persistence buffer */
    static const int8_t sin_lut_p[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    uint16_t y_vals[PERSIST_WIDTH];
    uint16_t y_center = SCOPE_H / 2;
    for (uint16_t px = 0; px < PERSIST_WIDTH; px++) {
        uint8_t idx = (uint8_t)((px * 4 + frame) & 0x3F);
        int16_t yv = (int16_t)(y_center - (sin_lut_p[idx] * 40 / 100));
        if (yv < 0) yv = 0;
        if (yv >= PERSIST_HEIGHT) yv = PERSIST_HEIGHT - 1;
        y_vals[px] = (uint16_t)yv;
    }
    persist_add_trace(y_vals, 0);
    persist_decay();

    const uint8_t *buf = persist_get_buffer();
    if (!buf) return;

    for (uint16_t y = 0; y < PERSIST_HEIGHT; y++) {
        for (uint16_t x = 0; x < PERSIST_WIDTH; x++) {
            uint8_t intensity = buf[y * PERSIST_WIDTH + x];
            if (intensity > 0) {
                uint16_t color = persist_intensity_to_color_ch1(intensity);
                if (color != 0x0000)
                    lcd_set_pixel(x, y + SCOPE_TOP, color);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Math channel overlay
 * ═══════════════════════════════════════════════════════════════════ */

static void draw_math_waveform(uint32_t frame)
{
    if (!math_enabled) return;

    const theme_t *th = theme_get();

    static const int8_t sin_lut_m[64] = {
         0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
        100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
         0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
       -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
    };

    int16_t ch_a[LCD_WIDTH], ch_b[LCD_WIDTH], result[LCD_WIDTH];
    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
        ch_a[x] = (int16_t)(sin_lut_m[idx] * 40);
        uint8_t phase = (uint8_t)((x * 4 + frame) & 0x3F);
        ch_b[x] = (int16_t)(phase < 32 ? -2500 : 2500);
    }

    math_config_t cfg;
    cfg.operation = (math_op_t)math_op;
    cfg.scale = 1.0f;
    math_channel_compute(ch_a, ch_b, result, LCD_WIDTH, &cfg);

    uint16_t y_center = SCOPE_MID_Y;
    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        int16_t y = (int16_t)(y_center - result[x] / 100);
        if (y >= SCOPE_TOP && y < SCOPE_BOT)
            lcd_set_pixel(x, (uint16_t)y, th->warning);
    }

    const char *name = math_channel_name((math_op_t)math_op);
    font_draw_string(130, SCOPE_TOP, "MATH:", th->warning, th->warning, &font_small);
    font_draw_string(170, SCOPE_TOP, name, th->warning, th->warning, &font_small);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main scope screen compositor
 * ═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Debug Overlay
 * Shows raw acquisition status for diagnosing scope data pipeline.
 * Off by default. Enable with -DSCOPE_DEBUG_OVERLAY at build time.
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef SCOPE_DEBUG_OVERLAY

/* SCOPE_DBG_Y is defined with the layout constants at the top of this file,
 * because the badge rows must be positioned relative to it. */
#define SCOPE_DBG_H   58

/*
 * GPIO pin scanner — REMOVED 2026-08-13.
 *
 * It accumulated per-port "this pin has toggled since boot" masks
 * (gpio_toggle_a..e) every frame, and NOTHING read them: the overlay line
 * that printed them was replaced during the config-entry experiments, and
 * the two derived counters (pb3/pb4_toggle_count) were assigned and never
 * used, which is why they showed up as unused statics in the build. An
 * instrument with no readout is not an instrument; this file has enough
 * history of numbers that could not mean what they appeared to mean.
 *
 * It is in git if it is ever wanted again: `git log -S gpio_toggle_a --
 * firmware/src/ui/scope_ui.c` (added in 5b7437a, last read before 134fa83).
 * Note the in-context MISO finder it once carried was already deleted in
 * 2026-04-06 because it drove SPI3 from the display task and corrupted the
 * acquisition task's transfers.
 */

/* Set by draw_scope_screen (whose full-area clear wipes 180-224) so the strip
 * background is refilled exactly when needed; the live incremental path never
 * dirties it, so per-frame text updates go straight over their own opaque
 * cells with no blank state — that blank-then-text sequence at frame rate was
 * half of the visible full-screen flashing. */
static uint8_t scope_dbg_strip_dirty = 1;

/* Pad to a fixed width so opaque redraw erases a previously-longer line
 * (counters shrink after a reset; P0 flips H/L width-stable but SS does not). */
static void dbg_pad(char *s, unsigned cap, unsigned width)
{
    unsigned l = 0;
    while (s[l] != '\0') l++;
    while (l < width && l + 1 < cap) s[l++] = ' ';
    s[l] = '\0';
}

static void draw_scope_debug(const theme_t *th)
{
    /* Dark background strip */
#if (defined(FPGA_PIN_SWEEP_BUILD) && FPGA_PIN_SWEEP_BUILD) || \
    (defined(FPGA_CFG_TRACE_BUILD) && FPGA_CFG_TRACE_BUILD) || \
    (defined(FPGA_IDCODE_PROBE) && FPGA_IDCODE_PROBE)
    /* Variant overlays draw variable-length content — keep the full refill. */
    lcd_fill_rect(0, SCOPE_DBG_Y, LCD_WIDTH, SCOPE_DBG_H, 0x0000);
#else
    if (scope_dbg_strip_dirty) {
        lcd_fill_rect(0, SCOPE_DBG_Y, LCD_WIDTH, SCOPE_DBG_H, 0x0000);
        scope_dbg_strip_dirty = 0;
    }
#endif

    char buf[64];

#if defined(FPGA_PIN_SWEEP_BUILD) && FPGA_PIN_SWEEP_BUILD
    /* ── RECONFIG_N pin sweep results ───────────────────────────────────────
     * Press SAVE in scope mode to run it. Read HIT first: it is the only line
     * that can say "we found something".
     *   HIT:-- H:0 AF:0  -> every candidate pulsed, nothing moved the STATUS
     *                       register. No MCU-owned pin stock drives is RECONFIG_N.
     *   HIT:PCn          -> that pin changed the status after CONFIG_ENABLE.
     *   AF non-zero      -> that many candidates failed the IDCODE anchor and
     *                       were DISCARDED, not counted as hits. Worth a look
     *                       anyway: a pin that CLOSES the config port would also
     *                       land here (Exp L behaviour). */
    {
        const char *st = (fpga.sweep_state == 0) ? "IDLE" :
                         (fpga.sweep_state == 1) ? "RUN"  : "DONE";
        snprintf(buf, sizeof(buf), "SWEEP:%s %u/%u", st,
                 (unsigned)fpga.sweep_tested, (unsigned)fpga.sweep_total);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "BASE:%08lX",
                 (unsigned long)fpga.sweep_baseline);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        /* PH distinguishes two very different findings, which Exp O's single
         * snapshot could not tell apart:
         *   P = the PULSE ALONE moved the status — the pin triggered something
         *       on its own, which is what a real RECONFIG_N does (reload from NV).
         *   C = the status only moved after CONFIG_ENABLE — the pulse made 0x15
         *       land, i.e. it unlocked config entry. This is the jackpot. */
        const char *ph = (fpga.sweep_hit_phase == 1) ? "P" :
                         (fpga.sweep_hit_phase == 2) ? "C" : "-";
        snprintf(buf, sizeof(buf), "HIT:%s/%s %08lX",
                 fpga_sweep_pin_name(fpga.sweep_first_hit), ph,
                 (unsigned long)fpga.sweep_hit_status);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "H:%u AF:%u  press SAVE",
                 (unsigned)fpga.sweep_hits, (unsigned)fpga.sweep_anchor_fail);
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

#if defined(FPGA_CFG_TRACE_BUILD) && FPGA_CFG_TRACE_BUILD
    /* ── Step-resolved anchored status trace ────────────────────────────────
     * Six checkpoints through the config sequence, each anchored on the IDCODE.
     * Checked in this order before drawing any conclusion:
     *
     *   A: line     the anchor at each checkpoint. '0' = IDCODE found aligned,
     *               'x' = anchor FAILED and that Tn is NOT a measurement. An 'x'
     *               appearing partway through is itself the finding: the config
     *               port closed at that step (Exp L behaviour).
     *   T0..T5      anchor-corrected STATUS. FFFFFFFF means "not measured".
     *
     * All six identical => the part ignores every config command while still
     * answering reads. Any step where it MOVES is where the sequence first has
     * an effect, and is where to aim next. */
    {
        snprintf(buf, sizeof(buf), "T0:%08lX T1:%08lX",
                 (unsigned long)fpga.cfg_trace[0], (unsigned long)fpga.cfg_trace[1]);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "T2:%08lX T3:%08lX",
                 (unsigned long)fpga.cfg_trace[2], (unsigned long)fpga.cfg_trace[3]);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "T4:%08lX T5:%08lX",
                 (unsigned long)fpga.cfg_trace[4], (unsigned long)fpga.cfg_trace[5]);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        char a[FPGA_CFG_TRACE_N + 1];
        for (unsigned i = 0; i < FPGA_CFG_TRACE_N; i++) {
            int8_t o = fpga.cfg_trace_anchor[i];
            a[i] = (o < 0) ? 'x' : (char)('0' + (o % 10));
        }
        a[FPGA_CFG_TRACE_N] = '\0';
        /* MV = the count of checkpoints whose status differs from T0. 0 means the
         * register never moved anywhere in the sequence. */
        unsigned moved = 0;
        for (unsigned i = 1; i < FPGA_CFG_TRACE_N; i++)
            if (fpga.cfg_trace[i] != fpga.cfg_trace[0]) moved++;
        snprintf(buf, sizeof(buf), "A:%s MV:%u H2:%c", a, moved,
                 fpga.h2_upload_done ? 'Y' : 'N');
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

#if defined(FPGA_IDCODE_PROBE) && FPGA_IDCODE_PROBE
    /* ── Experiment J overlay: the anchored opcode-discrimination probe ──────
     * Replaces the normal overlay entirely for this build. Four opcodes read at
     * /256 on a pristine bus before the prelude, 8 bytes each.
     *
     * READ IT LIKE THIS:
     *   ID / NP identical, and each showing a repeated 4-byte group
     *     => the FPGA free-runs a fixed pattern and ignores MOSI. Confirms the
     *        2026-06-13 conclusion on a valid measurement for the first time;
     *        SSPI cannot reach the config engine and JTAG is the route.
     *   ID differs from NP, ID@ >= 0
     *     => the FPGA DOES decode SSPI opcodes. The "not in config-receive mode"
     *        conclusion collapses and the config-entry search reopens.
     *   ID@ >= 0 but not 0
     *     => IDCODE present but phase-shifted by that many bits; the reply is
     *        real and our sampling alignment is off (see Exp I).
     */
    {
        const volatile uint8_t *p;

        p = fpga.probe_idcode;
        snprintf(buf, sizeof(buf), "ID:%02X%02X%02X%02X %02X%02X%02X%02X",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        font_draw_string(2, SCOPE_DBG_Y + 2, buf, 0x07E0, 0x0000, &font_small);

        p = fpga.probe_noop;
        snprintf(buf, sizeof(buf), "NP:%02X%02X%02X%02X %02X%02X%02X%02X",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        font_draw_string(2, SCOPE_DBG_Y + 15, buf, 0x07FF, 0x0000, &font_small);

        snprintf(buf, sizeof(buf), "ST:%02X%02X%02X%02X US:%02X%02X%02X%02X",
                 fpga.probe_status[0], fpga.probe_status[1],
                 fpga.probe_status[2], fpga.probe_status[3],
                 fpga.probe_user[0], fpga.probe_user[1],
                 fpga.probe_user[2], fpga.probe_user[3]);
        font_draw_string(2, SCOPE_DBG_Y + 28, buf, 0xFFE0, 0x0000, &font_small);

        /* ID@ = bit offset where 0x0120681B was found, -1 = absent.
         * SM  = 0x11 and 0x00 returned identical bytes (Y => no opcode decode).
         * RP  = the 0x11 reply repeats every 4 bytes (Y => free-running pattern).
         * PO@ = same IDCODE search on the 0x11 read taken AFTER CONFIG_ENABLE.
         * CL@ = same again AFTER the full upload and the 0x3A close. Per Exp L a
         *       configured part stops answering 0x11, so CL@ >= 0 means the config
         *       port never closed => we are definitively NOT configured. */
        snprintf(buf, sizeof(buf), "ID@%d SM:%c RP:%c PO@%d CL@%d",
                 (int)fpga.probe_id_bit,
                 fpga.probe_all_same ? 'Y' : 'N',
                 fpga.probe_repeats  ? 'Y' : 'N',
                 (int)fpga.probe_id_bit_post,
                 (int)fpga.probe_id_bit_close);
        font_draw_string(2, SCOPE_DBG_Y + 41, buf, 0xF81F, 0x0000, &font_small);
    }
    return;
#endif

    /* Line 1 (green): transport counters from the real FPGA path */
    /* Line 1 (green): post-upload scope-engine config readback.
     * SS = fpga.scope_status[] from the 0x03 status read — stock capture
     * returns 00 01 42 2E. CL = 0x3A close status — stock returns F8.
     * R  = first 4 raw CH1 bytes (pre-cal), to see if samples vary at all. */
    snprintf(buf, sizeof(buf), "SS:%02X%02X%02X%02X CL:%02X R:%02X%02X%02X%02X",
             fpga.scope_status[0], fpga.scope_status[1],
             fpga.scope_status[2], fpga.scope_status[3],
             fpga.h2_close_status,
             fpga.diag_ch1_raw[0], fpga.diag_ch1_raw[1],
             fpga.diag_ch1_raw[2], fpga.diag_ch1_raw[3]);
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 2, buf,
                     0x07E0, 0x0000, &font_small);  /* green */

    /* Line 2 (cyan): acquisition state and critical control pins */
    snprintf(buf, sizeof(buf), "OK:%u TO:%u 1:%02X P0:%c P6:%c B:%c",
             (unsigned)fpga.spi3_ok_count,
             (unsigned)fpga.spi3_total_timeouts,
             fpga.spi3_first_byte,
             (GPIOC->idt & (1 << 0)) ? 'H' : 'L',
             (GPIOC->odt & (1 << 6)) ? 'H' : 'L',
             (GPIOB->odt & (1 << 11)) ? 'H' : 'L');
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 15, buf,
                     0x07FF, 0x0000, &font_small);  /* cyan */

    /* Line 3 (yellow): Init handshake responses + raw PB4 state
     * G1[0-3] = sync+0x05+pad, G2[4-6] = 0x12+pad, G3[7-10] = 0x15+pad+0x3B
     * PB4: raw GPIO read of the MISO pin (1=HIGH/floating, 0=driven LOW) */
    /* L/H = min/max across the whole CH1 acquisition buffer. If L==H the
     * samples are a stuck constant (analog/trigger problem); if they spread,
     * real signal is arriving and the fault is downstream in rendering. */
    {
        const volatile uint8_t *cb = fpga_get_ch1_buf();
        uint8_t lo = 0xFF, hi = 0x00;
        for (uint32_t i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
            uint8_t v = cb[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        /* CFG = Gowin STATUS_REGISTER (opcode 0x41) read right after the 0x3A
         * close — the authoritative "did the upload take?" verdict. Read at a
         * forced /256 since 2026-07-28; every earlier value was clocked at /2
         * and slipped one bit (the persistent 80 01 C8 10 is 0x00039020 read
         * one bit early — see fpga.c [6b]).
         *
         * D = DONE_FINAL (bit 13) — 1 means configuration completed. This, not
         *     SYSTEM_EDIT_MODE, is the success signal (Exp H showed stock reads
         *     the same 0x00039020 at the config-enable instant that we do, so
         *     ED cannot discriminate).
         * E = the error nibble, bits 3..0 = TIMEOUT | ID_VERIFY_FAILED |
         *     BAD_COMMAND | CRC_ERROR. Non-zero means bytes DID reach the config
         *     engine and were rejected → a content/transport bug, localizable.
         *     Zero with D=0 means the engine never received anything at all. */
        uint32_t sr = ((uint32_t)fpga.cfg_status_reg[0] << 24) |
                      ((uint32_t)fpga.cfg_status_reg[1] << 16) |
                      ((uint32_t)fpga.cfg_status_reg[2] << 8)  |
                      ((uint32_t)fpga.cfg_status_reg[3]);
        /* A = the post-0x3A IDCODE anchor: the bit offset at which 0x0120681B was
         *     found in the read taken immediately before CFG, at the same clock.
         *     A>=0  -> CFG was read on a validated path AND the config port is
         *             still open, i.e. we are definitively NOT configured.
         *     A=-1  -> either the port closed (Exp L: what a successfully
         *             configured part does) or the bus is dead. CFG is then
         *             UNTRUSTWORTHY on its own — corroborate with a live trace. */
        snprintf(buf, sizeof(buf), "CFG:%08lX D%u E%X A%d L%u H%u",
                 (unsigned long)sr,
                 (unsigned)((sr >> 13) & 1u),
                 (unsigned)(sr & 0x0Fu),
                 (int)fpga.probe_id_bit_close,
                 (unsigned)lo, (unsigned)hi);
    }
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 28, buf,
                     0xFFE0, 0x0000, &font_small);  /* yellow */

    /* Line 4 (magenta): SPI3 CTRL1 + the EDIT_MODE probe + H2 upload flag.
     * S1 = SPI3 CTRL1 as latched during the config frame. Bits[5:3] are BR, so
     *      0347 = /2 (stock, Exp F fidelity build) and 037F = /256. This is the
     *      on-device proof of which clock the config frame actually ran at.
     * ED = STATUS_REGISTER (0x41) read at /256 immediately after 0x15.
     *      SYSTEM_EDIT_MODE is bit 7 of the ASSEMBLED 32-bit word, i.e. bit 7 of
     *      ED[3] — NOT bit 7 of ED[0], as this comment and scripts/swd_fpga_status.sh
     *      both said until 2026-07-28. (Same verdict either way for 0x00039020:
     *      ED[3]=0x20, bit 7 clear. But the test was reading bit 31.)
     *      Exp H demoted this from "the wall" to a non-discriminator: stock reads
     *      the same 0x00039020 here and still configures. Compare CFG's D flag.
     * Dropped S2/ST here — CTRL2 and STS were static (0003 / 0002) across every
     * run so far, and ED is the number this experiment turns on.
     * H2T means bytes streamed, not recovered FPGA acceptance. */
    snprintf(buf, sizeof(buf), "S1:%04X ED:%02X%02X%02X%02X H2T:%c",
             (uint16_t)fpga.diag_spi_ctrl1,
             fpga.edit_mode_status[0], fpga.edit_mode_status[1],
             fpga.edit_mode_status[2], fpga.edit_mode_status[3],
             fpga.h2_upload_done ? 'Y' : 'N');
    dbg_pad(buf, sizeof(buf), 38);
    font_draw_string(2, SCOPE_DBG_Y + 41, buf,
                     0xF81F, 0x0000, &font_small);  /* magenta */
}

#endif /* SCOPE_DEBUG_OVERLAY */

/* ═══════════════════════════════════════════════════════════════════
 * X-Y (Lissajous) view — CH1 drives X, CH2 drives Y
 *
 * Plots the two ADC buffers against each other instead of against time:
 * op04 (CH1) -> horizontal, op05 (CH2) -> vertical. The buffers come from
 * one synchronised acquisition and were bench-confirmed time-aligned
 * (~20 deg phase spread, EXP-21 2026-08-21), so (ch1[i], ch2[i]) is a
 * single instant and a Lissajous figure renders coherently.
 *
 * CH2's readout is a dead buffer until its vertical-offset reference
 * (TMR13 CH1 PWM-DAC on PA6) is armed — the same thing `trig2 raw` does
 * from the shell — so this view arms it once on first entry. Square plot
 * area (side = SCOPE_H) so frequency ratios are not distorted.
 *
 * Reads the volatile buffers directly (no tear-check copy): a frame the
 * acq task is mid-updating just displaces a few points, cosmetically
 * negligible here. Consecutive samples are connected so the locus reads as
 * a continuous trace, the way an analog scope draws X-Y.
 * ═══════════════════════════════════════════════════════════════════ */

/* Bresenham segment written into the persistence buffer (buffer coords). */
static void xy_line_persist(int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = (dx > dy ? dx : -dy) / 2, e2;
    for (;;) {
        persist_add_point((uint16_t)x0, (uint16_t)y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

void draw_xy_screen(void)
{
    const theme_t *th = theme_get();
    const uint16_t side = SCOPE_H;                 /* square side, 206 px */
    const uint16_t x0   = (LCD_WIDTH - side) / 2;  /* centred: 57 */
    const uint16_t y0   = SCOPE_TOP;               /* 18 */
    const uint16_t cx   = x0 + side / 2;
    const uint16_t cy   = y0 + side / 2;

    /* One-time bring-up: offset DACs (CH1 DAC1 + CH2 TMR13/PA6) and the shared
     * persistence buffer, borrowed here for the phosphor trail (X-Y and the
     * time-domain persistence overlay never run at once). */
    static bool xy_init = false;
    static int  xy_dac1 = 2048, xy_dac2 = 2048;
    if (!xy_init) {
        scope_trigger_dac_init();
        scope_trigger_ch2_init();
        scope_trigger_dac_raw((uint16_t)xy_dac1);
        scope_trigger_ch2_raw((uint16_t)xy_dac2);
        if (!persist_is_initialized()) persist_init();
        persist_set_mode(PERSIST_LOW);             /* short phosphor tail */
        persist_clear();
        xy_init = true;
    }

    /* Margins + title: background repaint of the strips either side of the
     * square (also wipes a prior view on entry). The square is streamed from
     * the persistence buffer below and never blanks, so there is no flash. */
    lcd_fill_rect(0, SCOPE_TOP, x0, SCOPE_H, th->background);
    lcd_fill_rect((uint16_t)(x0 + side), SCOPE_TOP,
                  (uint16_t)(LCD_WIDTH - (x0 + side)), SCOPE_H, th->background);
    font_draw_string(4, SCOPE_TOP + 2, "X-Y", th->text_secondary,
                     th->background, &font_small);

    const volatile uint8_t *xb = fpga_get_ch1_buf();
    const volatile uint8_t *yb = fpga_get_ch2_buf();
    const bool have = fpga_data_ready() && xb != NULL && yb != NULL
                      && persist_is_initialized();

    persist_decay();                     /* fade the whole buffer once per frame */

    if (have) {
        /* Coherent snapshot (odd generation = acq commit in progress). */
        static uint8_t xs[FPGA_ADC_BUF_SIZE];
        static uint8_t ys[FPGA_ADC_BUF_SIZE];
        for (int t = 0; t < 4; t++) {
            uint32_t g0 = fpga_acq_frame_generation();
            memcpy(xs, (const void *)xb, FPGA_ADC_BUF_SIZE);
            memcpy(ys, (const void *)yb, FPGA_ADC_BUF_SIZE);
            if ((g0 & 1u) == 0u && fpga_acq_frame_generation() == g0)
                break;
        }

        uint8_t xmin = 255, xmax = 0, ymin = 255, ymax = 0;
        for (uint16_t i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
            if (xs[i] < xmin) xmin = xs[i];
            if (xs[i] > xmax) xmax = xs[i];
            if (ys[i] < ymin) ymin = ys[i];
            if (ys[i] > ymax) ymax = ys[i];
        }
        const int xmid_raw = (xmin + xmax) / 2;
        const int ymid_raw = (ymin + ymax) / 2;
        int rng_raw = (xmax - xmin) > (ymax - ymin) ? (xmax - xmin) : (ymax - ymin);
        if (rng_raw < 4) rng_raw = 4;

        /* Smoothed auto-scale: rounded EMA (alpha 1/8) of centre and span. */
        static int xmid = 128, ymid = 128, rng = 64;
        static bool seeded = false;
        if (!seeded) { xmid = xmid_raw; ymid = ymid_raw; rng = rng_raw; seeded = true; }
        else {
            xmid += ((xmid_raw - xmid) + (xmid_raw >= xmid ? 4 : -4)) / 8;
            ymid += ((ymid_raw - ymid) + (ymid_raw >= ymid ? 4 : -4)) / 8;
            rng  += ((rng_raw  - rng ) + (rng_raw  >= rng  ? 4 : -4)) / 8;
        }
        if (rng < 4) rng = 4;

        /* Auto-centre servo on the offset DACs (baseline -> ADC mid-scale). */
        int e1 = 128 - xmid_raw, e2 = 128 - ymid_raw;
        if (e1 > 4 || e1 < -4) {
            xy_dac1 += e1 * 5;
            if (xy_dac1 < 0) xy_dac1 = 0; else if (xy_dac1 > 4095) xy_dac1 = 4095;
            scope_trigger_dac_raw((uint16_t)xy_dac1);
        }
        if (e2 > 4 || e2 < -4) {
            xy_dac2 += e2 * 5;
            if (xy_dac2 < 0) xy_dac2 = 0; else if (xy_dac2 > 4095) xy_dac2 = 4095;
            scope_trigger_ch2_raw((uint16_t)xy_dac2);
        }

        /* Plot into the persistence buffer, buffer coords (x 0..319,
         * y 0..PERSIST_HEIGHT-1 = screen y - SCOPE_TOP). Y inverted. */
        const int half  = side / 2 - 6;
        const int chord = side / 3;
        const int xl = (int)x0, xh = (int)x0 + (int)side - 1;
        const int yl = 0,       yh = (int)side - 1;
        const int cyb = (int)cy - (int)SCOPE_TOP;
        int pbx = 0, pby = 0;
        for (uint16_t i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
            int bx = (int)cx + ((int)xs[i] - xmid) * half * 2 / rng;
            int by = cyb - ((int)ys[i] - ymid) * half * 2 / rng;
            if (bx < xl) bx = xl; else if (bx > xh) bx = xh;
            if (by < yl) by = yl; else if (by > yh) by = yh;
            if (i > 0) {
                int adx = bx > pbx ? bx - pbx : pbx - bx;
                int ady = by > pby ? by - pby : pby - by;
                if (adx < chord && ady < chord)
                    xy_line_persist(pbx, pby, bx, by);
                else
                    persist_add_point((uint16_t)bx, (uint16_t)by);
            } else {
                persist_add_point((uint16_t)bx, (uint16_t)by);
            }
            pbx = bx; pby = by;
        }
    }

    /* Stream the square from the persistence buffer, compositing the centre
     * axes. Empty pixels -> background, so faded points erase cleanly and the
     * square is repainted with no blank phase (no flash). */
    const uint8_t *pb = persist_get_buffer();
    uint16_t xy_row[SCOPE_H];            /* stack, not BSS — one row of the square */
    const int cxr = (int)cx - (int)x0;      /* centre col within the square */
    const int cyr = (int)cy - (int)y0;      /* centre row within the square */
    for (int ry = 0; ry < (int)side; ry++) {
        for (int rx = 0; rx < (int)side; rx++) {
            uint8_t inten = pb ? pb[ry * PERSIST_WIDTH + (int)x0 + rx] : 0;
            uint16_t c;
            if (inten)
                c = persist_intensity_to_color_ch1(inten);
            else if (rx == cxr || ry == cyr)
                c = th->grid;
            else
                c = th->background;
            xy_row[rx] = c;
        }
        lcd_set_window(x0, (uint16_t)(y0 + ry), side, 1);
        lcd_write_pixels(xy_row, side);
    }

    if (!have)
        font_draw_string_center(LCD_WIDTH / 2, SCOPE_MID_Y, "X-Y: no signal",
                                th->warning, th->background, &font_small);
}

void draw_scope_screen(uint32_t frame)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();

    /* Clear waveform area */
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, th->background);

    /* Layer 1: Persistence overlay (under everything) */
    draw_persistence_overlay(frame);

    /* Layer 2: Grid */
    draw_scope_grid();

    /* Layer 3: Ground reference markers */
    draw_ground_markers(ss, th);

    /* Layer 4: Trigger level indicator */
    draw_trigger_indicator(ss, th);

    /* Layer 5: Waveform */
    draw_demo_waveform(frame);

    /* Layer 6: Math channel overlay */
    draw_math_waveform(frame);

    /* Layer 7: Cursor lines */
    draw_cursors();

    /* Layer 8: Trigger status badge */
    draw_trigger_status(ss, th);

    /* Layer 9: Run/Stop */
    draw_run_stop(ss, th);

    /* Layer 10: Measurement badges */
    draw_measurement_badges(ss, th);

    /* Layer 11: Quick-change popup (on top of everything) */
    draw_popup(th);

    /* Active channel indicator (top-left) */
    const char *ch_label = (active_channel == 0) ? "CH1" : "CH2";
    uint16_t ch_color = (active_channel == 0) ? th->ch1 : th->ch2;
    font_draw_string(4, SCOPE_TOP + 2, ch_label, ch_color, ch_color, &font_small);

#ifndef EMULATOR_BUILD
    /* SPI3 acquisition diagnostic — small text overlay top-center.
     * Shows probing status so we can see FPGA data flow without serial. */
    {
        char spi3_buf[24];
        uint16_t spi3_color;
        if (fpga.spi3_ok_count > 0) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:OK %u",
                     (unsigned)fpga.spi3_ok_count);
            spi3_color = th->success;
        } else if (fpga.spi3_total_timeouts > 0) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:-- %u [%02X]",
                     (unsigned)fpga.spi3_total_timeouts,
                     fpga.spi3_first_byte);
            spi3_color = th->warning;
        } else if (fpga.initialized) {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:wait");
            spi3_color = th->text_secondary;
        } else {
            snprintf(spi3_buf, sizeof(spi3_buf), "SPI3:off");
            spi3_color = th->text_secondary;
        }
        font_draw_string(40, SCOPE_TOP + 2, spi3_buf,
                         spi3_color, th->background, &font_small);
    }
#endif

#ifdef SCOPE_DEBUG_OVERLAY
    /* Layer 12: SPI3 debug overlay (bottom of screen) */
    scope_dbg_strip_dirty = 1;   /* the :1050-style full clear wiped 180-224 */
    draw_scope_debug(th);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * Live incremental frame (2026-08-12 rendering pass)
 *
 * Flicker-free trace update for the real-data path: repaints the waveform
 * band column-by-column, compositing each pixel's FINAL color (trace over
 * trigger line over crosshair over grid over background) into one streamed
 * window write per column — the screen never holds a blank state, unlike
 * draw_scope_screen's clear-then-redraw, which is the visible flash. Grid
 * pixels are re-plotted arithmetically (there is no framebuffer to restore
 * from; pattern mirrors draw_scope_grid exactly). Cost: one lcd_set_window
 * (~13 us) per column + streamed pixel writes ≈ 6-8 ms per frame — 20 fps
 * capable, vs the full path's blank-flash at any rate.
 *
 * Decorations (badges, ground markers, cursors, popups, trigger arrow) are
 * NOT drawn here. main.c falls back to draw_scope_screen whenever a popup
 * is active or cursors are on, and button DCMDs still force full redraws,
 * so those layers refresh exactly when they can change.
 * ═══════════════════════════════════════════════════════════════════ */
void draw_scope_live_frame(void)
{
    const theme_t *th = theme_get();
    const scope_state_t *ss = scope_state_get();
    const volatile uint8_t *b1 = fpga_get_ch1_buf();
    const volatile uint8_t *b2 = fpga_get_ch2_buf();

    if (!b1 || !b2) return;

    /* The live band stops where fixed furniture begins: the debug strip
     * (debug builds) or the measurement badge rows. Those regions repaint
     * themselves; streaming over them would z-fight at frame rate. */
    /* Always the badge rows: they are the topmost fixed furniture in BOTH
     * configurations. In overlay builds the badges now sit ABOVE the debug
     * strip (see BADGE_ROW_Y), so stopping at SCOPE_DBG_Y — as this did until
     * 2026-08-14 — streamed the waveform straight over them at frame rate.
     * That was the second occlusion bug in a row here: first the debug strip
     * painted over the badges, then moving the badges up put them inside the
     * live band. Anchor to the badges and both cases are covered. */
    const uint16_t band_bot = BADGE_ROW2_Y;
    const uint16_t band_h = band_bot - SCOPE_TOP;
    (void)band_h;

    /* Trigger dotted line, exactly as draw_trigger_indicator places it. */
    int16_t trig_y = SCOPE_MID_Y - ss->trigger.level;
    if (trig_y < SCOPE_TOP + 2) trig_y = SCOPE_TOP + 2;
    if (trig_y > SCOPE_BOT - 3) trig_y = SCOPE_BOT - 3;

    /* Autofit exactly as the full path does, including its band split, so a
     * change of renderer never moves the trace. The transform runs against
     * SCOPE_BOT even though this function only PAINTS down to band_bot: the
     * full path draws the trace over that strip and then paints the badges on
     * top, so matching its geometry means matching its y_bot, not its visible
     * extent.
     *
     * The channel position offset is deliberately NOT applied. The full path
     * ignores it, and against an autoscaled trace it has no meaning — keeping
     * it here is what let the two paths disagree in the first place. */
    const bool en1 = ss->ch1.enabled;
    const bool en2 = ss->ch2.enabled;

    autofit_t a1 = { false, 0, 0, 0, 8, 0 };
    autofit_t a2 = { false, 0, 0, 0, 8, 0 };
    if (en1 && en2) {
        autofit_prep(&a1, b1, SCOPE_TOP, SCOPE_MID_Y - 1);
        autofit_prep(&a2, b2, SCOPE_MID_Y + 1, SCOPE_BOT);
    } else if (en1) {
        autofit_prep(&a1, b1, SCOPE_TOP, SCOPE_BOT);
    } else if (en2) {
        autofit_prep(&a2, b2, SCOPE_TOP, SCOPE_BOT);
    }

    int16_t p1 = 0, p2 = 0;  /* previous column's y — vertical continuity */

    /* Popup hole, clamped into the band so a future move of either rectangle
     * cannot produce an inverted or out-of-band window. */
    bool     hole     = (popup_frames > 0);
    uint16_t hole_top = POPUP_Y;
    uint16_t hole_bot = POPUP_Y + POPUP_H;
    if (hole_top < SCOPE_TOP) hole_top = SCOPE_TOP;
    if (hole_bot > band_bot)  hole_bot = band_bot;
    if (hole_top >= hole_bot) hole = false;

    for (uint16_t x = 0; x < LCD_WIDTH; x++) {
        /* One transform, shared with the full path — see autofit_prep. */
        int16_t y1 = a1.on ? autofit_y(&a1, b1[x]) : (int16_t)SCOPE_MID_Y;
        int16_t y2 = a2.on ? autofit_y(&a2, b2[x]) : (int16_t)SCOPE_MID_Y;

        /* 2-px dot plus a span to the previous sample, so steep edges draw
         * as connected verticals instead of the full path's dotted gaps. */
        int16_t lo1 = y1, hi1 = y1 + 1, lo2 = y2, hi2 = y2 + 1;
        if (x > 0) {
            if (p1 < lo1) lo1 = p1;
            if (p1 > hi1) hi1 = p1;
            if (p2 < lo2) lo2 = p2;
            if (p2 > hi2) hi2 = p2;
        }
        p1 = y1;
        p2 = y2;

        /* The popup is a HOLE in the live band, not a layer: its glyphs are
         * the one thing this compositor cannot recompute per pixel, so the
         * column is emitted as up to two segments that step around the box.
         * When the countdown ends the hole closes and the very next live
         * frame paints the band back over it — no full repaint at either
         * end, which is the whole point of the change. */
        uint16_t seg_top[2], seg_bot[2];
        uint8_t  nseg = 1;
        seg_top[0] = SCOPE_TOP;
        seg_bot[0] = band_bot;
        if (hole && x >= POPUP_X && x < POPUP_X + POPUP_W) {
            seg_bot[0] = hole_top;
            seg_top[1] = hole_bot;
            seg_bot[1] = band_bot;
            nseg = 2;
        }

        for (uint8_t sg = 0; sg < nseg; sg++) {
        if (seg_bot[sg] <= seg_top[sg]) continue;
        lcd_set_window(x, seg_top[sg], 1, (uint16_t)(seg_bot[sg] - seg_top[sg]));
        for (uint16_t y = seg_top[sg]; y < seg_bot[sg]; y++) {
            uint16_t c;
            /* Z-order matches the full path: CH2 painted after CH1 there,
             * so CH2 wins here; trace over trigger line over grid. */
            if (a2.on && (int16_t)y >= lo2 && (int16_t)y <= hi2)
                c = th->ch2;
            else if (a1.on && (int16_t)y >= lo1 && (int16_t)y <= hi1)
                c = th->ch1;
            else if ((int16_t)y == trig_y && (x & 3u) == 0 && x < LCD_WIDTH - 6)
                c = th->trigger;
            else if (y == SCOPE_MID_Y || x == LCD_WIDTH / 2)
                c = th->grid_center;
            else if (((x & 31u) == 0 && (y & 1u) == 0) ||
                     (((y - SCOPE_TOP) % 26u) == 0 && (x & 1u) == 0))
                c = th->grid;
            else
                c = th->background;
            lcd_write_data(c);
        }
        }
    }

    /* ⚠ Measurement badges are NOT refreshed here — REVERTED 2026-08-14.
     * DO NOT REINSTATE WITHOUT READING THIS.
     *
     * Calling draw_measurement_badges() from this function KILLED ACQUISITION
     * on the bench: OK: reached exactly 20 and stopped, TO: climbed forever,
     * and it survived two full FPGA power cycles and a pinhole reset.
     * Reproduced on BOTH guest-persist and guest-coldtrace, so it was not the
     * settings-persistence work. Before the edit, OK: ran past 191,000.
     *
     * Why: the measurement passes read 1024 samples from BOTH channel buffers,
     * three times each, from the DISPLAY task — while the acquisition task is
     * writing those same buffers over SPI3.
     *
     * The badges do need refreshing (they otherwise latch "--" from scope
     * entry, where fpga.c has just cleared data_ready, and B2's redraw gating
     * means no further full repaint is triggered). But the fix belongs on the
     * other side: give the redraw epoch a measurement-generation term so a
     * full repaint happens when new samples land, instead of doing 6 KB of
     * analysis inline on the capture-rate path.
     *
     * Cost of leaving it out: stale badges — a display bug. The version that
     * was here was a data-loss bug. Prefer the display bug. */

#ifdef SCOPE_DEBUG_OVERLAY
    /* Keep the counters moving; opaque fixed-width lines, no strip refill. */
    draw_scope_debug(th);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * FFT views (unchanged — reference scope_state for consistency)
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef FEATURE_FFT

static void format_freq(float freq_hz, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (freq_hz >= 1000000.0f) {
        val = freq_hz / 1000000.0f;
        unit = "MHz";
    } else if (freq_hz >= 1000.0f) {
        val = freq_hz / 1000.0f;
        unit = "kHz";
    } else {
        val = freq_hz;
        unit = "Hz";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 10.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + integer / 100);
    if (integer >= 10  && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac);

    while (*unit && pos < bufsize - 1)
        buf[pos++] = *unit++;
    buf[pos] = '\0';
}

/* ── Display reference auto-ranging ─────────────────────────────────
 *
 * fft_process() reports magnitude in dB relative to ONE ADC COUNT
 * (20*log10(|X| * 2/N), |X| in counts) — not dBFS. A full-scale signal
 * therefore peaks near 20*log10(32767) = 90 dB, and the configured default
 * ref_level_db = 0.0 puts the top of the display ~90 dB BELOW the signal:
 * every populated bin clamps to normalized = 0, i.e. the top colour of the
 * ramp. Measured on the host with the same synthetic 1 kHz square the UI
 * feeds itself (scratch probe, 2026-08-13): peak = 84.4 dB, and 318 of the
 * 320 waterfall columns land in the RED band. That is the "hot red field
 * with thin nulls" seen on the bench the same day.
 *
 * Fix: track the observed peak instead of hard-coding a magic 90. The
 * reference follows frame_max + headroom, moving up immediately (so a
 * growing signal never clips against the top) and easing down slowly
 * through a dead band (so a jittering peak does not make the colours
 * breathe). cfg->ref_level_db is preserved as a USER TRIM applied on top,
 * which keeps fft_adjust_ref_level() meaningful.
 *
 * Known limit, stated rather than hidden: this ranges off the peak only. A
 * spectrum with no dynamic range (broadband noise and nothing else) has its
 * mean within ~10-15 dB of its peak and will still render as a flat, fairly
 * hot field. That is a true statement about such a signal; distinguishing it
 * would need a floor estimator as well, which is not warranted until real
 * captured data (rather than the synthetic square below) drives this view.
 */
#define FFT_REF_HEADROOM_DB   6.0f   /* keep the peak just below the top    */
#define FFT_REF_DEAD_BAND_DB  3.0f   /* ignore downward wobble below this   */
#define FFT_REF_DECAY         0.10f  /* per-frame fraction on the way down  */

static float fft_ref_tracked = 0.0f;
static bool  fft_ref_primed  = false;

/* ── Where the spectrum views get their samples ──────────────────────
 *
 * Until 2026-08-13 every FFT view — spectrum, split and waterfall — called
 * test_signal_generate() unconditionally and analysed a synthetic 1 kHz
 * square. Cold-boot capture started working that same day and none of them
 * lit up, because none of them was ever connected to the ADC. Worse, the
 * views did not say so: a user could not tell the spectrum of their probe
 * from the spectrum of a constant compiled into the firmware.
 *
 * Now: the live CH1 record if there is one, the test signal otherwise, and
 * the view is LABELLED with which it got. Same latch discipline as the
 * trace (scope_ui.c fpga_data_ready() gate).
 *
 * Two honest limits, both visible in the labelling rather than hidden:
 *
 *  1. THE FREQUENCY AXIS IS NOT CALIBRATED FOR LIVE DATA. fft_config's
 *     sample_rate_hz is a placeholder (there is no timebase — dev plan §F4),
 *     so bin -> Hz is unknown. The live view therefore reports the peak by
 *     BIN INDEX, which is exact, and never in Hz. The demo view does quote
 *     Hz, and may: the test signal is synthesised at exactly that assumed
 *     rate, so the two agree by construction.
 *  2. The record is 1024 samples into a 4096-point transform. fft_process()
 *     zero-pads, which interpolates the spectrum (fine), but applies the
 *     first quarter of a 4096-point window to it, which is an asymmetric
 *     taper (not fine — it costs sidelobe rejection). Relative magnitudes
 *     stay meaningful; this is a real limitation of analysing a short record
 *     with a long window, and the fix belongs in fft.c, not here.
 */
static bool fft_prepare_input(const fft_config_t *cfg, int16_t *sbuf,
                              uint16_t *n_out)
{
    const volatile uint8_t *b = fpga_get_ch1_buf();

    if (fpga_data_ready() && b != NULL) {
        /* Unsigned 8-bit about 128 -> signed, then a fixed <<7 for numeric
         * headroom in the transform. A constant gain shifts every bin by the
         * same dB and cannot change the shape of the spectrum. */
        for (uint16_t i = 0; i < FPGA_ADC_BUF_SIZE; i++)
            sbuf[i] = (int16_t)(((int16_t)b[i] - 128) * 128);
        *n_out = FPGA_ADC_BUF_SIZE;
        return true;
    }

    test_signal_generate(TEST_SIG_SQUARE, sbuf,
                         FFT_SIZE, cfg->sample_rate_hz,
                         1000.0f, 0.0f, 0.8f);
    *n_out = FFT_SIZE;
    return false;
}

static float fft_display_ref(const fft_config_t *cfg, const float *data,
                             uint16_t start_bin, uint16_t end_bin)
{
    if (end_bin >= FFT_BINS) end_bin = FFT_BINS - 1;
    if (start_bin > end_bin) start_bin = end_bin;

    float peak = data[start_bin];
    uint16_t b;
    for (b = (uint16_t)(start_bin + 1); b <= end_bin; b++)
        if (data[b] > peak) peak = data[b];

    float target = peak + FFT_REF_HEADROOM_DB;

    if (!fft_ref_primed) {
        fft_ref_tracked = target;
        fft_ref_primed = true;
    } else if (target > fft_ref_tracked) {
        fft_ref_tracked = target;                       /* attack: immediate */
    } else if (fft_ref_tracked - target > FFT_REF_DEAD_BAND_DB) {
        fft_ref_tracked += (target - fft_ref_tracked) * FFT_REF_DECAY;
    }

    return fft_ref_tracked + cfg->ref_level_db;
}

static void draw_fft_region(uint16_t y_top, uint16_t height)
{
    const fft_config_t *cfg = fft_get_config();
    uint16_t y_bot = y_top + height;

    int16_t *sbuf = fft_get_sample_buf();
    if (!sbuf) return;  /* FFT not initialized */
    uint16_t nsamp = 0;
    bool live = fft_prepare_input(cfg, sbuf, &nsamp);
    fft_process(sbuf, nsamp, &fft_result);

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.level_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    uint16_t x;

    /* Same auto-ranged reference the waterfall uses: with the configured
     * ref_level_db = 0 every bar pinned to full height, because the dB scale
     * is relative to one count, not to full scale. See fft_display_ref(). */
    float ref_db = fft_display_ref(cfg, draw_data, zoom_start, zoom_end);
    float range_db = cfg->db_range;

    for (x = 0; x < LCD_WIDTH; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / LCD_WIDTH);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;

        float db = draw_data[bin];
        float normalized = (ref_db - db) / range_db;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        uint16_t bar_top = y_top + (uint16_t)(normalized * (float)height);
        if (bar_top < y_bot) {
            uint16_t color;
            if (normalized < 0.25f) color = COLOR_CH1;
            else if (normalized < 0.5f) color = COLOR_GREEN;
            else if (normalized < 0.75f) color = COLOR_CYAN;
            else color = COLOR_GRID;
            lcd_fill_rect(x, bar_top, 1, y_bot - bar_top, color);
        }

        if (fft_result.max_hold_db != NULL) {
            float mh_norm = (ref_db - fft_result.max_hold_db[bin]) / range_db;
            if (mh_norm >= 0.0f && mh_norm <= 1.0f) {
                uint16_t mh_y = y_top + (uint16_t)(mh_norm * (float)height);
                if (mh_y >= y_top && mh_y < y_bot && (x & 1) == 0)
                    lcd_set_pixel(x, mh_y, COLOR_RED);
            }
        }
    }

    /* Horizontal grid lines (every 10dB) */
    float db;
    for (db = ref_db; db > ref_db - range_db; db -= 10.0f) {
        float normalized = (ref_db - db) / range_db;
        uint16_t y = y_top + (uint16_t)(normalized * (float)height);
        if (y > y_top && y < y_bot)
            for (x = 0; x < LCD_WIDTH; x += 4)
                lcd_set_pixel(x, y, COLOR_GRID);
    }

    if (height > 80) {
        for (db = ref_db; db > ref_db - range_db; db -= 20.0f) {
            float normalized = (ref_db - db) / range_db;
            uint16_t y = y_top + (uint16_t)(normalized * (float)height);
            if (y > y_top + 8 && y < y_bot - 8) {
                int db_int = (int)db;
                char label[8];
                int pos = 0;
                if (db_int < 0) { label[pos++] = '-'; db_int = -db_int; }
                if (db_int >= 100) label[pos++] = (char)('0' + db_int / 100);
                if (db_int >= 10)  label[pos++] = (char)('0' + (db_int / 10) % 10);
                label[pos++] = (char)('0' + db_int % 10);
                label[pos] = '\0';
                font_draw_string(2, y - 5, label, COLOR_GRAY, COLOR_GRAY, &font_small);
            }
        }
    }

    uint8_t p;
    for (p = 0; p < fft_result.num_peaks && p < 3; p++) {
        uint16_t peak_bin = fft_result.peaks[p].bin;
        if (peak_bin < zoom_start || peak_bin > zoom_end) continue;

        uint16_t peak_x = (uint16_t)((uint32_t)(peak_bin - zoom_start)
                          * LCD_WIDTH / zoom_span);
        float norm = (ref_db - fft_result.peaks[p].level_db) / range_db;
        if (norm < 0.0f) norm = 0.0f;
        uint16_t peak_y = y_top + (uint16_t)(norm * (float)height);

        if (peak_y >= y_top + 4 && peak_x > 2 && peak_x < LCD_WIDTH - 2) {
            lcd_set_pixel(peak_x, peak_y - 3, COLOR_RED);
            lcd_set_pixel(peak_x - 1, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x, peak_y - 2, COLOR_RED);
            lcd_set_pixel(peak_x + 1, peak_y - 2, COLOR_RED);
        }

        /* The per-peak harmonic tags ("Fund", "H2", ...) are RATIOS between
         * bins, so they stay true whatever the sample rate turns out to be —
         * unlike an absolute Hz figure, which is why one is drawn here and
         * the other is not. */
        if (fft_result.peaks[p].label[0] != '\0' && peak_x > 8 && peak_x < LCD_WIDTH - 30) {
            font_draw_string(peak_x - 8, peak_y - 12,
                             fft_result.peaks[p].label, COLOR_ORANGE, COLOR_ORANGE, &font_small);
        }
    }

    /* Source + peak header. The peak is quoted in Hz ONLY for the synthetic
     * signal, where the sample rate is the one it was generated at; for live
     * capture it is quoted as a bin index, which needs no rate to be true. */
    {
        char hdr[28];
        uint16_t color;
        if (live) {
            color = COLOR_WHITE;
            if (fft_result.num_peaks > 0)
                snprintf(hdr, sizeof(hdr), "LIVE CH1  pk bin %u",
                         (unsigned)fft_result.peaks[0].bin);
            else
                snprintf(hdr, sizeof(hdr), "LIVE CH1");
        } else {
            char freq_str[16];
            color = COLOR_ORANGE;
            if (fft_result.num_peaks > 0) {
                format_freq(fft_result.peaks[0].freq_hz, freq_str,
                            sizeof(freq_str));
                snprintf(hdr, sizeof(hdr), "DEMO sq  pk %s", freq_str);
            } else {
                snprintf(hdr, sizeof(hdr), "DEMO SIGNAL");
            }
        }
        font_draw_string(4, y_top + 2, hdr, color, color, &font_small);
    }

    const char *win_names[] = { "Rect", "Hann", "Hamm", "BHar", "Flat" };
    const char *win_name = (cfg->window < FFT_WINDOW_COUNT)
                           ? win_names[cfg->window] : "?";
    font_draw_string_right(LCD_WIDTH - 4, y_top + 2, win_name,
                           COLOR_GRAY, COLOR_GRAY, &font_small);
}

void draw_fft_screen(void)
{
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);
    draw_fft_region(SCOPE_TOP + 2, SCOPE_H - 4);
}

void draw_split_screen(uint32_t frame)
{
    lcd_fill_rect(0, SCOPE_TOP, LCD_WIDTH, SCOPE_H, COLOR_BLACK);

    uint16_t scope_top = SCOPE_TOP;
    uint16_t scope_bot = SCOPE_TOP + SCOPE_H / 2 - 1;
    uint16_t scope_mid = (scope_top + scope_bot) / 2;
    uint16_t x;

    for (x = 0; x < LCD_WIDTH; x += 32)
        for (uint16_t y = scope_top; y < scope_bot; y += 2)
            lcd_set_pixel(x, y, COLOR_GRID);
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, scope_mid, COLOR_GRID_CENTER);

    /* Top half: the real CH1 record when there is one. Half-height, so the
     * same y-transform as the full trace with SCOPE_H replaced by the band
     * height — one screen column is still one sample. */
    const volatile uint8_t *b1 = fpga_get_ch1_buf();
    bool trace_live = fpga_data_ready() && b1 != NULL;

    if (trace_live) {
        uint16_t band_h = (uint16_t)(scope_bot - scope_top);
        for (x = 0; x < LCD_WIDTH && x < FPGA_ADC_BUF_SIZE; x++) {
            int16_t wy = (int16_t)scope_mid -
                         (int16_t)(((int16_t)b1[x] - 128) * band_h / 256);
            if (wy >= (int16_t)scope_top && wy < (int16_t)scope_bot)
                lcd_set_pixel(x, (uint16_t)wy, COLOR_CH1);
        }
    } else {
        static const int8_t sin_lut[64] = {
             0, 10, 19, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 99,
            100, 99, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 19, 10,
             0,-10,-19,-29,-38,-47,-56,-63,-71,-77,-83,-88,-92,-96,-98,-99,
           -100,-99,-98,-96,-92,-88,-83,-77,-71,-63,-56,-47,-38,-29,-19,-10,
        };
        for (x = 0; x < LCD_WIDTH; x++) {
            uint8_t idx = (uint8_t)((x * 4 + frame) & 0x3F);
            int16_t wy = scope_mid - (sin_lut[idx] * 25 / 100);
            if (wy >= (int16_t)scope_top && wy < (int16_t)scope_bot)
                lcd_set_pixel(x, (uint16_t)wy, COLOR_CH1);
        }
        /* An animated sine that no probe produced. Say so. */
        font_draw_string(4, scope_top + 2, "DEMO TRACE",
                         COLOR_ORANGE, COLOR_ORANGE, &font_small);
    }

    uint16_t divider_y = SCOPE_TOP + SCOPE_H / 2;
    for (x = 0; x < LCD_WIDTH; x++)
        lcd_set_pixel(x, divider_y, COLOR_GRID_CENTER);

    draw_fft_region(divider_y + 1, SCOPE_H / 2 - 2);
}

/* Waterfall
 *
 * The 20 KB history buffer lives in the shared pool as a sub-tenant of the
 * FFT region (see SHMEM_FFT_WATERFALL_OFFSET) rather than in .bss. It cannot
 * be a pool owner of its own: this function calls fft_process(), so it only
 * ever runs while SHMEM_OWNER_FFT holds the pool, and claiming it separately
 * would evict the FFT it depends on.
 *
 * Only waterfall_row_idx, the generation counter and one assembled screen
 * line stay static — 645 bytes instead of 20,480.
 */
#define WATERFALL_ROWS  SHMEM_FFT_WATERFALL_ROWS
#define WATERFALL_COLS  SHMEM_FFT_WATERFALL_COLS
static uint8_t waterfall_row_idx = 0;
static uint32_t waterfall_pool_generation = 0;

/* The row blit below sets one window across the full plot width per history
 * row and streams into it, so a history row must be exactly one screen line
 * wide. */
_Static_assert(WATERFALL_COLS == LCD_WIDTH,
               "waterfall row blit assumes one history column per screen pixel");

/*
 * One assembled screen line, 640 bytes. Colours are converted once per
 * history row and pushed row_height times, instead of being recomputed for
 * every sub-row. It is deliberately NOT the 20 KB history buffer's problem:
 * that lives in the shared pool (see shared_mem.h) precisely because it is
 * large, whereas one line is small enough that the pool's ownership
 * lifetime rules would cost more than they save.
 */
static uint16_t waterfall_line[WATERFALL_COLS];

/*
 * Intensity -> colour, with a BLACK floor.
 *
 * intensity 0 = at/above the display reference, 255 = at/below the floor
 * (reference - db_range). The old ramp ended at pure blue, so the noise
 * floor painted the plot solid blue and only the very top 20% of the scale
 * was red; combined with the reference bug (see fft_display_ref) that made
 * the whole plot one flat colour. This one runs black -> blue -> cyan ->
 * yellow -> red in four equal 64-step segments, which is the conventional
 * spectrogram ordering and puts "nothing here" at black.
 */
static uint16_t intensity_to_color(uint8_t intensity)
{
    uint16_t level = (uint16_t)(255u - intensity);   /* 0 = floor, 255 = hot */
    uint8_t  t = (uint8_t)((level & 63u) * 4u);      /* position in segment  */

    if (level < 64)  return RGB565(0, 0, t);                        /* black->blue  */
    if (level < 128) return RGB565(0, t, 255);                      /* blue ->cyan  */
    if (level < 192) return RGB565(t, 255, (uint8_t)(255 - t));     /* cyan ->yellow*/
    return RGB565(255, (uint8_t)(255 - t), 0);                      /* yellow->red  */
}

void draw_waterfall_screen(void)
{
    const fft_config_t *cfg = fft_get_config();

    int16_t *sbuf = fft_get_sample_buf();
    if (!sbuf) return;  /* FFT not initialized */
    uint16_t nsamp = 0;
    bool live = fft_prepare_input(cfg, sbuf, &nsamp);
    fft_process(sbuf, nsamp, &fft_result);

    /* Resolve the history buffer out of the FFT's pool tenancy. Returns NULL
     * if FFT no longer owns the pool, in which case there is nothing valid to
     * draw into — bail rather than scribble over another owner's data. This is
     * a stricter check than the fft_get_sample_buf() guard above, which only
     * proves the FFT was initialised at some point. */
    uint8_t *wf_pool = shared_mem_get(SHMEM_OWNER_FFT);
    if (!wf_pool) return;
    uint8_t (*waterfall_buf)[WATERFALL_COLS] =
        (uint8_t (*)[WATERFALL_COLS])(wf_pool + SHMEM_FFT_WATERFALL_OFFSET);

    /* shared_mem_acquire() zeroes the pool whenever the owner changes, so a
     * round trip through screenshot/persistence wipes the history. Detect that
     * and restart from row 0 instead of scrolling through a zeroed tail.
     *
     * Refill with 0xFF rather than leaving the pool's zeros: intensity 0 maps
     * to the top of the ramp (red = at the display reference), so zeroed rows
     * would render as maximum signal. 0xFF is the other end of the ramp
     * (black = reference - db_range = the noise floor), which is what "no data
     * yet" should look like. Bench 2026-08-13: with the zero fill, entering
     * WFALL painted the whole plot red and the history grew downward
     * through it. */
    uint32_t pool_generation = shared_mem_transition_count();
    if (pool_generation != waterfall_pool_generation) {
        waterfall_pool_generation = pool_generation;
        waterfall_row_idx = 0;
        memset(waterfall_buf, 0xFF, SHMEM_FFT_WATERFALL_SIZE);
    }

    const float *draw_data = (fft_result.avg_db != NULL)
                             ? fft_result.avg_db : fft_result.level_db;

    uint16_t zoom_start = cfg->zoom_start_bin;
    uint16_t zoom_end = cfg->zoom_end_bin;
    uint16_t zoom_span = zoom_end - zoom_start;
    if (zoom_span == 0) zoom_span = 1;

    float ref_db = fft_display_ref(cfg, draw_data, zoom_start, zoom_end);

    uint16_t x;
    for (x = 0; x < WATERFALL_COLS; x++) {
        uint16_t bin = zoom_start + (uint16_t)((uint32_t)x * zoom_span / WATERFALL_COLS);
        if (bin >= FFT_BINS) bin = FFT_BINS - 1;
        float db = draw_data[bin];
        float normalized = (ref_db - db) / cfg->db_range;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        waterfall_buf[waterfall_row_idx][x] = (uint8_t)(normalized * 255.0f);
    }

    uint8_t newest_row = waterfall_row_idx;
    waterfall_row_idx = (uint8_t)((waterfall_row_idx + 1) % WATERFALL_ROWS);

    /* ── Blit ───────────────────────────────────────────────────────
     *
     * This used to clear the whole plot and then draw every pixel column as
     * its own 1 x row_height lcd_fill_rect: 20,481 fill_rects and therefore
     * 20,481 lcd_set_window() calls per frame, each one 3 commands + 8 byte
     * writes with a bus delay after every single one — ~225,000 delayed bus
     * transactions to paint 61,440 pixels. Bench 2026-08-13: the plot
     * visibly rastered left-to-right, top-to-bottom over seconds.
     *
     * Now: one window per history row, and each row is converted once into
     * waterfall_line and streamed row_height times. 65 set_windows and 192
     * lcd_write_pixels() calls per frame, with the same pixels at the same
     * coordinates.
     *
     * One window for the WHOLE block would cost 2 set_windows instead of 65,
     * but the window is global driver state and vInputTask runs at priority 4
     * against the display task's 1: its power-off countdown overlay
     * (input_handler.c) calls lcd_fill_rect() and so re-points the window
     * from under a stream in progress. Per-row windows bound that to one
     * corrupted row for one frame — the same blast radius the per-column
     * version had — for ~700 extra bus transactions out of 66,000.
     *
     * The clear is gone too: the block covers every pixel it draws over, so
     * the only part of the plot that still needs clearing is the residual
     * strip left when SCOPE_H is not a multiple of WATERFALL_ROWS (14 rows
     * at the current 206/64). That halves the pixel writes as well.
     *
     * Why not scroll-and-append, which would be O(1 row)? The history
     * scrolls by exactly one row per frame, so 63 of 64 rows are unchanged
     * in CONTENT — but they all MOVE, and with no framebuffer on this side
     * of the bus there is nothing to shift; the ST7789's own vertical
     * scroll works on panel rows, which MADCTL = 0xA0 (MV = 1) maps to
     * screen COLUMNS, i.e. it would scroll the waterfall sideways. An
     * O(1)-per-frame waterfall needs either a RAM framebuffer (the shared
     * pool is already spoken for by the FFT here) or a different history
     * layout that lets the newest row wrap in place instead of the image
     * scrolling — which changes what is drawn, so it is a design decision,
     * not a speedup. */
    uint16_t row_height = SCOPE_H / WATERFALL_ROWS;
    if (row_height < 1) row_height = 1;

    uint16_t rows_drawn = WATERFALL_ROWS;
    if ((uint32_t)rows_drawn * row_height > SCOPE_H)
        rows_drawn = (uint16_t)(SCOPE_H / row_height);
    uint16_t block_h = (uint16_t)(rows_drawn * row_height);

    if (block_h < SCOPE_H)
        lcd_fill_rect(0, (uint16_t)(SCOPE_TOP + block_h), LCD_WIDTH,
                      (uint16_t)(SCOPE_H - block_h), COLOR_BLACK);

    uint16_t r;
    for (r = 0; r < rows_drawn; r++) {
        const uint8_t *src =
            waterfall_buf[(newest_row + WATERFALL_ROWS - r) % WATERFALL_ROWS];
        for (x = 0; x < WATERFALL_COLS; x++)
            waterfall_line[x] = intensity_to_color(src[x]);

        lcd_set_window(0, (uint16_t)(SCOPE_TOP + r * row_height),
                       WATERFALL_COLS, row_height);

        uint16_t rep;
        for (rep = 0; rep < row_height; rep++)
            lcd_write_pixels(waterfall_line, WATERFALL_COLS);
    }

    /* Say which signal built this history. A waterfall of a compiled-in test
     * tone looks exactly like a waterfall of a probe. */
    if (live)
        font_draw_string(4, SCOPE_TOP + 2, "WFALL LIVE CH1",
                         COLOR_WHITE, COLOR_WHITE, &font_small);
    else
        font_draw_string(4, SCOPE_TOP + 2, "WFALL DEMO SIGNAL",
                         COLOR_ORANGE, COLOR_ORANGE, &font_small);
}

#endif /* FEATURE_FFT */
