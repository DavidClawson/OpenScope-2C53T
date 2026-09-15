/*
 * fft_live.c — the live acquisition record as spectrum input. See fft_live.h.
 */
#include "fft_live.h"

uint16_t fft_live_prepare(const uint8_t *rec, uint16_t n,
                          int16_t *out, uint16_t out_cap)
{
    if (rec == 0 || out == 0 || n <= FFT_LIVE_HEAD_SKIP || out_cap == 0)
        return 0;

    uint16_t count = (uint16_t)(n - FFT_LIVE_HEAD_SKIP);
    if (count > out_cap)
        count = out_cap;

    for (uint16_t i = 0; i < count; i++)
        out[i] = (int16_t)(((int16_t)rec[FFT_LIVE_HEAD_SKIP + i] - 128)
                           * FFT_LIVE_GAIN);
    return count;
}

bool fft_live_axis_known(float sample_rate_hz)
{
    return sample_rate_hz > 0.0f;
}

float fft_live_bin_hz(float sample_rate_hz, uint16_t fft_size, uint16_t bin)
{
    if (!fft_live_axis_known(sample_rate_hz) || fft_size == 0)
        return 0.0f;
    return (float)bin * sample_rate_hz / (float)fft_size;
}

void fft_live_format_hz(float hz, char *buf, int bufsize)
{
    const char *unit;
    float val;

    if (bufsize <= 0)
        return;
    if (hz < 0.0f)
        hz = 0.0f;

    if (hz >= 1000000.0f) {
        val = hz / 1000000.0f;
        unit = "MHz";
    } else if (hz >= 1000.0f) {
        val = hz / 1000.0f;
        unit = "kHz";
    } else {
        val = hz;
        unit = "Hz";
    }

    int integer = (int)val;
    int frac = (int)((val - (float)integer) * 10.0f);
    if (frac < 0) frac = -frac;

    int pos = 0;
    if (integer >= 100 && pos < bufsize - 1) buf[pos++] = (char)('0' + integer / 100);
    if (integer >= 10  && pos < bufsize - 1) buf[pos++] = (char)('0' + (integer / 10) % 10);
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + integer % 10);
    if (pos < bufsize - 1) buf[pos++] = '.';
    if (pos < bufsize - 1) buf[pos++] = (char)('0' + frac);

    while (*unit && pos < bufsize - 1)
        buf[pos++] = *unit++;
    buf[pos] = '\0';
}

void fft_live_axis_label(float sample_rate_hz, uint16_t fft_size,
                         uint16_t bin, char *buf, int bufsize)
{
    if (bufsize <= 0)
        return;
    if (!fft_live_axis_known(sample_rate_hz) || fft_size == 0) {
        int pos = 0;
        if (pos < bufsize - 1) buf[pos++] = '-';
        if (pos < bufsize - 1) buf[pos++] = '-';
        buf[pos] = '\0';
        return;
    }
    fft_live_format_hz(fft_live_bin_hz(sample_rate_hz, fft_size, bin),
                       buf, bufsize);
}
