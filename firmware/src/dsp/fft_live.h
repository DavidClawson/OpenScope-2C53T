/*
 * fft_live.h — the live acquisition record as spectrum input.
 *
 * Everything here is pure (no LCD, no RTOS, no FPGA driver) so the same code
 * that feeds the spectrum on the device runs under tests/test_fft_live.c on
 * the host. The UI supplies the record and the sample rate; this module
 * decides which samples are analysed and what the frequency axis may claim.
 *
 * Refusal discipline (same as scope_freq / the Freq badge): a bin index is
 * always exact; a figure in hertz is only produced when the sample rate is
 * a measured one (> 0). fft_live_axis_known() is the single test for that.
 */
#ifndef FFT_LIVE_H
#define FFT_LIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "scope_record.h"

/* First record index the spectrum analyses — see scope_record.h. */
#define FFT_LIVE_HEAD_SKIP  SCOPE_RECORD_HEAD_SKIP

/* Numeric gain applied when widening 8-bit samples to int16: a constant,
 * so it moves every bin by the same dB and cannot change the spectrum's
 * shape. 128 = <<7, leaving one bit of headroom in int16. */
#define FFT_LIVE_GAIN       128

/* Copy the valid body of an unsigned 8-bit record into a signed int16
 * buffer: out[i] = (rec[HEAD_SKIP + i] - 128) * FFT_LIVE_GAIN.
 * Returns the number of samples written (0 if the record is no longer than
 * the head, or out_cap is 0); never writes more than out_cap. */
uint16_t fft_live_prepare(const uint8_t *rec, uint16_t n,
                          int16_t *out, uint16_t out_cap);

/* True when a frequency axis may be labelled in hertz. */
bool fft_live_axis_known(float sample_rate_hz);

/* Centre frequency of `bin` for a transform of `fft_size` points, or 0.0f
 * when the axis is not known. Callers must not print 0 as a frequency;
 * check fft_live_axis_known() first. */
float fft_live_bin_hz(float sample_rate_hz, uint16_t fft_size, uint16_t bin);

/* "123.4Hz" / "12.3kHz" / "1.2MHz" — one decimal, no float printf needed
 * (the firmware links newlib-nano without float formatting). */
void fft_live_format_hz(float hz, char *buf, int bufsize);

/* Axis label for `bin`: hertz when the axis is known, "--" when it is not.
 * Exactly the string the screen shows, so the test pins it. */
void fft_live_axis_label(float sample_rate_hz, uint16_t fft_size,
                         uint16_t bin, char *buf, int bufsize);

#endif /* FFT_LIVE_H */
