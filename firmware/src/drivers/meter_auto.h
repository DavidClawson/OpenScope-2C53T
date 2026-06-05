/*
 * OpenScope 2C53T - DMM auto-selection helpers
 */

#ifndef METER_AUTO_H
#define METER_AUTO_H

#include <stddef.h>
#include <stdint.h>

#include "meter_data.h"

const uint8_t *meter_auto_candidates(size_t *count);
uint8_t meter_auto_score(uint8_t submode, const meter_reading_t *reading);

#endif /* METER_AUTO_H */
