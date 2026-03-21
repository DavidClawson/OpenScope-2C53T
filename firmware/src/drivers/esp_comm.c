/*
 * ESP32 Communication Protocol — GD32F307 side
 *
 * Receives commands from ESP32 over UART, dispatches to handlers,
 * sends responses. Handles module transfers and firmware staging.
 */

#include "esp_comm.h"
#include <string.h>

/* ─── Receiver state machine ─── */

typedef enum {
    RX_WAIT_SYNC = 0,
    RX_WAIT_CMD,
    RX_WAIT_LEN_HI,
    RX_WAIT_LEN_LO,
    RX_WAIT_PAYLOAD,
    RX_WAIT_CHECKSUM,
} rx_state_t;

static rx_state_t rx_state = RX_WAIT_SYNC;
static esp_packet_t rx_packet;
static uint16_t rx_payload_idx = 0;
static uint8_t rx_checksum_acc = 0;

/* ─── Transfer state ─── */

typedef struct {
    bool     active;
    bool     is_firmware;       /* true=firmware, false=module */
    uint8_t  slot;              /* module slot (0-3) */
    uint32_t total_size;
    uint32_t received;
    uint32_t flash_offset;      /* current write position in SPI flash */
} transfer_state_t;

static transfer_state_t transfer = {0};

/* Module slot metadata */
static module_slot_info_t modules[ESP_MODULE_SLOT_COUNT];

/* UART write function (set by caller) */
static esp_write_fn uart_write = 0;

/* Firmware version */
static const char fw_version[] = "0.2.0-dev";

/* ─── Checksum ─── */

uint8_t esp_comm_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t chk = 0;
    uint16_t i;
    for (i = 0; i < len; i++)
        chk ^= data[i];
    return chk;
}

/* ─── Init ─── */

void esp_comm_init(void)
{
    rx_state = RX_WAIT_SYNC;
    memset(&rx_packet, 0, sizeof(rx_packet));
    memset(&transfer, 0, sizeof(transfer));
    memset(modules, 0, sizeof(modules));
}

void esp_comm_set_writer(esp_write_fn fn)
{
    uart_write = fn;
}

/* ─── Packet receiver (byte-at-a-time state machine) ─── */

bool esp_comm_receive_byte(uint8_t byte)
{
    switch (rx_state) {
    case RX_WAIT_SYNC:
        if (byte == ESP_SYNC_BYTE) {
            rx_state = RX_WAIT_CMD;
            rx_checksum_acc = 0;
        }
        return false;

    case RX_WAIT_CMD:
        rx_packet.cmd = byte;
        rx_checksum_acc ^= byte;
        rx_state = RX_WAIT_LEN_HI;
        return false;

    case RX_WAIT_LEN_HI:
        rx_packet.payload_len = (uint16_t)(byte << 8);
        rx_checksum_acc ^= byte;
        rx_state = RX_WAIT_LEN_LO;
        return false;

    case RX_WAIT_LEN_LO:
        rx_packet.payload_len |= byte;
        rx_checksum_acc ^= byte;
        rx_payload_idx = 0;

        if (rx_packet.payload_len > ESP_MAX_PAYLOAD) {
            /* Payload too large — reset */
            rx_state = RX_WAIT_SYNC;
            return false;
        }
        if (rx_packet.payload_len == 0) {
            rx_state = RX_WAIT_CHECKSUM;
        } else {
            rx_state = RX_WAIT_PAYLOAD;
        }
        return false;

    case RX_WAIT_PAYLOAD:
        rx_packet.payload[rx_payload_idx++] = byte;
        rx_checksum_acc ^= byte;
        if (rx_payload_idx >= rx_packet.payload_len)
            rx_state = RX_WAIT_CHECKSUM;
        return false;

    case RX_WAIT_CHECKSUM:
        rx_state = RX_WAIT_SYNC;
        if (byte == rx_checksum_acc) {
            rx_packet.valid = true;
            return true;  /* Complete valid packet! */
        }
        rx_packet.valid = false;
        return false;
    }

    rx_state = RX_WAIT_SYNC;
    return false;
}

const esp_packet_t *esp_comm_get_packet(void)
{
    return &rx_packet;
}

/* ─── Packet sender ─── */

void esp_comm_send_response(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    if (!uart_write) return;

    uint8_t chk = 0;

    uart_write(ESP_SYNC_BYTE);

    uart_write(cmd);
    chk ^= cmd;

    uart_write((uint8_t)(len >> 8));
    chk ^= (uint8_t)(len >> 8);

    uart_write((uint8_t)(len & 0xFF));
    chk ^= (uint8_t)(len & 0xFF);

    uint16_t i;
    for (i = 0; i < len; i++) {
        uart_write(payload[i]);
        chk ^= payload[i];
    }

    uart_write(chk);
}

void esp_comm_send_ack(void)
{
    esp_comm_send_response(ESP_RSP_ACK, 0, 0);
}

void esp_comm_send_nak(uint8_t error_code)
{
    esp_comm_send_response(ESP_RSP_NAK, &error_code, 1);
}

bool esp_comm_transfer_active(void)
{
    return transfer.active;
}

/* ─── Command handlers ─── */

static void handle_ping(const esp_packet_t *pkt)
{
    (void)pkt;
    esp_comm_send_response(ESP_RSP_DATA,
                           (const uint8_t *)fw_version,
                           (uint16_t)strlen(fw_version));
}

static void handle_status(const esp_packet_t *pkt)
{
    (void)pkt;
    device_status_t status;
    memset(&status, 0, sizeof(status));
    strncpy(status.fw_version, fw_version, sizeof(status.fw_version) - 1);
    status.current_mode = 0;  /* TODO: get from device state */
    status.battery_pct = 100; /* TODO: read ADC */
    status.fw_state = FW_UPDATE_IDLE;

    uint8_t i;
    for (i = 0; i < ESP_MODULE_SLOT_COUNT; i++) {
        if (modules[i].installed)
            status.num_modules++;
    }

    esp_comm_send_response(ESP_RSP_STATUS,
                           (const uint8_t *)&status, sizeof(status));
}

static void handle_button(const esp_packet_t *pkt)
{
    if (pkt->payload_len < 1) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }
    /* uint8_t button_id = pkt->payload[0]; */
    /* TODO: inject into button queue via xQueueSend(xInputQueue, ...) */
    esp_comm_send_ack();
}

static void handle_module_start(const esp_packet_t *pkt)
{
    if (transfer.active) {
        esp_comm_send_nak(ESP_ERR_TRANSFER_ACTIVE);
        return;
    }
    if (pkt->payload_len < 5) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }

    uint8_t slot = pkt->payload[0];
    uint32_t size = ((uint32_t)pkt->payload[1] << 24) |
                    ((uint32_t)pkt->payload[2] << 16) |
                    ((uint32_t)pkt->payload[3] << 8) |
                    (uint32_t)pkt->payload[4];

    if (slot >= ESP_MODULE_SLOT_COUNT) {
        esp_comm_send_nak(ESP_ERR_INVALID_SLOT);
        return;
    }
    if (size > ESP_MODULE_MAX_SIZE) {
        esp_comm_send_nak(ESP_ERR_FLASH_FULL);
        return;
    }

    transfer.active = true;
    transfer.is_firmware = false;
    transfer.slot = slot;
    transfer.total_size = size;
    transfer.received = 0;
    /* SPI flash offset: slot 0 at 0x100000, slot 1 at 0x200000, etc. */
    transfer.flash_offset = (uint32_t)(slot + 1) * 0x100000;

    /* TODO: erase SPI flash sector for this slot */

    esp_comm_send_ack();
}

static void handle_module_data(const esp_packet_t *pkt)
{
    if (!transfer.active || transfer.is_firmware) {
        esp_comm_send_nak(ESP_ERR_NOT_READY);
        return;
    }

    uint32_t remaining = transfer.total_size - transfer.received;
    uint16_t chunk = pkt->payload_len;
    if (chunk > remaining)
        chunk = (uint16_t)remaining;

    /* TODO: write pkt->payload[0..chunk-1] to SPI flash at
     * transfer.flash_offset + transfer.received */

    transfer.received += chunk;
    esp_comm_send_ack();
}

static void handle_module_end(const esp_packet_t *pkt)
{
    (void)pkt;
    if (!transfer.active || transfer.is_firmware) {
        esp_comm_send_nak(ESP_ERR_NOT_READY);
        return;
    }

    /* Mark module as installed */
    modules[transfer.slot].installed = true;
    modules[transfer.slot].size = transfer.received;

    /* Copy name from first bytes of module if available */
    strncpy(modules[transfer.slot].name, "Module",
            sizeof(modules[transfer.slot].name) - 1);
    strncpy(modules[transfer.slot].version, "1.0",
            sizeof(modules[transfer.slot].version) - 1);

    transfer.active = false;

    /* TODO: write module metadata to SPI flash index */

    esp_comm_send_ack();
}

static void handle_module_list(const esp_packet_t *pkt)
{
    (void)pkt;
    /* Send module slot info for all slots */
    esp_comm_send_response(ESP_RSP_MODULE_LIST,
                           (const uint8_t *)modules,
                           sizeof(modules));
}

static void handle_module_delete(const esp_packet_t *pkt)
{
    if (pkt->payload_len < 1) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }
    uint8_t slot = pkt->payload[0];
    if (slot >= ESP_MODULE_SLOT_COUNT) {
        esp_comm_send_nak(ESP_ERR_INVALID_SLOT);
        return;
    }

    /* TODO: erase SPI flash for this slot */
    memset(&modules[slot], 0, sizeof(module_slot_info_t));
    esp_comm_send_ack();
}

static void handle_fw_update_start(const esp_packet_t *pkt)
{
    if (transfer.active) {
        esp_comm_send_nak(ESP_ERR_TRANSFER_ACTIVE);
        return;
    }
    if (pkt->payload_len < 4) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }

    uint32_t size = ((uint32_t)pkt->payload[0] << 24) |
                    ((uint32_t)pkt->payload[1] << 16) |
                    ((uint32_t)pkt->payload[2] << 8) |
                    (uint32_t)pkt->payload[3];

    transfer.active = true;
    transfer.is_firmware = true;
    transfer.total_size = size;
    transfer.received = 0;
    /* Stage firmware in upper half of flash: 0x08080000 */
    transfer.flash_offset = 0x08080000;

    /* TODO: erase staging flash area */

    esp_comm_send_ack();
}

static void handle_fw_update_data(const esp_packet_t *pkt)
{
    if (!transfer.active || !transfer.is_firmware) {
        esp_comm_send_nak(ESP_ERR_NOT_READY);
        return;
    }

    uint32_t remaining = transfer.total_size - transfer.received;
    uint16_t chunk = pkt->payload_len;
    if (chunk > remaining)
        chunk = (uint16_t)remaining;

    /* TODO: write to staging flash at transfer.flash_offset + transfer.received */

    transfer.received += chunk;
    esp_comm_send_ack();
}

static void handle_fw_update_commit(const esp_packet_t *pkt)
{
    (void)pkt;
    if (!transfer.active || !transfer.is_firmware) {
        esp_comm_send_nak(ESP_ERR_NOT_READY);
        return;
    }

    if (transfer.received < transfer.total_size) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }

    /* TODO: Write "update pending" flag to a known flash location.
     * The bootloader checks this flag on boot and copies
     * staged firmware to the active slot. */

    transfer.active = false;
    esp_comm_send_ack();

    /* TODO: NVIC_SystemReset() to reboot into bootloader */
}

/* ─── Command dispatcher ─── */

void esp_comm_process(const esp_packet_t *pkt)
{
    if (!pkt->valid) {
        esp_comm_send_nak(ESP_ERR_BAD_CHECKSUM);
        return;
    }

    switch (pkt->cmd) {
    case ESP_CMD_PING:              handle_ping(pkt); break;
    case ESP_CMD_STATUS:            handle_status(pkt); break;
    case ESP_CMD_BUTTON:            handle_button(pkt); break;
    case ESP_CMD_MODULE_START:      handle_module_start(pkt); break;
    case ESP_CMD_MODULE_DATA:       handle_module_data(pkt); break;
    case ESP_CMD_MODULE_END:        handle_module_end(pkt); break;
    case ESP_CMD_MODULE_LIST:       handle_module_list(pkt); break;
    case ESP_CMD_MODULE_DELETE:     handle_module_delete(pkt); break;
    case ESP_CMD_FW_UPDATE_START:   handle_fw_update_start(pkt); break;
    case ESP_CMD_FW_UPDATE_DATA:    handle_fw_update_data(pkt); break;
    case ESP_CMD_FW_UPDATE_COMMIT:  handle_fw_update_commit(pkt); break;

    case ESP_CMD_FRAMEBUFFER:
        /* TODO: send current framebuffer as multi-packet response */
        esp_comm_send_ack();
        break;

    case ESP_CMD_SIGNAL_CONFIG:
        /* TODO: parse signal injection config, apply to signal injector */
        esp_comm_send_ack();
        break;

    default:
        esp_comm_send_nak(ESP_ERR_UNKNOWN_CMD);
        break;
    }
}
