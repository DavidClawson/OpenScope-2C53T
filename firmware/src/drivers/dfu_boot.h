/*
 * OpenScope 2C53T - Software DFU Boot Entry
 *
 * Provides two ways to enter the AT32's built-in USB DFU bootloader
 * without needing physical access to the BOOT0 pad:
 *
 *   1. dfu_request_reboot() — called from Settings menu "Firmware Update"
 *      Writes a magic word to RAM, resets, early boot code detects it
 *      and jumps to the ROM bootloader.
 *
 *   2. dfu_check_boot_button() — called very early in main(), before LCD init.
 *      If POWER button (PC8) is held LOW during power-on, jumps to bootloader.
 *      This is the failsafe for bricked firmware.
 *
 * After entering DFU mode, flash with:
 *   dfu-util -a 0 -d 2e3c:df11 -s 0x08000000:leave -D firmware.bin
 */

#ifndef DFU_BOOT_H
#define DFU_BOOT_H

/*
 * Request reboot into DFU mode. Writes magic word to RAM and resets.
 * This function does not return.
 */
void dfu_request_reboot(void);

/*
 * Check if POWER button is held during boot. If so, enter DFU mode.
 * Call this very early in main(), after power hold but before LCD init.
 * Only enters DFU if button is held; returns normally otherwise.
 */
void dfu_check_boot_button(void);

/*
 * Check for the DFU magic word in RAM (set by dfu_request_reboot).
 * If found, clears it and jumps to bootloader. Call before main().
 * Returns normally if no magic word found.
 */
void dfu_check_magic(void);

#endif /* DFU_BOOT_H */
