/*
 * OpenScope 2C53T - FPGA SPI Activation Scanner
 *
 * Brute-force sweep of SPI parameters and USART commands to discover
 * what activates the FPGA's SPI data output (MISO on PB4).
 *
 * Runs autonomously — takes over the display, shows progress,
 * logs any hits where the FPGA responds with non-0xFF data or
 * any GPIO pin toggles during SPI transfer.
 *
 * Designed to run for hours if needed (plugged in via USB).
 */

#ifndef FPGA_SCANNER_H
#define FPGA_SCANNER_H

#include <stdint.h>

/* Maximum hits to record before stopping */
#define SCANNER_MAX_HITS  32

/* A single hit record */
typedef struct {
    uint8_t  tier;         /* Which tier found this */
    uint8_t  spi_mode;     /* SPI mode 0-3 */
    uint8_t  first_byte;   /* First MOSI byte sent */
    uint8_t  cmd_hi;       /* USART cmd_hi (0 if no USART cmd) */
    uint8_t  cmd_lo;       /* USART cmd_lo (0 if no USART cmd) */
    uint8_t  response[4];  /* First 4 SPI response bytes */
    uint32_t gpio_delta;   /* OR of all port deltas (packed: A[7:0]|B[7:0]|C[7:0]|D[7:0]) */
} scanner_hit_t;

/*
 * Run the full FPGA SPI activation scan.
 * Takes over the display. Returns when complete or user presses POWER.
 * Call from settings menu handler (runs in display task context).
 */
void fpga_scanner_run(void);

#endif /* FPGA_SCANNER_H */
