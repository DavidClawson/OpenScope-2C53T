/*
 * factory_cal.c — see mcu_cal.h. Read-only decode of the factory
 * calibration page; no writes, no side effects, no FreeRTOS calls, so it
 * is safe anywhere including pre-scheduler diagnostics.
 */

#include "mcu_cal.h"

/* Page read through const volatile, exactly like cal_dump.c. */
#define PAGE ((const volatile uint8_t *)MCU_CAL_PAGE_BASE)

/* Word i of the cal region: (B[i] << 16) | A[i], little-endian. */
static uint32_t cal_word(uint32_t i)
{
    const volatile uint8_t *p = PAGE + MCU_CAL_OFF + 4u * i;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

mcu_cal_status_t mcu_cal_status(void)
{
    if (PAGE[0] != MCU_CAL_SIG)
        return MCU_CAL_NO_PAGE;
    uint16_t sentinel = (uint16_t)(cal_word(MCU_CAL_WORDS - 1) >> 16); /* B[59] */
    if (sentinel == MCU_CAL_SENTINEL || sentinel == 0u)
        return MCU_CAL_NO_TABLE;
    return MCU_CAL_OK;
}

bool mcu_cal_trigger_pair(uint8_t ch, uint8_t range, uint16_t *base, uint16_t *upper)
{
    if (range >= MCU_CAL_RANGES || base == 0 || upper == 0)
        return false;
    if (mcu_cal_status() != MCU_CAL_OK)
        return false;

    /* ch 1 -> A (word low half), ch 2 -> B (word high half). Baselines are
     * words 0..29, uppers words 30..59 — stock indexes [range] and
     * [30 + range] within each half. */
    uint32_t w_base = cal_word(range);
    uint32_t w_up   = cal_word(30u + range);
    if (ch == 1u) {
        *base  = (uint16_t)(w_base & 0xFFFFu);
        *upper = (uint16_t)(w_up & 0xFFFFu);
    } else if (ch == 2u) {
        *base  = (uint16_t)(w_base >> 16);
        *upper = (uint16_t)(w_up >> 16);
    } else {
        return false;
    }
    return true;
}
