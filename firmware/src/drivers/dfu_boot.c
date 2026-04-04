/*
 * OpenScope 2C53T - DFU Boot Entry
 *
 * Writes a magic word to a fixed RAM address, then resets.
 * The bootloader at 0x08000000 checks this same address on startup.
 *
 * Address 0x20037FE0 is chosen to be:
 *   - Below _estack (0x20037FF0) — not touched by initial SP
 *   - Well above BSS end (~0x20036914) — not zeroed by startup
 *   - In the stack growth area, but only used momentarily before reset
 */

#include "dfu_boot.h"
#include "at32f403a_407.h"

#define DFU_MAGIC_ADDR   ((volatile uint32_t *)0x20037FE0)
#define DFU_MAGIC_VALUE  0xDEADBEEF

/* Boot validation counter — must match bootloader's definition */
#define BOOT_COUNTER_ADDR    ((volatile uint32_t *)0x20037FDC)

void dfu_request_reboot(void) {
    *DFU_MAGIC_ADDR = DFU_MAGIC_VALUE;
    __DSB();
    NVIC_SystemReset();
    while (1);
}

void dfu_check_magic(void) {
}

void dfu_check_boot_button(void) {
}

/* Must match bootloader's definition */
#define BOOT_COUNTER_MAGIC   0xB007F000

void boot_validate(void) {
    /* Reset the boot attempt counter to zero while preserving the magic
     * marker. The bootloader checks (val & 0xFFFF0000) == BOOT_COUNTER_MAGIC
     * to recognize a valid counter.
     *
     * Previously this wrote plain 0, which broke the 3-strike system:
     * the bootloader wouldn't recognize 0 as a valid counter, so the
     * fail count never accumulated across crashes that happen AFTER
     * boot_validate() (e.g., FreeRTOS task crashes, watchdog resets).
     *
     * With the magic preserved, the flow is:
     *   Boot 1: bootloader writes MAGIC|1, app writes MAGIC|0 (success)
     *   Boot 2: bootloader writes MAGIC|1, app crashes → watchdog reset
     *   Boot 3: bootloader writes MAGIC|2, app crashes → watchdog reset
     *   Boot 4: bootloader sees count=3 ≥ BOOT_FAIL_MAX → safe mode */
    *BOOT_COUNTER_ADDR = BOOT_COUNTER_MAGIC;  /* count = 0, magic preserved */
    __DSB();
}
