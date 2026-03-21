/*
 * ESP32 Communication Protocol Tests
 *
 * Build:
 *   gcc -o tests/test_esp_comm tests/test_esp_comm.c src/drivers/esp_comm.c \
 *       -Isrc/drivers -O2
 */

#include <stdio.h>
#include <string.h>
#include "esp_comm.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { printf("  FAIL: %s\n", msg); } \
} while(0)

/* Capture UART output */
#define TX_BUF_SIZE 4096
static uint8_t tx_buf[TX_BUF_SIZE];
static int tx_pos = 0;

static void mock_uart_write(uint8_t byte)
{
    if (tx_pos < TX_BUF_SIZE)
        tx_buf[tx_pos++] = byte;
}

static void tx_reset(void)
{
    tx_pos = 0;
    memset(tx_buf, 0, TX_BUF_SIZE);
}

/* Build a packet and feed it to the receiver */
static bool send_packet(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    esp_comm_receive_byte(ESP_SYNC_BYTE);
    uint8_t chk = 0;

    esp_comm_receive_byte(cmd);
    chk ^= cmd;

    uint8_t lh = (uint8_t)(len >> 8);
    uint8_t ll = (uint8_t)(len & 0xFF);
    esp_comm_receive_byte(lh);
    chk ^= lh;
    esp_comm_receive_byte(ll);
    chk ^= ll;

    uint16_t i;
    for (i = 0; i < len; i++) {
        esp_comm_receive_byte(payload[i]);
        chk ^= payload[i];
    }

    return esp_comm_receive_byte(chk);
}

/* Parse a response from tx_buf */
static bool parse_response(uint8_t *cmd_out, uint8_t *payload_out,
                           uint16_t *len_out)
{
    if (tx_pos < 5) return false;
    if (tx_buf[0] != ESP_SYNC_BYTE) return false;

    *cmd_out = tx_buf[1];
    *len_out = (uint16_t)((tx_buf[2] << 8) | tx_buf[3]);

    if (payload_out && *len_out > 0)
        memcpy(payload_out, &tx_buf[4], *len_out);

    /* Verify checksum */
    uint8_t chk = 0;
    int i;
    for (i = 1; i < 4 + *len_out; i++)
        chk ^= tx_buf[i];

    return (tx_buf[4 + *len_out] == chk);
}

/* ─── Tests ─── */

static void test_checksum(void)
{
    printf("\n[Test] Checksum computation\n");

    uint8_t data[] = {0x01, 0x00, 0x00};
    uint8_t chk = esp_comm_checksum(data, 3);
    ASSERT(chk == 0x01, "Checksum of {0x01,0x00,0x00} = 0x01");

    uint8_t data2[] = {0xFF, 0xFF};
    chk = esp_comm_checksum(data2, 2);
    ASSERT(chk == 0x00, "Checksum of {0xFF,0xFF} = 0x00 (XOR)");
}

static void test_packet_receive(void)
{
    printf("\n[Test] Packet receive state machine\n");

    esp_comm_init();
    bool result = send_packet(ESP_CMD_PING, NULL, 0);
    ASSERT(result, "PING packet received successfully");

    const esp_packet_t *pkt = esp_comm_get_packet();
    ASSERT(pkt->valid, "Packet marked valid");
    ASSERT(pkt->cmd == ESP_CMD_PING, "Command is PING");
    ASSERT(pkt->payload_len == 0, "Payload length is 0");
}

static void test_bad_checksum(void)
{
    printf("\n[Test] Bad checksum rejection\n");

    esp_comm_init();

    /* Send packet with wrong checksum */
    esp_comm_receive_byte(ESP_SYNC_BYTE);
    esp_comm_receive_byte(ESP_CMD_PING);
    esp_comm_receive_byte(0x00);
    esp_comm_receive_byte(0x00);
    bool result = esp_comm_receive_byte(0xFF);  /* Wrong checksum */

    ASSERT(!result, "Bad checksum packet rejected");
}

static void test_ping_response(void)
{
    printf("\n[Test] PING command response\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);
    tx_reset();

    send_packet(ESP_CMD_PING, NULL, 0);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[256];
    uint16_t len;
    bool ok = parse_response(&cmd, payload, &len);

    ASSERT(ok, "Response has valid checksum");
    ASSERT(cmd == ESP_RSP_DATA, "Response type is DATA");
    ASSERT(len > 0, "Response has version string");
    ASSERT(payload[0] == '0', "Version starts with '0'");
}

static void test_status_response(void)
{
    printf("\n[Test] STATUS command response\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);
    tx_reset();

    send_packet(ESP_CMD_STATUS, NULL, 0);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[256];
    uint16_t len;
    bool ok = parse_response(&cmd, payload, &len);

    ASSERT(ok, "Status response valid");
    ASSERT(cmd == ESP_RSP_STATUS, "Response type is STATUS");
    ASSERT(len == sizeof(device_status_t), "Status size correct");

    device_status_t *status = (device_status_t *)payload;
    ASSERT(status->battery_pct == 100, "Battery shows 100%");
    ASSERT(status->fw_state == FW_UPDATE_IDLE, "FW state is IDLE");
}

static void test_button_command(void)
{
    printf("\n[Test] BUTTON command\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);
    tx_reset();

    uint8_t btn = ESP_BTN_MENU;
    send_packet(ESP_CMD_BUTTON, &btn, 1);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint16_t len;
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "Button press acknowledged");
}

static void test_module_transfer(void)
{
    printf("\n[Test] Module transfer flow\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);

    /* Start: slot 0, size 512 */
    uint8_t start_payload[] = {0x00, 0x00, 0x00, 0x02, 0x00};  /* slot=0, size=512 */
    tx_reset();
    send_packet(ESP_CMD_MODULE_START, start_payload, 5);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint16_t len;
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "Module start ACK");
    ASSERT(esp_comm_transfer_active(), "Transfer is active");

    /* Send 2 data chunks of 256 bytes */
    uint8_t chunk[256];
    memset(chunk, 0xAB, 256);

    tx_reset();
    send_packet(ESP_CMD_MODULE_DATA, chunk, 256);
    esp_comm_process(esp_comm_get_packet());
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "Data chunk 1 ACK");

    tx_reset();
    send_packet(ESP_CMD_MODULE_DATA, chunk, 256);
    esp_comm_process(esp_comm_get_packet());
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "Data chunk 2 ACK");

    /* End transfer */
    tx_reset();
    send_packet(ESP_CMD_MODULE_END, NULL, 0);
    esp_comm_process(esp_comm_get_packet());
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "Module end ACK");
    ASSERT(!esp_comm_transfer_active(), "Transfer completed");
}

static void test_invalid_slot(void)
{
    printf("\n[Test] Invalid module slot rejection\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);
    tx_reset();

    uint8_t start_payload[] = {0x05, 0x00, 0x00, 0x01, 0x00};  /* slot=5 (invalid) */
    send_packet(ESP_CMD_MODULE_START, start_payload, 5);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[16];
    uint16_t len;
    parse_response(&cmd, payload, &len);
    ASSERT(cmd == ESP_RSP_NAK, "Invalid slot NAK'd");
    ASSERT(payload[0] == ESP_ERR_INVALID_SLOT, "Error code is INVALID_SLOT");
}

static void test_unknown_command(void)
{
    printf("\n[Test] Unknown command rejection\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);
    tx_reset();

    send_packet(0xFF, NULL, 0);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[16];
    uint16_t len;
    parse_response(&cmd, payload, &len);
    ASSERT(cmd == ESP_RSP_NAK, "Unknown command NAK'd");
    ASSERT(payload[0] == ESP_ERR_UNKNOWN_CMD, "Error code is UNKNOWN_CMD");
}

static void test_fw_update_flow(void)
{
    printf("\n[Test] Firmware update flow\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);

    /* Start FW update: size=1024 */
    uint8_t fw_start[] = {0x00, 0x00, 0x04, 0x00};  /* 1024 bytes */
    tx_reset();
    send_packet(ESP_CMD_FW_UPDATE_START, fw_start, 4);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint16_t len;
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "FW update start ACK");
    ASSERT(esp_comm_transfer_active(), "FW transfer active");

    /* Send 4 chunks of 256 bytes */
    uint8_t chunk[256];
    memset(chunk, 0xCD, 256);
    int i;
    for (i = 0; i < 4; i++) {
        tx_reset();
        send_packet(ESP_CMD_FW_UPDATE_DATA, chunk, 256);
        esp_comm_process(esp_comm_get_packet());
        parse_response(&cmd, NULL, &len);
        ASSERT(cmd == ESP_RSP_ACK, "FW data chunk ACK");
    }

    /* Commit */
    tx_reset();
    send_packet(ESP_CMD_FW_UPDATE_COMMIT, NULL, 0);
    esp_comm_process(esp_comm_get_packet());
    parse_response(&cmd, NULL, &len);
    ASSERT(cmd == ESP_RSP_ACK, "FW commit ACK");
    ASSERT(!esp_comm_transfer_active(), "FW transfer done");
}

static void test_double_transfer_rejected(void)
{
    printf("\n[Test] Double transfer rejection\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);

    /* Start a module transfer */
    uint8_t start[] = {0x00, 0x00, 0x00, 0x01, 0x00};
    send_packet(ESP_CMD_MODULE_START, start, 5);
    esp_comm_process(esp_comm_get_packet());

    /* Try to start another while first is active */
    tx_reset();
    send_packet(ESP_CMD_MODULE_START, start, 5);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[16];
    uint16_t len;
    parse_response(&cmd, payload, &len);
    ASSERT(cmd == ESP_RSP_NAK, "Double transfer NAK'd");
    ASSERT(payload[0] == ESP_ERR_TRANSFER_ACTIVE, "Error is TRANSFER_ACTIVE");
}

static void test_module_list(void)
{
    printf("\n[Test] Module list\n");

    esp_comm_init();
    esp_comm_set_writer(mock_uart_write);

    /* Install a module first */
    uint8_t start[] = {0x01, 0x00, 0x00, 0x01, 0x00};  /* slot 1, 256 bytes */
    send_packet(ESP_CMD_MODULE_START, start, 5);
    esp_comm_process(esp_comm_get_packet());

    uint8_t chunk[256];
    memset(chunk, 0, 256);
    send_packet(ESP_CMD_MODULE_DATA, chunk, 256);
    esp_comm_process(esp_comm_get_packet());

    send_packet(ESP_CMD_MODULE_END, NULL, 0);
    esp_comm_process(esp_comm_get_packet());

    /* Request list */
    tx_reset();
    send_packet(ESP_CMD_MODULE_LIST, NULL, 0);
    esp_comm_process(esp_comm_get_packet());

    uint8_t cmd;
    uint8_t payload[512];
    uint16_t len;
    parse_response(&cmd, payload, &len);
    ASSERT(cmd == ESP_RSP_MODULE_LIST, "Module list response");
    ASSERT(len == sizeof(module_slot_info_t) * ESP_MODULE_SLOT_COUNT,
           "Module list size correct");

    module_slot_info_t *list = (module_slot_info_t *)payload;
    ASSERT(!list[0].installed, "Slot 0 empty");
    ASSERT(list[1].installed, "Slot 1 installed");
    ASSERT(list[1].size == 256, "Slot 1 size = 256");
}

/* ─── Main ─── */

int main(void)
{
    printf("===================================\n");
    printf("  ESP32 Communication Protocol Tests\n");
    printf("===================================\n");

    test_checksum();
    test_packet_receive();
    test_bad_checksum();
    test_ping_response();
    test_status_response();
    test_button_command();
    test_module_transfer();
    test_invalid_slot();
    test_unknown_command();
    test_fw_update_flow();
    test_double_transfer_rejected();
    test_module_list();

    printf("\n===================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("===================================\n");

    return tests_passed == tests_run ? 0 : 1;
}
