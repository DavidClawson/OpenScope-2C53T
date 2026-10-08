/*
 * Fuse tester model: the arithmetic, colours and shapes behind the fuse
 * view, kept free of the LCD so the host tests can check them.
 *
 * The current estimate is I = V_drop / R_fuse. With the drop in mV and the
 * resistance in micro-ohms that is mA = mV * 1e6 / uOhm. Until 2026-10-06
 * fuse_ui.c used mV * 1e3 / uOhm, which is AMPS, and printed it as mA -- a
 * 48 mA parasitic draw on a 10 A ATO read "0 mA". tests/test_fuse.c keeps
 * the old formula as a negative control.
 */
#ifndef FUSE_MODEL_H
#define FUSE_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "fuse_table.h"

/* Parasitic-draw rule of thumb the views mark (a whole car at rest is
 * usually expected under ~50 mA). */
#define FUSE_DRAW_LIMIT_MA      50u
/* Full scale of the Detail view's bar: three times the limit. */
#define FUSE_BAR_FULL_MA        150u

#define FUSE_SCAN_THRESH_MIN_MV 0.1f
#define FUSE_SCAN_THRESH_MAX_MV 5.0f
#define FUSE_SCAN_THRESH_STEP   0.1f

/* Current through the fuse in mA, rounded; the sign of the drop (probe
 * polarity) is ignored. Saturates at UINT32_MAX/2; 0 for an unknown fuse. */
uint32_t fuse_current_ma(float drop_mv, uint32_t resistance_uohm);

/* A meter reading as a drop in mV. Only "V" and "mV" readings qualify; any
 * other unit (the meter is on another function) returns false. */
bool fuse_drop_mv_from_reading(float value, const char *unit, float *mv_out);

/* Decimals to print the drop in mV with, from the meter's own display string
 * (so the screen never claims more resolution than the meter gave). */
int fuse_mv_decimals(const char *display_str, const char *unit);

/* "7.5" for the table's 7 A entry (it is a 7.5 A fuse), else the number. */
void fuse_rating_label(uint8_t rating_amps, char *buf, uint8_t n);

/* The common colour for a rating in a type's family, as 0xRRGGBB. False when
 * the family has no standard colour for it -- draw it neutral, never guess. */
bool fuse_rating_rgb(fuse_type_t type, uint8_t rating_amps, uint32_t *rgb);

/* A representative colour for drawing the type itself (Types page). */
uint32_t fuse_type_rgb(fuse_type_t type);

/* The four ratings the Scan view estimates for ("if this fuse is..."). */
uint8_t fuse_scan_ratings(fuse_type_t type, uint8_t out[4]);

/* Next Draw-if preset above `cur_mv` (wraps to the smallest). */
float fuse_scan_threshold_next_preset(float cur_mv);

/*
 * The drop as it arrives: the lead offset ("Cal leads") and whether the
 * reading has settled. Shorted, unit #1's leads read a steady -0.9 mV
 * (+/-0.1 over 40 readings) -- 114 mA on a 10 A ATO, a phantom DRAW. Open,
 * they wander -2.8..+5.1 mV, and a verdict there is noise (F47).
 *
 * Steady = the last FUSE_STEADY_N readings span no more than
 * FUSE_STEADY_SPAN_MV plus FUSE_STEADY_SPAN_PCT of the drop, so a loaded
 * fuse whose current wobbles a little still counts.
 */
#define FUSE_STEADY_N           8       /* ~1 s of meter frames */
#define FUSE_STEADY_SPAN_MV     0.5f
#define FUSE_STEADY_SPAN_PCT    20u
#define FUSE_CAL_MAX_MV         5.0f    /* larger is not two tips touching */

typedef struct {
    int16_t  hist[FUSE_STEADY_N];   /* raw drops, 0.1 mV units */
    uint8_t  n, i;
    float    cal_mv;                /* subtracted from every drop when set */
    bool     cal_set;
} fuse_input_t;

void  fuse_input_push(fuse_input_t *s, float raw_mv);
void  fuse_input_clear(fuse_input_t *s);          /* history only, not the cal */
bool  fuse_input_steady(const fuse_input_t *s);
float fuse_input_mean(const fuse_input_t *s);     /* of the history, mV */

typedef enum {
    FUSE_CAL_OK = 0,
    FUSE_CAL_UNSTEADY,      /* not settled: tips not held together */
    FUSE_CAL_TOO_LARGE,     /* settled but over FUSE_CAL_MAX_MV: on a live fuse? */
} fuse_cal_result_t;

/* Store the settled mean as the lead offset, or refuse and leave it as is. */
fuse_cal_result_t fuse_input_calibrate(fuse_input_t *s);

/*
 * Outline of each type at 4 px per mm, from the design canvas (FuseTypes).
 * Front view: body at (1,1) size w-2 x body_h, two blades mirrored about the
 * centre (none for a cartridge), a window, and for a cartridge two dark
 * slots. Top view: body w x top_h with two mirrored probe pads.
 */
typedef struct {
    uint8_t w, h;                         /* front view, overall */
    uint8_t body_h;
    uint8_t blade_x, blade_y, blade_w, blade_h;   /* left blade or slot */
    uint8_t win_x, win_y, win_w, win_h;
    uint8_t top_h;
    uint8_t pad_x, pad_y, pad_w, pad_h;   /* left pad */
    uint8_t width_mm, height_mm;          /* for the caption */
    bool    cartridge;                    /* J-Case: no blades, round pads */
} fuse_shape_t;

const fuse_shape_t *fuse_shape(fuse_type_t type);

#endif /* FUSE_MODEL_H */
