/*
 * OpenScope 2C53T - DAC Output Driver
 *
 * Continuous waveform output via DAC1 (PA4) using TMR6 + DMA2 CH3.
 *
 * Signal path:
 *   TMR6 overflow → TRGO → triggers DAC1 conversion
 *   DMA2 CH3 transfers next sample from circular buffer to DAC1 data register
 *   DAC1 outputs analog voltage on PA4 (0-3.3V, 12-bit resolution)
 *
 * Sample rate is controlled by TMR6 period:
 *   sample_rate = APB1_clock / (prescaler × (period + 1))
 *   APB1 = 120 MHz (HCLK/2 at 240 MHz system clock)
 */

#include "dac_output.h"
#include "at32f403a_407.h"

/* TMR6 registers (basic timer — no GPIO, no capture/compare) */
#define TMR6   ((tmr_type *)TMR6_BASE)

/* DMA2 Channel 3 — maps to DAC1 on AT32F403A */
#define DMA2_CH3  ((dma_channel_type *)DMA2_CHANNEL3_BASE)
#define DMA2_REG  ((dma_type *)DMA2_BASE)

/* APB1 clock = system_core_clock / 2 */
#define APB1_CLOCK  (system_core_clock / 2)

/* Waveform buffer in SRAM (DMA source) */
static uint16_t dac_buffer[DAC_BUFFER_SIZE];
static volatile bool running = false;

/* ═══════════════════════════════════════════════════════════════════
 * Timer configuration
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Configure TMR6 period for desired sample rate.
 * Uses prescaler=1 for rates ≥ 1831 Hz (120MHz / 65536).
 * Uses prescaler=8 for slower rates down to ~229 Hz.
 * Uses prescaler=64 for rates down to ~29 Hz.
 */
static void tmr6_set_rate(uint32_t sample_rate)
{
    uint16_t prescaler = 0;  /* div by (prescaler+1) */
    uint32_t period;

    if (sample_rate == 0) sample_rate = 1;

    /* Try prescaler=1 first */
    period = APB1_CLOCK / sample_rate;
    if (period > 65536) {
        /* Need prescaler */
        prescaler = 7;  /* div by 8 */
        period = APB1_CLOCK / (8 * sample_rate);
    }
    if (period > 65536) {
        prescaler = 63;  /* div by 64 */
        period = APB1_CLOCK / (64 * sample_rate);
    }
    if (period < 1) period = 1;
    if (period > 65536) period = 65536;

    TMR6->div = prescaler;
    TMR6->pr = (uint16_t)(period - 1);

    /* Force update so new values take effect immediately */
    TMR6->swevt_bit.ovfswtr = 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void dac_output_init(void)
{
    /* Enable peripheral clocks */
    crm_periph_clock_enable(CRM_DMA2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DAC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_TMR6_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

    /* PA4 = DAC1 output: analog mode (MODE=00, CNF=00) */
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_4;
    gpio_cfg.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init(GPIOA, &gpio_cfg);

    /* ── DAC1 configuration ── */

    /* Disable output buffer (lower impedance, direct analog out) */
    dac_output_buffer_enable(DAC1_SELECT, FALSE);

    /* Trigger source = TMR6 TRGO */
    dac_trigger_select(DAC1_SELECT, DAC_TMR6_TRGOUT_EVENT);
    dac_trigger_enable(DAC1_SELECT, TRUE);

    /* Enable DMA for DAC1 */
    dac_dma_enable(DAC1_SELECT, TRUE);

    /* DAC1 starts DISABLED — only enabled by dac_output_start().
     * This prevents PA4 from outputting voltage when not in siggen mode. */
    dac_enable(DAC1_SELECT, FALSE);

    /* ── DMA2 Channel 3 configuration ── */

    /* Reset DMA channel */
    DMA2_CH3->ctrl = 0;

    /* Source: dac_buffer in memory (16-bit, increment) */
    /* Dest: DAC1 12-bit right-aligned data register (16-bit, fixed) */
    DMA2_CH3->dtcnt = DAC_BUFFER_SIZE;
    DMA2_CH3->paddr = (uint32_t)&DAC->d1dth12r;
    DMA2_CH3->maddr = (uint32_t)dac_buffer;

    DMA2_CH3->ctrl =
        (1 << 4)  |  /* DIR: memory → peripheral */
        (1 << 5)  |  /* LM: circular mode */
        (0 << 6)  |  /* PINCM: peripheral address fixed */
        (1 << 7)  |  /* MINCM: memory address increment */
        (1 << 8)  |  /* PWIDTH: 16-bit peripheral */
        (0 << 9)  |  /* (PWIDTH continued) */
        (1 << 10) |  /* MWIDTH: 16-bit memory */
        (0 << 11) |  /* (MWIDTH continued) */
        (2 << 12) |  /* CHPL: high priority */
        (0 << 14);   /* M2M: not memory-to-memory */

    /* ── TMR6 configuration (basic timer for DAC trigger) ── */

    /* Default: 1kHz output = 256kHz sample rate */
    TMR6->ctrl1 = 0;
    TMR6->div = 0;                     /* prescaler = 1 */
    TMR6->pr = (APB1_CLOCK / 256000) - 1;  /* ~468 for 256kHz */

    /* Master mode: TRGO on update event (overflow) */
    TMR6->ctrl2_bit.ptos = 2;  /* TRGO = update event */

    /* Fill buffer with mid-scale initially */
    for (int i = 0; i < DAC_BUFFER_SIZE; i++) {
        dac_buffer[i] = 2048;
    }
}

void dac_output_start(uint32_t sample_rate)
{
    /* Stop first if already running */
    if (running) dac_output_stop();

    /* Configure timer for requested sample rate */
    tmr6_set_rate(sample_rate);

    /* Reset DMA transfer count */
    DMA2_CH3->ctrl_bit.chen = 0;
    DMA2_CH3->dtcnt = DAC_BUFFER_SIZE;
    DMA2_CH3->maddr = (uint32_t)dac_buffer;

    /* Enable DAC1 (was disabled at init) */
    dac_enable(DAC1_SELECT, TRUE);

    /* Enable DMA channel */
    DMA2_CH3->ctrl_bit.chen = 1;

    /* Start TMR6 */
    TMR6->ctrl1_bit.tmren = 1;

    running = true;
}

void dac_output_stop(void)
{
    /* Stop TMR6 */
    TMR6->ctrl1_bit.tmren = 0;

    /* Disable DMA */
    DMA2_CH3->ctrl_bit.chen = 0;

    /* Disable DAC1 — PA4 goes high-impedance */
    dac_enable(DAC1_SELECT, FALSE);

    running = false;
}

void dac_output_set_rate(uint32_t sample_rate)
{
    tmr6_set_rate(sample_rate);
}

uint16_t *dac_output_get_buffer(void)
{
    return dac_buffer;
}

bool dac_output_is_running(void)
{
    return running;
}
