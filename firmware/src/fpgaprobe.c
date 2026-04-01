/*
 * FPGA USB Peripheral Probe for FNIRSI 2C53T
 *
 * Tests whether the FPGA communicates via the USB peripheral hardware.
 * The decompiled firmware heavily accesses:
 *   0x40005C00 — USB endpoint registers (EP0-EP7)
 *   0x40006000 — USB packet buffer SRAM (512 bytes)
 *
 * USB D+/D- are PA11/PA12 — may be wired to FPGA I/O.
 *
 * Phase 0: Read USB registers raw (no init)
 * Phase 1: Enable USB clock, read again
 * Phase 2: Dump USB packet buffer (0x40006000, 64 bytes)
 * Phase 3: Read PA11/PA12 as GPIO + try USB register patterns from decompiled code
 * Phase 4: Monitor — watch for changes
 *
 * Build: make -f Makefile.hwtest TEST=fpgaprobe
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"
#include <string.h>

extern void system_clock_config(void);

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

static void delay_ms(uint32_t ms) {
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;
        while (count--) __asm volatile("nop");
    }
}

static void lcd_bus_delay(void) {
    volatile uint32_t i = 50;
    while (i--) __asm volatile("nop");
}

static void lcd_cmd_wr(uint8_t cmd) { LCD_CMD = cmd; lcd_bus_delay(); }
static void lcd_dat(uint8_t data) { LCD_DATA = data; lcd_bus_delay(); }
static void lcd_dat16(uint16_t data) { LCD_DATA = data; }

static void gpio_pin_cfg(uint32_t base, uint8_t pin, uint8_t mode, uint8_t cnf) {
    volatile uint32_t *reg = (pin < 8) ?
        (volatile uint32_t *)(base + 0x00) :
        (volatile uint32_t *)(base + 0x04);
    uint8_t pos = (pin < 8) ? pin : (pin - 8);
    uint32_t val = *reg;
    val &= ~(0xFU << (pos * 4));
    val |= (((mode) | ((cnf) << 2)) << (pos * 4));
    *reg = val;
}

static const uint8_t font5x7[][5] = {
    {0x7C,0x8A,0x92,0xA2,0x7C}, {0x00,0x42,0xFE,0x02,0x00},
    {0x46,0x8A,0x92,0x92,0x62}, {0x44,0x82,0x92,0x92,0x6C},
    {0x18,0x28,0x48,0xFE,0x08}, {0xE4,0xA2,0xA2,0xA2,0x9C},
    {0x3C,0x52,0x92,0x92,0x0C}, {0x80,0x8E,0x90,0xA0,0xC0},
    {0x6C,0x92,0x92,0x92,0x6C}, {0x60,0x92,0x92,0x94,0x78},
    {0x7C,0x90,0x90,0x90,0x7C}, {0xFE,0x92,0x92,0x92,0x6C},
    {0x7C,0x82,0x82,0x82,0x44}, {0xFE,0x82,0x82,0x82,0x7C},
    {0xFE,0x92,0x92,0x92,0x82}, {0xFE,0x90,0x90,0x90,0x80},
};

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    lcd_cmd_wr(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x+w-1) >> 8); lcd_dat((x+w-1) & 0xFF);
    lcd_cmd_wr(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y+h-1) >> 8); lcd_dat((y+h-1) & 0xFF);
    lcd_cmd_wr(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) lcd_dat16(color);
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph,
                           uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_set_window(x, y, 5*s, 7*s);
    for (int r = 0; r < 7; r++)
        for (int sr = 0; sr < s; sr++)
            for (int c = 0; c < 5; c++)
                for (int sc = 0; sc < s; sc++)
                    lcd_dat16((glyph[c] & (1<<(7-r))) ? fg : bg);
}

static void lcd_hex8(uint16_t x, uint16_t y, uint8_t val, uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_draw_glyph(x, y, font5x7[(val >> 4) & 0xF], fg, bg, s);
    lcd_draw_glyph(x + 6*s, y, font5x7[val & 0xF], fg, bg, s);
}

/* Show a 32-bit value as 8 hex digits */
static void lcd_hex32(uint16_t x, uint16_t y, uint32_t val, uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_hex8(x, y, (val >> 24) & 0xFF, fg, bg, s);
    lcd_hex8(x + 12*s, y, (val >> 16) & 0xFF, fg, bg, s);
    lcd_hex8(x + 24*s, y, (val >> 8) & 0xFF, fg, bg, s);
    lcd_hex8(x + 36*s, y, val & 0xFF, fg, bg, s);
}

#define BG    0x0008
#define WHITE 0xFFFF
#define GREEN 0x07E0
#define CYAN  0x07FF
#define YELLO 0xFFE0
#define RED   0xF800
#define GRAY  0x4208
#define ORNG  0xFBE0
#define MAGNT 0xF81F

/*
 * USB peripheral register map on AT32F403A:
 *   0x40005C00 + n*4: USB_EPnR (endpoint n register), n=0..7
 *   0x40005C40: USB_CNTR  (control register)
 *   0x40005C44: USB_ISTR  (interrupt status)
 *   0x40005C48: USB_FNR   (frame number)
 *   0x40005C4C: USB_DADDR (device address)
 *   0x40005C50: USB_BTABLE (buffer table address)
 *
 *   0x40006000-0x400063FF: USB packet buffer memory (512 bytes, word-aligned access)
 */
#define USB_BASE    0x40005C00
#define USB_PBUF    0x40006000

static volatile uint32_t *usb_reg(uint32_t offset) {
    return (volatile uint32_t *)(USB_BASE + offset);
}

static volatile uint32_t *usb_pbuf(uint32_t offset) {
    return (volatile uint32_t *)(USB_PBUF + offset);
}

int main(void) {
    /* Power hold */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    system_clock_config();

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1 << 8);

    /* LCD init */
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
    for (int i = 0; i < 12; i++)
        gpio_pin_cfg(GPIOD_BASE, pd_pins[i], 3, 2);
    for (int i = 7; i <= 15; i++)
        gpio_pin_cfg(GPIOE_BASE, i, 3, 2);

    *(volatile uint32_t *)0xA0000000 = 0x00005010;
    *(volatile uint32_t *)0xA0000004 = 0x02020424;
    *(volatile uint32_t *)0xA0000104 = 0x00000202;
    *(volatile uint32_t *)0xA0000000 |= 0x0001;
    delay_ms(50);

    lcd_cmd_wr(0x01); delay_ms(200);
    lcd_cmd_wr(0x11); delay_ms(200);
    lcd_cmd_wr(0x36); lcd_bus_delay(); lcd_dat(0xA0); delay_ms(10);
    lcd_cmd_wr(0x3A); lcd_bus_delay(); lcd_dat(0x55); delay_ms(10);
    lcd_cmd_wr(0x29); delay_ms(50);

    wdt_register_write_enable(TRUE);
    wdt_divider_set(WDT_CLK_DIV_64);
    wdt_reload_value_set(1875);
    wdt_counter_reload();
    wdt_enable();

    lcd_fill(0, 0, 320, 240, BG);

    /* ================================================================
     * PHASE 0: Read USB registers BEFORE enabling USB clock
     * This tells us what the hardware state is at reset
     * ================================================================ */
    lcd_hex8(5, 2, 0x00, ORNG, BG, 2);

    /* Read EP0-EP7 registers */
    uint16_t y = 20;
    for (int i = 0; i < 8; i++) {
        uint32_t val = *usb_reg(i * 4);
        uint16_t c = (val != 0) ? YELLO : GRAY;
        lcd_hex8(5, y, i, MAGNT, BG, 2);
        lcd_hex32(35, y, val, c, BG, 2);
        y += 16;
    }
    /* Read control registers */
    uint32_t cntr = *usb_reg(0x40);
    uint32_t istr = *usb_reg(0x44);
    uint32_t fnr  = *usb_reg(0x48);
    uint32_t daddr = *usb_reg(0x4C);
    uint32_t btable = *usb_reg(0x50);

    y += 4;
    lcd_hex32(5, y, cntr, (cntr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(110, y, istr, (istr != 0) ? CYAN : GRAY, BG, 2);
    y += 16;
    lcd_hex32(5, y, fnr, (fnr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(110, y, daddr, (daddr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(215, y, btable, (btable != 0) ? CYAN : GRAY, BG, 2);

    /* Hold 8 seconds */
    for (int i = 0; i < 8; i++) { wdt_counter_reload(); delay_ms(1000); }

    /* ================================================================
     * PHASE 1: Enable USB clock, read registers again
     * ================================================================ */
    lcd_fill(0, 0, 320, 240, BG);
    lcd_hex8(5, 2, 0x01, ORNG, BG, 2);

    /* Enable USB peripheral clock */
    crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);
    delay_ms(50);

    /* Read EP0-EP7 */
    y = 20;
    for (int i = 0; i < 8; i++) {
        uint32_t val = *usb_reg(i * 4);
        uint16_t c = (val != 0) ? YELLO : GRAY;
        lcd_hex8(5, y, i, MAGNT, BG, 2);
        lcd_hex32(35, y, val, c, BG, 2);
        y += 16;
    }
    /* Control regs */
    cntr = *usb_reg(0x40);
    istr = *usb_reg(0x44);
    fnr  = *usb_reg(0x48);
    daddr = *usb_reg(0x4C);
    btable = *usb_reg(0x50);

    y += 4;
    lcd_hex32(5, y, cntr, (cntr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(110, y, istr, (istr != 0) ? CYAN : GRAY, BG, 2);
    y += 16;
    lcd_hex32(5, y, fnr, (fnr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(110, y, daddr, (daddr != 0) ? CYAN : GRAY, BG, 2);
    lcd_hex32(215, y, btable, (btable != 0) ? CYAN : GRAY, BG, 2);

    /* Hold 8 seconds */
    for (int i = 0; i < 8; i++) { wdt_counter_reload(); delay_ms(1000); }

    /* ================================================================
     * PHASE 2: Dump USB packet buffer memory (0x40006000)
     * Read first 128 bytes (64 16-bit words)
     * On AT32, packet buffer is accessed as 16-bit at 32-bit aligned addrs
     * ================================================================ */
    lcd_fill(0, 0, 320, 240, BG);
    lcd_hex8(5, 2, 0x02, ORNG, BG, 2);

    y = 20;
    int any_nonzero = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int idx = row * 8 + col;
            /* USB packet buffer: 16-bit values at 32-bit aligned addresses */
            uint16_t val = *(volatile uint16_t *)(USB_PBUF + idx * 4);
            uint16_t c = GRAY;
            if (val != 0x0000 && val != 0xFFFF) { c = YELLO; any_nonzero = 1; }
            else if (val == 0xFFFF) { c = RED; any_nonzero = 1; }
            lcd_hex8(5 + col * 38, y, (val >> 8) & 0xFF, c, BG, 2);
            lcd_hex8(5 + col * 38 + 15, y, val & 0xFF, c, BG, 1);
        }
        y += 18;
    }

    /* Also read PA11/PA12 as GPIO inputs to see USB line state */
    y += 8;
    gpio_pin_cfg(GPIOA_BASE, 11, 0, 1);  /* PA11 = input floating (USB D-) */
    gpio_pin_cfg(GPIOA_BASE, 12, 0, 1);  /* PA12 = input floating (USB D+) */
    delay_ms(10);
    uint32_t pa_idr = GPIOA->idt;
    uint8_t pa11 = (pa_idr >> 11) & 1;  /* USB D- */
    uint8_t pa12 = (pa_idr >> 12) & 1;  /* USB D+ */
    lcd_hex8(5, y, 0xA1, MAGNT, BG, 2);     /* "A1" = PA11 */
    lcd_hex8(35, y, pa11, pa11 ? GREEN : RED, BG, 2);
    lcd_hex8(80, y, 0xA2, MAGNT, BG, 2);    /* "A2" = PA12 */
    lcd_hex8(110, y, pa12, pa12 ? GREEN : RED, BG, 2);

    /* Read all of GPIOA IDR for reference */
    lcd_hex32(160, y, pa_idr, WHITE, BG, 2);

    /* Hold 15 seconds */
    for (int i = 0; i < 15; i++) { wdt_counter_reload(); delay_ms(1000); }

    /* ================================================================
     * PHASE 3: Try decompiled USB register patterns
     * The decompiled code does things like:
     *   *(0x40005C00 + ch*4) & 0x8f0f  — mask EP register
     *   *(0x40005C00 + ch*4) ^ 0xb080  — toggle bits
     *   *(0x40006000 + offset) = data   — write to packet buffer
     * Let's try writing and see if anything happens
     * ================================================================ */
    lcd_fill(0, 0, 320, 240, BG);
    lcd_hex8(5, 2, 0x03, ORNG, BG, 2);

    y = 20;

    /* Try writing to EP0 register with decompiled pattern */
    uint32_t ep0_before = *usb_reg(0);
    *usb_reg(0) = 0x80b0;  /* From decompiled: ^ 0x80b0 */
    delay_ms(10);
    uint32_t ep0_after = *usb_reg(0);

    lcd_hex32(5, y, ep0_before, GRAY, BG, 2);
    lcd_hex32(120, y, 0x80b0, GREEN, BG, 2);
    lcd_hex32(235, y, ep0_after, (ep0_after != ep0_before) ? YELLO : GRAY, BG, 2);
    y += 18;

    /* Write to packet buffer and read back */
    *(volatile uint16_t *)(USB_PBUF + 0) = 0xDEAD;
    *(volatile uint16_t *)(USB_PBUF + 4) = 0xBEEF;
    delay_ms(10);
    uint16_t pb0 = *(volatile uint16_t *)(USB_PBUF + 0);
    uint16_t pb1 = *(volatile uint16_t *)(USB_PBUF + 4);

    lcd_hex8(5, y, 0xDE, (pb0 == 0xDEAD) ? GREEN : RED, BG, 2);
    lcd_hex8(30, y, 0xAD, (pb0 == 0xDEAD) ? GREEN : RED, BG, 2);
    lcd_hex8(70, y, (pb0 >> 8) & 0xFF, CYAN, BG, 2);
    lcd_hex8(95, y, pb0 & 0xFF, CYAN, BG, 2);
    lcd_hex8(150, y, 0xBE, (pb1 == 0xBEEF) ? GREEN : RED, BG, 2);
    lcd_hex8(175, y, 0xEF, (pb1 == 0xBEEF) ? GREEN : RED, BG, 2);
    lcd_hex8(215, y, (pb1 >> 8) & 0xFF, CYAN, BG, 2);
    lcd_hex8(240, y, pb1 & 0xFF, CYAN, BG, 2);
    y += 22;

    /* Now check: does the FPGA heartbeat on PA3 change after USB writes? */
    /* Re-init USART2 to monitor */
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 1);
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = (1 << 2) | (1 << 13);  /* RX only */

    delay_ms(200);

    /* Collect a few frames and show */
    uint8_t rx_buf[24];
    uint16_t rx_n = 0;
    for (int t = 0; t < 20; t++) {
        wdt_counter_reload();
        while ((USART2->sts & (1 << 5)) && rx_n < 24) {
            rx_buf[rx_n++] = USART2->dt & 0xFF;
        }
        if (USART2->sts & (1 << 3)) { (void)USART2->sts; (void)USART2->dt; }
        delay_ms(100);
    }

    /* Show RX data */
    for (int i = 0; i < 12 && i < rx_n; i++) {
        lcd_hex8(5 + i * 26, y, rx_buf[i], CYAN, BG, 2);
    }
    y += 18;
    for (int i = 12; i < 24 && i < rx_n; i++) {
        lcd_hex8(5 + (i-12) * 26, y, rx_buf[i], CYAN, BG, 2);
    }
    y += 22;

    /* Show total RX count */
    lcd_hex8(5, y, (rx_n >> 8) & 0xFF, WHITE, BG, 2);
    lcd_hex8(30, y, rx_n & 0xFF, WHITE, BG, 2);

    /* ================================================================
     * PHASE 4: EXMC bus probe — SAFE version
     * Only use known-good addresses: 0x6001FFFE and 0x60020000
     * The FPGA shares the EXMC NE1 bus with the LCD.
     * Try writing non-LCD commands and reading data responses.
     * ================================================================ */
    lcd_fill(0, 0, 320, 240, BG);
    lcd_hex8(5, 2, 0x04, ORNG, BG, 2);

    y = 20;

    /* First: read 0x60020000 multiple times to see if it returns
     * changing data (FIFO from FPGA) or static data (LCD echo) */
    uint16_t reads[8];
    for (int i = 0; i < 8; i++) {
        reads[i] = *(volatile uint16_t *)0x60020000;
    }
    for (int i = 0; i < 8; i++) {
        uint16_t c = (i > 0 && reads[i] != reads[0]) ? GREEN : CYAN;
        lcd_hex8(5 + i * 38, y, (reads[i] >> 8) & 0xFF, c, BG, 2);
        lcd_hex8(25 + i * 38, y, reads[i] & 0xFF, c, BG, 1);
    }
    y += 20;

    /* Send ST7789V "read ID" commands — these are safe LCD reads.
     * But if FPGA is on the same bus, it might respond too. */

    /* LCD cmd 0x04 = Read Display ID (returns 3 bytes) */
    *(volatile uint16_t *)0x6001FFFE = 0x04;
    delay_ms(1);
    uint16_t id1 = *(volatile uint16_t *)0x60020000;
    uint16_t id2 = *(volatile uint16_t *)0x60020000;
    uint16_t id3 = *(volatile uint16_t *)0x60020000;
    lcd_hex8(5, y, 0x04, MAGNT, BG, 2);
    lcd_hex8(35, y, (id1 >> 8) & 0xFF, YELLO, BG, 2);
    lcd_hex8(60, y, id1 & 0xFF, YELLO, BG, 2);
    lcd_hex8(90, y, (id2 >> 8) & 0xFF, YELLO, BG, 2);
    lcd_hex8(115, y, id2 & 0xFF, YELLO, BG, 2);
    lcd_hex8(145, y, (id3 >> 8) & 0xFF, YELLO, BG, 2);
    lcd_hex8(170, y, id3 & 0xFF, YELLO, BG, 2);
    y += 20;

    /* LCD cmd 0x09 = Read Display Status (returns 4 bytes) */
    *(volatile uint16_t *)0x6001FFFE = 0x09;
    delay_ms(1);
    for (int i = 0; i < 4; i++) {
        uint16_t val = *(volatile uint16_t *)0x60020000;
        lcd_hex8(5 + i * 52, y, (val >> 8) & 0xFF, YELLO, BG, 2);
        lcd_hex8(30 + i * 52, y, val & 0xFF, YELLO, BG, 2);
    }
    y += 20;

    /* LCD cmd 0xDA = Read ID1, 0xDB = Read ID2, 0xDC = Read ID3 */
    uint8_t id_cmds[] = {0xDA, 0xDB, 0xDC};
    for (int c = 0; c < 3; c++) {
        *(volatile uint16_t *)0x6001FFFE = id_cmds[c];
        delay_ms(1);
        uint16_t val = *(volatile uint16_t *)0x60020000;
        lcd_hex8(5 + c * 80, y, id_cmds[c], MAGNT, BG, 2);
        lcd_hex8(35 + c * 80, y, (val >> 8) & 0xFF, YELLO, BG, 2);
        lcd_hex8(60 + c * 80, y, val & 0xFF, YELLO, BG, 2);
    }
    y += 24;

    /* Now the key test: write decompiled FPGA-style values to 0x6001FFFE
     * and read multiple words from 0x60020000.
     * The decompiled code writes _DAT_20008348 to 0x6001FFFE before DMA.
     * Common values from the scope FSM: 0x2C, 0x2E, 0x22, 0x36
     * These are actually ST7789V commands too:
     *   0x2A = Column Address Set
     *   0x2B = Row Address Set
     *   0x2C = Memory Write
     *   0x2E = Memory Read  <-- THIS could read FPGA data!
     */

    /* Try 0x2E = Memory Read — this reads pixel data from display RAM
     * If FPGA data is in display RAM, this is how you get it! */
    *(volatile uint16_t *)0x6001FFFE = 0x2E;  /* Memory Read */
    delay_ms(1);
    lcd_hex8(5, y, 0x2E, GREEN, BG, 2);
    for (int i = 0; i < 8; i++) {
        uint16_t val = *(volatile uint16_t *)0x60020000;
        uint16_t c = (val != 0x0000 && val != 0xFFFF) ? YELLO : GRAY;
        lcd_hex8(35 + i * 34, y, (val >> 8) & 0xFF, c, BG, 2);
    }
    y += 20;

    /* Read 8 more to see if data stream continues */
    for (int i = 0; i < 8; i++) {
        uint16_t val = *(volatile uint16_t *)0x60020000;
        uint16_t c = (val != 0x0000 && val != 0xFFFF) ? YELLO : GRAY;
        lcd_hex8(35 + i * 34, y, (val >> 8) & 0xFF, c, BG, 2);
    }
    y += 20;

    /* Restore LCD to normal write mode */
    *(volatile uint16_t *)0x6001FFFE = 0x2C;  /* Memory Write */

    /* Stay forever */
    while (1) {
        wdt_counter_reload();
        while (USART2->sts & (1 << 5)) { (void)USART2->dt; }
        if (USART2->sts & (1 << 3)) { (void)USART2->sts; (void)USART2->dt; }
        delay_ms(200);
    }
}
