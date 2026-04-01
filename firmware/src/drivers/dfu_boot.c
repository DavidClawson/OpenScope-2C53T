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
