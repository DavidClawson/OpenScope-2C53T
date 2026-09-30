/*
 * ESP32 Communication Protocol for OpenScope 2C53T
 *
 * UART-based protocol between the GD32F307 (scope MCU) and an ESP32
 * co-processor module. Handles module downloads, firmware updates,
 * framebuffer streaming, and remote button input.
 *
 * Packet format:
 *   [0xAA] [cmd] [len_hi] [len_lo] [payload...] [checksum]
 *   checksum = XOR of all bytes from cmd through end of payload
 *
 * All communication is half-duplex: ESP32 sends command, GD32 responds.
 */

#ifndef ESP_COMM_H
#define ESP_COMM_H

#include <stdint.h>
#include <stdbool.h>

/* Packet framing */
#define ESP_SYNC_BYTE       0xAA
#define ESP_MAX_PAYLOAD     256
#define ESP_HEADER_SIZE     4       /* sync + cmd + len_hi + len_lo */
#define ESP_CHECKSUM_SIZE   1

/* Commands: ESP32 → GD32 */
#define ESP_CMD_PING            0x01    /* Ping — scope replies with version */
#define ESP_CMD_MODULE_START    0x02    /* Begin module transfer (slot + size) */
#define ESP_CMD_MODULE_DATA     0x03    /* Module data chunk (up to 256 bytes) */
#define ESP_CMD_MODULE_END      0x04    /* Finalize module install */
#define ESP_CMD_FW_UPDATE_START 0x05    /* Begin firmware update (size) */
#define ESP_CMD_FW_UPDATE_DATA  0x06    /* Firmware data chunk */
#define ESP_CMD_FW_UPDATE_COMMIT 0x07   /* Mark staged firmware, reboot */
#define ESP_CMD_STATUS          0x08    /* Request device status */
#define ESP_CMD_FRAMEBUFFER     0x09    /* Request current framebuffer */
#define ESP_CMD_BUTTON          0x0A    /* Simulate button press */
#define ESP_CMD_SIGNAL_CONFIG   0x0B    /* Set signal injection config */
#define ESP_CMD_MODULE_LIST     0x0C    /* List installed modules */
#define ESP_CMD_MODULE_DELETE   0x0D    /* Delete a module by slot */

/* Responses: GD32 → ESP32 */
#define ESP_RSP_ACK             0x81    /* Command accepted */
#define ESP_RSP_NAK             0x82    /* Command rejected */
#define ESP_RSP_DATA            0x83    /* Data response */
#define ESP_RSP_FRAMEBUFFER     0x84    /* Framebuffer data (multi-packet) */
#define ESP_RSP_STATUS          0x85    /* Status response */
#define ESP_RSP_MODULE_LIST     0x86    /* Module list response */

/* NAK error codes */
#define ESP_ERR_UNKNOWN_CMD     0x01
#define ESP_ERR_BAD_CHECKSUM    0x02
#define ESP_ERR_BAD_LENGTH      0x03
#define ESP_ERR_FLASH_WRITE     0x04
#define ESP_ERR_FLASH_FULL      0x05
#define ESP_ERR_INVALID_SLOT    0x06
#define ESP_ERR_NOT_READY       0x07
#define ESP_ERR_TRANSFER_ACTIVE 0x08
#define ESP_ERR_UNSUPPORTED     0x09    /* command exists but is not implemented on this build */
#define ESP_ERR_TIMEOUT         0x0A    /* packet abandoned mid-frame (inter-byte gap) */
#define ESP_ERR_NO_CAPTURE_DATA 0x0B    /* remote_protocol.md §3.4 — never substitute the demo trace */
#define ESP_ERR_UNSUPPORTED_IN_MODE 0x0C
#define ESP_ERR_BAD_ARG         0x0D    /* argument out of range */

/* ─── Remote protocol (issue #10, docs/design/remote_protocol.md) ───
 *
 * Wire-format version reported in STATUS byte 0. Bump the major on any
 * incompatible change; the host refuses unknown majors (§3.7). */
#define ESP_PROTO_VERSION       1

/* A frame whose bytes stop arriving for this long is abandoned and the
 * receiver resyncs. Without it a truncated packet leaves the parser in
 * PAYLOAD and, on the shared CDC endpoint, it would swallow the operator's
 * shell text (and any later 0xAA) as payload. USB delivers a host write in
 * back-to-back 64-byte packets, so a real gap of this size means the host
 * gave up, not that it is slow. */
#define ESP_RX_GAP_MS           50

/* STATUS payload v1 — explicit little-endian layout, NOT a C struct dump
 * (a struct's padding and enum width depend on the compiler):
 *   [0]    u8   proto_version   (ESP_PROTO_VERSION)
 *   [1]    u8   current_mode    (device_mode_t: 0 scope 1 meter 2 siggen 3 settings)
 *   [2]    u8   battery_pct     (0..100)
 *   [3]    u8   flags           bit0 charging, bit1 capture data ready,
 *                               bit2 battery critical
 *   [4..5] u16  battery_mv
 *   [6..9] u32  uptime_ms
 *   [10..13] u32 usb_tx_stalls  (CDC IN waits that timed out, issue #39)
 *   [14..15] u16 usb_heals      (transport self-heal reconnects, issue #39)
 *   [16]   u8   fw_len
 *   [17..] char fw_version[fw_len]  (no NUL)
 */
#define ESP_STATUS_FIXED_LEN    17
#define ESP_STATUS_FLAG_CHARGING      0x01
#define ESP_STATUS_FLAG_CAPTURE_READY 0x02
#define ESP_STATUS_FLAG_BATT_CRITICAL 0x04

/* Snapshot the firmware fills in for STATUS/PING. esp_comm itself knows
 * nothing about the device, so the host tests can inject any state. */
typedef struct {
    uint8_t     current_mode;
    uint8_t     battery_pct;
    uint8_t     flags;
    uint16_t    battery_mv;
    uint32_t    uptime_ms;
    uint32_t    usb_tx_stalls;
    uint16_t    usb_heals;
    const char *fw_version;     /* NUL-terminated; truncated to 32 on the wire */
} esp_status_snapshot_t;

typedef void (*esp_status_fn)(esp_status_snapshot_t *out);
/* Inject a button press (id 1..15 = button_id_t). Return false if it could
 * not be queued, so the host gets NAK instead of a false ACK. */
typedef bool (*esp_button_fn)(uint8_t button_id);
/* Block writer: the whole of `len` bytes, in order. Preferred over the
 * byte writer — the USB CDC path sends 64-byte packets, not bytes. */
typedef void (*esp_write_block_fn)(const uint8_t *data, uint16_t len);
/* Where non-protocol bytes go when routing a shared stream (§3.2). */
typedef void (*esp_passthrough_fn)(const uint8_t *data, uint16_t len, void *ctx);

/* Module slots */
#define ESP_MODULE_SLOT_COUNT   4
#define ESP_MODULE_MAX_SIZE     (1024 * 1024)   /* 1MB per slot */

/* Button IDs (matches button_id_t in ui.h) */
#define ESP_BTN_CH1     1
#define ESP_BTN_CH2     2
#define ESP_BTN_MOVE    3
#define ESP_BTN_SELECT  4
#define ESP_BTN_TRIGGER 5
#define ESP_BTN_PRM     6
#define ESP_BTN_AUTO    7
#define ESP_BTN_SAVE    8
#define ESP_BTN_MENU    9
#define ESP_BTN_UP      10
#define ESP_BTN_DOWN    11
#define ESP_BTN_LEFT    12
#define ESP_BTN_RIGHT   13
#define ESP_BTN_OK      14
#define ESP_BTN_POWER   15

/* Firmware update state */
typedef enum {
    FW_UPDATE_IDLE = 0,
    FW_UPDATE_RECEIVING,
    FW_UPDATE_STAGED,
    FW_UPDATE_FAILED,
} fw_update_state_t;

/* Module slot info */
typedef struct {
    char     name[32];
    char     version[16];
    uint32_t size;
    bool     installed;
} module_slot_info_t;

/* Device status (sent in response to STATUS command) */
typedef struct {
    char     fw_version[16];
    uint8_t  current_mode;
    uint8_t  battery_pct;
    uint8_t  num_modules;
    fw_update_state_t fw_state;
} device_status_t;

/* Received packet (parsed) */
typedef struct {
    uint8_t  cmd;
    uint16_t payload_len;
    uint8_t  payload[ESP_MAX_PAYLOAD];
    bool     valid;
} esp_packet_t;

/* ─── Protocol API ─── */

/* Initialize the ESP32 communication handler */
void esp_comm_init(void);

/* Process one byte from UART RX. Call this from UART ISR or polling loop.
 * Returns true when a complete valid packet is ready. */
bool esp_comm_receive_byte(uint8_t byte);

/* Get the last received packet (valid after esp_comm_receive_byte returns true) */
const esp_packet_t *esp_comm_get_packet(void);

/* Process the received packet and generate response.
 * This is the main command dispatcher. */
void esp_comm_process(const esp_packet_t *pkt);

/* Send a response packet over UART.
 * write_byte: function pointer to UART TX byte function */
typedef void (*esp_write_fn)(uint8_t byte);

void esp_comm_set_writer(esp_write_fn fn);

/* Send a raw response */
void esp_comm_send_response(uint8_t cmd, const uint8_t *payload, uint16_t len);

/* Send ACK */
void esp_comm_send_ack(void);

/* Send NAK with error code */
void esp_comm_send_nak(uint8_t error_code);

/* Check if a module transfer or firmware update is in progress */
bool esp_comm_transfer_active(void);

/* ─── Remote protocol bindings ─── */

void esp_comm_set_block_writer(esp_write_block_fn fn);
void esp_comm_set_status_provider(esp_status_fn fn);
void esp_comm_set_button_injector(esp_button_fn fn);

/* True while a frame is being received (sync seen, checksum not yet). */
bool esp_comm_rx_in_frame(void);

/* Abandon a frame whose bytes stopped arriving ESP_RX_GAP_MS ago.
 * Returns true if a frame was abandoned (a NAK(TIMEOUT) is sent). */
bool esp_comm_rx_poll(uint32_t now_ms);

/* Route a chunk from a stream shared with the ASCII shell (§3.2):
 * bytes belonging to a frame (starting at 0xAA) go to the parser and every
 * completed frame is dispatched; all other bytes are handed, in order and
 * in contiguous runs, to `passthrough`. Malformed frames are answered with
 * NAK (bad checksum / bad length) rather than dropped silently. */
void esp_comm_route(const uint8_t *data, uint16_t len, uint32_t now_ms,
                    esp_passthrough_fn passthrough, void *ctx);

/* Diagnostics for `usbstat`/tests. */
typedef struct {
    uint32_t frames_ok;
    uint32_t bad_checksum;
    uint32_t bad_length;
    uint32_t gap_timeouts;
} esp_rx_stats_t;
void esp_comm_get_rx_stats(esp_rx_stats_t *out);

/* Compute XOR checksum */
uint8_t esp_comm_checksum(const uint8_t *data, uint16_t len);

#endif /* ESP_COMM_H */
