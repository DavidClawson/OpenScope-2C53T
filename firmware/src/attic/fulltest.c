/*
 * fulltest.c — Comprehensive FPGA + Button Integration Test
 * FNIRSI 2C53T — AT32F403A @ 240MHz
 *
 * This test incorporates ALL findings from the stock firmware decompilation:
 *
 * BUTTONS (from input_and_housekeeping analysis):
 *   - All 15 buttons are on MCU GPIO (GPIOB/GPIOC)
 *   - 3-group multiplexed scanning with 70-tick debounce
 *   - Button map table at 0x08046528 in stock firmware
 *
 * FPGA SPI3 (from spi3_acquisition_task analysis):
 *   - Mode 3 (CPOL=1, CPHA=1), /2 = 60MHz, software CS on PB6
 *   - MUST set PB11 HIGH (active mode) and PC6 HIGH (SPI enable)
 *   - MUST send USART boot commands (0x01, 0x02, 0x06, 0x07, 0x08) first
 *   - MUST use SysTick delays between init phases
 *   - Acquisition triggered by queue, not polling
 *
 * USART2 (from usart_tx_frame_builder analysis):
 *   - 9600 baud on PA2(TX)/PA3(RX)
 *   - 10-byte TX frames: [0x00 0x00 cmd_hi cmd_lo 0x00*5 checksum]
 *   - 12-byte RX frames: [0x5A 0xA5 data*10] or [0xAA 0x55 echo*8]
 *
 * IOMUX (from remaining_unknowns extraction):
 *   - AFIO remap: (reg & ~0xF000) | 0x2000 — JTAG-DP off, SW-DP kept
 *
 * ADC CALIBRATION (from FPGA_TASK_ANALYSIS):
 *   - Interleaved CH1/CH2 unsigned 8-bit
 *   - Normal mode: 1024 bytes (512 pairs)
 *   - ADC offset: -28.0
 *
 * BATTERY (from remaining_unknowns extraction):
 *   - ADC1 Channel 9 on PB1
 *
 * Build: make -f Makefile.hwtest TEST=fulltest
 * Flash: make -f Makefile.hwtest TEST=fulltest flash
 */
#include "at32f403a_407.h"
#include <string.h>

extern void system_clock_config(void);

/* ═══════════════════════════════════════════════════════════════════
 * Hardware Registers
 * ═══════════════════════════════════════════════════════════════════ */

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

/* SPI3 (redefine with our names — AT32 HAL uses different naming) */
#undef SPI3_BASE
#define SPI3_BASE   0x40003C00
#define SPI3_CTL0   (*(volatile uint32_t *)(SPI3_BASE + 0x00))
#define SPI3_CTL1   (*(volatile uint32_t *)(SPI3_BASE + 0x04))
#define SPI3_STS    (*(volatile uint32_t *)(SPI3_BASE + 0x08))
#define SPI3_DT     (*(volatile uint32_t *)(SPI3_BASE + 0x0C))
#define SPI3_I2SCTL (*(volatile uint32_t *)(SPI3_BASE + 0x1C))

/* USART2 */
#undef USART2_BASE
#define USART2_BASE 0x40004400
#define USART2_STS  (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DT   (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BAUDR (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CTRL1 (*(volatile uint32_t *)(USART2_BASE + 0x0C))
#define USART2_CTRL2 (*(volatile uint32_t *)(USART2_BASE + 0x10))
#define USART2_CTRL3 (*(volatile uint32_t *)(USART2_BASE + 0x14))

/* GPIO bases */
#define GPIOA_BASE_ADDR 0x40010800
#define GPIOB_BASE_ADDR 0x40010C00
#define GPIOC_BASE_ADDR 0x40011000
#define GPIOD_BASE_ADDR 0x40011400
#define GPIOE_BASE_ADDR 0x40011800

/* GPIO register offsets */
#define GPIO_CRL   0x00
#define GPIO_CRH   0x04
#define GPIO_IDR   0x08
#define GPIO_ODR   0x0C
#define GPIO_BOP   0x10  /* set bits */
#define GPIO_BCR   0x14  /* clear bits */

/* AFIO */
#define AFIO_BASE   0x40010000
#define AFIO_MAPR   (*(volatile uint32_t *)(AFIO_BASE + 0x04))

/* SysTick */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)

/* Colors */
#define BG     0x0008
#define WHITE  0xFFFF
#define GREEN  0x07E0
#define CYAN   0x07FF
#define YELLO  0xFFE0
#define RED    0xF800
#define GRAY   0x4208
#define ORNG   0xFBE0
#define MAGNT  0xF81F
#define DKGRN  0x03E0

/* ═══════════════════════════════════════════════════════════════════
 * LCD / Font Primitives (same as other test firmwares)
 * ═══════════════════════════════════════════════════════════════════ */

static void delay_ms(uint32_t ms) {
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;
        while (count--) __asm volatile("nop");
    }
}

static void systick_delay_us(uint32_t us) {
    SYST_RVR = (system_core_clock / 1000000) * us;
    SYST_CVR = 0;
    SYST_CSR = 0x05; /* enable, processor clock, no interrupt */
    while (!(SYST_CSR & (1 << 16))); /* wait for COUNTFLAG */
    SYST_CSR = 0;
}

static void lcd_bus_delay(void) {
    volatile uint32_t i = 50;
    while (i--) __asm volatile("nop");
}

static void lcd_cmd_wr(uint8_t cmd) { LCD_CMD = cmd; lcd_bus_delay(); }
static void lcd_dat(uint8_t data)   { LCD_DATA = data; lcd_bus_delay(); }
static void lcd_dat16(uint16_t data){ LCD_DATA = data; }

static void gpio_pin_cfg(uint32_t base, uint8_t pin, uint8_t mode, uint8_t cnf) {
    volatile uint32_t *reg = (pin < 8) ?
        (volatile uint32_t *)(base + GPIO_CRL) :
        (volatile uint32_t *)(base + GPIO_CRH);
    uint8_t pos = (pin < 8) ? pin : (pin - 8);
    uint32_t val = *reg;
    val &= ~(0xFU << (pos * 4));
    val |= (((mode) | ((cnf) << 2)) << (pos * 4));
    *reg = val;
}

static void gpio_set(uint32_t base, uint8_t pin) {
    *(volatile uint32_t *)(base + GPIO_BOP) = (1 << pin);
}
static void gpio_clr(uint32_t base, uint8_t pin) {
    *(volatile uint32_t *)(base + GPIO_BCR) = (1 << pin);
}
static uint16_t gpio_read(uint32_t base) {
    return (uint16_t)(*(volatile uint32_t *)(base + GPIO_IDR));
}

/* 5x7 bitmap font (hex digits) */
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

static void lcd_hex8(uint16_t x, uint16_t y, uint8_t val,
                     uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_draw_glyph(x, y, font5x7[(val >> 4) & 0xF], fg, bg, s);
    lcd_draw_glyph(x + 6*s, y, font5x7[val & 0xF], fg, bg, s);
}

static void lcd_hex16(uint16_t x, uint16_t y, uint16_t val,
                      uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_hex8(x, y, (val >> 8) & 0xFF, fg, bg, s);
    lcd_hex8(x + 12*s, y, val & 0xFF, fg, bg, s);
}

/* Tiny letter glyphs for labels */
static const uint8_t font_B[] = {0xFE,0x92,0x92,0x92,0x6C};
static const uint8_t font_T[] = {0x80,0x80,0xFE,0x80,0x80};
static const uint8_t font_N[] = {0xFE,0x40,0x20,0x10,0xFE};
static const uint8_t font_S[] = {0x64,0x92,0x92,0x92,0x4C};
static const uint8_t font_P[] = {0xFE,0x90,0x90,0x90,0x60};
static const uint8_t font_I[] = {0x00,0x82,0xFE,0x82,0x00};
static const uint8_t font_U[] = {0xFC,0x02,0x02,0x02,0xFC};
static const uint8_t font_A[] = {0x7E,0x88,0x88,0x88,0x7E};
static const uint8_t font_R[] = {0xFE,0x90,0x98,0x94,0x62};
static const uint8_t font_O[] = {0x7C,0x82,0x82,0x82,0x7C};
/* font_K available if needed: {0xFE,0x10,0x28,0x44,0x82} */

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Functions
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t spi3_xfer(uint8_t tx) {
    uint32_t timeout = 100000;
    while (!(SPI3_STS & 0x02) && --timeout); /* TXE */
    if (!timeout) return 0xEE;
    SPI3_DT = tx;
    timeout = 100000;
    while (!(SPI3_STS & 0x01) && --timeout); /* RXNE */
    if (!timeout) return 0xEF;
    return (uint8_t)(SPI3_DT & 0xFF);
}

static void spi3_cs_assert(void)  { gpio_clr(GPIOB_BASE_ADDR, 6); }
static void spi3_cs_deassert(void){ gpio_set(GPIOB_BASE_ADDR, 6); }

/* ═══════════════════════════════════════════════════════════════════
 * USART2 Functions
 * ═══════════════════════════════════════════════════════════════════ */

static void usart2_send_byte(uint8_t b) {
    while (!(USART2_STS & (1 << 7))); /* TDBE */
    USART2_DT = b;
}

static void usart2_send_frame(uint8_t cmd) {
    /* 10-byte frame: [0x00 0x00 cmd_hi cmd_lo 0x00*5 checksum] */
    usart2_send_byte(0x00);  /* byte 0 */
    usart2_send_byte(0x00);  /* byte 1 */
    usart2_send_byte(0x00);  /* cmd high */
    usart2_send_byte(cmd);   /* cmd low */
    usart2_send_byte(0x00);  /* param 0 */
    usart2_send_byte(0x00);  /* param 1 */
    usart2_send_byte(0x00);  /* param 2 */
    usart2_send_byte(0x00);  /* param 3 */
    usart2_send_byte(0x00);  /* param 4 */
    usart2_send_byte(cmd);   /* checksum = (0x00 + cmd) & 0xFF */
    while (!(USART2_STS & (1 << 6))); /* TDC — wait for transmit complete */
}

static int usart2_rx_ready(void) {
    return (USART2_STS & (1 << 5)) ? 1 : 0; /* RDBF */
}

static uint8_t usart2_read_byte(void) {
    return (uint8_t)(USART2_DT & 0xFF);
}

/* Drain all pending RX bytes */
static void usart2_drain(void) {
    while (usart2_rx_ready()) (void)usart2_read_byte();
}

/* Try to receive a 12-byte frame with timeout */
static int usart2_recv_frame(uint8_t *buf, uint32_t timeout_ms) {
    uint32_t t;
    int idx = 0;

    /* Wait for frame start byte (0x5A or 0xAA) */
    t = timeout_ms * 1000;
    while (t--) {
        if (usart2_rx_ready()) {
            uint8_t b = usart2_read_byte();
            if (b == 0x5A || b == 0xAA) {
                buf[0] = b;
                idx = 1;
                break;
            }
        }
    }
    if (idx == 0) return 0;

    /* Read remaining bytes */
    uint8_t expected_len = (buf[0] == 0x5A) ? 12 : 10;
    for (int i = 1; i < expected_len; i++) {
        t = 50000;
        while (!usart2_rx_ready() && --t);
        if (!t) return idx;
        buf[i] = usart2_read_byte();
        idx++;
    }
    return idx;
}

/* ═══════════════════════════════════════════════════════════════════
 * Button Scanning (from stock firmware input_and_housekeeping)
 *
 * 3-group multiplexed scan on GPIOB/GPIOC.
 * Stock firmware uses bit masks: 0x0080, 0x0800, 0x0100, 0x0200,
 *   0x0020, 0x0010, 0x0008, 0x2000, 0x1000, 0x0004, 0x4000, 0x0002
 *
 * For this test we read GPIOB and GPIOC IDR directly and display
 * all bits, highlighting changes to find the exact mapping.
 * ═══════════════════════════════════════════════════════════════════ */

static uint16_t prev_portb = 0xFFFF;
static uint16_t prev_portc = 0xFFFF;
static uint32_t button_changes = 0;

static void scan_buttons(uint16_t *portb_out, uint16_t *portc_out) {
    *portb_out = gpio_read(GPIOB_BASE_ADDR);
    *portc_out = gpio_read(GPIOC_BASE_ADDR);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void) {
    /* ──────────────────────────────────────────────────────────────
     * BOOT: Power hold + clock
     * ────────────────────────────────────────────────────────────── */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE_ADDR, 9, 3, 0);
    gpio_set(GPIOC_BASE_ADDR, 9);  /* PC9 HIGH — power hold */

    system_clock_config();  /* 240 MHz */

    /* Enable all GPIO + peripheral clocks */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE_ADDR, 8, 3, 0);
    gpio_set(GPIOB_BASE_ADDR, 8);  /* PB8 HIGH */

    /* ──────────────────────────────────────────────────────────────
     * LCD Init (EXMC + ST7789V)
     * ────────────────────────────────────────────────────────────── */
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
    for (int i = 0; i < 12; i++)
        gpio_pin_cfg(GPIOD_BASE_ADDR, pd_pins[i], 3, 2);
    for (int i = 7; i <= 15; i++)
        gpio_pin_cfg(GPIOE_BASE_ADDR, i, 3, 2);

    *(volatile uint32_t *)0xA0000000 = 0x00005010;  /* SNCTL0 */
    *(volatile uint32_t *)0xA0000004 = 0x02020424;  /* SNTCFG0 */
    *(volatile uint32_t *)0xA0000104 = 0x00000202;  /* SNWTCFG0 */
    *(volatile uint32_t *)0xA0000000 |= 0x0001;     /* Enable */
    delay_ms(50);

    lcd_cmd_wr(0x01); delay_ms(200);  /* Software reset */
    lcd_cmd_wr(0x11); delay_ms(200);  /* Sleep out */
    lcd_cmd_wr(0x36); lcd_bus_delay(); lcd_dat(0xA0); delay_ms(10);  /* Landscape */
    lcd_cmd_wr(0x3A); lcd_bus_delay(); lcd_dat(0x55); delay_ms(10);  /* RGB565 */
    lcd_cmd_wr(0x29); delay_ms(50);  /* Display on */

    lcd_fill(0, 0, 320, 240, BG);

    /* Title */
    lcd_draw_glyph(5, 2, font5x7[15], ORNG, BG, 2);  /* F */
    lcd_draw_glyph(17, 2, font_U, ORNG, BG, 2);
    lcd_hex8(33, 2, 0x11, ORNG, BG, 2);  /* "11" = visual separator */
    lcd_draw_glyph(55, 2, font_T, ORNG, BG, 2);
    lcd_draw_glyph(67, 2, font5x7[14], ORNG, BG, 2);  /* E */
    lcd_draw_glyph(79, 2, font_S, ORNG, BG, 2);
    lcd_draw_glyph(91, 2, font_T, ORNG, BG, 2);

    uint16_t y = 20;

    /* ──────────────────────────────────────────────────────────────
     * STEP 1: IOMUX Remap — Disable JTAG, free PB3/PB4/PB5
     * From stock firmware: (AFIO_PCF0 & ~0xF000) | 0x2000
     * ────────────────────────────────────────────────────────────── */
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);  /* JTAG-DP off, SW-DP on */
    gpio_pin_remap_config(SPI3_GMUX_0010, TRUE);   /* SPI3 on PB3/PB4/PB5 */

    lcd_hex8(5, y, 0x01, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font_I, GREEN, BG, 1);  /* "I" for IOMUX */
    lcd_draw_glyph(26, y, font_O, GREEN, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 2: FPGA Control Pins — PC6 HIGH + PB11 HIGH
     * BOTH are required for FPGA to respond on SPI3!
     * ────────────────────────────────────────────────────────────── */
    gpio_pin_cfg(GPIOC_BASE_ADDR, 6, 3, 0);   /* PC6: push-pull output */
    gpio_set(GPIOC_BASE_ADDR, 6);              /* PC6 HIGH — FPGA SPI enable */

    gpio_pin_cfg(GPIOB_BASE_ADDR, 11, 3, 0);  /* PB11: push-pull output */
    gpio_set(GPIOB_BASE_ADDR, 11);             /* PB11 HIGH — FPGA active mode */

    lcd_hex8(5, y, 0x02, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font_P, GREEN, BG, 1);
    lcd_draw_glyph(26, y, font_I, GREEN, BG, 1);
    lcd_draw_glyph(32, y, font_N, GREEN, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 3: Configure SPI3 — Mode 3, Master, /2 (60MHz)
     * ────────────────────────────────────────────────────────────── */
    /* GPIO pins */
    gpio_pin_cfg(GPIOB_BASE_ADDR, 3, 3, 2);  /* PB3 SCK: AF push-pull */
    gpio_pin_cfg(GPIOB_BASE_ADDR, 4, 0, 1);  /* PB4 MISO: input floating */
    gpio_pin_cfg(GPIOB_BASE_ADDR, 5, 3, 2);  /* PB5 MOSI: AF push-pull */
    gpio_pin_cfg(GPIOB_BASE_ADDR, 6, 3, 0);  /* PB6 CS: push-pull output */
    spi3_cs_deassert();                        /* CS HIGH (idle) */

    /* Disable I2S mode first (stock firmware does this) */
    SPI3_I2SCTL &= ~(1 << 11);

    /* SPI3 config: Mode 3, Master, /2, 8-bit, MSB, software NSS */
    SPI3_CTL0 = 0;
    SPI3_CTL1 = 0;
    SPI3_CTL0 = (1 << 2)   /* MSTEN: Master */
              | (1 << 1)   /* CLKPOL: CPOL=1 (clock idle HIGH) */
              | (1 << 0)   /* CLKPHA: CPHA=1 (trailing edge sample) */
              | (0 << 3)   /* MDIV[2:0]: /2 = 60MHz */
              | (1 << 9)   /* SWCSEN: Software CS management */
              | (1 << 8);  /* SWCSIL: Internal CS high */

    /* CTL1: enable DMA request bits (stock firmware does this) */
    SPI3_CTL1 |= (1 << 0) | (1 << 1);  /* DMAREN + DMATEN */

    /* Enable SPI */
    SPI3_CTL0 |= (1 << 6);  /* SPIEN */

    lcd_hex8(5, y, 0x03, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font_S, GREEN, BG, 1);
    lcd_draw_glyph(26, y, font_P, GREEN, BG, 1);
    lcd_draw_glyph(32, y, font_I, GREEN, BG, 1);
    lcd_hex8(40, y, (uint8_t)(SPI3_CTL0 & 0xFF), CYAN, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 4: Configure USART2 — 9600 baud, PA2=TX, PA3=RX
     * ────────────────────────────────────────────────────────────── */
    gpio_pin_cfg(GPIOA_BASE_ADDR, 2, 3, 2);  /* PA2: AF push-pull (TX) */
    gpio_pin_cfg(GPIOA_BASE_ADDR, 3, 0, 1);  /* PA3: input floating (RX) */

    USART2_CTRL1 = 0;
    USART2_CTRL2 = 0;
    USART2_CTRL3 = 0;
    USART2_BAUDR = 120000000 / 9600;  /* 12500 — APB1 = 120MHz */
    USART2_CTRL1 = (1 << 13)  /* UEN */
                 | (1 << 3)   /* TEN */
                 | (1 << 2);  /* REN */

    lcd_hex8(5, y, 0x04, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font_U, GREEN, BG, 1);
    lcd_draw_glyph(26, y, font_A, GREEN, BG, 1);
    lcd_draw_glyph(32, y, font_R, GREEN, BG, 1);
    lcd_draw_glyph(38, y, font_T, GREEN, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 5: Wait for FPGA heartbeat on USART2 RX
     * ────────────────────────────────────────────────────────────── */
    delay_ms(300);  /* Wait for FPGA to start heartbeat */
    uint8_t hb[12] = {0};
    int hb_count = usart2_recv_frame(hb, 500);

    lcd_hex8(5, y, 0x05, (hb_count > 0) ? GREEN : RED, BG, 1);
    for (int i = 0; i < 8 && i < hb_count; i++) {
        lcd_hex8(20 + i * 16, y, hb[i], YELLO, BG, 1);
    }
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 6: Send USART boot commands (stock firmware sequence)
     * Commands: 0x01, 0x02, 0x06, 0x07, 0x08
     * With SysTick delays between phases
     * ────────────────────────────────────────────────────────────── */
    usart2_drain();
    uint8_t boot_cmds[] = {0x01, 0x02, 0x06, 0x07, 0x08};
    for (int c = 0; c < 5; c++) {
        usart2_send_frame(boot_cmds[c]);
        delay_ms(50);  /* Inter-command delay */
        usart2_drain();
    }

    lcd_hex8(5, y, 0x06, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font_B, GREEN, BG, 1);
    lcd_draw_glyph(26, y, font_O, GREEN, BG, 1);
    lcd_draw_glyph(32, y, font_O, GREEN, BG, 1);
    lcd_draw_glyph(38, y, font_T, GREEN, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 7: SysTick delays (stock firmware Phase 6)
     * ────────────────────────────────────────────────────────────── */
    systick_delay_us(10000);  /* 10ms */
    systick_delay_us(5000);   /* 5ms */
    systick_delay_us(50000);  /* 50ms — timed loop equivalent */

    lcd_hex8(5, y, 0x07, GREEN, BG, 1);
    lcd_draw_glyph(20, y, font5x7[0x0D], GREEN, BG, 1); /* D for Delay */
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 8: SPI3 FPGA Handshake (stock firmware Phase 7)
     * ────────────────────────────────────────────────────────────── */

    /* Step 8a: Dummy flush with CS high */
    spi3_cs_deassert();
    uint8_t h0 = spi3_xfer(0x00);

    /* Step 8b: CS assert, send 0x05 + 0x00 */
    spi3_cs_assert();
    uint8_t h1 = spi3_xfer(0x05);  /* FPGA command */
    uint8_t h2 = spi3_xfer(0x00);  /* parameter */
    spi3_cs_deassert();

    /* Step 8c: Dummy flushes */
    uint8_t h3 = spi3_xfer(0x00);
    uint8_t h4 = spi3_xfer(0x00);

    /* Step 8d: Second delay */
    systick_delay_us(50000);

    lcd_hex8(5, y, 0x08, ORNG, BG, 1);
    lcd_hex8(20, y, h0, (h0 != 0xFF) ? GREEN : GRAY, BG, 1);
    lcd_hex8(32, y, h1, (h1 != 0xFF) ? GREEN : GRAY, BG, 1);
    lcd_hex8(44, y, h2, (h2 != 0xFF) ? GREEN : GRAY, BG, 1);
    lcd_hex8(56, y, h3, (h3 != 0xFF) ? GREEN : GRAY, BG, 1);
    lcd_hex8(68, y, h4, (h4 != 0xFF) ? GREEN : GRAY, BG, 1);
    y += 10;

    /* ──────────────────────────────────────────────────────────────
     * STEP 9: Post-handshake SPI3 commands
     * Try sending scope/acquisition commands
     * ────────────────────────────────────────────────────────────── */

    /* Send more USART commands to put FPGA in scope mode */
    usart2_send_frame(0x0B);  /* scope channel config */
    delay_ms(50);
    usart2_drain();

    /* Now try a real SPI3 acquisition read */
    spi3_cs_assert();
    uint8_t acq_cmd = spi3_xfer(0xFF);  /* Bulk read command */
    uint8_t spi_data[16];
    for (int i = 0; i < 16; i++) {
        spi_data[i] = spi3_xfer(0xFF);
    }
    spi3_cs_deassert();

    lcd_hex8(5, y, 0x09, ORNG, BG, 1);
    lcd_hex8(20, y, acq_cmd, (acq_cmd != 0xFF) ? GREEN : RED, BG, 1);
    for (int i = 0; i < 8; i++) {
        uint16_t c = (spi_data[i] != 0xFF) ? GREEN : GRAY;
        lcd_hex8(36 + i * 16, y, spi_data[i], c, BG, 1);
    }
    y += 10;

    /* Second row of SPI data */
    for (int i = 8; i < 16; i++) {
        uint16_t c = (spi_data[i] != 0xFF) ? GREEN : GRAY;
        lcd_hex8(5 + (i-8) * 16, y, spi_data[i], c, BG, 1);
    }
    y += 12;

    /* ──────────────────────────────────────────────────────────────
     * LIVE MONITOR: Buttons + USART + SPI3
     *
     * Top section: Init results (above)
     * Bottom section: Live monitoring
     *   Line 1: GPIOB IDR (hex) — changed bits highlighted
     *   Line 2: GPIOC IDR (hex) — changed bits highlighted
     *   Line 3: Last USART RX frame
     *   Line 4: SPI3 probe (periodic)
     *   Line 5: Button event + frame counter
     * ────────────────────────────────────────────────────────────── */

    uint16_t live_y = 142;
    lcd_fill(0, live_y - 2, 320, 2, ORNG);  /* separator line */

    /* Labels */
    lcd_draw_glyph(5, live_y, font_B, DKGRN, BG, 1);  /* "B:" for GPIOB */
    lcd_draw_glyph(5, live_y + 12, font5x7[0x0C], DKGRN, BG, 1);  /* "C:" for GPIOC */
    lcd_draw_glyph(5, live_y + 24, font_R, DKGRN, BG, 1);  /* "R:" for RX */
    lcd_draw_glyph(5, live_y + 36, font_S, DKGRN, BG, 1);  /* "S:" for SPI */
    lcd_draw_glyph(5, live_y + 48, font5x7[14], DKGRN, BG, 1);  /* "E:" for Event */

    /* Initialize baseline GPIO readings */
    prev_portb = gpio_read(GPIOB_BASE_ADDR);
    prev_portc = gpio_read(GPIOC_BASE_ADDR);

    uint32_t frame_count = 0;
    uint32_t spi_probe_timer = 0;
    uint8_t last_rx[12] = {0};
    uint8_t last_spi[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    while (1) {
        /* ── Button Scan ──────────────────────────────────────── */
        uint16_t portb, portc;
        scan_buttons(&portb, &portc);

        /* Display GPIOB with change highlighting */
        uint16_t portb_diff = portb ^ prev_portb;
        lcd_hex16(14, live_y, portb, WHITE, BG, 1);
        if (portb_diff) {
            lcd_hex16(42, live_y, portb_diff, RED, BG, 1);
            button_changes++;
        }

        /* Display GPIOC with change highlighting */
        uint16_t portc_diff = portc ^ prev_portc;
        lcd_hex16(14, live_y + 12, portc, WHITE, BG, 1);
        if (portc_diff) {
            lcd_hex16(42, live_y + 12, portc_diff, RED, BG, 1);
            button_changes++;
        }

        /* Show specific button-mapped bits from stock firmware */
        /* Group 1 (GPIOC): 0x0080, 0x0800, 0x0100, 0x0200 */
        /* Group 2 (PB8-derived): 0x0020, 0x0010, 0x0008, 0x2000 */
        /* Group 3 (GPIOC bit 10): 0x1000, 0x0004, 0x4000, 0x0002 */
        uint16_t btn_c = portc & 0x4F86;  /* all GPIOC button bits OR'd */
        uint16_t btn_b = portb & 0x2038;  /* all GPIOB button bits OR'd */
        lcd_hex16(80, live_y, btn_b, CYAN, BG, 1);
        lcd_hex16(80, live_y + 12, btn_c, CYAN, BG, 1);

        /* Show total change count */
        lcd_hex16(14, live_y + 48, (uint16_t)button_changes, YELLO, BG, 1);

        prev_portb = portb;
        prev_portc = portc;

        /* ── USART RX Monitor ─────────────────────────────────── */
        if (usart2_rx_ready()) {
            uint8_t b = usart2_read_byte();
            if (b == 0x5A || b == 0xAA) {
                last_rx[0] = b;
                int len = (b == 0x5A) ? 12 : 10;
                for (int i = 1; i < len; i++) {
                    uint32_t t = 20000;
                    while (!usart2_rx_ready() && --t);
                    last_rx[i] = t ? usart2_read_byte() : 0x00;
                }
                /* Display frame */
                uint16_t fc = (last_rx[0] == 0x5A) ? CYAN : YELLO;
                for (int i = 0; i < 10; i++) {
                    lcd_hex8(14 + i * 16, live_y + 24, last_rx[i], fc, BG, 1);
                }
                frame_count++;
            }
        }

        /* ── Periodic SPI3 Probe ──────────────────────────────── */
        spi_probe_timer++;
        if (spi_probe_timer % 5000 == 0) {
            /* Send a USART command to stimulate FPGA */
            usart2_send_frame(0x07);

            delay_ms(20);

            /* Try SPI3 read */
            spi3_cs_assert();
            last_spi[0] = spi3_xfer(0xFF);
            for (int i = 1; i < 8; i++)
                last_spi[i] = spi3_xfer(0xFF);
            spi3_cs_deassert();

            /* Display */
            for (int i = 0; i < 8; i++) {
                uint16_t c = (last_spi[i] != 0xFF) ? GREEN : GRAY;
                lcd_hex8(14 + i * 16, live_y + 36, last_spi[i], c, BG, 1);
            }
        }

        /* Frame counter */
        lcd_hex16(50, live_y + 48, (uint16_t)frame_count, GRAY, BG, 1);
        lcd_hex16(80, live_y + 48, (uint16_t)(spi_probe_timer / 5000), GRAY, BG, 1);

        /* Small delay to keep scan rate reasonable */
        /* ~50 iterations/sec for button responsiveness */
        delay_ms(20);
    }
}
