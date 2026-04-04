/*
 * OpenScope 2C53T - Meter Data Parser
 *
 * Implements BCD digit extraction from FPGA USART2 RX frames,
 * following the stock firmware's meter_data_processor (0x08036AC0)
 * and meter_mode_handler (0x080371B0).
 *
 * USART2 RX data frame format (12 bytes):
 *   [0] = 0x5A  (header byte 1)
 *   [1] = 0xA5  (header byte 2)
 *   [2]-[6] = packed BCD nibble pairs (measurement digits)
 *   [7] = status flags (AC, auto-range, overload, polarity)
 *   [8]-[9] = additional status
 *   [10]-[11] = extra data (range info)
 *
 * BCD extraction: digits are encoded as cross-byte nibble pairs:
 *   digit0 = lookup((rx[2] & 0xF0) | (rx[3] & 0x0F))
 *   digit1 = lookup((rx[3] & 0xF0) | (rx[4] & 0x0F))
 *   digit2 = lookup((rx[4] & 0xF0) | (rx[5] & 0x0F))
 *   digit3 = lookup((rx[5] & 0xF0) | (rx[6] & 0x0F))
 */

#include "meter_data.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Global State
 * ═══════════════════════════════════════════════════════════════════ */

meter_reading_t meter_reading;

/* ═══════════════════════════════════════════════════════════════════
 * BCD Nibble Lookup — extracted from stock firmware FUN_08033EF8
 *
 * The FPGA contains a meter IC core (likely FS9922 or similar Chinese
 * multimeter ASIC) that outputs 7-segment LCD drive signals rather
 * than clean BCD. The cross-byte nibble pairs encode which LCD
 * segments are lit, using this scrambled bit mapping:
 *
 *   input bit 0 → segment d (bottom)
 *   input bit 1 → segment c (lower right)
 *   input bit 2 → segment g (middle bar)
 *   input bit 3 → segment b (upper right)
 *   input bit 4 → (unused, masked off by AND 0xEF)
 *   input bit 5 → segment e (lower left)
 *   input bit 6 → segment f (upper left)
 *   input bit 7 → segment a (top)
 *
 * The stock firmware reverses this encoding back to digit values
 * using a function with TBB/TBH jump tables. This lookup table is
 * the equivalent, extracted by simulating FUN_08033EF8 for all 256
 * input values against the V1.2.0 firmware binary.
 *
 * Output codes: 0-9 = BCD digits, 0x0A = OL_hi, 0x0B = OL_lo,
 *   0x0C/0x0D/0x0E/0x0F = special, 0x10 = blank, 0x11 = partial,
 *   0x12 = continuity, 0x13 = mode change, 0x14 = special,
 *   0xFF = invalid/unmapped.
 *
 * Since bit 4 is masked, rows 0x1n and 0x0n are identical, etc.
 * ═══════════════════════════════════════════════════════════════════ */

static const uint8_t bcd_lookup[256] = {
    0x10, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00-0x0F */
    0x10, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x10-0x1F */
    0xFF, 0xFF, 0xFF, 0x0B, 0x14, 0xFF, 0xFF, 0x0D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x20-0x2F */
    0xFF, 0xFF, 0xFF, 0x0B, 0x14, 0xFF, 0xFF, 0x0D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x30-0x3F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x04, 0xFF,  /* 0x40-0x4F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x04, 0xFF,  /* 0x50-0x5F */
    0xFF, 0x0E, 0xFF, 0xFF, 0xFF, 0x0C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x60-0x6F */
    0xFF, 0x0E, 0xFF, 0xFF, 0xFF, 0x0C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x70-0x7F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0x03,  /* 0x80-0x8F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0x03,  /* 0x90-0x9F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0xFF, 0xFF,  /* 0xA0-0xAF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0xFF, 0xFF,  /* 0xB0-0xBF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x09,  /* 0xC0-0xCF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x09,  /* 0xD0-0xDF */
    0xFF, 0x11, 0xFF, 0xFF, 0xFF, 0x13, 0xFF, 0x06, 0xFF, 0xFF, 0xFF, 0x00, 0x12, 0xFF, 0x0A, 0x08,  /* 0xE0-0xEF */
    0xFF, 0x11, 0xFF, 0xFF, 0xFF, 0x13, 0xFF, 0x06, 0xFF, 0xFF, 0xFF, 0x00, 0x12, 0xFF, 0x0A, 0x08   /* 0xF0-0xFF */
};

static uint8_t bcd_nibble_lookup(uint8_t combined)
{
    return bcd_lookup[combined];
}

/* ═══════════════════════════════════════════════════════════════════
 * Decimal point placement per sub-mode
 *
 * Based on the result formatting switch in meter_data_processor:
 *   Sub-mode 0 (DC V):   X.XXX  → decimal at position 1 (after 1st digit)
 *   Sub-mode 1 (AC V):   XX.XX  → decimal at position 2
 *   Sub-mode 2 (DC mA):  XX.XX  → decimal at position 2
 *   Sub-mode 3 (DC A):   X.XXX  → decimal at position 1
 *   Sub-mode 4 (AC mA):  XX.XX  → decimal at position 2
 *   Sub-mode 5 (AC A):   X.XXX  → decimal at position 1
 *   Sub-mode 6 (Ohm):    X.XXX  → decimal at position 1
 *   Sub-mode 7 (Cont):   XXX.X  → decimal at position 3
 *   Sub-mode 8 (Diode):  X.XXX  → decimal at position 1
 *   Sub-mode 9 (Cap):    XX.XX  → decimal at position 2
 *
 * The actual decimal position depends on the auto-range state,
 * but these are the defaults for the most common range.
 * ═══════════════════════════════════════════════════════════════════ */

/* Default decimal position per submode (index 0 = no decimal, 1-3 = after nth digit).
 * Empirically tuned from hardware readings:
 *   DCV (0): 1-10V range, raw 9899 → 9.899 V, decimal after digit 1
 *   ACV (1): similar
 *   Resistance (6): 20k range, raw 9899 → 98.99 kΩ, decimal after digit 2
 *   Continuity (7): 200Ω range, raw 16 → 1.6 Ω, decimal after digit 3
 *   Diode (8): 2V range, raw 623 → 0.623 V, decimal after digit 1
 *   Capacitance (9): 200nF range, raw 1034 → 103.4 nF, decimal after digit 3
 */
static const uint8_t default_decimal_pos[10] = {
    1,  /* 0: DCV       — 9.899 V */
    2,  /* 1: ACV       — 98.99 V */
    2,  /* 2: DCA (mA)  — 98.99 mA */
    1,  /* 3: DCA (A)   — 9.899 A */
    2,  /* 4: ACA (mA)  — 98.99 mA */
    1,  /* 5: ACA (A) or Frequency */
    2,  /* 6: Resistance— 98.99 kΩ */
    3,  /* 7: Continuity— 198.9 Ω */
    1,  /* 8: Diode     — 0.623 V */
    3,  /* 9: Capacitance— 198.9 nF */
};

/* Full-scale values per sub-mode (for bar graph calculation) */
static const float bar_full_scale[10] = {
    20.0f,    /* DC V: 20V */
    200.0f,   /* AC V: 200V */
    200.0f,   /* DC mA: 200mA */
    10.0f,    /* DC A: 10A */
    200.0f,   /* AC mA: 200mA */
    10.0f,    /* AC A: 10A */
    20.0f,    /* Ohm: 20kOhm */
    200.0f,   /* Cont: 200 Ohm */
    2.0f,     /* Diode: 2V */
    200.0f,   /* Cap: 200nF */
};

/* ═══════════════════════════════════════════════════════════════════
 * Unit suffix table
 *
 * Indexed by [submode][unit_variant]. Variant 0 is the only variant
 * the stock firmware actually uses for submodes 1-7 — see
 * reverse_engineering/analysis_v120/meter_fsm_deep_dive.md Q2: the
 * variant state (stock `DAT_2000102e`) is only written inside the
 * DCV FSM case (case 0 of meter_mode_handler at 0x080371B0), and it
 * persists from there into whatever submode runs next. Other modes
 * read it as a side-effect but never drive it.
 *
 * Variants 1/2 for submodes 1-7 are placeholder strings that will
 * never be selected at runtime today. They're left here as
 * documentation targets for when we wire up per-submode range
 * feedback in a future phase.
 *
 * Strings are ASCII-only so the font renderer doesn't need Greek
 * mu/ohm glyphs.
 * ═══════════════════════════════════════════════════════════════════ */

static const char * const unit_suffix_table[10][3] = {
    /*                v0       v1       v2    */
    /* 0 DCV      */ { "V",    "mV",    "mV"   },
    /* 1 ACV      */ { "V",    "mV",    "mV"   },
    /* 2 DCA(mA)  */ { "mA",   "uA",    "mA"   },
    /* 3 DCA(A)   */ { "A",    "A",     "A"    },
    /* 4 ACA(mA)  */ { "mA",   "uA",    "mA"   },
    /* 5 Freq/ACA */ { "Hz",   "kHz",   "MHz"  },
    /* 6 Ohm      */ { "Ohm",  "kOhm",  "MOhm" },
    /* 7 Cont     */ { "Ohm",  "Ohm",   "Ohm"  },
    /* 8 Diode    */ { "V",    "V",     "V"    },
    /* 9 Cap      */ { "nF",   "uF",    "uF"   },
};

/* ═══════════════════════════════════════════════════════════════════
 * Format value into display string
 * ═══════════════════════════════════════════════════════════════════ */

static void format_reading(meter_reading_t *r, uint8_t submode)
{
    char *s = r->display_str;
    int pos = 0;

    if (r->negative) {
        s[pos++] = '-';
    }

    uint8_t dec = r->decimal_pos;

    for (int i = 0; i < 4; i++) {
        if (i == (int)dec && dec > 0 && dec < 4) {
            s[pos++] = '.';
        }
        s[pos++] = '0' + r->digits[i];
    }
    s[pos] = '\0';

    /* Strip leading zeros (but keep at least one digit before decimal) */
    /* e.g., "0.623" stays, but "0047" becomes "47" */
    if (dec == 0) {
        /* No decimal point — strip leading zeros */
        int start = r->negative ? 1 : 0;
        int first_nonzero = start;
        while (first_nonzero < pos - 1 && s[first_nonzero] == '0') {
            first_nonzero++;
        }
        if (first_nonzero > start) {
            memmove(s + start, s + first_nonzero, pos - first_nonzero + 1);
        }
    }

    /* Calculate float value from BCD */
    float divisor = 1.0f;
    for (int i = 0; i < (4 - (int)dec); i++) {
        divisor *= 10.0f;
    }
    r->value = (float)r->raw_bcd / divisor;
    if (r->negative) r->value = -r->value;

    /* Bar graph fraction */
    float abs_val = r->value < 0 ? -r->value : r->value;
    float full_scale = (submode < 10) ? bar_full_scale[submode] : 1000.0f;
    r->bar_fraction = abs_val / full_scale;
    if (r->bar_fraction > 1.0f) r->bar_fraction = 1.0f;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void meter_data_init(void)
{
    memset(&meter_reading, 0, sizeof(meter_reading));
    strcpy(meter_reading.display_str, "---");
    meter_reading.unit_suffix = "";  /* Never NULL — UI can render directly. */
    meter_reading.unit_variant = 0;
}

void meter_data_process_frame(const volatile uint8_t *frame, uint8_t submode)
{
    meter_reading_t *r = &meter_reading;

    /* Validate header */
    if (frame[0] != 0x5A || frame[1] != 0xA5) return;

    /* Save raw frame for debug display */
    for (int i = 0; i < 12; i++) r->dbg_frame[i] = frame[i];

    /* Extract cross-byte nibble pairs */
    uint8_t b2 = frame[2], b3 = frame[3], b4 = frame[4];
    uint8_t b5 = frame[5], b6 = frame[6];

    uint8_t nib0 = (b2 & 0xF0) | (b3 & 0x0F);
    uint8_t nib1 = (b3 & 0xF0) | (b4 & 0x0F);
    uint8_t nib2 = (b4 & 0xF0) | (b5 & 0x0F);
    uint8_t nib3 = (b5 & 0xF0) | (b6 & 0x0F);

    /* Save pre-lookup nibbles for debug */
    r->dbg_nibbles[0] = nib0;
    r->dbg_nibbles[1] = nib1;
    r->dbg_nibbles[2] = nib2;
    r->dbg_nibbles[3] = nib3;

    uint8_t digit0 = bcd_nibble_lookup(nib0);
    uint8_t digit1 = bcd_nibble_lookup(nib1);
    uint8_t digit2 = bcd_nibble_lookup(nib2);
    uint8_t digit3 = bcd_nibble_lookup(nib3);

    /* Save post-lookup digit codes for debug */
    r->dbg_raw_digits[0] = digit0;
    r->dbg_raw_digits[1] = digit1;
    r->dbg_raw_digits[2] = digit2;
    r->dbg_raw_digits[3] = digit3;

    /* Parse status flags from byte [7]
     * Based on meter_mode_handler FSM at 0x080371B0 in stock firmware.
     * The status byte encodes polarity, AC/DC, overload, and range info. */
    uint8_t status = frame[7];
    r->is_ac = (status & (1 << 2)) != 0;
    r->is_auto_range = (status & (1 << 3)) != 0;
    r->negative = (status & (1 << 0)) != 0;

    /* Parse flags from byte [6] */
    uint8_t flags = frame[6];
    r->is_hold = (flags & (1 << 6)) != 0;

    /* Range indicators from frame[6] bits 4-5 (stock firmware FSM case 4) */
    r->range_indicator = (flags >> 4) & 0x03;

    /* Determine probe_type from status bits (stock firmware FSM case 0).
     * This drives the calibration coefficient selection in fpga_state_update.
     *   status bit 1 set → probe_type = 1
     *   status bit 0 set (alone) → probe_type = 2
     *   status bit 3 set (auto-range) → probe_type = 2
     *   otherwise → probe_type = 0
     */
    if (status & (1 << 1)) {
        r->probe_type = 1;
    } else if (status & (1 << 0)) {
        r->probe_type = 2;
    } else if (status & (1 << 3)) {
        r->probe_type = 2;
    } else {
        r->probe_type = 0;
    }

    /* Legacy "range command" parameter — kept for API compatibility.
     *
     * Earlier notes claimed stock firmware "sends cmds 0x1B/0x1C/0x1E
     * as FPGA range-select commands" from FUN_080028e0. That was wrong:
     * see reverse_engineering/analysis_v120/meter_fsm_deep_dive.md Q3.
     * Those bytes are display-queue dispatch codes (indices into the
     * display task function table at 0x804be74), not FPGA commands.
     * Stock firmware does NOT implement firmware-driven auto-ranging;
     * the FPGA meter IC auto-ranges autonomously and the MCU just
     * renders whatever frame it receives. */
    r->range_cmd = r->probe_type;

    /* --- Special value detection --- */

    /* Overload: "OL" */
    if (digit0 == 0x0A && digit1 == 0x0B) {
        r->result_class = METER_RESULT_OVERLOAD;
        strcpy(r->display_str, "OL");
        r->value = 0.0f;
        r->bar_fraction = 1.0f;
        r->continuity_beep = false;
        r->valid = true;
        r->update_count++;
        return;
    }

    /* Blank display */
    if (digit0 == 0x10 && digit1 == 0x10) {
        r->result_class = METER_RESULT_BLANK;
        strcpy(r->display_str, "---");
        r->value = 0.0f;
        r->bar_fraction = 0.0f;
        r->continuity_beep = false;
        r->valid = true;
        r->update_count++;
        return;
    }

    /* Partial blank */
    if (digit0 == 0x10 && digit1 == 0x11) {
        r->result_class = METER_RESULT_BLANK;
        strcpy(r->display_str, "---");
        r->value = 0.0f;
        r->bar_fraction = 0.0f;
        r->valid = true;
        r->update_count++;
        return;
    }

    /* Continuity detection */
    if (digit1 == 0x12 && digit2 == 0x0A && digit3 == 5) {
        r->result_class = METER_RESULT_CONTINUITY;
        r->continuity_beep = true;
        /* Still try to show a value if digit0 is valid */
        if (digit0 <= 9) {
            r->digits[0] = digit0;
            r->digits[1] = 0;
            r->digits[2] = 0;
            r->digits[3] = 0;
            r->raw_bcd = digit0;
            r->decimal_pos = 0;
            format_reading(r, submode);
        } else {
            strcpy(r->display_str, "CONT");
            r->value = 0.0f;
        }
        r->valid = true;
        r->update_count++;
        return;
    }

    /* Invalid digits */
    if (digit0 == 0xFF || digit1 == 0xFF || digit2 == 0xFF || digit3 == 0xFF) {
        r->result_class = METER_RESULT_INVALID;
        strcpy(r->display_str, "ERR");
        r->value = 0.0f;
        r->bar_fraction = 0.0f;
        r->valid = true;
        r->update_count++;
        return;
    }

    /* --- Normal BCD value --- */

    /* Mask off special code high bits for assembly */
    uint8_t d0 = (digit0 >= 0x10) ? (digit0 - 0x10) : digit0;
    uint8_t d1 = (digit1 >= 0x10) ? (digit1 - 0x10) : digit1;
    uint8_t d2 = (digit2 >= 0x10) ? (digit2 - 0x10) : digit2;
    uint8_t d3 = (digit3 >= 0x10) ? (digit3 - 0x10) : digit3;

    /* Clamp individual digits to valid BCD range */
    if (d0 > 9) d0 = 0;
    if (d1 > 9) d1 = 0;
    if (d2 > 9) d2 = 0;
    if (d3 > 9) d3 = 0;

    r->digits[0] = d0;
    r->digits[1] = d1;
    r->digits[2] = d2;
    r->digits[3] = d3;
    r->raw_bcd = d0 * 1000 + d1 * 100 + d2 * 10 + d3;

    /* Decimal position: the FSM port from stock meter_mode_handler
     * (decode_decimal_from_fsm) tested worse than the static default
     * on hardware — bit semantics of frame[6]/[7] don't match what
     * the decomp suggested. Reverted to the static default pending a
     * proper frame capture of real auto-range transitions. The FSM
     * function is retained below but unused; see the comment there. */
    r->decimal_pos = (submode < 10) ? default_decimal_pos[submode] : 1;

    /* Unit suffix: populate from the static table at variant 0. Live
     * auto-range variant updates will be wired once the FSM is fixed. */
    r->unit_variant = 0;
    r->unit_suffix = (submode < 10)
                     ? unit_suffix_table[submode][0]
                     : "";

    /* Format the display string and compute float value */
    r->result_class = METER_RESULT_NORMAL;
    r->continuity_beep = false;
    format_reading(r, submode);

    r->valid = true;
    r->update_count++;
}

bool meter_data_valid(void)
{
    return meter_reading.valid;
}

float meter_data_get_value(void)
{
    return meter_reading.value;
}

const char *meter_data_get_display_str(void)
{
    return meter_reading.display_str;
}

float meter_data_get_bar_fraction(void)
{
    return meter_reading.bar_fraction;
}
