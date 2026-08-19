/*
 * OpenScope 2C53T — frequency measurement from a capture record
 *
 * See scope_freq.h for the evidence behind every threshold here, and for why
 * this carries its own transform instead of calling CMSIS-DSP.
 *
 * Free of hardware and RTOS dependencies so the host tests can drive it with
 * real bench records — see tests/test_scope_freq.c.
 */

#include "scope_freq.h"

#include <math.h>
#include <string.h>

/* Scratch. ~12 KB of .bss against 224 KB of SRAM. Not reentrant: one caller,
 * the display task. */
static float fft_re[SCOPE_FREQ_MAX_N];
static float fft_im[SCOPE_FREQ_MAX_N];
static float hann[SCOPE_FREQ_MAX_N];
static uint16_t hann_n;                     /* length `hann` was built for */

static void build_hann(uint16_t n)
{
    if (hann_n == n)
        return;
    for (uint16_t i = 0; i < n; i++)
        hann[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i /
                                     (float)(n - 1u));
    hann_n = n;
}

/* In-place radix-2 decimation-in-time FFT. n must be a power of two. */
static void fft(uint16_t n)
{
    /* bit-reversal permutation */
    for (uint16_t i = 1u, j = 0u; i < n; i++) {
        uint16_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            float t;
            t = fft_re[i]; fft_re[i] = fft_re[j]; fft_re[j] = t;
            t = fft_im[i]; fft_im[i] = fft_im[j]; fft_im[j] = t;
        }
    }

    for (uint16_t len = 2u; len <= n; len <<= 1) {
        const float ang = -2.0f * (float)M_PI / (float)len;
        const float wr = cosf(ang), wi = sinf(ang);
        for (uint16_t i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (uint16_t k = 0; k < (len >> 1); k++) {
                const uint16_t a = i + k, b = a + (len >> 1);
                const float xr = fft_re[b] * cr - fft_im[b] * ci;
                const float xi = fft_re[b] * ci + fft_im[b] * cr;
                fft_re[b] = fft_re[a] - xr;
                fft_im[b] = fft_im[a] - xi;
                fft_re[a] += xr;
                fft_im[a] += xi;
                const float nr = cr * wr - ci * wi;   /* advance the twiddle */
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

/*
 * One pass over `n` samples. Fills bin/sharpness; returns false if the record
 * has no usable peak at all (flat, or a peak at an edge bin).
 *
 * `bin` comes back in units of THIS window's bins; the caller scales it to
 * full-record bins so the min-bin rule means one thing.
 */
static bool analyse(const uint8_t *s, uint16_t n, float *bin, float *sharp)
{
    *bin = 0.0f;
    *sharp = 0.0f;

    /* Mean-remove in the same pass that copies, so a large DC pedestal does
     * not eat the window's dynamic range. */
    float sum = 0.0f;
    for (uint16_t i = 0; i < n; i++)
        sum += (float)s[i];
    const float mean = sum / (float)n;

    build_hann(n);
    for (uint16_t i = 0; i < n; i++) {
        fft_re[i] = ((float)s[i] - mean) * hann[i];
        fft_im[i] = 0.0f;
    }

    fft(n);

    /* Power spectrum, DC discarded: DC is not a signal and including it would
     * let a pedestal dominate the sharpness figure. */
    const uint16_t half = (uint16_t)(n >> 1);
    float total = 0.0f;
    uint16_t peak = 0;
    float peak_p = 0.0f;
    for (uint16_t k = 1u; k < half; k++) {
        const float p = fft_re[k] * fft_re[k] + fft_im[k] * fft_im[k];
        fft_re[k] = p;                       /* reuse: power in place */
        total += p;
        if (p > peak_p) { peak_p = p; peak = k; }
    }

    if (total <= 0.0f || peak == 0u || peak >= (uint16_t)(half - 1u))
        return false;

    *sharp = (fft_re[peak - 1u] + fft_re[peak] + fft_re[peak + 1u]) / total;

    /* Parabolic interpolation on MAGNITUDES, which is where the standard
     * three-point vertex formula is unbiased for a windowed sinusoid. */
    const float a = sqrtf(fft_re[peak - 1u]);
    const float b = sqrtf(fft_re[peak]);
    const float c = sqrtf(fft_re[peak + 1u]);
    const float den = a - 2.0f * b + c;
    const float frac = (den != 0.0f) ? (0.5f * (a - c) / den) : 0.0f;

    *bin = (float)peak + frac;
    return true;
}

bool scope_freq_estimate(const uint8_t *samples, uint16_t n,
                         float sample_rate_hz, scope_freq_t *out)
{
    if (out == NULL)
        return false;

    memset(out, 0, sizeof(*out));

    if (samples == NULL || n < 64u || n > SCOPE_FREQ_MAX_N ||
        (n & (uint16_t)(n - 1u)) != 0u)
        return false;

    /* A timebase code with no trustworthy rate has no frequency either.
     * Refusing here is the whole contract — see scope_timebase.h. */
    if (!(sample_rate_hz > 0.0f))
        return false;

    float bin = 0.0f, sharp = 0.0f;
    uint16_t win = n;
    const bool got = analyse(samples, n, &bin, &sharp);

    /*
     * Retry on the LAST HALF when the full record is not sharp.
     *
     * Records are torn at random and the tail is the fresher end of a rolling
     * buffer, so the second half is usually clean when the whole is not — one
     * bench record scored 0.380 whole and 0.999 over its last 512. The cost is
     * half the frequency resolution, which is why this is a fallback and not
     * the default: at a fast timebase a low tone needs every bin it can get.
     */
    if (!got || sharp < SCOPE_FREQ_MIN_SHARP) {
        float bin2 = 0.0f, sharp2 = 0.0f;
        const uint16_t h = (uint16_t)(n >> 1);
        if (h >= 64u && analyse(samples + h, h, &bin2, &sharp2) &&
            sharp2 > sharp) {
            /* Express in full-record bins so MIN_BIN means one thing. */
            bin = bin2 * 2.0f;
            sharp = sharp2;
            win = h;
        } else if (!got) {
            return false;
        }
    }

    out->bin = bin;
    out->sharpness = sharp;
    out->window = win;

    if (sharp < SCOPE_FREQ_MIN_SHARP || bin < SCOPE_FREQ_MIN_BIN)
        return false;                        /* hz stays 0.0f */

    out->hz = bin * sample_rate_hz / (float)n;
    return true;
}
