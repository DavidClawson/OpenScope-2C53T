/*
 * OpenScope 2C53T high-recovery layout installer.
 *
 * This app-slot image is flashed through the existing HID bootloader. Once it
 * starts, it writes the high recovery bootloader first and the tiny low-flash
 * stage0 sector last. If power is lost before the last step, the old low
 * bootloader remains available; if power is lost during the stage0 sector
 * rewrite, ROM DFU/SWD recovery may be required.
 */

#include <stdint.h>
#include "at32f403a_407.h"
#include "at32f403a_407_flash.h"

#define FLASH_BASE_ADDRESS             0x08000000u
#define FLASH_APP_ADDRESS              0x08004000u
#define HIGH_DISPATCHER_ADDRESS        0x080E0000u
#define HIGH_BOOTLOADER_ADDRESS        0x080F0000u
#define FLASH_MAX_ADDRESS              0x08100000u
#define STAGE0_REGION_SIZE             0x800u
#define HIGH_BOOTLOADER_REGION_SIZE    (FLASH_MAX_ADDRESS - HIGH_BOOTLOADER_ADDRESS)
#define SECTOR_SIZE                    0x800u
#define RAM_MASK                       0xFFF00000u
#define RAM_BASE                       0x20000000u
#define BOOT_COUNTER_ADDR              ((volatile uint32_t *)0x20037FDCu)
#define DFU_MAGIC_ADDR                 ((volatile uint32_t *)0x20037FE0u)
#define DFU_MAGIC_VALUE                0xDEADBEEFu

extern const uint8_t stage0_payload[];
extern const uint32_t stage0_payload_size;
extern const uint8_t high_bootloader_payload[];
extern const uint32_t high_bootloader_payload_size;
extern const char high_layout_updater_marker[];

static volatile uint32_t updater_error_code;

static uint32_t load_le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void fail(uint32_t code)
{
    updater_error_code = code;
    while (1) {
        __NOP();
    }
}

static int valid_stage0_payload(void)
{
    if (stage0_payload_size < 8u || stage0_payload_size > STAGE0_REGION_SIZE) {
        return 0;
    }
    uint32_t sp = load_le32(&stage0_payload[0]);
    uint32_t rv = load_le32(&stage0_payload[4]);
    if ((sp & RAM_MASK) != RAM_BASE) {
        return 0;
    }
    if (!((rv >= FLASH_BASE_ADDRESS && rv < FLASH_BASE_ADDRESS + STAGE0_REGION_SIZE) ||
          (rv >= HIGH_DISPATCHER_ADDRESS && rv < HIGH_BOOTLOADER_ADDRESS))) {
        return 0;
    }
    if (((uintptr_t)stage0_payload & 3u) != 0u) {
        return 0;
    }
    return 1;
}

static int valid_high_bootloader_payload(void)
{
    if (high_bootloader_payload_size < 8u ||
        high_bootloader_payload_size > HIGH_BOOTLOADER_REGION_SIZE) {
        return 0;
    }
    uint32_t sp = load_le32(&high_bootloader_payload[0]);
    uint32_t rv = load_le32(&high_bootloader_payload[4]);
    if ((sp & RAM_MASK) != RAM_BASE) {
        return 0;
    }
    if (rv < HIGH_BOOTLOADER_ADDRESS || rv >= FLASH_MAX_ADDRESS) {
        return 0;
    }
    if (((uintptr_t)high_bootloader_payload & 3u) != 0u) {
        return 0;
    }
    return 1;
}

static void erase_region(uint32_t start, uint32_t size, uint32_t code_base)
{
    for (uint32_t addr = start; addr < start + size; addr += SECTOR_SIZE) {
        if (flash_sector_erase(addr) != FLASH_OPERATE_DONE) {
            fail(code_base | (addr & 0x000FFFFFu));
        }
    }
}

static void program_region(uint32_t start, uint32_t region_size, const uint8_t *payload,
                           uint32_t payload_size, uint32_t code_base)
{
    for (uint32_t offset = 0; offset < region_size; offset += 4u) {
        uint32_t word = 0xFFFFFFFFu;
        if (offset < payload_size) {
            uint32_t remaining = payload_size - offset;
            const uint8_t *p = &payload[offset];
            if (remaining >= 4u) {
                word = load_le32(p);
            } else {
                for (uint32_t i = 0; i < remaining; i++) {
                    word = (word & ~(0xFFu << (i * 8u))) | ((uint32_t)p[i] << (i * 8u));
                }
            }
        }
        if (flash_word_program(start + offset, word) != FLASH_OPERATE_DONE) {
            fail(code_base | offset);
        }
    }
}

static void verify_region(uint32_t start, uint32_t region_size, const uint8_t *payload,
                          uint32_t payload_size, uint32_t code_base)
{
    const uint8_t *flash = (const uint8_t *)start;
    for (uint32_t i = 0; i < payload_size; i++) {
        if (flash[i] != payload[i]) {
            fail(code_base | i);
        }
    }
    for (uint32_t i = payload_size; i < region_size; i++) {
        if (flash[i] != 0xFFu) {
            fail((code_base + 0x01000000u) | i);
        }
    }
}

int main(void)
{
    (void)high_layout_updater_marker;

    /* Keep the handheld powered while the installer rewrites flash. */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 4)) | (0x3u << 4);
    GPIOC->scr = (1u << 9);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    if (!valid_stage0_payload()) {
        fail(0xE0000001u);
    }
    if (!valid_high_bootloader_payload()) {
        fail(0xE0000002u);
    }

    flash_unlock();

    erase_region(HIGH_BOOTLOADER_ADDRESS, HIGH_BOOTLOADER_REGION_SIZE, 0xE1000000u);
    program_region(HIGH_BOOTLOADER_ADDRESS, HIGH_BOOTLOADER_REGION_SIZE,
                   high_bootloader_payload, high_bootloader_payload_size, 0xE2000000u);
    verify_region(HIGH_BOOTLOADER_ADDRESS, HIGH_BOOTLOADER_REGION_SIZE,
                  high_bootloader_payload, high_bootloader_payload_size, 0xE3000000u);

    erase_region(FLASH_BASE_ADDRESS, STAGE0_REGION_SIZE, 0xE5000000u);
    program_region(FLASH_BASE_ADDRESS, STAGE0_REGION_SIZE,
                   stage0_payload, stage0_payload_size, 0xE6000000u);
    verify_region(FLASH_BASE_ADDRESS, STAGE0_REGION_SIZE,
                  stage0_payload, stage0_payload_size, 0xE7000000u);

    flash_lock();

    *BOOT_COUNTER_ADDR = 0u;
    *DFU_MAGIC_ADDR = DFU_MAGIC_VALUE;
    __DSB();
    NVIC_SystemReset();

    while (1) {
        __NOP();
    }
}
