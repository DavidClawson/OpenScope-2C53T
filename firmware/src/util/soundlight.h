/*
 * soundlight.h - Sound and Light settings (menu item 1).
 *
 * Two settings, one home each:
 *   - brightness: 0-100 percent, driven as PWM on PB8 (TMR4_CH3, the
 *     LCD backlight pin stock PWM-dims; OpenScope had it as a plain output
 *     HIGH - see main.c and docs/stock_vs_openscope.md "Backlight dimming").
 *   - sound: on/off, gating the continuity buzzer task (PB9, TMR4_CH4 in
 *     stock; issue #25).
 *
 * The two device_config_t fields (display_brightness, sound_enabled) have
 * been stored and round-tripped since the v3 record existed but nothing
 * owned them - config.h says so. This module is that owner.
 *
 * State lives here (not in UI code) so the settings store can capture/apply
 * it and host tests can drive it with no UI linked in.
 */

#ifndef SOUNDLIGHT_H
#define SOUNDLIGHT_H

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

/* Apply cfg->display_brightness / cfg->sound_enabled to live state and push
 * them to the hardware. Range-clamps like settings_store_apply() does for
 * every other restored field. Call at boot (after settings_store_init())
 * and from host tests. */
void soundlight_apply(const device_config_t *cfg);

/* Capture live state into cfg (the settings-store capture hook). */
void soundlight_capture(device_config_t *cfg);

/* Runtime setters - clamp, remember, and drive the hardware. */
void soundlight_set_brightness(uint8_t percent);
void soundlight_set_sound(bool on);

/* Adjust the selected Sound and Light row: row 0 steps brightness by
 * 20 (wrapping), row 1 toggles sound (with a confirmation beep when it
 * lands on). Rows beyond the menu do nothing. */
void soundlight_adjust(int row, int dir);

/* Runtime getters. */
uint8_t soundlight_get_brightness(void);
bool soundlight_get_sound(void);

/* Short confirmation beep through the continuity buzzer (no-op when sound
 * is off). Used as menu feedback so the setting is audible at the moment
 * it changes, exactly like stock's Sound and Light screen. */
void soundlight_beep(void);

/* True when beeps are enabled (sound on). The continuity buzzer task
 * consults this so one switch gates both the preview beep and the
 * continuity beep. */
bool soundlight_beep_ok(void);

#endif /* SOUNDLIGHT_H */
