/*
 * OpenScope 2C53T - Watchdog & System Health Monitor
 *
 * Uses the GD32F307 FWDGT (Free Watchdog Timer) with a cooperative
 * health-check pattern: the watchdog is only fed when ALL registered
 * tasks have checked in within the deadline. If any task hangs, the
 * system resets cleanly instead of freezing forever.
 *
 * This directly addresses community issues #1 (auto-measurement hang),
 * #2 (GUI lockup), and #7 (unexpected restarts/freezes).
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

/* Maximum number of tasks that can register with the health monitor */
#define HEALTH_MAX_TASKS    6

/* Deadline: if a task hasn't checked in for this many ticks, it's stalled */
#define HEALTH_DEADLINE_MS  2000

/* ═══════════════════════════════════════════════════════════════════
 * Watchdog (FWDGT) — low-level
 * ═══════════════════════════════════════════════════════════════════ */

/* Initialize FWDGT with ~3 second timeout (must be > HEALTH_DEADLINE_MS) */
void watchdog_init(void);

/* Feed the watchdog — only called by the health monitor, not by tasks */
void watchdog_feed(void);

/* ═══════════════════════════════════════════════════════════════════
 * Task Health Monitor — cooperative check-in pattern
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    const char    *name;               /* Task name (for diagnostics) */
    TaskHandle_t   handle;             /* FreeRTOS task handle */
    TickType_t     last_checkin;        /* Tick count of last check-in */
    UBaseType_t    stack_hwm;           /* Stack high water mark (words) */
    bool           registered;         /* Slot in use */
} task_health_t;

/* Register a task for health monitoring. Returns slot index or -1 on failure. */
int health_register(const char *name, TaskHandle_t handle);

/* Task check-in: call this periodically from each task's main loop */
void health_checkin(int slot);

/* Run one health check cycle: update HWMs, check deadlines, feed or starve WDT.
 * Called from a timer callback or dedicated low-priority task. */
void health_check(void);

/* Get read-only pointer to task health array (for diagnostics display) */
const task_health_t *health_get_tasks(void);

/* Get count of registered tasks */
int health_get_count(void);

/* Check if a specific task is stalled */
bool health_is_stalled(int slot);

/* ═══════════════════════════════════════════════════════════════════
 * Fault display — show error on LCD before reset
 * ═══════════════════════════════════════════════════════════════════ */

/* Display a fatal error on the LCD (red screen of death).
 * This bypasses FreeRTOS and writes directly to the LCD. */
void fault_display(const char *title, const char *detail);

#endif /* WATCHDOG_H */
