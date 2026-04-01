/*
 * fulltest2.c — FPGA + Button Matrix Scan + TMR3
 * FNIRSI 2C53T — AT32F403A @ 240MHz
 *
 * V2 improvements over fulltest.c:
 *   - Active bidirectional 4x3 GPIO matrix button scanning
 *   - TMR3 at 500Hz driving the scan (matches stock firmware)
 *   - TMR3-paced USART exchange (stock firmware pattern)
 *   - Displays button bit position and event mask on press
 *
 * Button Matrix (from stock firmware decompilation):
 *   Rows: PA7, PB0, PC5, PE2 (Phase 1: input pull-up)
 *   Cols: PA8, PC10, PE3      (Phase 1: output LOW)
 *   Passive: PC8 (active LOW), PB7 (active HIGH!), PC13 (active LOW)
 *
 * Build: make -f Makefile.hwtest TEST=fulltest2
 * Flash: make -f Makefile.hwtest TEST=fulltest2 flash
 */
#include "at32f403a_407.h"
#include <string.h>

extern void system_clock_config(void);

/* ═══════════════════════════════════════════════════════════════════
 * Hardware Registers (direct access for speed in ISR)
 * ═══════════════════════════════════════════════════════════════════ */

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

#undef SPI3_BASE
#define SPI3_BASE   0x40003C00
#define SPI3_CTL0   (*(volatile uint32_t *)(SPI3_BASE + 0x00))
#define SPI3_CTL1   (*(volatile uint32_t *)(SPI3_BASE + 0x04))
#define SPI3_STS    (*(volatile uint32_t *)(SPI3_BASE + 0x08))
#define SPI3_DT     (*(volatile uint32_t *)(SPI3_BASE + 0x0C))
#define SPI3_I2SCTL (*(volatile uint32_t *)(SPI3_BASE + 0x1C))

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
 * Volatile globals (shared between ISR and main)
 * ═══════════════════════════════════════════════════════════════════ */

static volatile uint16_t g_button_raw = 0;      /* raw scan result (15 bits) */
static volatile uint16_t g_button_prev = 0;      /* previous scan for diff */
static volatile uint8_t  g_button_confirmed = 0; /* debounced button bit index (0-14) + 0x80 flag */
static volatile uint32_t g_scan_count = 0;       /* TMR3 tick counter */
static volatile uint8_t  g_phase1_row = 0;       /* Phase 1 raw row state */

/* Debounce counters (15 buttons) */
static uint8_t g_debounce[15] = {0};

/* USART exchange state */
static volatile uint8_t  g_usart_tx_buf[10] = {0};
static volatile uint8_t  g_usart_rx_buf[12] = {0};
static volatile uint8_t  g_usart_tx_idx = 0;
static volatile uint8_t  g_usart_rx_idx = 0;
static volatile uint8_t  g_usart_rx_ready = 0;
static volatile uint32_t g_frame_count = 0;

/* ═══════════════════════════════════════════════════════════════════
 * LCD / Font Primitives
 * ═══════════════════════════════════════════════════════════════════ */

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
static void lcd_dat(uint8_t data)   { LCD_DATA = data; lcd_bus_delay(); }
static void lcd_dat16(uint16_t data){ LCD_DATA = data; }

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

/* ═══════════════════════════════════════════════════════════════════
 * GPIO helpers for matrix scan (fast, inline-friendly)
 * ═══════════════════════════════════════════════════════════════════ */

static void pin_input_pullup(gpio_type *port, uint16_t pin) {
    gpio_init_type cfg;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pin;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(port, &cfg);
}

static void pin_output_pp(gpio_type *port, uint16_t pin) {
    gpio_init_type cfg;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pin;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(port, &cfg);
}

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Functions
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t spi3_xfer(uint8_t tx) {
    uint32_t timeout = 100000;
    while (!(SPI3_STS & 0x02) && --timeout);
    if (!timeout) return 0xEE;
    SPI3_DT = tx;
    timeout = 100000;
    while (!(SPI3_STS & 0x01) && --timeout);
    if (!timeout) return 0xEF;
    return (uint8_t)(SPI3_DT & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════
 * TMR3 ISR — 500 Hz Button Scan + USART Exchange
 *
 * This matches the stock firmware's timing exactly:
 *   - Bidirectional matrix scan every 2ms
 *   - Debounce with 70-tick (140ms) confirm threshold
 *   - Periodic USART TX to keep FPGA exchange alive
 * ═══════════════════════════════════════════════════════════════════ */

void TMR3_GLOBAL_IRQHandler(void) {
    if (!(TMR3->ists & 0x01)) return;
    TMR3->ists &= ~0x01;  /* Clear update interrupt flag */

    g_scan_count++;
    uint16_t events = 0;
    uint8_t input_state = 0;

    /* ── Passive button reads ──────────────────────────────── */
    if (!(GPIOC->idt & (1 << 8)))   events |= 0x0001;  /* PC8 LOW = pressed */
    if (GPIOB->idt & (1 << 7))      events |= 0x0040;  /* PB7 HIGH = pressed (active HIGH!) */
    if (!(GPIOC->idt & (1 << 13)))  events |= 0x0400;  /* PC13 LOW = pressed */

    /* ── Phase 1: Rows = input pull-up, Cols = output LOW ── */
    pin_input_pullup(GPIOA, GPIO_PINS_7);
    pin_input_pullup(GPIOB, GPIO_PINS_0);
    pin_input_pullup(GPIOC, GPIO_PINS_5);
    pin_input_pullup(GPIOE, GPIO_PINS_2);

    pin_output_pp(GPIOA, GPIO_PINS_8);
    pin_output_pp(GPIOC, GPIO_PINS_10);
    pin_output_pp(GPIOE, GPIO_PINS_3);

    GPIOA->clr = (1 << 8);   /* PA8 LOW */
    GPIOC->clr = (1 << 10);  /* PC10 LOW */
    GPIOE->clr = (1 << 3);   /* PE3 LOW */

    __NOP(); __NOP(); __NOP(); __NOP();  /* settle */

    /* Read rows */
    if (!(GPIOA->idt & (1 << 7)))  input_state |= 1;
    if (!(GPIOC->idt & (1 << 5)))  input_state |= 2;
    if (!(GPIOB->idt & (1 << 0)))  input_state |= 4;
    if (!(GPIOE->idt & (1 << 2)))  input_state |= 8;

    g_phase1_row = input_state;  /* expose for debug display */

    /* Only proceed if exactly one row active */
    if (input_state == 1 || input_state == 2 ||
        input_state == 4 || input_state == 8) {

        /* ── Phase 2: Swap — Rows = output LOW, Cols = input pull-up ── */
        pin_input_pullup(GPIOA, GPIO_PINS_8);
        pin_input_pullup(GPIOC, GPIO_PINS_10);
        pin_input_pullup(GPIOE, GPIO_PINS_3);

        pin_output_pp(GPIOA, GPIO_PINS_7);
        pin_output_pp(GPIOB, GPIO_PINS_0);
        pin_output_pp(GPIOC, GPIO_PINS_5);
        pin_output_pp(GPIOE, GPIO_PINS_2);

        GPIOA->clr = (1 << 7);   /* PA7 LOW */
        GPIOB->clr = (1 << 0);   /* PB0 LOW */
        GPIOC->clr = (1 << 5);   /* PC5 LOW */
        GPIOE->clr = (1 << 2);   /* PE2 LOW */

        __NOP(); __NOP(); __NOP(); __NOP();

        /* Read columns, combine with row to identify button */
        if (!(GPIOE->idt & (1 << 3))) {  /* PE3 = col 1 */
            switch (input_state) {
                case 1: events |= 0x0080; break;
                case 2: events |= 0x0800; break;
                case 4: events |= 0x0100; break;
                case 8: events |= 0x0200; break;
            }
        }
        if (!(GPIOA->idt & (1 << 8))) {  /* PA8 = col 2 */
            switch (input_state) {
                case 1: events |= 0x0020; break;
                case 2: events |= 0x0010; break;
                case 4: events |= 0x0008; break;
                case 8: events |= 0x2000; break;
            }
        }
        if (!(GPIOC->idt & (1 << 10))) {  /* PC10 = col 3 */
            switch (input_state) {
                case 1: events |= 0x1000; break;
                case 2: events |= 0x0004; break;
                case 4: events |= 0x4000; break;
                case 8: events |= 0x0002; break;
            }
        }
    }

    /* ── Debounce ──────────────────────────────────────────── */
    for (int i = 0; i < 15; i++) {
        uint16_t mask = (1 << i);
        if (events & mask) {
            if (g_debounce[i] < 0xFF) g_debounce[i]++;
            if (g_debounce[i] == 70) {
                g_button_confirmed = (uint8_t)i | 0x80;  /* flag + bit index */
            }
        } else {
            g_debounce[i] = 0;
        }
    }

    g_button_raw = events;

    /* ── Periodic USART TX (every 10 ticks = 20ms) ────────── */
    if ((g_scan_count % 10) == 0) {
        /* Send a scope mode command to keep FPGA exchange alive */
        uint8_t cmd = 0x07;
        uint8_t frame[10] = {0,0, 0,cmd, 0,0,0,0,0, cmd};
        for (int i = 0; i < 10; i++) {
            uint32_t t = 10000;
            while (!(USART2->sts & (1 << 7)) && --t);
            if (t) USART2->dt = frame[i];
        }
    }

    /* ── Drain USART RX into buffer ───────────────────────── */
    while (USART2->sts & (1 << 5)) {
        uint8_t b = (uint8_t)(USART2->dt & 0xFF);
        if (g_usart_rx_idx == 0) {
            if (b == 0x5A) {
                g_usart_rx_buf[0] = b;
                g_usart_rx_idx = 1;
            }
        } else {
            g_usart_rx_buf[g_usart_rx_idx++] = b;
            if (g_usart_rx_idx >= 12) {
                if (g_usart_rx_buf[1] == 0xA5) {
                    g_usart_rx_ready = 1;
                    g_frame_count++;
                }
                g_usart_rx_idx = 0;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void) {
    /* ── Power hold + clock ─────────────────────────────────── */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xF << 4)) | (0x3 << 4);
    GPIOC->scr = (1 << 9);  /* PC9 HIGH */

    system_clock_config();

    /* Enable ALL GPIO clocks (buttons use A, B, C, E) */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_8;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1 << 8);

    /* ── LCD Init ───────────────────────────────────────────── */
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
    for (int i = 0; i < 12; i++) {
        gpio_cfg.gpio_pins = (1 << pd_pins[i]);
        gpio_cfg.gpio_mode = GPIO_MODE_MUX;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
        gpio_init(GPIOD, &gpio_cfg);
    }
    for (int i = 7; i <= 15; i++) {
        gpio_cfg.gpio_pins = (1 << i);
        gpio_init(GPIOE, &gpio_cfg);
    }

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

    lcd_fill(0, 0, 320, 240, BG);

    /* ── IOMUX + FPGA pins ──────────────────────────────────── */
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    gpio_pin_remap_config(SPI3_GMUX_0010, TRUE);

    /* FPGA control: PC6 HIGH, PB11 HIGH */
    gpio_cfg.gpio_pins = GPIO_PINS_6;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOC, &gpio_cfg);
    GPIOC->scr = (1 << 6);

    gpio_cfg.gpio_pins = GPIO_PINS_11;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1 << 11);

    /* ── SPI3 Config ────────────────────────────────────────── */
    gpio_cfg.gpio_pins = GPIO_PINS_3;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_init(GPIOB, &gpio_cfg);

    gpio_cfg.gpio_pins = GPIO_PINS_4;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_cfg);

    gpio_cfg.gpio_pins = GPIO_PINS_5;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init(GPIOB, &gpio_cfg);

    gpio_cfg.gpio_pins = GPIO_PINS_6;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1 << 6);  /* CS HIGH */

    SPI3_I2SCTL &= ~(1 << 11);
    SPI3_CTL0 = 0;
    SPI3_CTL1 = 0;
    SPI3_CTL0 = (1 << 2) | (1 << 1) | (1 << 0) | (1 << 9) | (1 << 8);
    SPI3_CTL1 |= 0x03;
    SPI3_CTL0 |= (1 << 6);

    /* ── USART2: 9600 baud ──────────────────────────────────── */
    gpio_cfg.gpio_pins = GPIO_PINS_2;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init(GPIOA, &gpio_cfg);

    gpio_cfg.gpio_pins = GPIO_PINS_3;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_cfg);

    USART2->ctrl1 = 0;
    USART2->ctrl2 = 0;
    USART2->ctrl3 = 0;
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = (1 << 13) | (1 << 3) | (1 << 2);

    /* ── Passive button pins ────────────────────────────────── */
    pin_input_pullup(GPIOC, GPIO_PINS_8);
    pin_input_pullup(GPIOB, GPIO_PINS_7);
    pin_input_pullup(GPIOC, GPIO_PINS_13);

    /* ── Wait for FPGA heartbeat + send boot commands ────────── */
    delay_ms(300);

    /* Drain heartbeat */
    while (USART2->sts & (1 << 5)) (void)USART2->dt;

    /* Boot commands */
    uint8_t boot_cmds[] = {0x01, 0x02, 0x06, 0x07, 0x08};
    for (int c = 0; c < 5; c++) {
        uint8_t frame[10] = {0,0, 0,boot_cmds[c], 0,0,0,0,0, boot_cmds[c]};
        for (int i = 0; i < 10; i++) {
            while (!(USART2->sts & (1 << 7)));
            USART2->dt = frame[i];
        }
        while (!(USART2->sts & (1 << 6)));
        delay_ms(50);
        while (USART2->sts & (1 << 5)) (void)USART2->dt;
    }

    /* SPI3 handshake */
    delay_ms(100);
    GPIOB->scr = (1 << 6);  /* CS HIGH */
    spi3_xfer(0x00);         /* dummy flush */
    GPIOB->clr = (1 << 6);  /* CS LOW */
    spi3_xfer(0x05);
    spi3_xfer(0x00);
    GPIOB->scr = (1 << 6);  /* CS HIGH */
    spi3_xfer(0x00);
    spi3_xfer(0x00);
    delay_ms(100);

    /* ── Start TMR3 at 500Hz (direct register access) ────────── */
    /* TMR3 base = 0x40000400 */
    TMR3->ctrl1 = 0;             /* Stop timer, up-count */
    TMR3->div = 11999;           /* PSC = 11999 */
    TMR3->pr = 19;               /* ARR = 19 → 120MHz/(12000*20) = 500Hz */
    TMR3->cval = 0;              /* Reset counter */
    TMR3->ists = 0;              /* Clear all interrupt flags */
    TMR3->iden = (1 << 0);       /* UIE: Update interrupt enable */

    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
    nvic_irq_enable(TMR3_GLOBAL_IRQn, 5, 0);

    TMR3->ctrl1 |= (1 << 0);    /* CEN: Counter enable */

    /* ── Draw static UI ──────────────────────────────────────── */
    /* Title bar */
    lcd_hex8(5, 2, 0xB2, ORNG, BG, 2);  /* "B2" = v2 marker */

    /* Labels for live data */
    uint16_t ly = 20;
    /* Row 0: Raw scan result (15 bits) */
    /* Row 1: Phase 1 row state */
    /* Row 2: Confirmed button */
    /* Row 3: USART RX frame */
    /* Row 4: SPI3 probe */
    /* Row 5-8: Button bit map (which bit = which button) */

    /* ── Main loop — Display updates only ────────────────────── */
    uint16_t last_raw = 0xFFFF;
    uint8_t  last_confirmed = 0;
    uint32_t last_frame_count = 0;
    uint32_t spi_timer = 0;

    /* History of last 8 confirmed buttons */
    uint8_t btn_history[8] = {0};
    uint8_t btn_hist_idx = 0;

    while (1) {
        uint16_t raw = g_button_raw;

        /* ── Row 0: Raw scan bits ─────────────────────────────── */
        lcd_hex16(5, ly, raw, (raw != 0) ? GREEN : GRAY, BG, 2);

        /* Show individual bits as colored boxes */
        for (int i = 0; i < 15; i++) {
            uint16_t c = (raw & (1 << i)) ? GREEN : GRAY;
            if ((raw & (1 << i)) && !(last_raw & (1 << i)))
                c = RED;  /* newly set bit = red flash */
            lcd_fill(115 + i * 13, ly, 11, 14, c);
            lcd_hex8(116 + i * 13, ly + 3, (uint8_t)i, BG, c, 1);
        }

        /* ── Row 1: Phase 1 row + scan counter ────────────────── */
        lcd_hex8(5, ly + 18, g_phase1_row, CYAN, BG, 2);
        lcd_hex16(45, ly + 18, (uint16_t)(g_scan_count & 0xFFFF), GRAY, BG, 2);

        /* ── Row 2: Confirmed button ──────────────────────────── */
        uint8_t confirmed = g_button_confirmed;
        if (confirmed & 0x80) {
            uint8_t bit = confirmed & 0x7F;
            g_button_confirmed = 0;  /* consume */

            /* Add to history */
            btn_history[btn_hist_idx % 8] = bit;
            btn_hist_idx++;

            /* Flash the button number big */
            lcd_fill(5, ly + 36, 100, 28, BG);
            lcd_hex8(5, ly + 36, bit, RED, BG, 3);
            lcd_hex16(50, ly + 36, (1 << bit), YELLO, BG, 2);
        }

        /* Show history */
        for (int i = 0; i < 8; i++) {
            int idx = (btn_hist_idx - 8 + i);
            if (idx < 0) idx = 0;
            uint8_t b = btn_history[idx % 8];
            lcd_hex8(115 + i * 24, ly + 36, b,
                     (idx >= 0 && btn_hist_idx > 0) ? CYAN : GRAY, BG, 2);
        }

        /* ── Row 3: USART RX frame ────────────────────────────── */
        if (g_usart_rx_ready) {
            g_usart_rx_ready = 0;
            for (int i = 0; i < 10; i++) {
                uint16_t c = (g_usart_rx_buf[i] != 0) ? CYAN : GRAY;
                lcd_hex8(5 + i * 24, ly + 66, g_usart_rx_buf[i], c, BG, 2);
            }
        }
        lcd_hex16(250, ly + 66, (uint16_t)(g_frame_count & 0xFFFF), DKGRN, BG, 2);

        /* ── Row 4: Periodic SPI3 probe ───────────────────────── */
        spi_timer++;
        if (spi_timer % 100 == 0) {
            GPIOB->clr = (1 << 6);  /* CS LOW */
            uint8_t s0 = spi3_xfer(0xFF);
            uint8_t sdata[8];
            for (int i = 0; i < 8; i++)
                sdata[i] = spi3_xfer(0xFF);
            GPIOB->scr = (1 << 6);  /* CS HIGH */

            lcd_hex8(5, ly + 86, s0, (s0 != 0xFF) ? GREEN : GRAY, BG, 2);
            for (int i = 0; i < 8; i++) {
                uint16_t c = (sdata[i] != 0xFF) ? GREEN : GRAY;
                lcd_hex8(35 + i * 24, ly + 86, sdata[i], c, BG, 2);
            }
        }

        /* ── Row 5: Debounce counters (top 5 active) ──────────── */
        for (int i = 0; i < 15; i++) {
            uint8_t d = g_debounce[i];
            if (d > 0) {
                uint16_t c = (d >= 70) ? GREEN : YELLO;
                lcd_hex8(5 + i * 20, ly + 106, d, c, BG, 1);
            } else {
                lcd_fill(5 + i * 20, ly + 106, 12, 7, BG);
            }
        }

        last_raw = raw;
        delay_ms(30);
    }
}
