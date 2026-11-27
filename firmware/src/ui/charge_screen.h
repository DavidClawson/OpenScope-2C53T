/*
 * OpenScope 2C53T - charge screen (the "off but on USB" state)
 *
 * Shown when USB powered the unit up, or after a power-off on USB, with PC9
 * LOW so the battery path stays released: unplugging USB switches the unit
 * off for real. Runs before the scheduler, bare-metal, and returns once
 * POWER has been held for CHARGE_SCREEN_HOLD_MS. The caller then latches PC9
 * and boots normally. See power.h for the stock behaviour it mirrors.
 */

#ifndef CHARGE_SCREEN_H
#define CHARGE_SCREEN_H

#include <stdbool.h>

#define CHARGE_SCREEN_HOLD_MS  300u

/* Needs the LCD initialised and PB8 (backlight) configured as an output.
 * need_release: POWER must be seen up once before a hold counts (set after
 * our own power-off, where the user is still holding the button). */
void charge_screen_run(bool need_release);

#endif /* CHARGE_SCREEN_H */
