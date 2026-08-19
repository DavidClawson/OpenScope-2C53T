// esp32_siggen — serial-controllable 2-channel test-signal generator for the
// OpenScope 2C53T bench.
//
// Outputs: DAC1 on GPIO25 (= scope CH1), DAC2 on GPIO26 (= scope CH2).
//   0..3.3 V, 8-bit, 40 kSa/s software DDS per channel.
//   Wire  GPIO25 -> CH1 probe tip,  GPIO26 -> CH2 probe tip,  GND -> ground clip(s).
//
// Control: USB serial @ 115200, one command per line. An optional leading channel
// number (1 or 2) selects the channel; default is CH1.
//   [ch] sine  <hz>            clean sine
//   [ch] square <hz> [duty%]   square (default 50%)
//   [ch] tri   <hz>            triangle
//   [ch] saw   <hz>            sawtooth
//   [ch] dc    <mV>            constant level (0..3300)
//   [ch] amp   <mVpp>          peak-to-peak amplitude (default 3000; centered ~1.65 V)
//   [ch] off                   settle to midscale (~1.65 V)
//   [ch] phase <deg>           set this channel's phase RELATIVE TO CH1 (0..360)
//   pwm <hz> [duty%]           HARDWARE PWM on GPIO27, up to MHz (window probe)
//   pwm off                    stop it
//   status | ?                 print both channels + pwm
// Examples:  "sine 1000"  (CH1)   "2 tri 200"  (CH2)   "2 square 500 25"
//
// Usable ~1 Hz .. ~5 kHz per channel. Board: any ESP32 with 2 DACs (WROOM/WROVER,
// LOLIN D32 PRO). FQBN esp32:esp32:esp32.

#include <Arduino.h>
#include <math.h>

static const int      DAC_PIN[2] = { 25, 26 };   // CH1 = DAC1/GPIO25, CH2 = DAC2/GPIO26
static const uint32_t FS         = 40000;         // sample rate (Hz)

// per-channel DDS state (index 0 = CH1, 1 = CH2)
volatile int      g_mode [2] = { 1, 1 };          // 0=dc/off 1=sine 2=square 3=tri 4=saw
volatile uint32_t g_phase[2] = { 0, 0 };
volatile uint32_t g_inc  [2] = { 0, 0 };          // phase step per sample (2^32 * f / FS)
volatile uint8_t  g_amp  [2] = { 232, 232 };      // p-p in DAC counts (3000 mV ≈ 232)
volatile uint8_t  g_mid  [2] = { 128, 128 };      // center (~1.65 V)
volatile uint8_t  g_duty [2] = { 128, 128 };      // square threshold (0..255)
volatile uint8_t  g_dc   [2] = { 128, 128 };
volatile int      g_deg  [2] = { 0, 0 };        // phase offset vs CH1, degrees
volatile float    g_hz   [2] = { 0.0f, 0.0f };    // COMMANDED frequency, as asked for

/*
 * ---- achieved sample rate ------------------------------------------------
 *
 * FS above is an ASSUMPTION, not a measurement, and the loop below cannot
 * actually hold it: it schedules the next sample as `now + 25us` AFTER the
 * work has already been done, so the period is 25us plus however long two
 * dacWrite() calls and a serial poll take.  The error does not average out.
 *
 * That matters far beyond this sketch.  Every frequency this generator has
 * ever produced was reported to the rest of the project as the COMMANDED
 * value, and every sample rate we have published for the 2C53T was fitted
 * against those commanded values.  If the achieved rate is not 40 kHz, every
 * one of those numbers carries the same multiplicative error.
 *
 * So: count samples, and let the ESP32's own crystal-derived micros() say
 * what the rate really is.  `fs` reports it.  `usefs 1` makes set_freq use
 * the measured rate so commanded and delivered agree.
 *
 * DEFAULT IS OFF.  Booting with usefs=0 keeps this instrument bit-identical
 * to the one that took every previous measurement, so flashing this image
 * does not silently invalidate the archive.
 */
volatile uint32_t g_nsamp   = 0;                  // samples emitted since t0
static   uint32_t g_stat_t0 = 0;                  // micros() at window start
static   double   g_fs_eff  = (double)FS;         // what set_freq divides by
static   bool     g_usefs   = false;              // false = nominal, true = measured

static uint8_t sinelut[256];

static inline uint8_t mv_to_counts(long mv) {
  if (mv < 0) mv = 0; if (mv > 3300) mv = 3300;
  return (uint8_t)((mv * 255L) / 3300L);
}

static void set_freq(int ch, float hz) {
  if (hz < 0) hz = 0;
  g_hz[ch]  = hz;
  g_inc[ch] = (uint32_t)((double)hz * 4294967296.0 / g_fs_eff);     // 2^32 * hz / FS
}

static inline uint8_t next_sample(int ch) {
  uint8_t p = (uint8_t)(g_phase[ch] >> 24);       // 0..255 within one cycle
  int v;
  switch (g_mode[ch]) {
    case 1:  v = sinelut[p]; break;
    case 2:  v = (p < g_duty[ch]) ? 255 : 0; break;
    case 3:  v = (p < 128) ? (p * 2) : (255 - (p - 128) * 2); break;  // triangle
    case 4:  v = p; break;                                            // sawtooth
    default: return g_dc[ch];                                         // dc / off
  }
  int s = g_mid[ch] + ((v - 128) * (int)g_amp[ch]) / 255;
  if (s < 0) s = 0; if (s > 255) s = 255;
  return (uint8_t)s;
}

// ---- LEDC hardware PWM (GPIO27) -------------------------------------------
//
// Added 2026-08-14 for one specific measurement: how long is the scope's
// capture window?
//
// The DDS above is software-timed at FS = 40 kSa/s, so it tops out near 5 kHz.
// That is far too slow to answer the question. On the 2C53T the FPGA samples
// fast and the firmware has no rate control, so a 1024-sample capture spans a
// window believed to be microseconds — which is why a 2 Hz square renders as a
// flat line that jumps between top and bottom rather than as a waveform.
//
// If that window is ~4 us, a signal only becomes a visible WAVEFORM when a few
// cycles fit inside it, i.e. somewhere in the hundreds of kHz. Sweeping this
// output upward and noting where the trace stops being a moving level measures
// the window directly, and the sample rate falls out of it.
//
// LEDC is hardware, so it does not perturb the DDS timing in loop().
// Resolution must shrink as frequency rises: f_max = 80 MHz / 2^bits.
static const int PWM_PIN = 27;
static bool     g_pwm_on = false;
static float    g_pwm_hz = 0;
static int      g_pwm_bits = 0;
static int      g_pwm_duty_pct = 50;

static void pwm_set(float hz, int duty_pct) {
  if (hz <= 0) { if (g_pwm_on) { ledcDetach(PWM_PIN); g_pwm_on = false; } g_pwm_hz = 0; return; }

  // Largest resolution that still supports this frequency off the 80 MHz APB.
  int bits = 14;
  while (bits > 1 && (80000000.0f / (float)(1UL << bits)) < hz) bits--;

  if (g_pwm_on) ledcDetach(PWM_PIN);
  if (!ledcAttach(PWM_PIN, hz, bits)) { Serial.println("[pwm] attach FAILED"); g_pwm_on = false; return; }
  g_pwm_on = true; g_pwm_hz = hz; g_pwm_bits = bits; g_pwm_duty_pct = duty_pct;

  uint32_t maxduty = (1UL << bits) - 1UL;
  ledcWrite(PWM_PIN, (uint32_t)((float)maxduty * (float)duty_pct / 100.0f));
}

static void print_pwm() {
  if (!g_pwm_on) { Serial.println("[pwm] GPIO27 off"); return; }
  /* ledcReadFreq() returns uint32_t. Printing it with %f read garbage off the
   * stack and reported "actual 1.7 Hz" for a 1 kHz request — a plausible-looking
   * number produced by a format mismatch, not a measurement. */
  Serial.printf("[pwm] GPIO27 req=%.0f Hz  %d%%  %d-bit  actual=%u Hz\n",
                g_pwm_hz, g_pwm_duty_pct, g_pwm_bits,
                (unsigned)ledcReadFreq(PWM_PIN));
}

// ---- achieved sample rate reporting ---------------------------------------

static void fs_reset() { g_nsamp = 0; g_stat_t0 = (uint32_t)micros(); }

static double fs_measured() {
  uint32_t n  = g_nsamp;
  uint32_t dt = (uint32_t)micros() - g_stat_t0;   /* wrap-safe for <71 min */
  if (n < 2 || dt == 0) return 0.0;
  return (double)n * 1000000.0 / (double)dt;
}

static void print_fs() {
  double m = fs_measured();
  if (m <= 0.0) { Serial.println("[fs] window too short - try again"); return; }
  Serial.printf("[fs] nominal=%u  achieved=%.1f Hz  ratio=%.4f  n=%u  dt=%.2fs  set_freq uses %s\n",
                (unsigned)FS, m, m / (double)FS, (unsigned)g_nsamp,
                (double)((uint32_t)micros() - g_stat_t0) / 1e6,
                g_usefs ? "MEASURED" : "nominal");
}

static void use_fs(bool on) {
  double m = fs_measured();
  if (on && m <= 0.0) { Serial.println("[fs] no measurement yet - refusing"); return; }
  g_usefs  = on;
  g_fs_eff = on ? m : (double)FS;
  for (int ch = 0; ch < 2; ch++) set_freq(ch, g_hz[ch]);   /* re-derive both */
  Serial.printf("[fs] set_freq now divides by %.1f (%s)\n",
                g_fs_eff, on ? "MEASURED" : "nominal");
}

// ---- command parser -------------------------------------------------------
static char linebuf[64];
static uint8_t linelen = 0;

static const char *mode_name(int ch) {
  switch (g_mode[ch]) { case 1: return "sine"; case 2: return "square";
                        case 3: return "tri";  case 4: return "saw"; default: return "dc"; }
}

static void print_channel(int ch) {
  float hz = g_hz[ch];   /* as commanded; back-computing it through FS would
                          * hide exactly the error this sketch now measures */
  Serial.printf("[siggen] CH%d mode=%s  freq=%.1f Hz  amp=%d mVpp  mid=%d mV  duty=%d%%  phase=%d deg\n",
                ch + 1, mode_name(ch), hz, (int)((long)g_amp[ch] * 3300 / 255),
                (int)((long)g_mid[ch] * 3300 / 255), (int)((long)g_duty[ch] * 100 / 255),
                g_deg[ch]);
}

static void handle_line(char *s) {
  while (*s == ' ') s++;
  char *t = strtok(s, " \t");
  if (!t) return;

  // optional leading channel number (1/2); default CH1
  int ch = 0;
  if ((t[0] == '1' || t[0] == '2') && t[1] == 0) {
    ch = t[0] - '1';
    t = strtok(NULL, " \t");
    if (!t) { print_channel(ch); return; }
  }
  char *cmd = t;
  char *a1 = strtok(NULL, " \t");
  char *a2 = strtok(NULL, " \t");

  if      (!strcmp(cmd, "sine"))   { g_mode[ch] = 1; if (a1) set_freq(ch, atof(a1)); }
  else if (!strcmp(cmd, "square")) { g_mode[ch] = 2; if (a1) set_freq(ch, atof(a1));
                                     if (a2) g_duty[ch] = (uint8_t)(atol(a2) * 255 / 100); }
  else if (!strcmp(cmd, "tri"))    { g_mode[ch] = 3; if (a1) set_freq(ch, atof(a1)); }
  else if (!strcmp(cmd, "saw"))    { g_mode[ch] = 4; if (a1) set_freq(ch, atof(a1)); }
  else if (!strcmp(cmd, "dc"))     { g_mode[ch] = 0; if (a1) g_dc[ch] = mv_to_counts(atol(a1)); }
  else if (!strcmp(cmd, "amp"))    { if (a1) g_amp[ch] = mv_to_counts(atol(a1)); }
  else if (!strcmp(cmd, "off"))    { g_mode[ch] = 0; g_dc[ch] = g_mid[ch]; }
  else if (!strcmp(cmd, "phase")) {
    /* Phase relative to CH1, in degrees. Both channels advance by their own
     * g_inc every sample, so when the two frequencies are equal the offset
     * set here stays constant — that is what makes an anti-phase (180 deg)
     * pair usable as a two-channel test signal. Added 2026-08-15 to verify
     * 2C53T CH1/CH2 hardware independently of firmware. */
    if (a1) {
      long deg = atol(a1) % 360; if (deg < 0) deg += 360;
      g_phase[ch] = g_phase[0] + (uint32_t)((double)deg * 4294967296.0 / 360.0);
      g_deg[ch] = (int)deg;
    }
  }
  else if (!strcmp(cmd, "pwm")) {
    if (!a1) { print_pwm(); return; }
    if (!strcmp(a1, "off")) { pwm_set(0, 0); Serial.println("[pwm] off"); return; }
    pwm_set(atof(a1), a2 ? atoi(a2) : 50);
    print_pwm(); return;
  }
  else if (!strcmp(cmd, "fs")) {
    if (!a1)                      { print_fs(); return; }
    if (!strcmp(a1, "reset"))     { fs_reset(); Serial.println("[fs] window reset"); return; }
    print_fs(); return;
  }
  else if (!strcmp(cmd, "usefs")) {
    if (!a1) { print_fs(); return; }
    use_fs(atoi(a1) != 0); return;
  }
  else if (!strcmp(cmd, "status") || !strcmp(cmd, "?") || !strcmp(cmd, "help")) {
    print_channel(0); print_channel(1); print_pwm(); print_fs(); return;
  }
  else { Serial.printf("[siggen] ? unknown: %s\n", cmd); return; }
  print_channel(ch);
}

static void poll_serial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { linebuf[linelen] = 0; if (linelen) handle_line(linebuf); linelen = 0; }
    else if (linelen < sizeof(linebuf) - 1) linebuf[linelen++] = c;
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 256; i++)
    sinelut[i] = (uint8_t)(127.5 + 127.5 * sinf(2.0f * (float)M_PI * i / 256.0f));
  for (int ch = 0; ch < 2; ch++) {
    dacWrite(DAC_PIN[ch], g_mid[ch]);
    set_freq(ch, 1000.0f);      // default 1 kHz
    g_mode[ch] = 1;             // sine on boot so both channels show a trace
  }
  fs_reset();
  Serial.println("\n[siggen] ready — GPIO25->CH1, GPIO26->CH2, GPIO27=PWM, GND->ground.");
  print_channel(0); print_channel(1);
}

void loop() {
  static uint32_t next_us = 0;
  uint32_t now = (uint32_t)micros();
  if ((int32_t)(now - next_us) >= 0) {
    next_us = now + (1000000UL / FS);
    g_phase[0] += g_inc[0];
    g_phase[1] += g_inc[1];
    dacWrite(DAC_PIN[0], next_sample(0));
    dacWrite(DAC_PIN[1], next_sample(1));
    g_nsamp++;
  }
  poll_serial();
}
