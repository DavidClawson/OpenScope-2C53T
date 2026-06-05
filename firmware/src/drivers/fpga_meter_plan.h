#ifndef FPGA_METER_PLAN_H
#define FPGA_METER_PLAN_H

#include <stdint.h>

#define FPGA_METER_LOCAL_SUBMODE_COUNT 10u
#define FPGA_METER_STOCK_MODE_COUNT    8u

uint8_t fpga_meter_stock_mode_for_submode(uint8_t submode);
uint8_t fpga_meter_stock_cmd_low_for_mode(uint8_t stock_mode);
uint16_t fpga_meter_stock_cmd_word_for_submode(uint8_t submode);

#endif /* FPGA_METER_PLAN_H */
