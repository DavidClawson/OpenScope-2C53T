#ifndef FPGA_METER_PLAN_H
#define FPGA_METER_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#define FPGA_METER_LOCAL_SUBMODE_COUNT 11u
#define FPGA_METER_STOCK_MODE_COUNT    8u
#define FPGA_METER_TRANSITION_DISCARD_FRAMES 2u
#define FPGA_METER_TRANSITION_SETTLE_MS      20u
#define FPGA_METER_INVALID_STOCK_MODE        0xFFu
#define FPGA_METER_INVALID_SELECTOR_WORD     0x0000u

typedef enum {
    FPGA_METER_FRAME_FAMILY_VOLTAGE = 0,
    FPGA_METER_FRAME_FAMILY_CURRENT = 1,
    FPGA_METER_FRAME_FAMILY_RESISTANCE = 2,
    FPGA_METER_FRAME_FAMILY_CONTINUITY = 3,
    FPGA_METER_FRAME_FAMILY_DIODE = 4,
    FPGA_METER_FRAME_FAMILY_EXTENDED = 5,
    FPGA_METER_FRAME_FAMILY_INVALID = 0xFF,
} fpga_meter_frame_family_t;

typedef struct {
    uint8_t function_selector;  /* Stock DMM mode index used by the raw 0x05xx table. */
    uint8_t range_selector;     /* Low byte from the recovered stock DMM command table. */
    bool    voltage_function_axis;
} fpga_meter_selector_t;

typedef struct {
    uint8_t submode;
    uint8_t stock_mode;
    uint8_t mux_index;          /* Debug alias for the Port C/E function mux. */
    uint8_t portc_porte_mux;    /* Stock ms[0x02] -> gpio_mux_portc_porte(). */
    uint8_t porta_portb_mux;    /* Stock ms[0x03] -> gpio_mux_porta_portb(). */
    uint8_t frame_family;
    uint8_t discard_frames;
    uint16_t settle_ms;
    uint16_t selector_word;
    bool has_apply_word;
    uint16_t apply_word;
    bool voltage_function_axis;
} fpga_meter_transition_plan_t;

bool fpga_meter_submode_is_valid(uint8_t submode);
uint8_t fpga_meter_stock_mode_for_submode(uint8_t submode);
uint8_t fpga_meter_stock_cmd_low_for_mode(uint8_t stock_mode);
uint16_t fpga_meter_stock_cmd_word_for_submode(uint8_t submode);
bool fpga_meter_stock_apply_cmd_word_for_submode(uint8_t submode, uint16_t *word);
fpga_meter_frame_family_t fpga_meter_frame_family_for_submode(uint8_t submode);
fpga_meter_transition_plan_t fpga_meter_transition_plan_for_submode(uint8_t submode);
bool fpga_meter_rx_frame_should_parse(bool transition_busy,
                                      volatile uint8_t *discard_count,
                                      volatile uint32_t *transition_skip_count);

#endif /* FPGA_METER_PLAN_H */
