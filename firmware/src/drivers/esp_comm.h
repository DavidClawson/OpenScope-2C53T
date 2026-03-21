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

/* Compute XOR checksum */
uint8_t esp_comm_checksum(const uint8_t *data, uint16_t len);

#endif /* ESP_COMM_H */
