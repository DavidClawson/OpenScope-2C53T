/*
 * esp32_sspi_bringup.ino — external Gowin SSPI master for FNIRSI 2C53T bring-up
 * ============================================================================
 *
 *  EXPERIMENTAL / UNTESTED (2026-06-14). Bench-validation pending — see
 *  tools/esp32_sspi_bringup/README.md and the experimental/esp32-bringup branch.
 *
 *  WHAT THIS IS
 *  ------------
 *  A tiny serial-driven SPI master that talks to the FNIRSI 2C53T's Gowin
 *  GW1N-UV2 over its SSPI configuration port, via the back-side SPI3 test pads
 *  that @maksidze exposed on GitHub issue #18. It lets us poke the FPGA's
 *  config controller DIRECTLY — at a controlled slow clock — without the
 *  build → flash → pinhole-reset loop on the MCU.
 *
 *  USE WITH the 2C53T running the `experimental/esp32-bringup` firmware and
 *  the `fpga busrelease` debug-shell command issued first, so the MCU has
 *  tri-stated PB3/PB5/PB6 and handed us the bus (no contention). The MCU keeps
 *  the board powered (PC9) and holds PC6/PB11 HIGH for us.
 *
 *  WIRING (ESP32 default VSPI ↔ 2C53T SPI3 test pads — straight through)
 *  --------------------------------------------------------------------
 *    ESP32 GPIO18 (SCK)  ── SCK  pad
 *    ESP32 GPIO23 (MOSI) ── MOSI pad      (ESP32 drives FPGA data-in)
 *    ESP32 GPIO19 (MISO) ── MISO pad      (ESP32 reads  FPGA data-out)
 *    ESP32 GPIO5  (CS)   ── CS   pad      (software chip-select, active LOW)
 *    ESP32 GND           ── board GND     (REQUIRED — common reference)
 *
 *  ESP32 is native 3.3 V, so these connect directly to the 3.3 V bus.
 *  Do NOT use a 5 V Arduino Uno here without level shifting.
 *
 *  CLOCK
 *  -----
 *  Our bench found the GW1N SSPI port is clock-limited: IDCODE reads as garbage
 *  (0x8090340D) at 60 MHz but correct (0x0120681B) at ~470 kHz. So we default
 *  to a slow 400 kHz here. SPI mode 3 (CPOL=1, CPHA=1), MSB-first — matches the
 *  2C53T's SPI3 config (CLAUDE.md).
 *
 *  SERIAL COMMANDS (115200 baud)
 *  -----------------------------
 *    id              read IDCODE (opcode 0x11) — expect 01 20 68 1B
 *    status          read STATUS_REGISTER (opcode 0x41)
 *    enable          send the FNIRSI config-enable prelude (05 / 12 / 15)
 *    clk <kHz>       set SCK frequency in kHz (e.g. clk 400)
 *    raw XX XX ..    one CS frame: clock the given hex bytes, print MISO echo
 *    help            print this list
 *
 *  All opcodes/expected values are from the issue-#18 stock-boot capture and
 *  our bench notes; the exact dummy-byte padding for reads may need tuning
 *  against maksidze's capture — see the TODO in read_idcode().
 */

#include <SPI.h>

// ---- pin map (ESP32 VSPI defaults) ----
static const int PIN_SCK  = 18;
static const int PIN_MISO = 19;
static const int PIN_MOSI = 23;
static const int PIN_CS   = 5;

// ---- SPI settings ----
static uint32_t g_sck_hz = 400000;            // 400 kHz default (slow, safe)
static const uint8_t  SPI_BITORDER = MSBFIRST;
static const uint8_t  SPI_MODE_FPGA = SPI_MODE3;   // CPOL=1, CPHA=1

static void cs_low()  { digitalWrite(PIN_CS, LOW);  }
static void cs_high() { digitalWrite(PIN_CS, HIGH); }

static void begin_frame() {
  SPI.beginTransaction(SPISettings(g_sck_hz, SPI_BITORDER, SPI_MODE_FPGA));
  cs_low();
}
static void end_frame() {
  cs_high();
  SPI.endTransaction();
}

// Clock one byte, return the MISO byte captured during the same transfer.
static uint8_t xfer(uint8_t b) { return SPI.transfer(b); }

// --- Read the Gowin IDCODE (opcode 0x11). Expect 01 20 68 1B (GW1N-2). ---
static void read_idcode() {
  begin_frame();
  xfer(0x11);
  // Gowin SSPI typically pads the opcode out to a 4-byte command word before
  // the 4-byte response is clocked. TODO: confirm the exact pad count against
  // maksidze's #18 capture; 3 dummy bytes is the working assumption.
  xfer(0x00); xfer(0x00); xfer(0x00);
  uint8_t id[4];
  for (int i = 0; i < 4; i++) id[i] = xfer(0x00);
  end_frame();
  Serial.printf("IDCODE: %02X %02X %02X %02X  (expect 01 20 68 1B)\n",
                id[0], id[1], id[2], id[3]);
}

// --- Read the Gowin STATUS_REGISTER (opcode 0x41). ---
static void read_status() {
  begin_frame();
  xfer(0x41);
  xfer(0x00); xfer(0x00); xfer(0x00);
  uint8_t s[4];
  for (int i = 0; i < 4; i++) s[i] = xfer(0x00);
  end_frame();
  Serial.printf("STATUS: %02X %02X %02X %02X\n", s[0], s[1], s[2], s[3]);
  Serial.println("  (all-FF => FPGA not driving MISO / config port not open)");
}

// --- FNIRSI config-enable prelude: 05/12/15, each in its own CS frame. ---
static void config_enable() {
  const uint8_t op[3] = {0x05, 0x12, 0x15};
  for (int i = 0; i < 3; i++) {
    begin_frame();
    uint8_t r0 = xfer(op[i]);
    uint8_t r1 = xfer(0x00);
    end_frame();
    Serial.printf("prelude %02X 00 -> MISO %02X %02X\n", op[i], r0, r1);
    delay(100);   // stock spaces these ~100 ms apart
  }
  Serial.println("prelude sent; read 'status' to see if config-receive engaged");
}

// --- one CS frame of arbitrary hex bytes, print the MISO echo ---
static void raw_frame(char *args) {
  uint8_t tx[64]; int n = 0;
  char *tok = strtok(args, " ");
  while (tok && n < 64) {
    tx[n++] = (uint8_t) strtol(tok, nullptr, 16);
    tok = strtok(nullptr, " ");
  }
  if (n == 0) { Serial.println("usage: raw XX XX .."); return; }
  begin_frame();
  Serial.print("MISO:");
  for (int i = 0; i < n; i++) Serial.printf(" %02X", xfer(tx[i]));
  end_frame();
  Serial.println();
}

static void print_help() {
  Serial.println("commands: id | status | enable | clk <kHz> | raw XX XX .. | help");
  Serial.printf("SCK = %lu Hz, mode 3, MSB-first\n", (unsigned long) g_sck_hz);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CS, OUTPUT);
  cs_high();
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(200);
  Serial.println("\n2C53T Gowin SSPI bring-up master (EXPERIMENTAL/UNTESTED)");
  print_help();
}

void loop() {
  static char buf[96]; static int len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;
      buf[len] = 0; len = 0;
      if      (!strcmp(buf, "id"))      read_idcode();
      else if (!strcmp(buf, "status"))  read_status();
      else if (!strcmp(buf, "enable"))  config_enable();
      else if (!strcmp(buf, "help"))    print_help();
      else if (!strncmp(buf, "clk ", 4)) {
        g_sck_hz = (uint32_t) strtoul(buf + 4, nullptr, 10) * 1000UL;
        Serial.printf("SCK = %lu Hz\n", (unsigned long) g_sck_hz);
      }
      else if (!strncmp(buf, "raw ", 4)) raw_frame(buf + 4);
      else Serial.println("?  (type 'help')");
    } else if (len < (int)sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}
