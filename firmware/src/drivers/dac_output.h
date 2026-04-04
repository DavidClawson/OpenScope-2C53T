/*
 * OpenScope 2C53T - DAC Output Driver
 *
 * Drives the AT32F403A's 12-bit DAC1 (PA4) for signal generator output.
 * Uses TMR6 as trigger source and DMA2 Channel 3 for zero-CPU-overhead
 * continuous waveform output from a circular buffer.
 *
 * Architecture:
 *   siggen_fill_buffer() → dac_buffer[256] → DMA2 CH3 → DAC1 → PA4
 *                                              ↑
 *                                         TMR6 TRGO trigger
 */

#ifndef DAC_OUTPUT_H
#define DAC_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Waveform buffer size — one complete cycle.
 * Output frequency = sample_rate / DAC_BUFFER_SIZE */
#define DAC_BUFFER_SIZE  256

/*
 * Initialize DAC1 hardware:
 *   - PA4 as analog output
 *   - DAC1 with TMR6 trigger
 *   - DMA2 Channel 3 in circular mode
 *   - TMR6 configured but not started
 *
 * Call once during system init, before FreeRTOS scheduler.
 */
void dac_output_init(void);

/*
 * Start continuous DAC output from the waveform buffer.
 * sample_rate: DAC conversion rate in Hz (e.g., 256000 for 1kHz sine)
 *
 * The buffer must be filled with 12-bit unsigned values (0-4095)
 * before calling this. DMA runs in circular mode — the buffer is
 * output repeatedly until dac_output_stop() is called.
 */
void dac_output_start(uint32_t sample_rate);

/*
 * Stop DAC output. Disables TMR6 and sets DAC output to mid-scale.
 */
void dac_output_stop(void);

/*
 * Update the sample rate without stopping output.
 * Use when frequency changes but waveform shape stays the same.
 */
void dac_output_set_rate(uint32_t sample_rate);

/*
 * Get pointer to the waveform buffer for filling.
 * Buffer holds DAC_BUFFER_SIZE uint16_t values (12-bit, 0-4095).
 */
uint16_t *dac_output_get_buffer(void);

/*
 * Check if DAC output is currently active.
 */
bool dac_output_is_running(void);

#endif /* DAC_OUTPUT_H */
