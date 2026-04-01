/*
 * OpenScope 2C53T - Button Matrix Scan Driver
 *
 * Hardware-verified bidirectional 4x3 GPIO matrix scan for the
 * FNIRSI 2C53T's 15 physical buttons. Runs in TMR3 ISR at 500Hz
 * with 70-tick (140ms) debounce, matching stock firmware exactly.
 *
 * Matrix pins:
 *   Rows: PA7, PB0, PC5, PE2
 *   Cols: PA8, PC10, PE3
 *   Passive: PC8 (POWER), PB7 (PRM), PC13 (UP)
 *
 * Usage:
 *   button_scan_init(queue_handle);  // call once, before starting scheduler
 *   // TMR3 ISR runs automatically, sends button_id_t to the queue
 */

#ifndef BUTTON_SCAN_H
#define BUTTON_SCAN_H

#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "ui.h"  /* for button_id_t */

/*
 * Initialize TMR3 at 500Hz and configure GPIO for matrix scanning.
 * Confirmed button presses are sent as button_id_t values to the
 * provided FreeRTOS queue.
 */
void button_scan_init(QueueHandle_t button_queue);

/*
 * Get the raw 15-bit scan state (for debug display).
 * Each bit corresponds to a button position (see button_map_confirmed.md).
 */
uint16_t button_scan_get_raw(void);

#endif /* BUTTON_SCAN_H */
