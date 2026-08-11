/*
 * OpenScope 2C53T bootloader updater app.
 *
 * This is intentionally an app-slot image, not a relaxed HID bootloader mode.
 * Old bootloaders can flash it at 0x08004000 and jump to it. Once running from
 * app flash, it validates the embedded bootloader payload, rewrites the 16KB
 * bootloader region, verifies the bytes, and resets into the new bootloader.
 *
 * Power loss during the erase/program window can still require ROM DFU/SWD
 * recovery. The host preflight must make that explicit before this image is
 * flashed and jumped.
 */

#include <stdint.h>
#include "at32f403a_407.h"
#include "at32f403a_407_flash.h"

#define FLASH_BASE_ADDRESS       0x08000000u
#define FLASH_APP_ADDRESS        0x08004000u
#define BOOTLOADER_REGION_SIZE   (FLASH_APP_ADDRESS - FLASH_BASE_ADDRESS)
#define SECTOR_SIZE              0x800u
#define RAM_MASK                 0xFFF00000u
#define RAM_BASE                 0x20000000u
#define FLASH_MAX_ADDRESS        0x08100000u
#define BOOT_COUNTER_ADDR        ((volatile uint32_t *)0x20037FDCu)
#define DFU_MAGIC_ADDR           ((volatile uint32_t *)0x20037FE0u)
#define DFU_MAGIC_VALUE          0xDEADBEEFu

extern const uint8_t bootloader_payload[];
extern const uint32_t bootloader_payload_size;
extern const char bootloader_updater_marker[];

static volatile uint32_t updater_error_code;

static uint32_t load_le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int valid_bootloader_payload(void)
{
    if (bootloader_payload_size < 8u || bootloader_payload_size > BOOTLOADER_REGION_SIZE) {
        return 0;
    }

    uint32_t sp = load_le32(&bootloader_payload[0]);
    uint32_t rv = load_le32(&bootloader_payload[4]);

    if ((sp & RAM_MASK) != RAM_BASE) {
        return 0;
    }
    if (rv < FLASH_BASE_ADDRESS || rv >= FLASH_APP_ADDRESS) {
        return 0;
    }
    if (((uintptr_t)bootloader_payload & 3u) != 0u) {
        return 0;
    }

    return 1;
}

static void fail(uint32_t code)
{
    updater_error_code = code;
    while (1) {
        __NOP();
    }
}

static void erase_bootloader_region(void)
{
    for (uint32_t addr = FLASH_BASE_ADDRESS; addr < FLASH_APP_ADDRESS; addr += SECTOR_SIZE) {
        if (flash_sector_erase(addr) != FLASH_OPERATE_DONE) {
            fail(0xE1000000u | addr);
        }
    }
}

static void program_bootloader_region(void)
{
    for (uint32_t offset = 0; offset < BOOTLOADER_REGION_SIZE; offset += 4u) {
        uint32_t word = 0xFFFFFFFFu;

        if (offset < bootloader_payload_size) {
            uint32_t remaining = bootloader_payload_size - offset;
            const uint8_t *p = &bootloader_payload[offset];
            if (remaining >= 4u) {
                word = load_le32(p);
            } else {
                word = 0xFFFFFFFFu;
                for (uint32_t i = 0; i < remaining; i++) {
                    word = (word & ~(0xFFu << (i * 8u))) | ((uint32_t)p[i] << (i * 8u));
                }
            }
        }

        if (flash_word_program(FLASH_BASE_ADDRESS + offset, word) != FLASH_OPERATE_DONE) {
            fail(0xE2000000u | offset);
        }
    }
}

static void verify_bootloader_region(void)
{
    const uint8_t *flash = (const uint8_t *)FLASH_BASE_ADDRESS;
    for (uint32_t i = 0; i < bootloader_payload_size; i++) {
        if (flash[i] != bootloader_payload[i]) {
            fail(0xE3000000u | i);
        }
    }
    for (uint32_t i = bootloader_payload_size; i < BOOTLOADER_REGION_SIZE; i++) {
        if (flash[i] != 0xFFu) {
            fail(0xE4000000u | i);
        }
    }
}

int main(void)
{
    (void)bootloader_updater_marker;

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    if (!valid_bootloader_payload()) {
        fail(0xE0000001u);
    }

    flash_unlock();
    erase_bootloader_region();
    program_bootloader_region();
    flash_lock();
    verify_bootloader_region();

    *BOOT_COUNTER_ADDR = 0u;
    *DFU_MAGIC_ADDR = DFU_MAGIC_VALUE;
    __DSB();
    NVIC_SystemReset();

    while (1) {
        __NOP();
    }
}
