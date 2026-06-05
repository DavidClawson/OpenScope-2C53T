/*
 * OpenScope 2C53T - Meter Data Parser
 *
 * Implements BCD digit extraction from FPGA USART2 RX frames and mirrors the
 * stock DMM display-state machine documented under reverse_engineering/.
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

/* Distinct frame[6] history — see meter_data.h for semantics. */
uint8_t meter_f6_history[METER_F6_HISTORY_LEN];
uint8_t meter_f6_history_count;

meter_frame_history_t meter_frame_history[METER_FRAME_HISTORY_LEN];
uint8_t meter_frame_history_count;
uint8_t meter_frame_history_head;

typedef struct {
    uint8_t stock_mode;
    uint8_t variant;
    uint8_t format;
    uint8_t dc_state;
    uint8_t display_cmd;
    uint8_t unit_index;
    uint8_t composite_index;
} meter_stock_fsm_t;

static meter_stock_fsm_t meter_stock_fsm;

static void meter_record_history(void)
{
    meter_frame_history_t *h = &meter_frame_history[meter_frame_history_head];
    const meter_reading_t *r = &meter_reading;

    h->update_count = r->update_count;
    h->submode = r->submode;
    h->result_class = (uint8_t)r->result_class;
    h->decimal_pos = r->decimal_pos;
    h->unit_variant = r->unit_variant;
    h->status = r->dbg_frame[7];
    h->flags = r->dbg_frame[6];
    h->meas_flags = r->dbg_frame[8];
    h->additional_status = r->dbg_frame[9];
    h->extra = ((uint16_t)r->dbg_frame[10] << 8) | r->dbg_frame[11];
    h->aux_freq_hz_i10 = (uint16_t)(r->aux_freq_hz * 10.0f + 0.5f);
    h->raw_bcd = r->raw_bcd;
    strncpy(h->display_str, r->display_str, sizeof(h->display_str) - 1);
    h->display_str[sizeof(h->display_str) - 1] = '\0';
    h->unit_suffix = r->unit_suffix ? r->unit_suffix : "";
    memcpy(h->frame, r->dbg_frame, sizeof(h->frame));
    memcpy(h->raw_digits, r->dbg_raw_digits, sizeof(h->raw_digits));

    meter_frame_history_head++;
    if (meter_frame_history_head >= METER_FRAME_HISTORY_LEN) {
        meter_frame_history_head = 0;
    }
    if (meter_frame_history_count < METER_FRAME_HISTORY_LEN) {
        meter_frame_history_count++;
    }
}

static uint16_t meter_aux_freq_i10(const meter_reading_t *r)
{
    if (r->aux_freq_hz <= 0.0f) return 0;
    return (uint16_t)(r->aux_freq_hz * 10.0f + 0.5f);
}

#define METER_FINISH_FRAME() do { \
    r->valid = true; \
    r->update_count++; \
    if (old_valid != r->valid || old_submode != r->submode || \
        old_result_class != r->result_class || old_raw_bcd != r->raw_bcd || \
        old_decimal_pos != r->decimal_pos || old_negative != r->negative || \
        old_unit_variant != r->unit_variant || old_aux_freq_i10 != meter_aux_freq_i10(r) || \
        old_continuity_beep != r->continuity_beep || \
        strcmp(old_display_str, r->display_str) != 0 || \
        strcmp(old_unit_suffix, r->unit_suffix ? r->unit_suffix : "") != 0) { \
        r->display_update_count++; \
    } \
    meter_record_history(); \
} while (0)

#define METER_REJECT_FRAME() do { \
    r->value = 0.0f; \
    r->raw_bcd = 0; \
    memset(r->digits, 0, sizeof(r->digits)); \
    r->decimal_pos = 0; \
    r->negative = false; \
    strcpy(r->display_str, "---"); \
    r->unit_suffix = ""; \
    r->unit_variant = 0; \
    r->bar_fraction = 0.0f; \
    r->aux_freq_hz = 0.0f; \
    r->result_class = METER_RESULT_NONE; \
    r->continuity_beep = false; \
    r->valid = false; \
    r->update_count++; \
    if (old_valid != r->valid || old_submode != r->submode || \
        old_result_class != r->result_class || old_raw_bcd != r->raw_bcd || \
        old_decimal_pos != r->decimal_pos || old_negative != r->negative || \
        old_unit_variant != r->unit_variant || old_aux_freq_i10 != meter_aux_freq_i10(r) || \
        old_continuity_beep != r->continuity_beep || \
        strcmp(old_display_str, r->display_str) != 0 || \
        strcmp(old_unit_suffix, r->unit_suffix ? r->unit_suffix : "") != 0) { \
        r->display_update_count++; \
    } \
} while (0)

static void meter_clear_payload(meter_reading_t *r)
{
    r->value = 0.0f;
    r->raw_bcd = 0;
    memset(r->digits, 0, sizeof(r->digits));
    r->decimal_pos = 0;
    r->negative = false;
    r->unit_suffix = "";
    r->unit_variant = 0;
    r->bar_fraction = 0.0f;
    r->aux_freq_hz = 0.0f;
    r->continuity_beep = false;
}

/* ═══════════════════════════════════════════════════════════════════
 * BCD Nibble Lookup
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
 * the equivalent, extracted by simulating the stock lookup for all 256
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
 * Low-Ω band factory calibration
 *
 * Bench truth on unit #1 (2026-04-04):
 *   147 Ω ref → raw_bcd ≈ 4830/4831 → true_ohms = raw_bcd × 0.0304
 *
 * The FPGA meter IC's low-Ω range outputs raw BCD counts that need a
 * fixed linear correction. The correction factor is independent of
 * whichever dp/unit interpretation the frame's f6 byte happens to
 * suggest — for a 147 Ω input this unit reports raw 4830/4831 and
 * we apply the same factor regardless.
 *
 * Detection: resistance/continuity submode (6 or 7) with frame[6]
 * upper nibble == 0. Bench captures (2026-04-04) show the FPGA
 * rotates through 0x07, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F in this band.
 * Upper nibble 4 (0x40, 0x4B, 0x4D) is the kΩ band and is already
 * accurate without correction.
 *
 * The 0.0304 factor is per-device (factory calibrated). When the
 * SPI flash driver lands it should come from "3:/System file/..."
 * — see flash_fs.c:220 and the open question in
 * analysis_v120/fpga_h2_spi3_bulk.md about whether the stock boot's
 * 115,638-byte SPI3 bulk cal upload is what supplies these values.
 *
 * Bench data:
 *   147 Ω ref  → pre-fix: 48.36 Ω / 4.831 kΩ (flickering)
 *                post-fix: 147 Ω (stable)
 *   3.3 kΩ ref → 3.230 kΩ (unchanged, f6 upper nibble 4)
 *   10 kΩ ref  → 9.840 kΩ (unchanged)
 *   5 V DC ref → 5.008 V  (unchanged, different submode)
 * ═══════════════════════════════════════════════════════════════════ */

#define METER_CAL_LOW_OHM_FACTOR  0.0304f   /* raw_bcd × this = Ω, low band */
#define METER_CAL_KOHM_FACTOR     0.001f    /* raw_bcd × this = kΩ, mid band */

/* ═══════════════════════════════════════════════════════════════════
 * 4-digit float → string formatter (newlib-nano has no %f support)
 *
 * Writes the positive value `v` into `s` as a 4-significant-digit
 * decimal with the decimal point placed for maximum precision.
 * Caller is responsible for the leading sign. Buffer must have room
 * for up to 6 chars + null terminator.
 * ═══════════════════════════════════════════════════════════════════ */

static void format_4digit_unsigned(float v, char *s)
{
    int whole, frac, dp;

    if (v < 10.0f) {
        /* 0.000 - 9.999 → "X.XXX" */
        whole = (int)v;
        frac  = (int)((v - (float)whole) * 1000.0f + 0.5f);
        if (frac >= 1000) { whole++; frac = 0; }
        dp = 3;
    } else if (v < 100.0f) {
        /* 10.00 - 99.99 → "XX.XX" */
        whole = (int)v;
        frac  = (int)((v - (float)whole) * 100.0f + 0.5f);
        if (frac >= 100) { whole++; frac = 0; }
        dp = 2;
    } else if (v < 1000.0f) {
        /* 100.0 - 999.9 → "XXX.X" */
        whole = (int)v;
        frac  = (int)((v - (float)whole) * 10.0f + 0.5f);
        if (frac >= 10) { whole++; frac = 0; }
        dp = 1;
    } else {
        /* 1000 - 9999 → "XXXX" (clamped) */
        whole = (int)(v + 0.5f);
        if (whole > 9999) whole = 9999;
        frac  = 0;
        dp    = 0;
    }

    int pos = 0;
    if (whole >= 1000) s[pos++] = (char)('0' + (whole / 1000) % 10);
    if (whole >= 100)  s[pos++] = (char)('0' + (whole / 100)  % 10);
    if (whole >= 10)   s[pos++] = (char)('0' + (whole / 10)   % 10);
    s[pos++] = (char)('0' + whole % 10);

    if (dp > 0) {
        s[pos++] = '.';
        if (dp == 3) {
            s[pos++] = (char)('0' + (frac / 100) % 10);
            s[pos++] = (char)('0' + (frac / 10)  % 10);
            s[pos++] = (char)('0' + (frac % 10));
        } else if (dp == 2) {
            s[pos++] = (char)('0' + (frac / 10) % 10);
            s[pos++] = (char)('0' + (frac % 10));
        } else {
            s[pos++] = (char)('0' + (frac % 10));
        }
    }
    s[pos] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 * Unit suffix table
 *
 * Indexed by [submode][unit_variant]. Variant 0 is the only variant
 * the stock firmware actually uses for submodes 1-7 — see
 * reverse_engineering/analysis_v120/meter_fsm_deep_dive.md Q2: the
 * variant state is only written inside the DCV stock state machine, and it
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

static uint8_t stock_mode_from_ui_submode(uint8_t submode)
{
    switch (submode) {
    case 0: return 0;  /* DC voltage */
    case 1: return 1;  /* AC voltage */
    case 2: return 2;  /* small DC current */
    case 3: return 2;  /* large DC current, same stock formatter family */
    case 4: return 3;  /* small AC current */
    case 5: return 3;  /* large AC current, same stock formatter family */
    case 6: return 4;  /* resistance */
    case 7: return 6;  /* continuity */
    case 8: return 7;  /* diode */
    case 9: return 5;  /* extended/frequency-like formatter slot */
    default: return 0;
    }
}

static void meter_stock_fsm_reset(uint8_t stock_mode)
{
    memset(&meter_stock_fsm, 0, sizeof(meter_stock_fsm));
    meter_stock_fsm.stock_mode = stock_mode;
    meter_stock_fsm.variant = (stock_mode == 2) ? 2U : 1U;
    meter_stock_fsm.dc_state = (stock_mode == 0) ? 1U : 0U;
}

static void meter_stock_fsm_apply(uint8_t ui_submode,
                                  const volatile uint8_t *frame,
                                  const uint8_t raw_digits[4])
{
    uint8_t stock_mode = stock_mode_from_ui_submode(ui_submode);
    uint8_t flags = frame[6];
    uint8_t status = frame[7];
    uint8_t leading = raw_digits[0];
    uint8_t format_result = 0;

    if (meter_stock_fsm.stock_mode != stock_mode) {
        meter_stock_fsm_reset(stock_mode);
    }

    switch (stock_mode) {
    case 0:
        if (meter_stock_fsm.dc_state != 0) {
            if (status & 0x20U) {
                meter_stock_fsm.variant = 0;
                meter_stock_fsm.format = (status & 0x04U) ? 2U : ((flags >> 6) & 1U);
                meter_stock_fsm.dc_state = 3;
            } else if (leading & 0x02U) {
                meter_stock_fsm.variant = 1;
                meter_stock_fsm.format = (uint8_t)(1U & (uint8_t)~status);
                meter_stock_fsm.dc_state = 1;
            } else if (leading & 0x01U) {
                meter_stock_fsm.variant = 2;
                meter_stock_fsm.format = (uint8_t)(2U & (uint8_t)~status);
                meter_stock_fsm.dc_state = 1;
            } else if (status & 0x08U) {
                meter_stock_fsm.variant = 2;
                meter_stock_fsm.format = (status & 0x04U) ? 2U : ((flags >> 6) & 1U);
            } else {
                meter_stock_fsm.variant = 0;
                meter_stock_fsm.format = 0;
            }
        }
        break;

    case 1:
        meter_stock_fsm.format = (status & 0x01U) ? 0U : 1U;
        break;

    case 2:
        if (meter_stock_fsm.variant == 1) {
            meter_stock_fsm.format = (status & 0x01U) ? 0U : 1U;
            format_result = 2;
        } else if (meter_stock_fsm.variant == 2) {
            meter_stock_fsm.format = 0;
            format_result = 2;
        }
        break;

    case 3:
        if (status & 0x04U) {
            meter_stock_fsm.format = 2;
        } else if (flags & 0x20U) {
            meter_stock_fsm.format = 1;
        } else {
            meter_stock_fsm.format = 0;
        }
        format_result = 3;
        break;

    case 4:
        if (flags & 0x40U) {
            meter_stock_fsm.format = 0;
            format_result = 4;
        } else if (flags & 0x20U) {
            meter_stock_fsm.format = 1;
            format_result = 3;
        } else {
            meter_stock_fsm.format = (status & 0x01U) ? 2U : 3U;
            format_result = 4;
        }
        break;

    case 5:
        meter_stock_fsm.format = (frame[9] == 0) ? 0U : 4U;
        format_result = 5;
        break;

    case 6:
    case 7:
        if (flags & 0x10U) {
            meter_stock_fsm.format = 0;
        } else {
            meter_stock_fsm.format = (status & 0x01U) ? 1U : 2U;
        }
        break;
    }

    switch (stock_mode) {
    case 0:
        if (meter_stock_fsm.dc_state == 0) {
            meter_stock_fsm.display_cmd = 0;
            meter_stock_fsm.composite_index = 0xFF;
            meter_stock_fsm.unit_index = 0;
        } else if (meter_stock_fsm.dc_state == 1) {
            meter_stock_fsm.display_cmd = meter_stock_fsm.variant;
            meter_stock_fsm.composite_index = meter_stock_fsm.format;
            if (meter_stock_fsm.variant == 1 || meter_stock_fsm.variant == 2) {
                meter_stock_fsm.unit_index = meter_stock_fsm.variant;
            }
        } else {
            meter_stock_fsm.display_cmd = (meter_stock_fsm.dc_state == 2 &&
                                          meter_stock_fsm.variant == 2) ? 3U : 5U;
            meter_stock_fsm.composite_index = meter_stock_fsm.format + 2U;
            meter_stock_fsm.unit_index = meter_stock_fsm.display_cmd;
        }
        break;

    case 1:
        meter_stock_fsm.display_cmd = meter_stock_fsm.variant;
        meter_stock_fsm.composite_index = meter_stock_fsm.format;
        if (meter_stock_fsm.variant == 1 || meter_stock_fsm.variant == 2) {
            meter_stock_fsm.unit_index = meter_stock_fsm.variant;
        }
        break;

    case 2:
        meter_stock_fsm.display_cmd = format_result;
        meter_stock_fsm.unit_index = (meter_stock_fsm.variant == 2) ? 3U : 4U;
        meter_stock_fsm.composite_index = meter_stock_fsm.format +
                                          ((meter_stock_fsm.variant == 2) ? 2U : 0U);
        break;

    case 3:
        meter_stock_fsm.display_cmd = format_result;
        meter_stock_fsm.unit_index = 5;
        meter_stock_fsm.composite_index = meter_stock_fsm.format + 2U;
        break;

    case 4:
        meter_stock_fsm.display_cmd = format_result;
        meter_stock_fsm.unit_index = 6;
        meter_stock_fsm.composite_index = meter_stock_fsm.format + 5U;
        break;

    case 5:
        meter_stock_fsm.display_cmd = format_result;
        meter_stock_fsm.unit_index = 7;
        meter_stock_fsm.composite_index = meter_stock_fsm.format + 9U;
        break;

    case 6:
    case 7:
        meter_stock_fsm.display_cmd = (meter_stock_fsm.variant == 2) ? 9U : 8U;
        meter_stock_fsm.unit_index = (meter_stock_fsm.variant == 2) ? 9U : 8U;
        meter_stock_fsm.composite_index = meter_stock_fsm.format + 10U;
        break;
    }
}

static const char *unit_suffix_from_stock(uint8_t ui_submode, uint8_t unit_index)
{
    if (ui_submode == 1) return "V";
    if (ui_submode == 2 || ui_submode == 4) return "mA";
    if (ui_submode == 3 || ui_submode == 5) return "A";
    if (ui_submode == 7) return "Ohm";
    if (ui_submode == 8) return "V";
    if (ui_submode == 9) return "nF";

    switch (unit_index) {
    case 0:  return "V";
    case 1:  return "mV";
    case 2:  return "mV";
    case 3:  return "A";
    case 4:  return "mA";
    case 5:  return "mA";
    case 6:  return "Ohm";
    case 7:  return "Hz";
    case 8:  return "kOhm";
    case 9:  return "MOhm";
    case 10: return "kHz";
    case 11: return "MHz";
    default: break;
    }
    return (ui_submode < 10) ? unit_suffix_table[ui_submode][0] : "";
}

static uint8_t decimal_pos_from_stock(uint8_t ui_submode,
                                      const meter_stock_fsm_t *fsm)
{
    uint8_t fmt = fsm->format;

    switch (stock_mode_from_ui_submode(ui_submode)) {
    case 0:
    case 1:
        if (fmt == 0) return 1;
        if (fmt == 1) return 3;
        if (fmt == 2) return 2;
        return default_decimal_pos[ui_submode];
    case 2:
    case 3:
        if (ui_submode == 2 || ui_submode == 4) return 2;
        if (ui_submode == 3 || ui_submode == 5) return 1;
        if (fsm->unit_index == 3) return 1;
        if (fmt >= 2) return 1;
        return 2;
    case 4:
        if (fsm->unit_index == 8 || fsm->unit_index == 9) return 2;
        if (fmt == 0) return 2;
        if (fmt == 1) return 1;
        if (fmt == 2) return 2;
        return 3;
    case 5:
        return (ui_submode == 9) ? 3U : (uint8_t)(fmt > 3 ? 3 : fmt);
    case 6:
    case 7:
        return (fmt == 0) ? 0U : ((fmt == 1) ? 1U : 3U);
    default:
        return default_decimal_pos[ui_submode];
    }
}

static bool apply_verified_frame_range(meter_reading_t *r,
                                       uint8_t submode,
                                       uint8_t f6,
                                       const volatile uint8_t *frame,
                                       uint16_t extra)
{
    if (submode == 0 &&
        frame[8] == 0x02 && frame[9] == 0x00 &&
        extra >= 45 && extra <= 65) {
        r->decimal_pos = 3;
        r->unit_variant = 0;
        r->unit_suffix = "V";
        return true;
    }

    if (submode == 0) {
        r->decimal_pos = 1;
        r->unit_variant = 0;
        r->unit_suffix = "V";
        return true;
    }

    if (submode == 1 &&
        (f6 == 0x0A || f6 == 0x0B || f6 == 0x0D || f6 == 0x0F)) {
        r->decimal_pos = 3;
        r->unit_variant = 0;
        r->unit_suffix = "V";
        return true;
    }

    return false;
}

static bool frame_is_voltage_payload(uint8_t submode,
                                     const volatile uint8_t *frame)
{
    if (submode == 0 || submode == 1) return false;
    return frame[8] == 0x02 && frame[9] == 0x00;
}

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
    memset(&meter_stock_fsm, 0, sizeof(meter_stock_fsm));
    memset(meter_frame_history, 0, sizeof(meter_frame_history));
    memset(meter_f6_history, 0, sizeof(meter_f6_history));
    strcpy(meter_reading.display_str, "---");
    meter_reading.unit_suffix = "";  /* Never NULL — UI can render directly. */
    meter_reading.unit_variant = 0;
    meter_f6_history_count = 0;
    meter_frame_history_count = 0;
    meter_frame_history_head = 0;
    meter_stock_fsm.stock_mode = 0xFF;
}

void meter_data_invalidate(uint8_t submode)
{
    meter_reading_t *r = &meter_reading;

    r->value = 0.0f;
    r->raw_bcd = 0;
    memset(r->digits, 0, sizeof(r->digits));
    r->decimal_pos = 0;
    r->negative = false;
    strcpy(r->display_str, "---");
    r->unit_suffix = "";
    r->unit_variant = 0;
    r->bar_fraction = 0.0f;
    r->aux_freq_hz = 0.0f;
    r->result_class = METER_RESULT_NONE;
    r->is_ac = false;
    r->is_auto_range = false;
    r->is_hold = false;
    r->submode = submode;
    r->probe_type = 0;
    r->range_indicator = 0;
    r->range_cmd = 0;
    r->continuity_beep = false;
    r->valid = false;
    r->display_update_count++;
    memset(r->dbg_frame, 0, sizeof(r->dbg_frame));
    memset(r->dbg_nibbles, 0, sizeof(r->dbg_nibbles));
    memset(r->dbg_raw_digits, 0, sizeof(r->dbg_raw_digits));

    meter_stock_fsm_reset(stock_mode_from_ui_submode(submode));
    r->update_count++;
}

void meter_data_process_frame(const volatile uint8_t *frame, uint8_t submode)
{
    meter_reading_t *r = &meter_reading;

    /* Validate header */
    if (frame[0] != 0x5A || frame[1] != 0xA5) return;

    bool old_valid = r->valid;
    uint8_t old_submode = r->submode;
    meter_result_class_t old_result_class = r->result_class;
    int old_raw_bcd = r->raw_bcd;
    uint8_t old_decimal_pos = r->decimal_pos;
    bool old_negative = r->negative;
    uint8_t old_unit_variant = r->unit_variant;
    uint16_t old_aux_freq_i10 = meter_aux_freq_i10(r);
    bool old_continuity_beep = r->continuity_beep;
    char old_display_str[sizeof(r->display_str)];
    char old_unit_suffix[12];
    strcpy(old_display_str, r->display_str);
    strncpy(old_unit_suffix, r->unit_suffix ? r->unit_suffix : "",
            sizeof(old_unit_suffix) - 1);
    old_unit_suffix[sizeof(old_unit_suffix) - 1] = '\0';

    /* Save raw frame for debug display */
    for (int i = 0; i < 12; i++) r->dbg_frame[i] = frame[i];
    r->submode = submode;
    r->continuity_beep = false;
    r->aux_freq_hz = 0.0f;

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

    /* Parse status flags from byte [7]. The stock parser treats this as the
     * status byte for data frames and as an integrity marker for echo frames. */
    uint8_t status = frame[7];
    r->is_ac = (status & (1 << 2)) != 0;
    r->is_auto_range = (status & (1 << 3)) != 0;
    r->negative = ((submode == 0 || submode == 2 || submode == 3) &&
                   (status & (1 << 0)) != 0);

    /* Parse flags from byte [6] */
    uint8_t flags = frame[6];
    r->is_hold = (flags & (1 << 6)) != 0;
    uint16_t extra = ((uint16_t)frame[10] << 8) | frame[11];

    /* Record distinct frame[6] values for Phase 1 diagnostic.
     * Push-front with dedup: if the current value is already in the
     * history, leave it alone. Otherwise insert at slot 0 and shift
     * older entries down, dropping the oldest. */
    {
        bool seen = false;
        for (int k = 0; k < meter_f6_history_count; k++) {
            if (meter_f6_history[k] == flags) { seen = true; break; }
        }
        if (!seen) {
            int n = meter_f6_history_count;
            if (n < METER_F6_HISTORY_LEN) n++;
            for (int k = n - 1; k > 0; k--) {
                meter_f6_history[k] = meter_f6_history[k - 1];
            }
            meter_f6_history[0] = flags;
            meter_f6_history_count = (uint8_t)n;
        }
    }

    /* Range indicators from frame[6] bits 4-5. */
    r->range_indicator = (flags >> 4) & 0x03;

    /* Legacy probe classification kept for diagnostics and UI state. */
    if (status & (1 << 1)) {
        r->probe_type = 1;
    } else if (status & (1 << 0)) {
        r->probe_type = 2;
    } else if (status & (1 << 3)) {
        r->probe_type = 2;
    } else {
        r->probe_type = 0;
    }

    /* Legacy "range command" field kept for old debug output. Stock DMM
     * firmware renders autonomous FPGA/meter-IC range feedback; it does not
     * use this value as a range-select command. */
    r->range_cmd = r->probe_type;

    /* --- Special value detection --- */

    if (frame_is_voltage_payload(submode, frame)) {
        METER_REJECT_FRAME();
        return;
    }

    /* Overload: "OL" */
    if (digit0 == 0x0A && digit1 == 0x0B) {
        meter_clear_payload(r);
        r->result_class = METER_RESULT_OVERLOAD;
        strcpy(r->display_str, "OL");
        r->bar_fraction = 1.0f;
        METER_FINISH_FRAME();
        return;
    }

    /* Blank display */
    if (digit0 == 0x10 && digit1 == 0x10) {
        meter_clear_payload(r);
        r->result_class = METER_RESULT_BLANK;
        strcpy(r->display_str, "---");
        METER_FINISH_FRAME();
        return;
    }

    /* Partial blank */
    if (digit0 == 0x10 && digit1 == 0x11) {
        meter_clear_payload(r);
        r->result_class = METER_RESULT_BLANK;
        strcpy(r->display_str, "---");
        METER_FINISH_FRAME();
        return;
    }

    /* Continuity detection */
    if (digit1 == 0x12 && digit2 == 0x0A && digit3 == 5) {
        meter_clear_payload(r);
        r->result_class = METER_RESULT_CONTINUITY;
        r->continuity_beep = true;
        r->unit_suffix = "Ohm";
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
        METER_FINISH_FRAME();
        return;
    }

    /* Invalid digits */
    if (digit0 == 0xFF || digit1 == 0xFF || digit2 == 0xFF || digit3 == 0xFF) {
        meter_clear_payload(r);
        r->result_class = METER_RESULT_INVALID;
        strcpy(r->display_str, "ERR");
        METER_FINISH_FRAME();
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

    uint8_t raw_digit_codes[4] = { digit0, digit1, digit2, digit3 };
    uint8_t f6 = flags;

    meter_stock_fsm_apply(submode, frame, raw_digit_codes);
    r->decimal_pos = decimal_pos_from_stock(submode, &meter_stock_fsm);
    r->unit_variant = meter_stock_fsm.variant;
    r->unit_suffix = unit_suffix_from_stock(submode, meter_stock_fsm.unit_index);
    (void)apply_verified_frame_range(r, submode, f6, frame, extra);

    if ((submode == 0 || submode == 1) &&
        frame[8] == 0x02 && frame[9] == 0x00 &&
        extra >= 45 && extra <= 65) {
        r->aux_freq_hz = (float)extra;
    }

    /* Format the display string and compute float value */
    r->result_class = METER_RESULT_NORMAL;
    r->continuity_beep = false;
    format_reading(r, submode);

    /* ── Resistance band factory calibration override ──
     *
     * For resistance/continuity submodes, the FPGA meter IC rotates
     * through multiple frame variants per measurement, each claiming
     * a different dp/unit. The "correct" interpretation depends on
     * which specific frame[6] we happen to catch — without override,
     * the display flickers between e.g. "9.821 kOhm" and "98.24 kOhm"
     * for the SAME 10 kΩ resistor.
     *
     * The FIX is to compute resistance from raw_bcd at the band level,
     * ignoring the per-frame dp hint entirely:
     *
     *   Low-Ω band  (frame[6] upper nibble 0): value = raw_bcd × 0.0304 Ω
     *   kΩ    band  (frame[6] upper nibble 4): value = raw_bcd × 0.001  kΩ
     *
     * Bench data (2026-04-04, unit #1):
     *   147 Ω ref  → raw ≈ 4824 → 4824 × 0.0304 = 146.6 Ω  ✓
     *   3.3 kΩ ref → raw ≈ 3230 → 3230 × 0.001  = 3.230 kΩ ✓
     *   10 kΩ ref  → raw ≈ 9821 → 9821 × 0.001  = 9.821 kΩ ✓
     *
     * The 0.0304 factor is per-device (factory calibrated). When the
     * SPI flash driver lands it should come from "3:/System file/..."
     * — see flash_fs.c:220. The 0.001 kΩ factor is a geometric
     * identity (raw counts already expressed in Ω, shifted to kΩ) and
     * should be stable across units.
     *
     * Higher bands (MΩ, autorange to 200 kΩ / 2 MΩ) are not yet
     * characterized — those will need additional upper-nibble cases.
     *
     * We leave raw_bcd, decimal_pos, and digits[] untouched so the
     * debug overlay at meter_ui.c:948 still shows the FPGA's raw
     * pre-cal report. UI code reads `value` and `display_str` for
     * the final numbers.
     */
    if (submode == 6 || submode == 7) {
        float       scale = 0.0f;
        const char *unit  = NULL;

        switch (f6 & 0xF0) {
        case 0x00:  scale = METER_CAL_LOW_OHM_FACTOR; unit = "Ohm";  break;
        case 0x40:  scale = METER_CAL_KOHM_FACTOR;    unit = "kOhm"; break;
        default:    break;  /* Unknown band — leave format_reading's output */
        }

        if (scale != 0.0f) {
            float v = (float)r->raw_bcd * scale;
            if (r->negative) v = -v;
            r->value       = v;
            r->unit_suffix = unit;

            char *s = r->display_str;
            int   pos = 0;
            float av  = v;
            if (av < 0.0f) { s[pos++] = '-'; av = -av; }
            format_4digit_unsigned(av, s + pos);

            /* Recompute bar graph fraction from the corrected value. */
            float abs_v = v < 0.0f ? -v : v;
            float full_scale = (submode < 10) ? bar_full_scale[submode] : 1000.0f;
            r->bar_fraction = abs_v / full_scale;
            if (r->bar_fraction > 1.0f) r->bar_fraction = 1.0f;
        }
    }

    METER_FINISH_FRAME();
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
