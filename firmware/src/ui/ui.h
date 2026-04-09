/*
 * OpenScope 2C53T - UI Types and Declarations
 *
 * Common types, enums, and function prototypes shared across
 * all UI drawing modules and the main task code.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Enums (shared between main.c tasks and UI drawing code)
 * ═══════════════════════════════════════════════════════════════════ */

/* Device modes (matching original firmware structure) */
typedef enum {
    MODE_OSCILLOSCOPE = 0,
    MODE_MULTIMETER,
    MODE_SIGNAL_GEN,
    MODE_SETTINGS,
    MODE_COUNT
} device_mode_t;

/* Display commands sent via queue */
typedef enum {
    DCMD_DRAW_SPLASH = 0,
    DCMD_DRAW_SCOPE,
    DCMD_DRAW_METER,
    DCMD_DRAW_SIGGEN,
    DCMD_DRAW_SETTINGS,
    DCMD_DRAW_STATUS_BAR,
    DCMD_REDRAW_ALL,
#ifdef FEATURE_FFT
    DCMD_DRAW_FFT,
#endif
} display_cmd_t;

/* Button IDs */
typedef enum {
    BTN_NONE = 0,
    BTN_CH1,
    BTN_CH2,
    BTN_MOVE,
    BTN_SELECT,
    BTN_TRIGGER,
    BTN_PRM,
    BTN_AUTO,
    BTN_SAVE,
    BTN_MENU,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_OK,
    BTN_POWER,
} button_id_t;

#ifdef FEATURE_FFT
/* Oscilloscope sub-view (time domain vs FFT) */
typedef enum {
    SCOPE_VIEW_TIME = 0,
    SCOPE_VIEW_FFT,
    SCOPE_VIEW_SPLIT,
    SCOPE_VIEW_WATERFALL,
    SCOPE_VIEW_COUNT
} scope_view_t;
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Extern globals (defined in main.c, used by UI modules)
 * ═══════════════════════════════════════════════════════════════════ */

extern volatile device_mode_t current_mode;
extern volatile uint32_t      uptime_seconds;
extern volatile int8_t        settings_selected;  /* Selected menu item index */
extern volatile int8_t        settings_depth;      /* 0=top, 1=sub-menu, 2=about, 3=osc-math, 4=component */
extern volatile int8_t        settings_sub_selected; /* Sub-menu selection */
extern volatile uint8_t       active_channel;      /* 0=CH1, 1=CH2 (for scope adjustments) */
extern volatile uint8_t       meter_submode;       /* 0-9: current meter sub-mode */
extern volatile uint8_t       meter_layout;        /* 0=full, 1=chart, 2=stats */
extern volatile bool          meter_rel_enabled;   /* Relative/delta mode */
extern volatile float         meter_rel_reference; /* Zero reference value */
extern volatile bool          meter_hold_enabled;  /* Auto-hold mode */
extern volatile bool          meter_hold_locked;   /* Hold captured a reading */
extern volatile float         meter_hold_value;    /* Captured hold value */
extern volatile uint8_t       fuse_type;           /* fuse_type_t: ATO, Mini, etc. */
extern volatile uint8_t       fuse_rating_idx;     /* Index into fuse table for selected type */
extern volatile uint8_t       fuse_view;           /* FUSE_VIEW_DETAIL/MULTI/SCAN */
extern volatile float         fuse_scan_threshold_mv; /* Pass/fail threshold in mV */

/* Scope feature toggles (defined in main.c) */
extern volatile bool          math_enabled;
extern volatile uint8_t       math_op;        /* math_op_t cast */
extern volatile bool          persist_enabled;

#define SETTINGS_ITEM_COUNT     12
#define SETTINGS_OSC_ITEM_COUNT 8
#define SETTINGS_ABOUT_LINES    5
#define METER_SUBMODE_COUNT     10
#define METER_LAYOUT_COUNT      4
#define METER_LAYOUT_FULL       0
#define METER_LAYOUT_CHART      1
#define METER_LAYOUT_STATS      2
#define METER_LAYOUT_FUSE       3

/* Fuse tester sub-views (cycled with SELECT in fuse layout) */
#define FUSE_VIEW_DETAIL        0   /* Single fuse: current + V_drop + R + bar */
#define FUSE_VIEW_MULTI         1   /* All ratings for selected type */
#define FUSE_VIEW_SCAN          2   /* Pass/fail parasitic draw hunting */
#define FUSE_VIEW_COUNT         3

#ifdef FEATURE_FFT
#include "fft.h"
#include "fft_test_signals.h"
extern volatile scope_view_t scope_view;
extern fft_result_t     fft_result;
#endif

/* ═══════════════════════════════════════════════════════════════════
 * UI Drawing Function Prototypes
 * ═══════════════════════════════════════════════════════════════════ */

/* status_bar.c */
void draw_status_bar(void);
void status_bar_invalidate(void);
void draw_info_bar(void);
void draw_splash(void);

/* scope_ui.c */
void draw_scope_grid(void);
void draw_demo_waveform(uint32_t frame);
void draw_scope_screen(uint32_t frame);
void scope_show_popup(const char *text);
bool scope_popup_active(void);
#ifdef FEATURE_FFT
void draw_fft_screen(void);
void draw_split_screen(uint32_t frame);
void draw_waterfall_screen(void);
#endif

/* meter_ui.c */
void draw_meter_screen(void);
void meter_reset_minmaxavg(void);
void meter_toggle_relative(void);
void meter_toggle_hold(void);
void meter_toggle_debug_overlay(void);

/* fuse_ui.c */
void draw_fuse_screen(float voltage_drop_mv);
void fuse_cycle_view(void);
void fuse_next_rating(void);
void fuse_prev_rating(void);
void fuse_next_type(void);
void fuse_prev_type(void);
const char *meter_submode_name(uint8_t submode);

/* siggen_ui.c */
void draw_siggen_screen(uint32_t frame);

/* settings_ui.c */
void draw_settings_screen(void);
void draw_health_panel(uint16_t y_start);

/* component_ui.c */
void draw_component_test_screen(void);

/* bode_ui.c */
void draw_bode_screen(void);

/* resistor_calc_ui.c */
void draw_resistor_calc_screen(void);
void resistor_calc_init(void);
void resistor_calc_move_band(int direction);
void resistor_calc_change_color(int direction);
void resistor_calc_simulate_measure(void);
float resistor_calc_get_value(void);

#endif /* UI_H */
