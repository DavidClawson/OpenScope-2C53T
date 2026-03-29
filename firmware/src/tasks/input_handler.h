/*
 * OpenScope 2C53T - Input Handler
 *
 * Processes debounced button presses and dispatches actions
 * based on current device mode. Extracted from main.c to keep
 * the input task thin and the button logic testable.
 */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "ui.h"
#include "FreeRTOS.h"
#include "queue.h"

/*
 * Handle a debounced button press.
 *
 * Reads the current device mode, scope state, and settings state
 * to determine the appropriate action. Sends display commands
 * via the provided queue.
 *
 * Returns the display command sent (or 0 if none).
 */
uint8_t input_handle_button(button_id_t button, QueueHandle_t display_queue);

/*
 * Handle OK press in settings menu (top-level and sub-menus).
 * Called internally by input_handle_button, but exposed for testing.
 */
void input_handle_settings_ok(void);

#endif /* INPUT_HANDLER_H */
