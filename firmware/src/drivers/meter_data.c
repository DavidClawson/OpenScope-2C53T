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
 *   [10]-[11] = extra data (auxiliary meter status/frequency candidates)
 *
 * BCD extraction: digits are encoded as cross-byte nibble pairs:
 *   digit0 = lookup((rx[2] & 0xF0) | (rx[3] & 0x0F))
 *   digit1 = lookup((rx[3] & 0xF0) | (rx[4] & 0x0F))
 *   digit2 = lookup((rx[4] & 0xF0) | (rx[5] & 0x0F))
 *   digit3 = lookup((rx[5] & 0xF0) | (rx[6] & 0x0F))
 */

#include "meter_data.h"
#include "fpga_meter_plan.h"
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

static volatile uint32_t meter_reading_seq;

static void meter_data_barrier(void)
{
    __asm volatile ("" ::: "memory");
}

static void meter_reading_write_begin(void)
{
    meter_reading_seq++;
    meter_data_barrier();
}

static void meter_reading_write_end(void)
{
    meter_data_barrier();
    meter_reading_seq++;
}

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
    h->expected_frame_family = r->expected_frame_family;
    h->observed_frame_family = r->observed_frame_family;
    h->reject_reason = r->reject_reason;
    h->bcd_value = r->bcd_value;
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
        old_result_class != r->result_class || old_bcd_value != r->bcd_value || \
        old_decimal_pos != r->decimal_pos || old_negative != r->negative || \
        old_unit_variant != r->unit_variant || old_aux_freq_i10 != meter_aux_freq_i10(r) || \
        old_continuity_beep != r->continuity_beep || \
        strcmp(old_display_str, r->display_str) != 0 || \
        strcmp(old_unit_suffix, r->unit_suffix ? r->unit_suffix : "") != 0) { \
        r->display_update_count++; \
    } \
    meter_reading_write_end(); \
    meter_record_history(); \
} while (0)

#define METER_REJECT_FRAME() do { \
    r->value = 0.0f; \
    r->bcd_value = 0; \
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
        old_result_class != r->result_class || old_bcd_value != r->bcd_value || \
        old_decimal_pos != r->decimal_pos || old_negative != r->negative || \
        old_unit_variant != r->unit_variant || old_aux_freq_i10 != meter_aux_freq_i10(r) || \
        old_continuity_beep != r->continuity_beep || \
        strcmp(old_display_str, r->display_str) != 0 || \
        strcmp(old_unit_suffix, r->unit_suffix ? r->unit_suffix : "") != 0) { \
        r->display_update_count++; \
    } \
    meter_reading_write_end(); \
    meter_record_history(); \
} while (0)

static void meter_clear_payload(meter_reading_t *r)
{
    r->value = 0.0f;
    r->bcd_value = 0;
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
 *   Sub-mode 5 (local AC A policy): X.XXX → decimal at position 1
 *   Sub-mode 6 (Ohm):    X.XXX  → decimal at position 1
 *   Sub-mode 7 (Cont):   XXX.X  → decimal at position 3
 *   Sub-mode 8 (Diode):  X.XXX  → decimal at position 1
 *   Sub-mode 9 (Cap):    XX.XX  → decimal at position 2
 *   Sub-mode 10 (Temp):  XXX.X   → decimal at position 3
 *
 * The actual decimal position is driven later by the stock-like formatter
 * state. These entries are local fallbacks for invalid/uncharacterized paths;
 * do not promote them to range truth without stock disassembly or bench traces.
 * ═══════════════════════════════════════════════════════════════════ */

/* Default decimal position per submode (index 0 = no decimal, 1-3 = after nth digit).
 * These are fallback display positions. The normal parser path below uses
 * stock display-state fields before formatting, and DCV then applies the
 * recovered per-frame voltage range hint. Current range splits, local AC A,
 * and the cap/temp split remain local UI policy unless the stock FSM or live
 * evidence proves a narrower hardware state.
 *
 * Bench references:
 *   DCV (0): 1-10V range, raw 9899 → 9.899 V, decimal after digit 1
 *   ACV (1): stock frame[7] display format, broader AC sweep still needed
 *   Resistance (6): 20k range, raw 9899 → 98.99 kΩ, decimal after digit 2
 *   Continuity (7): 200Ω range, raw 16 → 1.6 Ω, decimal after digit 3
 *   Diode (8): 2V range, raw 623 → 0.623 V, decimal after digit 1
 *   Capacitance (9): 200nF range, raw 1034 → 103.4 nF, decimal after digit 3
 *   Temperature (10): stock extended family, raw 248 → 24.8 C, decimal after digit 3
 */
static const uint8_t default_decimal_pos[11] = {
    1,  /* 0: DCV       — 9.899 V */
    2,  /* 1: ACV       — 98.99 V */
    2,  /* 2: DCA (mA)  — 98.99 mA */
    1,  /* 3: DCA (A)   — 9.899 A */
    2,  /* 4: ACA (mA)  — 98.99 mA */
    1,  /* 5: local AC A policy over stock ACA slot */
    2,  /* 6: Resistance— 98.99 kΩ */
    3,  /* 7: Continuity— 198.9 Ω */
    1,  /* 8: Diode     — 0.623 V */
    3,  /* 9: Capacitance— 198.9 nF */
    3,  /* 10: Temp     — 24.8 C */
};

/* Full-scale values per sub-mode (for bar graph calculation) */
static const float bar_full_scale[11] = {
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
    100.0f,   /* Temp: 100 C */
};

/* Resistance band boundary.
 *
 * Stock-visible frame metadata separates at least two resistance regimes:
 *   frame[6] upper nibble 0: low-Ohm band, factory coefficient unresolved
 *   frame[6] upper nibble 4: kOhm band, raw counts are already Ohm and need
 *                            only the unit shift bcd_value * 0.001 kOhm
 *
 * A prior branch used a one-unit bench factor for the low-Ohm band. That made
 * one resistor look right but violated the stock-evidence rule used for the
 * DMM state machine: if a physical coefficient is not recovered from
 * stock/W25Q/H2 evidence, do not invent it. Low-Ohm normal frames therefore
 * fail closed until the factory calibration source is recovered. Continuity
 * marker frames are handled earlier as a distinct stock frame family.
 */
#define METER_KOHM_UNIT_FACTOR  0.001f

typedef struct {
    uint8_t class_id;
    uint8_t display_decimal_pos;
    float divisor;
} dcv_stock_class_t;

static const dcv_stock_class_t dcv_stock_classes[] = {
    { 4, 1, 10000.0f },
    { 3, 1, 1000.0f  },
    { 2, 2, 100.0f   },
    { 1, 3, 10.0f    },
    { 0, 0, 1.0f     },
};

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
 * Indexed by [local submode][local unit_variant]. Stock display evidence is
 * narrower than this UI table: `meter_fsm_deep_dive.md` and full_decompile.c
 * prove DCA formatter indices 4 (mA) and 3 (A), while ACA case 3 proves index
 * 5 (mA-like). No inspected stock path proves a uA selector/range or a
 * separate AC A formatter. The local variants therefore document UI intent and keep stale
 * voltage/current data visibly separated; they must not be used as proof of an
 * unresolved hardware range state.
 *
 * Strings are ASCII-only so the font renderer doesn't need Greek mu/ohm
 * glyphs. Stock formatter writes DAT_20001026 indices, but the downloaded
 * V1.2.0 APP image does not contain a recovered stock unit string table at
 * 0x0804C40C; that region is a documented negative boundary. The suffix text
 * below is local UI text attached to stock formatter indices, not a recovered
 * stock string dump.
 * ═══════════════════════════════════════════════════════════════════ */

static const char * const unit_suffix_table[11][3] = {
    /*                v0       v1       v2    */
    /* 0 DCV      */ { "V",    "mV",    "mV"   },
    /* 1 ACV      */ { "V",    "mV",    "mV"   },
    /* 2 DCA(mA)  */ { "mA",   "mA",    "mA"   },
    /* 3 DCA(A)   */ { "A",    "A",     "A"    },
    /* 4 ACA(mA)  */ { "mA",   "mA",    "mA"   },
    /* 5 local AC A */ { "A",  "A",     "A"    },
    /* 6 Ohm      */ { "Ohm",  "kOhm",  "MOhm" },
    /* 7 Cont     */ { "Ohm",  "Ohm",   "Ohm"  },
    /* 8 Diode    */ { "V",    "V",     "V"    },
    /* 9 Cap      */ { "nF",   "uF",    "uF"   },
    /* 10 Temp    */ { "C",    "C",     "F"    },
};

static uint8_t stock_mode_from_ui_submode(uint8_t submode)
{
    return fpga_meter_stock_mode_for_submode(submode);
}

static void meter_stock_fsm_reset(uint8_t stock_mode)
{
    memset(&meter_stock_fsm, 0, sizeof(meter_stock_fsm));
    meter_stock_fsm.stock_mode = stock_mode;
    meter_stock_fsm.variant = 1U;
    meter_stock_fsm.dc_state = (stock_mode == 0) ? 1U : 0U;
}

static bool frame_has_voltage_payload_marker(const volatile uint8_t *frame)
{
    /* Stock USART2 DMM frames carry voltage family metadata in frame[8]:
     * `0x02` for common DCV-class payloads, and bit 7 as the highest-priority
     * stock voltage decimal-class bit. Live low-DCV frames can arrive as
     * `0x80` (class bit only) or `0x82` (class bit plus the low DCV marker).
     * Both forms must clear stale current/passive readings when they leak into
     * the wrong local mode. This is metadata-based fail-closed behavior; it
     * must not depend on whether the BCD count happens to resemble any unit. */
    return (((frame[8] & 0x7FU) == 0x02U) || ((frame[8] & 0x80U) != 0)) &&
           frame[9] == 0x00;
}

static bool raw_digits_are_continuity_marker(const uint8_t raw_digits[4])
{
    return raw_digits[1] == 0x12 && raw_digits[2] == 0x0A &&
           raw_digits[3] == 5;
}

static uint8_t observed_frame_family(uint8_t expected_family,
                                     const volatile uint8_t *frame)
{
    if (frame_has_voltage_payload_marker(frame)) {
        return (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE;
    }
    return expected_family;
}

static bool ui_submode_is_small_current(uint8_t ui_submode)
{
    return ui_submode == 2 || ui_submode == 4;
}

static bool ui_submode_is_large_current(uint8_t ui_submode)
{
    /* Local UI range split only. Stock slot 2 has DCA mA/A formatter evidence;
     * stock slot 3 currently has only ACA mA formatter evidence. */
    return ui_submode == 3 || ui_submode == 5;
}

static void meter_sync_stock_fsm_debug(meter_reading_t *r)
{
    r->stock_mode = meter_stock_fsm.stock_mode;
    r->stock_variant = meter_stock_fsm.variant;
    r->stock_format = meter_stock_fsm.format;
    r->stock_dc_state = meter_stock_fsm.dc_state;
    r->stock_display_cmd = meter_stock_fsm.display_cmd;
    r->stock_unit_index = meter_stock_fsm.unit_index;
    r->stock_composite_index = meter_stock_fsm.composite_index;
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
    if (ui_submode_is_small_current(ui_submode)) {
        meter_stock_fsm.variant = 1U;
    } else if (ui_submode_is_large_current(ui_submode)) {
        meter_stock_fsm.variant = 2U;
    }

    /*
     * Stock formatter port.
     * full_decompile.c:2931..3009 computes DAT_20001026 (unit/display index)
     * and DAT_20001030 (composite decimal/template index) from stock mode,
     * frame[6], frame[7], and DAT_2000102e; meter_fsm_deep_dive.md records
     * that only DCV writes the stock DAT_2000102e variant state itself. Local
     * current submodes therefore seed `variant` from UI range policy before
     * entering the stock-shaped cases: DC slot 2 has mA/A formatter evidence,
     * while AC slot 3 currently has ACA mA evidence and a local AC A override.
     * This function must not decide exponent or mode from the raw BCD value;
     * all decisions come from active UI/stock mode plus frame flags.
     */
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
        meter_stock_fsm.unit_index = 5U;
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

    meter_sync_stock_fsm_debug(&meter_reading);
}

static const char *unit_suffix_from_stock(uint8_t ui_submode, uint8_t unit_index)
{
    /* Local overrides keep the selected UI range visible after the stock-like
     * formatter has established a matching current frame family. They are not
     * standalone evidence that the analog frontend entered a distinct AC A/uA
     * range; frontend validation must prove that separately. */
    if (ui_submode == 3 || ui_submode == 5) return "A";
    if (ui_submode == 1) return "V";
    if (ui_submode == 7) return "Ohm";
    if (ui_submode == 8) return "V";
    if (ui_submode == 9) return "nF";
    if (ui_submode == 10) return "C";

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
    return (ui_submode < 11) ? unit_suffix_table[ui_submode][0] : "";
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
        if (ui_submode_is_large_current(ui_submode)) return 1;
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
        return (ui_submode == 9 || ui_submode == 10) ? 3U :
               (uint8_t)(fmt > 3 ? 3 : fmt);
    case 6:
    case 7:
        return (fmt == 0) ? 0U : ((fmt == 1) ? 1U : 3U);
    default:
        return (ui_submode < FPGA_METER_LOCAL_SUBMODE_COUNT) ?
               default_decimal_pos[ui_submode] : 0U;
    }
}

static uint8_t voltage_range_hint_from_stock_frame(const volatile uint8_t *frame)
{
    /*
     * Stock evidence: the V1.2.0 DMM RX analysis exposes an ordered set of
     * autonomous meter-frame voltage range bits. Raw full_decompile.c proves
     * the later DAT_2000102f/DAT_20001030 formatter, while the annotated RX
     * pass is the current source for this per-frame voltage exponent hint.
     * The bit priority is documented in reverse_engineering/analysis_v120/
     * meter_math_pipeline_annotated.c:
     * frame[8].7 -> class 4, frame[3].4 -> class 3, frame[4].4 -> class 2,
     * frame[5].4 -> class 1, otherwise class 0. Bytes 10..11 are not part of
     * this stock exponent decision; in live voltage frames they are only an
     * empirical auxiliary/frequency hint.
     */
    if (frame[8] & 0x80U) return 4;
    if (frame[3] & 0x10U) return 3;
    if (frame[4] & 0x10U) return 2;
    if (frame[5] & 0x10U) return 1;
    return 0;
}

static const dcv_stock_class_t *
dcv_stock_class_from_frame(const volatile uint8_t *frame)
{
    uint8_t class_id = voltage_range_hint_from_stock_frame(frame);

    for (unsigned i = 0; i < sizeof(dcv_stock_classes) / sizeof(dcv_stock_classes[0]); i++) {
        if (dcv_stock_classes[i].class_id == class_id) {
            return &dcv_stock_classes[i];
        }
    }

    return NULL;
}

static bool apply_stock_dcv_voltage_range_hint(meter_reading_t *r,
                                               uint8_t submode,
                                               const volatile uint8_t *frame)
{
    const dcv_stock_class_t *stock_class;

    if (submode != 0) {
        return false;
    }

    stock_class = dcv_stock_class_from_frame(frame);
    if (stock_class == NULL) {
        return false;
    }

    /*
     * The range entry is selected only from stock frame status bits, before
     * looking at the decoded digits. This mirrors the instrument contract:
     * the meter IC/front-end declares its active range in the 12-byte USART2
     * report, and the renderer converts the extended raw value by the matching
     * stock decimal exponent. This table is not a recognition table for
     * convenient bench values.
     */
    r->decimal_pos = stock_class->display_decimal_pos;
    r->unit_variant = 0;
    r->unit_suffix = "V";
    return true;
}

static void format_stock_decimal_value(int raw_value, uint8_t class_id,
                                       bool negative, char *s)
{
    int pos = 0;
    int divisor = 1;
    uint8_t decimals = class_id;

    if (negative) {
        s[pos++] = '-';
    }

    for (uint8_t i = 0; i < decimals; i++) {
        divisor *= 10;
    }

    int whole = raw_value / divisor;
    int frac = raw_value % divisor;

    if (whole >= 10000) s[pos++] = (char)('0' + (whole / 10000) % 10);
    if (whole >= 1000)  s[pos++] = (char)('0' + (whole / 1000) % 10);
    if (whole >= 100)   s[pos++] = (char)('0' + (whole / 100) % 10);
    if (whole >= 10)    s[pos++] = (char)('0' + (whole / 10) % 10);
    s[pos++] = (char)('0' + whole % 10);

    if (decimals > 0) {
        int place = divisor / 10;
        s[pos++] = '.';
        while (place > 0) {
            s[pos++] = (char)('0' + (frac / place) % 10);
            place /= 10;
        }
    }

    s[pos] = '\0';
}

static bool apply_stock_dcv_voltage_multiplier(meter_reading_t *r,
                                               uint8_t submode,
                                               const volatile uint8_t *frame)
{
    const dcv_stock_class_t *stock_class;

    if (submode != 0) {
        return false;
    }

    stock_class = dcv_stock_class_from_frame(frame);
    if (stock_class == NULL) {
        return false;
    }

    /*
     * DCV value reconstruction.
     *
     * Stock V1.2.0 does two separate things in FUN_08036AC0:
     *   1. build raw = d0*1000 + d1*100 + d2*10 + d3, then add 10000.0f
     *      when frame[2].3 is set;
     *   2. choose a decimal exponent from frame[8].7, frame[3].4, frame[4].4,
     *      frame[5].4, or default, and divide the extended raw value by
     *      10^class.
     *
     * No stock-only evidence has a special one-point low-voltage coefficient.
     * Factory calibration may still exist in W25Q/SPI bulk data, but it is not
     * this display exponent table and must not be invented here.
     */
    float v = (float)r->bcd_value / stock_class->divisor;
    if (r->negative) v = -v;
    r->value = v;
    r->unit_variant = 0;
    r->unit_suffix = "V";
    format_stock_decimal_value(r->bcd_value, stock_class->class_id,
                               r->negative, r->display_str);

    float abs_v = v < 0.0f ? -v : v;
    float full_scale = (submode < 11) ? bar_full_scale[submode] : 1000.0f;
    r->bar_fraction = abs_v / full_scale;
    if (r->bar_fraction > 1.0f) r->bar_fraction = 1.0f;

    return true;
}

static bool frame_is_voltage_payload(uint8_t submode,
                                     const volatile uint8_t *frame)
{
    return fpga_meter_frame_family_for_submode(submode) !=
           FPGA_METER_FRAME_FAMILY_VOLTAGE &&
           frame_has_voltage_payload_marker(frame);
}

static bool frame_family_mismatch(const meter_reading_t *r)
{
    return !fpga_meter_frame_family_is_acceptable(r->expected_frame_family,
                                                  r->observed_frame_family);
}

static bool meter_submode_invalid(uint8_t submode)
{
    return !fpga_meter_submode_is_valid(submode);
}

static bool submode_requires_ac_evidence(uint8_t submode)
{
    return submode == 1 || submode == 4 || submode == 5;
}

static bool frame_extra_is_empirical_line_frequency_hint(uint16_t extra)
{
    return extra >= 45U && extra <= 65U;
}

static bool frame_has_ac_evidence(uint8_t submode,
                                  uint8_t status,
                                  uint16_t extra)
{
    (void)status;
    /*
     * AC confidence is deliberately narrower than the stock display-format
     * status byte. Live DCV frames at a visually confirmed 0.200 V can carry
     * frame[7]=0x24 while the source/load display is DC; treating bit 2 alone
     * as "AC present" lets ACV render a confident but false 436.x V. The stock
     * ACV formatter evidence only proves frame[7].0 as a decimal-format
     * selector. Until a stock xref proves a separate AC-valid flag, require the
     * empirical companion line-frequency hint and fail closed otherwise.
     */
    return submode_requires_ac_evidence(submode) &&
           frame_extra_is_empirical_line_frequency_hint(extra);
}

static bool resistance_low_ohm_calibration_unresolved(uint8_t submode,
                                                      uint8_t flags)
{
    return (submode == 6 || submode == 7) && ((flags & 0xF0U) == 0x00U);
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
    r->value = (float)r->bcd_value / divisor;
    if (r->negative) r->value = -r->value;

    /* Bar graph fraction */
    float abs_val = r->value < 0 ? -r->value : r->value;
    float full_scale = (submode < 11) ? bar_full_scale[submode] : 1000.0f;
    r->bar_fraction = abs_val / full_scale;
    if (r->bar_fraction > 1.0f) r->bar_fraction = 1.0f;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void meter_data_init(void)
{
    meter_reading_seq = 0;
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

bool meter_data_snapshot(meter_reading_t *out)
{
    if (out == NULL) return false;

    for (int attempt = 0; attempt < 8; attempt++) {
        uint32_t before = meter_reading_seq;
        meter_data_barrier();
        if ((before & 1U) != 0U) continue;

        *out = meter_reading;

        meter_data_barrier();
        uint32_t after = meter_reading_seq;
        if (before == after && (after & 1U) == 0U) return true;
    }

    return false;
}

void meter_data_invalidate(uint8_t submode)
{
    meter_reading_t *r = &meter_reading;

    meter_reading_write_begin();
    r->value = 0.0f;
    r->bcd_value = 0;
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
    r->expected_frame_family =
        (uint8_t)fpga_meter_frame_family_for_submode(submode);
    r->observed_frame_family = r->expected_frame_family;
    r->reject_reason = METER_REJECT_NONE;
    r->display_update_count++;
    memset(r->dbg_frame, 0, sizeof(r->dbg_frame));
    memset(r->dbg_nibbles, 0, sizeof(r->dbg_nibbles));
    memset(r->dbg_raw_digits, 0, sizeof(r->dbg_raw_digits));

    meter_stock_fsm_reset(stock_mode_from_ui_submode(submode));
    meter_sync_stock_fsm_debug(r);
    r->update_count++;
    meter_reading_write_end();
}

void meter_data_process_frame(const volatile uint8_t *frame, uint8_t submode)
{
    meter_reading_t *r = &meter_reading;

    /* Validate header */
    if (frame[0] != 0x5A || frame[1] != 0xA5) return;

    bool old_valid = r->valid;
    uint8_t old_submode = r->submode;
    meter_result_class_t old_result_class = r->result_class;
    int old_bcd_value = r->bcd_value;
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

    meter_reading_write_begin();

    /* Save raw frame for debug display */
    for (int i = 0; i < 12; i++) r->dbg_frame[i] = frame[i];
    r->submode = submode;
    r->continuity_beep = false;
    r->aux_freq_hz = 0.0f;
    r->expected_frame_family =
        (uint8_t)fpga_meter_frame_family_for_submode(submode);
    r->observed_frame_family =
        observed_frame_family(r->expected_frame_family, frame);
    r->reject_reason = METER_REJECT_NONE;
    if (meter_stock_fsm.stock_mode != stock_mode_from_ui_submode(submode)) {
        meter_stock_fsm_reset(stock_mode_from_ui_submode(submode));
    }
    meter_sync_stock_fsm_debug(r);

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
    if (!frame_has_voltage_payload_marker(frame) &&
        raw_digits_are_continuity_marker(r->dbg_raw_digits)) {
        r->observed_frame_family =
            (uint8_t)FPGA_METER_FRAME_FAMILY_CONTINUITY;
    }

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

    if (meter_submode_invalid(submode)) {
        r->reject_reason = METER_REJECT_INVALID_SUBMODE;
        METER_REJECT_FRAME();
        return;
    }

    if (frame_is_voltage_payload(submode, frame) ||
        frame_family_mismatch(r)) {
        r->reject_reason = METER_REJECT_WRONG_FRAME_FAMILY;
        METER_REJECT_FRAME();
        return;
    }

    if (submode_requires_ac_evidence(submode) &&
        !frame_has_ac_evidence(submode, status, extra)) {
        r->reject_reason = METER_REJECT_MISSING_AC_EVIDENCE;
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
            r->bcd_value = digit0;
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

    if (resistance_low_ohm_calibration_unresolved(submode, flags)) {
        r->reject_reason = METER_REJECT_UNRESOLVED_CALIBRATION;
        METER_REJECT_FRAME();
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
    r->bcd_value = d0 * 1000 + d1 * 100 + d2 * 10 + d3;
    if (frame[2] & 0x08U) {
        r->bcd_value += 10000;
    }

    uint8_t raw_digit_codes[4] = { digit0, digit1, digit2, digit3 };

    meter_stock_fsm_apply(submode, frame, raw_digit_codes);
    r->decimal_pos = decimal_pos_from_stock(submode, &meter_stock_fsm);
    r->unit_variant = meter_stock_fsm.variant;
    r->unit_suffix = unit_suffix_from_stock(submode, meter_stock_fsm.unit_index);
    (void)apply_stock_dcv_voltage_range_hint(r, submode, frame);

    if (((submode == 0 && frame[8] == 0x02 && frame[9] == 0x00) ||
         submode_requires_ac_evidence(submode)) &&
        frame_extra_is_empirical_line_frequency_hint(extra)) {
        r->aux_freq_hz = (float)extra;
    }

    /* Format the display string and compute float value */
    r->result_class = METER_RESULT_NORMAL;
    r->continuity_beep = false;
    format_reading(r, submode);
    (void)apply_stock_dcv_voltage_multiplier(r, submode, frame);

    /* ── Resistance kOhm band unit normalization ──
     *
     * For resistance/continuity submodes, the FPGA meter IC rotates
     * through multiple frame variants per measurement, each claiming
     * a different dp/unit. The "correct" interpretation depends on
     * which specific frame[6] we happen to catch — without override,
     * the display flickers between e.g. "9.821 kOhm" and "98.24 kOhm"
     * for the SAME 10 kΩ resistor.
     *
     * Only the kOhm band is normalized here:
     *
     *   kOhm band (frame[6] upper nibble 4): value = bcd_value * 0.001 kOhm
     *
     * The low-Ohm band is rejected above until its factory calibration source is
     * recovered. The 0.001 kOhm factor is a geometric identity (raw counts
     * already expressed in Ohm, shifted to kOhm) and should be stable across
     * units.
     *
     * Higher bands (MΩ, autorange to 200 kΩ / 2 MΩ) are not yet
     * characterized — those will need additional upper-nibble cases.
     *
     * We leave bcd_value, decimal_pos, and digits[] untouched so the
     * debug overlay at meter_ui.c:948 still shows the FPGA's raw
     * pre-cal report. UI code reads `value` and `display_str` for
     * the final numbers.
     */
    if (submode == 6 || submode == 7) {
        float       scale = 0.0f;
        const char *unit  = NULL;

        switch (flags & 0xF0) {
        case 0x40:  scale = METER_KOHM_UNIT_FACTOR; unit = "kOhm"; break;
        default:    break;  /* Unknown band — leave format_reading's output */
        }

        if (scale != 0.0f) {
            float v = (float)r->bcd_value * scale;
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
            float full_scale = (submode < 11) ? bar_full_scale[submode] : 1000.0f;
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
