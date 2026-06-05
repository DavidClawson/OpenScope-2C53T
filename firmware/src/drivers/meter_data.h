/*
 * OpenScope 2C53T - Meter Data Parser
 *
 * Parses FPGA USART2 RX frames to extract multimeter readings.
 * The FPGA sends 12-byte data frames (0x5A 0xA5 header + 10 data bytes)
 * containing BCD-encoded measurement digits and status flags.
 *
 * Based on the stock DMM frame parser and display formatter documented under
 * reverse_engineering/analysis_v120/.
 */

#ifndef METER_DATA_H
#define METER_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Result Classification
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    METER_RESULT_NONE      = 0,  /* No data yet */
    METER_RESULT_NORMAL    = 1,  /* Valid reading */
    METER_RESULT_UNDERRANGE= 2,  /* Value too small for current range */
    METER_RESULT_OVERRANGE = 3,  /* Exceeds display, not OL */
    METER_RESULT_INVALID   = 4,  /* Unrecognized data */
    METER_RESULT_OVERLOAD  = 5,  /* "OL" — input overloaded */
    METER_RESULT_BLANK     = 6,  /* No measurement (blank display) */
    METER_RESULT_CONTINUITY= 7,  /* Continuity detected */
} meter_result_class_t;

typedef enum {
    METER_REJECT_NONE = 0,
    METER_REJECT_WRONG_FRAME_FAMILY = 1,
    METER_REJECT_INVALID_SUBMODE = 2,
    METER_REJECT_MISSING_AC_EVIDENCE = 3,
    METER_REJECT_UNRESOLVED_CALIBRATION = 4,
} meter_reject_reason_t;

/* ═══════════════════════════════════════════════════════════════════
 * Parsed Meter Reading
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Parsed measurement value */
    float    value;              /* Scaled measurement value */
    int      bcd_value;            /* Stock raw value: 4 BCD digits plus optional frame[2].3 +10000 */
    uint8_t  digits[4];          /* Individual BCD digits */
    uint8_t  decimal_pos;        /* Decimal point position (0=none, 1-3) */
    bool     negative;           /* Negative polarity */

    /* Display string (pre-formatted for UI) */
    char     display_str[16];    /* e.g., "-13.82", "OL", "---" */

    /* Unit suffix string for the current range.
     * Points to a static string (never NULL). Example values:
     *   DCV:         "V", "mV"
     *   ACV:         "V", "mV"
     *   DCA/ACA:     "A", "mA" (uA is a local placeholder, not stock-proven)
     *   Resistance:  "Ohm", "kOhm", "MOhm"
     *   Frequency:   "Hz", "kHz", "MHz"
     *   Capacitance: "nF", "uF"
     *   Diode:       "V"
     */
    const char *unit_suffix;

    /* Local unit/range variant (0..2) within the current submode. DC current
     * has stock formatter evidence for mA/A; AC A and uA remain local policy
     * until stock range state or live current traces prove them. */
    uint8_t  unit_variant;

    /* Bar graph fraction (0.0 - 1.0) */
    float    bar_fraction;
    float    aux_freq_hz;        /* Companion frequency from frame extra, if known */

    /* Classification */
    meter_result_class_t result_class;

    /* Status flags from RX frame byte [7] */
    bool     is_ac;              /* AC mode active */
    bool     is_auto_range;      /* Auto-range enabled */
    bool     is_hold;            /* Hold mode active */

    /* Meter mode handler state (from frame[6]/[7] status bits) */
    uint8_t  submode;            /* UI submode that produced this reading */
    uint8_t  probe_type;         /* 0, 1, or 2 — from frame[7] bit pattern */
    uint8_t  range_indicator;    /* from frame[6] bits 4-5: range band */
    uint8_t  range_cmd;          /* Legacy diagnostic mirror, not a range command. */
    uint8_t  stock_mode;         /* Stock DMM FSM mode family (0-7) */
    uint8_t  stock_variant;      /* Stock FSM unit/range variant */
    uint8_t  stock_format;       /* Stock FSM decimal/display format */
    uint8_t  stock_dc_state;     /* Stock DCV multi-frame state */
    uint8_t  stock_display_cmd;  /* Stock display selector */
    uint8_t  stock_unit_index;   /* Stock unit selector */
    uint8_t  stock_composite_index; /* Stock composite formatting selector */
    uint8_t  expected_frame_family; /* fpga_meter_frame_family_t from the active plan */
    uint8_t  observed_frame_family; /* Known marker family, or expected family if unclassified */
    uint8_t  reject_reason;      /* meter_reject_reason_t */

    /* Continuity buzzer state */
    bool     continuity_beep;    /* Should buzzer sound */

    /* Validity */
    bool     valid;              /* At least one successful parse */
    uint32_t update_count;       /* Incremented on each new reading */
    uint32_t display_update_count; /* Incremented only when the UI-visible value changes */

    /* Debug: raw frame bytes and pre-lookup nibble pairs */
    uint8_t  dbg_frame[12];     /* Last raw USART RX frame */
    uint8_t  dbg_nibbles[4];    /* Pre-lookup nibble pair values */
    uint8_t  dbg_raw_digits[4]; /* Post-lookup digit codes (before masking) */

} meter_reading_t;

typedef struct {
    uint32_t update_count;
    uint8_t  submode;
    uint8_t  result_class;
    uint8_t  decimal_pos;
    uint8_t  unit_variant;
    uint8_t  status;
    uint8_t  flags;
    uint8_t  meas_flags;
    uint8_t  additional_status;
    uint16_t extra;
    uint16_t aux_freq_hz_i10;
    uint8_t  expected_frame_family;
    uint8_t  observed_frame_family;
    uint8_t  reject_reason;
    int      bcd_value;
    char     display_str[16];
    const char *unit_suffix;
    uint8_t  frame[12];
    uint8_t  raw_digits[4];
} meter_frame_history_t;

/* Global meter reading (defined in meter_data.c) */
extern meter_reading_t meter_reading;

/*
 * Copy the current reading as one coherent UI/debug snapshot.
 * The parser updates meter_reading from another task; direct multi-field reads
 * can otherwise mix an old special state (for example OL) with a newer value.
 */
bool meter_data_snapshot(meter_reading_t *out);

/* Debug: distinct values of frame[6] seen since boot.
 * Up to 8 unique byte values stored; new values push out the oldest.
 * Used by the Phase 1 meter UI debug strip to visualize how many
 * different frame types the FPGA is sending per measurement. */
#define METER_F6_HISTORY_LEN 8
extern uint8_t meter_f6_history[METER_F6_HISTORY_LEN];
extern uint8_t meter_f6_history_count;

#define METER_FRAME_HISTORY_LEN 8
extern meter_frame_history_t meter_frame_history[METER_FRAME_HISTORY_LEN];
extern uint8_t meter_frame_history_count;
extern uint8_t meter_frame_history_head;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Initialize meter data parser state.
 * Call once at startup.
 */
void meter_data_init(void);

/*
 * Invalidate the current reading during meter mode/range transitions.
 * This prevents the UI and USB debug shell from relabeling the previous
 * mode's last valid frame while the FPGA/meter IC settles into the new mode.
 */
void meter_data_invalidate(uint8_t submode);

/*
 * Process a complete FPGA USART2 RX data frame (12 bytes).
 * Extracts BCD digits, detects special codes (OL, continuity, blank),
 * assembles the measurement value, and updates meter_reading.
 *
 * Call this from the USART RX task when a valid data frame arrives.
 *
 * frame: pointer to 12-byte RX frame (0x5A 0xA5 + 10 data bytes)
 * submode: current meter sub-mode for decimal point placement
 */
void meter_data_process_frame(const volatile uint8_t *frame, uint8_t submode);

/*
 * Check if valid meter data is available.
 */
bool meter_data_valid(void);

/*
 * Get the current meter reading value.
 * Returns 0.0 if no valid data.
 */
float meter_data_get_value(void);

/*
 * Get the display string for the current reading.
 * Returns pointer to internal buffer (e.g., "13.82", "OL", "---").
 */
const char *meter_data_get_display_str(void);

/*
 * Get the bar graph fraction (0.0 - 1.0).
 */
float meter_data_get_bar_fraction(void);

#endif /* METER_DATA_H */
