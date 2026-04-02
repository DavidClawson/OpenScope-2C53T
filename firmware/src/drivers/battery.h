/*
 * OpenScope 2C53T - Battery Voltage Monitor
 * PB1 / ADC1 Channel 9, 239.5-cycle sample time
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

/* Initialize ADC1 for battery voltage reading on PB1 */
void battery_adc_init(void);

/* Call once per second to take a reading and update state */
void battery_update(void);

/* Read averaged battery voltage in millivolts (no ADC access, returns cached) */
uint16_t battery_read_mv(void);

/* Get battery percentage estimate (0-100) */
uint8_t battery_percent(void);

/* Returns 1 if USB charging detected (voltage > 4.3V) */
uint8_t battery_is_charging(void);

/* Returns 1 if voltage critically low (< 3.3V) — should auto-shutdown */
uint8_t battery_is_critical(void);

#endif
