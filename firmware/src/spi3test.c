/*
 * SPI3 FPGA Interface Test for FNIRSI 2C53T
 *
 * Tests the hypothesis that the FPGA communicates via SPI3 (0x40003C00)
 * on PB3 (SCK), PB4 (MISO), PB5 (MOSI) with PB6 as GPIO chip select.
 *
 * PB3/PB4 are JTAG pins by default — we must disable JTAG via IOMUX remap.
 *
 * Phase 0: Disable JTAG, configure SPI3, show register state
 * Phase 1: Assert CS (PB6), try SPI3 read (send 0xFF, read back)
 * Phase 2: Try multiple CS pins (PB6, PA15, PA4) in case CS is elsewhere
 * Phase 3: Try sending FPGA command bytes and reading response
 * Phase 4: Monitor — continuous read with display
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

    gpio_pin_remap_config(SPI3_GMUX_0010, TRUE);
    lcd_hex8(170, 2, 0x02, CYAN, BG, 2); /* marker: SPI3 remapped */

    /* Show IOMUX state */
    lcd_hex32(5, y, *(volatile uint32_t *)(0x40010004), CYAN, BG, 2);
    y += 18;

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
     * Master mode, 8-bit, software CS, various clock configs to try */

    /* First try: CPOL=0, CPHA=0, clock = PCLK1/16
     * At 240MHz, APB1 is typically 120MHz, so SPI clock = 7.5MHz */
    SPI3_CTL0 = 0;  /* Reset */
    SPI3_CTL0 = (1 << 2)   /* MSTEN: Master mode */
              | (0 << 1)   /* CLKPHA: CPHA=0 */
              | (0 << 0)   /* CLKPOL: CPOL=0 */
              | (3 << 3)   /* MDIV: /16 (bits[5:3]=011) */
              | (1 << 9)   /* SWCSEN: Software CS management */
              | (1 << 8);  /* SWCSIL: Internal CS high */
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

    /* Send 10-byte command frames */
    {
        uint8_t cmd0[] = {0xAA, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t cmd1[] = {0xAA, 0x55, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        uint8_t cmd2[] = {0xAA, 0x55, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C};

        for (int f = 0; f < 3; f++) {
            uint8_t *cmd = (f == 0) ? cmd0 : (f == 1) ? cmd1 : cmd2;
            for (int i = 0; i < 10; i++) {
                while (!(USART2->sts & (1 << 7))); /* TDBE */
                USART2->dt = cmd[i];
            }
            /* Wait for TX complete */
            while (!(USART2->sts & (1 << 6))); /* TDC */
            delay_ms(50);
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
     * PHASE 3: SPI3 read after USART2 commands
     * ============================================================ */
    lcd_hex8(5, y, 0x03, ORNG, BG, 2);
    y += 18;

    SPI3_CTL0 &= ~(1 << 6);
    SPI3_CTL0 &= ~0x03;
    SPI3_CTL0 |= (1 << 6);

    cs_assert(GPIOB_BASE, 6);
    delay_ms(1);
    uint8_t rx_buf[8];
    for (int i = 0; i < 8; i++)
        rx_buf[i] = spi3_xfer(0xFF);
    cs_deassert(GPIOB_BASE, 6);

    for (int i = 0; i < 8; i++) {
        uint16_t color = (rx_buf[i] != 0xFF && rx_buf[i] != 0x00) ? GREEN : GRAY;
        lcd_hex8(5 + i * 30, y, rx_buf[i], color, BG, 2);
    }
    y += 18;

    /* ============================================================
     * PHASE 4: Monitor — SPI3 + USART2 RX
     * ============================================================ */
    uint32_t frame = 0;
    while (1) {
        cs_assert(GPIOB_BASE, 6);
        uint8_t spi_mon[8];
        for (int i = 0; i < 8; i++)
            spi_mon[i] = spi3_xfer(0xFF);
        cs_deassert(GPIOB_BASE, 6);

        uint8_t uart_mon[8] = {0};
        for (int i = 0; i < 8; i++) {
            if (USART2->sts & (1 << 5))
                uart_mon[i] = (uint8_t)(USART2->dt & 0xFF);
        }

        for (int i = 0; i < 8; i++) {
            uint16_t color = (spi_mon[i] != 0xFF && spi_mon[i] != 0x00) ? GREEN : GRAY;
            lcd_hex8(5 + i * 30, 204, spi_mon[i], color, BG, 2);
        }
        for (int i = 0; i < 8; i++) {
            uint16_t color = (uart_mon[i] != 0x00) ? YELLO : GRAY;
            lcd_hex8(5 + i * 30, 222, uart_mon[i], color, BG, 2);
        }

        lcd_hex32(200, 2, frame, GRAY, BG, 2);
        frame++;
        delay_ms(50);
    }
}
