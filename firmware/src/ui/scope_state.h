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
    uint8_t         vdiv_idx;       /* Index into vdiv_table[] */
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

/* Full oscilloscope state */
typedef struct {
    channel_state_t ch1;
    channel_state_t ch2;
    trigger_state_t trigger;
    uint8_t         timebase_idx;   /* Index into timebase_table[] */
    bool            running;        /* Acquisition running/stopped */
} scope_state_t;

/* ═══════════════════════════════════════════════════════════════════
 * Lookup tables (defined in scope_state.c)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *label;      /* Display string: "5mV", "10mV", ..., "10V" */
    uint32_t    uv_per_div; /* Microvolts per division (for computation) */
} vdiv_entry_t;

typedef struct {
    const char *label;      /* Display string: "5ns", "10ns", ..., "50s" */
    uint32_t    ns_per_div; /* Nanoseconds per division (for computation) */
} timebase_entry_t;

extern const vdiv_entry_t     vdiv_table[VDIV_COUNT];
extern const timebase_entry_t timebase_table[TIMEBASE_COUNT];

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

#endif /* SCOPE_STATE_H */
