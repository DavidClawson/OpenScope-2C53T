/*
 * OpenScope 2C53T - DMM auto-selection helpers
 */

#include "meter_auto.h"
#include "fpga_meter_plan.h"

static const uint8_t meter_auto_candidate_order[] = {
    /*
     * AUTO is live hardware probing, not just parser scoring.  Keep the
     * default candidate list to the V/Ohm/C jack voltage functions until a
     * stock/live safety path proves how to detect the current jack and a
     * de-energized passive probe condition.  Current and passive submodes are
     * still supported when the user selects them explicitly, and their frames
     * remain covered by the state-machine tests; they are not swept by AUTO on
     * an unknown bench input.
     */
    0, 1
};

const uint8_t *meter_auto_candidates(size_t *count)
{
    if (count) {
        *count = sizeof(meter_auto_candidate_order) /
                 sizeof(meter_auto_candidate_order[0]);
    }
    return meter_auto_candidate_order;
}

static bool reading_has_ac_evidence(const meter_reading_t *r)
{
    /*
     * Stock evidence currently proves frame[7].0 as an ACV decimal-format
     * selector, not frame[7].2 as an AC-present bit. `is_ac` mirrors that
     * status byte for diagnostics only; auto-selection confidence must stay
     * tied to the independent companion frequency hint so DC input cannot
     * become a confident ACV/ACA candidate through a status-bit echo.
     * Keep this window aligned with meter_data.c's parser evidence boundary:
     * values outside the observed mains-like companion range are diagnostic
     * only, not AC confidence for autoscan.
     */
    return r->aux_freq_hz >= 45.0f && r->aux_freq_hz <= 65.0f;
}

static bool reading_has_clean_frame_family(uint8_t submode,
                                           const meter_reading_t *r)
{
    uint8_t expected = (uint8_t)fpga_meter_frame_family_for_submode(submode);

    return r->reject_reason == METER_REJECT_NONE &&
           r->expected_frame_family == expected &&
           r->observed_frame_family == expected;
}

uint8_t meter_auto_score(uint8_t submode, const meter_reading_t *r)
{
    if (!fpga_meter_submode_is_valid(submode)) return 0;
    if (!r || !r->valid || r->submode != submode) return 0;
    if (!reading_has_clean_frame_family(submode, r)) return 0;

    if (r->result_class == METER_RESULT_NORMAL) {
        switch (submode) {
        case 0:
            return 90U;
        case 1:
            return reading_has_ac_evidence(r) ? 90U : 0U;
        case 6:
        case 7:
            return 70U;
        case 8:
        case 9:
        case 10:
            return 60U;
        case 2:
        case 3:
            return !reading_has_ac_evidence(r) ? 50U : 0U;
        case 4:
        case 5:
            return reading_has_ac_evidence(r) ? 50U : 0U;
        default:
            return 10U;
        }
    }

    if (r->result_class == METER_RESULT_CONTINUITY) return 80U;
    return 0;
}
