/*
 * Remote protocol over the shared CDC stream — host tests (issue #10).
 *
 * Covers what binding esp_comm to USB adds on top of the framing tests in
 * test_esp_comm.c: the §3.2 router (binary frames vs shell text on one
 * stream), the rejection paths that must answer instead of going silent,
 * the inter-byte gap timeout, the explicit STATUS v1 layout, and button
 * injection that only ACKs what was really queued.
 *
 * scripts/test_remote_proto.py builds this against mutated copies of
 * esp_comm.c and requires it to FAIL for each guard removed.
 *
 * Build:  make test-remote-proto
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_comm.h"

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; \
    printf("  FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

/* ─── captured device output ─── */
static uint8_t tx[8192];
static size_t tx_len;
static int block_calls;
static void block_writer(const uint8_t *d, uint16_t n)
{
    block_calls++;
    if (tx_len + n <= sizeof(tx)) { memcpy(tx + tx_len, d, n); tx_len += n; }
}
static uint8_t tx_bytes[8192];
static size_t tx_bytes_len;
static void byte_writer(uint8_t b) { if (tx_bytes_len < sizeof(tx_bytes)) tx_bytes[tx_bytes_len++] = b; }

/* ─── captured shell passthrough ─── */
static char shell[4096];
static size_t shell_len;
static int shell_calls;
static void shell_sink(const uint8_t *d, uint16_t n, void *ctx)
{
    (void)ctx;
    shell_calls++;
    if (shell_len + n < sizeof(shell)) { memcpy(shell + shell_len, d, n); shell_len += n; }
    shell[shell_len] = 0;
}

/* ─── injected device state ─── */
static esp_status_snapshot_t fake_status;
static void status_provider(esp_status_snapshot_t *out) { *out = fake_status; }
static uint8_t injected[16];
static int injected_n;
static bool inject_ok;
static bool button_injector(uint8_t id)
{
    if (!inject_ok) return false;
    injected[injected_n++] = id;
    return true;
}

static void reset(void)
{
    esp_comm_init();
    esp_comm_set_writer(0);
    esp_comm_set_block_writer(block_writer);
    esp_comm_set_status_provider(0);
    esp_comm_set_button_injector(0);
    tx_len = 0; block_calls = 0; tx_bytes_len = 0;
    shell_len = 0; shell[0] = 0; shell_calls = 0;
    injected_n = 0; inject_ok = true;
}

static size_t frame(uint8_t *out, uint8_t cmd, const uint8_t *p, uint16_t n)
{
    out[0] = 0xAA; out[1] = cmd; out[2] = (uint8_t)(n >> 8); out[3] = (uint8_t)n;
    uint8_t c = cmd ^ out[2] ^ out[3];
    for (uint16_t i = 0; i < n; i++) { out[4 + i] = p[i]; c ^= p[i]; }
    out[4 + n] = c;
    return (size_t)n + 5;
}

/* Parse one response frame from tx at *off. Returns cmd or -1. */
static int take(size_t *off, uint8_t *payload, uint16_t *plen)
{
    if (*off + 5 > tx_len || tx[*off] != 0xAA) return -1;
    uint8_t cmd = tx[*off + 1];
    uint16_t n = (uint16_t)((tx[*off + 2] << 8) | tx[*off + 3]);
    if (*off + 5 + n > tx_len) return -1;
    uint8_t c = cmd ^ tx[*off + 2] ^ tx[*off + 3];
    for (uint16_t i = 0; i < n; i++) { c ^= tx[*off + 4 + i]; if (payload) payload[i] = tx[*off + 4 + i]; }
    if (c != tx[*off + 4 + n]) return -1;
    if (plen) *plen = n;
    *off += 5 + n;
    return cmd;
}

static void route(const void *d, size_t n, uint32_t t)
{
    esp_comm_route((const uint8_t *)d, (uint16_t)n, t, shell_sink, 0);
}

/* ─────────────────────────────────────────────────────────────── */

static void test_text_passes_through_untouched(void)
{
    reset();
    route("version\r\n", 9, 0);
    CHECK(strcmp(shell, "version\r\n") == 0, "plain shell text reaches the shell verbatim");
    CHECK(tx_len == 0, "plain text produces no protocol output");
}

static void test_frame_between_text_keeps_order(void)
{
    uint8_t buf[64]; size_t n = 0, off = 0; uint8_t p[64]; uint16_t pl;
    reset();
    memcpy(buf, "ab", 2); n = 2;
    n += frame(buf + n, ESP_CMD_PING, 0, 0);
    memcpy(buf + n, "cd\r", 3); n += 3;
    route(buf, n, 0);
    CHECK(strcmp(shell, "abcd\r") == 0, "text around a frame reaches the shell, frame bytes do not");
    CHECK(shell_calls == 2, "text before and after the frame are separate, ordered runs");
    CHECK(take(&off, p, &pl) == ESP_RSP_DATA, "PING inside a text stream is answered");
}

static void test_frame_split_across_usb_packets(void)
{
    uint8_t buf[16]; size_t n = frame(buf, ESP_CMD_STATUS, 0, 0), off = 0;
    reset();
    for (size_t i = 0; i < n; i++) route(buf + i, 1, (uint32_t)i);   /* 1 byte per packet */
    CHECK(take(&off, 0, 0) == ESP_RSP_STATUS, "a frame split byte-by-byte is reassembled");
    CHECK(shell_len == 0, "no frame byte leaked to the shell");
}

static void test_status_v1_layout(void)
{
    uint8_t buf[16], p[64]; uint16_t pl = 0; size_t off = 0;
    reset();
    fake_status.current_mode = 1;          /* MODE_MULTIMETER */
    fake_status.battery_pct = 73;
    fake_status.flags = ESP_STATUS_FLAG_CHARGING | ESP_STATUS_FLAG_CAPTURE_READY;
    fake_status.battery_mv = 3987;
    fake_status.uptime_ms = 0x01020304;
    fake_status.usb_tx_stalls = 0xA0B0C0D0;
    fake_status.usb_heals = 0x1234;
    fake_status.fw_version = "OpenScope Oct  1 2026 03:00:00";
    esp_comm_set_status_provider(status_provider);
    route(buf, frame(buf, ESP_CMD_STATUS, 0, 0), 0);
    CHECK(take(&off, p, &pl) == ESP_RSP_STATUS, "STATUS answered");
    CHECK(pl == ESP_STATUS_FIXED_LEN + strlen(fake_status.fw_version), "STATUS length = fixed + version");
    CHECK(p[0] == ESP_PROTO_VERSION, "byte 0 = protocol version");
    CHECK(p[1] == 1 && p[2] == 73 && p[3] == 0x03, "mode, battery %, flags");
    CHECK(p[4] == (3987 & 0xFF) && p[5] == (3987 >> 8), "battery_mv little-endian");
    CHECK(p[6] == 0x04 && p[7] == 0x03 && p[8] == 0x02 && p[9] == 0x01, "uptime little-endian");
    CHECK(p[10] == 0xD0 && p[13] == 0xA0, "usb_tx_stalls little-endian");
    CHECK(p[14] == 0x34 && p[15] == 0x12, "usb_heals little-endian");
    CHECK(p[16] == strlen(fake_status.fw_version) &&
          memcmp(p + 17, fake_status.fw_version, p[16]) == 0, "version string, length-prefixed");
}

static void test_status_reflects_live_state(void)
{
    uint8_t buf[16], p[64]; size_t off = 0;
    reset();
    esp_comm_set_status_provider(status_provider);
    fake_status.fw_version = "v"; fake_status.current_mode = 0;
    route(buf, frame(buf, ESP_CMD_STATUS, 0, 0), 0);
    take(&off, p, 0);
    CHECK(p[1] == 0, "mode 0 reported");
    fake_status.current_mode = 2;
    route(buf, frame(buf, ESP_CMD_STATUS, 0, 0), 0);
    take(&off, p, 0);
    CHECK(p[1] == 2, "mode change is visible on the next STATUS (not hardcoded)");
}

static void test_ping_reports_bound_version(void)
{
    uint8_t buf[16], p[64]; uint16_t pl = 0; size_t off = 0;
    reset();
    fake_status.fw_version = "OpenScope 0.4.0+rp";
    esp_comm_set_status_provider(status_provider);
    route(buf, frame(buf, ESP_CMD_PING, 0, 0), 0);
    CHECK(take(&off, p, &pl) == ESP_RSP_DATA, "PING -> DATA");
    CHECK(pl == strlen("OpenScope 0.4.0+rp") && memcmp(p, "OpenScope 0.4.0+rp", pl) == 0,
          "PING carries the firmware's own version string");
}

static void test_bad_checksum_is_answered(void)
{
    uint8_t buf[16], p[8]; size_t n = frame(buf, ESP_CMD_PING, 0, 0), off = 0;
    reset();
    buf[n - 1] ^= 0x5A;
    route(buf, n, 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_BAD_CHECKSUM,
          "corrupted frame gets NAK(BAD_CHECKSUM), not silence");
    CHECK(shell_len == 0, "corrupted frame does not leak into the shell");
}

static void test_oversize_frame_is_swallowed(void)
{
    uint8_t buf[600], p[8]; size_t off = 0, n;
    reset();
    buf[0] = 0xAA; buf[1] = ESP_CMD_PING; buf[2] = 0x01; buf[3] = 0x2C;   /* 300 > 256 */
    for (int i = 0; i < 300; i++) buf[4 + i] = (uint8_t)('A' + i % 26);   /* looks like text */
    buf[304] = 0x00;                                                      /* its checksum */
    n = 305;
    memcpy(buf + n, "help\r", 5); n += 5;
    route(buf, n, 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_BAD_LENGTH,
          "oversize length gets NAK(BAD_LENGTH)");
    CHECK(strcmp(shell, "help\r") == 0,
          "the oversize frame's payload is swallowed; only the text after it reaches the shell");
}

static void test_truncated_frame_times_out_and_shell_recovers(void)
{
    uint8_t buf[16];
    reset();
    size_t n = frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x09", 1);
    route(buf, n - 2, 1000);                     /* host dies mid-frame */
    route("l", 1, 1000 + ESP_RX_GAP_MS - 1);     /* still inside the gap: it is the payload */
    CHECK(shell_len == 0 && esp_comm_rx_in_frame(), "a byte inside the gap window is still a frame byte");
    reset();
    route(buf, n - 2, 1000);
    CHECK(esp_comm_rx_poll(1000 + ESP_RX_GAP_MS) == true, "poll abandons the stale frame after the gap");
    CHECK(tx_len == 0, "abandoning is silent: no unsolicited reply to be mistaken for the next answer");
    route("version\r", 8, 1000 + ESP_RX_GAP_MS + 5);
    CHECK(strcmp(shell, "version\r") == 0, "after the timeout the shell hears the operator again");
    esp_rx_stats_t st; esp_comm_get_rx_stats(&st);
    CHECK(st.gap_timeouts == 1, "gap timeout counted");
}

static void test_route_expires_stale_frame_itself(void)
{
    uint8_t buf[16];
    reset();
    size_t n = frame(buf, ESP_CMD_PING, 0, 0);
    route(buf, n - 1, 0);                        /* no poll call in between */
    route("x\r", 2, 10 * ESP_RX_GAP_MS);
    CHECK(strcmp(shell, "x\r") == 0, "route() itself expires a stale frame before routing new bytes");
}

static void test_stubs_do_not_claim_success(void)
{
    static const uint8_t cmds[] = {
        ESP_CMD_MODULE_START, ESP_CMD_MODULE_DATA, ESP_CMD_MODULE_END,
        ESP_CMD_MODULE_LIST, ESP_CMD_MODULE_DELETE, ESP_CMD_FW_UPDATE_START,
        ESP_CMD_FW_UPDATE_DATA, ESP_CMD_FW_UPDATE_COMMIT, ESP_CMD_FRAMEBUFFER,
        ESP_CMD_SIGNAL_CONFIG };
    uint8_t buf[32], p[8], arg[5] = {0, 0, 0, 1, 0};
    for (size_t i = 0; i < sizeof(cmds); i++) {
        size_t off = 0;
        reset();
        route(buf, frame(buf, cmds[i], arg, sizeof(arg)), 0);
        char msg[80];
        snprintf(msg, sizeof(msg), "unimplemented cmd 0x%02X answers NAK(UNSUPPORTED), not ACK", cmds[i]);
        CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_UNSUPPORTED, msg);
    }
}

static void test_unknown_command(void)
{
    uint8_t buf[16], p[8]; size_t off = 0;
    reset();
    route(buf, frame(buf, 0x7E, 0, 0), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_UNKNOWN_CMD, "unknown cmd -> NAK(UNKNOWN_CMD)");
}

static void test_button_injection(void)
{
    uint8_t buf[16], p[8]; size_t off;

    reset(); off = 0;
    route(buf, frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x09", 1), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_UNSUPPORTED,
          "BUTTON with no injector bound: UNSUPPORTED, not a false ACK");

    reset(); off = 0; esp_comm_set_button_injector(button_injector);
    route(buf, frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x09", 1), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_ACK && injected_n == 1 && injected[0] == 9,
          "BUTTON MENU(9) queued exactly once, then ACK");

    reset(); off = 0; esp_comm_set_button_injector(button_injector); inject_ok = false;
    route(buf, frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x09", 1), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_NOT_READY,
          "queue full: NAK(NOT_READY)");

    reset(); off = 0; esp_comm_set_button_injector(button_injector);
    route(buf, frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x10", 1), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_BAD_ARG && injected_n == 0,
          "button id 16 rejected, nothing injected");

    reset(); off = 0; esp_comm_set_button_injector(button_injector);
    route(buf, frame(buf, ESP_CMD_BUTTON, (const uint8_t *)"\x09\x09", 2), 0);
    CHECK(take(&off, p, 0) == ESP_RSP_NAK && p[0] == ESP_ERR_BAD_LENGTH && injected_n == 0,
          "BUTTON with 2-byte payload rejected");
}

static void test_block_and_byte_writers_agree(void)
{
    uint8_t buf[16];
    reset();
    fake_status.fw_version = "abc";
    esp_comm_set_status_provider(status_provider);
    route(buf, frame(buf, ESP_CMD_STATUS, 0, 0), 0);
    CHECK(block_calls == 3, "one response = header, payload, checksum writes (no per-byte USB sends)");
    size_t a = tx_len; uint8_t copy[128]; memcpy(copy, tx, a);
    esp_comm_set_block_writer(0);
    esp_comm_set_writer(byte_writer);
    route(buf, frame(buf, ESP_CMD_STATUS, 0, 0), 0);
    CHECK(tx_bytes_len == a && memcmp(tx_bytes, copy, a) == 0, "byte writer emits identical bytes");
}

int main(void)
{
    printf("test_remote_proto\n");
    test_text_passes_through_untouched();
    test_frame_between_text_keeps_order();
    test_frame_split_across_usb_packets();
    test_status_v1_layout();
    test_status_reflects_live_state();
    test_ping_reports_bound_version();
    test_bad_checksum_is_answered();
    test_oversize_frame_is_swallowed();
    test_truncated_frame_times_out_and_shell_recovers();
    test_route_expires_stale_frame_itself();
    test_stubs_do_not_claim_success();
    test_unknown_command();
    test_button_injection();
    test_block_and_byte_writers_agree();
    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
