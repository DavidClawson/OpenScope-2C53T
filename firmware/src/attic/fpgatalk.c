/*
 * FPGA Talker for FNIRSI 2C53T
 * Sends 10-byte command frames to FPGA via USART2 (PA2 TX / PA3 RX) at 9600 baud.
 * Displays RX frames on LCD, highlights bytes that change (button detection).
 *
 * Protocol (from decompiled firmware):
 *   TX: 10 bytes — [0][1][param_lo][param_hi][4][5][6][7][8][checksum]
 *   RX: 2-byte ACK [5A A5] or 10-byte status [AA 55 ...] or 12-byte completion
 *   checksum = param_hi + param_lo
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

/* Minimal 5x7 font for hex + some letters */
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
    uint8_t hi = (val >> 4) & 0xF, lo = val & 0xF;
    lcd_draw_glyph(x, y, font5x7[hi], fg, bg, s);
    lcd_draw_glyph(x + 6*s, y, font5x7[lo], fg, bg, s);
}

/* RX ring buffer */
#define RXBUF_SIZE 64
static volatile uint8_t rx_buf[RXBUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint16_t rx_count = 0;

static void usart2_init(void) {
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    /* PA2 = TX (AF push-pull), PA3 = RX (input floating) */
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 1);

    /* 9600 baud, APB1 = 120MHz */
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = 0;
    USART2->ctrl1 = (1 << 2) | (1 << 3) | (1 << 13);  /* RE + TE + UEN */
}

static void usart2_send(const uint8_t *data, uint8_t len) {
    for (int i = 0; i < len; i++) {
        while (!(USART2->sts & (1 << 7))) {}  /* Wait TDE */
        USART2->dt = data[i];
    }
    while (!(USART2->sts & (1 << 6))) {}  /* Wait TRAC (transmit complete) */
}

static uint8_t usart2_rx_available(void) {
    return (USART2->sts & (1 << 5)) ? 1 : 0;  /* RDBF */
}

static uint8_t usart2_rx_byte(void) {
    return USART2->dt & 0xFF;
}

/* Drain all available RX bytes into ring buffer */
static void usart2_drain(void) {
    while (usart2_rx_available()) {
        rx_buf[rx_head] = usart2_rx_byte();
        rx_head = (rx_head + 1) % RXBUF_SIZE;
        rx_count++;
    }
    /* Clear overrun if any */
    if (USART2->sts & (1 << 3)) {
        (void)USART2->sts;
        (void)USART2->dt;
    }
}

/* Build a 10-byte TX frame */
static void build_tx_frame(uint8_t *frame, uint8_t param_lo, uint8_t param_hi) {
    memset(frame, 0, 10);
    frame[2] = param_lo;
    frame[3] = param_hi;
    frame[9] = param_hi + param_lo;  /* Checksum */
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

    /* Baud rate auto-detect: try each rate for 3 seconds, count valid frames */
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 2);  /* PA3 = RX input pull-up/pull-down */
    GPIOA->scr = (1 << 3);              /* Pull-UP (set bit = pull-up) */

    uint32_t bauds[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    uint8_t num_bauds = 8;
    uint16_t frames_at[8] = {0};
    uint16_t bytes_at[8] = {0};
    uint8_t best_baud = 0;

    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Test each baud rate */
    for (uint8_t b = 0; b < num_bauds; b++) {
        wdt_counter_reload();

        /* Configure USART2 */
        USART2->ctrl1 = 0;  /* Disable */
        USART2->baudr = 120000000 / bauds[b];
        USART2->ctrl1 = (1 << 2) | (1 << 13);  /* RE + UEN */

        /* Flush any pending data */
        while (USART2->sts & (1 << 5)) { (void)USART2->dt; }
        if (USART2->sts & (1 << 3)) { (void)USART2->sts; (void)USART2->dt; }

        /* Show current baud rate being tested */
        lcd_fill(5, 2, 310, 16, 0x0008);
        lcd_hex8(5, 2, b, 0xFFFF, 0x0008, 2);
        /* Show baud as hex (rough indicator) */
        lcd_hex8(40, 2, (bauds[b] >> 8) & 0xFF, 0x07E0, 0x0008, 2);
        lcd_hex8(65, 2, bauds[b] & 0xFF, 0x07E0, 0x0008, 2);

        /* Collect data for 3 seconds */
        uint16_t rx_count = 0;
        uint16_t frame_count = 0;
        uint8_t frame_pos = 0;
        uint8_t a5_count = 0;

        for (int t = 0; t < 60; t++) {  /* 60 * 50ms = 3 seconds */
            wdt_counter_reload();

            while (USART2->sts & (1 << 5)) {
                uint8_t byte = USART2->dt & 0xFF;
                rx_count++;

                if (byte == 0xA5) a5_count++;

                /* Simple frame counter */
                if (frame_pos == 0 && byte == 0x5A) { frame_pos = 1; }
                else if (frame_pos == 1 && byte == 0xA5) { frame_pos = 2; }
                else if (frame_pos == 1) { frame_pos = (byte == 0x5A) ? 1 : 0; }
                else if (frame_pos >= 2) {
                    frame_pos++;
                    if (frame_pos >= 12) { frame_count++; frame_pos = 0; }
                }
            }
            if (USART2->sts & (1 << 3)) { (void)USART2->sts; (void)USART2->dt; }

            delay_ms(50);
        }

        frames_at[b] = frame_count;
        bytes_at[b] = rx_count;

        /* Display result for this baud */
        uint16_t y = 22 + b * 26;
        /* Baud rate indicator */
        lcd_hex8(5, y, (bauds[b] >> 16) & 0xFF, 0x8410, 0x0008, 2);
        lcd_hex8(30, y, (bauds[b] >> 8) & 0xFF, 0x8410, 0x0008, 2);
        lcd_hex8(55, y, bauds[b] & 0xFF, 0x8410, 0x0008, 2);

        /* Bytes received */
        lcd_hex8(100, y, (rx_count >> 8) & 0xFF, 0xFFFF, 0x0008, 2);
        lcd_hex8(125, y, rx_count & 0xFF, 0xFFFF, 0x0008, 2);

        /* Frames found */
        uint16_t fc_color = (frame_count > 0) ? 0x07E0 : 0xF800;
        lcd_hex8(170, y, (frame_count >> 8) & 0xFF, fc_color, 0x0008, 2);
        lcd_hex8(195, y, frame_count & 0xFF, fc_color, 0x0008, 2);

        /* A5 count — if correct baud, should see many A5 bytes */
        lcd_hex8(240, y, a5_count, 0x07FF, 0x0008, 2);

        /* Green bar for frame count */
        if (frame_count > 0) {
            uint16_t bar = (frame_count > 40) ? 40 : frame_count;
            lcd_fill(275, y, bar * 1, 14, 0x07E0);
        }

        if (frame_count > frames_at[best_baud]) best_baud = b;
    }

    /* Highlight best baud */
    lcd_fill(0, 22 + best_baud * 26, 4, 14, 0xFFE0);

    /* Now run continuously at best baud rate showing frames */
    USART2->ctrl1 = 0;
    USART2->baudr = 120000000 / bauds[best_baud];
    USART2->ctrl1 = (1 << 2) | (1 << 13);

    /* Sit here forever, watchdog keeps us alive */
    while (1) {
        wdt_counter_reload();
        delay_ms(100);
    }
}
