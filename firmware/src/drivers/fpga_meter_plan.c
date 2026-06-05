#include "fpga_meter_plan.h"

#define FPGA_METER_STOCK_WORD_BASE 0x0500u

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
    case 5: /* AC A */
        return 3;
    case 6: return 4; /* Resistance */
    case 7: return 6; /* Continuity */
    case 8: return 7; /* Diode */
    case 9:  /* Capacitance */
    case 10: /* Temperature, same recovered extended stock formatter family. */
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
        plan.discard_frames = 0;
        plan.settle_ms = 0;
        plan.selector_word = FPGA_METER_INVALID_SELECTOR_WORD;
        plan.has_apply_word = false;
        plan.apply_word = 0;
        plan.voltage_function_axis = false;
        return plan;
    }
    plan.mux_index = plan.stock_mode;
    if (!plan.has_apply_word) {
        plan.apply_word = 0;
    }
    plan.voltage_function_axis =
        (plan.frame_family == FPGA_METER_FRAME_FAMILY_VOLTAGE);
    return plan;
}
