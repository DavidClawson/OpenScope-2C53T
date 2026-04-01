/*
 * ESP32 USB-UART Bridge for FNIRSI 2C53T debugging
 *
 * Wiring:
 *   ESP32 GPIO16 (RX2) → 2C53T debug TX pad
 *   ESP32 GPIO17 (TX2) → 2C53T debug RX pad
 *   ESP32 GND          → 2C53T GND
 *   (Do NOT connect VCC — the 2C53T runs on its own battery)
 *
 * Opens two serial ports:
 *   Serial  (USB)   — 115200 baud to your computer
 *   Serial2 (UART2) — 9600 baud to the 2C53T debug pads
 *
 * Bidirectional bridge: anything received from 2C53T is forwarded to USB,
 * and anything typed in the serial monitor is forwarded to 2C53T.
 *
 * Also prints hex dump of received bytes for debugging binary protocols.
 */

#include <Arduino.h>

#define USB_BAUD    115200
#define TARGET_BAUD 9600

// ESP32 UART2 pins (change if your board uses different pins)
#define RXD2 16  // Connect to 2C53T debug TX pad
#define TXD2 17  // Connect to 2C53T debug RX pad

// Set to 1 for hex dump mode (shows raw bytes as hex)
// Set to 0 for passthrough mode (raw binary forwarding)
#define HEX_DUMP_MODE 1

void setup() {
  Serial.begin(USB_BAUD);
  Serial2.begin(TARGET_BAUD, SERIAL_8N1, RXD2, TXD2);

  Serial.println();
  Serial.println("=== FNIRSI 2C53T UART Bridge ===");
  Serial.print("Target baud: ");
  Serial.println(TARGET_BAUD);
  Serial.print("RX pin: GPIO");
  Serial.print(RXD2);
  Serial.print(", TX pin: GPIO");
  Serial.println(TXD2);
  Serial.println("Waiting for data...");
  Serial.println();
}

int byteCount = 0;

void loop() {
  // Forward from 2C53T → USB
  while (Serial2.available()) {
    uint8_t b = Serial2.read();

    if (HEX_DUMP_MODE) {
      // Print as hex with frame markers
      if (b == 0x5A && byteCount > 0) {
        Serial.println();  // New line before each frame
        byteCount = 0;
      }
      if (b < 0x10) Serial.print("0");
      Serial.print(b, HEX);
      Serial.print(" ");
      byteCount++;
      if (byteCount >= 16) {
        Serial.println();
        byteCount = 0;
      }
    } else {
      Serial.write(b);
    }
  }

  // Forward from USB → 2C53T
  while (Serial.available()) {
    Serial2.write(Serial.read());
  }
}
