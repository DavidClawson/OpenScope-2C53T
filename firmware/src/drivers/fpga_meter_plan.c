#include "fpga_meter_plan.h"

#define FPGA_METER_STOCK_WORD_BASE 0x0500u

/*
 * Stock source of truth:
 * reverse_engineering/analysis_v120/meter_mode_command_table_2026_06_05.md
 * records the eight recovered selector low bytes below. The open firmware has
 * eleven UI submodes, so this module maps local UI policy onto those eight
 * hardware selector slots; it does not claim that stock has eleven independent
 * analog frontend modes. In particular, DC mA/DC A share stock slot 2,
 * AC mA/local AC A share stock slot 3, and capacitance/temperature share stock
 * slot 5 until a stock writer or bench trace proves a narrower selector. If a
 * future live case is surprising, keep that boundary here: do not invent a new
 * 0x05xx selector for the local split without binary stock evidence.
 */
static const uint8_t stock_meter_cmd_low[FPGA_METER_STOCK_MODE_COUNT] = {
    0x14, 0x0C, 0x17, 0x0B, 0x0A, 0x12, 0x11, 0x10
};

/*
 * Logical DMM function surface.
 * The user-facing goal includes the usual DMM current families uA/mA/A, but
 * stock V1.2.0 evidence recovered so far exposes only shared current slots for
 * mA/A-like behavior. No selector-table entry, formatter path, mux writer, or
 * safe live trace proves a microamp frontend/range. Model that absence
 * explicitly: DC_UA and AC_UA map to INVALID, so autoscan/UI work cannot
 * quietly introduce a microamp path until new stock/live evidence changes this
 * table and its tests.
 */
static const uint8_t
logical_function_local_submode[FPGA_METER_LOGICAL_FUNCTION_COUNT] = {
    0,                                /* DCV */
    1,                                /* ACV */
    FPGA_METER_INVALID_LOCAL_SUBMODE, /* DC uA unresolved */
    2,                                /* DC mA */
    3,                                /* DC A */
    FPGA_METER_INVALID_LOCAL_SUBMODE, /* AC uA unresolved */
    4,                                /* AC mA */
    5,                                /* AC A */
    6,                                /* Resistance */
    7,                                /* Continuity */
    8,                                /* Diode */
    9,                                /* Capacitance */
    10,                               /* Temperature */
};

typedef struct {
    uint8_t pc12;
    uint8_t pe4;
    uint8_t pe5;
    uint8_t pe6;
} fpga_meter_portc_porte_state_t;

typedef struct {
    uint8_t pa15;
    uint8_t pa10;
    uint8_t pb10;
    uint8_t pb11;
} fpga_meter_porta_portb_state_t;

/*
 * Stock source of truth:
 * - FUN_080018a4 @ 0x080018A4 projects ms[0x02] into PC12/PE4/PE5/PE6.
 * - FUN_08001a58 @ 0x08001A58 projects ms[0x03] into PA15/PA10/PB10/PB11.
 *
 * The stock writers only touch the pins selected by each switch arm. These
 * final levels include the open firmware's stock-like pre-seed before applying
 * that projection. PB9 and PA6 are kept low because the recovered stock sites
 * only prove reset/output-low setup for the auxiliary AFE pins.
 */
static const fpga_meter_mux_gpio_state_t meter_mux_baseline = {
    1, 1, 0, 1,
    1, 1, 0, 1,
    0, 0
};

static const fpga_meter_portc_porte_state_t stock_portc_porte_mux[10] = {
    { 1, 1, 0, 1 },
    { 1, 1, 0, 1 },
    { 1, 1, 1, 0 },
    { 1, 1, 0, 0 },
    { 1, 1, 1, 0 },
    { 0, 1, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 1, 0, 0 },
    { 0, 1, 1, 0 },
};

static const fpga_meter_porta_portb_state_t stock_porta_portb_mux[10] = {
    { 1, 1, 0, 1 },
    { 1, 1, 1, 1 },
    { 1, 0, 1, 0 },
    { 1, 0, 0, 1 },
    { 1, 0, 1, 1 },
    { 0, 1, 0, 1 },
    { 0, 1, 1, 1 },
    { 0, 1, 1, 0 },
    { 0, 0, 0, 1 },
    { 0, 0, 1, 1 },
};

bool fpga_meter_submode_is_valid(uint8_t submode)
{
    return submode < FPGA_METER_LOCAL_SUBMODE_COUNT;
}

bool fpga_meter_logical_function_is_valid(uint8_t function)
{
    return function < FPGA_METER_LOGICAL_FUNCTION_COUNT;
}

uint8_t fpga_meter_submode_for_logical_function(uint8_t function)
{
    if (!fpga_meter_logical_function_is_valid(function)) {
        return FPGA_METER_INVALID_LOCAL_SUBMODE;
    }
    return logical_function_local_submode[function];
}

bool fpga_meter_logical_function_is_supported(uint8_t function)
{
    return fpga_meter_submode_is_valid(
        fpga_meter_submode_for_logical_function(function));
}

bool fpga_meter_logical_function_is_unresolved(uint8_t function)
{
    return fpga_meter_logical_function_is_valid(function) &&
           !fpga_meter_logical_function_is_supported(function);
}

uint8_t fpga_meter_stock_mode_for_submode(uint8_t submode)
{
    switch (submode) {
    case 0: return 0; /* DCV */
    case 1: return 1; /* ACV */
    case 2: /* DC mA */
    case 3: /* DC A */
        return 2;
    case 4: /* AC mA */
    case 5: /* Local AC A policy over the recovered ACA slot. */
        return 3;
    case 6: return 4; /* Resistance */
    case 7: return 6; /* Continuity */
    case 8: return 7; /* Diode */
    case 9:  /* Capacitance */
    case 10: /* Local temp split on the recovered extended stock slot. */
        return 5;
    default:
        return FPGA_METER_INVALID_STOCK_MODE;
    }
}

uint8_t fpga_meter_stock_cmd_low_for_mode(uint8_t stock_mode)
{
    if (stock_mode >= FPGA_METER_STOCK_MODE_COUNT) {
        return 0;
    }
    return stock_meter_cmd_low[stock_mode];
}

uint16_t fpga_meter_stock_cmd_word_for_submode(uint8_t submode)
{
    uint8_t stock_mode = fpga_meter_stock_mode_for_submode(submode);
    if (stock_mode >= FPGA_METER_STOCK_MODE_COUNT) {
        return FPGA_METER_INVALID_SELECTOR_WORD;
    }
    return (uint16_t)(FPGA_METER_STOCK_WORD_BASE |
                      fpga_meter_stock_cmd_low_for_mode(stock_mode));
}

bool fpga_meter_stock_apply_cmd_word_for_submode(uint8_t submode, uint16_t *word)
{
    uint8_t stock_mode = fpga_meter_stock_mode_for_submode(submode);
    uint8_t low;

    switch (stock_mode) {
    case 1:
        low = 0x0D;
        break;
    case 2:
        low = 0x0E;
        break;
    case 6:
        low = 0x16;
        break;
    case 7:
        low = 0x15;
        break;
    default:
        return false;
    }

    if (word != 0) {
        *word = (uint16_t)(FPGA_METER_STOCK_WORD_BASE | low);
    }
    return true;
}

fpga_meter_frame_family_t fpga_meter_frame_family_for_submode(uint8_t submode)
{
    switch (submode) {
    case 0:
    case 1:
        return FPGA_METER_FRAME_FAMILY_VOLTAGE;
    case 2:
    case 3:
    case 4:
    case 5:
        return FPGA_METER_FRAME_FAMILY_CURRENT;
    case 6:
        return FPGA_METER_FRAME_FAMILY_RESISTANCE;
    case 7:
        return FPGA_METER_FRAME_FAMILY_CONTINUITY;
    case 8:
        return FPGA_METER_FRAME_FAMILY_DIODE;
    case 9:
    case 10:
        return FPGA_METER_FRAME_FAMILY_EXTENDED;
    default:
        return FPGA_METER_FRAME_FAMILY_INVALID;
    }
}

bool fpga_meter_frame_family_is_recovered(uint8_t family)
{
    /*
     * Active-plan families recovered from stock selector/formatter evidence.
     * This answers "what family does the selected DMM mode expect?", not "can
     * a foreign frame from this family be independently recognized on the RX
     * wire?"  Keep the latter boundary in fpga_meter_frame_family_has_stock_marker().
     */
    return family == FPGA_METER_FRAME_FAMILY_VOLTAGE ||
           family == FPGA_METER_FRAME_FAMILY_CURRENT ||
           family == FPGA_METER_FRAME_FAMILY_RESISTANCE ||
           family == FPGA_METER_FRAME_FAMILY_CONTINUITY ||
           family == FPGA_METER_FRAME_FAMILY_DIODE ||
           family == FPGA_METER_FRAME_FAMILY_EXTENDED;
}

bool fpga_meter_frame_family_has_stock_marker(uint8_t family)
{
    /*
     * Marker-visible families recovered so far. Voltage carries stock-visible
     * metadata in frame[8]/frame[9], and continuity has a distinctive segment
     * marker. Current, resistance, diode, and extended normal digit frames are
     * only active-plan classified until stock xrefs or safe live traces recover
     * family metadata; do not promote BCD shape, frame[6], or unit text into a
     * marker.
     */
    return family == FPGA_METER_FRAME_FAMILY_VOLTAGE ||
           family == FPGA_METER_FRAME_FAMILY_CONTINUITY;
}

bool fpga_meter_frame_family_is_acceptable(uint8_t expected, uint8_t observed)
{
    /*
     * Pure state-machine policy, not raw-frame recognition.
     * The parser can only tag observed families when recovered stock metadata
     * is visible in the frame (currently voltage and continuity markers, plus
     * explicit unresolved gaps in the RE notes). Once a family is known,
     * however, there is no cross-family fallback: current, resistance, diode,
     * and extended payloads must not be relabeled into the active UI mode just
     * because their BCD digits would format cleanly.
     */
    return fpga_meter_frame_family_is_recovered(expected) &&
           expected == observed;
}

fpga_meter_transition_plan_t fpga_meter_transition_plan_for_submode(uint8_t submode)
{
    fpga_meter_transition_plan_t plan;

    plan.submode = submode;
    plan.stock_mode = fpga_meter_stock_mode_for_submode(submode);
    plan.frame_family = (uint8_t)fpga_meter_frame_family_for_submode(submode);
    /*
     * Stock proves the transport shape around mode changes: pause/drain/reset
     * before resuming DMM traffic. It does not yet prove an exact frame count
     * or millisecond delay for every local submode. Keep these as a uniform
     * local settle/discard policy, guarded by tests and RE notes, until a stock
     * path or repeatable bench trace recovers narrower timing.
     */
    plan.discard_frames = FPGA_METER_TRANSITION_DISCARD_FRAMES;
    plan.settle_ms = FPGA_METER_TRANSITION_SETTLE_MS;
    plan.selector_word = fpga_meter_stock_cmd_word_for_submode(submode);
    plan.has_apply_word =
        fpga_meter_stock_apply_cmd_word_for_submode(submode, &plan.apply_word);
    plan.has_probe_detect = true;
    plan.start_word = FPGA_METER_START_WORD;
    if (plan.stock_mode >= FPGA_METER_STOCK_MODE_COUNT) {
        plan.mux_index = FPGA_METER_INVALID_STOCK_MODE;
        plan.portc_porte_mux = FPGA_METER_INVALID_STOCK_MODE;
        plan.porta_portb_mux = FPGA_METER_INVALID_STOCK_MODE;
        plan.discard_frames = 0;
        plan.settle_ms = 0;
        plan.selector_word = FPGA_METER_INVALID_SELECTOR_WORD;
        plan.has_apply_word = false;
        plan.apply_word = 0;
        plan.has_probe_detect = false;
        plan.start_word = 0;
        plan.voltage_function_axis = false;
        return plan;
    }
    /*
     * The stock decompile names two analog-frontend mux bytes, ms[0x02] and
     * ms[0x03], which feed the Port C/E and Port A/B GPIO writers. The current
     * port uses the recovered stock slot as both mux indices because no scoped
     * disassembly path has yet shown an extra writer that splits small current,
     * A-range current, capacitance, or temperature inside a shared slot.
     * This is a software state-machine contract only. Physical correctness for
     * low DCV and shared local ranges still needs stock xrefs or repeatable
     * live traces; do not hide those gaps with decoder-side value-shape hacks.
     */
    plan.portc_porte_mux = plan.stock_mode;
    plan.porta_portb_mux = plan.stock_mode;
    plan.mux_index = plan.portc_porte_mux;
    if (!plan.has_apply_word) {
        plan.apply_word = 0;
    }
    plan.voltage_function_axis =
        (plan.frame_family == FPGA_METER_FRAME_FAMILY_VOLTAGE);
    return plan;
}

bool fpga_meter_mux_gpio_state_for_submode(uint8_t submode,
                                           fpga_meter_mux_gpio_state_t *out)
{
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(submode);
    fpga_meter_portc_porte_state_t ce;
    fpga_meter_porta_portb_state_t ab;

    if (out == 0) {
        return false;
    }
    *out = meter_mux_baseline;

    if (plan.portc_porte_mux >= 10 || plan.porta_portb_mux >= 10) {
        return false;
    }

    ce = stock_portc_porte_mux[plan.portc_porte_mux];
    ab = stock_porta_portb_mux[plan.porta_portb_mux];

    out->pc12 = ce.pc12;
    out->pe4 = ce.pe4;
    out->pe5 = ce.pe5;
    out->pe6 = ce.pe6;
    out->pa15 = ab.pa15;
    out->pa10 = ab.pa10;
    out->pb10 = ab.pb10;
    out->pb11 = ab.pb11;
    return true;
}

bool fpga_meter_rx_frame_should_parse(bool transition_busy,
                                      volatile uint8_t *discard_count,
                                      volatile uint32_t *transition_skip_count)
{
    if (transition_busy) {
        if (transition_skip_count != 0) {
            (*transition_skip_count)++;
        }
        return false;
    }
    if (discard_count != 0 && *discard_count > 0) {
        (*discard_count)--;
        return false;
    }
    return true;
}
