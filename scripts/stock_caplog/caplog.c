/* caplog -- a logger that rides inside stock V1.2.0.
 *
 * Built by scripts/stock_caplog/build_caplog_image.py into a code cave appended
 * to the user's own stock APP image (0x080BE700, the same 2 KB flash page the
 * image ends in) and entered from two six-byte hooks:
 *   caplog_tx()   from the dvom TX task right after a frame to the meter SoC is
 *                 complete (0x0803E44C, before vTaskDelay(10));
 *   caplog_tick() from the FreeRTOS SysTick handler (0x0802A994), every tick.
 *
 * Everything it records sits in a fixed SRAM block at 0x20037000..0x20037700 --
 * above stock's stack top (0x20036F90), above this firmware's RAM end and below
 * the factory IAP bootloader's RAM magic (0x20037FE0; the IAP's own RAM ends at
 * 0x20001A08). MENU+Power keeps the power rail up, so the block survives
 * stock -> IAP -> OpenScope, and `mem read 20037000 64` reads it back.
 * Measured on unit #2 (issue #15): three round trips, magic intact each time.
 *
 * Records are 16 bytes:
 *   type 1  TX frame   : tick u32, 1, len=10, frame[10]
 *   type 2  ODR change : tick u32, 2, port, odr_new u16, odr_old u16, pad[6]
 *   type 3  CFG change : tick u32, 3, port, pad u16, cfglr u32, cfghr u32
 * The TX ring keeps the last 32 frames, the event ring the last 64 GPIO
 * changes (both wrap; the counters tell how many were written in total), and
 * tog[] counts ODR toggles per pin so a noisy pin is visible even after the
 * ring has wrapped.
 *
 * No .data/.bss, no libc, integer registers only -- the hooks must not disturb
 * the task or the handler they interrupt beyond r0-r3/r12. The linker script
 * asserts the first two, -mgeneral-regs-only enforces the third.
 */
#include <stdint.h>

#define CAPLOG_BASE   0x20037000u
#define CAPLOG_MAGIC  0x4C504143u   /* "CAPL" */
#define CAPLOG_SIZE   0x700u

#define TX_BUF        ((const volatile uint8_t *)0x20000005u)  /* stock dvom TX frame */
#define GPIO_BASE(p)  (0x40010800u + 0x400u * (p))              /* A..E */
#define GPIO_CFGLR(p) (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00u))
#define GPIO_CFGHR(p) (*(volatile uint32_t *)(GPIO_BASE(p) + 0x04u))
#define GPIO_ODR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0Cu))

/* ODR bits to ignore per port (A..E) when a pin turns out to toggle every
 * tick and floods the ring. Round 1: nothing ignored. */
static const uint16_t caplog_ignore[5] = { 0, 0, 0, 0, 0 };

typedef struct {
    uint32_t magic;        /* 0x000 */
    uint32_t ticks;        /* 0x004 */
    uint32_t tx_count;     /* 0x008 */
    uint32_t gpio_count;   /* 0x00C */
    uint16_t odr[5];       /* 0x010 */
    uint16_t pad0;         /* 0x01A */
    uint32_t cfg[10];      /* 0x01C  CFGLR/CFGHR pairs, A..E */
    uint32_t rsvd[7];      /* 0x044 */
    uint8_t  tx[32][16];   /* 0x060 */
    uint8_t  ev[64][16];   /* 0x260 */
    uint16_t tog[80];      /* 0x660  port*16 + pin */
} caplog_t;                /* 0x700 */

typedef char caplog_size_check[(sizeof(caplog_t) == CAPLOG_SIZE) ? 1 : -1];

#define L ((volatile caplog_t *)CAPLOG_BASE)

static void caplog_init(void) {
    volatile uint32_t *p = (volatile uint32_t *)CAPLOG_BASE;
    uint32_t i;
    for (i = 0; i < CAPLOG_SIZE / 4u; ++i) {
        p[i] = 0u;
    }
    for (i = 0; i < 5u; ++i) {
        L->odr[i] = (uint16_t)GPIO_ODR(i);
        L->cfg[2u * i] = GPIO_CFGLR(i);
        L->cfg[2u * i + 1u] = GPIO_CFGHR(i);
    }
    L->magic = CAPLOG_MAGIC;
}

static void caplog_put32(volatile uint8_t *r, uint32_t v) {
    r[0] = (uint8_t)v;
    r[1] = (uint8_t)(v >> 8);
    r[2] = (uint8_t)(v >> 16);
    r[3] = (uint8_t)(v >> 24);
}

void caplog_tx(void) {
    volatile uint8_t *r;
    uint32_t i;
    if (L->magic != CAPLOG_MAGIC) {
        caplog_init();
    }
    r = L->tx[L->tx_count & 31u];
    caplog_put32(r, L->ticks);
    r[4] = 1u;
    r[5] = 10u;
    for (i = 0; i < 10u; ++i) {
        r[6u + i] = TX_BUF[i];
    }
    L->tx_count = L->tx_count + 1u;
}

static volatile uint8_t *caplog_ev(uint8_t type, uint32_t port) {
    volatile uint8_t *r = L->ev[L->gpio_count & 63u];
    uint32_t i;
    L->gpio_count = L->gpio_count + 1u;
    caplog_put32(r, L->ticks);
    r[4] = type;
    r[5] = (uint8_t)port;
    for (i = 6u; i < 16u; ++i) {
        r[i] = 0u;
    }
    return r;
}

void caplog_tick(void) {
    uint32_t p;
    if (L->magic != CAPLOG_MAGIC) {
        caplog_init();
    }
    L->ticks = L->ticks + 1u;
    for (p = 0; p < 5u; ++p) {
        uint32_t odr = GPIO_ODR(p) & 0xFFFFu & (uint32_t)~caplog_ignore[p];
        uint32_t old = L->odr[p] & (uint32_t)~caplog_ignore[p];
        uint32_t cfgl = GPIO_CFGLR(p);
        uint32_t cfgh = GPIO_CFGHR(p);
        if (odr != old) {
            uint32_t diff = odr ^ old, b;
            volatile uint8_t *r = caplog_ev(2u, p);
            r[6] = (uint8_t)odr;
            r[7] = (uint8_t)(odr >> 8);
            r[8] = (uint8_t)old;
            r[9] = (uint8_t)(old >> 8);
            for (b = 0; b < 16u; ++b) {
                if (diff & (1u << b)) {
                    uint16_t c = L->tog[p * 16u + b];
                    if (c != 0xFFFFu) {
                        L->tog[p * 16u + b] = (uint16_t)(c + 1u);
                    }
                }
            }
            L->odr[p] = (uint16_t)odr;
        }
        if (cfgl != L->cfg[2u * p] || cfgh != L->cfg[2u * p + 1u]) {
            volatile uint8_t *r = caplog_ev(3u, p);
            caplog_put32(r + 8, cfgl);
            caplog_put32(r + 12, cfgh);
            L->cfg[2u * p] = cfgl;
            L->cfg[2u * p + 1u] = cfgh;
        }
    }
}
