/*
 * OpenScope 2C53T - Oscilloscope Runtime State
 *
 * Live state for the oscilloscope mode: channel config, trigger,
 * timebase, and display options. This is the single source of truth
 * that button handlers modify and UI screens read.
 *
 * Separate from config.h (which is for save/load persistence).
 * On boot, scope_state is initialized from config; on save, it's
 * written back.
 */

#ifndef SCOPE_STATE_H
#define SCOPE_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Enums
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    COUPLING_DC = 0,
    COUPLING_AC,
    COUPLING_GND,
    COUPLING_COUNT
} coupling_t;

typedef enum {
    PROBE_1X = 0,
    PROBE_10X,
    PROBE_COUNT
} probe_t;

typedef enum {
    TRIG_AUTO = 0,
    TRIG_NORMAL,
    TRIG_SINGLE,
    TRIG_COUNT
} trigger_mode_t;

typedef enum {
    TRIG_RISING = 0,
    TRIG_FALLING,
    TRIG_EDGE_COUNT
} trigger_edge_t;

typedef enum {
    TRIG_SRC_CH1 = 0,
    TRIG_SRC_CH2,
    TRIG_SRC_COUNT
} trigger_source_t;

/* ═══════════════════════════════════════════════════════════════════
 * Volts/div and timebase tables
 * ═══════════════════════════════════════════════════════════════════ */

#define VDIV_COUNT      10
#define TIMEBASE_COUNT  21

/* Per-channel configuration */
typedef struct {
    bool            enabled;
    uint8_t         vdiv_idx;       /* Frontend range index, 0..VDIV_COUNT-1.
                                     * Written straight to the attenuator bank;
                                     * calibrated in scope_cal.c. */
    coupling_t      coupling;
    probe_t         probe;
    bool            bw_limit;       /* 20MHz bandwidth limit */
    int16_t         position;       /* Vertical position offset (pixels) */
} channel_state_t;

/* Trigger configuration */
typedef struct {
    trigger_mode_t  mode;
    trigger_edge_t  edge;
    trigger_source_t source;
    int16_t         level;          /* Trigger level (pixel offset from center) */
} trigger_state_t;

/* ═══════════════════════════════════════════════════════════════════
 * Cursor measurement types
 * ═══════════════════════════════════════════════════════════════════ */

/* Scope waveform area boundaries (for cursor clamping) */
#define CURSOR_SCOPE_TOP       18
#define CURSOR_SCOPE_BOT       224
#define CURSOR_SCOPE_LEFT      0
#define CURSOR_SCOPE_RIGHT     319
#define CURSOR_SCOPE_HEIGHT    (CURSOR_SCOPE_BOT - CURSOR_SCOPE_TOP)
#define CURSOR_SCOPE_WIDTH     (CURSOR_SCOPE_RIGHT - CURSOR_SCOPE_LEFT + 1)

typedef enum {
    CURSOR_OFF = 0,
    CURSOR_VERTICAL,
    CURSOR_HORIZONTAL,
    CURSOR_BOTH,
    CURSOR_MODE_COUNT
} cursor_mode_t;

typedef enum {
    CURSOR_SEL_V1 = 0,
    CURSOR_SEL_V2,
    CURSOR_SEL_H1,
    CURSOR_SEL_H2,
} cursor_sel_t;

typedef struct {
    cursor_mode_t mode;
    cursor_sel_t  active;

    uint16_t v1_x;
    uint16_t v2_x;
    uint16_t h1_y;
    uint16_t h2_y;

    /* Display scales for the cursor delta readout.
     *
     * 0.0 means UNKNOWN — not "zero seconds per pixel". Both are 0 today
     * (see scope_state_init) because seconds need a timebase and volts need
     * per-range calibration, and this firmware has neither. Consumers MUST
     * check for > 0 before using them; scope_ui.c falls back to samples and
     * ADC counts, which are exact, when they are 0. */
    float time_per_pixel;
    float volts_per_pixel;
} cursor_state_t;

/* Full oscilloscope state */
typedef struct {
    channel_state_t ch1;
    channel_state_t ch2;
    trigger_state_t trigger;
    uint8_t         timebase_idx;   /* SPI reg 0x01 timebase code, written
                                     * straight to the FPGA; calibrated in
                                     * scope_timebase.c. */
    bool            running;        /* Acquisition running/stopped */
    cursor_state_t  cursor;         /* Cursor measurement state */

    /* Vertical graticule mode. false (default) = AUTOFIT: each channel's trace
     * is scaled from its own min/max to fill the band, so the grid is a
     * position reference, not a volts reference. true = TRUE SCALE: the trace
     * is drawn at a FIXED SCOPE_CAL_COUNTS_PER_DIV counts per division, so one
     * grid division means exactly the volts/div the status bar prints — but
     * only on ranges scope_cal calls usable; a NONE range always autofits.
     *
     * Off by default because true scale needs the baseline centred (DAC1/PA4
     * for CH1, TMR13/PA6 for CH2, via `fpga scope center`); uncentred, a
     * DC-offset trace sits off-screen, which is honest but surprising. Toggle
     * on the bench with `fpga scope graticule true`. */
    bool            true_scale;

    /* Software display trigger. true (default) = each frame's render window is
     * shifted to start on the trigger source's level crossing (edge/level/source
     * from `trigger`), so a periodic trace stands still instead of free-running.
     * false = draw from sample 0 every frame (the old free-run "dancing"). When
     * no crossing is found the render free-runs regardless, which is AUTO-mode
     * behaviour. Toggle on the bench with `fpga scope softtrig`. */
    bool            soft_trigger;
} scope_state_t;

/* ═══════════════════════════════════════════════════════════════════
 * Lookup tables (defined in scope_state.c)
 * ═══════════════════════════════════════════════════════════════════ */

/* vdiv_entry_t / vdiv_table removed 2026-08-18 — volts/div is measured, not
 * nominal. See scope_cal.h and the note in scope_state.c. */

/* timebase_entry_t / timebase_table removed 2026-08-18 — time/div is derived
 * from the measured sample rate. See scope_timebase.h and scope_state.c. */


extern const char *coupling_labels[COUPLING_COUNT];
extern const char *probe_labels[PROBE_COUNT];
extern const char *trigger_mode_labels[TRIG_COUNT];
extern const char *trigger_edge_labels[TRIG_EDGE_COUNT];
extern const char *trigger_source_labels[TRIG_SRC_COUNT];

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/* Initialize scope state with defaults */
void scope_state_init(scope_state_t *s);

/* Get the global scope state (singleton, defined in scope_state.c) */
scope_state_t *scope_state_get(void);

/* Convenience: cycle through enum values with wrap */
void scope_cycle_trigger_mode(scope_state_t *s);
void scope_cycle_trigger_edge(scope_state_t *s);
void scope_cycle_trigger_source(scope_state_t *s);
void scope_cycle_coupling(channel_state_t *ch);
void scope_cycle_probe(channel_state_t *ch);
void scope_toggle_bw_limit(channel_state_t *ch);
void scope_toggle_channel(channel_state_t *ch);
void scope_adjust_vdiv(channel_state_t *ch, int direction);
void scope_adjust_timebase(scope_state_t *s, int direction);
void scope_adjust_trigger_level(scope_state_t *s, int direction);
void scope_toggle_running(scope_state_t *s);

/* Cursor operations */
void scope_cursor_cycle_mode(void);
void scope_cursor_next_sel(void);
void scope_cursor_move(int16_t delta);

/* Acquisition-ready latch.
 *
 * Starts false at boot and stays false until the scope has observed at
 * least one real ADC frame from the FPGA. The UI uses this to decide
 * whether to show the synthetic demo waveform (shipped as a "not
 * connected" placeholder) or to leave the grid empty.
 *
 * Phase 4 will flip this via scope_mark_acquisition_ready() as soon as
 * the SPI3 bringup is working. */
bool scope_acquisition_ready(void);
void scope_mark_acquisition_ready(void);

#endif /* SCOPE_STATE_H */
