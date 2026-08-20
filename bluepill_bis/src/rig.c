/*
 * bluepill_bis — the config-transport bisect rig (BIS-1/BIS-2, dev_plan_2026-08-21.md §6)
 *
 * A Blue Pill (STM32F103C8) impersonating the 2C53T's MCU side of the Gowin
 * SSPI config interface, aimed at a Tang Nano 20K / 9K. Both transports are
 * ports of the PROJECT'S OWN bench-proven code, kept faithful on purpose:
 *
 *   hardware SPI  <- firmware/src/drivers/fpga.c fpga_spi3_config_sequence()
 *   bit-bang GPIO <- firmware/src/drivers/fpga.c fpga_bitbang_config_sequence()
 *                    (itself the Stlkv/maksidze 2C23T-V0.4 transplant that
 *                     broke the config wall on 2026-08-12)
 *
 * Fidelity choices that are load-bearing — do not "clean these up":
 *   - Pins are the scope's pins: SPI1 REMAPPED to PB3(SCK)/PB4(MISO)/PB5(MOSI)
 *     plus PB6 software CS. Same AFIO SWJ partial-disable the stock firmware
 *     uses (Exp E: stock MAPR = 0x02000000).
 *   - PB4/MISO defaults to input PULL-UP, matching stock (Exp E finding: our
 *     firmware's floating MISO was instrument bug #2). `m 0` floats it.
 *   - Each transport keeps its native read framing from fpga.c: hardware SPI
 *     reads op + 3 pad + 8 data (spi3_read_reg8); bit-bang reads op + 3 pad +
 *     4 data (bb_read_reg32). Unifying them would un-faithful the port.
 *   - Reads run at /256 (project law: SSPI reads are garbage at fast clock,
 *     fpga.c:1564). Command/upload BR are settable (`d`).
 *
 * Legs (single-letter commands over UART1 PA9/PA10 115200 8N1):
 *   i  IDCODE probe, hardware SPI, /256  (anchor first — project method)
 *   I  IDCODE probe, bit-bang
 *   a  entry attempt, hardware SPI, stock-faithful prelude (05/12/15)
 *   1  BIS-1: hardware SPI + V0.4 prelude reads 11/13/41 between 05 and 12
 *   2  BIS-2: hardware SPI, 0x05 ERASE_SRAM omitted
 *   b  entry attempt, bit-bang, V0.4 prelude (11/13/41 -> 12 -> 15, no 05)
 *   B  bit-bang WITH 05 inserted (the BIS-2 cross-check)
 *   p  toggle payload mode: next leg pauses after CONFIG_ENABLE, prints
 *      PAYLOAD, then expects 4-byte LE length + raw bitstream bytes on UART,
 *      streams them in one CS-LOW 0x3B frame, then closes (41, 11, 3A, 41).
 *   s  read STATUS(0x41) now, both framings
 *   e <hex8>  set expected IDCODE (default 0120681B; set per target board)
 *   d <cmd_br> <upload_br>  SPI BR dividers 0..7 (/2../256), default 7/7
 *   m <0|1>  MISO pull: 0 float, 1 pull-up (default)
 *   ?  help
 *
 * Output is KEY:VALUE lines (the project's overlay style) for host parsing.
 */

#include <stdint.h>

/* ── STM32F103 registers (only what the rig touches) ─────────────────── */
#define REG(a)          (*(volatile uint32_t *)(a))
#define RCC_CR          REG(0x40021000)
#define RCC_CFGR        REG(0x40021004)
#define RCC_APB2ENR     REG(0x40021018)
#define FLASH_ACR       REG(0x40022000)

#define GPIOA_CRH       REG(0x40010804)
#define GPIOB_CRL       REG(0x40010C00)
#define GPIOB_IDR       REG(0x40010C08)
#define GPIOB_ODR       REG(0x40010C0C)
#define GPIOB_BSRR      REG(0x40010C10)
#define GPIOB_BRR       REG(0x40010C14)
#define AFIO_MAPR       REG(0x40010004)

#define USART1_SR       REG(0x40013800)
#define USART1_DR       REG(0x40013804)
#define USART1_BRR      REG(0x40013808)
#define USART1_CR1      REG(0x4001380C)

#define SPI1_CR1        REG(0x40013000)
#define SPI1_SR         REG(0x40013008)
#define SPI1_DR         REG(0x4001300C)

#define SYST_CSR        REG(0xE000E010)
#define SYST_RVR        REG(0xE000E014)
#define SYST_CVR        REG(0xE000E018)

#define PIN_SCK   (1u << 3)   /* PB3 — scope's SPI3 pins, via SPI1 remap */
#define PIN_MISO  (1u << 4)   /* PB4 */
#define PIN_MOSI  (1u << 5)   /* PB5 */
#define PIN_CS    (1u << 6)   /* PB6 — software CS, as on the scope */

#define CS_LOW()   (GPIOB_BRR  = PIN_CS)
#define CS_HIGH()  (GPIOB_BSRR = PIN_CS)

/* ── state ───────────────────────────────────────────────────────────── */
static volatile uint32_t ms_ticks;
static uint32_t expected_idcode = 0x0120681Bu;  /* GW1N-2; `e` to change */
static uint8_t  cmd_br    = 7;                  /* /256 */
static uint8_t  upload_br = 7;
static uint8_t  miso_pullup = 1;                /* stock-faithful default */
static uint8_t  payload_armed;                  /* one-shot, set by `p` */

/* ── clock / systick ─────────────────────────────────────────────────── */
static void clock_init(void)
{
    /* 8 MHz HSE x9 = 72 MHz. APB1 /2 (36 MHz), APB2 /1 (72 MHz). */
    RCC_CR |= (1u << 16);                        /* HSEON */
    while (!(RCC_CR & (1u << 17))) {}            /* HSERDY */
    FLASH_ACR = (FLASH_ACR & ~7u) | 2u | (1u << 4);  /* 2 WS + prefetch */
    RCC_CFGR = (7u << 18) | (1u << 16)           /* PLLMUL=x9, PLLSRC=HSE */
             | (4u << 8);                        /* PPRE1 = /2 */
    RCC_CR |= (1u << 24);                        /* PLLON */
    while (!(RCC_CR & (1u << 25))) {}            /* PLLRDY */
    RCC_CFGR |= 2u;                              /* SW = PLL */
    while ((RCC_CFGR & (3u << 2)) != (2u << 2)) {}

    SYST_RVR = 72000u - 1u;                      /* 1 ms tick */
    SYST_CVR = 0;
    SYST_CSR = 7u;                               /* enable, tick int, core clk */
}

static void delay_ms(uint32_t n)
{
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < n) {}
}

/* ── uart ────────────────────────────────────────────────────────────── */
static void uart_init(void)
{
    /* PA9 TX AF-PP 50MHz, PA10 RX floating input */
    GPIOA_CRH = (GPIOA_CRH & ~0x00000FF0u) | 0x000004B0u;
    USART1_BRR = 625u;                           /* 72 MHz / 115200 */
    USART1_CR1 = (1u << 13) | (1u << 3) | (1u << 2);  /* UE TE RE */
}

static void uart_putc(char c)
{
    while (!(USART1_SR & (1u << 7))) {}          /* TXE */
    USART1_DR = (uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_hex8(uint8_t v)
{
    const char *h = "0123456789ABCDEF";
    uart_putc(h[v >> 4]); uart_putc(h[v & 0xF]);
}

static void uart_hex32(uint32_t v)
{
    for (int i = 3; i >= 0; --i) uart_hex8((uint8_t)(v >> (8 * i)));
}

static int uart_getc_timeout(uint32_t timeout_ms)
{
    uint32_t start = ms_ticks;
    while (!(USART1_SR & (1u << 5))) {           /* RXNE */
        if (timeout_ms && (ms_ticks - start) >= timeout_ms) return -1;
    }
    return (int)(USART1_DR & 0xFF);
}

/* ── pin muxing between the two transports ───────────────────────────── */
static void pins_common(void)
{
    /* CS: GPIO out PP 50MHz, idle HIGH. MISO per knob: pull-up (stock,
     * Exp E) or floating (our firmware's historic bug, kept as a control). */
    CS_HIGH();
    GPIOB_CRL = (GPIOB_CRL & ~0x0F000000u) | 0x03000000u;      /* PB6 */
    if (miso_pullup) {
        GPIOB_CRL = (GPIOB_CRL & ~0x000F0000u) | 0x00080000u;  /* PB4 IPU */
        GPIOB_ODR |= PIN_MISO;
    } else {
        GPIOB_CRL = (GPIOB_CRL & ~0x000F0000u) | 0x00040000u;  /* PB4 float */
    }
}

static void pins_hw_spi(void)
{
    pins_common();
    /* PB3 SCK AF-PP, PB5 MOSI AF-PP */
    GPIOB_CRL = (GPIOB_CRL & ~0x00F0F000u) | 0x00B0B000u;
}

static void pins_bitbang(void)
{
    pins_common();
    /* Pre-load mode-3 idle FIRST (CLK HIGH, MOSI LOW), then switch to GPIO —
     * same glitch-avoidance order as fpga_bitbang_config_sequence(). */
    GPIOB_BSRR = PIN_SCK;
    GPIOB_BRR  = PIN_MOSI;
    GPIOB_CRL = (GPIOB_CRL & ~0x00F0F000u) | 0x00303000u;
}

/* ── hardware SPI transport (port of fpga.c spi3_*) ──────────────────── */
static void spi_set_br(uint8_t br)
{
    /* Mode 3, master, MSB-first, 8-bit, SSM/SSI, BR[5:3] */
    SPI1_CR1 = 0;
    SPI1_CR1 = (1u << 9) | (1u << 8) | (1u << 2) | (1u << 1) | (1u << 0)
             | ((uint32_t)(br & 7u) << 3);
    SPI1_CR1 |= (1u << 6);                       /* SPE */
}

static uint8_t spi_xfer(uint8_t tx)
{
    while (!(SPI1_SR & (1u << 1))) {}            /* TXE */
    SPI1_DR = tx;
    while (!(SPI1_SR & (1u << 0))) {}            /* RXNE */
    return (uint8_t)SPI1_DR;
}

/* fpga.c spi3_read_reg8: bare clock CS-HIGH, then op + 3 pad + 8 data. */
static void hw_read_reg8(uint8_t opcode, uint8_t *out8)
{
    uint8_t save = (uint8_t)((SPI1_CR1 >> 3) & 7u);
    spi_set_br(7);                               /* reads only valid slow */
    spi_xfer(0x00);
    CS_LOW();
    spi_xfer(opcode);
    spi_xfer(0x00); spi_xfer(0x00); spi_xfer(0x00);
    for (unsigned i = 0; i < 8; i++) out8[i] = spi_xfer(0x00);
    CS_HIGH();
    spi_set_br(save);
}

static void hw_cmd16(uint8_t hi, uint8_t lo)
{
    CS_LOW();
    spi_xfer(hi);
    spi_xfer(lo);
    CS_HIGH();
}

/* ── bit-bang transport (port of fpga.c bb_*) ────────────────────────── */
static uint8_t bb_xfer(uint8_t value)
{
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8u; ++i) {
        GPIOB_BRR = PIN_SCK;
        if (value & 0x80u) GPIOB_BSRR = PIN_MOSI;
        else               GPIOB_BRR  = PIN_MOSI;
        GPIOB_BSRR = PIN_SCK;
        result = (uint8_t)(result << 1);
        value  = (uint8_t)(value << 1);
        if (GPIOB_IDR & PIN_MISO) result |= 1u;
    }
    return result;
}

/* fpga.c bb_read_reg32: dummy byte CS-HIGH, then op + 3 pad + 4 data. */
static void bb_read_reg32(uint8_t opcode, uint8_t *out4)
{
    (void)bb_xfer(0);
    CS_LOW();
    (void)bb_xfer(opcode);
    (void)bb_xfer(0); (void)bb_xfer(0); (void)bb_xfer(0);
    for (uint8_t i = 0; i < 4u; ++i) out4[i] = bb_xfer(0);
    CS_HIGH();
}

static void bb_cmd16(uint8_t hi, uint8_t lo)
{
    (void)bb_xfer(0);
    CS_LOW();
    (void)bb_xfer(hi);
    (void)bb_xfer(lo);
    CS_HIGH();
}

/* ── IDCODE alignment search (port of fpga.c spi3_find_idcode) ───────── */
static int8_t find_idcode(const uint8_t *b, unsigned n)
{
    if (n < 4) return -1;
    uint64_t w = 0;
    for (unsigned i = 0; i < 8; i++)
        w = (w << 8) | (i < n ? b[i] : 0);
    /* 8-byte replies: slide all 33 alignments (phase-shift artifacts are this
     * project's signature failure — see fpga.c spi3_find_idcode). 4-byte
     * bit-bang replies only carry the exact alignment. */
    unsigned max_off = (n >= 8) ? 33u : 1u;
    for (unsigned off = 0; off < max_off; off++) {
        if ((uint32_t)(w >> (32u - off)) == expected_idcode)
            return (int8_t)off;
    }
    return -1;
}

/* ── payload streaming (host: 4-byte LE length, then raw bytes) ──────── */
static void stream_payload(uint8_t bb)
{
    uart_puts("PAYLOAD:SEND\r\n");
    uint32_t len = 0;
    for (unsigned i = 0; i < 4; i++) {
        int c = uart_getc_timeout(10000);
        if (c < 0) { uart_puts("PAYLOAD:TIMEOUT\r\n"); return; }
        len |= ((uint32_t)c) << (8 * i);
    }
    if (!bb) spi_set_br(upload_br);
    if (bb) { (void)bb_xfer(0); }
    CS_LOW();
    if (bb) (void)bb_xfer(0x3B); else (void)spi_xfer(0x3B);
    uint32_t got = 0;
    while (got < len) {
        int c = uart_getc_timeout(5000);
        if (c < 0) break;
        if (bb) (void)bb_xfer((uint8_t)c); else (void)spi_xfer((uint8_t)c);
        got++;
    }
    CS_HIGH();
    if (!bb) spi_set_br(cmd_br);
    uart_puts("PAYLOAD:"); uart_hex32(got);
    uart_puts(got == len ? " OK\r\n" : " SHORT\r\n");
}

/* ── result printing ─────────────────────────────────────────────────── */
static void print_status32(const char *key, const uint8_t *b4)
{
    uint32_t st = ((uint32_t)b4[0] << 24) | ((uint32_t)b4[1] << 16)
                | ((uint32_t)b4[2] << 8)  |  (uint32_t)b4[3];
    uart_puts(key); uart_putc(':'); uart_hex32(st);
    uart_puts(" EDIT:"); uart_putc((st & (1u << 7)) ? '1' : '0');
    uart_puts(" DONE:"); uart_putc((st & (1u << 13)) ? '1' : '0');
    uart_puts("\r\n");
}

/* ── the legs ────────────────────────────────────────────────────────── */

/* Hardware-SPI entry (port of fpga_spi3_config_sequence, split-frame mode,
 * default knobs). with_05 / with_reads select base / BIS-1 / BIS-2. */
static void leg_hw(uint8_t with_05, uint8_t with_reads)
{
    uint8_t buf8[8], st[8];
    pins_hw_spi();
    spi_set_br(cmd_br);

    /* [0] bare CS pulse, zero clocks (stock t=3.6082), then ~100 ms */
    CS_HIGH();
    (void)SPI1_DR;
    CS_LOW();
    for (volatile int d = 0; d < 50; d++) { __asm__ volatile("nop"); }
    CS_HIGH();
    delay_ms(100);

    if (with_05) { hw_cmd16(0x05, 0x00); delay_ms(100); }      /* ERASE_SRAM */

    if (with_reads) {                            /* BIS-1: V0.4 reads here */
        hw_read_reg8(0x11, buf8);
        uart_puts("PRE11:");
        for (unsigned i = 0; i < 8; i++) uart_hex8(buf8[i]);
        uart_puts(" OFF:");
        int8_t off = find_idcode(buf8, 8);
        if (off < 0) uart_puts("-1"); else { uart_putc('0' + off / 10); uart_putc('0' + off % 10); }
        uart_puts("\r\n");
        hw_read_reg8(0x13, buf8);
        hw_read_reg8(0x41, st);
        print_status32("PRE41", st);
    }

    hw_cmd16(0x12, 0x00); delay_ms(100);         /* INIT_ADDR */
    hw_cmd16(0x15, 0x00);                        /* CONFIG_ENABLE */

    if (payload_armed) { stream_payload(0); payload_armed = 0; }

    /* the wall test: STATUS at /256 right after 0x15 (probe_edit) */
    hw_read_reg8(0x41, st);
    print_status32("ST15", st);
    hw_read_reg8(0x11, buf8);                    /* Exp L/M open-port check */
    uart_puts("ID15:");
    for (unsigned i = 0; i < 8; i++) uart_hex8(buf8[i]);
    uart_puts("\r\n");

    hw_cmd16(0x3A, 0x00);                        /* CONFIG_DISABLE */
    delay_ms(100);
    hw_read_reg8(0x41, st);
    print_status32("ST3A", st);
}

/* Bit-bang entry (port of fpga_bitbang_config_sequence V0.4 branch).
 * with_05 inserts ERASE_SRAM = the BIS-2 cross-check leg. */
static void leg_bb(uint8_t with_05)
{
    uint8_t b4[4];
    pins_bitbang();

    if (with_05) { bb_cmd16(0x05, 0x00); delay_ms(100); }

    bb_read_reg32(0x11, b4);                     /* V0.4 prelude reads */
    uart_puts("PRE11:");
    for (unsigned i = 0; i < 4; i++) uart_hex8(b4[i]);
    uart_puts("\r\n");
    bb_read_reg32(0x13, b4);
    bb_read_reg32(0x41, b4);
    print_status32("PRE41", b4);

    bb_cmd16(0x12, 0x00);                        /* INIT_ADDR */
    bb_cmd16(0x15, 0x00);                        /* CONFIG_ENABLE */

    if (payload_armed) { stream_payload(1); payload_armed = 0; }

    bb_read_reg32(0x41, b4);
    print_status32("ST15", b4);
    bb_read_reg32(0x11, b4);
    uart_puts("ID15:");
    for (unsigned i = 0; i < 4; i++) uart_hex8(b4[i]);
    uart_puts("\r\n");

    bb_cmd16(0x3A, 0x00);
    delay_ms(100);
    bb_read_reg32(0x41, b4);
    print_status32("ST3A", b4);
}

static void leg_idcode(uint8_t bb)
{
    uint8_t b[8];
    if (bb) {
        pins_bitbang();
        bb_read_reg32(0x11, b);
        uart_puts("ID:");
        for (unsigned i = 0; i < 4; i++) uart_hex8(b[i]);
        uart_puts(" OFF:");
        uart_puts(find_idcode(b, 4) == 0 ? "0" : "-1");
        bb_read_reg32(0x41, b);
        uart_puts("\r\n");
        print_status32("ST", b);
    } else {
        pins_hw_spi();
        spi_set_br(cmd_br);
        hw_read_reg8(0x11, b);
        uart_puts("ID:");
        for (unsigned i = 0; i < 8; i++) uart_hex8(b[i]);
        int8_t off = find_idcode(b, 8);
        uart_puts(" OFF:");
        if (off < 0) uart_puts("-1");
        else { uart_putc('0' + off / 10); uart_putc('0' + off % 10); }
        uart_puts("\r\n");
        hw_read_reg8(0x41, b);
        print_status32("ST", b);
    }
}

/* ── tiny line reader / number parsing ───────────────────────────────── */
static uint32_t parse_hex(const char *s)
{
    uint32_t v = 0;
    while (*s == ' ') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9') v = (v << 4) | (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v = (v << 4) | (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = (v << 4) | (uint32_t)(c - 'A' + 10);
        else break;
    }
    return v;
}

static void help(void)
{
    uart_puts("bluepill_bis rig — legs: i/I idcode(hw/bb) a base 1 BIS-1 "
              "2 BIS-2 b bb-V0.4 B bb+05 | p arm-payload s status "
              "e <idcode> d <cmd> <upl> m <0|1> ?\r\n");
}

int main(void)
{
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3)   /* AFIO IOPA IOPB */
                 | (1u << 12) | (1u << 14);            /* SPI1 USART1 */
    clock_init();
    /* Free PB3/PB4 (SWJ_CFG=010, JTAG off SWD on — stock's exact posture,
     * Exp E: MAPR=0x02000000) and remap SPI1 onto PB3/4/5. */
    AFIO_MAPR = (AFIO_MAPR & ~(7u << 24)) | (2u << 24) | 1u;
    uart_init();
    pins_hw_spi();
    spi_set_br(cmd_br);

    uart_puts("\r\nRIG:bluepill_bis BUILD:" __DATE__ " " __TIME__ "\r\n");
    help();

    char line[48];
    unsigned n = 0;
    for (;;) {
        int c = uart_getc_timeout(0);
        if (c < 0) continue;
        if (c == '\r' || c == '\n') {
            line[n] = 0;
            if (n) {
                uart_puts("\r\n");
                switch (line[0]) {
                case 'i': leg_idcode(0); break;
                case 'I': leg_idcode(1); break;
                case 'a': uart_puts("LEG:a HW 05:1 READS:0\r\n"); leg_hw(1, 0); break;
                case '1': uart_puts("LEG:1 HW 05:1 READS:1\r\n"); leg_hw(1, 1); break;
                case '2': uart_puts("LEG:2 HW 05:0 READS:0\r\n"); leg_hw(0, 0); break;
                case 'b': uart_puts("LEG:b BB 05:0 READS:1\r\n"); leg_bb(0); break;
                case 'B': uart_puts("LEG:B BB 05:1 READS:1\r\n"); leg_bb(1); break;
                case 'p': payload_armed = 1; uart_puts("PAYLOAD:ARMED\r\n"); break;
                case 's': leg_idcode(0); break;
                case 'e': expected_idcode = parse_hex(line + 1);
                          uart_puts("IDEXP:"); uart_hex32(expected_idcode);
                          uart_puts("\r\n"); break;
                case 'd': {
                    const char *s = line + 1;
                    while (*s == ' ') s++;
                    cmd_br = (uint8_t)(*s ? (*s - '0') & 7 : 7);
                    while (*s && *s != ' ') s++;
                    while (*s == ' ') s++;
                    upload_br = (uint8_t)(*s ? (*s - '0') & 7 : cmd_br);
                    uart_puts("BR:"); uart_putc('0' + cmd_br);
                    uart_putc('/'); uart_putc('0' + upload_br);
                    uart_puts("\r\n"); break;
                }
                case 'm': miso_pullup = (line[1] == ' ' ? line[2] : line[1]) != '0';
                          uart_puts(miso_pullup ? "MISO:PULLUP\r\n" : "MISO:FLOAT\r\n");
                          break;
                default: help(); break;
                }
                uart_puts("OK>\r\n");
            }
            n = 0;
        } else if (n < sizeof line - 1) {
            line[n++] = (char)c;
            uart_putc((char)c);                  /* echo */
        }
    }
}

/* ── startup ─────────────────────────────────────────────────────────── */
extern uint32_t _etext, _sdata, _edata, _sbss, _ebss, _estack;

void Reset_Handler(void)
{
    uint32_t *src = &_etext, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    main();
    for (;;) {}
}

void SysTick_Handler(void) { ms_ticks++; }
static void Default_Handler(void) { for (;;) {} }

__attribute__((section(".isr_vector")))
const void *vectors[] = {
    &_estack, Reset_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, 0, 0, 0, 0,
    Default_Handler, Default_Handler, 0, Default_Handler,
    SysTick_Handler,
};
