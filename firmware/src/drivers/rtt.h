/*
 * OpenScope 2C53T - RTT debug transport
 *
 * Bidirectional console over the SWD wires that are already soldered to the
 * bench unit. Replaces the USB CDC transport, which has never enumerated on
 * that unit (see CLAUDE.local.md).
 *
 * This is a clean-room implementation of the RTT control-block layout that
 * OpenOCD's `rtt` command searches for. It is NOT SEGGER's source: SEGGER's
 * RTT implementation is licensed for use with J-Link probes only, and this
 * project is GPLv3 and drives an ST-Link. Only the wire format is shared,
 * which is what makes the host tooling work.
 *
 * Host side:
 *   scripts/rtt_shell.sh          # opens a telnet console on :9090
 *
 * The control block lives in SRAM and is located by OpenOCD scanning for the
 * ID string, so no address needs to be kept in sync with the linker script.
 */

#ifndef RTT_H
#define RTT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Build the control block and arm the ID string. Safe to call before the
 * FreeRTOS scheduler starts; call it early in main() so boot-time output is
 * captured.
 */
void rtt_init(void);

/*
 * Write bytes to the up (target -> host) buffer.
 *
 * If a host is attached and the buffer is full, blocks for up to
 * RTT_TX_TIMEOUT_MS waiting for drain, then drops the remainder. If no host
 * is attached, returns immediately without blocking, so an unattached device
 * never stalls on debug output.
 *
 * Returns the number of bytes actually written.
 */
size_t rtt_write(const void *data, size_t len);

/* Convenience wrapper around rtt_write() for NUL-terminated strings. */
size_t rtt_puts(const char *s);

/*
 * Read up to len bytes from the down (host -> target) buffer.
 * Never blocks. Returns the number of bytes read, 0 if nothing is pending.
 */
size_t rtt_read(void *data, size_t len);

/*
 * True once the host has moved the up-buffer read pointer, i.e. something is
 * actually consuming output. Used to decide whether a full buffer is worth
 * waiting on.
 */
bool rtt_host_attached(void);

#endif /* RTT_H */
