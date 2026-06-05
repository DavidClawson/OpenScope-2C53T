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
 * slot 5 until a stock writer or bench trace proves a narrower selector.
 */
static const uint8_t stock_meter_cmd_low[FPGA_METER_STOCK_MODE_COUNT] = {
    0x14, 0x0C, 0x17, 0x0B, 0x0A, 0x12, 0x11, 0x10
};

bool fpga_meter_submode_is_valid(uint8_t submode)
{
    return submode < FPGA_METER_LOCAL_SUBMODE_COUNT;
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

fpga_meter_transition_plan_t fpga_meter_transition_plan_for_submode(uint8_t submode)
{
    fpga_meter_transition_plan_t plan;

    plan.submode = submode;
    plan.stock_mode = fpga_meter_stock_mode_for_submode(submode);
    plan.frame_family = (uint8_t)fpga_meter_frame_family_for_submode(submode);
    plan.discard_frames = FPGA_METER_TRANSITION_DISCARD_FRAMES;
    plan.settle_ms = FPGA_METER_TRANSITION_SETTLE_MS;
    plan.selector_word = fpga_meter_stock_cmd_word_for_submode(submode);
    plan.has_apply_word =
        fpga_meter_stock_apply_cmd_word_for_submode(submode, &plan.apply_word);
    if (plan.stock_mode >= FPGA_METER_STOCK_MODE_COUNT) {
        plan.mux_index = FPGA_METER_INVALID_STOCK_MODE;
        plan.portc_porte_mux = FPGA_METER_INVALID_STOCK_MODE;
        plan.porta_portb_mux = FPGA_METER_INVALID_STOCK_MODE;
        plan.discard_frames = 0;
        plan.settle_ms = 0;
        plan.selector_word = FPGA_METER_INVALID_SELECTOR_WORD;
        plan.has_apply_word = false;
        plan.apply_word = 0;
        plan.voltage_function_axis = false;
        return plan;
    }
    /*
     * The stock decompile names two analog-frontend mux bytes, ms[0x02] and
     * ms[0x03], which feed the Port C/E and Port A/B GPIO writers. The current
     * port uses the recovered stock slot as both mux indices because no scoped
     * disassembly path has yet shown an extra writer that splits small current,
     * A-range current, capacitance, or temperature inside a shared slot.
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
