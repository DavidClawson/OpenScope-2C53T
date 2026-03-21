# ESP32 Co-Processor: Architecture & Spec

## Overview

The ESP32-S3 (or ESP32-C3) module is soldered inside the 2C53T case with 4 wires (3.3V, GND, UART TX, UART RX). It runs its own firmware and communicates with the scope MCU (GD32F307) via a simple UART protocol. The ESP32 handles everything network-related — the scope MCU never touches WiFi, BLE, or HTTP.

## Hardware

| | ESP32-S3 (recommended) | ESP32-C3 (budget) |
|---|---|---|
| Module | Super Mini (~$4) | Super Mini (~$3) |
| Size | 12×18mm | 12×18mm |
| CPU | Dual Xtensa @ 240MHz | RISC-V @ 160MHz |
| RAM | 512KB + 8MB PSRAM (opt) | 400KB |
| WiFi | 802.11 b/g/n | 802.11 b/g/n |
| BLE | 5.0 | 5.0 |
| USB | OTG (can act as host) | No |
| Flash | 4MB onboard | 4MB onboard |

Physical installation: module sits near battery compartment or behind LCD. Onboard PCB antenna faces toward plastic front panel for best WiFi signal. 30AWG bodge wire for UART connections.

## UART Protocol

See `firmware/src/drivers/esp_comm.h` for the full protocol spec. Summary:

```
Packet: [0xAA] [cmd] [len_hi] [len_lo] [payload...] [checksum]
Checksum: XOR of all bytes from cmd through end of payload
Baud: 115200 (or 921600 for framebuffer streaming)
```

### Commands the ESP32 can send:

| Cmd | Name | Description |
|-----|------|-------------|
| 0x01 | PING | Get firmware version |
| 0x02 | MODULE_START | Begin module download (slot + size) |
| 0x03 | MODULE_DATA | Module data chunk (256 bytes) |
| 0x04 | MODULE_END | Finalize module install |
| 0x05 | FW_UPDATE_START | Begin firmware update |
| 0x06 | FW_UPDATE_DATA | Firmware data chunk |
| 0x07 | FW_UPDATE_COMMIT | Stage firmware and reboot |
| 0x08 | STATUS | Get device status |
| 0x09 | FRAMEBUFFER | Request LCD framebuffer |
| 0x0A | BUTTON | Simulate button press |
| 0x0B | SIGNAL_CONFIG | Set signal injection parameters |
| 0x0C | MODULE_LIST | List installed modules |
| 0x0D | MODULE_DELETE | Remove a module |

### Responses from the scope:

| Cmd | Name | Description |
|-----|------|-------------|
| 0x81 | ACK | Command accepted |
| 0x82 | NAK | Error (payload = error code) |
| 0x83 | DATA | Generic data response |
| 0x84 | FRAMEBUFFER | LCD framebuffer data |
| 0x85 | STATUS | Device status struct |
| 0x86 | MODULE_LIST | Module slot info |

## ESP32 Firmware Responsibilities

### 1. WiFi Access Point

On first boot, the ESP32 creates a WiFi AP:
```
SSID: OpenScope-XXXX (last 4 of MAC)
Password: openscope (or open network for easy setup)
IP: 192.168.4.1
```

User connects phone/laptop to this AP, opens browser to `http://192.168.4.1`.

Optional: ESP32 can also connect to home WiFi as a client (configured via the web UI) for internet access (OTA updates, module downloads from GitHub).

### 2. Web Server

Serves a Progressive Web App (PWA) from the ESP32's 4MB flash. The PWA is a single-page application that provides:

- **Live scope display** — framebuffer streamed via WebSocket
- **Button controls** — touch/click sends button commands
- **Settings panel** — change scope configuration
- **Module store** — browse and install modules
- **Firmware updates** — check for and install OTA updates

### 3. WebSocket Bridge

```
Phone browser ←→ WebSocket ←→ ESP32 ←→ UART ←→ GD32F307

Browser sends:  {"type":"button","button":"MENU"}
ESP32 converts: [0xAA][0x0A][0x00][0x01][0x09][chk]  → UART TX

GD32 sends:     [0xAA][0x84][len][framebuffer data]   → UART TX
ESP32 forwards:  binary WebSocket frame to browser
```

### 4. Module Store Backend

When the user browses the module store:

1. ESP32 fetches module index from GitHub:
   `https://raw.githubusercontent.com/openscope-modules/registry/main/index.json`
2. Displays available modules with name, description, size, version
3. User taps "Install"
4. ESP32 downloads module binary from GitHub
5. ESP32 sends MODULE_START → MODULE_DATA (chunked) → MODULE_END to scope
6. Scope writes module to SPI flash

### 5. OTA Firmware Updates

1. ESP32 checks GitHub releases API for latest firmware version
2. Compares against scope's current version (via PING command)
3. If update available, shows notification in web UI
4. User taps "Update"
5. ESP32 downloads firmware binary
6. Sends FW_UPDATE_START → FW_UPDATE_DATA → FW_UPDATE_COMMIT
7. Scope stages firmware and reboots into bootloader

### 6. BLE Beacon (optional)

ESP32 advertises a BLE beacon with basic scope info:
- Device name: "OpenScope-XXXX"
- Battery level
- Current mode
- A companion phone app could auto-discover nearby scopes

## Progressive Web App (PWA)

The web UI is built as a PWA so it works like a native app:

### What PWA gives you:
- **Install to home screen** — looks and feels like a native app
- **Works offline** — the app is cached, only live data needs connection
- **Full screen** — no browser chrome
- **Fast load** — service worker caches all assets
- **No app store** — no Apple/Google approval needed, no 30% cut
- **Cross-platform** — same app on iOS, Android, Windows, Mac, Linux

### PWA Manifest (`manifest.json`):
```json
{
  "name": "OpenScope 2C53T",
  "short_name": "OpenScope",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#1a1a2e",
  "theme_color": "#00ffff",
  "icons": [
    {"src": "icon-192.png", "sizes": "192x192", "type": "image/png"},
    {"src": "icon-512.png", "sizes": "512x512", "type": "image/png"}
  ]
}
```

### Service Worker (offline caching):
```javascript
// Cache the app shell on install
self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open('openscope-v1').then((cache) => {
      return cache.addAll(['/', '/app.js', '/style.css', '/manifest.json']);
    })
  );
});

// Serve from cache, fall back to network
self.addEventListener('fetch', (event) => {
  event.respondWith(
    caches.match(event.request).then((response) => {
      return response || fetch(event.request);
    })
  );
});
```

### PWA UI Screens:

**1. Scope Display (main screen)**
```
┌──────────────────────────────────────┐
│ OpenScope 2C53T    SCOPE     00:42   │
├──────────────────────────────────────┤
│                                      │
│  Live scope display (from framebuf)  │
│  Pinch to zoom, drag to pan         │
│  Tap waveform for cursor measurement│
│                                      │
├──────────────────────────────────────┤
│ CH1:2V  Auto  H=50uS  CH2:200mV    │
├──────────────────────────────────────┤
│  [CH1] [CH2] [TRIG] [AUTO] [MENU]   │
│     [◄] [▲] [OK] [▼] [►]           │
└──────────────────────────────────────┘
```

**2. Module Store**
```
┌──────────────────────────────────────┐
│ ← Module Store                       │
├──────────────────────────────────────┤
│                                      │
│ 📦 Toyota Diagnostic Suite    v2.1   │
│    K-Line, 847 DTCs, sensor PIDs     │
│    245 KB            [Install]       │
│                                      │
│ 📦 Fuel System Analyzer      v1.3   │
│    Pump current, injector balance    │
│    128 KB            [Install]       │
│                                      │
│ 📦 HVAC Technician Pack      v1.0   │
│    Cap test, compressor analysis     │
│    96 KB          [Installed ✓]      │
│                                      │
│ Installed: 1/4 slots used            │
└──────────────────────────────────────┘
```

**3. Settings / WiFi Config**
```
┌──────────────────────────────────────┐
│ ← Settings                           │
├──────────────────────────────────────┤
│                                      │
│ WiFi Mode                            │
│ ○ Access Point (OpenScope-A3F2)      │
│ ● Connect to network                │
│   SSID: [HomeWiFi________]          │
│   Pass: [••••••••________]          │
│   Status: Connected ✓ 192.168.1.42  │
│                                      │
│ Firmware                             │
│   Current: v0.2.0-dev               │
│   Latest:  v0.2.1                   │
│   [Check for updates]               │
│                                      │
│ Device                               │
│   Battery: 87%                       │
│   Modules: 1 installed              │
│   Uptime: 00:42:17                  │
└──────────────────────────────────────┘
```

## ESP32 Firmware Stack

```
┌─────────────────────────────────────────┐
│ Application Layer                        │
│  ┌──────────┐ ┌────────┐ ┌───────────┐ │
│  │ Web Server│ │Module  │ │OTA Update │ │
│  │ (HTTP)   │ │Store   │ │Manager    │ │
│  └────┬─────┘ └───┬────┘ └─────┬─────┘ │
│       │            │            │        │
│  ┌────▼────────────▼────────────▼─────┐ │
│  │ WebSocket Server                    │ │
│  │ (framebuffer + commands)            │ │
│  └────────────────┬───────────────────┘ │
│                   │                      │
│  ┌────────────────▼───────────────────┐ │
│  │ UART Protocol Handler              │ │
│  │ (packet framing, command dispatch) │ │
│  └────────────────┬───────────────────┘ │
│                   │                      │
│  ┌────────────────▼───────────────────┐ │
│  │ ESP-IDF / Arduino Framework        │ │
│  │ WiFi + BLE + HTTP + SPIFFS         │ │
│  └────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

Recommended framework: **Arduino + ESPAsyncWebServer** for fastest development, or **ESP-IDF** for more control.

## File System on ESP32 Flash (SPIFFS/LittleFS)

```
/
├── index.html          # PWA main page
├── app.js              # Application JavaScript
├── style.css           # Styles
├── manifest.json       # PWA manifest
├── sw.js               # Service worker
├── icon-192.png        # App icon
├── icon-512.png        # App icon (large)
└── config.json         # WiFi credentials, preferences
```

Total PWA size: ~50-100KB. Fits easily in ESP32's 4MB flash with room for buffering module downloads.

## Development Plan

### Phase 1: Basic Bridge (get it working)
- ESP32 Arduino sketch with WiFi AP + WebSocket server
- Forward UART packets between scope and browser
- Simple HTML page showing framebuffer
- Button panel sends commands
- **Deliverable: see scope screen on phone, press buttons remotely**

### Phase 2: PWA + Module Store
- Build full PWA with offline caching
- Module store UI with GitHub registry backend
- Module download → UART transfer → SPI flash
- Install to home screen
- **Deliverable: installable app that manages modules**

### Phase 3: OTA + Cloud
- Firmware update flow via GitHub releases
- Home WiFi client mode (not just AP)
- Cloud data logging (InfluxDB/MQTT)
- Waveform sharing (upload captures)
- **Deliverable: self-updating scope with cloud connectivity**

## Hardware Installation Guide (for mod documentation)

### Tools needed:
- Soldering iron with fine tip (conical or chisel ≤1mm)
- 30 AWG wire (silicone insulated preferred)
- Flush cutters
- Tweezers
- Double-sided foam tape (to mount ESP32 module)
- Phillips screwdriver (for case screws)

### Steps:
1. Remove 4-6 screws from back of 2C53T case
2. Carefully separate case halves (watch for ribbon cables)
3. Photograph the PCB for reference
4. Identify UART pads on GD32F307 (TBD from teardown — look for TP labels or trace from datasheet)
5. Tin the 4 pads (3.3V, GND, TX, RX) and ESP32 module pins
6. Cut 4 lengths of 30AWG wire (~3cm each)
7. Solder wires: ESP32 TX → GD32 RX, ESP32 RX → GD32 TX, 3.3V, GND
8. Mount ESP32 module with foam tape (antenna toward plastic panel)
9. Route wires neatly, ensure no shorts
10. Flash ESP32 firmware via USB before installing (or use USB-C port on ESP32 module after installation)
11. Reassemble case, verify fit
12. Power on — scope should boot normally
13. Look for "OpenScope-XXXX" WiFi network on phone
14. Connect and open browser to 192.168.4.1

### If something goes wrong:
- Scope won't boot → check for shorts, desolder ESP32, verify scope works without it
- No WiFi → check ESP32 power (LED should blink), re-flash ESP32 firmware
- UART not working → verify TX/RX aren't swapped, check baud rate matches
- The $70 scope is not bricked by this mod — the ESP32 is completely passive on the UART until the scope firmware enables communication
