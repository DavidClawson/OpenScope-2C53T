/*
 * settings_store.h — the glue between live UI state and persisted settings.
 *
 * config.c owns the record format and the flash access; this file owns the
 * question "which live variables are settings, and when do we write them".
 * Keeping them apart is what lets config.c be host-tested against a NOR model
 * with no UI linked in.
 *
 * WHEN DOES IT WRITE (the interesting design question)
 * ---------------------------------------------------
 * Not on every keypress: adjusting the timebase with ten presses would be ten
 * records, and a record per keypress is how an append log turns into a
 * compaction (i.e. an erase) loop.
 *
 * Instead:
 *   - every button press captures live state and compares it against the last
 *     saved image. Unchanged -> nothing pending, no flash access at all;
 *   - a change starts (or restarts) a settle window, currently 2 s. The write
 *     happens on the first press after the window expires, so a burst of
 *     adjustments collapses into one record;
 *   - deliberate boundaries flush immediately, ignoring the window: switching
 *     device mode (MENU), and the power-off countdown just before PC9 drops.
 *
 * Everything is driven from the input task, on purpose. Every settings change
 * on this device originates from a button press, so the input task is where
 * the work belongs; it also keeps SPI2 flash traffic out of the FreeRTOS timer
 * service task, which is the highest-priority task in the system and also
 * feeds the watchdog. A nearly-full 64 KB append log takes a few thousand
 * short SPI reads to scan (~100 ms at the /16 prescaler), and that is not
 * something to do from the timer task.
 *
 * KNOWN GAP, stated rather than hidden: a change followed by no further button
 * press and no orderly power-off (battery pulled, auto power-off) is lost. The
 * settings are only ever as durable as the last press or the last flush point.
 */

#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

/* How long a change must be quiet before it is written. */
#define SETTINGS_STORE_SETTLE_MS  2000u

typedef struct {
    bool                 storage_bound;   /* region layer accepted the backend */
    config_load_result_t load_result;     /* how boot-time load ended          */
    uint32_t             writes;          /* successful config_save() calls    */
    uint32_t             write_failures;
    uint32_t             changes_seen;    /* captures that differed            */
} settings_store_status_t;

/* Bind the region layer to the W25Q, load the saved settings (or defaults) and
 * apply them to live UI state. Call once from main(), after theme_init() and
 * scope_state_init() and after flash_fs_init() (the region backend uses the
 * flash_fs raw primitives and their mutex).
 *
 * Never fails in a way the caller must handle: with no storage, or with an
 * unreadable/corrupt/foreign record, the device comes up on defaults. */
void settings_store_init(void);

/* Load the newest record (or defaults) and apply it. Split out of _init() so
 * host tests can drive it against a flash model. */
config_load_result_t settings_store_load_and_apply(void);

/* Capture live state; if it differs from the last saved image, start/restart
 * the settle window. Cheap: one struct compare, no flash access. Call at the
 * end of every button handled. */
void settings_store_note_change(uint32_t now_ms);

/* Write if a change has settled. Returns true if a record was written. */
bool settings_store_service(uint32_t now_ms);

/* Capture and write immediately if anything is pending, ignoring the settle
 * window. Use at deliberate boundaries (mode change, power off). Returns true
 * if a record was written. */
bool settings_store_flush(uint32_t now_ms);

/* Apply a config to live UI state, clamping every index to its live range. A
 * record can pass its CRC and its checksum and still hold, say, vdiv index 200
 * — from an older layout, a truncated struct, or a bit flip that the byte-sum
 * checksum happens to miss. Nothing here indexes a table with an unclamped
 * value. */
void settings_store_apply(const device_config_t *cfg);

/* Capture live UI state into *cfg (fields not owned by this layer are left
 * as-is), then stamp magic/version/checksum. */
void settings_store_capture(device_config_t *cfg);

const settings_store_status_t *settings_store_get_status(void);

#endif /* SETTINGS_STORE_H */
