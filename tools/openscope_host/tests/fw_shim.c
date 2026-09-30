/* Test shim: the real firmware esp_comm.c behind a C ABI that Python's
 * ctypes can drive, so the host tool is tested against the device's own
 * parser/dispatcher instead of a Python re-implementation of it. */
#include <stdint.h>
#include <string.h>
#include "esp_comm.h"

static uint8_t tx[65536]; static uint32_t tx_n;
static uint8_t sh[65536]; static uint32_t sh_n;
static esp_status_snapshot_t st;
static int inject_ok = 1, last_button = -1, presses = 0;
static esp_meter_snapshot_t mt; static int meter_ok = 0, meter_wrong = 0;

static void wr(const uint8_t *d, uint16_t n) { if (tx_n + n <= sizeof tx) { memcpy(tx + tx_n, d, n); tx_n += n; } }
static void to_shell(const uint8_t *d, uint16_t n, void *c) { (void)c; if (sh_n + n <= sizeof sh) { memcpy(sh + sh_n, d, n); sh_n += n; } }
static void status(esp_status_snapshot_t *o) { *o = st; }
static esp_meter_result_t meter(esp_meter_snapshot_t *o) { if (meter_wrong) return ESP_METER_WRONG_MODE; if (!meter_ok) return ESP_METER_NOT_READY; *o = mt; return ESP_METER_OK; }
void shim_set_meter_wrong_mode(int w) { meter_wrong = w; }
static bool inject(uint8_t id) { if (!inject_ok) return false; last_button = id; presses++; return true; }

void shim_init(void)
{
    esp_comm_init();
    esp_comm_set_block_writer(wr);
    esp_comm_set_status_provider(status);
    esp_comm_set_button_injector(inject);
    esp_comm_set_meter_provider(meter);
    meter_ok = 0; meter_wrong = 0; memset(&mt, 0, sizeof mt);
    tx_n = sh_n = 0; presses = 0; last_button = -1; inject_ok = 1;
    memset(&st, 0, sizeof st);
    st.fw_version = "OpenScope 2C53T shim";
}
void shim_set_status(int mode, int pct, int flags, int mv, uint32_t up, uint32_t stalls, int heals)
{
    st.current_mode = (uint8_t)mode; st.battery_pct = (uint8_t)pct; st.flags = (uint8_t)flags;
    st.battery_mv = (uint16_t)mv; st.uptime_ms = up; st.usb_tx_stalls = stalls; st.usb_heals = (uint16_t)heals;
}
void shim_set_inject_ok(int ok) { inject_ok = ok; }
int  shim_last_button(void) { return last_button; }
int  shim_presses(void) { return presses; }
void shim_feed(const uint8_t *d, uint16_t n, uint32_t now) { esp_comm_route(d, n, now, to_shell, 0); }
int  shim_poll(uint32_t now) { return esp_comm_rx_poll(now); }
uint32_t shim_take_tx(uint8_t *out, uint32_t max) { uint32_t n = tx_n < max ? tx_n : max; memcpy(out, tx, n); memmove(tx, tx + n, tx_n - n); tx_n -= n; return n; }
uint32_t shim_take_shell(uint8_t *out, uint32_t max) { uint32_t n = sh_n < max ? sh_n : max; memcpy(out, sh, n); memmove(sh, sh + n, sh_n - n); sh_n -= n; return n; }
void shim_set_meter(uint32_t count, float value, int bcd, int dp, int cls, int flags, const char *unit, const char *disp)
{
    meter_ok = 1; mt.update_count = count; mt.value = value; mt.raw_bcd = (int16_t)bcd;
    mt.decimal_pos = (uint8_t)dp; mt.result_class = (uint8_t)cls; mt.flags = (uint8_t)flags;
    strncpy(mt.unit, unit, 15); strncpy(mt.display, disp, 15);
}
