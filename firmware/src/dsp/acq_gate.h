/*
 * OpenScope 2C53T - is an acquisition frame live data or an idle bus?
 *
 * An undriven SPI3 MISO is pulled up (PB4 input pull-up, stock's idle level,
 * kept after the bit-bang config since 2026-11-27), so a dead bus reads 0xFF
 * in every byte, and after the -28 ADC offset its samples are a flat 227. A
 * railed ADC (an input above the top of the range) produces the same flat
 * 227 in the samples, so the samples alone cannot tell the two apart. Until
 * 2026-11-26 the acquisition loop accepted a frame only if it carried the
 * data-ready marker or CH1 varied, so a clipped input was taken for a dead
 * bus: nothing was committed and the screen kept the last unclipped trace
 * (EXP-82, my unit: range 4, 1.5 V DC from GEN).
 *
 * The byte MISO carries while the opcode goes out is driven by a configured
 * FPGA (0x00, or 0x80 with the data-ready marker) and is 0xFF on a dead bus,
 * like every other byte. So a frame is live when any of:
 *   - either read carried the data-ready marker (0x80),
 *   - either channel's samples vary (a dead bus cannot make them vary),
 *   - neither read's opcode-phase byte is 0xFF, and the opcode phase is
 *     trusted (below).
 * A dead bus fails all three. A frame with a poll expiry inside it is
 * rejected separately by the caller (its bytes are partly fabricated 0xFF).
 *
 * Why the third term needs trust: a driven opcode-phase byte does not by
 * itself mean a configured capture engine. A part whose configuration
 * failed keeps its SSPI config port open and drives MISO the whole time:
 * EXP-31's wire capture (2026-08-26) shows 0x04/0x05 on an open port
 * returning 00 01 C8 10 ..., a 0x00 opcode-phase byte followed by the STATUS
 * register streaming. Trusted alone, the term would commit an unconfigured
 * part's frames and latch off the demo trace (review of #109).
 *
 * The opcode phase is trusted only when this boot's config sequence read the
 * IDCODE after the upload and it did NOT answer. A configured part closes its
 * config port (Exp L), on a cold boot (STATUS 0x0003F460) and on a warm one
 * alike: after a CDC flash or pinhole reset the part is still configured
 * from the previous boot, and our own STATUS read then returns 00000000 (my
 * unit, 2026-11-27), so DONE_FINAL cannot be the test. A failed config
 * leaves the port answering 0x0120681B (Exp M). A silent IDCODE is also what
 * a dead bus gives, but a dead bus reads 0xFF in the opcode phase and fails
 * the term anyway. Builds that run no config sequence, or a variant that
 * skips the IDCODE read, never trust it and keep the pre-EXP-82 gate.
 */

#ifndef ACQ_GATE_H
#define ACQ_GATE_H

#include <stdbool.h>
#include <stdint.h>

/* anchor_read: the IDCODE-after-upload read ran this boot.
 * id_bit: where it found 0x0120681B, or -1 if it did not answer. */
static inline bool acq_opphase_trusted(bool anchor_read, int id_bit)
{
    return anchor_read && id_bit < 0;
}

static inline bool acq_samples_vary(const volatile uint8_t *b, int n)
{
    for (int i = 1; i < n; i++)
        if (b[i] != b[0])
            return true;
    return false;
}

static inline bool acq_frame_live(uint8_t s1, uint8_t s2,
                                  const volatile uint8_t *ch1,
                                  const volatile uint8_t *ch2, int n,
                                  bool opphase_trusted)
{
    if (s1 == 0x80 || s2 == 0x80)
        return true;
    if (acq_samples_vary(ch1, n) || acq_samples_vary(ch2, n))
        return true;
    return opphase_trusted && s1 != 0xFF && s2 != 0xFF;
}

#endif /* ACQ_GATE_H */
