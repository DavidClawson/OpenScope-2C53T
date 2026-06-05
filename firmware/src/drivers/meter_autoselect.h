/*
 * OpenScope 2C53T - asynchronous DMM function auto-select service
 */

#ifndef METER_AUTOSELECT_H
#define METER_AUTOSELECT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    METER_AUTOSELECT_IDLE = 0,
    METER_AUTOSELECT_RUNNING,
    METER_AUTOSELECT_DONE,
    METER_AUTOSELECT_CANCELLED,
} meter_autoselect_state_t;

typedef struct {
    meter_autoselect_state_t state;
    uint8_t current_submode;
    uint8_t current_index;
    uint8_t candidate_count;
    uint8_t best_submode;
    uint8_t best_score;
    uint8_t last_score;
    uint32_t settle_ms;
    uint32_t wait_budget_ms;
    bool cancel_pending;
} meter_autoselect_status_t;

void meter_autoselect_create_task(void);
bool meter_autoselect_start(uint32_t settle_ms);
void meter_autoselect_cancel(void);
bool meter_autoselect_is_running(void);
void meter_autoselect_get_status(meter_autoselect_status_t *out);
const char *meter_autoselect_state_name(meter_autoselect_state_t state);

#endif /* METER_AUTOSELECT_H */
