# FFT / Spectrum Analysis Design

## Why the Stock FFT Is Broken

Community reports say the FFT feature "doesn't produce usable results." Likely causes:

1. **No windowing** — rectangular window causes massive spectral leakage
2. **Wrong amplitude scaling** — magnitudes are meaningless numbers
3. **Fixed sample rate mismatch** — uses current timebase rate regardless of signal content
4. **Linear display** — can't see anything except the dominant frequency
5. **Too few points** — insufficient frequency resolution

## Hardware Capabilities

- **CPU:** Cortex-M4 @ 120MHz with hardware FPU and single-cycle MAC (DSP instructions)
- **Library:** CMSIS-DSP provides optimized `arm_rfft_fast_f32()` — a 4096-point FFT runs in ~1-2ms
- **RAM:** 256KB total, ~100KB available after framebuffer and RTOS overhead
- **FPGA:** Provides sample buffers at configurable rates (not continuous streaming)

## Memory Budget

| FFT Size | Input Buffer | Output Buffer | Magnitude | Window | Total | Resolution at 1MS/s |
|---|---|---|---|---|---|---|
| 1024 | 4 KB | 4 KB | 2 KB | 4 KB | 14 KB | 977 Hz |
| 2048 | 8 KB | 8 KB | 4 KB | 8 KB | 28 KB | 488 Hz |
| **4096** | **16 KB** | **16 KB** | **8 KB** | **16 KB** | **56 KB** | **244 Hz** |
| 8192 | 32 KB | 32 KB | 16 KB | 32 KB | 112 KB | 122 Hz |

**4096 is the sweet spot** — 56KB total (~22% of RAM), sub-kHz resolution at moderate sample rates, 1-2ms compute time.

## Frequency Resolution vs Sample Rate

The FFT resolution depends on both FFT size and sample rate: `bin_width = sample_rate / FFT_size`

| Timebase | Effective Rate | 4096-pt Resolution | Useful Range |
|---|---|---|---|
| 50ns/div | 250 MS/s | 61 kHz | RF, harmonics to 125MHz |
| 1µs/div | 50 MS/s | 12.2 kHz | RF, MHz signals |
| 10µs/div | 5 MS/s | 1.22 kHz | Audio, kHz signals |
| 100µs/div | 500 kS/s | 122 Hz | Audio detail |
| 1ms/div | 50 kS/s | 12.2 Hz | Low frequency, mains harmonics |
| 10ms/div | 5 kS/s | 1.22 Hz | Sub-Hz resolution |

## Window Functions

| Window | Sidelobe | Main Lobe Width | Best For | Implementation |
|---|---|---|---|---|
| **Hanning** (default) | -31 dB | Medium | General purpose | `0.5 * (1 - cos(2π*n/N))` |
| **Hamming** | -43 dB | Medium | Better rejection | `0.54 - 0.46 * cos(2π*n/N)` |
| **Blackman-Harris** | -92 dB | Wide | Weak signals near strong | 4-term cosine sum |
| **Flat-top** | -44 dB | Widest | Accurate amplitude | 5-term cosine sum |
| **Rectangular** | -13 dB | Narrowest | Exactly periodic signals | No multiply (×1) |

Window coefficients are precomputed at init time — just a multiply per sample during processing.

## Implementation

### Core FFT Pipeline

```c
#include "arm_math.h"  // CMSIS-DSP

#define FFT_SIZE 4096
#define FFT_BINS (FFT_SIZE / 2)

// Buffers (allocated in RAM)
static float32_t fft_input[FFT_SIZE];
static float32_t fft_output[FFT_SIZE];
static float32_t fft_magnitude[FFT_BINS];
static float32_t fft_avg[FFT_BINS];       // running average
static float32_t window_coeffs[FFT_SIZE];  // precomputed window

static arm_rfft_fast_instance_f32 fft_instance;

typedef enum {
    FFT_WINDOW_RECTANGULAR,
    FFT_WINDOW_HANNING,
    FFT_WINDOW_HAMMING,
    FFT_WINDOW_BLACKMAN_HARRIS,
    FFT_WINDOW_FLAT_TOP,
} fft_window_t;

typedef struct {
    fft_window_t window;
    uint16_t     fft_size;        // 512, 1024, 2048, 4096, 8192
    uint8_t      avg_count;       // 0=off, 2, 4, 8, 16, 32
    float32_t    ref_level_db;    // top of display (dBV)
    float32_t    db_per_div;      // vertical scale
    bool         log_freq_axis;   // true=log, false=linear
    bool         auto_mode;       // auto-detect and configure
    uint8_t      peak_count;      // number of peaks to label (0=off)
} fft_config_t;

void fft_init(fft_config_t *cfg) {
    arm_rfft_fast_init_f32(&fft_instance, cfg->fft_size);
    fft_compute_window(cfg->window, cfg->fft_size);
    memset(fft_avg, 0, sizeof(fft_avg));
}

void fft_compute_window(fft_window_t type, uint16_t size) {
    for (int i = 0; i < size; i++) {
        float32_t n = (float32_t)i / (float32_t)(size - 1);
        switch (type) {
            case FFT_WINDOW_HANNING:
                window_coeffs[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * n));
                break;
            case FFT_WINDOW_HAMMING:
                window_coeffs[i] = 0.54f - 0.46f * arm_cos_f32(2.0f * PI * n);
                break;
            case FFT_WINDOW_BLACKMAN_HARRIS:
                window_coeffs[i] = 0.35875f - 0.48829f * arm_cos_f32(2*PI*n)
                                 + 0.14128f * arm_cos_f32(4*PI*n)
                                 - 0.01168f * arm_cos_f32(6*PI*n);
                break;
            case FFT_WINDOW_FLAT_TOP:
                window_coeffs[i] = 0.21557895f - 0.41663158f * arm_cos_f32(2*PI*n)
                                 + 0.277263158f * arm_cos_f32(4*PI*n)
                                 - 0.083578947f * arm_cos_f32(6*PI*n)
                                 + 0.006947368f * arm_cos_f32(8*PI*n);
                break;
            default: // Rectangular
                window_coeffs[i] = 1.0f;
        }
    }
}

void fft_process(int16_t *adc_samples, uint16_t num_samples, float32_t sample_rate) {
    uint16_t n = (num_samples < FFT_SIZE) ? num_samples : FFT_SIZE;

    // Convert ADC samples to float and apply window
    for (int i = 0; i < n; i++) {
        fft_input[i] = (float32_t)adc_samples[i] * window_coeffs[i];
    }
    // Zero-pad if input is shorter than FFT size
    for (int i = n; i < FFT_SIZE; i++) {
        fft_input[i] = 0.0f;
    }

    // Run FFT (CMSIS-DSP optimized)
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);

    // Calculate magnitude (sqrt(re² + im²))
    arm_cmplx_mag_f32(fft_output, fft_magnitude, FFT_BINS);

    // Normalize by FFT size and window coherent gain
    float32_t norm = 2.0f / (float32_t)FFT_SIZE;  // ×2 for single-sided
    arm_scale_f32(fft_magnitude, norm, fft_magnitude, FFT_BINS);

    // Convert to dB
    for (int i = 0; i < FFT_BINS; i++) {
        if (fft_magnitude[i] < 1e-10f) fft_magnitude[i] = 1e-10f;
        fft_magnitude[i] = 20.0f * log10f(fft_magnitude[i]);
    }

    // Running average (exponential moving average)
    if (avg_count > 0) {
        float32_t alpha = 1.0f / (float32_t)avg_count;
        for (int i = 0; i < FFT_BINS; i++) {
            fft_avg[i] = fft_avg[i] * (1.0f - alpha) + fft_magnitude[i] * alpha;
        }
    }
}
```

### Auto Mode Algorithm

```c
void fft_auto_configure(int16_t *samples, uint16_t num_samples, float32_t sample_rate) {
    // Step 1: Estimate fundamental frequency from time domain
    // (find average zero-crossing interval)
    int crossings = 0;
    int last_crossing = 0;
    float avg_period = 0;

    for (int i = 1; i < num_samples; i++) {
        if ((samples[i-1] < 0 && samples[i] >= 0)) {  // rising zero crossing
            if (crossings > 0) {
                avg_period += (float)(i - last_crossing);
            }
            last_crossing = i;
            crossings++;
        }
    }

    float fundamental_hz = 0;
    if (crossings > 1) {
        avg_period /= (float)(crossings - 1);
        fundamental_hz = sample_rate / avg_period;
    }

    // Step 2: Choose appropriate sample rate for the signal
    // Want at least 10× the fundamental, up to 100× for seeing harmonics
    float desired_rate = fundamental_hz * 100.0f;
    if (desired_rate > 250e6f) desired_rate = 250e6f;
    if (desired_rate < 1000.0f) desired_rate = 1000.0f;

    // Step 3: Configure FPGA sample rate (via timebase setting)
    // ... set_fpga_timebase(rate_to_timebase(desired_rate));

    // Step 4: Choose FFT size for ~10 bins per decade
    // ... select 4096 as default

    // Step 5: Set display range
    // Show from DC to sample_rate/2, auto-scale vertical to peak
}
```

### Peak Detection

```c
typedef struct {
    uint16_t  bin;
    float32_t freq_hz;
    float32_t magnitude_db;
    char      label[8];  // "Fund", "H2", "H3", etc.
} fft_peak_t;

int fft_find_peaks(float32_t *mag, uint16_t num_bins, float32_t sample_rate,
                   fft_peak_t *peaks, int max_peaks, float32_t threshold_db) {
    int count = 0;
    float32_t bin_width = sample_rate / (float32_t)(num_bins * 2);

    for (int i = 2; i < num_bins - 1 && count < max_peaks; i++) {
        // Peak: higher than both neighbors and above threshold
        if (mag[i] > mag[i-1] && mag[i] > mag[i+1] && mag[i] > threshold_db) {
            peaks[count].bin = i;
            peaks[count].freq_hz = (float32_t)i * bin_width;
            peaks[count].magnitude_db = mag[i];

            // Label harmonics relative to fundamental
            if (count == 0) {
                snprintf(peaks[count].label, 8, "Fund");
            } else {
                float ratio = peaks[count].freq_hz / peaks[0].freq_hz;
                int harmonic = (int)(ratio + 0.5f);
                if (fabsf(ratio - harmonic) < 0.05f) {
                    snprintf(peaks[count].label, 8, "H%d", harmonic);
                } else {
                    snprintf(peaks[count].label, 8, "%.0fHz", peaks[count].freq_hz);
                }
            }
            count++;
        }
    }
    return count;
}
```

## Display Modes

### Standard FFT View

```
┌─────────────────────────────────────────┐
│ FFT  4096pt  Hanning  Avg:4  dBV       │
│  0 ┤ ▓                                  │
│-20 ┤ ▓                                  │
│-40 ┤ ▓         ▓                        │
│-60 ┤ ▓         ▓         ▓              │
│-80 ┤▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│    └┴─────────┴─────────┴──────────┴─── │
│     1kHz     2kHz      3kHz       5kHz  │
│ Peak: 1.002kHz @ -3.2dBV  H2: -42dBc   │
└─────────────────────────────────────────┘
```

### Split View (Time + Frequency)

Top half shows the time-domain waveform, bottom half shows FFT. Both update together. The most useful default view — you see what the signal looks like AND what frequencies it contains.

### Waterfall / Spectrogram

Frequency on X axis, time scrolling on Y axis. Brightness = magnitude. Shows how the frequency content changes over time. Useful for:
- Watching a signal drift in frequency
- Seeing intermittent interference
- FT8/CW signals in ham radio
- Motor vibration patterns in industrial

### Max Hold

Don't clear the FFT between updates — keep the maximum value at each bin. Over time, this builds up the envelope of the worst-case frequency content. Useful for finding intermittent spurs.

## User Interface Integration

**Button mapping:**
- `MENU` → FFT submenu (window, size, averaging, scale, axis)
- `AUTO` → Auto FFT mode (detect signal, configure, display)
- `SELECT` → Cycle through display modes (standard, split, waterfall, max hold)
- `UP/DOWN` → Adjust reference level (vertical position of the display)
- `LEFT/RIGHT` → Zoom frequency range
- `CH1/CH2` → Select which channel to FFT

**Status bar shows:** FFT size, window type, averaging count, scale unit, peak frequency

## Integration with Other Modules

The FFT engine is shared infrastructure used by multiple features:

| Module | How it uses FFT |
|---|---|
| **Harmonic analyzer** | FFT + auto-detect fundamental + label harmonics + FCC limit overlay |
| **Audio frequency response** | FFT at each sweep frequency for Bode plot magnitude |
| **Speaker impedance** | FFT of current/voltage for impedance vs frequency |
| **Motor current analysis (MCSA)** | FFT of current waveform to detect mechanical faults |
| **Power quality (THD)** | FFT of mains voltage, calculate total harmonic distortion |
| **CW decoder** | Goertzel (single-bin FFT) for tone detection |
| **FT8 waterfall** | Repeated FFT for spectrogram display |
| **Vibration analysis** | FFT of accelerometer signal |

This means the FFT code should be a **standalone library module** (`fft.c` / `fft.h`) that any feature can call, not embedded in the scope display code.
