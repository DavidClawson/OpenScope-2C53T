/*
 * OpenScope 2C53T - DMM auto-selection helpers
 */

#include "meter_auto.h"

static const uint8_t meter_auto_candidate_order[] = {
    0, 1, 6, 7, 8, 9, 2, 4, 3, 5
};

const uint8_t *meter_auto_candidates(size_t *count)
{
    if (count) {
        *count = sizeof(meter_auto_candidate_order) /
                 sizeof(meter_auto_candidate_order[0]);
    }
    return meter_auto_candidate_order;
}

uint8_t meter_auto_score(uint8_t submode, const meter_reading_t *r)
{
    if (!r || !r->valid || r->submode != submode) return 0;

    if (r->result_class == METER_RESULT_NORMAL) {
        switch (submode) {
        case 0:
            return (r->bcd_value > 0) ? 90U : 0U;
        case 1:
            return (r->bcd_value > 0 && r->aux_freq_hz >= 1.0f) ? 90U : 0U;
        case 6:
        case 7:
            return (r->bcd_value > 0) ? 70U : 0U;
        case 8:
        case 9:
            return (r->bcd_value > 0) ? 60U : 0U;
        case 2:
        case 3:
        case 4:
        case 5:
            return (r->bcd_value > 0) ? 50U : 0U;
        default:
            return 10U;
        }
    }

    if (r->result_class == METER_RESULT_CONTINUITY) return 80U;
    return 0;
}
