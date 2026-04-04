/*
 * UART Sniffer for FNIRSI 2C53T
 * Initializes all USARTs as RX at 9600 baud.
 * Displays received byte count and last bytes for each USART on LCD.
 * Also shows GPIO registers for button cross-reference.
 *
 * AT32F403A USART default pins:
 *   USART1: TX=PA9,  RX=PA10  (APB2, 120MHz)
 *   USART2: TX=PA2,  RX=PA3   (APB1, 120MHz)
 *   USART3: TX=PB10, RX=PB11  (APB1, 120MHz)
 *   UART4:  TX=PC10, RX=PC11  (APB1, 120MHz)
 *   UART5:  TX=PC12, RX=PD2   (APB1, 120MHz)
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"
#include <string.h>

extern void system_clock_config(void);

/* LCD addresses */
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

static void lcd_cmd_wr(uint8_t cmd) {
    LCD_CMD = cmd;
    lcd_bus_delay();
}

static void lcd_dat(uint8_t data) {
    LCD_DATA = data;
    lcd_bus_delay();
}

static void lcd_dat16(uint16_t data) {
    LCD_DATA = data;
}

/* Simple 5x7 font for hex digits + letters */
static const uint8_t font5x7[][5] = {
    {0x7C,0x8A,0x92,0xA2,0x7C}, /* 0 */
    {0x00,0x42,0xFE,0x02,0x00}, /* 1 */
    {0x46,0x8A,0x92,0x92,0x62}, /* 2 */
    {0x44,0x82,0x92,0x92,0x6C}, /* 3 */
    {0x18,0x28,0x48,0xFE,0x08}, /* 4 */
    {0xE4,0xA2,0xA2,0xA2,0x9C}, /* 5 */
    {0x3C,0x52,0x92,0x92,0x0C}, /* 6 */
    {0x80,0x8E,0x90,0xA0,0xC0}, /* 7 */
    {0x6C,0x92,0x92,0x92,0x6C}, /* 8 */
    {0x60,0x92,0x92,0x94,0x78}, /* 9 */
    {0x7C,0x90,0x90,0x90,0x7C}, /* A */
    {0xFE,0x92,0x92,0x92,0x6C}, /* B */
    {0x7C,0x82,0x82,0x82,0x44}, /* C */
    {0xFE,0x82,0x82,0x82,0x7C}, /* D */
    {0xFE,0x92,0x92,0x92,0x82}, /* E */
    {0xFE,0x90,0x90,0x90,0x80}, /* F */
};

/* Additional letter glyphs for labels */
static const uint8_t glyph_U[] = {0xFC,0x02,0x02,0x02,0xFC};
static const uint8_t glyph_S[] = {0x64,0x92,0x92,0x92,0x4C};
static const uint8_t glyph_R[] = {0xFE,0x90,0x98,0x94,0x62};
static const uint8_t glyph_T[] = {0x80,0x80,0xFE,0x80,0x80};
static const uint8_t glyph_X[] = {0xC6,0x28,0x10,0x28,0xC6};
static const uint8_t glyph_N[] = {0xFE,0x40,0x20,0x10,0xFE};
static const uint8_t glyph_O[] = {0x7C,0x82,0x82,0x82,0x7C};
static const uint8_t glyph_I[] = {0x00,0x82,0xFE,0x82,0x00};
static const uint8_t glyph_P[] = {0xFE,0x90,0x90,0x90,0x60};

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    lcd_cmd_wr(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x + w - 1) >> 8); lcd_dat((x + w - 1) & 0xFF);
    lcd_cmd_wr(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y + h - 1) >> 8); lcd_dat((y + h - 1) & 0xFF);
    lcd_cmd_wr(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++)
        lcd_dat16(color);
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg, uint8_t scale) {
    lcd_set_window(x, y, 5*scale, 7*scale);
    for (int row = 0; row < 7; row++)
        for (int sr = 0; sr < scale; sr++)
            for (int col = 0; col < 5; col++)
                for (int sc = 0; sc < scale; sc++)
                    lcd_dat16((glyph[col] & (1 << (7-row))) ? fg : bg);
}

static void lcd_draw_char(uint16_t x, uint16_t y, uint8_t ch, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (ch >= '0' && ch <= '9') { lcd_draw_glyph(x, y, font5x7[ch - '0'], fg, bg, scale); return; }
    if (ch >= 'A' && ch <= 'F') { lcd_draw_glyph(x, y, font5x7[ch - 'A' + 10], fg, bg, scale); return; }
    if (ch >= 'a' && ch <= 'f') { lcd_draw_glyph(x, y, font5x7[ch - 'a' + 10], fg, bg, scale); return; }
    if (ch == 'U') { lcd_draw_glyph(x, y, glyph_U, fg, bg, scale); return; }
    if (ch == 'S') { lcd_draw_glyph(x, y, glyph_S, fg, bg, scale); return; }
    if (ch == 'R') { lcd_draw_glyph(x, y, glyph_R, fg, bg, scale); return; }
    if (ch == 'T') { lcd_draw_glyph(x, y, glyph_T, fg, bg, scale); return; }
    if (ch == 'X') { lcd_draw_glyph(x, y, glyph_X, fg, bg, scale); return; }
    if (ch == 'N') { lcd_draw_glyph(x, y, glyph_N, fg, bg, scale); return; }
    if (ch == 'O') { lcd_draw_glyph(x, y, glyph_O, fg, bg, scale); return; }
    if (ch == 'I') { lcd_draw_glyph(x, y, glyph_I, fg, bg, scale); return; }
    if (ch == 'P') { lcd_draw_glyph(x, y, glyph_P, fg, bg, scale); return; }
    if (ch == ':') {
        lcd_fill(x, y, 5*scale, 7*scale, bg);
        lcd_fill(x+2*scale, y+1*scale, scale, scale, fg);
        lcd_fill(x+2*scale, y+5*scale, scale, scale, fg);
        return;
    }
    if (ch == ' ' || ch == '-' || ch == '.' || ch == '/') {
        lcd_fill(x, y, 5*scale, 7*scale, bg);
        return;
    }
    lcd_fill(x, y, 5*scale, 7*scale, bg);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        lcd_draw_char(x, y, *s, fg, bg, scale);
        x += 6 * scale;
        s++;
    }
}

static void lcd_draw_hex8(uint16_t x, uint16_t y, uint8_t val, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[3];
    buf[0] = "0123456789ABCDEF"[(val >> 4) & 0xF];
    buf[1] = "0123456789ABCDEF"[val & 0xF];
    buf[2] = 0;
    lcd_draw_string(x, y, buf, fg, bg, scale);
}

static void lcd_draw_hex16(uint16_t x, uint16_t y, uint16_t val, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[5];
    buf[0] = "0123456789ABCDEF"[(val >> 12) & 0xF];
    buf[1] = "0123456789ABCDEF"[(val >> 8) & 0xF];
    buf[2] = "0123456789ABCDEF"[(val >> 4) & 0xF];
    buf[3] = "0123456789ABCDEF"[val & 0xF];
    buf[4] = 0;
    lcd_draw_string(x, y, buf, fg, bg, scale);
}

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

/* Ring buffer for each USART */
#define RXBUF_SIZE 16
typedef struct {
    usart_type *usart;
    uint8_t buf[RXBUF_SIZE];
    uint16_t count;
    uint8_t head;
} uart_rx_t;

static uart_rx_t uarts[5];

static void usart_init_rx(usart_type *usart, uint32_t apb_clock) {
    /* 9600 baud: BRG = apb_clock / 9600 */
    uint32_t div = apb_clock / 9600;
    usart->baudr = div;
    usart->ctrl1 = 0;  /* Reset */
    usart->ctrl1 |= (1 << 2);   /* RE - receiver enable */
    usart->ctrl1 |= (1 << 13);  /* UEN - USART enable */
}

static void poll_usart(uart_rx_t *u) {
    while (u->usart->sts & (1 << 5)) {  /* RDBF - receive data buffer full */
        uint8_t byte = u->usart->dt & 0xFF;
        u->buf[u->head] = byte;
        u->head = (u->head + 1) % RXBUF_SIZE;
        u->count++;
    }
    /* Clear any overrun by reading sts then dt */
    if (u->usart->sts & (1 << 3)) {  /* ROERR */
        (void)u->usart->sts;
        (void)u->usart->dt;
    }
}

int main(void) {
    /* Power hold */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    /* Clock - 240MHz */
    system_clock_config();

    /* Enable all GPIO clocks + IOMUX + XMC */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* Enable USART clocks */
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART3_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_UART4_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_UART5_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1 << 8);

    /* Configure RX pins as input floating (mode=0, cnf=1) */
    gpio_pin_cfg(GPIOA_BASE, 10, 0, 1);  /* USART1 RX = PA10 */
    gpio_pin_cfg(GPIOA_BASE, 3,  0, 1);  /* USART2 RX = PA3  */
    gpio_pin_cfg(GPIOB_BASE, 11, 0, 1);  /* USART3 RX = PB11 */
    gpio_pin_cfg(GPIOC_BASE, 11, 0, 1);  /* UART4  RX = PC11 */
    /* Skip UART5 PD2 — may conflict with EXMC */

    /* LCD init - proven sequence */
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

    /* Clear screen */
    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Title */
    lcd_draw_string(10, 2, "UART SNIFF", 0xFFFF, 0x0008, 2);

    /* APB1 = 120MHz (PCLK1 = SCLK/2), APB2 = 240MHz (PCLK2 = SCLK) */
    /* Actually AT32 APB1 max is 120MHz, APB2 max is 240MHz */
    uint32_t apb1_clock = system_core_clock / 2;  /* 120MHz */
    uint32_t apb2_clock = system_core_clock;       /* 240MHz */

    /* Init USARTs - RX only at 9600 baud */
    uarts[0].usart = USART1;
    uarts[1].usart = USART2;
    uarts[2].usart = USART3;
    uarts[3].usart = UART4;
    uarts[4].usart = UART5;
    for (int i = 0; i < 5; i++) {
        uarts[i].count = 0;
        uarts[i].head = 0;
        memset(uarts[i].buf, 0, RXBUF_SIZE);
    }

    usart_init_rx(USART1, apb2_clock);
    usart_init_rx(USART2, apb1_clock);
    usart_init_rx(USART3, apb1_clock);
    usart_init_rx(UART4,  apb1_clock);
    /* Skip UART5 */

    /* Watchdog — 4 second timeout, auto-reboot on crash */
    wdt_register_write_enable(TRUE);
    wdt_divider_set(WDT_CLK_DIV_64);
    wdt_reload_value_set(1875);  /* ~4 seconds */
    wdt_counter_reload();
    wdt_enable();

    /* Labels */
    uint16_t y = 22;
    lcd_draw_string(5, y, "U2 PA3 FPGA", 0x07E0, 0x0008, 2);

    /* Show all USARTs status in small text */
    lcd_draw_string(5, y + 22, "U1", 0xFFE0, 0x0008, 1);
    lcd_draw_string(5, y + 32, "U3", 0x07FF, 0x0008, 1);
    lcd_draw_string(5, y + 42, "U4", 0xF81F, 0x0008, 1);

    /* U2 frame display area — show 12 bytes per frame, last 8 frames */
    uint16_t frame_y = y + 55;
    lcd_draw_string(5, frame_y, "FRAMES:", 0xFFFF, 0x0008, 2);

    /* GPIO at bottom */
    uint16_t gy = 200;
    lcd_draw_string(5, gy, "PB:", 0x07E0, 0x0008, 2);
    lcd_draw_string(5, gy + 18, "PC:", 0x07FF, 0x0008, 2);

    /* Track U2 frames — 12 bytes each */
    #define FRAME_SIZE 12
    #define MAX_FRAMES 6
    uint8_t frames[MAX_FRAMES][FRAME_SIZE];
    uint8_t frame_buf[FRAME_SIZE];
    uint8_t frame_pos = 0;
    uint8_t frame_count = 0;
    uint8_t frame_idx = 0;
    uint8_t synced = 0;

    uint16_t prev_counts[5] = {0};
    uint16_t prev_pb = 0xFFFF, prev_pc = 0xFFFF;

    while (1) {
        wdt_counter_reload();  /* Feed watchdog */

        /* Poll all USARTs */
        for (int i = 0; i < 4; i++)  /* Only 4 now, skip UART5 */
            poll_usart(&uarts[i]);

        /* Process U2 bytes into frames */
        uart_rx_t *u2 = &uarts[1];
        while (u2->count > prev_counts[1]) {
            /* Get next byte from ring buffer */
            int rd_idx = (u2->head - (u2->count - prev_counts[1]) + RXBUF_SIZE) % RXBUF_SIZE;
            uint8_t byte = u2->buf[rd_idx];
            prev_counts[1]++;

            /* Sync on 5A A5 header */
            if (!synced) {
                if (frame_pos == 0 && byte == 0x5A) {
                    frame_buf[0] = byte;
                    frame_pos = 1;
                } else if (frame_pos == 1 && byte == 0xA5) {
                    frame_buf[1] = byte;
                    frame_pos = 2;
                    synced = 1;
                } else {
                    frame_pos = 0;
                }
            } else {
                frame_buf[frame_pos++] = byte;
                if (frame_pos >= FRAME_SIZE) {
                    /* Complete frame — store it */
                    memcpy(frames[frame_idx], frame_buf, FRAME_SIZE);
                    frame_idx = (frame_idx + 1) % MAX_FRAMES;
                    if (frame_count < MAX_FRAMES) frame_count++;

                    /* Display this frame */
                    uint16_t fy = frame_y + 18 + ((frame_count <= MAX_FRAMES ? frame_idx - 1 : frame_idx) % MAX_FRAMES) * 18;
                    if (fy > 195) fy = frame_y + 18;  /* wrap */
                    for (int j = 0; j < FRAME_SIZE; j++) {
                        lcd_draw_hex8(5 + j * 26, fy, frame_buf[j], 0xFFFF, 0x0008, 2);
                    }

                    /* Reset for next frame */
                    frame_pos = 0;
                    synced = 0;
                }
            }
        }

        /* Show other USART counts */
        if (uarts[0].count != prev_counts[0]) {
            lcd_draw_hex16(25, y + 22, uarts[0].count, 0xFFE0, 0x0008, 1);
            prev_counts[0] = uarts[0].count;
        }
        if (uarts[2].count != prev_counts[2]) {
            lcd_draw_hex16(25, y + 32, uarts[2].count, 0x07FF, 0x0008, 1);
            prev_counts[2] = uarts[2].count;
        }
        if (uarts[3].count != prev_counts[3]) {
            lcd_draw_hex16(25, y + 42, uarts[3].count, 0xF81F, 0x0008, 1);
            prev_counts[3] = uarts[3].count;
        }

        /* GPIO */
        uint16_t pb = GPIOB->idt & 0xFFFF;
        uint16_t pc = GPIOC->idt & 0xFFFF;
        if (pb != prev_pb) {
            lcd_draw_hex16(45, gy, pb, 0xF800, 0x0008, 2);
            prev_pb = pb;
        }
        if (pc != prev_pc) {
            lcd_draw_hex16(45, gy + 18, pc, 0xF800, 0x0008, 2);
            prev_pc = pc;
        }

        delay_ms(20);
    }
}
