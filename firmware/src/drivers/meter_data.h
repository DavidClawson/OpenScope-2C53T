/*
 * OpenScope 2C53T - Meter Data Parser
 *
 * Parses FPGA USART2 RX frames to extract multimeter readings.
 * The FPGA sends 12-byte data frames (0x5A 0xA5 header + 10 data bytes)
 * containing BCD-encoded measurement digits and status flags.
 *
 * Based on RE analysis:
 *   - reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md
 *   - Functions: meter_data_processor (0x08036AC0) and
 *                meter_mode_handler   (0x080371B0)
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

/* ═══════════════════════════════════════════════════════════════════
 * Parsed Meter Reading
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Parsed measurement value */
    float    value;              /* Scaled measurement value */
    int      raw_bcd;            /* Raw 4-digit BCD integer (0-9999) */
    uint8_t  digits[4];          /* Individual BCD digits */
    uint8_t  decimal_pos;        /* Decimal point position (0=none, 1-3) */
    bool     negative;           /* Negative polarity */

    /* Display string (pre-formatted for UI) */
    char     display_str[16];    /* e.g., "-13.82", "OL", "---" */

    /* Bar graph fraction (0.0 - 1.0) */
    float    bar_fraction;

    /* Classification */
    meter_result_class_t result_class;

    /* Status flags from RX frame byte [7] */
    bool     is_ac;              /* AC mode active */
    bool     is_auto_range;      /* Auto-range enabled */
    bool     is_hold;            /* Hold mode active */

    /* Continuity buzzer state */
    bool     continuity_beep;    /* Should buzzer sound */

    /* Validity */
    bool     valid;              /* At least one successful parse */
    uint32_t update_count;       /* Incremented on each new reading */

} meter_reading_t;

/* Global meter reading (defined in meter_data.c) */
extern meter_reading_t meter_reading;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Initialize meter data parser state.
 * Call once at startup.
 */
void meter_data_init(void);

/*
 * Process a complete FPGA USART2 RX data frame (12 bytes).
 * Extracts BCD digits, detects special codes (OL, continuity, blank),
 * assembles the measurement value, and updates meter_reading.
 *
 * Call this from the USART RX task when a valid data frame arrives.
 *
 * frame: pointer to 12-byte RX frame (0x5A 0xA5 + 10 data bytes)
 * submode: current meter sub-mode (0-9) for decimal point placement
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
