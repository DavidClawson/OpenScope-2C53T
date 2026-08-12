/*
 * OpenScope 2C53T - Scope Trigger Comparator DAC
 * See scope_trigger.h for the stock-equivalence rationale.
 */

#include "scope_trigger.h"
#include "at32f403a_407.h"

/* DAC peripheral registers (AT32F403A, base 0x40007400 — STM32F1-compatible).
 * Stock writes these exact addresses in FUN_080018a4. */
#define DAC_CTRL     (*(volatile uint32_t *)0x40007400)  /* control */
#define DAC_SWTRG    (*(volatile uint32_t *)0x40007404)  /* software trigger */
#define DAC_DHR12R1  (*(volatile uint32_t *)0x40007408)  /* CH1 12-bit right-aligned */

/* DAC_CTRL bit fields (channel 1) */
#define DAC_D1EN     (1u << 0)   /* DAC1 enable */
#define DAC_D1BOFF   (1u << 1)   /* CH1 output buffer DISABLE — stock keeps it
                                    CLEAR (buffer ON); do not set (see init) */
#define DAC_D1TEN    (1u << 2)   /* CH1 trigger enable */
#define DAC_D1TSEL_SW (7u << 3)  /* CH1 trigger source = software (0b111) */

/* Number of voltage ranges (stock switch in FUN_080018a4 spans cases 0..9). */
#define SCOPE_TRIG_RANGES 10

/*
 * PLACEHOLDER per-range cal table (factory cal unrecoverable — see header).
 * Full-scale linear: base=0, upper=4095 for every range, so
 *   level -100 -> 0,  level 0 -> ~2047 (mid),  level +100 -> 4095.
 * This makes the formula trivially verifiable on a scope and is replaced once a
 * real per-range calibration is regenerated.
 */
static const uint16_t cal_base[SCOPE_TRIG_RANGES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const uint16_t cal_upper[SCOPE_TRIG_RANGES] = {
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
};

/* Stock divisor constant (float @0x08001a54 in APP_2C53T_V1.2.0 = 200.0f). */
#define TRIG_DIVISOR 200.0f

static volatile uint16_t last_code = 0;
static volatile uint8_t  inited = 0;

void scope_trigger_dac_init(void)
{
    /* DAC + GPIOA clocks (idempotent with dac_output_init). */
    crm_periph_clock_enable(CRM_DAC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

    /* PA4 = DAC1 output, analog mode. */
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_4;
    gpio_cfg.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init(GPIOA, &gpio_cfg);

    /* CH1 control — stock's exact bring-up order (master_init +0x17F2,
     * 0x0802C23C-0x0802C26A, decoded by the ripcord session 2026-08-12):
     * TRSEL=software first, then TREN; wave-gen off, DMA off, and the output
     * buffer ENABLED (stock bic #0x02 — the previous D1BOFF here was a
     * faithfulness bug; stock's net CH1 CTRL is 0x3D, buffered). D1EN is set
     * LAST, in its own write, so the first software trigger cannot latch
     * against an unconfigured trigger select.
     * Preserve channel-2 bits (siggen / CH2 path may use them). */
    uint32_t ctrl = DAC_CTRL & 0xFFFF0000u;
    ctrl |= DAC_D1TSEL_SW | DAC_D1TEN;   /* trigger config, D1EN still clear */
    DAC_CTRL = ctrl;
    DAC_CTRL = ctrl | DAC_D1EN;          /* enable last (stock order) — 0x3D */

    inited = 1;
}

uint16_t scope_trigger_dac_compute(int range, int level)
{
    if (range < 0 || range >= SCOPE_TRIG_RANGES) range = 0;
    if (level < -100) level = -100;
    if (level >  100) level =  100;

    float upper = (float)cal_upper[range];
    float base  = (float)cal_base[range];
    /* Stock: ((upper - base) / 200.0) * (level + 100) + base */
    float dac = ((upper - base) / TRIG_DIVISOR) * (float)(level + 100) + base;

    if (dac < 0.0f)    dac = 0.0f;
    if (dac > 4095.0f) dac = 4095.0f;
    return (uint16_t)dac & 0x0FFF;
}

void scope_trigger_dac_raw(uint16_t code)
{
    /* Re-init if the CH1 control bits are not ours — `inited` alone is a
     * one-shot latch, so if anything (dac_output_start/stop, siggen) has
     * re-owned DAC1 since, a bare DHR write would land on a channel that is
     * no longer software-triggered/enabled. Checking the register makes
     * every caller (incl. the warmtest timeout re-arm) self-healing. */
    if (!inited || (DAC_CTRL & 0x0000FFFFu) !=
                   (DAC_D1TSEL_SW | DAC_D1TEN | DAC_D1EN))
        scope_trigger_dac_init();
    code &= 0x0FFF;
    /* Faithful to stock: preserve upper bits of DHR12R1, then software-trigger. */
    DAC_DHR12R1 = (DAC_DHR12R1 & 0xFFFFF000u) | code;
    DAC_SWTRG |= 1u;
    last_code = code;
}

void scope_trigger_dac_set(int range, int level)
{
    scope_trigger_dac_raw(scope_trigger_dac_compute(range, level));
}

uint16_t scope_trigger_dac_last(void)
{
    return last_code;
}
