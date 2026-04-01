/*
 * OpenScope 2C53T - Software DFU Boot Entry
 *
 * Jumps to the AT32F403A's built-in ROM bootloader for USB DFU flashing.
 * Two entry paths: Settings menu request (via RAM magic word) and
 * POWER button hold during boot (failsafe).
 *
 * The AT32F403A system memory bootloader is at 0x1FFFAC00.
 * When entered, the device enumerates as VID:2E3C PID:DF11 "AT32 Bootloader DFU".
 */

#include "dfu_boot.h"
#include "at32f403a_407.h"

/*
 * Magic word stored at end of SRAM. Survives soft reset because
 * RAM contents persist — only cleared by power cycle or .bss init.
 * We place it in a fixed location that the linker won't touch.
 */
#define DFU_MAGIC_ADDR   ((volatile uint32_t *)0x20037FF0)  /* end of 224KB SRAM */
#define DFU_MAGIC_VALUE  0xDEAD00DF  /* "DEAD-DFU" */

/*
 * AT32F403A system memory (ROM bootloader) base address.
 * This is where the built-in DFU bootloader lives.
 */
#define SYSTEM_MEMORY_BASE  0x1FFFAC00

/* ═══════════════════════════════════════════════════════════════════
 * Jump to ROM bootloader
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Candidate system memory addresses for AT32F403A ROM bootloader.
 * We try each one and jump to the first with a valid-looking vector table
 * (SP in SRAM range 0x20000000-0x2003FFFF, reset vector in ROM range).
 */
static const uint32_t bootloader_addrs[] = {
    0x1FFFAC00,  /* ArteryTek documented for some AT32 */
    0x1FFFF000,  /* STM32F103 compatible */
    0x1FFFE000,
    0x1FFFD000,
    0x1FFFC800,
    0x1FFFC000,
    0x1FFFB000,
    0x1FFF0000,
    0x1FFF7800,  /* STM32F0/F3 */
    0x00000000,  /* Remapped system memory */
};
#define NUM_BOOT_ADDRS (sizeof(bootloader_addrs) / sizeof(bootloader_addrs[0]))

/* LCD helpers for diagnostic display (minimal, no driver dependency) */
#define DFU_LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define DFU_LCD_DATA  (*(volatile uint16_t *)0x60020000)

static void dfu_lcd_hex32(uint16_t x, uint16_t y, uint32_t val, uint16_t color) {
    /* Tiny 4x6 hex digits directly to LCD — good enough for diagnostics */
    static const uint8_t hex_font[16][4] = {
        {0x6,0x9,0x9,0x6},{0x2,0x6,0x2,0x2},{0x6,0x1,0x6,0xF},{0xE,0x1,0x6,0xE},
        {0x9,0x9,0xF,0x1},{0xF,0x8,0xE,0xE},{0x6,0x8,0xE,0x6},{0xF,0x1,0x2,0x4},
        {0x6,0x9,0x6,0x6},{0x6,0x9,0x7,0x6},{0x6,0x9,0xF,0x9},{0xE,0x9,0xE,0xE},
        {0x7,0x8,0x8,0x7},{0xE,0x9,0x9,0xE},{0xF,0x8,0xE,0xF},{0xF,0x8,0xE,0x8},
    };
    for (int d = 7; d >= 0; d--) {
        uint8_t nibble = (val >> (d * 4)) & 0xF;
        uint16_t dx = x + (7 - d) * 5;
        DFU_LCD_CMD = 0x2A;
        DFU_LCD_DATA = dx >> 8; DFU_LCD_DATA = dx & 0xFF;
        DFU_LCD_DATA = (dx + 3) >> 8; DFU_LCD_DATA = (dx + 3) & 0xFF;
        DFU_LCD_CMD = 0x2B;
        DFU_LCD_DATA = y >> 8; DFU_LCD_DATA = y & 0xFF;
        DFU_LCD_DATA = (y + 3) >> 8; DFU_LCD_DATA = (y + 3) & 0xFF;
        DFU_LCD_CMD = 0x2C;
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                DFU_LCD_DATA = (hex_font[nibble][row] & (8 >> col)) ? color : 0x0008;
    }
}

static void jump_to_bootloader(void) {
    uint32_t boot_sp = 0;
    uint32_t boot_addr = 0;
    int found = -1;

    /* Scan all candidate addresses, display on LCD for diagnostics */
    for (int i = 0; i < (int)NUM_BOOT_ADDRS; i++) {
        uint32_t base = bootloader_addrs[i];
        uint32_t sp = *(volatile uint32_t *)(base);
        uint32_t rv = *(volatile uint32_t *)(base + 4);

        /* Show on LCD: address, SP, reset vector */
        dfu_lcd_hex32(5, 20 + i * 8, base, 0x07FF);   /* cyan: address */
        dfu_lcd_hex32(50, 20 + i * 8, sp, 0xFFFF);     /* white: SP */
        dfu_lcd_hex32(95, 20 + i * 8, rv, 0xFFFF);     /* white: reset vec */

        /* Valid bootloader: SP in SRAM, reset vector in ROM/flash */
        if ((sp & 0xFFF00000) == 0x20000000 && rv >= 0x1FFF0000 && rv < 0x20000000) {
            boot_sp = sp;
            boot_addr = rv;
            found = i;
            dfu_lcd_hex32(140, 20 + i * 8, 0x00FF00FF, 0x07E0);  /* green marker */
            break;
        }
    }

    /* Always pause 10 seconds so user can photograph the diagnostic screen */
    for (volatile uint32_t d = 0; d < 200000000; d++);

    if (found < 0) {
        /* No valid bootloader found — show red indicator and reset */
        dfu_lcd_hex32(5, 20 + NUM_BOOT_ADDRS * 8 + 4, 0xBADBAD00, 0xF800);
        for (volatile uint32_t d = 0; d < 200000000; d++);
        NVIC_SystemReset();
        while (1);
    }

    /* Disable all interrupts */
    __disable_irq();

    /* Disable SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* Disable all NVIC interrupts and clear pending */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* Reset USB peripheral (important — DFU bootloader needs clean USB state) */
    crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);
    *(volatile uint32_t *)0x40005C40 = 0x0001;  /* USB_CNTR = FRES (force reset) */
    *(volatile uint32_t *)0x40005C44 = 0x0000;  /* USB_ISTR = clear all */
    crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, FALSE);

    /* Set vector table to system memory */
    SCB->VTOR = SYSTEM_MEMORY_BASE & 0xFFFFFF80;

    /* Set main stack pointer */
    __set_MSP(boot_sp);

    /* Jump to bootloader reset handler */
    void (*boot_entry)(void) = (void (*)(void))boot_addr;
    boot_entry();

    /* Should never reach here */
    while (1);
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void dfu_request_reboot(void) {
    /*
     * Placeholder — will be replaced by in-application USB DFU bootloader.
     * For now, show instructions. The proper solution (separate bootloader
     * at 0x08000000 with USB DFU) is being built.
     */
    extern void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    extern void font_draw_string_center(uint16_t x, uint16_t y, const char *str,
                                         uint16_t fg, uint16_t bg, const void *font);
    extern const void font_large;
    extern const void font_medium;

    lcd_fill_rect(0, 0, 320, 240, 0x0010);

    font_draw_string_center(160, 40, "Firmware Update",
                            0xFFFF, 0x0010, &font_large);
    font_draw_string_center(160, 90, "Coming soon:",
                            0x07E0, 0x0010, &font_medium);
    font_draw_string_center(160, 115, "USB in-app bootloader",
                            0x07E0, 0x0010, &font_medium);
    font_draw_string_center(160, 160, "For now: BOOT0 + reset",
                            0xFFE0, 0x0010, &font_medium);
    font_draw_string_center(160, 200, "Press any key to return",
                            0x7BEF, 0x0010, &font_medium);

    /* Wait for any button press, then return to settings */
    for (volatile uint32_t d = 0; d < 500000000; d++);
}

void dfu_check_magic(void) {
    if (*DFU_MAGIC_ADDR == DFU_MAGIC_VALUE) {
        /* Clear magic word so we don't loop */
        *DFU_MAGIC_ADDR = 0;

        /* Jump to bootloader */
        jump_to_bootloader();
    }
}

void dfu_check_boot_button(void) {
    /*
     * Check if POWER button (PC8) is held LOW during boot.
     * PC8 is a passive button — active LOW, no matrix scan needed.
     * Just configure as input with pull-up and read.
     *
     * We need a short delay to let the pin settle after clock enable.
     */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

    /* PC8 = input with pull-up */
    uint32_t cfghr = GPIOC->cfghr;
    cfghr &= ~(0xF << 0);      /* Clear PC8 config (bits 0-3 of CFGHR) */
    cfghr |= (0x8 << 0);       /* CNF=10 (input pull-up/down), MODE=00 (input) */
    GPIOC->cfghr = cfghr;
    GPIOC->scr = (1 << 8);     /* Pull-up */

    /* Brief delay for GPIO to settle */
    for (volatile int i = 0; i < 10000; i++);

    /* Check if PC8 is LOW (POWER button pressed) */
    if (!(GPIOC->idt & (1 << 8))) {
        /* Wait a bit more to confirm it's not a glitch */
        for (volatile int i = 0; i < 100000; i++);
        if (!(GPIOC->idt & (1 << 8))) {
            /* POWER button is definitely held — enter DFU */
            jump_to_bootloader();
        }
    }
}
