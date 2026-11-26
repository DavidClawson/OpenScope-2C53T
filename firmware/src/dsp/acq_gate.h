/*
 * OpenScope 2C53T - is an acquisition frame live data or an idle bus?
 *
 * An undriven SPI3 MISO is pulled up, so a dead bus reads 0xFF in every
 * byte, and after the -28 ADC offset its samples are a flat 227. A railed
 * ADC (an input above the top of the range) produces the same flat 227 in
 * the samples, so the samples alone cannot tell the two apart. Until
 * 2026-11-26 the acquisition loop accepted a frame only if it carried the
 * data-ready marker or CH1 varied, so a clipped input was taken for a dead
 * bus: nothing was committed and the screen kept the last unclipped trace
 * (EXP-82, my unit: range 4, 1.5 V DC from GEN).
 *
 * The byte MISO carries while the opcode goes out is driven by a live FPGA
 * (0x00, or 0x80 with the data-ready marker) and is 0xFF on a dead bus, like
 * every other byte. So a frame is live when any of:
 *   - either read carried the data-ready marker (0x80),
 *   - either channel's samples vary (a dead bus cannot make them vary),
 *   - neither read's opcode-phase byte is 0xFF.
 * Every term only adds frames a live FPGA produced; a dead bus fails all
 * three. A frame with a poll expiry inside it is rejected separately by the
 * caller (its bytes are partly fabricated 0xFF).
 */

#ifndef ACQ_GATE_H
#define ACQ_GATE_H

#include <stdbool.h>
#include <stdint.h>

static inline bool acq_samples_vary(const volatile uint8_t *b, int n)
{
    for (int i = 1; i < n; i++)
        if (b[i] != b[0])
            return true;
    return false;
}

static inline bool acq_frame_live(uint8_t s1, uint8_t s2,
                                  const volatile uint8_t *ch1,
                                  const volatile uint8_t *ch2, int n)
{
    if (s1 == 0x80 || s2 == 0x80)
        return true;
    if (acq_samples_vary(ch1, n) || acq_samples_vary(ch2, n))
        return true;
    return s1 != 0xFF && s2 != 0xFF;
}

#endif /* ACQ_GATE_H */
