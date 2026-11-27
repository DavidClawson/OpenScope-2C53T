/*
 * OpenScope 2C53T - power latch, boot classification and power-off
 *
 * The supply has two sources. The battery path is latched by PC9 (power
 * hold): the POWER button bridges it while held, and the MCU must drive PC9
 * HIGH before the button is released. USB VBUS powers the board on its own,
 * whatever PC9 does. Consequences:
 *
 *   - plugging USB into a switched-off unit boots the MCU with nobody
 *     touching POWER;
 *   - dropping PC9 on USB does not remove power, so a power-off just hangs.
 *
 * Stock handles both with a charge screen inside master_init (V1.2.0 flash
 * 0x0802B350..0x0802B40C): PC9 is driven LOW at entry (0x0802AAB6), a
 * battery animation runs while PC8 (POWER, active LOW) reads HIGH, and PC9
 * goes HIGH only once POWER is seen LOW twice (0x0802B412). Stock's power-off
 * is PC9 LOW followed directly by NVIC_SystemReset (0x08029D3A ->
 * 0x0802F314): on battery the rail is gone before the reset finishes, on USB
 * the reset lands back on the charge screen.
 *
 * This module does the same, with one deliberate difference: only a genuine
 * power-on (POR without a later software or watchdog reset) or our own
 * power-off lands on the charge screen. A pinhole reset, the jump into the
 * app after an IAP flash, the software reset after a CDC flash, and a
 * watchdog reset boot straight into the app as before, so the case-closed
 * bench loop is unchanged.
 *
 * The IAP case needs its own signal because the factory bootloader does not
 * reset after a flash: it jumps to the app (0x08002784, from the MSC write
 * path), so the reset flags are still those of whatever reset entered
 * upgrade mode, which is a POR on a cold entry. What marks it is PC9: the
 * factory IAP drives PC9 LOW before its normal jump (0x0800096E) and HIGH
 * only in upgrade mode (0x080029B6), so the app finds it already latched.
 */

#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POWER_BOOT_RUN    = 0,  /* latch PC9 and boot the instrument */
    POWER_BOOT_CHARGE = 1,  /* leave PC9 LOW, show the charge screen */
} power_boot_t;

/* CRM_CTRLSTS reset flags (AT32F403A RM, CRM_CTRLSTS bits 26..31). */
#define POWER_RSTF_NRST   (1u << 26)
#define POWER_RSTF_POR    (1u << 27)
#define POWER_RSTF_SW     (1u << 28)
#define POWER_RSTF_WDT    (1u << 29)
#define POWER_RSTF_WWDT   (1u << 30)
#define POWER_RSTF_LP     (1u << 31)

/* Pure decision, host-tested (tests/test_power_boot.c).
 *   ctrlsts     snapshot of CRM_CTRLSTS taken before the flags are cleared
 *   power_held  POWER (PC8) read pressed at entry
 *   charge_req  our own power-off asked for the charge screen
 *   latched     PC9 was already an output driven HIGH at entry: the
 *               factory IAP jumped here from upgrade mode (GUEST_BUILD only;
 *               our HID bootloader latches PC9 on every boot) */
power_boot_t power_boot_classify(uint32_t ctrlsts, bool power_held,
                                 bool charge_req, bool latched);

/* Our power-off request counts only if the rail stayed up since (no POR). */
bool power_charge_req_valid(uint32_t ctrlsts, bool charge_req);

#ifndef POWER_HOST_TEST

/* First call in main(). Reads POWER and the reset cause, clears the reset
 * flags and the power-off request, then drives PC9: HIGH for RUN (as fast
 * as the old unconditional latch), LOW for CHARGE. */
power_boot_t power_boot_decide(void);

/* True when the CHARGE decision came from our own power-off. POWER is then
 * probably still held from the 3 s hold, and the charge screen must see it
 * released before a new hold can count. */
bool power_boot_after_poweroff(void);

/* PC9 HIGH. Used when the charge screen hands over to a real boot. */
void power_latch_on(void);

/* PC9 LOW, then a system reset, like stock. On battery the rail drops
 * during the wait; on USB the reset lands on the charge screen. Safe from a
 * FreeRTOS task: it only busy-waits. */
void power_off(void) __attribute__((noreturn));

/* POWER (PC8) is active LOW with a pull-up (stock's charge-mode posture,
 * analysis_v120/swd_chargemode_baseline.txt; our button_scan agrees). */
bool power_button_held(void);

#endif /* POWER_HOST_TEST */

#endif /* POWER_H */
