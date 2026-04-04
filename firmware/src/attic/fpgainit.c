/*
 * FPGA Initialization & Response Monitor
 * Sends oscilloscope mode commands to FPGA via USART2 at 9600 baud,
 * displays response frames on LCD to discover button data encoding.
 *
 * TX Frame (10 bytes): AA 55 [cmd_hi] [cmd_lo] [id] 00 00 00 AA [checksum]
 * RX Frame (12 bytes): 5A A5 [10 data bytes]
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

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg, uint8_t s) {
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

/* USART2 at 9600 baud */
static void usart2_init(void) {
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    /* PA2 = USART2 TX (alt push-pull), PA3 = USART2 RX (input floating) */
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);  /* AF push-pull, 50MHz */
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 1);  /* Input floating */

    /* 9600 baud at 120MHz APB1 (PCLK1 = HCLK/2 = 120MHz) */
    /* BRR = PCLK1 / baud = 120000000 / 9600 = 12500 = 0x30D4 */
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = 0;
    USART2->ctrl1 |= (1 << 2);   /* RE: Receiver enable */
    USART2->ctrl1 |= (1 << 3);   /* TE: Transmitter enable */
    USART2->ctrl1 |= (1 << 13);  /* UEN: USART enable */
}

static void usart2_send_byte(uint8_t b) {
    while (!(USART2->sts & (1 << 7)));  /* Wait for TDC (TX data empty) */
    USART2->dt = b;
}

static int usart2_rx_available(void) {
    return (USART2->sts & (1 << 5)) ? 1 : 0;  /* RDBF: RX data buffer full */
}

static uint8_t usart2_read_byte(void) {
    return (uint8_t)USART2->dt;
}

/* Send a 10-byte FPGA command frame */
static void fpga_send_cmd(uint8_t cmd) {
    uint8_t frame[10];
    frame[0] = 0xAA;       /* Header */
    frame[1] = 0x55;       /* Header */
    frame[2] = 0x00;       /* Command high byte */
    frame[3] = cmd;        /* Command low byte */
    frame[4] = 0x00;       /* ID / fixed (unknown) */
    frame[5] = 0x00;
    frame[6] = 0x00;
    frame[7] = 0x00;
    frame[8] = 0xAA;       /* Trailer */
    frame[9] = cmd;        /* Checksum: byte2 + byte3 */

    for (int i = 0; i < 10; i++) {
        usart2_send_byte(frame[i]);
    }
}

/* Oscilloscope mode init commands */
static const uint8_t scope_cmds[] = {
    0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
};

/* Raw byte stream buffer — show exactly what FPGA sends */
static uint8_t stream[120];  /* 10 rows x 12 bytes */
static uint16_t stream_total = 0;
static uint8_t stream_pos = 0;  /* 0-119, wraps */

static void process_rx(void) {
    while (usart2_rx_available()) {
        uint8_t b = usart2_read_byte();
        stream[stream_pos] = b;
        stream_pos++;
        if (stream_pos >= 120) stream_pos = 0;
        stream_total++;
    }
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

    /* Watchdog */
    wdt_register_write_enable(TRUE);
    wdt_divider_set(WDT_CLK_DIV_64);
    wdt_reload_value_set(1875);
    wdt_counter_reload();
    wdt_enable();

    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Init USART2 */
    usart2_init();
    delay_ms(100);

    /* Send oscilloscope mode init commands to FPGA */
    for (int i = 0; i < (int)(sizeof(scope_cmds)/sizeof(scope_cmds[0])); i++) {
        fpga_send_cmd(scope_cmds[i]);
        delay_ms(50);  /* Give FPGA time to process each command */
        process_rx();  /* Drain any responses */
    }

    memset(stream, 0, sizeof(stream));

    while (1) {
        wdt_counter_reload();
        process_rx();

        /* Total byte count (yellow) */
        lcd_hex8(5, 5, (stream_total >> 8) & 0xFF, 0xFFE0, 0x0008, 2);
        lcd_hex8(30, 5, stream_total & 0xFF, 0xFFE0, 0x0008, 2);

        /* Raw hex dump: 10 rows of 12 bytes = 120 bytes visible */
        /* Start from oldest byte so newest is at bottom */
        uint8_t start = stream_pos;  /* This is where next byte goes = oldest */
        for (int row = 0; row < 10; row++) {
            uint16_t y = 25 + row * 20;
            for (int col = 0; col < 12; col++) {
                uint8_t idx = (start + row * 12 + col) % 120;
                uint8_t val = stream[idx];

                /* Color code: 5A=dim, A5=yellow, 00=gray, others=bright */
                uint16_t color;
                if (val == 0x5A) color = 0x2945;       /* Dim blue-gray */
                else if (val == 0xA5) color = 0xFFE0;   /* Yellow */
                else if (val == 0x00) color = 0x4208;    /* Dark gray */
                else color = 0x07FF;                     /* Cyan = interesting */

                lcd_hex8(5 + col * 26, y, val, color, 0x0008, 2);
            }
        }

        delay_ms(100);
    }
}
