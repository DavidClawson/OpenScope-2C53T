/*
 * =============================================================================
 * FNIRSI 2C53T V1.2.0 — Master Init Phase 4: Saved Config Restore & Calibration
 * =============================================================================
 *
 * Source:   APP_2C53T_V1.2.0_251015.bin (base 0x08000000)
 * Range:    0x08025250 — 0x08025E50 (~3KB)
 * Function: FUN_08023A50 (master_init), continuation
 * Method:   Capstone disassembly + manual annotation from existing decompile
 *
 * Context:
 *   This is the fourth phase of the 15.4KB master init function.
 *   Phases 1-3 configured clocks, GPIO, EXMC, LCD, and basic peripherals.
 *   Phase 4 covers:
 *     A. I2C peripheral configuration (0x08025250-0x08025270)
 *     B. DAC calibration value computation from saved tables (0x08025270-0x08025370)
 *     C. ADC1 configuration for battery sense (0x08025370-0x08025468)
 *     D. Button matrix GPIO setup (0x08025468-0x0802553C)
 *     E. Meter state init + analog MUX calls (0x0802553C-0x0802559A)
 *     F. Analog frontend relay control from saved state (0x0802559A-0x08025662)
 *     G. USART2 GPIO + AFIO remap for SPI3 (0x08025662-0x080257FC)
 *     H. This file's CORE: Saved config validation + restore (0x08025D92-0x08025E50)
 *
 * Key structures:
 *   meter_state (ms)  = 0x200000F8, ~4000 bytes of device state
 *   saved_config      = 0x08006000, flash sector for persistent settings
 *   ms[0xF64]         = saved mode (scope/meter/siggen)
 *   ms[0xF68]         = mode_state (active mode FSM state)
 *   ms[0x260-0x2F8]   = scope calibration gain table (CH1, 20 entries x 16-bit)
 *   ms[0x2D8-0x34E]   = scope calibration offset table (CH2, 20 entries x 16-bit)
 *   ms[0x232-0x236]   = scope voltage scale data
 *
 * Register context at entry to this section:
 *   r4 = 0x40007400   (I2C1 base) at 0x08025250, later reassigned
 *   r5 = 0x40021018   (RCU_APB2EN)
 *   r6 = 0x200000F8   (meter_state base, in some sub-sections)
 *   sl = 0x200000F8   (meter_state base, after 0x08025D9C)
 *   lr = 0x200000F8   (meter_state base, in timer section)
 *   sb = 0x200000F8   (meter_state base, in analog frontend section)
 */

#include <stdint.h>

/* Peripheral base addresses */
#define I2C1_BASE       0x40007400
#define USART2_BASE     0x40004400
#define ADC1_BASE       0x40012400
#define SPI3_BASE       0x40003C00
#define GPIOA_BASE      0x40010800
#define GPIOB_BASE      0x40010C00
#define GPIOC_BASE      0x40011000
#define GPIOD_BASE      0x40011400
#define GPIOE_BASE      0x40011800
#define AFIO_BASE       0x40010000
#define RCU_APB2EN      0x40021018
#define RCU_APB1EN      0x4002101C
#define RCU_AHBEN       0x40021014
#define RCU_CFG0        0x40021004
#define SCB_AIRCR       0xE000ED0C
#define NVIC_ISER0      0xE000E100

/* RAM addresses */
#define METER_STATE_BASE     0x200000F8   /* ms = meter_state */
#define SAVED_CONFIG_FLASH   0x08006000   /* persistent config in flash */
#define RAM_USB_CONFIG       0x20002B24

/* meter_state offsets (all relative to 0x200000F8) */
#define MS_RELAY_CH1         0x00    /* CH1 relay state (AC/DC coupling) */
#define MS_RELAY_CH2         0x01    /* CH2 relay state */
#define MS_METER_MODE        0x02    /* Meter function select (V/A/R/etc) */
#define MS_METER_RANGE       0x03    /* Meter range select */
#define MS_CH1_OFFSET        0x04    /* CH1 signed offset adjustment */
#define MS_CH2_OFFSET        0x05    /* CH2 signed offset adjustment */
#define MS_SCOPE_UNKNOWN_07  0x07    /* Unknown scope param */
#define MS_PARAM_08          0x08    /* Extracted from config word */
#define MS_PARAM_09          0x09    /* Extracted from config word */
#define MS_PARAM_0A          0x0A    /* 16-bit halfword */
#define MS_PROBE_TYPE        0x14    /* Probe type: 1=1x, 2=10x, 3=100x */
#define MS_TB_RANGE          0x16    /* Timebase range index */
#define MS_TB_MODE           0x17    /* Timebase mode (normal/roll/etc) */
#define MS_SCOPE_18          0x18    /* Scope param */
#define MS_SCOPE_1A          0x1A    /* 32-bit scope param */
#define MS_COUPLING          0x2D    /* Coupling mode (AC/DC/GND) */
#define MS_TRIG_LEVEL        0x34    /* Trigger level */
#define MS_TRIG_MODE         0x35    /* Trigger mode (auto/normal/single) */
#define MS_TRIG_EDGE         0x3C    /* Trigger edge (rising/falling) */
#define MS_PARAM_48          0x48    /* Unknown param */
#define MS_PARAM_4C          0x4C    /* 32-bit param */
#define MS_CAL_GAIN_BASE     0x232   /* Scope calibration: voltage scale data */
#define MS_CAL_OFFSET_BASE   0x236   /* Scope calibration: voltage offset data */
#define MS_CAL_ADJ           0x23A   /* Calibration adjustment byte */
#define MS_SCOPE_GAIN_CH1    0x260   /* CH1 gain table start (20 x uint16_t) */
#define MS_SCOPE_GAIN_CH2    0x288   /* CH2 gain table start (20 x uint16_t) (approx) */
#define MS_SCOPE_OFFSET_CH1  0x2D8   /* CH1 offset table start (20 x uint16_t) */
#define MS_SCOPE_OFFSET_CH2  0x300   /* CH2 offset table start (20 x uint16_t) (approx) */
#define MS_SIGGEN_PARAM      0x354   /* Signal generator param */
#define MS_TIMER_PRESCALE    0xE58   /* Timer prescale (4 bytes across E58-E61) */
#define MS_TIMER_PERIOD      0xE5C   /* Timer period */
#define MS_SAVED_MODE        0xF39   /* Saved operational mode byte */
#define MS_MODE_DATA         0xF60   /* Mode data (32-bit) */
#define MS_SAVED_MODE_WORD   0xF64   /* Saved mode (halfword) — scope/meter/siggen */
#define MS_MODE_STATE        0xF68   /* Current mode FSM state */


/* =========================================================================
 * SECTION A: I2C Peripheral Configuration (0x08025250 - 0x08025270)
 * =========================================================================
 *
 * r4 = 0x40007400 (I2C1 base) at this point
 *
 * This configures the I2C1 peripheral for touch controller (GT911/GT915)
 * communication. The touch panel is present on the PCB but not used for
 * button input (buttons use GPIO matrix + passive pins).
 */
void master_init_phase4_i2c(void) {
    volatile uint32_t *i2c = (volatile uint32_t *)I2C1_BASE;

    /* I2C1_CTL0 (0x40007400): configure control register */
    i2c[0] |= 0x38;    /* Set bits 5:3 (frequency/mode config) */
    i2c[0] |= 0x04;    /* Set bit 2 */
    i2c[0] &= ~0xC0;   /* Clear bits 7:6 */
    i2c[0] &= ~0x02;   /* Clear bit 1 (ACK disable?) */
    i2c[0] &= ~0x1000; /* Clear bit 12 */
    i2c[0] |= 0x01;    /* Set bit 0 — PE (peripheral enable) */
}


/* =========================================================================
 * SECTION B: DAC Calibration Computation (0x08025270 - 0x08025370)
 * =========================================================================
 *
 * r6 = meter_state base (0x200000F8)
 * r4 = I2C1 base, later reassigned to ADC1
 *
 * This computes DAC output values for the analog frontend using VFP
 * floating-point math. It reads calibration gain/offset pairs from
 * meter_state tables and interpolates based on probe type and timebase
 * range to produce the correct bias voltage for each channel.
 *
 * The calibration tables at ms[0x260..0x2F8] contain per-range gain
 * values, and ms[0x2D8..0x34E] contain per-range offset values.
 * These are 16-bit unsigned values stored as pairs (gain, offset) for
 * each of the ~20 voltage/timebase ranges.
 *
 * The DAC output calculation uses this formula:
 *   result = ((gain - offset) / scale_factor) * (signed_adj + 100) + offset
 * where signed_adj comes from ms[0x04] or ms[0x05] (CH1/CH2 offset trim).
 */
void master_init_phase4_dac_cal(void) {
    uint8_t *ms = (uint8_t *)METER_STATE_BASE;
    float scale_factor;  /* loaded from literal pool via VLDR s0, [pc, #-0x1FC] */

    /* --- CH1 DAC calibration (0x08025270) --- */
    uint8_t tb_range = ms[0x2D];    /* timebase range index */
    uint8_t ch_index = ms[0x02];    /* channel/meter mode index */

    uint16_t gain_val, offset_val;

    if (tb_range >= 5) {
        /* Fast timebase: use table at ms[0x29C] / ms[0x260] */
        gain_val   = *(uint16_t *)(ms + ch_index * 2 + 0x29C);
        offset_val = *(uint16_t *)(ms + ch_index * 2 + 0x260);
    } else if (tb_range == 4 || ms[0x14] == 3) {
        /* Medium timebase or 100x probe: use table at ms[0x2B0] / ms[0x274] */
        gain_val   = *(uint16_t *)(ms + ch_index * 2 + 0x2B0);
        offset_val = *(uint16_t *)(ms + ch_index * 2 + 0x274);
    } else {
        /* Slow timebase: use table at ms[0x2C4] / ms[0x288] */
        gain_val   = *(uint16_t *)(ms + ch_index * 2 + 0x2C4);
        offset_val = *(uint16_t *)(ms + ch_index * 2 + 0x288);
    }

    /* VFP interpolation */
    float delta = (float)(gain_val - offset_val);
    float normalized = delta / scale_factor;
    int8_t ch1_offset = (int8_t)ms[0x04];
    float adjusted = normalized * (float)(ch1_offset + 100);
    float dac_value = adjusted + (float)offset_val;

    /* Write to I2C1 or DAC register via r4[8], preserving upper 20 bits */
    uint32_t dac_reg = *(volatile uint32_t *)(I2C1_BASE + 8);
    uint32_t dac_out = (uint32_t)dac_value;
    dac_reg = (dac_reg & 0xFFFFF000) | (dac_out & 0xFFF);
    *(volatile uint32_t *)(I2C1_BASE + 8) = dac_reg;

    /* Enable: set bit 0 of I2C1+4 */
    *(volatile uint32_t *)(I2C1_BASE + 4) |= 1;

    /* --- CH2 DAC calibration (0x080252F6) --- */
    /* Same pattern as CH1 but uses ms[0x03], ms[0x05] */
    /* Tables offset by 0x38 from CH1 tables:           */
    /*   Fast:   ms[0x314] / ms[0x2D8]                  */
    /*   Medium: ms[0x328] / ms[0x2EC]                  */
    /*   Slow:   ms[0x33C] / ms[0x300]                  */
    /* Result written to r8 (separate DAC channel)       */

    uint8_t ch2_index = ms[0x03];
    /* ... same VFP math, writes to second DAC register ... */
}


/* =========================================================================
 * SECTION C: ADC1 Configuration for Battery Sense (0x08025370 - 0x08025468)
 * =========================================================================
 *
 * Configures ADC1 at 0x40012400 for PB1 (Channel 9) battery voltage
 * measurement. Uses 239.5-cycle sample time, right-aligned data,
 * software-triggered single conversion mode.
 *
 * r4 = 0x40012408 (ADC1_CTL0 + offset)
 * r5 = RCU_APB2EN
 */
void master_init_phase4_adc(void) {
    volatile uint32_t *rcu_apb2en = (volatile uint32_t *)RCU_APB2EN;

    /* Enable GPIOB clock for PB1 analog input */
    *rcu_apb2en |= 0x08;

    /* Configure PB1 as analog input via gpio_init() */
    /* gpio_init(GPIOA_BASE, {pin=2, mode=analog_input}) */
    /* (Pin 2 of sp+0x40 struct, GPIOA base passed in r0) */

    /* Enable ADC1 clock */
    *rcu_apb2en |= 0x200;

    /* Configure ADC clock prescaler: ADC_CLK = PCLK2 / 6 */
    /* RCU_CFG0[15:14] = 0b10 (divide by 6) */
    volatile uint32_t *rcu_cfg0 = (volatile uint32_t *)RCU_CFG0;
    uint32_t cfg = *rcu_cfg0;
    cfg = (cfg & ~0x0000C000) | (2 << 14);
    *rcu_cfg0 = cfg;
    *rcu_cfg0 &= ~0x10000000;  /* Clear bit 28 (additional prescaler) */

    volatile uint32_t *adc = (volatile uint32_t *)ADC1_BASE;

    /* ADC1_SAMPT1 (0x40012404): clear channel sample time bits [19:16] */
    adc[1] &= ~0xF0000;

    /* ADC1_SAMPT1: set channel 9 sample time = 239.5 cycles */
    /* Bits [8] = 1 (part of 3-bit sample time field for ch9 = 0b100?) */
    adc[1] |= 0x100;

    /* ADC1_CTL0 (0x40012408): configure scan mode, etc. */
    adc[2] |= 0x02;        /* SCAN mode enable? */
    adc[2] &= ~0x800;      /* Disable JEOCIE (injected end-of-conversion interrupt) */

    /* ADC1_SQR3 (0x4001242C): set regular sequence length */
    adc[0x2C/4] &= ~0xF00000;  /* Clear L[23:20] = 1 conversion */

    /* ADC1_SQR2 (0x40012410): configure sample time register */
    adc[0x10/4] |= 0x38000000;  /* Set ch9 sample time bits 29:27 = 0b111 (239.5 cycles) */

    /* ADC1_SQR1 (0x40012434): set first regular sequence channel = 9 */
    uint32_t sqr = adc[0x34/4];
    sqr = (sqr & ~0x1F) | 9;    /* SQ1[4:0] = 9 */
    adc[0x34/4] = sqr;

    /* ADC1 mode configuration */
    adc[2] &= ~0x2000000;  /* Clear ALIGN (right-aligned) */
    adc[2] |= 0xE0000;     /* External trigger: software trigger (EXTSEL = 0b111) */
    adc[2] |= 0x100000;    /* EXTTRIG: enable external trigger */
    adc[2] |= 0x100;       /* ADON related */
    adc[2] |= 0x01;        /* ADON = 1 (power on ADC) */
    adc[2] |= 0x08;        /* RSTCAL = 1 (reset calibration) */

    /* Wait for calibration reset to complete (RSTCAL clears when done) */
    /* Polls bit 3 of ADC1_CTL0 until clear */
    while (adc[2] & 0x08) { /* spin */ }

    /* Start calibration */
    adc[2] |= 0x04;        /* CAL = 1 (start calibration) */

    /* Wait for calibration to complete (CAL clears when done) */
    /* Polls bit 2 of ADC1_CTL0 until clear */
    while (adc[2] & 0x04) { /* spin */ }
}


/* =========================================================================
 * SECTION D: Button Matrix GPIO + Timer Setup (0x08025468 - 0x0802553C)
 * =========================================================================
 *
 * Configures GPIO pins for the 4x3 bidirectional button matrix and
 * 3 passive button inputs. Also configures timer peripherals.
 *
 * Matrix rows: PA7, PB0, PC5, PE2 (configured as push-pull output)
 * Matrix cols: PA8, PC10, PE3 (configured as input with pull-up)
 * Passive:     PC8 (POWER), PB7 (PRM), PC13 (UP)
 *
 * Enable clocks: GPIOC, GPIOD, GPIOE
 */
void master_init_phase4_buttons(void) {
    volatile uint32_t *rcu_apb2en = (volatile uint32_t *)RCU_APB2EN;

    /* Enable GPIOC clock */
    *rcu_apb2en |= 0x10;
    /* Enable GPIOD clock */
    *rcu_apb2en |= 0x40;
    /* Enable GPIOD clock (repeated -- belt-and-suspenders) */
    *rcu_apb2en |= 0x40;
    /* Enable GPIOD clock (repeated) */
    *rcu_apb2en |= 0x40;
    /* Enable GPIOA clock */
    *rcu_apb2en |= 0x04;
    /* Enable GPIOB clock */
    *rcu_apb2en |= 0x08;
    /* Enable GPIOB clock */
    *rcu_apb2en |= 0x08;
    /* Enable GPIOA clock */
    *rcu_apb2en |= 0x04;

    /* Configure GPIOC pin4 as output (matrix row PC5?) */
    /* gpio_init(GPIOC, pin=0x1000, mode=output_push_pull, speed=50MHz) */

    /* Configure GPIOE pins 0x10, 0x20, 0x40 as outputs */
    /* gpio_init(GPIOE, pin=0x10...) */
    /* gpio_init(GPIOE, pin=0x20...) */
    /* gpio_init(GPIOE, pin=0x40...) */

    /* Configure GPIOA pin PA15 as output */
    /* gpio_init(GPIOA, pin=0x8000, ...) */

    /* Configure GPIOB pins 0x800, 0x400 as inputs */
    /* gpio_init(GPIOB, pin=0x800...) */
    /* gpio_init(GPIOB, pin=0x400...) */

    /* Configure GPIOA pin 0x400 as input */
    /* gpio_init(GPIOA, pin=0x400...) */
}


/* =========================================================================
 * SECTION E: Meter State Init + Analog MUX Calls (0x0802553C - 0x0802559A)
 * =========================================================================
 *
 * THE KEY SECTION for meter IC activation.
 *
 * Loads meter_state base address (0x200000F8) and calls two critical
 * analog MUX configuration functions:
 *
 *   FUN_080018a4 (gpio_mux_portc_porte):
 *     10-way switch writing GPIOC/E BOP/BCR for analog relay switching.
 *     Called with ms[0x02] (meter function select).
 *     Controls which analog input path is routed to the meter IC in the FPGA.
 *
 *   FUN_08001a58 (gpio_mux_porta_portb):
 *     Writes GPIOA/B BOP/BCR for analog signal routing.
 *     Called with ms[0x03] (meter range select).
 *     Sets gain resistors / voltage dividers for the selected range.
 *
 * These are the functions that activate the analog frontend for the meter:
 * they control physical relays via GPIO that route the probe input through
 * different signal conditioning paths depending on whether we're measuring
 * voltage, current, resistance, continuity, or diode.
 *
 * NOTE: No SPI3 or USART2 commands are sent here. The FPGA meter IC is
 * activated later via USART2 commands (0x01 mode select) in the boot
 * command sequence at 0x08025D96+.
 */
void master_init_phase4_meter_mux(void) {
    uint8_t *ms = (uint8_t *)METER_STATE_BASE;

    /* r4 = 0x200000F8 (meter_state base) */

    /* Call analog MUX function for meter mode */
    uint8_t meter_mode = ms[MS_METER_MODE];      /* ms[0x02] */
    gpio_mux_portc_porte(meter_mode);             /* FUN_080018a4 */

    /* Call analog MUX function for meter range */
    uint8_t meter_range = ms[MS_METER_RANGE];     /* ms[0x03] */
    gpio_mux_porta_portb(meter_range);            /* FUN_08001a58 */

    /* Enable GPIOD, GPIOE clocks (for the relay GPIOs used above) */
    volatile uint32_t *rcu = (volatile uint32_t *)RCU_APB2EN;
    *rcu |= 0x20;  /* GPIOD */
    *rcu |= 0x20;  /* (repeated for safety) */
}


/* =========================================================================
 * SECTION F: Analog Frontend Relay Control (0x0802559A - 0x08025662)
 * =========================================================================
 *
 * Restores relay states from saved meter_state for CH1/CH2 coupling.
 *
 * ms[0x00] = CH1 relay state (0 = open/AC, nonzero = closed/DC)
 * ms[0x01] = CH2 relay state
 *
 * Controls:
 *   If ms[0x00] != 0: GPIOD_BOP (set PE12?) — close CH1 DC coupling relay
 *   If ms[0x00] == 0: GPIOD_BC  (clear)     — open CH1 relay (AC coupling)
 *   If ms[0x01] != 0: GPIOD_BOP (set PE13?) — close CH2 DC coupling relay
 *   If ms[0x01] == 0: GPIOD_BC  (clear)     — open CH2 relay
 *
 * Then based on ms[0x14] (probe type):
 *   Case 1 (1x probe):  Set PC2, Set PC1
 *   Case 3 (100x probe): Set PC2, Clear PC1
 *   Case 2 (10x probe):  Clear PC2, Clear PC1
 *   Default:             Skip relay control
 *
 * This controls the input attenuation network to match the probe type.
 */
void master_init_phase4_relay_restore(void) {
    uint8_t *ms = (uint8_t *)METER_STATE_BASE;

    /* CH1 coupling relay */
    if (ms[0x00] != 0) {
        GPIOD_BOP = 0x1000;   /* Set PD12 — DC coupling */
    } else {
        GPIOD_BC  = 0x1000;   /* Clear PD12 — AC coupling */
    }

    /* CH2 coupling relay */
    if (ms[0x01] != 0) {
        GPIOD_BOP = 0x2000;   /* Set PD13 — DC coupling */
    } else {
        GPIOD_BC  = 0x2000;   /* Clear PD13 — AC coupling */
    }

    /* Probe attenuation control via PC1, PC2 */
    uint8_t probe_type = ms[MS_PROBE_TYPE];  /* ms[0x14] */
    switch (probe_type) {
        case 1:  /* 1x probe */
            GPIOC_BOP = 0x04;  /* Set PC2 */
            GPIOC_BOP = 0x02;  /* Set PC1 */
            break;
        case 3:  /* 100x probe */
            GPIOC_BOP = 0x04;  /* Set PC2 */
            GPIOC_BC  = 0x02;  /* Clear PC1 */
            break;
        case 2:  /* 10x probe */
            GPIOC_BC  = 0x04;  /* Clear PC2 */
            GPIOC_BC  = 0x02;  /* Clear PC1 */
            break;
        default:
            /* No attenuation change */
            break;
    }
}


/* =========================================================================
 * SECTION G: USART2 GPIO + AFIO JTAG Remap (0x08025662 - 0x080257FC)
 * =========================================================================
 *
 * Configures USART2 (0x40004400) GPIO pins for FPGA command channel:
 *   PA2 = USART2_TX (AF push-pull, 50MHz)
 *   PA3 = USART2_RX (input floating)
 *
 * Also configures AFIO pin remapping to free PB3/PB4/PB5 from JTAG:
 *   AFIO_PCF0[15:12] = 0b0010 (disable JTAG-DP, keep SW-DP)
 *   This is CRITICAL: frees PB3(SCK), PB4(MISO), PB5(MOSI) for SPI3
 *
 * Additional EXTI and AFIO configurations for edge detection.
 *
 * NVIC priority configuration using SCB_AIRCR PRIGROUP field.
 */
void master_init_phase4_usart_afio(void) {
    volatile uint32_t *rcu = (volatile uint32_t *)RCU_APB2EN;

    /* Enable GPIOD, GPIOE clocks for relay/EXTI GPIOs */
    *rcu |= 0x20;
    *rcu |= 0x20;

    /* Configure GPIOD pin 8 as push-pull output (USART2 direction?) */
    /* gpio_init(GPIOD, pin=8, mode=output_push_pull) */

    /* Configure PA3 as input (USART2_RX) */
    /* gpio_init(GPIOA, pin=4, mode=AF_push_pull) — GPIOD actually */

    /* Set PD3 HIGH (USART direction control?) */
    GPIOD_BOP = 0x08;

    /* Enable GPIOA clock */
    *rcu |= 0x04;
    *rcu |= 0x04;

    /* Configure PA2 as AF push-pull (USART2_TX) */
    /* gpio_init(GPIOA, pin=2, mode=AF_push_pull, speed=50MHz) */

    /* Configure PA1 as input (USART2_RX complement?) */
    /* gpio_init(GPIOA, pin=1, mode=input_floating) */

    /* Enable GPIOC clock */
    *rcu |= 0x10;

    /* Configure PC11 as input (SPI3_MISO alternate?) */
    /* gpio_init(GPIOC, pin=0x800, mode=input) */
    GPIOC_BC = 0x800;  /* Clear PC11 */

    /* Configure PC8 as input (POWER button - passive) */
    /* gpio_init(GPIOC, pin=0x08, mode=input_pull_up) */

    /* Enable AFIO clock */
    *rcu |= 0x01;
    *rcu |= 0x10;

    /* === AFIO JTAG Remap === */
    /* AFIO_PCF0 (0x40010008): remap JTAG pins */
    volatile uint32_t *afio_pcf0 = (volatile uint32_t *)(AFIO_BASE + 0x08);
    *afio_pcf0 &= ~0xF000;    /* Clear JTAG remap bits [15:12] */
    *afio_pcf0 |=  0x2000;    /* Set to 0b0010: JTAG-DP disabled, SW-DP enabled */
    /* After this: PB3 = free (SPI3_SCK), PB4 = free (SPI3_MISO), PB5 = free (SPI3_MOSI) */

    /* EXTI configuration for button/probe edge detection */
    /* Various AFIO EXTICR register writes */

    /* NVIC priority configuration */
    uint32_t aircr = *(volatile uint32_t *)SCB_AIRCR;
    uint8_t prigroup = (aircr >> 8) & 0x07;
    /* ... compute and write interrupt priority ... */
}


/* =========================================================================
 * SECTION H: Saved Config Validation + State Restore (0x08025D92 - 0x08025E50)
 * =========================================================================
 *
 * THIS IS THE CORE OF PHASE 4.
 *
 * r4 = 0x08006000 (saved configuration data in flash)
 * sl = 0x200000F8 (meter_state base)
 *
 * The saved config at 0x08006000 is a ~300-byte structure written to
 * flash when the user powers off or changes settings. It contains all
 * persistent device state: mode, ranges, calibration trim, coupling,
 * probe type, trigger settings, and the full 40-entry gain/offset
 * calibration table.
 *
 * Validation logic:
 *   byte[0] of saved_config is checked:
 *     0x55 = valid config (normal boot, restore all settings)
 *     0xAA = valid but meter mode (set ms[0xF68] = 8 first, then restore)
 *     other = INVALID (skip restore, use defaults at 0x08026198)
 *
 * Data layout of saved_config at 0x08006000:
 *   [0x00]      uint8_t  signature (0x55 or 0xAA)
 *   [0x01]      uint8_t  -> ms[0x00] (CH1 relay state)
 *   [0x02]      uint8_t  -> ms[0x01] (CH2 relay state)
 *   [0x03]      uint8_t  -> ms[0x02] (meter mode)
 *   [0x04-0x07] uint32_t -> ms[0x03] (meter range + padding)
 *   [0x08]      uint8_t  -> ms[0x07] (scope param)
 *   [0x09]      uint8_t  -> ms[0x14] (probe type)
 *   [0x0A]      uint8_t  -> ms[0x16] (timebase range)
 *   [0x0B]      uint8_t  -> ms[0x17] (timebase mode)
 *   [0x0C-0x0F] block    -> ms[0x18], ms[0x2D], ms[0x34], ms[0x35]
 *   [0x10-0x13] uint32_t -> ms[0x1A]
 *   [0x14-0x17] block    -> ms[0x3C], ms[0x23A], ms[0x354], ms[0xF39]
 *   [0x18-0x1B] uint32_t -> ms[0x4C]
 *   [0x1C-0x1F] uint32_t -> ms[0x232] (scope cal scale data)
 *   [0x20-0x23] uint32_t -> ms[0x236] (scope cal offset data)
 *   [0x24-0x27] block    -> ms[0xE58..E61] (timer params)
 *   [0x28-0x2B] uint32_t -> ms[0xE5C] (timer period)
 *   [0x2C-0x2F] uint32_t -> ms[0xF60] (mode data)
 *   [0x30-0x33] block    -> ms[0xF64] (saved mode), ms[0x08], ms[0x09]
 *   [0x34-0x37] block    -> ms[0x0A] (halfword), ms[0x48]
 *   [0x38-0x4B] 20 bytes -> ms[0x260..0x266] + ms[0x2D8..0x2DE]
 *                           (first 4 gain/offset pairs, packed)
 *   [0x48-0x12C] remaining calibration table entries
 *                           40 x uint32_t, each split into two uint16_t:
 *                           low half  -> ms[0x260 + i*2] (gain table)
 *                           high half -> ms[0x2D8 + i*2] (offset table)
 */
void master_init_phase4_config_restore(void) {
    uint8_t *ms = (uint8_t *)METER_STATE_BASE;
    volatile uint32_t *config = (volatile uint32_t *)SAVED_CONFIG_FLASH;

    /* Load first word of saved config */
    uint32_t word0 = config[0];
    uint8_t signature = word0 & 0xFF;

    if (signature == 0x55) {
        /* Valid config — proceed to restore */
        goto restore_config;
    }
    else if (signature == 0xAA) {
        /* Valid config, but device was in meter mode when saved */
        ms[MS_MODE_STATE] = 8;  /* ms[0xF68] = 8 (meter mode FSM state) */
        goto restore_config;
    }
    else {
        /* INVALID config — use hardcoded defaults */
        goto use_defaults;  /* branch to 0x08026198 */
    }

restore_config:
    /*
     * Unpack saved_config into meter_state.
     * The firmware does this with a series of ldr/lsrs/strb/strh instructions,
     * unpacking packed 32-bit words into individual byte/halfword fields.
     */

    /* Word 0: [signature | ms[0x00] | ms[0x01] | ms[0x02]] */
    ms[0x00] = (word0 >> 8)  & 0xFF;   /* CH1 relay state */
    ms[0x01] = (word0 >> 16) & 0xFF;   /* CH2 relay state */
    ms[0x02] = (word0 >> 24) & 0xFF;   /* Meter mode select */

    /* Word 1: ms[0x03] (meter range, 4 bytes) */
    *(uint32_t *)(ms + 0x03) = config[1];

    /* Word 2: [ms[0x07] | ms[0x14] | ms[0x16] | ms[0x17]] */
    uint32_t word2 = config[2];
    ms[0x07] = word2 & 0xFF;           /* Scope param */
    ms[0x14] = (word2 >> 8)  & 0xFF;   /* Probe type */
    ms[0x16] = (word2 >> 16) & 0xFF;   /* Timebase range */
    ms[0x17] = (word2 >> 24) & 0xFF;   /* Timebase mode */

    /* Words 3-4: ldm r3, {r0, r1, r2, r3}  from config+0x0C */
    /* Unpacks into multiple scattered ms[] offsets */
    uint32_t w3_r0 = config[3];   /* [ms[0x18] | ms[0x2D] | ms[0x34] | ms[0x35]] */
    uint32_t w3_r1 = config[4];   /* -> ms[0x1A] (32-bit) */
    uint32_t w3_r2 = config[5];   /* [ms[0x3C] | ms[0x23A] | ms[0x354] | ms[0xF39]] */
    uint32_t w3_r3 = config[6];   /* -> ms[0x4C] (32-bit) */

    ms[0x18]  = w3_r0 & 0xFF;
    ms[0x2D]  = (w3_r0 >> 8) & 0xFF;    /* Coupling mode */
    ms[0x34]  = (w3_r0 >> 16) & 0xFF;   /* Trigger level */
    ms[0x35]  = (w3_r0 >> 24) & 0xFF;   /* Trigger mode */
    *(uint32_t *)(ms + 0x1A) = w3_r1;

    ms[0x3C]   = w3_r2 & 0xFF;          /* Trigger edge */
    ms[0x23A]  = (w3_r2 >> 8) & 0xFF;   /* Calibration adjustment */
    ms[0x354]  = (w3_r2 >> 16) & 0xFF;  /* Signal generator param */
    ms[0xF39]  = (w3_r2 >> 24) & 0xFF;  /* Saved operational mode */
    *(uint32_t *)(ms + 0x4C) = w3_r3;

    /* Words 7-10: ldm from config+0x1C */
    uint32_t w7_r0 = config[7];   /* -> ms[0x232] (scope cal scale) */
    uint32_t w7_r1 = config[8];   /* -> ms[0x236] (scope cal offset) */
    uint32_t w7_r2 = config[9];   /* -> ms[0xE58..E61] (timer params, 4 bytes scattered) */
    uint32_t w7_r3 = config[10];  /* -> ms[0xE5C] (timer period) */

    *(uint32_t *)(ms + 0x232) = w7_r0;
    *(uint32_t *)(ms + 0x236) = w7_r1;

    ms[0xE58] = w7_r2 & 0xFF;
    ms[0xE59] = (w7_r2 >> 8)  & 0xFF;
    ms[0xE60] = (w7_r2 >> 16) & 0xFF;
    ms[0xE61] = (w7_r2 >> 24) & 0xFF;
    *(uint32_t *)(ms + 0xE5C) = w7_r3;

    /* Words 11-12: mode data + mode/params */
    uint32_t w11 = config[11];  /* -> ms[0xF60] (mode data) */
    uint32_t w12 = config[12];  /* split: low16->ms[0xF64], byte2->ms[0x08], byte3->ms[0x09] */

    *(uint32_t *)(ms + 0xF60) = w11;
    *(uint16_t *)(ms + 0xF64) = w12 & 0xFFFF;   /* SAVED MODE (scope/meter/siggen) */
    ms[0x08] = (w12 >> 16) & 0xFF;
    ms[0x09] = (w12 >> 24) & 0xFF;

    /* Word 13: */
    uint32_t w13 = config[13];
    *(uint16_t *)(ms + 0x0A) = w13 & 0xFFFF;
    ms[0x48] = (w13 >> 16) & 0xFF;

    /*
     * === CALIBRATION TABLE RESTORE (0x08025E68 - 0x08026196) ===
     *
     * 40 entries loaded from config[14] through config[14+39].
     * Each 32-bit word is split into two 16-bit values:
     *   low  half -> scope gain table:   ms[0x260 + i*2]
     *   high half -> scope offset table: ms[0x2D8 + i*2]
     *
     * First 4 entries have special unpacking (different field mapping).
     * Entries 4-39 follow the regular pattern.
     *
     * This covers ALL 40 gain/offset calibration pairs:
     *   ms[0x260..0x2D6] = 60 bytes of gain values (CH1+CH2, 20 ranges each)
     *   ms[0x2D8..0x34E] = 60 bytes of offset values
     *
     * These tables are used by the VFP calibration pipeline in the
     * SPI3 acquisition task to convert raw 8-bit ADC values to
     * calibrated voltage readings.
     */

    /* First 4 entries have custom mapping: */
    /* config[14] -> ms[0x260](h), ms[0x262](h), ms[0x2DA](from >>16), ms[0x2DC] */
    /* ... special first block handling ... */

    /* Regular entries (example for entry i, starting from config[18]): */
    for (int i = 0; i < 36; i++) {
        uint32_t cal_word = config[18 + i];
        uint16_t gain_val   = cal_word & 0xFFFF;
        uint16_t offset_val = cal_word >> 16;
        *(uint16_t *)(ms + 0x268 + i * 2) = gain_val;
        *(uint16_t *)(ms + 0x2E0 + i * 2) = offset_val;
    }

    /* Final two halfword entries at end of table */
    *(uint16_t *)(ms + 0x350) = config[74] & 0xFFFF;   /* config[0x128] */
    *(uint16_t *)(ms + 0x352) = config[75] & 0xFFFF;   /* config[0x12C] */

    goto config_done;  /* b 0x080261A8 */


use_defaults:
    /* === DEFAULT VALUES (0x08026198 - 0x08026506) === */

    /*
     * When saved config is INVALID (no 0x55 or 0xAA signature),
     * the firmware writes hardcoded factory-default calibration values
     * to the gain and offset tables.
     *
     * Also sets:
     *   ms[0xF68] = 8  (mode_state = 8, meaning settings/startup mode)
     *   ms[0xF60] = 1  (mode data = 1)
     */
    ms[MS_MODE_STATE] = 8;          /* Default to startup/settings mode */
    *(uint8_t *)(ms + 0xF60) = 1;   /* mode data = 1 */

    /* ms[0x34E] was loaded earlier; if it's 0xFFFF or 0x0000, use defaults */

config_done:
    /* Check ms[0x34E] (last calibration entry / mode indicator) */
    uint16_t mode_indicator = *(uint16_t *)(ms + 0x34E);

    if (mode_indicator == 0xFFFF || mode_indicator == 0x0000) {
        /*
         * Calibration data missing or erased — write factory defaults.
         *
         * DEFAULT SCOPE GAIN TABLE (ms[0x260..0x2D6]):
         * These are ADC-to-voltage conversion factors for each range.
         * Values are ~0x0640-0x0670 range, representing ~1600-1640
         * in decimal — the raw ADC center point for each voltage scale.
         *
         * Table layout (20 entries, CH1 + CH2 interleaved):
         *   ms[0x260] = 0x0667   (50mV/div CH1)
         *   ms[0x262] = 0x0665   (50mV/div CH2)
         *   ms[0x264] = 0x065E   (100mV/div CH1)
         *   ms[0x266] = 0x065B   (100mV/div CH2)
         *   ms[0x268] = 0x065A   (200mV/div CH1)
         *   ms[0x26A] = 0x0669   (200mV/div CH2)
         *   ms[0x26C] = 0x065F   (500mV/div CH1)
         *   ms[0x26E] = 0x065A   (500mV/div CH2)
         *   ms[0x270] = 0x0657   (1V/div CH1)
         *   ms[0x272] = 0x0655   (1V/div CH2)
         *   ms[0x274] = 0x0662   (2V/div CH1)
         *   ms[0x276] = 0x0663   (2V/div CH2)
         *   ms[0x278] = 0x0659   (5V/div CH1)
         *   ms[0x27A] = 0x0659   (5V/div CH2)
         *   ms[0x27C] = 0x0657   (10V/div CH1)
         *   ms[0x27E] = 0x0667   (10V/div CH2)
         *   ms[0x280] = 0x065D   (20V/div, 1x probe)
         *   ms[0x282] = 0x0658   (50V CH1)
         *   ms[0x284] = 0x0655   (50V CH2)
         *   ms[0x286] = 0x0655   (100V)
         */
        *(uint32_t *)(ms + 0x260) = 0x06650667;  /* 50mV/div: CH1=0x667, CH2=0x665 */
        *(uint32_t *)(ms + 0x264) = 0x065B065E;
        *(uint32_t *)(ms + 0x268) = 0x0669065A;
        *(uint32_t *)(ms + 0x26C) = 0x065A065F;
        *(uint32_t *)(ms + 0x270) = 0x06550657;
        *(uint32_t *)(ms + 0x274) = 0x06630662;
        *(uint32_t *)(ms + 0x278) = 0x06590659;
        *(uint32_t *)(ms + 0x27C) = 0x06670657;
        *(uint16_t *)(ms + 0x280) = 0x065D;
        *(uint32_t *)(ms + 0x282) = 0x06550658;
        *(uint16_t *)(ms + 0x286) = 0x0655;

        /* 10x probe gain entries */
        *(uint32_t *)(ms + 0x288) = 0x065D0665;
        *(uint32_t *)(ms + 0x28C) = 0x065A0658;  /* adjusted for 10x */
        *(uint32_t *)(ms + 0x290) = 0x06660656;
        /* ... remaining 10x entries ... */

        /*
         * DEFAULT SCOPE OFFSET TABLE (ms[0x2D8..0x34E]):
         * These are the ADC zero-offset values for each range.
         * Values are in the ~0x0630-0x0660 range (slightly below gain values).
         * The difference (gain - offset) defines the measurement span.
         *
         *   ms[0x2D8] = 0x065D   (50mV/div CH1)
         *   ms[0x2DA] = 0x064B   (50mV/div CH2)
         *   ms[0x2DC] = 0x063F   (100mV/div)
         *   ms[0x2DE] = 0x063C   (100mV/div)
         *   ms[0x2E0] = 0x063A   (200mV)
         *   ms[0x2E2] = 0x0658   (200mV)
         *   ms[0x2E4] = 0x0648   (500mV)
         *   ms[0x2E6] = 0x063F   (500mV)
         *   ms[0x2E8] = 0x063A   (1V)
         *   ms[0x2EA] = 0x0639   (1V)
         *   ms[0x2EC] = 0x064B   (2V)
         *   ms[0x2EE] = 0x064A   (2V)
         *   ms[0x2F0] = 0x0639   (5V offset pair 1)
         *   ms[0x2F2] = 0x063A   (5V offset pair 2)
         *   ... and so on ...
         *   ms[0x2F8] = 0x0646   (20V offset)
         */
        *(uint32_t *)(ms + 0x2D8) = 0x064B065D;
        *(uint32_t *)(ms + 0x2DC) = 0x063C063F;
        *(uint32_t *)(ms + 0x2E0) = 0x0658063A;
        *(uint32_t *)(ms + 0x2E4) = 0x063F0648;
        *(uint32_t *)(ms + 0x2E8) = 0x0639063A;
        *(uint32_t *)(ms + 0x2EC) = 0x064A064B;
        *(uint32_t *)(ms + 0x2F0) = 0x063A0639;  /* strd */
        *(uint32_t *)(ms + 0x2F4) = 0x06560637;  /* strd */
        *(uint16_t *)(ms + 0x2F8) = 0x0646;
        *(uint32_t *)(ms + 0x2FA) = 0x0639063F;
        *(uint16_t *)(ms + 0x2FE) = 0x0637;

        /*
         * METER CALIBRATION DEFAULTS (ms[0x29C..0x2D6] and ms[0x314..0x34E]):
         * Higher range values for meter mode, using ~0x0C00 range values.
         * These are the factory-default calibration for the multimeter IC.
         *
         * These calibration values convert BCD-coded meter IC readings
         * to displayable voltage/current/resistance values.
         *
         *   ms[0x29C] = 0x0CCE   ms[0x314] = 0x0CC7/0x0CB4
         *   ms[0x29E] = 0x0CC4   ms[0x316] = pair
         *   ms[0x2A0] = 0x0CBC   ms[0x318] = 0x0CA7
         *   ms[0x2A2] = 0x0CBA   ms[0x31A] = 0x0CA5/0x0CA3
         *   ms[0x2A4] = 0x0CB9   ...
         *   ms[0x2A6] = 0x0CC5   ms[0x31E] = 0x0CC2
         *   ms[0x2A8] = 0x0CBB
         *   ...etc...
         *   ms[0x2D4] = 0x0CB0
         *   ms[0x2D6] = 0x0CAF
         *
         *   ms[0x350] = 0x05FA   (special trailing value)
         *   ms[0x34E] = 0x0CA4   (last offset entry)
         */
        *(uint32_t *)(ms + 0x29C) = 0x0CC40CCE;
        *(uint16_t *)(ms + 0x2A0) = 0x0CBC;
        *(uint32_t *)(ms + 0x2A2) = 0x0CB90CBA;
        *(uint32_t *)(ms + 0x2A6) = 0x0CBB0CC5;
        *(uint32_t *)(ms + 0x2AA) = 0x0CB40CB6;
        *(uint32_t *)(ms + 0x2AE) = 0x0CC40CB3;
        *(uint32_t *)(ms + 0x2B2) = 0x0CBB0CC2;
        *(uint32_t *)(ms + 0x2B6) = 0x0CB90CBB;
        *(uint32_t *)(ms + 0x2BA) = 0x0CCB;
        /* ... etc (continues through 0x2D6) ... */

        /* Meter offset defaults */
        *(uint32_t *)(ms + 0x310) = 0x0CB40CC7;
        *(uint16_t *)(ms + 0x318) = 0x0CA7;
        *(uint32_t *)(ms + 0x31A) = 0x0CA30CA5;
        *(uint16_t *)(ms + 0x31E) = 0x0CC2;
        *(uint32_t *)(ms + 0x320) = 0x0CAA0CB2;
        /* ... etc (continues through 0x34E) ... */

        /* Final special entries */
        *(uint32_t *)(ms + 0x34E) = 0x05FA0CA4;
        /* 0x05FA at ms[0x350]: possibly a firmware version marker or
         * special sentinel indicating factory-default calibration */
    }

    /* === END OF PHASE 4 ===
     *
     * At this point ms[0x34E] contains either:
     *   - A value loaded from saved flash config (if signature was 0x55/0xAA)
     *   - 0x0CA4 (factory default)
     *
     * The code continues at 0x080261A8 to check ms[0x34E]:
     *   if (ms[0x34E] == 0xFFFF || ms[0x34E] == 0x0000):
     *     Write ALL default calibration values (big block at 0x080261BE-0x08026506)
     *   else:
     *     Skip to 0x0802650A (SPI3 GPIO configuration — Phase 5)
     *
     * This means: even if saved config existed, if the calibration table's
     * last entry is 0xFFFF or 0x0000, the firmware treats the calibration
     * as corrupt and overwrites with factory defaults.
     */
}


/*
 * =============================================================================
 * KEY FINDINGS FOR METER IC ACTIVATION
 * =============================================================================
 *
 * 1. NO SPI3 WRITES TO FPGA IN THIS PHASE.
 *    The FPGA data bus (SPI3 at 0x40003C00) is NOT used in this section.
 *    SPI3 GPIO configuration happens LATER at 0x08026540+.
 *
 * 2. NO USART2 COMMANDS IN THIS PHASE.
 *    USART2 command transmission starts at 0x08025D96+ (the boot command
 *    sequence), but those are AFTER task creation, which is also after
 *    this calibration phase.
 *
 * 3. METER ACTIVATION IS VIA GPIO ANALOG MUX:
 *    At 0x0802553C, the firmware calls:
 *      gpio_mux_portc_porte(ms[0x02])  — selects meter function via relays
 *      gpio_mux_porta_portb(ms[0x03])  — selects meter range via gain resistors
 *    These two calls configure the ANALOG FRONTEND hardware path that routes
 *    the probe input to the FPGA's internal meter IC.
 *
 * 4. ANALOG FRONTEND PINS:
 *    - PC1, PC2: Probe attenuation control (1x/10x/100x)
 *    - PD12: CH1 DC coupling relay
 *    - PD13: CH2 DC coupling relay
 *    - The gpio_mux functions write to GPIOC/E (0x40011010/0x40011810)
 *      and GPIOA/B (0x40010810/0x40010C10) for relay switching.
 *
 * 5. CALIBRATION TABLE STRUCTURE:
 *    - 40 entries total (20 for scope ranges + 20 for meter ranges)
 *    - Scope gains:  ms[0x260..0x27E] (16 entries + extra)
 *    - Scope offsets: ms[0x2D8..0x2F8]
 *    - Meter gains:  ms[0x288..0x2D6] (values ~0x0C00 range, much higher than scope ~0x0600)
 *    - Meter offsets: ms[0x300..0x34E]
 *    - The ~2x higher meter calibration values (0x0C00 vs 0x0600) reflect
 *      the meter IC's different ADC resolution/range compared to the scope ADC.
 *
 * 6. CONFIG VALIDATION FLOW:
 *    flash[0x08006000] byte 0:
 *      0x55 -> restore all settings, keep current mode_state
 *      0xAA -> restore all settings, set mode_state=8 (meter mode)
 *      else -> set mode_state=8, mode_data=1, use factory defaults
 *    Then check last cal entry (ms[0x34E]):
 *      0xFFFF or 0x0000 -> overwrite with factory default calibration
 *      else -> keep restored values
 *
 * 7. MODE DISPATCH:
 *    ms[0xF64] = saved mode (halfword), used later by display task to
 *    determine initial screen (scope, meter, siggen, settings).
 *    ms[0xF68] = mode_state byte, set to 8 for meter/default modes.
 *    ms[0xF39] = saved operational mode byte (scope/meter/siggen),
 *    restored from config byte at offset 0x17 (shifted from word[5]).
 *
 * =============================================================================
 */
