/*
 * OpenScope 2C53T - Battery Voltage Monitor
 *
 * PB1 / ADC1 Channel 9 through a resistor divider on the PCB.
 *
 * Design:
 *   - battery_update() called once per second from timer callback
 *   - Takes ONE ADC reading per call
 *   - Charging detection: 5 consecutive readings >4300mV = charging
 *   - Battery average: only non-charging readings, 16-sample window
 *   - battery_percent() and battery_is_charging() return cached state
 */

#include "battery.h"
#include "at32f403a_407.h"

#define DIVIDER_MV_PER_COUNT  1611  /* 3.3V ref, 12-bit ADC, ~2:1 divider */

#define CHARGE_THRESHOLD_MV   4300  /* above this = USB power detected */
#define CHARGE_DEBOUNCE       5     /* consecutive readings to confirm */
#define DISCHARGE_DEBOUNCE    5     /* consecutive readings to confirm */

#define BAT_AVG_SIZE          16

static volatile uint8_t adc_ready = 0;

/* Rolling average buffer — only battery (non-charging) readings */
static uint16_t avg_buf[BAT_AVG_SIZE];
static uint8_t avg_idx = 0;
static uint8_t avg_count = 0;

/* Cached output state */
static uint8_t cached_percent = 0;
static uint8_t cached_charging = 0;
static uint8_t critical_count = 0;  /* consecutive readings below 3.3V */
static uint8_t cached_critical = 0;

/* Charge detection state */
static uint8_t charge_count = 0;     /* consecutive readings above threshold */
static uint8_t discharge_count = 0;  /* consecutive readings below threshold */

void battery_adc_init(void)
{
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
    crm_adc_clock_div_set(CRM_ADC_DIV_6);

    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_1;
    gpio_cfg.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init(GPIOB, &gpio_cfg);

    adc_base_config_type adc_cfg;
    adc_base_default_para_init(&adc_cfg);
    adc_cfg.sequence_mode = FALSE;
    adc_cfg.repeat_mode = FALSE;
    adc_cfg.data_align = ADC_RIGHT_ALIGNMENT;
    adc_cfg.ordinary_channel_length = 1;
    adc_base_config(ADC1, &adc_cfg);

    adc_ordinary_channel_set(ADC1, ADC_CHANNEL_9, 1, ADC_SAMPLETIME_239_5);
    adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_SOFTWARE, TRUE);
    adc_enable(ADC1, TRUE);

    /* Calibration with timeout */
    adc_calibration_init(ADC1);
    for (volatile uint32_t t = 0; t < 100000; t++)
        if (!adc_calibration_init_status_get(ADC1)) break;
    adc_calibration_start(ADC1);
    for (volatile uint32_t t = 0; t < 100000; t++)
        if (!adc_calibration_status_get(ADC1)) break;

    adc_ready = 1;

    /* Prime with initial readings */
    for (int i = 0; i < BAT_AVG_SIZE; i++)
        battery_update();
}

static uint16_t last_good_mv = 3700;  /* default until first good reading */

static uint16_t adc_read_mv_once(void)
{
    adc_ordinary_software_trigger_enable(ADC1, TRUE);
    for (volatile uint32_t t = 0; t < 240000; t++) {
        if (adc_flag_get(ADC1, ADC_CCE_FLAG) != RESET) {
            adc_flag_clear(ADC1, ADC_CCE_FLAG);
            uint16_t raw = adc_ordinary_conversion_data_get(ADC1);
            uint16_t mv = (uint16_t)((uint32_t)raw * DIVIDER_MV_PER_COUNT / 1000);
            /* Sanity check: 1S LiPo + USB range is ~3000-5500mV */
            if (mv >= 2500 && mv <= 6000) {
                last_good_mv = mv;
                return mv;
            }
            return last_good_mv;  /* out of range — use last good */
        }
    }
    return last_good_mv;  /* timeout — use last good */
}

static uint8_t mv_to_percent(uint16_t mv)
{
    /* Piecewise linear 1S LiPo discharge curve */
    if (mv >= 4200) return 100;
    if (mv >= 4100) return 90 + (mv - 4100) * 10 / 100;
    if (mv >= 4000) return 80 + (mv - 4000) * 10 / 100;
    if (mv >= 3900) return 60 + (mv - 3900) * 20 / 100;
    if (mv >= 3800) return 40 + (mv - 3800) * 20 / 100;
    if (mv >= 3700) return 20 + (mv - 3700) * 20 / 100;
    if (mv >= 3600) return 10 + (mv - 3600) * 10 / 100;
    if (mv >= 3300) return (mv - 3300) * 10 / 300;
    return 0;
}

void battery_update(void)
{
    if (!adc_ready) return;

    uint16_t mv = adc_read_mv_once();

    /* Charge state detection with debounce */
    if (mv > CHARGE_THRESHOLD_MV) {
        discharge_count = 0;
        if (charge_count < CHARGE_DEBOUNCE)
            charge_count++;
        if (charge_count >= CHARGE_DEBOUNCE)
            cached_charging = 1;
    } else {
        charge_count = 0;
        if (discharge_count < DISCHARGE_DEBOUNCE)
            discharge_count++;
        if (discharge_count >= DISCHARGE_DEBOUNCE)
            cached_charging = 0;

        /* Only add non-charging readings to the battery average */
        avg_buf[avg_idx] = mv;
        avg_idx = (avg_idx + 1) % BAT_AVG_SIZE;
        if (avg_count < BAT_AVG_SIZE) avg_count++;
    }

    /* Critical voltage detection (debounced — 10 consecutive readings) */
    if (mv < 3300 && !cached_charging) {
        if (critical_count < 10) critical_count++;
        if (critical_count >= 10) cached_critical = 1;
    } else {
        critical_count = 0;
        cached_critical = 0;
    }

    /* Update cached percentage from averaged battery readings */
    if (avg_count > 0) {
        uint32_t sum = 0;
        for (int i = 0; i < avg_count; i++)
            sum += avg_buf[i];
        cached_percent = mv_to_percent((uint16_t)(sum / avg_count));
    }
}

uint16_t battery_read_mv(void)
{
    /* Return averaged millivolts from cached state */
    if (avg_count == 0) return 0;
    uint32_t sum = 0;
    for (int i = 0; i < avg_count; i++)
        sum += avg_buf[i];
    return (uint16_t)(sum / avg_count);
}

uint8_t battery_percent(void)
{
    return cached_percent;
}

uint8_t battery_is_critical(void)
{
    return cached_critical;
}

uint8_t battery_is_charging(void)
{
    return cached_charging;
}
