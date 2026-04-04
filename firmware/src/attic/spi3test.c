/*
 * SPI3 FPGA Interface Test V2 for FNIRSI 2C53T
 *
 * Based on disassembly of stock firmware FPGA task (FUN_08036934, ~11KB):
 *
 * SPI3 CONFIG (from FUN_08036848 init + 0x080265DE):
 *   - Mode 3: CPOL=1 (clock idle HIGH), CPHA=1 (trailing edge sample)
 *   - Master, 8-bit, MSB first, software CS (SSM=1, SSI=1)
 *   - Stock uses APB1/2 = 60MHz clock (we use /16 = 7.5MHz for safety)
 *   - PB6 = software CS (GPIO), PB3=SCK, PB4=MISO, PB5=MOSI
 *
 * USART COMMAND FORMAT (from 0x08037420):
 *   - 3 bytes: [high_byte, low_byte, checksum(high+low)]
 *   - Single-byte commands: high=0, low=cmd, cksum=cmd
 *   - Known commands: 0x1B, 0x1C, 0x1E, 0x0A, 0x07, 0x22, 0x23
 *
 * SPI3 PROTOCOL (from 0x080374E8):
 *   - CS assert (PB6 LOW)
 *   - Send command byte, get response
 *   - Table branch on response (9 cases)
 *   - Bulk read: 0x400 (1024) bytes to meter_state_base + 0x5B0
 *   - CS deassert (PB6 HIGH)
 *
 * V1 result: SPI3 operational, all 0xFF reads (wrong SPI mode!)
 * V2 change: Mode 3 (CPOL=1, CPHA=1) + correct USART command format
 *
 * Build: make -f Makefile.hwtest TEST=spi3test
 * Flash: make -f Makefile.hwtest TEST=spi3test flash
 */
#include "at32f403a_407.h"
/* No watchdog for this test */
#include <string.h>

extern void system_clock_config(void);

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

/* SPI3 registers */
#define SPI3_BASE   0x40003C00
#define SPI3_CTL0   (*(volatile uint32_t *)(SPI3_BASE + 0x00))
#define SPI3_CTL1   (*(volatile uint32_t *)(SPI3_BASE + 0x04))
#define SPI3_STS    (*(volatile uint32_t *)(SPI3_BASE + 0x08))
#define SPI3_DT     (*(volatile uint32_t *)(SPI3_BASE + 0x0C))

/* SPI2 registers (for comparison — this is the SPI flash) */
#define SPI2_BASE   0x40003800
#define SPI2_CTL0   (*(volatile uint32_t *)(SPI2_BASE + 0x00))
#define SPI2_STS    (*(volatile uint32_t *)(SPI2_BASE + 0x08))
#define SPI2_DT     (*(volatile uint32_t *)(SPI2_BASE + 0x0C))

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

static void lcd_hex32(uint16_t x, uint16_t y, uint32_t val, uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_hex8(x, y, (val >> 24) & 0xFF, fg, bg, s);
    lcd_hex8(x + 12*s, y, (val >> 16) & 0xFF, fg, bg, s);
    lcd_hex8(x + 24*s, y, (val >> 8) & 0xFF, fg, bg, s);
    lcd_hex8(x + 36*s, y, val & 0xFF, fg, bg, s);
}

/* Simple text string (digits + hex + some letters) */
static const uint8_t font_S[] = {0x64,0x92,0x92,0x92,0x4C};
static const uint8_t font_P[] = {0xFE,0x90,0x90,0x90,0x60};
static const uint8_t font_I[] = {0x00,0x82,0xFE,0x82,0x00};
static const uint8_t font_3[] = {0x44,0x82,0x92,0x92,0x6C};
static const uint8_t font_T[] = {0x80,0x80,0xFE,0x80,0x80};
static const uint8_t font_R[] = {0xFE,0x90,0x98,0x94,0x62};
static const uint8_t font_X[] = {0xC6,0x28,0x10,0x28,0xC6};
static const uint8_t font_O[] = {0x7C,0x82,0x82,0x82,0x7C};
static const uint8_t font_K[] = {0xFE,0x10,0x28,0x44,0x82};

#define BG    0x0008
#define WHITE 0xFFFF
#define GREEN 0x07E0
#define CYAN  0x07FF
#define YELLO 0xFFE0
#define RED   0xF800
#define GRAY  0x4208
#define ORNG  0xFBE0
#define MAGNT 0xF81F

/* SPI3 transceive: send a byte, return received byte */
static uint8_t spi3_xfer(uint8_t tx) {
    /* Wait for TX empty */
    uint32_t timeout = 100000;
    while (!(SPI3_STS & 0x02) && --timeout);
    if (!timeout) return 0xEE; /* timeout marker */

    SPI3_DT = tx;

    /* Wait for RX not empty */
    timeout = 100000;
    while (!(SPI3_STS & 0x01) && --timeout);
    if (!timeout) return 0xEF; /* timeout marker */

    return (uint8_t)(SPI3_DT & 0xFF);
}

/* Assert CS low on a given pin */
static void cs_assert(uint32_t gpio_base, uint8_t pin) {
    *(volatile uint32_t *)(gpio_base + 0x14) = (1 << pin); /* BCR = clear */
}

/* Deassert CS high on a given pin */
static void cs_deassert(uint32_t gpio_base, uint8_t pin) {
    *(volatile uint32_t *)(gpio_base + 0x10) = (1 << pin); /* BOP = set */
}

int main(void) {
    /* ============================================================
     * BOOT: Power hold + clock + peripherals
     * ============================================================ */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);  /* PC9 push-pull output */
    GPIOC->scr = (1 << 9);              /* Power hold HIGH */

    system_clock_config();               /* 240 MHz */

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1 << 8);

    /* LCD GPIO + EXMC init */
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

    /* No watchdog — keep things simple for debugging */

    lcd_fill(0, 0, 320, 240, BG);

    /* Title: "SPI3" */
    lcd_draw_glyph(5, 2, font_S, ORNG, BG, 2);
    lcd_draw_glyph(17, 2, font_P, ORNG, BG, 2);
    lcd_draw_glyph(29, 2, font_I, ORNG, BG, 2);
    lcd_draw_glyph(41, 2, font_3, ORNG, BG, 2);

    /* Show "BOOT OK" marker so we know LCD init passed */
    lcd_hex8(100, 2, 0xBB, GREEN, BG, 2);

    uint16_t y = 20;


    /* ============================================================
     * PHASE 0: Disable JTAG, enable SPI3 clock, configure pins
     * ============================================================ */

    /* Step 1: Disable JTAG to free PB3/PB4 for SPI3
     * SWJTAG_GMUX_010 = JTAG-DP disabled, SW-DP enabled (frees PA15, PB3, PB4)
     * SPI3_GMUX_0010 = SPI3 on PB3(SCK), PB4(MISO), PB5(MOSI) */
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    lcd_hex8(140, 2, 0x01, CYAN, BG, 2); /* marker: JTAG disabled */

    /* RE-ENABLE SPI3 remap. On AT32F403A, JTAG disable frees the pins
     * but SPI3 AF may need explicit mux config via extended IOMUX registers.
     * SPI3_GMUX_0010 on AT32 = SPI3 on PB3/PB4/PB5 (NOT PC10/PC11/PC12). */
    gpio_pin_remap_config(SPI3_GMUX_0010, TRUE);
    lcd_hex8(170, 2, 0x04, CYAN, BG, 2); /* marker: SPI3 remap enabled */

    /* Show IOMUX state */
    lcd_hex32(5, y, *(volatile uint32_t *)(0x40010004), CYAN, BG, 2);
    y += 18;

    /* CRITICAL FPGA control pins from comprehensive decompilation:
     * PC6 = FPGA SPI enable (set HIGH at 0x0802663C before SPI3 init)
     * PB11 = FPGA active mode signal (set HIGH in mode_switch_reset_handler at 0x08007344)
     * Both MUST be HIGH for FPGA to respond on SPI3 */
    gpio_pin_cfg(GPIOC_BASE, 6, 3, 0);  /* PC6: push-pull output */
    GPIOC->scr = (1 << 6);              /* PC6 HIGH — FPGA SPI enable */

    gpio_pin_cfg(GPIOB_BASE, 11, 3, 0); /* PB11: push-pull output */
    GPIOB->scr = (1 << 11);             /* PB11 HIGH — FPGA active mode */

    /* Step 2: Enable SPI3 peripheral clock */
    crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);

    /* Step 3: Configure SPI3 GPIO pins */
    /* PB3 = SPI3_SCK  → AF push-pull output (mode=3, cnf=2) */
    /* PB4 = SPI3_MISO → Input floating (mode=0, cnf=1) */
    /* PB5 = SPI3_MOSI → AF push-pull output (mode=3, cnf=2) */
    gpio_pin_cfg(GPIOB_BASE, 3, 3, 2);  /* PB3 SCK: AF push-pull */
    gpio_pin_cfg(GPIOB_BASE, 4, 0, 1);  /* PB4 MISO: input floating */
    gpio_pin_cfg(GPIOB_BASE, 5, 3, 2);  /* PB5 MOSI: AF push-pull */

    /* PB6 = CS (GPIO push-pull output, start HIGH) */
    gpio_pin_cfg(GPIOB_BASE, 6, 3, 0);  /* PB6: push-pull output */
    cs_deassert(GPIOB_BASE, 6);          /* CS high (deasserted) */

    /* Step 4: Configure SPI3 peripheral
     * Extracted from stock firmware FUN_08036848 + init at 0x080265DE:
     *   Mode 3 (CPOL=1, CPHA=1), Master, 8-bit, MSB-first
     *   Software CS (SSM=1, SSI=1), Clock = APB1/2
     *
     * Stock firmware also sets CTL1 bits 0,1 (DMA RX/TX enable)
     * and then sets SPE (bit 6) to enable.
     *
     * Start with /16 divider for safety (can try /2 later) */
    SPI3_CTL0 = 0;  /* Reset */
    SPI3_CTL1 = 0;  /* Reset CTL1 */

    /* Disable I2S mode (stock firmware does this first) */
    *(volatile uint32_t *)(SPI3_BASE + 0x1C) &= ~(1 << 11);

    SPI3_CTL0 = (1 << 2)   /* MSTEN: Master mode */
              | (1 << 1)   /* CLKPOL: CPOL=1 (clock idle HIGH) — from firmware! */
              | (1 << 0)   /* CLKPHA: CPHA=1 (trailing edge) — from firmware! */
              | (0 << 3)   /* MDIV: /2 = 60MHz — match stock firmware exactly */
              | (1 << 9)   /* SWCSEN: Software CS management */
              | (1 << 8);  /* SWCSIL: Internal CS high */

    /* CTL1: enable DMA requests (stock firmware does this) */
    SPI3_CTL1 |= (1 << 0) | (1 << 1);  /* DMAREN + DMATEN */

    /* Enable SPI */
    SPI3_CTL0 |= (1 << 6); /* SPIEN: Enable SPI */

    /* Show SPI3 registers after config */
    lcd_hex32(5, y, SPI3_CTL0, GREEN, BG, 2); y += 18;
    lcd_hex32(5, y, SPI3_STS, GREEN, BG, 2); y += 18;


    /* ============================================================
     * PHASE 1: Init USART2 (PA2=TX, PA3=RX) and capture heartbeat
     * ============================================================ */
    lcd_hex8(5, y, 0x01, ORNG, BG, 2);

    /* Enable USART2 clock */
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    /* PA2 = USART2 TX: AF push-pull */
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);
    /* PA3 = USART2 RX: Input floating */
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 1);

    lcd_hex8(40, y, 0x11, CYAN, BG, 2); /* GPIO done */

    /* Configure USART2: 9600 baud, 8N1 using AT32 HAL struct */
    USART2->ctrl1 = 0;
    USART2->ctrl2 = 0;
    USART2->ctrl3 = 0;
    USART2->baudr = 12500;     /* 120MHz / 9600 = 12500 */
    USART2->ctrl1 = (1 << 13)  /* UEN: USART enable */
                  | (1 << 3)   /* TEN: TX enable */
                  | (1 << 2);  /* REN: RX enable */

    lcd_hex8(70, y, 0x22, CYAN, BG, 2); /* USART2 configured */
    y += 18;

    /* Wait for heartbeat bytes to arrive */
    delay_ms(200);

    /* Read whatever is in the RX buffer (heartbeat bytes) */
    uint8_t hb[12];
    int hb_count = 0;
    for (int i = 0; i < 12; i++) {
        uint32_t t = 50000;
        while (!(USART2->sts & (1 << 5)) && --t); /* RDBF: RX data ready */
        if (t) {
            hb[i] = (uint8_t)(USART2->dt & 0xFF);
            hb_count++;
        } else {
            hb[i] = 0x00;
        }
    }

    /* Show heartbeat bytes */
    for (int i = 0; i < 8; i++) {
        uint16_t color = (hb[i] != 0x00) ? YELLO : GRAY;
        lcd_hex8(5 + i * 30, y, hb[i], color, BG, 2);
    }
    y += 16;
    for (int i = 8; i < 12; i++) {
        uint16_t color = (hb[i] != 0x00) ? YELLO : GRAY;
        lcd_hex8(5 + (i-8) * 30, y, hb[i], color, BG, 2);
    }
    lcd_hex8(150, y, (uint8_t)hb_count, WHITE, BG, 2);
    y += 18;

    /* ============================================================
     * PHASE 2: Send FPGA init commands via USART2, then try SPI3
     * ============================================================ */
    lcd_hex8(5, y, 0x02, ORNG, BG, 2);

    /* Send FPGA commands via USART2 using correct 10-byte frame format.
     *
     * From TX ISR at 0x08027822:
     *   - Sends 10 bytes from buffer at 0x20000005
     *   - Stops when byte index reaches 10 (0x0A)
     *
     * Frame layout (10 bytes):
     *   [0-1] = sync header (likely 0xAA 0x55 based on RX echo check)
     *   [2]   = high byte of command (0x00 for single-byte cmds)
     *   [3]   = low byte of command
     *   [4-8] = zeros (parameters/padding)
     *   [9]   = checksum = (high + low) & 0xFF
     *
     * FPGA responds with 12-byte frame: 0x5A 0xA5 or 0xAA 0x55 header
     */
    {
        uint8_t all_cmds[] = {0x01, 0x02, 0x06, 0x07, 0x08, 0x1C, 0x1B, 0x1E};
        for (int c = 0; c < 8; c++) {
            uint8_t cmd = all_cmds[c];
            uint8_t frame[10] = {
                0x00, 0x00,     /* bytes 0-1: zero (BSS-initialized in stock FW) */
                0x00, cmd,      /* high=0, low=command */
                0x00, 0x00, 0x00, 0x00, 0x00,  /* padding */
                cmd             /* checksum = (0x00 + cmd) & 0xFF */
            };
            for (int i = 0; i < 10; i++) {
                while (!(USART2->sts & (1 << 7))); /* TDBE */
                USART2->dt = frame[i];
            }
            while (!(USART2->sts & (1 << 6))); /* TDC */
            delay_ms(100);  /* wait for FPGA to process + respond */

            /* Drain any USART RX response */
            for (int i = 0; i < 20; i++) {
                if (USART2->sts & (1 << 5))
                    (void)(USART2->dt);
            }
        }
    }

    lcd_hex8(40, y, 0xCC, GREEN, BG, 2);
    delay_ms(200);

    /* Read USART2 response */
    uint8_t resp[12];
    for (int i = 0; i < 12; i++) {
        uint32_t t = 50000;
        while (!(USART2->sts & (1 << 5)) && --t);
        resp[i] = t ? (uint8_t)(USART2->dt & 0xFF) : 0x00;
    }
    for (int i = 0; i < 8; i++) {
        uint16_t color = (resp[i] != 0x00) ? GREEN : GRAY;
        lcd_hex8(80 + i * 26, y, resp[i], color, BG, 2);
    }
    y += 18;

    /* ============================================================
     * PHASE 3: SPI3 init handshake + read
     * From stock firmware init at 0x0802676E:
     *   1. CS assert, send 0x00, read response, CS deassert
     *   2. CS assert, send 0x05, read response, CS deassert
     * Then bulk reads follow in the main FPGA task.
     * ============================================================ */
    lcd_hex8(5, y, 0x03, ORNG, BG, 2);

    /* Show GPIOB and GPIOC state */
    uint32_t gpioc_idr = *(volatile uint32_t *)(0x40011008);
    lcd_hex32(40, y, gpioc_idr, MAGNT, BG, 2);
    y += 18;

    /* SysTick-like delay before SPI3 handshake (stock FW does this) */
    delay_ms(100);

    /* ============================================================
     * SPI3 FPGA HANDSHAKE — exact sequence from stock firmware
     * Traced from init at 0x0802676E-0x08026872:
     *
     * Step 1: CS HIGH, send 0x00 (dummy flush)
     * Step 2: CS LOW,  send 0x05 (FPGA command)
     * Step 3:          send 0x00 (parameter, still CS low)
     * Step 4: CS HIGH (deassert)
     * Step 5: CS HIGH, send 0x00, 0x00 (dummy flushes)
     * Step 6: delay
     * ============================================================ */

    /* Step 1: Dummy flush with CS high */
    cs_deassert(GPIOB_BASE, 6);  /* ensure CS HIGH */
    uint8_t h0 = spi3_xfer(0x00);
    lcd_hex8(5, y, h0, (h0 != 0xFF) ? GREEN : GRAY, BG, 2);

    /* Step 2-3: CS assert, send 0x05 + 0x00 */
    cs_assert(GPIOB_BASE, 6);
    uint8_t h1 = spi3_xfer(0x05);  /* FPGA command */
    uint8_t h2 = spi3_xfer(0x00);  /* parameter */
    cs_deassert(GPIOB_BASE, 6);
    lcd_hex8(20, y, h1, (h1 != 0xFF) ? GREEN : GRAY, BG, 2);
    lcd_hex8(35, y, h2, (h2 != 0xFF) ? GREEN : GRAY, BG, 2);

    /* Step 5: Dummy flushes */
    uint8_t h3 = spi3_xfer(0x00);
    uint8_t h4 = spi3_xfer(0x00);
    lcd_hex8(50, y, h3, (h3 != 0xFF) ? GREEN : GRAY, BG, 2);
    lcd_hex8(65, y, h4, (h4 != 0xFF) ? GREEN : GRAY, BG, 2);

    /* Step 6: delay */
    delay_ms(100);

    /* Second handshake: CS assert, send more commands */
    cs_assert(GPIOB_BASE, 6);
    uint8_t h5 = spi3_xfer(0x01);  /* try command 1 */
    uint8_t h6 = spi3_xfer(0x00);
    cs_deassert(GPIOB_BASE, 6);
    lcd_hex8(80, y, h5, (h5 != 0xFF) ? GREEN : GRAY, BG, 2);
    lcd_hex8(95, y, h6, (h6 != 0xFF) ? GREEN : GRAY, BG, 2);
    y += 18;

    /* ============================================================
     * PHASE 3B: GPIO DIAGNOSTIC — Read PB4 as raw GPIO input
     * Disable SPI3, configure PB4 as plain input, read GPIOB_IDR
     * If bit 4 is always 1 → pin floating (no FPGA connection)
     * If bit 4 changes → FPGA is driving MISO
     * ============================================================ */
    /* ============================================================
     * PHASE 3B: USART FRAME CAPTURE
     * Capture complete 12-byte frames from FPGA (0x5A 0xA5 header)
     * Display raw frame data to understand FPGA protocol
     * Then try SPI3 after successful USART exchange
     * ============================================================ */

    /* Continuously capture and display USART frames */
    uint8_t rx_frame[16];
    uint8_t rx_idx = 0;
    uint32_t frame_count = 0;
    uint16_t fy = 168;
    uint32_t send_timer = 0;

    lcd_fill(0, 168, 320, 72, BG);  /* clear bottom area */

    while (1) {
        /* Periodically send USART commands to stimulate FPGA responses */
        send_timer++;
        if (send_timer % 50000 == 0) {
            uint8_t cmd = 0x07;  /* scope mode command */
            uint8_t frame10[10] = {0,0, 0,cmd, 0,0,0,0,0, cmd};
            for (int i = 0; i < 10; i++) {
                while (!(USART2->sts & (1 << 7)));
                USART2->dt = frame10[i];
            }
        }

        /* Check for USART RX data */
        if (USART2->sts & (1 << 5)) {  /* RDBF */
            uint8_t byte = (uint8_t)(USART2->dt & 0xFF);

            if (rx_idx == 0) {
                /* Looking for frame start: 0x5A or 0xAA */
                if (byte == 0x5A || byte == 0xAA) {
                    rx_frame[0] = byte;
                    rx_idx = 1;
                }
            } else if (rx_idx == 1) {
                /* Validate second sync byte */
                if ((rx_frame[0] == 0x5A && byte == 0xA5) ||
                    (rx_frame[0] == 0xAA && byte == 0x55)) {
                    rx_frame[1] = byte;
                    rx_idx = 2;
                } else {
                    rx_idx = 0;  /* bad sync, restart */
                }
            } else {
                rx_frame[rx_idx++] = byte;
                uint8_t expected_len = (rx_frame[0] == 0x5A) ? 12 : 10;

                if (rx_idx >= expected_len) {
                    /* Complete frame! Display it */
                    uint16_t color = (rx_frame[0] == 0x5A) ? CYAN : YELLO;
                    fy = 168 + (frame_count % 3) * 18;
                    lcd_fill(0, fy, 320, 16, BG);

                    /* Show frame type + all bytes */
                    for (int i = 0; i < expected_len && i < 12; i++) {
                        lcd_hex8(5 + i * 24, fy, rx_frame[i], color, BG, 2);
                    }
                    frame_count++;
                    lcd_hex32(220, 2, frame_count, GRAY, BG, 2);

                    /* After receiving 5 frames, try SPI3 */
                    if (frame_count == 5) {
                        lcd_fill(0, 222, 320, 16, BG);
                        cs_assert(GPIOB_BASE, 6);
                        uint8_t s0 = spi3_xfer(0x05);
                        uint8_t s1 = spi3_xfer(0x00);
                        cs_deassert(GPIOB_BASE, 6);
                        uint16_t sc = (s0 != 0xFF || s1 != 0xFF) ? GREEN : RED;
                        lcd_hex8(5, 222, s0, sc, BG, 2);
                        lcd_hex8(25, 222, s1, sc, BG, 2);

                        /* Also try reading 8 bytes */
                        cs_assert(GPIOB_BASE, 6);
                        for (int i = 0; i < 8; i++) {
                            uint8_t v = spi3_xfer(0xFF);
                            uint16_t vc = (v != 0xFF) ? GREEN : GRAY;
                            lcd_hex8(55 + i * 24, 222, v, vc, BG, 2);
                        }
                        cs_deassert(GPIOB_BASE, 6);
                    }

                    rx_idx = 0;  /* reset for next frame */
                }
            }
        }
    }

    /* ============================================================
     * PHASE 4: Monitor — SPI3 command/response + USART2 RX
     *
     * From FPGA task disassembly:
     *   - First SPI byte sent is a command ID
     *   - Response byte indicates what to do next
     *   - After command exchange, bulk reads of 0x400 (1024) bytes
     *   - Data stored at meter_state_base + 0x5B0
     * ============================================================ */
    uint32_t frame = 0;

    /* Try different SPI command bytes each frame */
    uint8_t spi_cmds[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    while (1) {
        uint8_t cmd = spi_cmds[frame % sizeof(spi_cmds)];

        /* SPI3 transaction: send command, read response */
        cs_assert(GPIOB_BASE, 6);
        uint8_t spi_resp = spi3_xfer(cmd);  /* Command byte */
        uint8_t spi_data[6];
        for (int i = 0; i < 6; i++)
            spi_data[i] = spi3_xfer(0xFF);  /* Read data bytes */
        cs_deassert(GPIOB_BASE, 6);

        /* Read any USART2 data */
        uint8_t uart_mon[8] = {0};
        for (int i = 0; i < 8; i++) {
            if (USART2->sts & (1 << 5))
                uart_mon[i] = (uint8_t)(USART2->dt & 0xFF);
        }

        /* Display: cmd sent | response | data bytes */
        lcd_hex8(5, 186, cmd, WHITE, BG, 2);
        lcd_hex8(35, 186, spi_resp, (spi_resp != 0xFF) ? GREEN : GRAY, BG, 2);
        for (int i = 0; i < 6; i++) {
            uint16_t color = (spi_data[i] != 0xFF && spi_data[i] != 0x00) ? GREEN : GRAY;
            lcd_hex8(75 + i * 30, 186, spi_data[i], color, BG, 2);
        }

        /* Show SPI3 status register */
        lcd_hex32(5, 204, SPI3_STS, CYAN, BG, 2);

        /* USART2 monitor */
        for (int i = 0; i < 8; i++) {
            uint16_t color = (uart_mon[i] != 0x00) ? YELLO : GRAY;
            lcd_hex8(5 + i * 30, 222, uart_mon[i], color, BG, 2);
        }

        lcd_hex32(200, 2, frame, GRAY, BG, 2);
        frame++;
        delay_ms(100);
    }
}
