/*
 * test_fft_live.c — the live acquisition record as spectrum input.
 *
 * Host half of the fft-live spec (S1 by code, S3 regression shape). What
 * it pins, each with a negative control where one is possible:
 *
 *   1. fft_live_prepare() skips the invalid head: out[0] is record[128],
 *      and a record no longer than the head yields 0 samples.
 *   2. bin -> Hz only from a known rate: fft_live_bin_hz(12490, 4096, 328)
 *      = 1000.1 Hz; at rate 0 it is 0 and the label is "--".
 *   3. A synthetic 1 kHz tone in an 8-bit record with a garbage head lands
 *      its peak in bin 328 ± 1 at fs = 12,490 S/s (the measured 0x10 rate)
 *      and is reported within one bin of 1000 Hz. NEGATIVE CONTROL: with
 *      the rate set to the 0x0F figure (24,979) the same record must report
 *      a frequency more than 5 bins from 1 kHz — a wrong fs must fail.
 *   4. The window spans the record, not the transform: an impulse at
 *      index k and one at index count-1-k produce the same magnitude
 *      (symmetric window). NEGATIVE CONTROL: the two magnitudes computed
 *      with the pre-fix quarter-window taper differ by > 6 dB.
 *   5. At rate 0, fft_process() reports bin_width_hz = 0 and every peak
 *      freq_hz = 0, but harmonic labels (bin ratios) survive: a square wave
 *      still gets "Fund" and "H3".
 *
 * Build: make test-fft-live
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fft.h"
#include "fft_live.h"
#include "scope_record.h"

#define FS_0X10   12490.0f    /* scope_timebase.c, measured (EXP-17) */
#define FS_0X0F   24979.1f    /* the neighbouring code — the wrong rate */
#define REC_N     1024

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, ...) do {                                   \
    if (cond) { tests_passed++; printf("  PASS: " __VA_ARGS__); } \
    else      { tests_failed++; printf("  FAIL: " __VA_ARGS__); } \
    printf("\n");                                                \
} while (0)

static uint8_t  rec[REC_N];
static int16_t  sbuf[FFT_SIZE];
static fft_result_t res;

static void make_tone_record(float f_hz, float fs, uint8_t head_fill)
{
    for (int i = 0; i < REC_N; i++) {
        if (i < (int)SCOPE_RECORD_HEAD_SKIP) {
            rec[i] = head_fill;                        /* the invalid head */
        } else {
            float v = 128.0f + 100.0f * sinf(2.0f * (float)M_PI * f_hz * (float)i / fs);
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            rec[i] = (uint8_t)(v + 0.5f);
        }
    }
}

static void init_fft(fft_window_t win, float fs)
{
    fft_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.window         = win;
    cfg.sample_rate_hz = fs;
    cfg.ref_level_db   = 0.0f;
    cfg.db_range       = 80.0f;
    cfg.peak_count     = 4;
    cfg.zoom_start_bin = 1;
    cfg.zoom_end_bin   = FFT_BINS - 1;
    fft_init(&cfg);
}

/* ── 1. head skip ─────────────────────────────────────────────────── */
static void test_head_skip(void)
{
    printf("\n[1] head skip\n");
    make_tone_record(1000.0f, FS_0X10, 0xFF);
    uint16_t n = fft_live_prepare(rec, REC_N, sbuf, FFT_SIZE);
    CHECK(n == REC_N - SCOPE_RECORD_HEAD_SKIP,
          "prepare yields %u samples (record minus head)", (unsigned)n);
    CHECK(sbuf[0] == (int16_t)(((int16_t)rec[SCOPE_RECORD_HEAD_SKIP] - 128) * FFT_LIVE_GAIN),
          "out[0] is record[%u], widened", (unsigned)SCOPE_RECORD_HEAD_SKIP);
    int touched_head = 0;
    for (uint16_t i = 0; i < n; i++)
        if (sbuf[i] == (int16_t)((0xFF - 128) * FFT_LIVE_GAIN)) touched_head++;
    CHECK(touched_head == 0, "no head sample (0xFF) reaches the transform input");
    CHECK(fft_live_prepare(rec, SCOPE_RECORD_HEAD_SKIP, sbuf, FFT_SIZE) == 0,
          "a record no longer than the head yields 0 samples");
    CHECK(fft_live_prepare(rec, REC_N, sbuf, 10) == 10,
          "output capacity is honoured");
}

/* ── 2. bin -> Hz and refusal ─────────────────────────────────────── */
static void test_bin_hz(void)
{
    printf("\n[2] bin -> Hz mapping and refusal\n");
    float hz = fft_live_bin_hz(FS_0X10, FFT_SIZE, 328);
    CHECK(fabsf(hz - 1000.1f) < 0.2f, "bin 328 at 12,490 S/s = %.1f Hz", hz);
    CHECK(fft_live_axis_known(FS_0X10), "axis known at a measured rate");
    CHECK(!fft_live_axis_known(0.0f), "axis NOT known at rate 0");
    CHECK(fft_live_bin_hz(0.0f, FFT_SIZE, 328) == 0.0f, "bin_hz at rate 0 is 0, not a number");

    char lbl[12];
    fft_live_axis_label(0.0f, FFT_SIZE, 328, lbl, sizeof(lbl));
    CHECK(strcmp(lbl, "--") == 0, "axis label at rate 0 is \"--\" (got \"%s\")", lbl);
    fft_live_axis_label(FS_0X10, FFT_SIZE, 328, lbl, sizeof(lbl));
    CHECK(strcmp(lbl, "1.0kHz") == 0, "axis label at 12,490 S/s is \"1.0kHz\" (got \"%s\")", lbl);
    fft_live_format_hz(999.9f, lbl, sizeof(lbl));
    CHECK(strcmp(lbl, "999.9Hz") == 0, "format 999.9 -> \"%s\"", lbl);
    fft_live_format_hz(2500000.0f, lbl, sizeof(lbl));
    CHECK(strcmp(lbl, "2.5MHz") == 0, "format 2.5e6 -> \"%s\"", lbl);
}

/* ── 3. tone lands in the predicted bin; wrong fs fails ──────────── */
static void test_tone_bin(void)
{
    printf("\n[3] 1 kHz tone at the 0x10 rate\n");
    make_tone_record(1000.0f, FS_0X10, 0x00);
    uint16_t n = fft_live_prepare(rec, REC_N, sbuf, FFT_SIZE);

    init_fft(FFT_WINDOW_HANNING, FS_0X10);
    fft_process(sbuf, n, &res);
    uint16_t expect_bin = (uint16_t)(1000.0f / (FS_0X10 / (float)FFT_SIZE) + 0.5f);
    CHECK(res.num_peaks > 0, "a peak was found");
    CHECK(res.num_peaks > 0 && abs((int)res.peaks[0].bin - (int)expect_bin) <= 1,
          "peak bin %u within 1 of predicted %u", (unsigned)res.peaks[0].bin, (unsigned)expect_bin);
    CHECK(fabsf(res.peak_freq_hz - 1000.0f) <= res.bin_width_hz,
          "peak reported at %.1f Hz, within one bin (%.2f Hz) of 1000",
          res.peak_freq_hz, res.bin_width_hz);

    /* Negative control: the wrong rate must produce the wrong answer. */
    fft_set_sample_rate(FS_0X0F);
    fft_process(sbuf, n, &res);
    CHECK(fabsf(res.peak_freq_hz - 1000.0f) > 5.0f * res.bin_width_hz,
          "NEGATIVE CONTROL: at the 0x0F rate the same record reports %.1f Hz (must not be 1000)",
          res.peak_freq_hz);
    fft_deinit();
}

/* ── 4. the window spans the record ──────────────────────────────── */
static float impulse_mag_db(uint16_t at, uint16_t count, uint16_t bin)
{
    memset(sbuf, 0, sizeof(sbuf));
    sbuf[at] = 20000;
    fft_process(sbuf, count, &res);
    return res.level_db[bin];
}

static void test_window_spans_record(void)
{
    printf("\n[4] window spans the record, not the transform\n");
    const uint16_t count = REC_N - SCOPE_RECORD_HEAD_SKIP;   /* 896 */
    const uint16_t k = 40;
    init_fft(FFT_WINDOW_HANNING, FS_0X10);

    float a = impulse_mag_db(k, count, 100);
    float b = impulse_mag_db((uint16_t)(count - 1 - k), count, 100);
    CHECK(fabsf(a - b) < 0.5f,
          "impulse at %u and at %u see the same window weight (%.2f vs %.2f dB)",
          (unsigned)k, (unsigned)(count - 1 - k), a, b);

    /* Negative control: the pre-fix behaviour applied window_coeffs[i]
     * directly — the first count/FFT_SIZE of a 4096-point Hann. Reproduce
     * that weighting by hand and show the asymmetry it would have had. */
    float w_a = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)k / (float)FFT_SIZE));
    float w_b = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)(count - 1 - k) / (float)FFT_SIZE));
    float asym_db = 20.0f * log10f(w_b / w_a);
    CHECK(asym_db > 6.0f,
          "NEGATIVE CONTROL: the old quarter-window taper weighted them %.1f dB apart",
          asym_db);

    /* Full-length input is untouched by the stretch: identity mapping. */
    float c = impulse_mag_db(k, FFT_SIZE, 100);
    float d = impulse_mag_db((uint16_t)(FFT_SIZE - 1 - k), FFT_SIZE, 100);
    CHECK(fabsf(c - d) < 0.5f, "4096-sample input still symmetric (%.2f vs %.2f dB)", c, d);
    fft_deinit();
}

/* ── 5. unknown rate: no hertz, labels survive ───────────────────── */
static void test_unknown_rate(void)
{
    printf("\n[5] unknown rate\n");
    /* 8-bit square, ~fs/40 so the fundamental and H3 sit in clean bins */
    for (int i = 0; i < REC_N; i++)
        rec[i] = (i < (int)SCOPE_RECORD_HEAD_SKIP) ? 0 : (((i / 20) & 1) ? 228 : 28);
    uint16_t n = fft_live_prepare(rec, REC_N, sbuf, FFT_SIZE);

    init_fft(FFT_WINDOW_HANNING, 0.0f);
    fft_process(sbuf, n, &res);
    CHECK(res.bin_width_hz == 0.0f, "bin_width_hz is 0 at rate 0");
    int any_hz = 0;
    for (uint8_t p = 0; p < res.num_peaks; p++)
        if (res.peaks[p].freq_hz != 0.0f) any_hz++;
    CHECK(res.num_peaks > 0 && any_hz == 0, "no peak carries a frequency in hertz");
    int fund = 0, h3 = 0;
    for (uint8_t p = 0; p < res.num_peaks; p++) {
        if (strcmp(res.peaks[p].label, "Fund") == 0) fund++;
        if (strcmp(res.peaks[p].label, "H3") == 0) h3++;
    }
    CHECK(fund == 1 && h3 == 1, "harmonic labels (bin ratios) still assigned: Fund %d, H3 %d", fund, h3);
    fft_deinit();
}

int main(void)
{
    printf("test_fft_live: the live acquisition record as spectrum input\n");
    test_head_skip();
    test_bin_hz();
    test_tone_bin();
    test_window_spans_record();
    test_unknown_rate();
    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed ? 1 : 0;
}
