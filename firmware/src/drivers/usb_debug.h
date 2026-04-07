/*
 * OpenScope 2C53T - USB Debug Shell
 *
 * USB CDC virtual serial port for interactive FPGA exploration.
 * Connect via: screen /dev/tty.usbmodem* 115200
 *
 * Commands:
 *   help                           - List available commands
 *   version                        - Firmware version info
 *   status                         - FPGA and system status
 *   usart tx <hex bytes>           - Send raw USART2 frame to FPGA
 *   gpio set <port><pin> <0|1>     - Set GPIO pin state
 *   gpio read <port><pin>          - Read GPIO pin state
 *   gpio scan                      - Scan all FPGA-related pins
 *   mem read <hex_addr> [count]    - Read memory/registers (32-bit)
 *   mem write <hex_addr> <hex_val> - Write memory/register (32-bit)
 *   fpga cmd <cmd> [param]         - Send FPGA command (decimal)
 *   fpga acq [mode]                - Trigger SPI3 acquisition
 *   spi3 read [len]                - Raw SPI3 read and hex dump
 */

#ifndef USB_DEBUG_H
#define USB_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Initialize USB CDC hardware:
 *   - Configure HICK 48MHz clock for USB (ACC calibrated)
 *   - Enable USB peripheral clock
 *   - Initialize USB device core with CDC class
 *   - Enable USB interrupt
 *
 * Call after system_clock_config() and before FreeRTOS scheduler start.
 */
void usb_debug_init(void);

/*
 * Create the USB debug shell FreeRTOS task.
 * Call after usb_debug_init() and before vTaskStartScheduler().
 */
void usb_debug_create_task(void);

/*
 * Send a formatted string to the USB debug console.
 * Safe to call from any task context. Max 256 chars per call.
 * Returns number of bytes queued, 0 if USB not connected.
 */
int usb_debug_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Check if USB debug console is connected and configured.
 */
bool usb_debug_connected(void);

#endif /* USB_DEBUG_H */
