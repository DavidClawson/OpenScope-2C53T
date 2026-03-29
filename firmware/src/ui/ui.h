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
extern volatile int8_t        settings_depth;      /* 0=top, 1=sub-menu */
extern volatile int8_t        settings_sub_selected; /* Sub-menu selection */
extern volatile uint8_t       active_channel;      /* 0=CH1, 1=CH2 (for scope adjustments) */

#define SETTINGS_ITEM_COUNT     7
#define SETTINGS_OSC_ITEM_COUNT 8
#define SETTINGS_ABOUT_LINES    5

#ifdef FEATURE_FFT
#include "fft.h"
#include "fft_test_signals.h"
extern volatile scope_view_t scope_view;
extern int16_t          fft_sample_buf[];
extern fft_result_t     fft_result;
#endif

/* ═══════════════════════════════════════════════════════════════════
 * UI Drawing Function Prototypes
 * ═══════════════════════════════════════════════════════════════════ */

/* status_bar.c */
void draw_status_bar(void);
void draw_info_bar(void);
void draw_splash(void);

/* scope_ui.c */
void draw_scope_grid(void);
void draw_demo_waveform(uint32_t frame);
void draw_scope_screen(uint32_t frame);
void scope_show_popup(const char *text);
#ifdef FEATURE_FFT
void draw_fft_screen(void);
void draw_split_screen(uint32_t frame);
void draw_waterfall_screen(void);
#endif

/* meter_ui.c */
void draw_meter_screen(void);

/* siggen_ui.c */
void draw_siggen_screen(uint32_t frame);

/* settings_ui.c */
void draw_settings_screen(void);
void draw_health_panel(uint16_t y_start);

#endif /* UI_H */
