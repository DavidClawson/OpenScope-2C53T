/*
 * ESP32 Communication Protocol — GD32F307 side
 *
 * Receives commands from ESP32 over UART, dispatches to handlers,
 * sends responses. Handles module transfers and firmware staging.
 */

#include "esp_comm.h"
#include <string.h>

#ifndef ESP_COMM_TRANSFER_STUBS
#define ESP_COMM_TRANSFER_STUBS 0
#endif

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

/* Oversize frame: swallow the bytes it announced (plus checksum) instead of
 * letting them fall through to whatever shares the stream. Bounded by the
 * inter-byte gap timeout, so a lying length cannot deafen the shell. */
static uint32_t rx_discard = 0;
static uint32_t rx_last_ms = 0;
static esp_rx_stats_t rx_stats;

typedef enum {
    RX_NONE = 0,        /* byte consumed, nothing complete */
    RX_PACKET,          /* complete frame, checksum good */
    RX_BAD_CHECKSUM,    /* complete frame, checksum bad */
    RX_BAD_LENGTH,      /* header announced > ESP_MAX_PAYLOAD */
} rx_result_t;

#if ESP_COMM_TRANSFER_STUBS
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
#endif /* ESP_COMM_TRANSFER_STUBS */

/* UART write function (set by caller) */
static esp_write_fn uart_write = 0;
static esp_write_block_fn block_write = 0;
static esp_status_fn status_provider = 0;
static esp_button_fn button_injector = 0;

/* Reported only when no status provider is bound (host tests, bare ESP32
 * bring-up). The firmware binds the real build string. */
static const char fw_version[] = "0.0.0-unbound";

/* ─── Checksum ─── */

uint8_t esp_comm_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t chk = 0;
    uint16_t i;
    if (!data) return 0;
    for (i = 0; i < len; i++)
        chk ^= data[i];
    return chk;
}

/* ─── Init ─── */

void esp_comm_init(void)
{
    rx_state = RX_WAIT_SYNC;
    rx_discard = 0;
    memset(&rx_stats, 0, sizeof(rx_stats));
    memset(&rx_packet, 0, sizeof(rx_packet));
#if ESP_COMM_TRANSFER_STUBS
    memset(&transfer, 0, sizeof(transfer));
    memset(modules, 0, sizeof(modules));
#endif
}

void esp_comm_set_writer(esp_write_fn fn)
{
    uart_write = fn;
}

void esp_comm_set_block_writer(esp_write_block_fn fn)
{
    block_write = fn;
}

void esp_comm_set_status_provider(esp_status_fn fn)
{
    status_provider = fn;
}

void esp_comm_set_button_injector(esp_button_fn fn)
{
    button_injector = fn;
}

void esp_comm_get_rx_stats(esp_rx_stats_t *out)
{
    *out = rx_stats;
}

/* ─── Packet receiver (byte-at-a-time state machine) ─── */

static rx_result_t rx_step(uint8_t byte)
{
    if (rx_discard > 0) {
        rx_discard--;
        return RX_NONE;
    }

    switch (rx_state) {
    case RX_WAIT_SYNC:
        if (byte == ESP_SYNC_BYTE) {
            rx_state = RX_WAIT_CMD;
            rx_checksum_acc = 0;
        }
        return RX_NONE;

    case RX_WAIT_CMD:
        rx_packet.cmd = byte;
        rx_checksum_acc ^= byte;
        rx_state = RX_WAIT_LEN_HI;
        return RX_NONE;

    case RX_WAIT_LEN_HI:
        rx_packet.payload_len = (uint16_t)(byte << 8);
        rx_checksum_acc ^= byte;
        rx_state = RX_WAIT_LEN_LO;
        return RX_NONE;

    case RX_WAIT_LEN_LO:
        rx_packet.payload_len |= byte;
        rx_checksum_acc ^= byte;
        rx_payload_idx = 0;

        if (rx_packet.payload_len > ESP_MAX_PAYLOAD) {
            /* Receive-only cap (§2.2/§3.5): responses may be larger. Eat
             * the rest of this frame so it cannot leak into the shell. */
            rx_discard = (uint32_t)rx_packet.payload_len + ESP_CHECKSUM_SIZE;
            rx_state = RX_WAIT_SYNC;
            rx_stats.bad_length++;
            return RX_BAD_LENGTH;
        }
        rx_state = (rx_packet.payload_len == 0) ? RX_WAIT_CHECKSUM
                                                : RX_WAIT_PAYLOAD;
        return RX_NONE;

    case RX_WAIT_PAYLOAD:
        rx_packet.payload[rx_payload_idx++] = byte;
        rx_checksum_acc ^= byte;
        if (rx_payload_idx >= rx_packet.payload_len)
            rx_state = RX_WAIT_CHECKSUM;
        return RX_NONE;

    case RX_WAIT_CHECKSUM:
        rx_state = RX_WAIT_SYNC;
        rx_packet.valid = (byte == rx_checksum_acc);
        if (rx_packet.valid) {
            rx_stats.frames_ok++;
            return RX_PACKET;
        }
        rx_stats.bad_checksum++;
        return RX_BAD_CHECKSUM;
    }

    rx_state = RX_WAIT_SYNC;
    return RX_NONE;
}

bool esp_comm_receive_byte(uint8_t byte)
{
    return rx_step(byte) == RX_PACKET;
}

bool esp_comm_rx_in_frame(void)
{
    return rx_state != RX_WAIT_SYNC || rx_discard > 0;
}

bool esp_comm_rx_poll(uint32_t now_ms)
{
    if (!esp_comm_rx_in_frame())
        return false;
    if ((uint32_t)(now_ms - rx_last_ms) < ESP_RX_GAP_MS)
        return false;
    rx_state = RX_WAIT_SYNC;
    rx_discard = 0;
    rx_stats.gap_timeouts++;
    /* Deliberately silent. The protocol has no request ids, so a reply
     * nobody is waiting for would be taken by the host as the answer to its
     * NEXT request. The host that abandoned this frame has already timed
     * out; the counter (usbstat) is the record. */
    return true;
}

void esp_comm_route(const uint8_t *data, uint16_t len, uint32_t now_ms,
                    esp_passthrough_fn passthrough, void *ctx)
{
    uint16_t run_start = 0;
    uint16_t i;

    (void)esp_comm_rx_poll(now_ms);

    for (i = 0; i < len; i++) {
        uint8_t b = data[i];

        if (!esp_comm_rx_in_frame() && b != ESP_SYNC_BYTE)
            continue;                       /* belongs to the passthrough run */

        /* Flush the text that preceded this frame byte, in order. */
        if (i > run_start && passthrough)
            passthrough(data + run_start, (uint16_t)(i - run_start), ctx);
        run_start = (uint16_t)(i + 1);

        rx_last_ms = now_ms;
        switch (rx_step(b)) {
        case RX_PACKET:       esp_comm_process(&rx_packet); break;
        case RX_BAD_CHECKSUM: esp_comm_send_nak(ESP_ERR_BAD_CHECKSUM); break;
        case RX_BAD_LENGTH:   esp_comm_send_nak(ESP_ERR_BAD_LENGTH); break;
        case RX_NONE:         break;
        }
    }
    if (len > run_start && passthrough)
        passthrough(data + run_start, (uint16_t)(len - run_start), ctx);
}

const esp_packet_t *esp_comm_get_packet(void)
{
    return &rx_packet;
}

/* ─── Packet sender ─── */

void esp_comm_send_response(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t hdr[ESP_HEADER_SIZE] = {
        ESP_SYNC_BYTE, cmd, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF)
    };
    uint8_t chk = (uint8_t)(cmd ^ hdr[2] ^ hdr[3]) ^ esp_comm_checksum(payload, len);

    if (block_write) {
        /* Three writes, one task: nothing else can interleave on the port. */
        block_write(hdr, ESP_HEADER_SIZE);
        if (len)
            block_write(payload, len);
        block_write(&chk, ESP_CHECKSUM_SIZE);
        return;
    }
    if (!uart_write) return;

    uint16_t i;
    for (i = 0; i < ESP_HEADER_SIZE; i++)
        uart_write(hdr[i]);
    for (i = 0; i < len; i++)
        uart_write(payload[i]);
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
#if ESP_COMM_TRANSFER_STUBS
    return transfer.active;
#else
    return false;   /* staging is not compiled in: RAM is ~1.5 KB from full */
#endif
}

/* ─── Command handlers ─── */

static void snapshot(esp_status_snapshot_t *st)
{
    memset(st, 0, sizeof(*st));
    st->fw_version = fw_version;
    if (status_provider)
        status_provider(st);
    if (!st->fw_version)
        st->fw_version = fw_version;
}

static uint8_t fw_version_len(const char *v)
{
    size_t n = strlen(v);
    return (uint8_t)(n > 32 ? 32 : n);
}

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void handle_ping(const esp_packet_t *pkt)
{
    esp_status_snapshot_t st;
    (void)pkt;
    snapshot(&st);
    esp_comm_send_response(ESP_RSP_DATA, (const uint8_t *)st.fw_version,
                           fw_version_len(st.fw_version));
}

/* STATUS v1 — layout documented in esp_comm.h (ESP_STATUS_FIXED_LEN). */
static void handle_status(const esp_packet_t *pkt)
{
    esp_status_snapshot_t st;
    uint8_t out[ESP_STATUS_FIXED_LEN + 32];
    uint8_t n;
    (void)pkt;

    snapshot(&st);
    n = fw_version_len(st.fw_version);
    out[0] = ESP_PROTO_VERSION;
    out[1] = st.current_mode;
    out[2] = st.battery_pct;
    out[3] = st.flags;
    put_u16(&out[4], st.battery_mv);
    put_u32(&out[6], st.uptime_ms);
    put_u32(&out[10], st.usb_tx_stalls);
    put_u16(&out[14], st.usb_heals);
    out[16] = n;
    memcpy(&out[ESP_STATUS_FIXED_LEN], st.fw_version, n);
    esp_comm_send_response(ESP_RSP_STATUS, out, (uint16_t)(ESP_STATUS_FIXED_LEN + n));
}

static void handle_button(const esp_packet_t *pkt)
{
    if (pkt->payload_len != 1) {
        esp_comm_send_nak(ESP_ERR_BAD_LENGTH);
        return;
    }
    if (pkt->payload[0] < ESP_BTN_CH1 || pkt->payload[0] > ESP_BTN_POWER) {
        esp_comm_send_nak(ESP_ERR_BAD_ARG);
        return;
    }
    if (!button_injector) {
        esp_comm_send_nak(ESP_ERR_UNSUPPORTED);
        return;
    }
    /* ACK only once the press is really queued (§3.7: never report an
     * action that did not happen). */
    if (!button_injector(pkt->payload[0])) {
        esp_comm_send_nak(ESP_ERR_NOT_READY);
        return;
    }
    esp_comm_send_ack();
}

#if ESP_COMM_TRANSFER_STUBS
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

#endif /* ESP_COMM_TRANSFER_STUBS */

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
#if ESP_COMM_TRANSFER_STUBS
    /* ESP32 co-processor staging. The flash writes are still TODO, so these
     * are compiled only where that is understood (the legacy flow tests). */
    case ESP_CMD_MODULE_START:      handle_module_start(pkt); break;
    case ESP_CMD_MODULE_DATA:       handle_module_data(pkt); break;
    case ESP_CMD_MODULE_END:        handle_module_end(pkt); break;
    case ESP_CMD_MODULE_LIST:       handle_module_list(pkt); break;
    case ESP_CMD_MODULE_DELETE:     handle_module_delete(pkt); break;
    case ESP_CMD_FW_UPDATE_START:   handle_fw_update_start(pkt); break;
    case ESP_CMD_FW_UPDATE_DATA:    handle_fw_update_data(pkt); break;
    case ESP_CMD_FW_UPDATE_COMMIT:  handle_fw_update_commit(pkt); break;
#else
    /* Known commands whose handlers would have ACKed data they then dropped
     * (flash writes are TODO). A success report for an action that never
     * happened is the failure mode this project keeps paying for, so they
     * answer UNSUPPORTED until they are real. */
    case ESP_CMD_MODULE_START:
    case ESP_CMD_MODULE_DATA:
    case ESP_CMD_MODULE_END:
    case ESP_CMD_MODULE_LIST:
    case ESP_CMD_MODULE_DELETE:
    case ESP_CMD_FW_UPDATE_START:
    case ESP_CMD_FW_UPDATE_DATA:
    case ESP_CMD_FW_UPDATE_COMMIT:
#endif
    case ESP_CMD_FRAMEBUFFER:
    case ESP_CMD_SIGNAL_CONFIG:
        esp_comm_send_nak(ESP_ERR_UNSUPPORTED);
        break;

    default:
        esp_comm_send_nak(ESP_ERR_UNKNOWN_CMD);
        break;
    }
}
