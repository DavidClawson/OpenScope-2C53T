#include "fpga_meter_plan.h"

#define FPGA_METER_STOCK_WORD_BASE 0x0500u

static const uint8_t stock_meter_cmd_low[FPGA_METER_STOCK_MODE_COUNT] = {
    0x14, 0x0C, 0x17, 0x0B, 0x0A, 0x12, 0x11, 0x10
};

uint8_t fpga_meter_stock_mode_for_submode(uint8_t submode)
{
    switch (submode) {
    case 0: return 0; /* DCV */
    case 1: return 1; /* ACV */
    case 2: /* DC mA */
    case 3: /* DC A */
        return 4;
    case 4: /* AC mA */
    case 5: /* AC A */
        return 3;
    case 6: return 2; /* Resistance */
    case 7: return 6; /* Continuity */
    case 8: return 7; /* Diode */
    case 9: return 5; /* Extended DMM slot; stock notes leave capacitance semantics open. */
    default:
        return 0;
    }
}

uint8_t fpga_meter_stock_cmd_low_for_mode(uint8_t stock_mode)
{
    if (stock_mode >= FPGA_METER_STOCK_MODE_COUNT) {
        stock_mode = 0;
    }
    return stock_meter_cmd_low[stock_mode];
}

uint16_t fpga_meter_stock_cmd_word_for_submode(uint8_t submode)
{
    uint8_t stock_mode = fpga_meter_stock_mode_for_submode(submode);
    return (uint16_t)(FPGA_METER_STOCK_WORD_BASE |
                      fpga_meter_stock_cmd_low_for_mode(stock_mode));
}
