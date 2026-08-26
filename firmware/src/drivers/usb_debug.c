/*
 * OpenScope 2C53T - USB Debug Shell
 *
 * USB CDC virtual serial port for interactive FPGA reverse engineering.
 * Provides a text command interface over USB for sending FPGA commands,
 * reading GPIO/registers, and triggering SPI3 acquisitions without
 * needing to reflash firmware.
 *
 * Usage: screen /dev/tty.usbmodem* 115200
 */

#include "usb_debug.h"
#include "at32f403a_407.h"
#include "usbd_core.h"
#include "usbd_int.h"
#include "cdc_class.h"
#include "cdc_desc.h"
#include "dfu_boot.h"
#include "flash_fs.h"
#include "cal_backup.h"
#include "rtt.h"
#include "continuity_buzzer.h"

/* On the bench unit USB CDC never enumerates (error -71, unsolved), yet
 * usbd_connect_state_get() still reports CONFIGURED — so usb_send_bytes()
 * walks into the CDC TX path and the shell task stalls there instead of
 * returning to poll RTT. Measured 2026-08-11: the banner reached the RTT
 * buffer (rtt_write runs first) and the task never came back.
 *
 * RTT exists precisely to bypass that transport, so this flag cuts the CDC
 * half out entirely. Build with -DDEBUG_SHELL_RTT_ONLY=1.
 *
 * NOTE this is an isolation switch, not the real fix. The underlying defect is
 * that a half-alive USB stack can wedge the shell; the proper repair is to
 * count consecutive CDC TX timeouts and latch the transport dead. */
#ifndef DEBUG_SHELL_RTT_ONLY
#define DEBUG_SHELL_RTT_ONLY 0
#endif
#include "scope_trigger.h"
#include "fpga.h"
#include "lcd.h"
#include "meter_auto.h"
#include "meter_autoselect.h"
#include "meter_data.h"
#include "ui.h"
#include "../ui/scope_state.h"
#include "../util/settings_store.h"
#include "../ui/scope_cal.h"
#include "../ui/scope_freq.h"
#include "../ui/scope_measure.h"
#include "../ui/scope_timebase.h"
#include "../ui/meter_voltage_wave.h"

#include "fpga_cal_table.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════
 * USB Device Instance
 * ═══════════════════════════════════════════════════════════════════ */

static usbd_core_type usb_core_dev;

/* ═══════════════════════════════════════════════════════════════════
 * USB Delay Helpers (required by USB middleware via usb_conf.h)
 * ═══════════════════════════════════════════════════════════════════ */

void usb_delay_ms(uint32_t ms)
{
    /* Use FreeRTOS delay if scheduler is running, otherwise spin */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        volatile uint32_t count;
        while (ms--) {
            count = system_core_clock / 10000;
            while (count--) __asm volatile("nop");
        }
    }
}

void usb_delay_us(uint32_t us)
{
    volatile uint32_t count;
    count = (system_core_clock / 1000000) * us;
    while (count--) __asm volatile("nop");
}

/* ═══════════════════════════════════════════════════════════════════
 * USB Interrupt Handler
 * ═══════════════════════════════════════════════════════════════════ */

void USBFS_L_CAN1_RX0_IRQHandler(void)
{
    usbd_irq_handler(&usb_core_dev);
}

/* ═══════════════════════════════════════════════════════════════════
 * USB CDC Initialization
 * ═══════════════════════════════════════════════════════════════════ */

void usb_debug_init(void)
{
#ifdef EMULATOR_BUILD
    return;  /* No USB in emulator */
#else
    /* At 240MHz, PLL dividers can't produce 48MHz for USB.
     * Use HICK (internal RC oscillator) with ACC calibration instead.
     * This is the standard AT32 approach for non-48/72/96/etc. clocks. */
    crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);

    /* Enable ACC and configure calibration for USB SOF sync */
    crm_periph_clock_enable(CRM_ACC_PERIPH_CLOCK, TRUE);
    acc_write_c1(7980);
    acc_write_c2(8000);
    acc_write_c3(8020);
    acc_calibration_mode_enable(ACC_CAL_HICKTRIM, TRUE);

    /* Enable USB peripheral clock */
    crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);

    /* Enable USB interrupt (low priority, below FreeRTOS syscall ceiling) */
    nvic_irq_enable(USBFS_L_CAN1_RX0_IRQn, 6, 0);

    /* Initialize USB device core with CDC class */
    usbd_core_init(&usb_core_dev, USB, &cdc_class_handler, &cdc_desc_handler, 0);

    /* Enable USB pull-up — device becomes visible to host */
    usbd_connect(&usb_core_dev);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * Output Helpers
 * ═══════════════════════════════════════════════════════════════════ */

bool usb_debug_connected(void)
{
#ifdef EMULATOR_BUILD
    return false;
#else
    return usbd_connect_state_get(&usb_core_dev) == USB_CONN_STATE_CONFIGURED;
#endif
}

/* Send raw bytes to the console.
 *
 * Two transports, both optional: RTT over the SWD wires and USB CDC. Output
 * goes to whichever is live. On the bench unit USB CDC has never enumerated
 * (CLAUDE.local.md), so in practice this is the RTT path — but the CDC path is
 * left intact so a unit with working USB behaves as before. */
static void usb_send_bytes(const uint8_t *data, uint16_t len)
{
    rtt_write(data, len);

#if DEBUG_SHELL_RTT_ONLY
    (void)data; (void)len;
    return;
#else
    if (!usb_debug_connected()) return;

    cdc_struct_type *pcdc = (cdc_struct_type *)usb_core_dev.class_handler->pdata;

    /* Send in 64-byte chunks (USB full-speed max packet) */
    while (len > 0) {
        uint16_t chunk = (len > USBD_CDC_IN_MAXPACKET_SIZE) ?
                         USBD_CDC_IN_MAXPACKET_SIZE : len;

        /* Wait for previous TX to complete (with timeout) */
        uint32_t timeout = 1000;
        while (!pcdc->g_tx_completed && --timeout) {
            if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
                vTaskDelay(1);
        }
        if (!timeout) return;

        usb_vcp_send_data(&usb_core_dev, (uint8_t *)data, chunk);
        data += chunk;
        len -= chunk;
    }
#endif
}

static void usb_send_str(const char *str)
{
    usb_send_bytes((const uint8_t *)str, strlen(str));
}

int usb_debug_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > (int)sizeof(buf) - 1) len = sizeof(buf) - 1;
        usb_send_bytes((const uint8_t *)buf, len);
    }
    return len;
}

/* ═══════════════════════════════════════════════════════════════════
 * Hex Parsing Helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Parse a hex string like "0x40021000" or "40021000" into uint32_t.
 * Returns 0 on success, -1 on error. */
static int parse_hex32(const char *s, uint32_t *out)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    char *end;
    *out = strtoul(s, &end, 16);
    return (*end == '\0' || *end == ' ' || *end == '\r' || *end == '\n') ? 0 : -1;
}

/* Parse a decimal or hex string into uint32_t */
static int parse_int(const char *s, uint32_t *out)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return parse_hex32(s, out);
    char *end;
    *out = strtoul(s, &end, 10);
    return (*end == '\0' || *end == ' ' || *end == '\r' || *end == '\n') ? 0 : -1;
}

/* Parse GPIO port letter + pin number, e.g. "A7" "B11" "C6" */
static int parse_gpio(const char *s, gpio_type **port, uint16_t *pin)
{
    char c = s[0];
    if (c >= 'a' && c <= 'e') c -= 32;  /* to upper */

    switch (c) {
        case 'A': *port = GPIOA; break;
        case 'B': *port = GPIOB; break;
        case 'C': *port = GPIOC; break;
        case 'D': *port = GPIOD; break;
        case 'E': *port = GPIOE; break;
        default: return -1;
    }

    uint32_t n;
    if (parse_int(s + 1, &n) != 0 || n > 15) return -1;
    *pin = (uint16_t)(1 << n);
    return 0;
}

typedef struct {
    uint16_t tx_count;
    uint16_t rx_byte_count;
    uint16_t frame_count;
    uint16_t echo_count;
    uint16_t spi3_ok_count;
} fpga_diag_snapshot_t;

static int parse_byte_args(const char *args, uint8_t *out, size_t expected)
{
    char buf[160];
    char *saveptr = NULL;
    char *tok;

    if (strlen(args) >= sizeof(buf)) return -1;
    strcpy(buf, args);

    tok = strtok_r(buf, " \t", &saveptr);
    for (size_t i = 0; i < expected; i++) {
        uint32_t value;

        if (tok == NULL) return -1;
        if (parse_int(tok, &value) != 0 || value > 0xFF) return -1;
        out[i] = (uint8_t)value;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    return (tok == NULL) ? 0 : -1;
}

static int parse_optional_byte_pair(const char *args, uint8_t *first, uint8_t *second)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok;
    uint32_t value;

    if (args == NULL || *args == '\0') return 0;
    if (strlen(args) >= sizeof(buf)) return -1;
    strcpy(buf, args);

    tok = strtok_r(buf, " \t", &saveptr);
    if (tok == NULL) return 0;
    if (parse_int(tok, &value) != 0 || value > 0xFF) return -1;
    *first = (uint8_t)value;

    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok != NULL) {
        if (parse_int(tok, &value) != 0 || value > 0xFF) return -1;
        *second = (uint8_t)value;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    return (tok == NULL) ? 0 : -1;
}

static int parse_wire_bank_mode(const char *args, uint8_t *bank_mode)
{
    if (args == NULL || *args == '\0' || strcmp(args, "ch1") == 0) {
        *bank_mode = 0;
        return 0;
    }
    if (strcmp(args, "ch2") == 0) {
        *bank_mode = 1;
        return 0;
    }
    if (strcmp(args, "both") == 0 || strcmp(args, "auto") == 0) {
        *bank_mode = 2;
        return 0;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Which FPGA config path ran this boot — the witness the status shell
 * needs before it is allowed to report anything about configuration.
 *
 * WHY THIS EXISTS. On 2026-08-13 the `status` command printed an FPGA
 * config REFUSAL on a device that was configured and capturing: it read
 * hardware-SPI-only fields on a bit-bang (FPGA_CONFIG_B) build, where
 * that path never runs and the fields sit at their zero-init values.
 * Zero is a plausible reading, so the shell reported it as one.
 *
 * The same hole was still open in the other direction. On a build where
 * NO config path runs at all — FPGA_WARM_HANDOFF_TEST (stock configured
 * the part before the handoff) or FPGA_BUS_RELEASED_BOOT — fpga_init()
 * returns before any config sequence, every cfg_* field stays zero, and
 * the old two-way test (marker present? no) labelled that "hardware
 * SPI3" and decoded the all-zero STATUS as "NOT configured". Both halves
 * of that sentence were fabricated.
 *
 * Evidence used, in order:
 *   h2_upload_done  set to 1 by BOTH config paths (fpga.c bit-bang ~2209,
 *                   hardware-SPI ~2562) and by nothing else. Clear => no
 *                   config sequence ran this boot, so NOTHING downstream
 *                   of it may be reported as a measurement.
 *   diag_spi_ctrl1  == 0xBBBB is not a register value; the bit-bang
 *                   sequence stores it as a path marker (fpga.c ~2169).
 *                   The hardware-SPI path leaves the real CTRL1 there.
 *
 * This is inference from runtime witnesses on purpose: the config
 * sequences themselves are bench-validated and are not to be edited to
 * add a flag. `fpga reinit` re-runs the hardware-SPI path from the shell
 * and correctly re-labels the result, because it rewrites both witnesses.
 * ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    CFG_PATH_NONE = 0,   /* no config sequence ran this boot */
    CFG_PATH_BITBANG,    /* fpga_bitbang_config_sequence() (FPGA_CONFIG_B) */
    CFG_PATH_HWSPI,      /* fpga_spi3_config_sequence() */
} cfg_path_t;

static cfg_path_t fpga_cfg_path(void)
{
    if (!fpga.h2_upload_done) return CFG_PATH_NONE;
    return (fpga.diag_spi_ctrl1 == 0xBBBBu) ? CFG_PATH_BITBANG : CFG_PATH_HWSPI;
}

static const char *fpga_cfg_path_name(cfg_path_t p)
{
    switch (p) {
    case CFG_PATH_BITBANG: return "GPIO bit-bang (FPGA_CONFIG_B)";
    case CFG_PATH_HWSPI:   return "hardware SPI3";
    default:               return "NONE — no config sequence ran this boot";
    }
}

static void fpga_diag_snapshot_take(fpga_diag_snapshot_t *snap)
{
    snap->tx_count = fpga.tx_count;
    snap->rx_byte_count = fpga.rx_byte_count;
    snap->frame_count = fpga.frame_count;
    snap->echo_count = fpga.echo_count;
    snap->spi3_ok_count = fpga.spi3_ok_count;
}

static void fpga_diag_print_delta(const fpga_diag_snapshot_t *before)
{
    uint16_t tx_delta = fpga.tx_count - before->tx_count;
    uint16_t rx_delta = fpga.rx_byte_count - before->rx_byte_count;
    uint16_t frame_delta = fpga.frame_count - before->frame_count;
    uint16_t echo_delta = fpga.echo_count - before->echo_count;
    uint16_t spi_delta = fpga.spi3_ok_count - before->spi3_ok_count;

    usb_debug_printf("Delta: TX %+d RX %+d DF %+d EF %+d SPI %+d\r\n",
                     (int)tx_delta, (int)rx_delta, (int)frame_delta,
                     (int)echo_delta, (int)spi_delta);

    if ((frame_delta > 0 || echo_delta > 0) && fpga.rx_frame_valid) {
        usb_debug_printf("RX:");
        for (int i = 0; i < FPGA_RX_FRAME_SIZE; i++) {
            usb_debug_printf(" %02X", fpga.rx_frame[i]);
        }
        usb_send_str("\r\n");
    }
}

static void usb_print_last_tx_frame(void)
{
    usb_send_str("last_tx_frame:");
    for (int i = 0; i < FPGA_TX_FRAME_SIZE; i++) {
        usb_debug_printf(" %02X", (unsigned)fpga.last_tx_frame[i]);
    }
    usb_send_str("\r\n");
}

static void usb_print_recent_tx_frames(void)
{
    uint8_t count = fpga.tx_frame_history_count;

    usb_send_str("tx_frames_recent:");
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = (uint8_t)((fpga.tx_frame_history_head +
                                 FPGA_TX_FRAME_HISTORY - count + i) %
                                FPGA_TX_FRAME_HISTORY);
        usb_send_str(" [");
        for (int j = 0; j < FPGA_TX_FRAME_SIZE; j++) {
            usb_debug_printf("%s%02X",
                             j == 0 ? "" : " ",
                             (unsigned)fpga.tx_frame_history[idx][j]);
        }
        usb_send_str("]");
    }
    usb_send_str("\r\n");
}

static void fpga_diag_clear(void)
{
    taskENTER_CRITICAL();
    fpga.tx_count = 0;
    fpga.rx_byte_count = 0;
    fpga.frame_count = 0;
    fpga.echo_count = 0;
    fpga.rx_sync_data_start_count = 0;
    fpga.rx_sync_echo_start_count = 0;
    fpga.rx_sync_data_header_count = 0;
    fpga.rx_sync_echo_header_count = 0;
    fpga.rx_sync_bad_second_count = 0;
    fpga.rx_sync_stray_count = 0;
    fpga.rx_data_tx_busy_drop_count = 0;
    fpga.rx_echo_valid_count = 0;
    fpga.rx_echo_bad_count = 0;
    memset((void *)fpga.rx_raw_history, 0, sizeof(fpga.rx_raw_history));
    memset((void *)fpga.rx_raw_history_tx_count, 0,
           sizeof(fpga.rx_raw_history_tx_count));
    memset((void *)fpga.rx_raw_history_tx_index, 0,
           sizeof(fpga.rx_raw_history_tx_index));
    memset((void *)fpga.rx_raw_history_rx_index, 0,
           sizeof(fpga.rx_raw_history_rx_index));
    fpga.rx_raw_history_head = 0;
    fpga.rx_raw_history_count = 0;
    fpga_meter_probe_tail_override = -1;
    fpga.spi3_ok_count = 0;
    fpga.spi3_timeout_count = 0;
    fpga.spi3_total_timeouts = 0;
    fpga.h2_rx_00_count = 0;
    fpga.h2_rx_ff_count = 0;
    fpga.h2_rx_other_count = 0;
    fpga.h2_close_rx_len = 0;
    memset((void *)fpga.h2_close_rx, 0, sizeof(fpga.h2_close_rx));
    fpga.rx_frame_valid = false;
    memset((void *)fpga.rx_frame, 0, sizeof(fpga.rx_frame));
    memset((void *)fpga.last_rx_echo_frame, 0, sizeof(fpga.last_rx_echo_frame));
    memset((void *)fpga.last_tx_frame, 0, sizeof(fpga.last_tx_frame));
    memset((void *)fpga.tx_frame_history, 0, sizeof(fpga.tx_frame_history));
    memset((void *)fpga.tx_frame_history_tx_count, 0,
           sizeof(fpga.tx_frame_history_tx_count));
    fpga.tx_frame_history_head = 0;
    fpga.tx_frame_history_count = 0;
    memset((void *)fpga.tx_control_frame_history, 0,
           sizeof(fpga.tx_control_frame_history));
    memset((void *)fpga.tx_control_frame_history_tx_count, 0,
           sizeof(fpga.tx_control_frame_history_tx_count));
    fpga.tx_control_frame_history_head = 0;
    fpga.tx_control_frame_history_count = 0;
    memset((void *)fpga.rx_frame_history, 0, sizeof(fpga.rx_frame_history));
    memset((void *)fpga.rx_history_frame_count, 0, sizeof(fpga.rx_history_frame_count));
    memset((void *)fpga.rx_history_tx_count, 0, sizeof(fpga.rx_history_tx_count));
    memset((void *)fpga.rx_history_echo_count, 0, sizeof(fpga.rx_history_echo_count));
    memset((void *)fpga.rx_history_sequence_count, 0, sizeof(fpga.rx_history_sequence_count));
    memset((void *)fpga.rx_history_sequence_submode, 0, sizeof(fpga.rx_history_sequence_submode));
    memset((void *)fpga.rx_history_discard_remaining, 0, sizeof(fpga.rx_history_discard_remaining));
    memset((void *)fpga.rx_history_transition_busy, 0, sizeof(fpga.rx_history_transition_busy));
    fpga.rx_frame_history_head = 0;
    fpga.rx_frame_history_count = 0;
    memset((void *)fpga.meter_transition_history_submode, 0, sizeof(fpga.meter_transition_history_submode));
    memset((void *)fpga.meter_transition_history_config, 0, sizeof(fpga.meter_transition_history_config));
    memset((void *)fpga.meter_transition_history_selector, 0, sizeof(fpga.meter_transition_history_selector));
    memset((void *)fpga.meter_transition_history_apply, 0, sizeof(fpga.meter_transition_history_apply));
    memset((void *)fpga.meter_transition_history_bank, 0, sizeof(fpga.meter_transition_history_bank));
    memset((void *)fpga.meter_transition_history_bank_first, 0, sizeof(fpga.meter_transition_history_bank_first));
    memset((void *)fpga.meter_transition_history_bank_second, 0, sizeof(fpga.meter_transition_history_bank_second));
    memset((void *)fpga.meter_transition_history_probe, 0, sizeof(fpga.meter_transition_history_probe));
    memset((void *)fpga.meter_transition_history_start, 0, sizeof(fpga.meter_transition_history_start));
    memset((void *)fpga.meter_transition_history_sequence_count, 0, sizeof(fpga.meter_transition_history_sequence_count));
    memset((void *)fpga.meter_transition_history_tx_before, 0, sizeof(fpga.meter_transition_history_tx_before));
    memset((void *)fpga.meter_transition_history_tx_after, 0, sizeof(fpga.meter_transition_history_tx_after));
    memset((void *)fpga.meter_transition_history_frame_before, 0, sizeof(fpga.meter_transition_history_frame_before));
    memset((void *)fpga.meter_transition_history_frame_after, 0, sizeof(fpga.meter_transition_history_frame_after));
    memset((void *)fpga.meter_transition_history_planned_gpio, 0, sizeof(fpga.meter_transition_history_planned_gpio));
    memset((void *)fpga.meter_transition_history_actual_gpio, 0, sizeof(fpga.meter_transition_history_actual_gpio));
    fpga.meter_transition_history_head = 0;
    fpga.meter_transition_history_count = 0;
    memset((void *)fpga.diag_ch1_raw, 0, sizeof(fpga.diag_ch1_raw));
    memset((void *)fpga.diag_ch2_raw, 0, sizeof(fpga.diag_ch2_raw));
    fpga.diag_data_varies = 0;
    taskEXIT_CRITICAL();
    fpga_stock_diag_reset();
}

static void fpga_stock_diag_print(void)
{
    /* MCU-SIDE MODEL, NOT A DEVICE READ. Every field here is written by our own
     * `fpga stock ...` commands (fpga_stock_shadow_write); nothing in it is
     * clocked off the FPGA. All-zero means "not seeded", not "the FPGA reports
     * zero". Labelled because it prints next to blocks that ARE device reads. */
    usb_debug_printf(
        "\r\n=== Stock Shadow (MCU-side model — not read from the FPGA) ===\r\n"
        "F68..6B: %u / %u / %u / %u\r\n"
        "E1A..D:  %u / %u / %u / %u\r\n"
        "0x355:   %u\r\n",
        fpga.stock_shadow.visible_state,
        fpga.stock_shadow.phase,
        fpga.stock_shadow.substate,
        fpga.stock_shadow.flags,
        fpga.stock_shadow.e1a,
        fpga.stock_shadow.e1b,
        fpga.stock_shadow.e1c,
        fpga.stock_shadow.e1d,
        fpga.stock_shadow.latch_355
    );

    usb_debug_printf("E12..19:");
    for (size_t i = 0; i < sizeof(fpga.stock_shadow.detail_bits); i++) {
        usb_debug_printf(" %02X", fpga.stock_shadow.detail_bits[i]);
    }
    usb_send_str("\r\n");
}

static bool fpga_send_cmd_timed(uint8_t cmd_hi, uint8_t cmd_lo, uint32_t delay_ms)
{
    BaseType_t ok = fpga_send_cmd(cmd_hi, cmd_lo);
    if (ok != pdTRUE) {
        usb_debug_printf("Queue full at %02X %02X\r\n", cmd_hi, cmd_lo);
        return false;
    }
    if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Command Handlers
 * ═══════════════════════════════════════════════════════════════════ */

/* cmd_help is defined next to the command table at the bottom of the file —
 * it renders the per-row help strings, so it must live after the table. */
static void cmd_help(void);

static uint16_t count_non_ff_bytes(const uint8_t *bytes, uint8_t len)
{
    uint16_t count = 0;

    for (uint8_t i = 0; i < len; i++) {
        if (bytes[i] != 0xFFU) {
            count++;
        }
    }

    return count;
}

static uint16_t count_post_h2_non_ff_snapshot(
    uint8_t rx_len[FPGA_POST_H2_TRIGGER_HISTORY],
    uint8_t rx[FPGA_POST_H2_TRIGGER_HISTORY][FPGA_POST_H2_RX_HISTORY])
{
    uint16_t count = 0;

    for (uint8_t i = 0; i < FPGA_POST_H2_TRIGGER_HISTORY; i++) {
        uint8_t shown = rx_len[i];
        if (shown > FPGA_POST_H2_RX_HISTORY) {
            shown = FPGA_POST_H2_RX_HISTORY;
        }
        count = (uint16_t)(count + count_non_ff_bytes(rx[i], shown));
    }

    return count;
}

static void cmd_version(void)
{
    usb_debug_printf(
        "OpenScope 2C53T\r\n"
        "Build: " __DATE__ " " __TIME__ "\r\n"
#ifdef FPGA_ALT_BITSTREAM
        "FPGA payload: " FPGA_BITSTREAM_NAME " (ALT — not the stock scope design)\r\n"
#endif
        "MCU: AT32F403A @ %uMHz\r\n"
        "SRAM: 224KB (EOPB0=0xFE)\r\n",
        system_core_clock / 1000000
    );
}

static void cmd_status(void)
{
    extern volatile uint32_t uptime_seconds;

    /*
     * Keep the status header in chunks smaller than usb_debug_printf()'s
     * 256-byte buffer. A single oversized format string silently truncates the
     * output and can glue the following line onto "SPI3 OK", which makes live
     * DMM evidence harder to compare across firmware builds.
     */
    usb_debug_printf(
        "=== System ===\r\n"
        "Uptime: %lus\r\n"
        "SYSCLK: %uMHz\r\n"
        "\r\n=== FPGA ===\r\n"
        "Initialized: %s\r\n"
        "SPI3 active: %s\r\n"
        "TX count: %u\r\n"
        "RX bytes: %u\r\n"
        "Data frames: %u\r\n"
        "Echo frames: %u\r\n"
        "RX sync: data_start=%u echo_start=%u data_hdr=%u echo_hdr=%u bad2=%u stray=%u\r\n",
        (unsigned long)uptime_seconds,
        system_core_clock / 1000000,
        fpga.initialized ? "YES" : "NO",
        fpga.spi3_active ? "YES" : "NO",
        fpga.tx_count,
        fpga.rx_byte_count,
        fpga.frame_count,
        fpga.echo_count,
        fpga.rx_sync_data_start_count,
        fpga.rx_sync_echo_start_count,
        fpga.rx_sync_data_header_count,
        fpga.rx_sync_echo_header_count,
        fpga.rx_sync_bad_second_count,
        fpga.rx_sync_stray_count
    );
    usb_debug_printf(
        "RX gates: data_tx_busy_drop=%u echo_valid=%u echo_bad=%u\r\n",
        fpga.rx_data_tx_busy_drop_count,
        fpga.rx_echo_valid_count,
        fpga.rx_echo_bad_count
    );
    usb_debug_printf(
        "SPI3 OK: %u\r\n"
        "SPI3 timeouts: %u (total %u, hw poll expiries %u)\r\n"
        "SPI3 first byte: 0x%02X\r\n",
        fpga.spi3_ok_count,
        fpga.spi3_timeout_count, fpga.spi3_total_timeouts,
        fpga.spi3_hw_timeouts,
        fpga.spi3_first_byte
    );
    /* Trigger-regime instruments (2026-08-14): PC0 falling edges = fresh
     * data-ready events (sample twice to get a rate); acq headers are the 3
     * MISO bytes of the last 04/05 read — stock's CH1 b2==01 = buffer valid. */
    usb_debug_printf(
        "PC0 edges: %lu\r\n"
        "acq hdr CH1: %02X %02X %02X  CH2: %02X %02X %02X\r\n",
        (unsigned long)fpga.pc0_edges,
        fpga.acq_hdr_ch1[0], fpga.acq_hdr_ch1[1], fpga.acq_hdr_ch1[2],
        fpga.acq_hdr_ch2[0], fpga.acq_hdr_ch2[1], fpga.acq_hdr_ch2[2]
    );
    usb_print_last_tx_frame();
    usb_print_recent_tx_frames();

    /* spi3_first_byte is written only when an acquisition read is actually
     * attempted (fpga.c ~1589 normal path, ~1718 warm-handoff 0x04 path). With
     * no attempt it is still 0x00 from .bss, which is indistinguishable from a
     * real 0x00 off the wire. Counters are the witness: a read attempt either
     * succeeds (ok_count) or times out (total_timeouts). */
    if (fpga.spi3_ok_count == 0 && fpga.spi3_total_timeouts == 0)
        usb_send_str("SPI3 first byte: n/a (no acquisition read attempted yet)\r\n");
    else
        usb_debug_printf("SPI3 first byte: 0x%02X\r\n", fpga.spi3_first_byte);

    /* Split FPGA diag into separate printf to avoid buffer overflow */
    usb_debug_printf(
        "\r\n=== FPGA Diag ===\r\n"
        "IOMUX remap: 0x%08lX (init)\r\n"
        "IOMUX remap5: 0x%08lX (init)\r\n"
        "IOMUX remap LIVE: 0x%08lX\r\n"
        "IOMUX remap5 LIVE: 0x%08lX\r\n"
        /* 0xBBBB is not a register value — fpga_bitbang_config_sequence()
         * stores it as a "this was the bit-bang build" marker. Printing it
         * bare invites reading it as a real CTRL1. */
        "SPI3 CTRL1: 0x%04lX%s  STS: 0x%04lX (both captured at SPI3 init)\r\n"
        "PB4(MISO) IDT: %d  PC6(EN): %d  PB6(CS): %d (live pin reads)\r\n",
        fpga.diag_remap5,
        fpga.diag_remap7,
        (unsigned long)IOMUX->remap,
        (unsigned long)IOMUX->remap5,
        fpga.diag_spi_ctrl1,
        (fpga.diag_spi_ctrl1 == 0xBBBBu) ? " (marker: bit-bang build, not a register read)" : "",
        fpga.diag_spi_sts,
        (GPIOB->idt & (1 << 4)) ? 1 : 0,
        (GPIOC->idt & (1 << 6)) ? 1 : 0,
        (GPIOB->idt & (1 << 6)) ? 1 : 0
    );

    /* ── SPI3 handshake MISO capture ──────────────────────────────────────
     *
     * init_hs[0..10] is written ONLY by fpga_spi3_config_sequence() (fpga.c
     * ~2449-2550). The bit-bang path does not touch it, and a build that runs
     * no config path at all does not either — so on those builds G1/G2/G3 read
     * 00 00 00 00, which is also what a dead MISO line looks like. That is the
     * exact defect fixed for the H2 block on 2026-08-13, still open here.
     *
     * init_hs[11] ("Probe") is separate: it is written in fpga_init() Step 10
     * (fpga.c ~3493), which runs on the hardware-SPI AND bit-bang boots but is
     * skipped by the warm-handoff / bus-released early return. diag_probe_valid
     * is set on the same line, so it is reported on its own witness rather than
     * being lumped in with the handshake bytes. */
    {
        cfg_path_t path = fpga_cfg_path();

        usb_send_str("\r\n=== SPI3 Handshake (11 bytes) ===\r\n");
        if (path == CFG_PATH_HWSPI) {
            usb_debug_printf(
                "G1: %02X %02X %02X %02X  G2: %02X %02X %02X\r\n"
                "G3: %02X %02X %02X %02X\r\n",
                fpga.init_hs[0], fpga.init_hs[1], fpga.init_hs[2], fpga.init_hs[3],
                fpga.init_hs[4], fpga.init_hs[5], fpga.init_hs[6],
                fpga.init_hs[7], fpga.init_hs[8], fpga.init_hs[9], fpga.init_hs[10]);
        } else {
            usb_debug_printf("G1/G2/G3: n/a — never captured (config path: %s)\r\n",
                             fpga_cfg_path_name(path));
        }

        if (fpga.diag_probe_valid)
            usb_debug_printf("Probe (post-init 0xFF): %02X\r\n", fpga.init_hs[11]);
        else
            usb_send_str("Probe (post-init 0xFF): n/a — fpga_init Step 10 did not run\r\n");
    }

    /* ── Config status: the field that actually says whether we configured ──
     *
     * 2026-08-13: this block used to print only the hardware-SPI fields below,
     * unconditionally. Under a bit-bang build (FPGA_CONFIG_B) that path never
     * runs, so `0x3A close status` and `0x03 scope status` sit at their
     * zero-init values — and the shell confidently printed 00 / 00 00 00 00,
     * the exact signature of a REFUSED config, on a device that was configured
     * and capturing. Same family as the /2 status reads and the floating MISO:
     * an instrument reporting on something it cannot see.
     *
     * cfg_status_reg[] is the Gowin STATUS register (0x41), populated by
     * whichever config path ran. It is the authoritative answer, so it goes
     * first and is decoded rather than left as raw hex. */
    {
        uint32_t sr = ((uint32_t)fpga.cfg_status_reg[0] << 24) |
                      ((uint32_t)fpga.cfg_status_reg[1] << 16) |
                      ((uint32_t)fpga.cfg_status_reg[2] << 8)  |
                      (uint32_t)fpga.cfg_status_reg[3];
        cfg_path_t path = fpga_cfg_path();

        usb_debug_printf("\r\n=== FPGA Config ===\r\n"
                         "path: %s\r\n", fpga_cfg_path_name(path));

        if (path == CFG_PATH_NONE) {
            /* Nothing below this line was measured. Say so and print nothing
             * that could be mistaken for a reading — cfg_status_reg,
             * probe_id_bit_close, h2_close_status and scope_status are all
             * still at their .bss zeros, and probe_id_bit_close == 0 in
             * particular would otherwise decode as "anchor matched at bit 0". */
            usb_send_str(
                "STATUS(0x41):  n/a — not read\r\n"
                "IDCODE anchor: n/a — not read\r\n"
                "H2 upload:     n/a — no bitstream sent this boot\r\n"
                "0x3A close status: n/a\r\n"
                "0x03 scope status: n/a\r\n"
                "NOTE: on a warm-handoff or bus-released build the FPGA may be\r\n"
                "      fully configured by stock; this shell simply cannot see it.\r\n");
        } else {
            /* ANCHOR FIRST — the project's mandatory method (CLAUDE.md, Exp J).
             * fpga.c reads IDCODE at /256 in the SAME window as this status, so
             * the status is only believed on a read path validated against a
             * known answer. The shell never showed it, which left STATUS looking
             * like a bare fact. Exp L: a part that configured successfully STOPS
             * answering SSPI, so id_bit_close < 0 is the success-shaped reading
             * and id_bit_close >= 0 proves the port never closed. */
            usb_debug_printf(
                "STATUS(0x41): %08lX  DONE_FINAL(13): %s\r\n"
                "IDCODE anchor after close: %s\r\n",
                (unsigned long)sr,
                ((sr >> 13) & 1u) ? "SET — configured" : "clear — NOT configured",
                (fpga.probe_id_bit_close < 0)
                    ? "silent (config port closed — consistent with configured;"
                      " a dead bus reads the same)"
                    : "still answering 0x0120681B => port OPEN => NOT configured");

            usb_debug_printf("\r\n=== H2 Bitstream Upload ===\r\n"
                             "Bytes sent: %lu / 115638\r\n"
                             "Upload done: %s\r\n",
                             fpga.h2_bytes_sent,
                             fpga.h2_upload_done ? "YES" : "NO");

            /* The two hardware-SPI-only readbacks. Suppress them on the bit-bang
             * path rather than printing zeros that read as a refusal. */
            if (path == CFG_PATH_BITBANG) {
                usb_send_str("0x3A close status: n/a (bit-bang path does not read it)\r\n"
                             "0x03 scope status: n/a (bit-bang path does not read it)\r\n");
            } else {
                usb_debug_printf(
                    "0x3A close status: %02X (stock: F8)\r\n"
                    "0x03 scope status: %02X %02X %02X %02X (stock: 00 01 42 2E)\r\n",
                    fpga.h2_close_status,
                    fpga.scope_status[0], fpga.scope_status[1],
                    fpga.scope_status[2], fpga.scope_status[3]);
            }
        }

        usb_debug_printf(
            "post-H2 SPI3 boot: enq=%u run=%u drop=%u mask=0x%02X\r\n",
            fpga.post_h2_spi3_boot_enqueued,
            fpga.post_h2_spi3_boot_run_count,
            fpga.post_h2_spi3_boot_dropped,
            fpga.post_h2_spi3_boot_mask);
        for (uint8_t i = 0; i < FPGA_POST_H2_TRIGGER_HISTORY; i++) {
            uint8_t len = fpga.post_h2_spi3_rx_len[i];
            usb_debug_printf("post-H2 rx[%u]: trigger=%02X len=%u bytes=",
                             (unsigned)i,
                             (unsigned)fpga.post_h2_spi3_trigger[i],
                             (unsigned)len);
            uint8_t shown = len;
            if (shown > FPGA_POST_H2_RX_HISTORY) shown = FPGA_POST_H2_RX_HISTORY;
            for (uint8_t j = 0; j < shown; j++) {
                usb_debug_printf("%s%02X", j == 0 ? "" : " ",
                                 (unsigned)fpga.post_h2_spi3_rx[i][j]);
            }
            if (len > FPGA_POST_H2_RX_HISTORY) usb_send_str(" ...");
            usb_send_str("\r\n");
        }
    }

    fpga_stock_diag_print();

    /* Show last RX frame if valid */
    if (fpga.rx_frame_valid) {
        usb_debug_printf("Last RX frame:");
        for (int i = 0; i < FPGA_RX_FRAME_SIZE; i++)
            usb_debug_printf(" %02X", fpga.rx_frame[i]);
        usb_send_str("\r\n");
    }
}

/* Can a byte actually leave PA2 right now?
 *
 * USART-silent builds (FPGA_USART_SILENT_SCOPE — coldtrace, warm handoff)
 * configure USART2's registers but never set UEN/TE, so every send path in
 * this shell, queued or polled, is a no-op that reports success. Print the
 * refusal here once and let each command bail. */
static bool usart_tx_wire_live(void)
{
    uint32_t ctrl1 = fpga_usart_ctrl1();
    unsigned uen = (unsigned)((ctrl1 >> 13) & 1u);
    unsigned ten = (unsigned)((ctrl1 >> 3) & 1u);

    if (uen && ten) return true;

    usb_debug_printf("REFUSED: USART2 is dark (CTRL1=%08lX UEN=%u TE=%u) — nothing can\r\n"
                     "         leave PA2. This is a USART-silent build. Run `fpga usart on`\r\n"
                     "         first, then re-issue this command.\r\n",
                     (unsigned long)ctrl1, uen, ten);
    return false;
}

/* Send one 2-byte command and report HOW it went out, not just that it did.
 * Returns false when nothing was transmitted. */
static bool usart_send_cmd_reporting(uint8_t cmd_hi, uint8_t cmd_lo)
{
    if (!usart_tx_wire_live()) return false;

    if (!fpga_usart_tx_task_exists()) {
        /* The queue has no consumer on this build — go straight out the
         * polled path rather than parking the frame forever. */
        fpga_send_cmd_direct(cmd_hi, cmd_lo);
        usb_debug_printf("TX [%02X %02X]: sent DIRECT (polled) — dvom_TX drain task does not\r\n"
                         "  exist on this build, so the queue would never have been emptied.\r\n",
                         cmd_hi, cmd_lo);
        return true;
    }

    if (fpga_send_cmd(cmd_hi, cmd_lo) != pdTRUE) {
        usb_debug_printf("TX [%02X %02X]: QUEUE FULL — NOT SENT\r\n", cmd_hi, cmd_lo);
        return false;
    }
    usb_debug_printf("TX [%02X %02X]: queued (dvom_TX will drain)\r\n", cmd_hi, cmd_lo);
    return true;
}

static void cmd_usart_tx(const char *args)
{
    /* Parse space-separated hex bytes, e.g. "00 09 00 00 00 00 00 00" */
    uint8_t bytes[8];
    int count = 0;

    const char *p = args;
    while (*p && count < 8) {
        while (*p == ' ') p++;
        if (!*p) break;
        uint32_t val;
        if (parse_hex32(p, &val) != 0 || val > 0xFF) {
            usb_debug_printf("ERR: bad hex byte at '%s'\r\n", p);
            return;
        }
        bytes[count++] = (uint8_t)val;
        while (*p && *p != ' ') p++;
    }

    if (count < 2) {
        usb_send_str("Usage: usart tx <cmd_hi> <cmd_lo>\r\n"
                      "  e.g.: usart tx 00 09\r\n");
        return;
    }

    /* Two ways this command used to lie, both of which have already voided a
     * bench experiment (exp(02), 2026-08-16 — recorded VOID, not negative):
     *
     *   1. fpga_send_cmd() only ENQUEUES. On coldtrace/warm-handoff builds the
     *      dvom_TX drain task is never created, so the frame sits in the queue
     *      forever while this command prints "queued" and the operator reads
     *      the absence of a reply as a measurement.
     *   2. Those same builds leave USART2 disabled (CTRL1 UEN clear,
     *      FPGA_USART_SILENT_SCOPE), so even the polled path cannot shift a
     *      byte out of PA2.
     *
     * So: check the wire first and refuse when it is dark, and when only the
     * drain task is missing, send on the polled path and SAY that is what
     * happened. */
    if (!usart_send_cmd_reporting(bytes[0], bytes[1])) return;

    /* Wait briefly for echo/response, then show last RX frame */
    vTaskDelay(pdMS_TO_TICKS(200));
    if (fpga.rx_frame_valid) {
        usb_debug_printf("RX:");
        for (int i = 0; i < FPGA_RX_FRAME_SIZE; i++)
            usb_debug_printf(" %02X", fpga.rx_frame[i]);
        usb_send_str("\r\n");
    } else {
        usb_send_str("RX: (no frame)\r\n");
    }
}

static void cmd_usart_raw(const char *args)
{
    /* Parse exactly 10 space-separated hex bytes for a raw USART2 frame */
    uint8_t frame[10];
    int count = 0;
    fpga_diag_snapshot_t before;

    const char *p = args;
    while (*p && count < 10) {
        while (*p == ' ') p++;
        if (!*p) break;
        uint32_t val;
        if (parse_hex32(p, &val) != 0 || val > 0xFF) {
            usb_debug_printf("ERR: bad hex byte at '%s'\r\n", p);
            return;
        }
        frame[count++] = (uint8_t)val;
        while (*p && *p != ' ') p++;
    }

    if (count != 10) {
        usb_send_str("Usage: usart raw <10 hex bytes>\r\n"
                      "  e.g.: usart raw 00 00 00 0B 01 00 00 00 00 0B\r\n"
                      "  Format: [hdr0][hdr1][cmd_hi][cmd_lo][p1][p2][p3][p4][p5][cksum]\r\n");
        return;
    }

    if (!usart_tx_wire_live()) return;

    usb_debug_printf("TX raw:");
    for (int i = 0; i < 10; i++)
        usb_debug_printf(" %02X", frame[i]);
    usb_send_str("\r\n");

    fpga_diag_snapshot_take(&before);
    fpga_send_raw_frame(frame);

    /* Wait for response */
    vTaskDelay(pdMS_TO_TICKS(200));
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_frame(const char *args)
{
    uint8_t frame[10] = {0};
    uint8_t bytes[8];
    int count = 0;
    const char *p = args;
    fpga_diag_snapshot_t before;

    while (*p && count < 8) {
        while (*p == ' ') p++;
        if (!*p) break;

        uint32_t val;
        if (parse_int(p, &val) != 0 || val > 0xFF) {
            usb_debug_printf("ERR: bad byte at '%s'\r\n", p);
            return;
        }

        bytes[count++] = (uint8_t)val;
        while (*p && *p != ' ') p++;
    }

    while (*p == ' ') p++;
    if (*p != '\0') {
        usb_send_str("ERR: too many bytes\r\n");
        return;
    }

    if (count < 2) {
        usb_send_str("Usage: fpga frame <hi> <lo> [p1 p2 p3 p4 p5 [ck]]\r\n"
                     "  Builds [00 00 hi lo p1 p2 p3 p4 p5 ck]\r\n"
                     "  Default ck = (hi + lo) & 0xFF\r\n"
                     "  e.g.: fpga frame 00 0B 01 00 00 00 00\r\n");
        return;
    }

    frame[2] = bytes[0];
    frame[3] = bytes[1];
    for (int i = 2; i < count && i < 7; i++) {
        frame[i + 2] = bytes[i];
    }

    if (count >= 8) {
        frame[9] = bytes[7];
    } else {
        frame[9] = (uint8_t)((frame[2] + frame[3]) & 0xFF);
    }

    if (!usart_tx_wire_live()) return;

    usb_debug_printf("TX frame:");
    for (int i = 0; i < 10; i++) {
        usb_debug_printf(" %02X", frame[i]);
    }
    usb_send_str("\r\n");

    fpga_diag_snapshot_take(&before);
    fpga_send_raw_frame(frame);
    vTaskDelay(pdMS_TO_TICKS(200));
    fpga_diag_print_delta(&before);
}

/* ═══════════════════════════════════════════════════════════════════
 * GPIO mode inspection — the instrument check `gpio set` never had
 *
 * WHY THIS EXISTS. `gpio set` writes scr/clr unconditionally. On a pin left
 * as an INPUT those writes only pick a pull resistor; the pin is not driven,
 * the command still prints "PA6 -> HIGH", and the experiment records a false
 * negative. That has happened three separate times (PA6 and PD12 both boot as
 * floating inputs; a stale PC12 HIGH from an earlier command silently killed
 * the analog path so a later test measured a dead one).
 *
 * The cure is the same one the FPGA work already learned from the /2 reads
 * and the floating MISO: never let a command report success for something it
 * cannot actually have done. So `gpio set` now reads the pin's config nibble
 * and REFUSES when the pin is not a plain output, and `gpio mode` exists to
 * change it deliberately.
 *
 * Nibble layout (AT32 cfglr/cfghr, STM32F1-compatible — the docs and bench
 * scripts in this repo call these registers CRL/CRH, so the shell does too):
 *   bits[1:0] MODE  0 = input, 1 = out 10MHz, 2 = out 2MHz, 3 = out 50MHz
 *   bits[3:2] CNF   input:  0 analog, 1 floating, 2 pull (up/down per ODT)
 *                   output: 0 push-pull, 1 open-drain, 2 AF-PP, 3 AF-OD
 * So nibble 0/4/8 = input, 1/2/3 = output PP, 5/6/7 = output OD, 9/A/B =
 * AF push-pull, D/E/F = AF open-drain.
 * ═══════════════════════════════════════════════════════════════════ */

/* 'A'..'E' for a port pointer (GPIO blocks are 0x400 apart). */
static char gpio_port_letter(gpio_type *port)
{
    return (char)('A' + (((uint32_t)port - (uint32_t)GPIOA) / 0x400));
}

/* &cfglr for pins 0-7, &cfghr for pins 8-15. */
static volatile uint32_t *gpio_cfg_reg(gpio_type *port, uint8_t pin_no)
{
    return (pin_no < 8) ? &port->cfglr : &port->cfghr;
}

static const char *gpio_cfg_reg_name(uint8_t pin_no)
{
    return (pin_no < 8) ? "CRL" : "CRH";
}

static uint8_t gpio_cfg_nibble(gpio_type *port, uint8_t pin_no)
{
    return (uint8_t)((*gpio_cfg_reg(port, pin_no) >> ((pin_no & 7) * 4)) & 0xF);
}

/* Read-modify-write of ONLY this pin's nibble — the pattern the bench scripts
 * have been doing by hand through `mem read`/`mem write`. */
static void gpio_cfg_nibble_set(gpio_type *port, uint8_t pin_no, uint8_t nibble)
{
    volatile uint32_t *reg = gpio_cfg_reg(port, pin_no);
    uint32_t shift = (uint32_t)(pin_no & 7) * 4;
    *reg = (*reg & ~(0xFu << shift)) | (((uint32_t)nibble & 0xFu) << shift);
}

static bool gpio_mode_is_output(uint8_t nibble)  { return (nibble & 0x3) != 0; }
static bool gpio_mode_is_af(uint8_t nibble)      { return gpio_mode_is_output(nibble) && (nibble & 0x8) != 0; }

/* Human name for a nibble, rendered into a CALLER-OWNED buffer (>= 16 bytes)
 * so two nibbles can be named in one printf — a shared static would alias and
 * print the same mode twice. `odt_bit` only disambiguates pull-up vs pull-down
 * on an input, where ODT selects the resistor rather than a drive level. */
#define GPIO_MODE_NAME_LEN 16
static const char *gpio_mode_name(uint8_t nibble, uint8_t odt_bit, char *out)
{
    static const char *const SPEED[4] = { "?", "10MHz", "2MHz", "50MHz" };
    const char *cnf;

    if (!gpio_mode_is_output(nibble)) {
        switch ((nibble >> 2) & 0x3) {
            case 0:  cnf = "analog in";   break;
            case 1:  cnf = "floating in"; break;
            case 2:  cnf = odt_bit ? "pull-up in" : "pull-down in"; break;
            default: cnf = "reserved in"; break;
        }
        snprintf(out, GPIO_MODE_NAME_LEN, "%s", cnf);
        return out;
    }

    cnf = ((nibble >> 2) & 0x3) == 0 ? "out PP" :
          ((nibble >> 2) & 0x3) == 1 ? "out OD" :
          ((nibble >> 2) & 0x3) == 2 ? "AF PP"  : "AF OD";
    snprintf(out, GPIO_MODE_NAME_LEN, "%s %s", cnf, SPEED[nibble & 0x3]);
    return out;
}

/* ═══════════════════════════════════════════════════════════════════
 * Bench pin registry
 *
 * Every pin this firmware DRIVES on purpose. One list, used by three
 * consumers so they cannot drift apart: `fpga selftest` (is the instrument
 * sane before I trust a measurement?), `bench snapshot`/`bench restore`
 * (did a previous experiment leak state into this one?), and the "we treat
 * this as an output" claim that `fpga selftest` checks against reality.
 *
 * The ROLE strings are deliberately hedged where the project's own evidence
 * is hedged — PB11 in particular (see cmd_gpio_scan). A table that asserts
 * a pin's function more confidently than the evidence does is the same
 * defect as printing an unmeasured value.
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    gpio_type  *port;
    uint8_t     pin_no;
    const char *role;
} bench_pin_t;

static const bench_pin_t BENCH_PINS[] = {
    { GPIOC, 12, "scope input coupling (HIGH = DC passes)" },
    { GPIOE,  4, "CH1 attenuator ladder bit 0" },
    { GPIOE,  5, "CH1 attenuator ladder bit 1" },
    { GPIOE,  6, "CH1 attenuator ladder bit 2" },
    { GPIOA, 15, "CH2 input relay (bench 2026-08-15)" },
    { GPIOB, 11, "\"active mode\" — DISPUTED, likely CH2 attenuator" },
    { GPIOB, 10, "gain select (CH2 bank)" },
    { GPIOA, 10, "gain select (CH2 bank)" },
    { GPIOA,  6, "analog frontend control (boots as FLOATING INPUT)" },
    { GPIOC, 11, "meter MUX enable" },
    { GPIOC,  6, "FPGA SPI enable (must be HIGH)" },
    { GPIOB,  6, "FPGA SPI3 chip select (idle HIGH)" },
    { GPIOD, 12, "coupling (bench 2026-08-15; boots as FLOATING INPUT)" },
    { GPIOD, 13, "coupling (bench 2026-08-15)" },
};
#define BENCH_PIN_COUNT (sizeof(BENCH_PINS) / sizeof(BENCH_PINS[0]))

/* One line of the pin table: mode nibble decoded, plus the LEVEL. For an
 * output the meaningful level is what we drive (ODT); for an input it is what
 * the world puts there (IDT). Both are printed so neither can be mistaken for
 * the other — reading IDT on an output and calling it "what we set" is how a
 * shorted relay line reads back as success. */
static void bench_pin_print_row(const bench_pin_t *bp, bool flag_inputs)
{
    uint8_t nib = gpio_cfg_nibble(bp->port, bp->pin_no);
    uint8_t odt = (bp->port->odt & (1u << bp->pin_no)) ? 1 : 0;
    uint8_t idt = (bp->port->idt & (1u << bp->pin_no)) ? 1 : 0;
    char mode[GPIO_MODE_NAME_LEN];

    usb_debug_printf("  P%c%-2u  %s %X  %-13s  odt=%u idt=%u%s\r\n",
                     gpio_port_letter(bp->port), bp->pin_no,
                     gpio_cfg_reg_name(bp->pin_no), nib,
                     gpio_mode_name(nib, odt, mode), odt, idt,
                     (flag_inputs && !gpio_mode_is_output(nib))
                         ? "   <== INPUT, but we drive it: writes are INERT"
                         : (flag_inputs && gpio_mode_is_af(nib))
                               ? "   <== ALT-FUNCTION: a peripheral owns it"
                               : "");
    usb_debug_printf("        %s\r\n", bp->role);
}

/* Saved GPIO posture, so an experiment can be made hermetic. */
typedef struct {
    uint8_t nibble;
    uint8_t odt;
} bench_pin_state_t;

static bench_pin_state_t bench_snap[BENCH_PIN_COUNT];
static bool              bench_snap_valid = false;

static void cmd_bench_snapshot(void)
{
    usb_send_str("=== bench snapshot: frontend/FPGA pin posture saved ===\r\n");
    for (unsigned i = 0; i < BENCH_PIN_COUNT; i++) {
        bench_snap[i].nibble = gpio_cfg_nibble(BENCH_PINS[i].port, BENCH_PINS[i].pin_no);
        bench_snap[i].odt    = (BENCH_PINS[i].port->odt & (1u << BENCH_PINS[i].pin_no)) ? 1 : 0;
        bench_pin_print_row(&BENCH_PINS[i], false);
    }
    bench_snap_valid = true;
}

static void cmd_bench_restore(void)
{
    unsigned changed = 0;

    if (!bench_snap_valid) {
        usb_send_str("ERR: nothing snapshotted this boot — run `bench snapshot` first\r\n");
        return;
    }

    usb_send_str("=== bench restore ===\r\n");
    for (unsigned i = 0; i < BENCH_PIN_COUNT; i++) {
        const bench_pin_t *bp = &BENCH_PINS[i];
        uint8_t nib_now = gpio_cfg_nibble(bp->port, bp->pin_no);
        uint8_t odt_now = (bp->port->odt & (1u << bp->pin_no)) ? 1 : 0;

        if (nib_now == bench_snap[i].nibble && odt_now == bench_snap[i].odt) continue;

        /* Level first, then mode: writing ODT while the pin is still an input
         * only moves the pull, so the pin cannot glitch to the wrong drive
         * level on the way back to being an output. */
        if (bench_snap[i].odt) bp->port->scr = (uint16_t)(1u << bp->pin_no);
        else                   bp->port->clr = (uint16_t)(1u << bp->pin_no);
        gpio_cfg_nibble_set(bp->port, bp->pin_no, bench_snap[i].nibble);

        usb_debug_printf("  P%c%-2u  %s %X odt=%u  ->  %s %X odt=%u\r\n",
                         gpio_port_letter(bp->port), bp->pin_no,
                         gpio_cfg_reg_name(bp->pin_no), nib_now, odt_now,
                         gpio_cfg_reg_name(bp->pin_no), bench_snap[i].nibble,
                         bench_snap[i].odt);
        changed++;
    }
    usb_debug_printf("%u pin(s) restored, %u already matched\r\n",
                     changed, (unsigned)BENCH_PIN_COUNT - changed);
}

/* `gpio mode <port><pin> out|in` — deliberate, single-pin read-modify-write of
 * the config nibble. `out` = push-pull 50MHz (nibble 3, what gpio_init gives
 * with GPIO_DRIVE_STRENGTH_STRONGER); `in` = floating input (nibble 4). */
static void cmd_gpio_mode(const char *args)
{
    gpio_type *port;
    uint16_t pin;
    uint8_t pin_no, old_nib, new_nib, odt;
    char old_name[GPIO_MODE_NAME_LEN], new_name[GPIO_MODE_NAME_LEN];
    char pin_str[8];
    const char *space = strchr(args, ' ');
    int len;

    if (!space) {
        usb_send_str("Usage: gpio mode <port><pin> <out|in>\r\n"
                     "  e.g.: gpio mode A6 out   (push-pull 50MHz, nibble 3)\r\n"
                     "        gpio mode A6 in    (floating input, nibble 4)\r\n");
        return;
    }

    len = space - args;
    if (len >= (int)sizeof(pin_str)) { usb_send_str("ERR: bad pin\r\n"); return; }
    memcpy(pin_str, args, len);
    pin_str[len] = '\0';

    if (parse_gpio(pin_str, &port, &pin) != 0) {
        usb_send_str("ERR: bad pin (e.g. A7, B11, C6)\r\n");
        return;
    }
    pin_no = (uint8_t)__builtin_ctz(pin);

    while (*space == ' ') space++;
    if      (strcmp(space, "out") == 0) new_nib = 0x3;
    else if (strcmp(space, "in")  == 0) new_nib = 0x4;
    else {
        usb_send_str("ERR: mode must be 'out' (push-pull 50MHz) or 'in' (floating)\r\n");
        return;
    }

    old_nib = gpio_cfg_nibble(port, pin_no);
    odt = (port->odt & pin) ? 1 : 0;
    (void)gpio_mode_name(old_nib, odt, old_name);
    (void)gpio_mode_name(new_nib, odt, new_name);

    if (gpio_mode_is_af(old_nib)) {
        usb_debug_printf("WARN: P%c%u was alternate-function (%s %X = %s) — a peripheral\r\n"
                         "      owned this pin and will now be disconnected from it.\r\n",
                         gpio_port_letter(port), pin_no,
                         gpio_cfg_reg_name(pin_no), old_nib, old_name);
    }
    gpio_cfg_nibble_set(port, pin_no, new_nib);

    usb_debug_printf("P%c%u %s nibble %X (%s) -> %X (%s)\r\n",
                     gpio_port_letter(port), pin_no, gpio_cfg_reg_name(pin_no),
                     old_nib, old_name, new_nib, new_name);
}

static void cmd_gpio_set(const char *args)
{
    /* Parse "<port><pin> <0|1>" e.g. "B11 1" */
    gpio_type *port;
    uint16_t pin;

    const char *space = strchr(args, ' ');
    if (!space) {
        usb_send_str("Usage: gpio set <port><pin> <0|1>\r\n");
        return;
    }

    /* Temporary null-terminate the pin spec */
    char pin_str[8];
    int len = space - args;
    if (len >= (int)sizeof(pin_str)) { usb_send_str("ERR: bad pin\r\n"); return; }
    memcpy(pin_str, args, len);
    pin_str[len] = '\0';

    if (parse_gpio(pin_str, &port, &pin) != 0) {
        usb_send_str("ERR: bad pin (e.g. A7, B11, C6)\r\n");
        return;
    }

    uint32_t val;
    if (parse_int(space + 1, &val) != 0 || val > 1) {
        usb_send_str("ERR: value must be 0 or 1\r\n");
        return;
    }

    /* REFUSE rather than pretend. On an input pin scr/clr only picks a pull
     * resistor — the pin is not driven — and three experiments have already
     * been invalidated by this command reporting HIGH/LOW anyway. On an AF
     * pin a peripheral owns the output and our write is equally inert. */
    {
        uint8_t pin_no = (uint8_t)__builtin_ctz(pin);
        uint8_t nib = gpio_cfg_nibble(port, pin_no);
        char mode[GPIO_MODE_NAME_LEN];

        if (!gpio_mode_is_output(nib)) {
            usb_debug_printf("REFUSED: P%c%u is a %s (%s nibble %X) — not driven.\r\n"
                             "         Use `gpio mode %c%u out` first.\r\n",
                             gpio_port_letter(port), pin_no,
                             gpio_mode_name(nib, (port->odt & pin) ? 1 : 0, mode),
                             gpio_cfg_reg_name(pin_no), nib,
                             gpio_port_letter(port), pin_no);
            return;
        }
        if (gpio_mode_is_af(nib)) {
            usb_debug_printf("REFUSED: P%c%u is %s (%s nibble %X) — a peripheral drives it,\r\n"
                             "         scr/clr writes are inert. Use `gpio mode %c%u out` to take it.\r\n",
                             gpio_port_letter(port), pin_no,
                             gpio_mode_name(nib, 0, mode),
                             gpio_cfg_reg_name(pin_no), nib,
                             gpio_port_letter(port), pin_no);
            return;
        }
    }

    if (val)
        port->scr = pin;    /* Set */
    else
        port->clr = pin;    /* Clear */

    /* Read back IDT: on a driven pin it is the pin itself, so a line clamped
     * by the outside world shows up here instead of being reported as set. */
    usb_debug_printf("P%c%d -> %s  (idt reads %u)\r\n",
                     gpio_port_letter(port),
                     __builtin_ctz(pin),
                     val ? "HIGH" : "LOW",
                     (port->idt & pin) ? 1u : 0u);
}

static void cmd_buzzer_test(const char *args)
{
    uint32_t duration = 750;
    bool started = false;
    bool active = false;
    uint32_t toggles = 0;
    uint32_t create_failures = 0;

    if (args && *args) {
        if (parse_int(args, &duration) != 0 || duration > 5000U) {
            usb_send_str("Usage: buzzer test [ms<=5000]\r\n");
            return;
        }
    }

    continuity_buzzer_force_ms(duration);
    continuity_buzzer_snapshot(&started, &active, &toggles, &create_failures);
    usb_debug_printf("buzzer forced_ms=%lu task=%u active=%u toggles=%lu create_fail=%lu\r\n",
                     duration,
                     started ? 1U : 0U,
                     active ? 1U : 0U,
                     toggles,
                     create_failures);
}

static void cmd_gpio_read(const char *args)
{
    gpio_type *port;
    uint16_t pin;

    if (parse_gpio(args, &port, &pin) != 0) {
        usb_send_str("ERR: bad pin (e.g. A7, B11, C6)\r\n");
        return;
    }

    /* Print the MODE alongside the level. "PA6 = 0" on a floating input and on
     * a driven-low output are the same three characters and mean entirely
     * different things; the shell should not make the reader guess which. */
    uint8_t pin_no = (uint8_t)__builtin_ctz(pin);
    uint8_t nib = gpio_cfg_nibble(port, pin_no);
    uint8_t odt = (port->odt & pin) ? 1 : 0;
    char mode[GPIO_MODE_NAME_LEN];

    usb_debug_printf("P%c%u = %u  [%s %X = %s, odt=%u]\r\n",
                     gpio_port_letter(port), pin_no,
                     (port->idt & pin) ? 1u : 0u,
                     gpio_cfg_reg_name(pin_no), nib,
                     gpio_mode_name(nib, odt, mode), odt);
}

static void cmd_gpio_scan(void)
{
    /* Live IDT reads throughout — every number below is measured, not cached.
     * The LABELS are the fallible part: PB11's is flagged because the project
     * no longer believes it (ripcord static sweep 2026-08-12 — stock writes
     * PB11 only from CH2 analog range/cal code, never in master_init or the
     * config window, so it is probably a CH2 attenuator relay). A diagnostic
     * that asserts a pin's function more confidently than the evidence does is
     * the same failure as printing an unmeasured value. */
    usb_send_str("=== FPGA Control Pins ===\r\n");
    usb_debug_printf("PC6  (SPI enable):  %d\r\n", (GPIOC->idt & (1 << 6))  ? 1 : 0);
    usb_debug_printf("PB11 (\"active mode\" — name DISPUTED, likely CH2 atten): %d\r\n",
                     (GPIOB->idt & (1 << 11)) ? 1 : 0);
    usb_debug_printf("PC0  (data ready):  %d\r\n", (GPIOC->idt & (1 << 0))  ? 1 : 0);
    usb_debug_printf("PC11 (meter MUX):   %d\r\n", (GPIOC->idt & (1 << 11)) ? 1 : 0);

    usb_send_str("\r\n=== SPI3 Pins ===\r\n");
    usb_debug_printf("PB3  (SCK):  %d\r\n",  (GPIOB->idt & (1 << 3)) ? 1 : 0);
    usb_debug_printf("PB4  (MISO): %d\r\n",  (GPIOB->idt & (1 << 4)) ? 1 : 0);
    usb_debug_printf("PB5  (MOSI): %d\r\n",  (GPIOB->idt & (1 << 5)) ? 1 : 0);
    usb_debug_printf("PB6  (CS):   %d\r\n",  (GPIOB->idt & (1 << 6)) ? 1 : 0);

    usb_send_str("\r\n=== Analog Frontend ===\r\n");
    usb_debug_printf("PC12 (input route): %d\r\n", (GPIOC->idt & (1 << 12)) ? 1 : 0);
    usb_debug_printf("PE4  (range):       %d\r\n", (GPIOE->idt & (1 << 4))  ? 1 : 0);
    usb_debug_printf("PE5  (atten):       %d\r\n", (GPIOE->idt & (1 << 5))  ? 1 : 0);
    usb_debug_printf("PE6  (atten):       %d\r\n", (GPIOE->idt & (1 << 6))  ? 1 : 0);

    usb_send_str("\r\n=== Gain Resistors ===\r\n");
    usb_debug_printf("PA15 (gain):  %d\r\n", (GPIOA->idt & (1 << 15)) ? 1 : 0);
    usb_debug_printf("PA10 (gain):  %d\r\n", (GPIOA->idt & (1 << 10)) ? 1 : 0);
    usb_debug_printf("PB10 (gain):  %d\r\n", (GPIOB->idt & (1 << 10)) ? 1 : 0);
    usb_debug_printf("PB9  (afe):   %d\r\n", (GPIOB->idt & (1 << 9))  ? 1 : 0);
    usb_debug_printf("PA6  (afe):   %d\r\n", (GPIOA->idt & (1 << 6))  ? 1 : 0);
}

static void cmd_mem_read(const char *args)
{
    /* Parse "<addr> [count]" */
    uint32_t addr;
    const char *space = strchr(args, ' ');
    char addr_str[16];

    if (space) {
        int len = space - args;
        if (len >= (int)sizeof(addr_str)) { usb_send_str("ERR: addr too long\r\n"); return; }
        memcpy(addr_str, args, len);
        addr_str[len] = '\0';
    } else {
        strncpy(addr_str, args, sizeof(addr_str) - 1);
        addr_str[sizeof(addr_str) - 1] = '\0';
    }

    if (parse_hex32(addr_str, &addr) != 0) {
        usb_send_str("Usage: mem read <hex_addr> [count]\r\n");
        return;
    }

    uint32_t count = 1;
    if (space) parse_int(space + 1, &count);
    if (count > 64) count = 64;

    /* Align to 4 bytes */
    addr &= ~3u;

    for (uint32_t i = 0; i < count; i++) {
        volatile uint32_t *p = (volatile uint32_t *)(addr + i * 4);
        if (i % 4 == 0) usb_debug_printf("0x%08lX:", addr + i * 4);
        usb_debug_printf(" %08lX", *p);
        if (i % 4 == 3 || i == count - 1) usb_send_str("\r\n");
    }
}

static void cmd_mem_write(const char *args)
{
    /* Parse "<addr> <value>" */
    uint32_t addr, value;
    const char *space = strchr(args, ' ');
    if (!space) {
        usb_send_str("Usage: mem write <hex_addr> <hex_value>\r\n");
        return;
    }

    char addr_str[16];
    int len = space - args;
    if (len >= (int)sizeof(addr_str)) { usb_send_str("ERR: addr too long\r\n"); return; }
    memcpy(addr_str, args, len);
    addr_str[len] = '\0';

    if (parse_hex32(addr_str, &addr) != 0 || parse_hex32(space + 1, &value) != 0) {
        usb_send_str("Usage: mem write <hex_addr> <hex_value>\r\n");
        return;
    }

    addr &= ~3u;
    *(volatile uint32_t *)addr = value;
    usb_debug_printf("0x%08lX <- 0x%08lX\r\n", addr, value);
}

static void cmd_flash_jedec(void)
{
    uint8_t manufacturer = 0;
    uint8_t memory_type = 0;
    uint8_t capacity = 0;

    flash_fs_error_t err = flash_fs_raw_read_jedec(&manufacturer, &memory_type, &capacity);
    if (err != FLASH_FS_OK) {
        usb_debug_printf("ERR: flash jedec failed (%d)\r\n", (int)err);
        return;
    }

    usb_debug_printf("SPI flash JEDEC: %02X %02X %02X\r\n",
                     manufacturer, memory_type, capacity);
}

static void cmd_flash_read(const char *args)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok;
    uint32_t addr;
    uint32_t len;
    uint8_t data[256];

    if (strlen(args) >= sizeof(buf)) {
        usb_send_str("Usage: flash read <addr> <len>\r\n");
        return;
    }

    strcpy(buf, args);
    tok = strtok_r(buf, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, &addr) != 0) {
        usb_send_str("Usage: flash read <addr> <len>\r\n");
        return;
    }

    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, &len) != 0 || len == 0 || len > sizeof(data)) {
        usb_send_str("Usage: flash read <addr> <len>\r\n");
        usb_send_str("  len must be 1..256\r\n");
        return;
    }

    flash_fs_error_t err = flash_fs_raw_read_bytes(addr, data, len);
    if (err != FLASH_FS_OK) {
        usb_debug_printf("ERR: flash read failed (%d)\r\n", (int)err);
        return;
    }

    usb_debug_printf("Flash read 0x%06lX (%lu bytes):\r\n", addr, len);
    for (uint32_t i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            usb_debug_printf("0x%06lX:", addr + i);
        }
        usb_debug_printf(" %02X", data[i]);
        if ((i % 16) == 15 || i == (len - 1)) {
            usb_send_str("\r\n");
        }
    }
}

static void cmd_flash_dump(const char *args)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok;
    uint32_t addr;
    uint32_t len;
    uint8_t chunk[256];

    if (strlen(args) >= sizeof(buf)) {
        usb_send_str("Usage: flash dump <addr> <len>\r\n");
        return;
    }

    strcpy(buf, args);
    tok = strtok_r(buf, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, &addr) != 0) {
        usb_send_str("Usage: flash dump <addr> <len>\r\n");
        return;
    }

    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, &len) != 0 || len == 0 || len > 4096) {
        usb_send_str("Usage: flash dump <addr> <len>\r\n");
        usb_send_str("  len must be 1..4096\r\n");
        return;
    }

    usb_debug_printf("FLASHDUMP %lu\r\n", len);

    while (len > 0) {
        uint32_t this_len = (len > sizeof(chunk)) ? sizeof(chunk) : len;
        flash_fs_error_t err = flash_fs_raw_read_bytes(addr, chunk, this_len);
        if (err != FLASH_FS_OK) {
            usb_debug_printf("\r\nERR: flash dump failed (%d)\r\n", (int)err);
            return;
        }

        usb_send_bytes(chunk, (uint16_t)this_len);
        addr += this_len;
        len -= this_len;
    }
}

/*
 * Self-protecting write-primitive bench test (V2 confirmation for the faithful
 * reimpl of the stock W25Q write driver — flash_fs_raw_program/sector_erase/
 * write_block). NON-DESTRUCTIVE by contract:
 *   - addr MUST be 4KB-sector-aligned;
 *   - the ENTIRE 4KB sector must already be erased (all 0xFF) — refuses otherwise,
 *     so it can only ever touch blank flash;
 *   - exercises both write paths (in-place page-program + erase/read-modify-write)
 *     then erases the sector back to 0xFF, restoring the pre-test state exactly.
 * Requires an explicit CONFIRM token: `flash wtest <addr> CONFIRM`.
 */
static void cmd_flash_diag(void)
{
    uint8_t sr[4] = {0};
    if (flash_fs_raw_status_diag(sr) != FLASH_FS_OK) { usb_send_str("ERR: status diag\r\n"); return; }
    usb_debug_printf("SR1=0x%02X SR2=0x%02X SR3=0x%02X | SR1-after-WREN=0x%02X\r\n",
                     sr[0], sr[1], sr[2], sr[3]);
    usb_debug_printf("  BP/protect bits (SR1&0x7C)=0x%02X  WEL-after-WREN=%d  BUSY=%d\r\n",
                     sr[0] & 0x7C, (sr[3] >> 1) & 1, sr[0] & 1);
}

static void cmd_flash_wtest(const char *args)
{
    /* Lean stack footprint: the usb_dbg task has only 2KB of stack, so this uses a
     * single 64-byte work buffer (NOT 256-byte arrays — that overflowed the task). */
    char abuf[40];
    char *saveptr = NULL;
    uint32_t addr;
    uint8_t b[64];
    const uint32_t TLEN = sizeof(b);   /* 64-byte test window within the sector */

    if (strlen(args) >= sizeof(abuf)) { usb_send_str("Usage: flash wtest <addr> CONFIRM\r\n"); return; }
    strcpy(abuf, args);

    char *t_addr = strtok_r(abuf, " \t", &saveptr);
    char *t_conf = strtok_r(NULL, " \t", &saveptr);
    if (t_addr == NULL || parse_int(t_addr, &addr) != 0) {
        usb_send_str("Usage: flash wtest <addr> CONFIRM\r\n"); return;
    }
    if (t_conf == NULL || strcmp(t_conf, "CONFIRM") != 0) {
        usb_send_str("Refused: append CONFIRM. This writes external flash.\r\n"); return;
    }
    if (addr & 0xFFFu) {
        usb_send_str("Refused: addr must be 4KB-sector-aligned (mask 0xFFF).\r\n"); return;
    }

    /* Safety: the whole 4KB sector must be blank (0xFF) so the restoring erase is
     * guaranteed non-destructive. */
    for (uint32_t off = 0; off < 4096; off += sizeof(b)) {
        if (flash_fs_raw_read_bytes(addr + off, b, sizeof(b)) != FLASH_FS_OK) {
            usb_send_str("ERR: pre-read failed\r\n"); return;
        }
        for (uint32_t i = 0; i < sizeof(b); i++) {
            if (b[i] != 0xFF) {
                usb_debug_printf("Refused: sector not blank (byte 0x%lX = 0x%02X). Pick an erased sector.\r\n",
                                 (unsigned long)(addr + off + i), b[i]);
                return;
            }
        }
    }
    usb_debug_printf("wtest @0x%lX: sector blank, OK to proceed\r\n", (unsigned long)addr);

    /* Path 1: program-in-place (target erased) — ascending pattern. */
    for (uint32_t i = 0; i < TLEN; i++) b[i] = (uint8_t)i;
    if (flash_fs_raw_write_block(addr, b, TLEN) != FLASH_FS_OK) { usb_send_str("ERR: write_block#1\r\n"); goto restore; }
    if (flash_fs_raw_read_bytes(addr, b, TLEN) != FLASH_FS_OK) { usb_send_str("ERR: readback#1\r\n"); goto restore; }
    {
        int bad = -1;
        for (uint32_t i = 0; i < TLEN; i++) if (b[i] != (uint8_t)i) { bad = (int)i; break; }
        if (bad >= 0) {
            usb_debug_printf("FAIL: in-place mismatch @+%d; readback[0..7]=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                             bad, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            goto restore;
        }
    }
    usb_send_str("PASS: page-program (in-place)\r\n");

    /* Path 2: erase + read-modify-write (target now non-0xFF) — inverse pattern. */
    for (uint32_t i = 0; i < TLEN; i++) b[i] = (uint8_t)(0xFF - i);
    if (flash_fs_raw_write_block(addr, b, TLEN) != FLASH_FS_OK) { usb_send_str("ERR: write_block#2\r\n"); goto restore; }
    if (flash_fs_raw_read_bytes(addr, b, TLEN) != FLASH_FS_OK) { usb_send_str("ERR: readback#2\r\n"); goto restore; }
    for (uint32_t i = 0; i < TLEN; i++) if (b[i] != (uint8_t)(0xFF - i)) { usb_send_str("FAIL: erase+RMW mismatch\r\n"); goto restore; }
    usb_send_str("PASS: erase + read-modify-write\r\n");

restore:
    /* Restore: erase the sector back to all-0xFF (its pre-test state). */
    if (flash_fs_raw_sector_erase(addr) != FLASH_FS_OK) { usb_send_str("ERR: restore erase\r\n"); return; }
    {
        bool blank = true;
        for (uint32_t off = 0; off < 4096 && blank; off += sizeof(b)) {
            if (flash_fs_raw_read_bytes(addr + off, b, sizeof(b)) != FLASH_FS_OK) { blank = false; break; }
            for (uint32_t i = 0; i < sizeof(b); i++) if (b[i] != 0xFF) { blank = false; break; }
        }
        usb_debug_printf("%s: sector restored to 0xFF\r\n", blank ? "PASS" : "FAIL");
    }
}

/*
 * Factory-cal self-protection (src/util/cal_backup.c).
 *   cal status              report the MCU cal page and the W25Q backup
 *   cal backup              copy the MCU cal page to the W25Q (safe: W25Q only)
 *   cal restore [force] CONFIRM   write the W25Q backup back to MCU flash
 * Restore refuses to overwrite a programmed (precious) page unless `force`.
 */
static void cmd_cal_status(void)
{
    cal_backup_report_t r;
    if (cal_backup_status(&r) != CAL_BK_OK) {
        usb_send_str("cal status: read failed\r\n");
        return;
    }
    usb_debug_printf("live  @0x%08lX  %-14s crc=0x%08lX\r\n",
                     (unsigned long)CAL_BACKUP_SRC_ADDR,
                     cal_page_class_str(r.live_class),
                     (unsigned long)r.live_crc);
    if (!r.backup_present) {
        usb_send_str("backup: region unreadable\r\n");
    } else if (r.backup_status != CAL_REC_OK) {
        usb_debug_printf("backup: %s (no valid record)\r\n",
                         cal_rec_status_str(r.backup_status));
    } else {
        usb_debug_printf("backup: valid v%u  payload_crc=0x%08lX  src=0x%08lX\r\n",
                         (unsigned)r.backup_version,
                         (unsigned long)r.backup_payload_crc,
                         (unsigned long)r.backup_src_addr);
        usb_debug_printf("match : %s\r\n", r.match ? "yes (live == backup)"
                                                   : "NO (live differs from backup)");
    }
}

static void cmd_cal_backup(void)
{
    cal_bk_status_t st = cal_backup_store();
    usb_debug_printf("cal backup: %s\r\n", cal_bk_status_str(st));
    if (st == CAL_BK_ERR_LIVE_BLANK) {
        usb_send_str("  (MCU page is blank/zeroed — nothing to preserve, existing backup kept)\r\n");
    }
}

static void cmd_cal_restore(const char *args)
{
    char abuf[40];
    char *saveptr = NULL;
    bool force = false;
    const char *confirm = NULL;

    if (strlen(args) >= sizeof(abuf)) {
        usb_send_str("Usage: cal restore [force] CONFIRM\r\n"); return;
    }
    strcpy(abuf, args);
    char *t1 = strtok_r(abuf, " \t", &saveptr);
    char *t2 = strtok_r(NULL, " \t", &saveptr);
    if (t1 != NULL && strcmp(t1, "force") == 0) {
        force = true;
        confirm = t2;
    } else {
        confirm = t1;
    }
    if (confirm == NULL || strcmp(confirm, "CONFIRM") != 0) {
        usb_send_str("Refused: append CONFIRM. This WRITES MCU flash 0x08006000.\r\n"
                     "  cal restore CONFIRM        (refuses over a programmed page)\r\n"
                     "  cal restore force CONFIRM  (overwrites a programmed page)\r\n");
        return;
    }
    cal_bk_status_t st = cal_backup_restore(force);
    usb_debug_printf("cal restore%s: %s\r\n", force ? " force" : "", cal_bk_status_str(st));
    if (st == CAL_BK_ERR_LIVE_PRECIOUS) {
        usb_send_str("  live page holds data — use 'cal restore force CONFIRM' only if you are sure\r\n");
    }
}

/*
 * Scope trigger-comparator DAC (faithful reimpl of stock FUN_080018a4 CH1 path).
 *   trig raw <0-4095>     direct 12-bit DAC1 write + software trigger
 *   trig <range> <level>  full cal-formula path; range 0-9, level -100..+100
 * Output appears on PA4 (DAC1). Scope PA4 to verify: V = code/4095 * Vref(~3.3V).
 */
static void cmd_scope_trig(const char *args)
{
    char buf[48];
    if (strlen(args) >= sizeof(buf)) { usb_send_str("Usage: trig raw <code> | trig <range> <level>\r\n"); return; }
    strcpy(buf, args);

    char *saveptr = NULL;
    char *t1 = strtok_r(buf, " \t", &saveptr);
    char *t2 = strtok_r(NULL, " \t", &saveptr);

    scope_trigger_dac_init();

    if (t1 != NULL && strcmp(t1, "raw") == 0 && t2 != NULL) {
        uint32_t code = 0;
        if (parse_int(t2, &code) != 0) { usb_send_str("Usage: trig raw <0-4095>\r\n"); return; }
        if (code > 4095) code = 4095;
        scope_trigger_dac_raw((uint16_t)code);
    } else if (t1 != NULL && t2 != NULL) {
        uint32_t r = 0, lv = 0; int level;
        const char *ls = t2;
        int neg = 0;
        if (*ls == '-') { neg = 1; ls++; }
        if (parse_int(t1, &r) != 0 || parse_int(ls, &lv) != 0) {
            usb_send_str("Usage: trig <range 0-9> <level -100..100>\r\n"); return;
        }
        level = neg ? -(int)lv : (int)lv;
        scope_trigger_dac_set((int)r, level);
    } else {
        usb_send_str("Usage: trig raw <code> | trig <range> <level>\r\n");
        return;
    }

    uint16_t code = scope_trigger_dac_last();
    /* Vref assumed 3.3V; mV = code * 3300 / 4095. */
    uint32_t mv = ((uint32_t)code * 3300u) / 4095u;
    usb_debug_printf("DAC1(PA4) = code %u (0x%03X)  ~%lu.%03lu V expected\r\n",
                     code, code, (unsigned long)(mv / 1000), (unsigned long)(mv % 1000));
}

/*
 * CH2 vertical-offset reference: TMR13 CH1 PWM-DAC on PA6.
 *   trig2 raw <0-4095>     direct duty write to TMR13_C1DT
 *   trig2 <range> <level>  same cal formula as CH1 (ripcord contract 38)
 *
 * WHY THIS EXISTS (2026-08-17). CH1's DAC1 (PA4) turned out to be a vertical-
 * OFFSET injector, not a trigger threshold: `fpga scope center` centers CH1 by
 * binary-searching DAC1 until the capture mean lands on 128, and that was
 * bench-validated (648492a). The FPGA's actual trigger level is digital —
 * SPI3 reg 0x08, an ADC code. CH2 has no DAC channel; stock drives its offset
 * from a TMR13 CH1 PWM-DAC on PA6. Our firmware never programmed TMR13, which
 * is the leading explanation for EXP-06: every CH2 mux tap parked at a fixed DC
 * level and railed regardless of drive amplitude, so only one tap was usable.
 *
 * PA6's identity is DECODED, NOT CONFIRMED (HARDWARE_PINOUT.md marks it
 * "candidate, unconfirmed"). The point of this command is to settle that on the
 * bench, with the CH1 DAC1 sweep as the positive control in the same session:
 * if sweeping DAC1 moves CH1's mean but sweeping this does not move CH2's, the
 * pin is wrong and the failure is clean rather than ambiguous.
 *
 * scope_trigger_ch2_raw() self-inits, so this works on ANY build — no
 * FPGA_CH2_TRIGGER flag needed. That flag only arms TMR13 at boot.
 */
static void cmd_scope_trig2(const char *args)
{
    char buf[48];
    if (strlen(args) >= sizeof(buf)) { usb_send_str("Usage: trig2 raw <code> | trig2 <range> <level>\r\n"); return; }
    strcpy(buf, args);

    char *saveptr = NULL;
    char *t1 = strtok_r(buf, " \t", &saveptr);
    char *t2 = strtok_r(NULL, " \t", &saveptr);

    scope_trigger_ch2_init();

    if (t1 != NULL && strcmp(t1, "raw") == 0 && t2 != NULL) {
        uint32_t code = 0;
        if (parse_int(t2, &code) != 0) { usb_send_str("Usage: trig2 raw <0-4095>\r\n"); return; }
        if (code > 4095) code = 4095;
        scope_trigger_ch2_raw((uint16_t)code);
    } else if (t1 != NULL && t2 != NULL) {
        uint32_t r = 0, lv = 0; int level;
        const char *ls = t2;
        int neg = 0;
        if (*ls == '-') { neg = 1; ls++; }
        if (parse_int(t1, &r) != 0 || parse_int(ls, &lv) != 0) {
            usb_send_str("Usage: trig2 <range 0-9> <level -100..100>\r\n"); return;
        }
        level = neg ? -(int)lv : (int)lv;
        scope_trigger_ch2_set((int)r, level);
    } else {
        usb_send_str("Usage: trig2 raw <code> | trig2 <range> <level>\r\n");
        return;
    }

    uint16_t code = scope_trigger_ch2_last();
    /* Duty ratio against the stock ARR (4094). The RC-filtered mean at the pin
     * is duty * 3.3V, so the arithmetic matches `trig`'s DAC1 line — but this
     * is a PWM average, not a DAC output, so it is only valid once filtered. */
    uint32_t mv = ((uint32_t)code * 3300u) / 4095u;
    usb_debug_printf("TMR13_C1DT(PA6) = code %u (0x%03X)  duty ~%lu.%03lu V after RC\r\n",
                     code, code, (unsigned long)(mv / 1000), (unsigned long)(mv % 1000));
}

static void cmd_screen_dump(const char *args)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = LCD_WIDTH;
    uint32_t h = LCD_HEIGHT;

    if (args && *args) {
        if (strlen(args) >= sizeof(buf)) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }

        strcpy(buf, args);
        tok = strtok_r(buf, " \t", &saveptr);
        if (tok && strcmp(tok, "shadow") == 0) {
            tok = strtok_r(NULL, " \t", &saveptr);
            if (tok == NULL) goto parsed_screen_args;
        }
        if (tok == NULL || parse_int(tok, &x) != 0) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, &y) != 0) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, &w) != 0) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, &h) != 0) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }
        if (strtok_r(NULL, " \t", &saveptr) != NULL) {
            usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
            return;
        }
    }

parsed_screen_args:
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT ||
        w == 0 || h == 0 ||
        w > LCD_WIDTH || h > LCD_HEIGHT ||
        x > LCD_WIDTH - w || y > LCD_HEIGHT - h) {
        usb_send_str("Usage: screen dump [shadow] [x y w h]\r\n");
        usb_debug_printf("  bounds: x<%u y<%u x+w<=%u y+h<=%u\r\n",
                         (unsigned)LCD_WIDTH,
                         (unsigned)LCD_HEIGHT,
                         (unsigned)LCD_WIDTH,
                         (unsigned)LCD_HEIGHT);
        return;
    }

    usb_debug_printf("SCREENDUMP x=%lu y=%lu w=%lu h=%lu format=%s\r\n",
                     x, y, w, h, "indexed4");

    const uint8_t *bits = lcd_shadow_bits();
    static const char hexdigits[] = "0123456789ABCDEF";
    uint16_t page_y = lcd_shadow_page_y();
    for (uint32_t row = 0; row < h; row++) {
        usb_debug_printf("ROW %lu ", row);
        for (uint32_t col = 0; col < w; col++) {
            uint32_t sx = x + col;
            uint32_t sy = y + row;
            uint8_t idx = 0;
            if (sy >= page_y && sy < (uint32_t)page_y + LCD_SHADOW_HEIGHT) {
                uint32_t shadow_y = sy - page_y;
                uint8_t byte = bits[shadow_y * LCD_SHADOW_STRIDE + (sx >> 1)];
                idx = (sx & 1U) ? (byte & 0x0FU) : (byte >> 4);
            }
            char c = hexdigits[idx & 0x0F];
            usb_send_bytes((const uint8_t *)&c, 1);
        }
        usb_send_str("\r\n");
    }
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static bool parse_screen_region_args(const char *args,
                                     uint32_t *x,
                                     uint32_t *y,
                                     uint32_t *w,
                                     uint32_t *h)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok;

    *x = 0;
    *y = 0;
    *w = LCD_WIDTH;
    *h = LCD_HEIGHT;

    if (args && *args) {
        if (strlen(args) >= sizeof(buf)) return false;
        strcpy(buf, args);

        tok = strtok_r(buf, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, x) != 0) return false;
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, y) != 0) return false;
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, w) != 0) return false;
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok == NULL || parse_int(tok, h) != 0) return false;
        if (strtok_r(NULL, " \t", &saveptr) != NULL) return false;
    }

    return *x < LCD_WIDTH && *y < LCD_HEIGHT &&
           *w > 0 && *h > 0 &&
           *w <= LCD_WIDTH && *h <= LCD_HEIGHT &&
           *x <= LCD_WIDTH - *w && *y <= LCD_HEIGHT - *h;
}

static void cmd_screen_dumpbin(const char *args)
{
    uint32_t x, y, w, h;
    uint8_t out_row[LCD_SHADOW_STRIDE];

    if (!parse_screen_region_args(args, &x, &y, &w, &h)) {
        usb_send_str("Usage: screen dumpbin [x y w h]\r\n");
        usb_debug_printf("  bounds: x<%u y<%u x+w<=%u y+h<=%u\r\n",
                         (unsigned)LCD_WIDTH,
                         (unsigned)LCD_HEIGHT,
                         (unsigned)LCD_WIDTH,
                         (unsigned)LCD_HEIGHT);
        return;
    }

    uint32_t row_len = (w + 1U) / 2U;
    uint32_t len = row_len * h;
    uint32_t crc = 0;
    const uint8_t *bits = lcd_shadow_bits();

    for (uint32_t row = 0; row < h; row++) {
        uint32_t sy = y + row;
        memset(out_row, 0, row_len);
        for (uint32_t col = 0; col < w; col++) {
            uint32_t sx = x + col;
            uint8_t src = bits[sy * LCD_SHADOW_STRIDE + (sx >> 1)];
            uint8_t idx = (sx & 1U) ? (src & 0x0FU) : (src >> 4);
            if ((col & 1U) == 0) {
                out_row[col >> 1] = (uint8_t)(idx << 4);
            } else {
                out_row[col >> 1] |= idx;
            }
        }
        crc = crc32_update(crc, out_row, row_len);
    }

    usb_debug_printf("SCREENBIN x=%lu y=%lu w=%lu h=%lu format=indexed4 len=%lu crc32=%08lX\r\n",
                     x, y, w, h, len, crc);

    for (uint32_t row = 0; row < h; row++) {
        uint32_t sy = y + row;
        memset(out_row, 0, row_len);
        for (uint32_t col = 0; col < w; col++) {
            uint32_t sx = x + col;
            uint8_t src = bits[sy * LCD_SHADOW_STRIDE + (sx >> 1)];
            uint8_t idx = (sx & 1U) ? (src & 0x0FU) : (src >> 4);
            if ((col & 1U) == 0) {
                out_row[col >> 1] = (uint8_t)(idx << 4);
            } else {
                out_row[col >> 1] |= idx;
            }
        }
        usb_send_bytes(out_row, (uint16_t)row_len);
    }

    usb_send_str("\r\nSCREENBIN END\r\n");
}

static void cmd_screen_shadow(const char *args)
{
    if (args == NULL || *args == '\0') {
        usb_debug_printf("shadow_page_y=%u height=%u\r\n",
                         (unsigned)lcd_shadow_page_y(),
                         (unsigned)LCD_SHADOW_HEIGHT);
        return;
    }

    if (strncmp(args, "page", 4) == 0 &&
        (args[4] == '\0' || args[4] == ' ' || args[4] == '\t')) {
        const char *value = args + 4;
        uint32_t y = 0;
        while (*value == ' ' || *value == '\t') value++;
        if (*value != '\0' && parse_int(value, &y) != 0) {
            usb_send_str("Usage: screen shadow page [y]\r\n");
            return;
        }
        lcd_shadow_set_page((uint16_t)y);
        usb_debug_printf("shadow_page_y=%u height=%u\r\n",
                         (unsigned)lcd_shadow_page_y(),
                         (unsigned)LCD_SHADOW_HEIGHT);
        return;
    }

    usb_send_str("Usage: screen shadow page [y]\r\n");
}

static void cmd_fpga_cmd(const char *args)
{
    uint32_t cmd_hi = 0, cmd_lo = 0;
    const char *space = strchr(args, ' ');
    fpga_diag_snapshot_t before;

    char cmd_str[8];
    if (space) {
        int len = space - args;
        if (len >= (int)sizeof(cmd_str)) { usb_send_str("ERR\r\n"); return; }
        memcpy(cmd_str, args, len);
        cmd_str[len] = '\0';
        if (parse_int(space + 1, &cmd_lo) != 0 || cmd_lo > 0xFF) {
            usb_send_str("Usage: fpga cmd <hi> <lo>\r\n");
            return;
        }
    } else {
        strncpy(cmd_str, args, sizeof(cmd_str) - 1);
        cmd_str[sizeof(cmd_str) - 1] = '\0';
    }

    if (parse_int(cmd_str, &cmd_hi) != 0) {
        usb_send_str("Usage: fpga cmd <hi> <lo>\r\n");
        return;
    }

    if (space == NULL) {
        /* Single combined value form: fpga cmd 0x0509 */
        if (cmd_hi > 0xFFFF) {
            usb_send_str("Usage: fpga cmd <hi> <lo>\r\n");
            return;
        }
        cmd_lo = cmd_hi & 0xFF;
        cmd_hi = (cmd_hi >> 8) & 0xFF;
    } else if (cmd_hi > 0xFF) {
        usb_send_str("Usage: fpga cmd <hi> <lo>\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    if (!usart_send_cmd_reporting((uint8_t)cmd_hi, (uint8_t)cmd_lo)) return;

    /* Wait for response */
    vTaskDelay(pdMS_TO_TICKS(200));
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_acq(const char *args)
{
    BaseType_t ok;

    if (args && *args) {
        uint32_t mode = FPGA_ACQ_NORMAL + 1;  /* Explicit low-level trigger byte */
        parse_int(args, &mode);
        ok = fpga_trigger_acquisition((uint8_t)mode);
        usb_debug_printf("Acquisition trigger mode %lu: %s\r\n",
                         mode, ok == pdTRUE ? "queued" : "FULL");
    } else {
        ok = fpga_trigger_scope_read();
        usb_debug_printf("Scope acquisition trigger: %s (policy mode %u)\r\n",
                         ok == pdTRUE ? "queued" : "FULL",
                         fpga.acq_mode);
    }

    /* Wait for data */
    vTaskDelay(pdMS_TO_TICKS(500));

    if (fpga.spi3_ok_count > 0) {
        usb_debug_printf("SPI3 OK=%u  First bytes: CH1[%02X %02X %02X %02X] CH2[%02X %02X %02X %02X] varies=%d\r\n",
                         fpga.spi3_ok_count,
                         fpga.diag_ch1_raw[0], fpga.diag_ch1_raw[1],
                         fpga.diag_ch1_raw[2], fpga.diag_ch1_raw[3],
                         fpga.diag_ch2_raw[0], fpga.diag_ch2_raw[1],
                         fpga.diag_ch2_raw[2], fpga.diag_ch2_raw[3],
                         fpga.diag_data_varies);
    } else {
        usb_send_str("No SPI3 data received\r\n");
    }
}

/* `fpga usart [on|off]` — bring USART2 up AFTER config (stock's order) and
 * report the registers back. Coldtrace builds leave it dark; stock's channel
 * configuration command lives on this bus. Prints RX counters so "did anything
 * answer" is a number rather than an impression. */
static void cmd_fpga_usart(const char *args)
{
    char buf[160];
    while (*args == ' ') args++;
    if      (strncmp(args, "on",  2) == 0) fpga_usart_scope_enable(true);
    else if (strncmp(args, "off", 3) == 0) fpga_usart_scope_enable(false);
    else if (*args) { usb_send_str("usage: fpga usart [on|off]\r\n"); return; }

    uint32_t c1 = fpga_usart_ctrl1();
    snprintf(buf, sizeof(buf),
             "USART2 CTRL1=%08lX BAUDR=%08lX  UEN=%u TE=%u RE=%u RDBFIEN=%u\r\n"
             "  rx_bytes=%u echo_frames=%u\r\n",
             (unsigned long)c1, (unsigned long)fpga_usart_baudr(),
             (unsigned)((c1 >> 13) & 1), (unsigned)((c1 >> 3) & 1),
             (unsigned)((c1 >> 2) & 1),  (unsigned)((c1 >> 5) & 1),
             (unsigned)fpga.rx_byte_count, (unsigned)fpga.echo_count);
    usb_send_str(buf);
}

/* `fpga rearm [on|off]` — stock's post-read re-arm write (reg 0x01 <- rate idx).
 * Runtime-toggleable so the comparison can be run A/B/A inside one boot: the
 * same probe, the same signal, the same FPGA configuration, one variable. */
static void cmd_fpga_rearm(const char *args)
{
    char buf[96];
    while (*args == ' ') args++;
    if (*args) {
        if      (strncmp(args, "on",  2) == 0) fpga_acq_rearm_set(true);
        else if (strncmp(args, "off", 3) == 0) fpga_acq_rearm_set(false);
        else { usb_send_str("usage: fpga rearm [on|off]\r\n"); return; }
    }
    snprintf(buf, sizeof(buf), "acq re-arm %s (reg 01 <- 0x%02X after each 04/05 pair)\r\n",
             fpga_acq_rearm_get() ? "ON" : "OFF", fpga_acq_rate_idx_get());
    usb_send_str(buf);
}

/* `fpga rate [idx]` — the reg-0x01 value the re-arm rewrites. Setting it here
 * keeps the re-arm from silently reverting a timebase chosen elsewhere. */
static void cmd_fpga_rate(const char *args)
{
    char buf[80];
    while (*args == ' ') args++;
    if (*args) {
        unsigned v = (unsigned)strtoul(args, NULL, 16);
        if (v > 0xFF) { usb_send_str("usage: fpga rate <hex idx>\r\n"); return; }
        fpga_acq_rate_idx_set((uint8_t)v);
    }
    snprintf(buf, sizeof(buf), "acq rate idx = 0x%02X\r\n", fpga_acq_rate_idx_get());
    usb_send_str(buf);
}

static void cmd_fpga_scope_reinit(void)
{
    fpga_request_scope_reinit();
    usb_send_str("Scope reinit queued\r\n");
}

/* `fpga scope vdiv <1|2> <0-9>` — the vdiv BUTTON's path from the shell:
 * updates scope_state (label + counts->volts k) AND drives the relay bank
 * through fpga_apply_vdiv(), the single entry point. This is to `fpga scope
 * range` what `fpga scope timebase` is to a raw `seq 01 XX`: the raw form
 * changes the hardware behind the display's back and exists only for
 * deliberately testing that divergence (EXP-19). */
static void cmd_fpga_scope_vdiv(const char *args)
{
    uint32_t chn = 0, n = 0;
    const char *space = args ? strchr(args, ' ') : NULL;
    if (!args || parse_int(args, &chn) != 0 || (chn != 1u && chn != 2u) ||
        !space) {
        usb_send_str("Usage: fpga scope vdiv <1|2> <0-9>\r\n");
        return;
    }
    while (*space == ' ') space++;
    if (parse_int(space, &n) != 0 || n > 9u) {
        usb_send_str("Usage: fpga scope vdiv <1|2> <0-9>\r\n");
        return;
    }
    if (!fpga_apply_vdiv((uint8_t)chn, (uint8_t)n)) {
        usb_send_str("vdiv: relay apply failed\r\n");
        return;
    }
    scope_state_t *ss = scope_state_get();
    channel_state_t *c = (chn == 1u) ? &ss->ch1 : &ss->ch2;
    c->vdiv_idx = (uint8_t)n;
    char vd[12];
    scope_cal_range_label((uint8_t)chn, (uint8_t)n, vd, sizeof(vd));
    usb_debug_printf("vdiv CH%lu = range %lu (%s/div)\r\n",
                     (unsigned long)chn, (unsigned long)n, vd);
}

/* `fpga scope range <n> <1|2|both>` — apply coarse frontend range n (0-9) to
 * one channel or, with the explicit `both` keyword, to both. The two channels
 * have independent relay banks (CH1 PC12/PE4/PE5/PE6, CH2 PA15/PB11/PB10/PA10)
 * driven from the same 10-case stock table. Coupling is NOT touched here — it
 * is PD12/PD13, set in the frontend init (bench 2026-08-15).
 *
 * The channel argument is MANDATORY and 1-based on purpose. It used to be
 * optional, and anything that was not 1 or 2 — including a `0` typed by an
 * experimenter who reasonably read the argument as 0-based — fell through to
 * "both", drove both relay banks, and invalidated the run while printing a
 * success line. Ambiguity now errors instead of guessing. */
static void cmd_fpga_scope_range(const char *args)
{
    static const char USAGE[] =
        "Usage: fpga scope range <0-9> <1|2|both>\r\n"
        "  channel is MANDATORY and 1-BASED: 1 = CH1, 2 = CH2, both = CH1+CH2\r\n";
    uint32_t n = 0;
    uint32_t chn = 0;
    uint8_t ch_sel;
    const char *space;

    if (args == NULL || *args == '\0' || parse_int(args, &n) != 0 || n > 9) {
        usb_send_str(USAGE);
        return;
    }

    space = strchr(args, ' ');
    if (space == NULL) {
        usb_send_str("ERR: no channel given — refusing to assume.\r\n");
        usb_send_str(USAGE);
        return;
    }
    while (*space == ' ') space++;

    if (strcmp(space, "both") == 0) {
        ch_sel = 0xFF;
    } else if (parse_int(space, &chn) == 0 && (chn == 1 || chn == 2)) {
        ch_sel = (uint8_t)(chn - 1);
    } else {
        usb_debug_printf("ERR: bad channel '%s' — 1, 2 or both (there is no channel 0)\r\n",
                         space);
        usb_send_str(USAGE);
        return;
    }

    fpga_scope_set_range_diag_ch(ch_sel, (uint8_t)n);
    usb_send_str("(raw relay drive — display state and counts->volts k NOT "
                 "updated; use `fpga scope vdiv`)\r\n");
    usb_debug_printf("scope frontend range = %lu on %s\r\n",
                     (unsigned long)n,
                     ch_sel == 0 ? "CH1" : (ch_sel == 1 ? "CH2" : "CH1+CH2"));
}

/* Raw SPI3 byte exchange on the shared FPGA bus (defined later in this file). */
static uint8_t spi3_raw_xfer(uint8_t tx);

/* ---- fpga scope center [range] -------------------------------------------
 *
 * Per-range DC-offset CENTERING for scope gain calibration.
 *
 * Each frontend voltage range has its own DC operating point, so before per-
 * range GAIN can be calibrated the vertical-offset DAC (DAC1 / PA4) must be
 * positioned so a quiet/DC input lands at mid-scale (ADC code ~128). Doing
 * this by hand for every range is slow; this command automates it on-device.
 *
 * For each range it applies the frontend (DC coupling) via
 * fpga_scope_set_range_diag(), then binary-searches DAC1 (0..4095) for the
 * value that drives the mean of a 0x04 CH1 capture closest to 128. DAC1 is
 * monotonic-increasing vs ADC code (bench: 500->~0, 2500->~140, 3500->~255),
 * so the search raises DAC1 when the mean is below target and lowers it when
 * above. The operator must keep the input quiet/DC while this runs (no AC
 * signal) — it averages a static level.
 *
 * Diagnostic / bench only — it does NOT touch the normal acquisition path.
 * The continuous acquisition task is parked with fpga_acq_pause() for the
 * whole command so our CS-framed reads never interleave with its capture
 * frames (same hazard class as `spi3 opread` / `spi3 opsweep`).
 */
#define SCOPE_CENTER_TARGET   128u   /* ADC mid-scale code we center on    */
#define SCOPE_CENTER_NBYTES   256u   /* payload bytes averaged per read     */
#define SCOPE_CENTER_AVG      2u     /* reads averaged to knock down noise  */
#define SCOPE_CENTER_ITERS    11u    /* binary-search steps over 0..4095    */
#define SCOPE_CENTER_SETTLE_MS 480u  /* >= one ~430ms buffer fill after a DAC
                                        move, so the read is not stale         */

/* One capture read window (opcode 0x04 = CH1 buffer, 0x05 = CH2); returns the
 * mean of the first `nbytes` payload bytes. Same framing as spi3_opread_window
 * (one CS-LOW window, opcode + 2 filler bytes, then payload). The two filler
 * bytes are the header stock discards. Caller must already hold the acq pause. */
static uint32_t scope_center_read_mean(uint8_t opcode, uint32_t nbytes)
{
    uint32_t ssum = 0;
    GPIOB->clr = (1 << 6);                 /* CS assert (PB6 LOW)    */
    (void)spi3_raw_xfer(opcode);
    (void)spi3_raw_xfer(0xFF);
    (void)spi3_raw_xfer(0xFF);
    for (uint32_t i = 0; i < nbytes; i++)
        ssum += spi3_raw_xfer(0xFF);
    GPIOB->scr = (1 << 6);                 /* CS deassert (PB6 HIGH) */
    return nbytes ? ssum / nbytes : 0;
}

/* Average SCOPE_CENTER_AVG windows for a noise-robust mean. */
static uint32_t scope_center_robust_mean(uint8_t opcode)
{
    uint32_t acc = 0;
    for (uint32_t k = 0; k < SCOPE_CENTER_AVG; k++) {
        acc += scope_center_read_mean(opcode, SCOPE_CENTER_NBYTES);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return acc / SCOPE_CENTER_AVG;
}

/* Binary-search the channel's offset reference for a mean of ~128. Assumes the
 * range is already applied and the acq task paused. `ch` is 1 or 2: CH1 drives
 * DAC1 (PA4) and reads opcode 0x04; CH2 drives TMR13_C1DT (PA6, PWM-DAC) and
 * reads 0x05. Returns the best code found and its resulting mean.
 *
 * The monotonic assumption below was measured for DAC1 on CH1. For CH2 it is
 * ASSUMED, not verified — TMR13's channel polarity is active-low (stock sets
 * CCTRL C1P), so the duty->voltage sense could invert. If a CH2 search
 * converges on a rail, suspect the direction before suspecting the pin: sweep
 * `trig2 raw` by hand and watch which way the mean moves. */
static void scope_center_one(uint8_t ch, uint16_t *out_dac, uint32_t *out_mean)
{
    const uint8_t opcode = (ch == 2) ? 0x05u : 0x04u;

    uint16_t lo = 0, hi = 4095;
    uint16_t best_dac = 2048;
    uint32_t best_mean = 0;
    uint32_t best_err = 0xFFFFFFFFu;

    for (uint32_t it = 0; it < SCOPE_CENTER_ITERS; it++) {
        uint16_t mid = (uint16_t)((lo + hi) / 2);
        if (ch == 2) scope_trigger_ch2_raw(mid);
        else         scope_trigger_dac_raw(mid);
        /* BUGFIX 2026-08-14 (round 2): the DAC moves fine, but the capture
         * buffer FREE-RUNS and takes ~430 ms to refill (1024 samples at
         * ~2.4 kS/s). A 10 ms settle read the STALE buffer (old, roughly-
         * centered DC ~130) every iteration, so the search never converged —
         * the first "DAC init" fix was aimed at the wrong cause. Wait a full
         * buffer fill so the read reflects the NEW DC operating point. */
        vTaskDelay(pdMS_TO_TICKS(SCOPE_CENTER_SETTLE_MS));
        uint32_t mean = scope_center_robust_mean(opcode);
        uint32_t err = (mean > SCOPE_CENTER_TARGET)
                       ? mean - SCOPE_CENTER_TARGET
                       : SCOPE_CENTER_TARGET - mean;
        if (err < best_err) {
            best_err = err;
            best_dac = mid;
            best_mean = mean;
        }
        if (mean < SCOPE_CENTER_TARGET) lo = mid;   /* need higher DAC1 */
        else                            hi = mid;   /* need lower  DAC1 */
        if (hi - lo <= 1) break;
    }
    *out_dac = best_dac;
    *out_mean = best_mean;
}

/*
 * `fpga scope freq [n]` — run the SHIPPED frequency estimator on the live
 * acquisition buffer, n times, and report every outcome including refusals.
 *
 * This exists because reading a number off the LCD cannot distinguish "the
 * estimator is right" from "the estimator is wrong and the badge is holding a
 * stale value". It reports the diagnostics the badge throws away: sharpness,
 * which window was used, and the interpolated bin.
 */
/*
 * `settings` — why did the boot reconcile say what it said?
 *
 * Added 2026-08-19 after three consecutive power cycles reported "pulled the
 * display down to the arm block's 0x08", i.e. the restored timebase had no
 * measured rate — including one run where the change was committed through a
 * documented immediate-flush point (a mode switch). That is either a write
 * that never reached flash or a load that never came back, and NOTHING ON THE
 * DEVICE DISTINGUISHED THEM: settings_store_status_t has carried
 * storage_bound, load_result, writes, write_failures and changes_seen since
 * the store was written, and no caller ever read them.
 *
 * Same shape as the /2 SSPI reads, the floating MISO and the invented
 * volts/div labels: a conclusion drawn from one bit, while the instrument that
 * would have settled it already existed and was simply never wired up.
 */
static void cmd_settings(void)
{
    const settings_store_status_t *st = settings_store_get_status();
    const scope_state_t *ss = scope_state_get();

    if (st == NULL) {
        usb_send_str("settings: no status\r\n");
        return;
    }

    usb_debug_printf("storage bound : %s\r\n",
                     st->storage_bound ? "YES" : "NO - nothing can persist");
    usb_debug_printf("boot load     : %s\r\n",
                     config_load_result_name(st->load_result));
    usb_debug_printf("writes ok     : %lu\r\n", (unsigned long)st->writes);
    usb_debug_printf("write failures: %lu\r\n", (unsigned long)st->write_failures);
    usb_debug_printf("changes seen  : %lu\r\n", (unsigned long)st->changes_seen);
    usb_debug_printf("live timebase : 0x%02X\r\n", (unsigned)ss->timebase_idx);

    /* The store's "write failures" above conflates a refusal we chose with a
     * failure we suffered — config.h:162 warns about exactly that lie, and on
     * 2026-08-20 it cost a bench session: three power-cycle tests blamed the
     * write path when the build had writes disabled. These lines make the
     * distinction unmissable. */
    const config_persist_stats_t *cs = config_persist_stats();
    usb_debug_printf("writes enabled: %s\r\n",
                     cs->writes_enabled
                         ? "YES (SETTINGS_PERSIST_WRITES=1)"
                         : "NO - SETTINGS_PERSIST_WRITES=0 in this build "
                           "(default is 1 since the 2026-08-20 bench pass)");
    usb_debug_printf("saves ok/fail/disabled: %lu/%lu/%lu  last save status: %ld\r\n",
                     (unsigned long)cs->saves_ok,
                     (unsigned long)cs->saves_failed,
                     (unsigned long)cs->saves_disabled,
                     (long)cs->last_save_status);
    usb_send_str("(a change carries only if a later button press or an "
                 "orderly power-off follows it - see settings_store.h)\r\n");
}

/* `fpga scope timebase [code]` — change the timebase the way the UI button
 * does, i.e. through the single entry point that updates BOTH the display
 * state and the hardware register. Writing reg 0x01 with a raw `seq 01 XX`
 * changes the hardware behind the display's back and is how the divergence
 * above goes unnoticed; this command exists so bench scripts do not have to
 * reproduce that bug. */
static void cmd_fpga_scope_timebase(const char *args)
{
    while (*args == ' ') args++;
    scope_state_t *ss = scope_state_get();

    if (*args) {
        unsigned v = (unsigned)strtoul(args, NULL, 16);
        if (v >= SCOPE_TIMEBASE_CODE_COUNT) {
            usb_debug_printf("usage: fpga scope timebase <hex code 00-%02X>\r\n",
                             SCOPE_TIMEBASE_CODE_COUNT - 1);
            return;
        }
        ss->timebase_idx = (uint8_t)v;
        if (!fpga_apply_timebase((uint8_t)v)) {
            usb_send_str("acq task would not park — reg 0x01 NOT written\r\n");
            return;
        }
    }

    static const char *const recon[] = {
        "NEVER RAN — this build's arm path does not call it",
        "pushed the persisted code to the FPGA",
        "pulled the display down to the arm block's 0x08",
    };
    const uint8_t ra = fpga_timebase_reconcile_action();
    usb_debug_printf("boot reconcile: %s\r\n", recon[ra < 3u ? ra : 0u]);

    char lbl[12];
    scope_timebase_label(ss->timebase_idx, lbl, sizeof(lbl));
    usb_debug_printf("timebase 0x%02X (reg 0x01 = 0x%02X)  %lu S/s  %s/div\r\n",
                     (unsigned)ss->timebase_idx,
                     (unsigned)fpga_acq_rate_idx_get(),
                     (unsigned long)(scope_timebase_sample_rate(ss->timebase_idx) + 0.5f),
                     lbl);
}

/* `fpga scope graticule [auto|true|toggle]` — the M3 seam. Choose whether the
 * live trace is drawn at TRUE volts/div (one grid division == the volts/div the
 * status bar prints, on calibrated ranges) or AUTOFIT (scaled to fill the band,
 * so the grid is a position reference only). Autofit at boot; this flips it on
 * the bench without a rebuild. True scale needs a centred baseline, so it also
 * prints which ranges will honour it and the reminder to center first. */
static void cmd_fpga_scope_graticule(const char *args)
{
    while (*args == ' ') args++;
    scope_state_t *ss = scope_state_get();

    if (strncmp(args, "true", 4) == 0)        ss->true_scale = true;
    else if (strncmp(args, "auto", 4) == 0)   ss->true_scale = false;
    else if (strncmp(args, "toggle", 6) == 0) ss->true_scale = !ss->true_scale;
    else if (*args) {
        usb_send_str("usage: fpga scope graticule [auto|true|toggle]\r\n");
        return;
    }

    usb_debug_printf("graticule: %s\r\n",
                     ss->true_scale ? "TRUE SCALE (grid == volts/div)"
                                    : "autofit (grid == position only)");
    if (ss->true_scale) {
        char l1[12], l2[12];
        scope_cal_range_label(1u, ss->ch1.vdiv_idx, l1, sizeof(l1));
        scope_cal_range_label(2u, ss->ch2.vdiv_idx, l2, sizeof(l2));
        usb_debug_printf("  CH1 rng %u -> %s/div %s\r\n",
                         (unsigned)ss->ch1.vdiv_idx, l1,
                         scope_cal_true_scale_ok(1u, ss->ch1.vdiv_idx)
                             ? "" : "(no cal -> autofit)");
        usb_debug_printf("  CH2 rng %u -> %s/div %s\r\n",
                         (unsigned)ss->ch2.vdiv_idx, l2,
                         scope_cal_true_scale_ok(2u, ss->ch2.vdiv_idx)
                             ? "" : "(no cal -> autofit)");
        usb_send_str("  center the baseline first: `fpga scope center`\r\n");
    }
}

/* `fpga scope softtrig [on|off|toggle]` — the software display trigger. When on
 * (default), each frame's render window starts on the trigger source's level
 * crossing so a periodic trace stands still; off free-runs from sample 0 (the
 * old "dancing"). Handy for A/B on the bench: off = dances, on = locked. */
static void cmd_fpga_scope_softtrig(const char *args)
{
    while (*args == ' ') args++;
    scope_state_t *ss = scope_state_get();

    if (strncmp(args, "on", 2) == 0)          ss->soft_trigger = true;
    else if (strncmp(args, "off", 3) == 0)    ss->soft_trigger = false;
    else if (strncmp(args, "toggle", 6) == 0) ss->soft_trigger = !ss->soft_trigger;
    else if (*args) {
        usb_send_str("usage: fpga scope softtrig [on|off|toggle]\r\n");
        return;
    }

    usb_debug_printf("softtrig: %s   (level=%d px, %s, src=CH%d)\r\n",
                     ss->soft_trigger ? "ON (trace locked to trigger)"
                                      : "off (free-run / dancing)",
                     (int)ss->trigger.level,
                     ss->trigger.edge == TRIG_RISING ? "rising" : "falling",
                     ss->trigger.source == TRIG_SRC_CH2 ? 2 : 1);
}

static void cmd_fpga_scope_freq(const char *args)
{
    uint32_t reps = 10;
    if (args && *args) parse_int(args, &reps);
    if (reps == 0u) reps = 1u;
    if (reps > 100u) reps = 100u;

    const scope_state_t *ss = scope_state_get();
    const float fs = scope_timebase_sample_rate(ss->timebase_idx);
    const uint8_t in_force = fpga_acq_rate_idx_get();

    usb_debug_printf("timebase 0x%02X -> %lu S/s  (reg 0x01 in force: 0x%02X)\r\n",
                     (unsigned)ss->timebase_idx, (unsigned long)(fs + 0.5f),
                     (unsigned)in_force);

    /* The whole point of printing both: on 2026-08-19 these were found to
     * disagree on a stock boot (display 0x0A, hardware 0x08), which would
     * have made every derived Hz wrong by the ratio of the two rates. A
     * frequency computed from the wrong rate is not worth printing. */
    if (in_force != ss->timebase_idx) {
        usb_send_str("MISMATCH: the display's timebase is not the one the "
                     "hardware is sampling at — refusing to derive a "
                     "frequency. Use `fpga scope timebase <code>`.\r\n");
        return;
    }

    if (fs <= 0.0f) {
        usb_send_str("no trustworthy rate for this code — no frequency is "
                     "derivable (see scope_timebase.h)\r\n");
        return;
    }

    const volatile uint8_t *ch1 = fpga_get_ch1_buf();
    if (!ch1) { usb_send_str("FPGA not initialized\r\n"); return; }

    uint32_t answered = 0;
    for (uint32_t i = 0; i < reps; i++) {
        scope_freq_t r;
        const bool ok = scope_freq_estimate((const uint8_t *)ch1,
                                            FPGA_ADC_BUF_SIZE, fs, &r);
        const uint32_t mhz = (uint32_t)(r.hz * 1000.0f + 0.5f);
        usb_debug_printf("  %2lu  %-8s  %6lu.%03lu Hz  bin %4lu.%02lu  "
                         "sharp %lu.%02lu  win %u\r\n",
                         (unsigned long)i,
                         ok ? "ANSWER" : "refuse",
                         (unsigned long)(mhz / 1000u), (unsigned long)(mhz % 1000u),
                         (unsigned long)r.bin,
                         (unsigned long)((r.bin - (float)(uint32_t)r.bin) * 100.0f),
                         (unsigned long)r.sharpness,
                         (unsigned long)((r.sharpness -
                                          (float)(uint32_t)r.sharpness) * 100.0f),
                         r.window);
        if (ok) answered++;
        usb_delay_ms(60);
    }
    usb_debug_printf("answered %lu/%lu\r\n",
                     (unsigned long)answered, (unsigned long)reps);
}

/* `fpga scope measure [reps]` — print what the measurement badges compute,
 * raw and machine-parseable: one line per fresh record. This is the bench
 * instrument for EXP-19 badge validation, and it deliberately calls the SAME
 * sources the badges call (scope_measure_record, scope_cal_volts_per_count,
 * scope_timebase_sample_rate, scope_freq_estimate) rather than reimplementing
 * any of them — two implementations of one quantity is the two-renderers bug.
 *
 * Integer-only output (shell printf carries no %f): voltages in MICROVOLTS,
 * duty in permille, period in samples x100, frequency in mHz. A field the
 * instrument refuses (k = 0, no rate, estimator declined) prints "-" so a
 * refusal can never be confused with a measured zero. */
static void cmd_fpga_scope_measure(const char *args)
{
    uint32_t reps = 5;
    if (args && *args) parse_int(args, &reps);
    if (reps == 0u) reps = 1u;
    if (reps > 50u) reps = 50u;

    const scope_state_t *ss = scope_state_get();
    const float fs = scope_timebase_sample_rate(ss->timebase_idx);
    const uint8_t in_force = fpga_acq_rate_idx_get();
    const bool tb_ok = (in_force == ss->timebase_idx) && (fs > 0.0f);
    const float k1 = scope_cal_volts_per_count(1u, ss->ch1.vdiv_idx);
    const float k2 = scope_cal_volts_per_count(2u, ss->ch2.vdiv_idx);

    usb_debug_printf("badge sources: rng1=%u k1_uV=%lu  rng2=%u k2_uV=%lu  "
                     "tb=0x%02X inforce=0x%02X fs=%lu\r\n",
                     (unsigned)ss->ch1.vdiv_idx,
                     (unsigned long)(k1 * 1e6f + 0.5f),
                     (unsigned)ss->ch2.vdiv_idx,
                     (unsigned long)(k2 * 1e6f + 0.5f),
                     (unsigned)ss->timebase_idx, (unsigned)in_force,
                     (unsigned long)(fs + 0.5f));
    if (in_force != ss->timebase_idx)
        usb_send_str("TB MISMATCH: frequency suppressed (see fpga scope freq)\r\n");

    const volatile uint8_t *ch1 = fpga_get_ch1_buf();
    const volatile uint8_t *ch2 = fpga_get_ch2_buf();
    if (!ch1) { usb_send_str("FPGA not initialized\r\n"); return; }

    for (uint32_t i = 0; i < reps; i++) {
        scope_measure_t m1, m2;
        scope_measure_record((const uint8_t *)ch1, FPGA_ADC_BUF_SIZE, &m1);
        scope_measure_record(ch2 ? (const uint8_t *)ch2 : NULL,
                             ch2 ? FPGA_ADC_BUF_SIZE : 0u, &m2);

        char vpp1[12] = "-", vrms1[12] = "-", per[12] = "-", f[16] = "-";
        char vpp2[12] = "-";
        if (k1 > 0.0f) {
            snprintf(vpp1, sizeof vpp1, "%lu",
                     (unsigned long)((float)m1.pp_robust * k1 * 1e6f + 0.5f));
            snprintf(vrms1, sizeof vrms1, "%lu",
                     (unsigned long)(m1.ac_rms * k1 * 1e6f + 0.5f));
        }
        if (m1.period_valid)
            snprintf(per, sizeof per, "%lu",
                     (unsigned long)(m1.period_samples * 100.0f + 0.5f));
        if (tb_ok) {
            scope_freq_t fr;
            if (scope_freq_estimate((const uint8_t *)ch1, FPGA_ADC_BUF_SIZE,
                                    fs, &fr))
                snprintf(f, sizeof f, "%lu",
                         (unsigned long)(fr.hz * 1000.0f + 0.5f));
        }
        if (m2.valid && k2 > 0.0f)
            snprintf(vpp2, sizeof vpp2, "%lu",
                     (unsigned long)((float)m2.pp_robust * k2 * 1e6f + 0.5f));

        /* Edge timing in SAMPLES x100 (M4). "-" when no clean edge was found,
         * same convention as per1. Kept in samples, not seconds: multiply by
         * 1/fs on the host when a rate is in force. */
        char rise[12] = "-", fall[12] = "-";
        if (m1.rise_valid)
            snprintf(rise, sizeof rise, "%lu",
                     (unsigned long)(m1.rise_samples * 100.0f + 0.5f));
        if (m1.fall_valid)
            snprintf(fall, sizeof fall, "%lu",
                     (unsigned long)(m1.fall_samples * 100.0f + 0.5f));

        usb_debug_printf("M %2lu pp1=%u ppr1=%u Vpp1_uV=%s Vrms1_uV=%s duty1_pm=%lu "
                         "per1_smp100=%s f1_mHz=%s rise1_smp100=%s fall1_smp100=%s "
                         "pp2=%u Vpp2_uV=%s\r\n",
                         (unsigned long)i, (unsigned)m1.pp,
                         (unsigned)m1.pp_robust, vpp1, vrms1,
                         (unsigned long)(m1.level_valid
                                         ? m1.duty_pct * 10.0f + 0.5f : 0.0f),
                         per, f, rise, fall, (unsigned)m2.pp, vpp2);
        usb_delay_ms(60);
    }
}

/* `fpga scope cal` — print what the firmware believes about vertical scale.
 *
 * This exists so the number on the screen can be checked against the number
 * in the source without a rebuild, and so a bench session can see at a glance
 * which ranges are cross-validated and which are only provisional. It reads
 * the table; it cannot change it. Recalibration is a source edit plus a
 * rebuild, deliberately, because a runtime-adjustable gain that nobody
 * records is how an instrument ends up lying with confidence.
 */
static void cmd_fpga_scope_cal(void)
{
    usb_send_str("vertical cal (bench unit #1, EXP-08 2026-08-17)\r\n");
    usb_debug_printf("source scale %d.%03d  counts/div %d\r\n",
                     (int)SCOPE_CAL_SOURCE_SCALE,
                     (int)((SCOPE_CAL_SOURCE_SCALE - (int)SCOPE_CAL_SOURCE_SCALE)
                           * 1000.0f + 0.5f),
                     (int)SCOPE_CAL_COUNTS_PER_DIV);
    usb_send_str("rng  CH1 uV/ct  CH1 V/div  CH2 uV/ct  CH2 V/div  tier\r\n");

    for (uint8_t r = 0; r < SCOPE_CAL_RANGE_COUNT; r++) {
        char l1[12], l2[12];
        scope_cal_range_label(1u, r, l1, sizeof(l1));
        scope_cal_range_label(2u, r, l2, sizeof(l2));

        const scope_cal_tier_t t = scope_cal_get_tier(1u, r);
        const char *tn = (t == SCOPE_CAL_MEASURED)    ? "measured"
                       : (t == SCOPE_CAL_PROVISIONAL) ? "provisional"
                                                      : "none (rails)";

        /* Microvolts per count keeps this integer-only — the shell printf is
         * not guaranteed to carry %f on every build. */
        usb_debug_printf("%2u  %9lu  %9s  %9lu  %9s  %s\r\n",
                         (unsigned)r,
                         (unsigned long)(scope_cal_mv_per_count(1u, r) * 1000.0f + 0.5f),
                         l1,
                         (unsigned long)(scope_cal_mv_per_count(2u, r) * 1000.0f + 0.5f),
                         l2,
                         tn);
    }

    usb_send_str("\r\ntimebase (reg 0x01) -> sample rate\r\n");
    usb_send_str("code    S/s    s/div   tier\r\n");
    for (uint8_t c = 0; c < SCOPE_TIMEBASE_CODE_COUNT; c++) {
        const float fs = scope_timebase_sample_rate(c);
        if (fs <= 0.0f)
            continue;                      /* skip the many unmeasured codes */
        char lbl[12];
        scope_timebase_label(c, lbl, sizeof(lbl));
        const scope_tb_tier_t t = scope_timebase_get_tier(c);
        usb_debug_printf("0x%02X  %7lu  %7s  %s\r\n",
                         (unsigned)c, (unsigned long)(fs + 0.5f), lbl,
                         (t == SCOPE_TB_MEASURED) ? "measured" : "provisional");
    }
    usb_send_str("codes not listed have no trustworthy rate, for three "
                 "DIFFERENT reasons:\r\n"
                 "  0x06-0x09  INCOHERENT — measured, and all four return the "
                 "same ~1.2-1.6 kS/s\r\n"
                 "             regardless of the code, with reads that do not "
                 "reproduce (EXP-15).\r\n"
                 "  0x0A-0x0C  need a faster source than ours — a statement "
                 "about our bench,\r\n"
                 "             not about the device.\r\n"
                 "  the rest   never measured.\r\n"
                 "See scope_timebase.h\r\n");
}

/* `fpga scope center [ch1|ch2] [0-9]` — auto-center the channel's vertical-
 * offset reference for one range (arg given) or for every range 0..9 (no arg),
 * one report line per range. Channel defaults to CH1, preserving the original
 * bench-validated `fpga scope center [0-9]` usage byte-for-byte.
 *
 * The channel is a NAMED TOKEN, not a numeric suffix, deliberately. On
 * 2026-08-17 `fpga scope range <n> 0` silently addressed BOTH channels because
 * its channel argument was 1-based and 0 fell through to "both", which invalidated
 * a whole relay test. A `center2` spelling would have been worse still: the
 * dispatcher matches "fpga scope center" on 17 chars, so `center2` would have
 * matched the CH1 handler with empty args and quietly swept all ten CH1 ranges. */
static void cmd_fpga_scope_center(const char *args)
{
    char buf[32];
    uint8_t ch = 1;

    if (args == NULL) args = "";
    if (strlen(args) >= sizeof(buf)) {
        usb_send_str("Usage: fpga scope center [ch1|ch2] [0-9]\r\n");
        return;
    }
    strcpy(buf, args);

    char *saveptr = NULL;
    char *t1 = strtok_r(buf, " \t", &saveptr);
    if (t1 != NULL && (strcmp(t1, "ch1") == 0 || strcmp(t1, "ch2") == 0)) {
        ch = (t1[2] == '2') ? 2u : 1u;
        t1 = strtok_r(NULL, " \t", &saveptr);
    }

    bool all = (t1 == NULL || *t1 == '\0');
    uint32_t n = 0;

    if (!all && (parse_int(t1, &n) != 0 || n > 9)) {
        usb_send_str("Usage: fpga scope center [ch1|ch2] [0-9]\r\n");
        return;
    }

    if (!fpga_acq_pause()) {
        usb_send_str("ERR: acq task did not park — bus not safe, aborting\r\n");
        return;
    }

    /* BUGFIX 2026-08-14: the search's scope_trigger_dac_raw() writes were inert
     * without this init (bench: reported means clustered 120-170 regardless of
     * DAC1, never spanning 0-255, so the binary search flailed around a fixed
     * operating point). `trig raw` calls it too — the DAC peripheral must be
     * enabled before raw writes take effect. */
    if (ch == 2) scope_trigger_ch2_init();
    else         scope_trigger_dac_init();

    uint32_t first = all ? 0 : n;
    uint32_t last  = all ? 9 : n;
    for (uint32_t r = first; r <= last; r++) {
        uint16_t dac;
        uint32_t mean;
        /* Deliberately applies the range to BOTH banks even when centering CH2:
         * that is what the bench-validated CH1 path did (648492a), and the two
         * frontends are independent, so restricting it would be an untested
         * change to a working measurement for no measurable gain. */
        fpga_scope_set_range_diag((uint8_t)r);
        vTaskDelay(pdMS_TO_TICKS(20));     /* relay/frontend settle */
        scope_center_one(ch, &dac, &mean);
        /* Name the reference in the output: a bare "center=" line would read
         * identically whichever channel ran, and a mislabelled log is how this
         * project has repeatedly convinced itself of the wrong pin. */
        usb_debug_printf("CH%u range %lu: center %s=%u (mean=%lu)\r\n",
                         (unsigned)ch, (unsigned long)r,
                         (ch == 2) ? "TMR13_C1DT" : "DAC1",
                         dac, (unsigned long)mean);
    }

    fpga_acq_resume();
}

static void cmd_fpga_diag_clear(void)
{
    fpga_diag_clear();
    usb_send_str("FPGA diagnostics cleared\r\n");
}

/* EXPERIMENTAL (experimental/esp32-bringup) — UNTESTED. Hand the SPI3 bus to
 * an external SSPI master (ESP32) on the back-side test pads. Re-flash to undo. */
static void cmd_fpga_bus_release(void)
{
    fpga_bus_release();
    usb_send_str("SPI3 bus RELEASED to external master.\r\n");
    usb_send_str("  PB3(SCK)/PB5(MOSI)/PB6(CS) -> Hi-Z, MCU off the bus.\r\n");
    usb_send_str("  PB4(MISO) input (FPGA-driven, shared read).\r\n");
    usb_send_str("  PC6=HIGH (SPI en), PB11=HIGH (active), PC9 power-hold kept.\r\n");
    usb_send_str("  ESP32 may now drive SSPI. Re-flash/power-cycle to reclaim.\r\n");
}

static void cmd_fpga_bus_reacquire(void)
{
    fpga_spi3_bus_reacquire();
    usb_send_str("SPI3 bus REACQUIRED by MCU.\r\n");
    usb_send_str("  PB3(SCK)/PB5(MOSI) -> SPI3 AF, PB6(CS) -> GPIO out HIGH.\r\n");
    usb_send_str("  PB4(MISO) input pull-up (stock idle), PC6=HIGH, SPE=1.\r\n");
    usb_send_str("  Config port left untouched. `fpga reinit` now drives it.\r\n");
}

/* fpga configbb — run the GPIO bit-bang SSPI config on demand (H7 step 1).
 * Fires the SAME bit-bang loader coldtrace uses at boot, but from the shell on
 * guest-bringup-bb, so bit-bang (succeeds) and hardware-SPI (`fpga reinit`, fails)
 * run from ONE build with identical off-SPI pin state. If bit-bang hits DONE_FINAL
 * here, the wall is in the SPI3 clocking character (LA-diffable), not off-bus.
 * Needs a cold/open port (power-cycle first); a successful config CLOSES the port,
 * so re-run only after another power cycle. */
static void cmd_fpga_configbb(void)
{
#if FPGA_CONFIG_B
    usb_send_str("bit-bang SSPI config (05-less V0.4 framing) — firing...\r\n");
    (void)fpga_bitbang_config_sequence();
    uint32_t sr = ((uint32_t)fpga.cfg_status_reg[0] << 24) |
                  ((uint32_t)fpga.cfg_status_reg[1] << 16) |
                  ((uint32_t)fpga.cfg_status_reg[2] << 8) |
                  (uint32_t)fpga.cfg_status_reg[3];
    usb_debug_printf("post-upload STATUS(0x41): %02X %02X %02X %02X (raw=%08lX)\r\n",
                     fpga.cfg_status_reg[0], fpga.cfg_status_reg[1],
                     fpga.cfg_status_reg[2], fpga.cfg_status_reg[3], sr);
    usb_debug_printf("  DONE_FINAL(bit13)=%s  flags:%s%s%s%s\r\n",
                     (sr & (1u << 13)) ? "YES -- CONFIG TOOK" : "no -- the wall",
                     (sr & (1u << 0))  ? " CRC_ERR" : "",
                     (sr & (1u << 2))  ? " ID_FAIL" : "",
                     (sr & (1u << 12)) ? " GWVLD"   : "",
                     (sr & (1u << 15)) ? " READY"   : "");
    usb_debug_printf("  close IDCODE(0x11)=%s (silent=port CLOSED=configured)\r\n",
                     fpga.probe_id_bit_close == 0 ? "0x0120681B ANSWERS (still open)"
                                                  : "silent/unanchored");
#else
    usb_send_str("configbb: not compiled — rebuild with FPGA_CONFIG_B "
                 "(make guest-bringup-bb)\r\n");
#endif
}

static void cmd_fpga_stock_diag(void)
{
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_clear(void)
{
    fpga_stock_diag_reset();
    usb_send_str("Stock shadow reset to base scope posture\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_set(const char *args)
{
    uint8_t p[9];

    if (parse_byte_args(args, p, 9) != 0) {
        usb_send_str("Usage: fpga stock set <F68> <F69> <F6A> <F6B> <E1A> <E1B> <E1C> <E1D> <355>\r\n");
        return;
    }

    fpga_stock_diag_set(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
    usb_send_str("Stock shadow updated\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_preset(const char *args)
{
    char buf[96];
    char *saveptr = NULL;
    char *tok;
    uint32_t value;
    uint8_t packed[5] = {0};
    size_t count = 0;

    if (args == NULL || *args == '\0' || strlen(args) >= sizeof(buf)) {
        usb_send_str("Usage: fpga stock preset <F68> <F69> <F6A> <F6B> [355]\r\n");
        return;
    }

    strcpy(buf, args);
    tok = strtok_r(buf, " \t", &saveptr);
    while (tok != NULL && count < 5) {
        if (parse_int(tok, &value) != 0 || value > 0xFF) {
            usb_send_str("Usage: fpga stock preset <F68> <F69> <F6A> <F6B> [355]\r\n");
            return;
        }
        packed[count++] = (uint8_t)value;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (tok != NULL || count < 4) {
        usb_send_str("Usage: fpga stock preset <F68> <F69> <F6A> <F6B> [355]\r\n");
        return;
    }

    fpga_stock_diag_seed_preset(packed[0], packed[1], packed[2], packed[3],
                                (count >= 5) ? packed[4] : fpga.stock_shadow.latch_355);
    usb_send_str("Stock packed preset updated\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_base2(void)
{
    fpga_stock_diag_seed_base2();
    usb_send_str("Stock shadow seeded to visible state 2\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_state5(const char *args)
{
    uint8_t e1b = 3;
    uint8_t e1d = 0;

    if (parse_optional_byte_pair(args, &e1b, &e1d) != 0) {
        usb_send_str("Usage: fpga stock state5 [E1B] [E1D]\r\n");
        return;
    }

    fpga_stock_diag_seed_state5(e1b, e1d);
    usb_send_str("Stock shadow seeded to visible state 5\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_state6(const char *args)
{
    uint8_t e1b = 3;
    uint8_t e1d = 0;

    if (parse_optional_byte_pair(args, &e1b, &e1d) != 0) {
        usb_send_str("Usage: fpga stock state6 [E1B] [E1D]\r\n");
        return;
    }

    if (e1b == 0) e1b = 1;
    fpga_stock_diag_seed_state6(e1b, e1d);
    usb_send_str("Stock shadow seeded to visible state 6\r\n");
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_reenter(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_reenter();
    usb_send_str("Stock-shadow reentry complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_prev(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_prev();
    usb_send_str("Stock adjust-prev complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_next(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_next();
    usb_send_str("Stock adjust-next complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_select(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_select();
    usb_send_str("Stock staged-select complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_toggle(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_toggle();
    usb_send_str("Stock staged-toggle complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_commit(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_commit();
    usb_send_str("Stock commit/bridge step complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_consume(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_consume();
    usb_send_str("Stock preset-consume step complete\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_bridge_fixed(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_bridge_fixed();
    usb_send_str("Stock bridge fixed-candidate path sent\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_stock_bridge_dynamic(const char *args)
{
    uint8_t bank_mode;
    fpga_diag_snapshot_t before;

    if (parse_wire_bank_mode(args, &bank_mode) != 0) {
        usb_send_str("Usage: fpga stock bridge dynamic [ch1|ch2|both]\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    fpga_stock_diag_bridge_dynamic(bank_mode);
    usb_send_str("Stock bridge dynamic-candidate path sent\r\n");
    fpga_diag_print_delta(&before);
    fpga_stock_diag_print();
}

static void cmd_fpga_wire_words(const char *args)
{
    char buf[192];
    char *saveptr = NULL;
    char *tok;
    uint32_t value;
    size_t count = 0;
    fpga_diag_snapshot_t before;

    if (args == NULL || *args == '\0' || strlen(args) >= sizeof(buf)) {
        usb_send_str("Usage: fpga wire words <word1> [word2 ...]\r\n");
        return;
    }

    strcpy(buf, args);
    fpga_diag_snapshot_take(&before);

    tok = strtok_r(buf, " \t", &saveptr);
    while (tok != NULL) {
        if (parse_int(tok, &value) != 0 || value > 0xFFFF) {
            usb_send_str("Usage: fpga wire words <word1> [word2 ...]\r\n");
            return;
        }
        fpga_wire_send_word((uint16_t)value, 15);
        count++;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    usb_debug_printf("Wire words sent: %u\r\n", (unsigned)count);
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_wire_entry(const char *args)
{
    uint8_t bank_mode;
    fpga_diag_snapshot_t before;

    if (parse_wire_bank_mode(args, &bank_mode) != 0) {
        usb_send_str("Usage: fpga wire entry [ch1|ch2|both]\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    fpga_wire_entry(bank_mode);
    usb_send_str("Wire-word entry sequence sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_wire_scope(const char *args)
{
    uint8_t bank_mode;
    fpga_diag_snapshot_t before;

    if (parse_wire_bank_mode(args, &bank_mode) != 0) {
        usb_send_str("Usage: fpga wire scope [ch1|ch2|both]\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    fpga_wire_scope_sequence(bank_mode);
    usb_send_str("Wire-word scope sequence sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_meter_reinit(const char *args)
{
    extern volatile uint8_t meter_submode;
    uint32_t submode = meter_submode;

    if (args && *args) {
        if (parse_int(args, &submode) != 0 || submode >= METER_SUBMODE_COUNT) {
            usb_debug_printf("Usage: fpga meter reinit [0-%u]\r\n",
                             (unsigned)(METER_SUBMODE_COUNT - 1));
            return;
        }
    }

    meter_submode = (uint8_t)submode;
    fpga_meter_reinit((uint8_t)submode);
    usb_debug_printf("Meter reinit complete: submode %lu (%s)\r\n",
                     submode, meter_submode_name((uint8_t)submode));
}

static void cmd_fpga_scope_wake(void)
{
    fpga_scope_wake();
    usb_send_str("Scope wake complete\r\n");
}

static void cmd_fpga_scope_acqmode(void)
{
    fpga_diag_snapshot_t before;

    fpga_diag_snapshot_take(&before);
    fpga_scope_refresh_acq_mode();
    usb_send_str("Scope acquisition mode block sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_scope_beat(const char *args)
{
    uint32_t count = 1;
    uint32_t delay_ms = 0;
    const char *space;
    fpga_diag_snapshot_t before;

    if (args && *args) {
        if (parse_int(args, &count) != 0 || count == 0) {
            usb_send_str("Usage: fpga scope beat [count] [delay_ms]\r\n");
            return;
        }

        space = strchr(args, ' ');
        if (space) {
            while (*space == ' ') space++;
            if (*space && (parse_int(space, &delay_ms) != 0)) {
                usb_send_str("Usage: fpga scope beat [count] [delay_ms]\r\n");
                return;
            }
        }
    }

    fpga_diag_snapshot_take(&before);
    for (uint32_t i = 0; i < count; i++) {
        fpga_scope_heartbeat();
        if (delay_ms > 0 && (i + 1U) < count) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    usb_debug_printf("Scope heartbeat x%lu complete\r\n", count);
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_scope_entry(const char *args)
{
    uint8_t p[8];
    fpga_diag_snapshot_t before;

    if (parse_byte_args(args, p, 8) != 0) {
        usb_send_str("Usage: fpga scope entry <01> <0B> <0C> <0D> <0E> <0F> <10> <11>\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    if (!fpga_send_cmd_timed(0x00, FPGA_CMD_RESET, 20)) return;
    if (!fpga_send_cmd_timed(p[0], FPGA_CMD_SCOPE_CH, 15)) return;
    if (!fpga_send_cmd_timed(p[1], FPGA_CMD_SCOPE_CFG_0B, 15)) return;
    if (!fpga_send_cmd_timed(p[2], FPGA_CMD_SCOPE_CFG_0C, 15)) return;
    if (!fpga_send_cmd_timed(p[3], FPGA_CMD_SCOPE_CFG_0D, 15)) return;
    if (!fpga_send_cmd_timed(p[4], FPGA_CMD_SCOPE_CFG_0E, 15)) return;
    if (!fpga_send_cmd_timed(p[5], FPGA_CMD_SCOPE_CFG_0F, 15)) return;
    if (!fpga_send_cmd_timed(p[6], FPGA_CMD_SCOPE_CFG_10, 15)) return;
    if (!fpga_send_cmd_timed(p[7], FPGA_CMD_SCOPE_CFG_11, 20)) return;

    usb_send_str("Scope entry block sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_scope_timing(const char *args)
{
    uint8_t p[5];
    fpga_diag_snapshot_t before;

    if (parse_byte_args(args, p, 5) != 0) {
        usb_send_str("Usage: fpga scope timing <20> <21> <26> <27> <28>\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    if (!fpga_send_cmd_timed(p[0], FPGA_CMD_FREQ_20, 15)) return;
    if (!fpga_send_cmd_timed(p[1], FPGA_CMD_FREQ_21, 15)) return;
    if (!fpga_send_cmd_timed(p[2], 0x26, 15)) return;
    if (!fpga_send_cmd_timed(p[3], 0x27, 15)) return;
    if (!fpga_send_cmd_timed(p[4], 0x28, 20)) return;

    usb_send_str("Scope timing block sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_fpga_scope_trig(const char *args)
{
    uint8_t p[4];
    const scope_state_t *ss = scope_state_get();
    uint8_t prefix_cmd = (ss->trigger.source == TRIG_SRC_CH2) ? 0x0A : 0x07;
    fpga_diag_snapshot_t before;

    if (parse_byte_args(args, p, 4) != 0) {
        usb_send_str("Usage: fpga scope trig <16> <17> <18> <19>\r\n");
        return;
    }

    fpga_diag_snapshot_take(&before);
    if (!fpga_send_cmd_timed(0x00, prefix_cmd, 15)) return;
    if (!fpga_send_cmd_timed(p[0], 0x16, 15)) return;
    if (!fpga_send_cmd_timed(p[1], 0x17, 15)) return;
    if (!fpga_send_cmd_timed(p[2], 0x18, 15)) return;
    if (!fpga_send_cmd_timed(p[3], 0x19, 20)) return;

    usb_send_str("Scope trigger block sent\r\n");
    fpga_diag_print_delta(&before);
}

static void cmd_reboot_bootloader(void)
{
    usb_send_str("Rebooting to bootloader...\r\n");
    usb_delay_ms(20);
    dfu_request_reboot();
}

static void cmd_spi3_read(const char *args)
{
    uint32_t len = 64;
    if (args && *args) parse_int(args, &len);
    if (len > FPGA_ADC_BUF_SIZE) len = FPGA_ADC_BUF_SIZE;

    const volatile uint8_t *ch1 = fpga_get_ch1_buf();
    if (!ch1) {
        usb_send_str("FPGA not initialized\r\n");
        return;
    }

    /*
     * SNAPSHOT FIRST. This used to hex-print `ch1[i]` straight out of the live
     * acquisition buffer, one byte at a time, interleaved with USB output — so
     * a full 1024-byte dump walked the buffer for ~270 ms while the acq task
     * refilled it every ~29 ms, rewriting it about nine times underneath.
     *
     * The records that came out were spliced, and it was not subtle: 11 of 12
     * consecutive dumps of one unchanged 500 Hz tone were spectrally torn
     * (mean sharpness 0.520), against 7 of 12 for `spi3 opread`, which already
     * snapshots. The tearing rate tracks READ DURATION, which is the signature
     * of the reader, not the device.
     *
     * That artifact reached a published conclusion — EXP-16 originally reported
     * "a fifth to a half of capture records are torn" as a property of
     * acquisition. It was a property of this loop. Corrected 2026-08-19.
     *
     * Copy under one pass, then print at leisure. Static rather than stack:
     * this runs on the shell task and 1 KB is more than it should borrow.
     */
    static uint8_t snap[FPGA_ADC_BUF_SIZE];
    for (uint32_t i = 0; i < len; i++)
        snap[i] = ch1[i];

    usb_debug_printf("CH1 buffer (%lu bytes):\r\n", len);
    for (uint32_t i = 0; i < len; i++) {
        if (i % 16 == 0) usb_debug_printf("%04lX:", i);
        usb_debug_printf(" %02X", snap[i]);
        if (i % 16 == 15 || i == len - 1) usb_send_str("\r\n");
    }
}

static int32_t scaled_i100(float value)
{
    float scaled = value * 100.0f;
    if (scaled >= 0.0f) return (int32_t)(scaled + 0.5f);
    return (int32_t)(scaled - 0.5f);
}

static int32_t scaled_i10000(float value)
{
    float scaled = value * 10000.0f;
    if (scaled >= 0.0f) return (int32_t)(scaled + 0.5f);
    return (int32_t)(scaled - 0.5f);
}

static void print_i100(const char *label, float value, const char *suffix)
{
    int32_t scaled = scaled_i100(value);
    int32_t abs_scaled = scaled < 0 ? -scaled : scaled;
    usb_debug_printf("%s%s%ld.%02ld%s\r\n",
                     label,
                     scaled < 0 ? "-" : "",
                     abs_scaled / 100,
                     abs_scaled % 100,
                     suffix ? suffix : "");
}

static const char *meter_layout_name(uint8_t layout)
{
    switch (layout) {
    case METER_LAYOUT_FULL:  return "full";
    case METER_LAYOUT_CHART: return "chart";
    case METER_LAYOUT_STATS: return "stats";
    case METER_LAYOUT_FUSE:  return "fuse";
    default:                 return "?";
    }
}

static meter_voltage_wave_snapshot_t usb_meter_wave_snap;

static void cmd_mode(const char *args)
{
    if (args == NULL || *args == '\0') {
        usb_debug_printf("current=%lu startup=%s meter_submode=%u (%s) layout=%u (%s)\r\n",
                         (uint32_t)current_mode,
                         startup_mode_name(startup_mode),
                         (unsigned)meter_submode,
                         meter_submode_name(meter_submode),
                         (unsigned)meter_layout,
                         meter_layout_name(meter_layout));
        return;
    }

    if (strcmp(args, "scope") == 0) {
        current_mode = MODE_OSCILLOSCOPE;
        fpga_enter_scope_mode();
        usb_send_str("mode=scope\r\n");
        return;
    }

    if (strncmp(args, "startup", 7) == 0 &&
        (args[7] == '\0' || args[7] == ' ' || args[7] == '\t')) {
        const char *value = args + 7;
        while (*value == ' ' || *value == '\t') value++;

        if (*value == '\0') {
            usb_debug_printf("startup=%s\r\n", startup_mode_name(startup_mode));
        } else if (strcmp(value, "scope") == 0) {
            startup_mode_set(STARTUP_SCOPE);
            usb_send_str("startup=Scope\r\n");
        } else if (strcmp(value, "meter") == 0) {
            startup_mode_set(STARTUP_METER);
            usb_send_str("startup=Meter\r\n");
        } else {
            usb_send_str("Usage: mode startup [scope|meter]\r\n");
        }
        return;
    }

    if (strncmp(args, "meter", 5) == 0 &&
        (args[5] == '\0' || args[5] == ' ' || args[5] == '\t')) {
        char buf[48];
        char *tok;
        char *saveptr = NULL;
        uint32_t submode = meter_submode;
        uint32_t layout = meter_layout;

        if (strlen(args) >= sizeof(buf)) {
            usb_send_str("Usage: mode meter [submode] [layout]\r\n");
            return;
        }
        strcpy(buf, args);
        tok = strtok_r(buf, " \t", &saveptr);  /* meter */
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok && (parse_int(tok, &submode) != 0 || submode >= METER_SUBMODE_COUNT)) {
            usb_debug_printf("Usage: mode meter [0-%u] [0-%u]\r\n",
                             (unsigned)(METER_SUBMODE_COUNT - 1),
                             (unsigned)(METER_LAYOUT_COUNT - 1));
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok && (parse_int(tok, &layout) != 0 || layout >= METER_LAYOUT_COUNT)) {
            usb_debug_printf("Usage: mode meter [0-%u] [0-%u]\r\n",
                             (unsigned)(METER_SUBMODE_COUNT - 1),
                             (unsigned)(METER_LAYOUT_COUNT - 1));
            return;
        }

        current_mode = MODE_MULTIMETER;
        meter_submode = (uint8_t)submode;
        meter_layout = (uint8_t)layout;
        meter_reset_minmaxavg();
        meter_voltage_wave_reset();
        /*
         * Match the button-driven production transition path. The explicit
         * `fpga meter reinit` debug command still performs the DCV wake preamble
         * when that diagnostic is wanted, but ordinary host mode switches must
         * not inject an extra DCV materialization before the requested submode.
         * Shorted-probe traces on 2026-06-07 showed that the wake preamble makes
         * host/sweep validation diverge from the real UI path and causes the
         * double relay click reported during sweeps.
         */
        fpga_set_meter_mode((uint8_t)submode);
        usb_debug_printf("mode=meter submode=%lu (%s) layout=%lu (%s)\r\n",
                         submode,
                         meter_submode_name((uint8_t)submode),
                         layout,
                         meter_layout_name((uint8_t)layout));
        return;
    }

    usb_send_str("Usage: mode [scope|meter [submode] [layout]|startup [scope|meter]]\r\n");
}

static void cmd_meter_dump(const char *args)
{
    uint32_t delay = 0;
    meter_autoselect_status_t auto_st;
    meter_reading_t snap;
    bool have_snap;
    bool live;

    if (args && *args) {
        if (parse_int(args, &delay) != 0 || delay > 5000) {
            usb_send_str("Usage: meter dump [delay_ms<=5000]\r\n");
            return;
        }
    }
    if (delay > 0) vTaskDelay(pdMS_TO_TICKS(delay));

    memset(&snap, 0, sizeof(snap));
    snap.display_str[0] = '-';
    snap.display_str[1] = '-';
    snap.display_str[2] = '-';
    snap.display_str[3] = '\0';
    snap.unit_suffix = "";
    have_snap = meter_data_snapshot(&snap);
    live = have_snap && snap.valid && snap.submode == meter_submode;
    meter_autoselect_get_status(&auto_st);

    usb_send_str("=== DMM State ===\r\n");
    usb_debug_printf("mode=%lu startup=%s meter_submode=%u (%s) layout=%u (%s)\r\n",
                     (uint32_t)current_mode,
                     startup_mode_name(startup_mode),
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     (unsigned)meter_layout,
                     meter_layout_name(meter_layout));
    usb_debug_printf("autoselect state=%s current=%u (%s) index=%u/%u best=%u (%s) best_score=%u last_score=%u cancel=%u\r\n",
                     meter_autoselect_state_name(auto_st.state),
                     (unsigned)auto_st.current_submode,
                     meter_submode_name(auto_st.current_submode),
                     (unsigned)auto_st.current_index,
                     (unsigned)auto_st.candidate_count,
                     (unsigned)auto_st.best_submode,
                     meter_submode_name(auto_st.best_submode),
                     (unsigned)auto_st.best_score,
                     (unsigned)auto_st.last_score,
                     auto_st.cancel_pending ? 1U : 0U);
    usb_debug_printf("ui_draws=%lu full_clears=%lu partial_clears=%lu draw_us=%lu max_draw_us=%lu over_budget=%lu rendered_update=%lu last_full=%u ui_live=%u continuity_flash=%u live_same_submode=%u\r\n",
                     meter_screen_draw_count,
                     meter_screen_full_clear_count,
                     meter_screen_partial_clear_count,
                     meter_screen_last_draw_us,
                     meter_screen_max_draw_us,
                     meter_screen_over_budget_count,
                     meter_screen_last_reading_display_update,
                     (unsigned)meter_screen_last_full_clear,
                     (unsigned)meter_screen_last_live,
                     (unsigned)meter_screen_last_continuity_flash,
                     live ? 1U : 0U);
    usb_debug_printf("valid=%u reading_submode=%u class=%u updates=%lu display=%s unit=%s\r\n",
                     snap.valid ? 1U : 0U,
                     (unsigned)snap.submode,
                     (unsigned)snap.result_class,
                     snap.update_count,
                     live ? snap.display_str : "---",
                     (live && snap.unit_suffix) ? snap.unit_suffix : "");
    usb_debug_printf("transition_discard_remaining=%u transition_frame_skips=%lu\r\n",
                     (unsigned)meter_frame_discard_count,
                     meter_transition_frame_skip_count);
    usb_debug_printf("bcd_value=%d decimal_pos=%u negative=%u unit_variant=%u bar_i100=%ld aux_freq_i10=%ld\r\n",
                     snap.bcd_value,
                     (unsigned)snap.decimal_pos,
                     snap.negative ? 1U : 0U,
                     (unsigned)snap.unit_variant,
                     (long)scaled_i100(snap.bar_fraction),
                     (long)scaled_i100(snap.aux_freq_hz) / 10L);
    usb_debug_printf("flags ac=%u auto=%u hold=%u probe=%u range_ind=%u range_cmd=%u beep=%u\r\n",
                     snap.is_ac ? 1U : 0U,
                     snap.is_auto_range ? 1U : 0U,
                     snap.is_hold ? 1U : 0U,
                     (unsigned)snap.probe_type,
                     (unsigned)snap.range_indicator,
                     (unsigned)snap.range_cmd,
                     snap.continuity_beep ? 1U : 0U);
    usb_debug_printf("stock_fsm mode=%u variant=%u format=%u dc_state=%u display_cmd=%u unit_index=%u composite=%u\r\n",
                     (unsigned)snap.stock_mode,
                     (unsigned)snap.stock_variant,
                     (unsigned)snap.stock_format,
                     (unsigned)snap.stock_dc_state,
                     (unsigned)snap.stock_display_cmd,
                     (unsigned)snap.stock_unit_index,
                     (unsigned)snap.stock_composite_index);
    usb_debug_printf("frame_family expected=%u observed=%u reject=%u\r\n",
                     (unsigned)snap.expected_frame_family,
                     (unsigned)snap.observed_frame_family,
                     (unsigned)snap.reject_reason);
    usb_send_str("frame=");
    for (int i = 0; i < 12; i++) usb_debug_printf("%02X%s", snap.dbg_frame[i], i == 11 ? "" : " ");
    usb_send_str("\r\nnibbles=");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X%s", snap.dbg_nibbles[i], i == 3 ? "" : " ");
    usb_send_str(" raw_digits=");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X%s", snap.dbg_raw_digits[i], i == 3 ? "" : " ");
    usb_send_str("\r\nf6_history=");
    uint8_t f6_count = meter_f6_history_count;
    if (f6_count > METER_F6_HISTORY_LEN) f6_count = METER_F6_HISTORY_LEN;
    for (int i = 0; i < f6_count; i++) usb_debug_printf("%02X%s", meter_f6_history[i], i + 1 == f6_count ? "" : " ");
    usb_send_str("\r\n");
    usb_debug_printf("wave_samples=%lu\r\n", meter_voltage_wave_sample_count());

    usb_send_str("history newest_first:\r\n");
    uint8_t count = meter_frame_history_count;
    if (count > METER_FRAME_HISTORY_LEN) count = METER_FRAME_HISTORY_LEN;
    for (uint8_t n = 0; n < count; n++) {
        uint8_t idx = (uint8_t)((meter_frame_history_head + METER_FRAME_HISTORY_LEN - 1U - n)
                                % METER_FRAME_HISTORY_LEN);
        const meter_frame_history_t *h = &meter_frame_history[idx];
        usb_debug_printf("#%u upd=%lu sub=%u cls=%u raw=%d dp=%u unit=%s disp=%s "
                         "family=%u/%u reject=%u "
                         "f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%u digits=%02X,%02X,%02X,%02X\r\n",
                         (unsigned)n,
                         h->update_count,
                         (unsigned)h->submode,
                         (unsigned)h->result_class,
                         h->bcd_value,
                         (unsigned)h->decimal_pos,
                         h->unit_suffix ? h->unit_suffix : "",
                         h->display_str,
                         (unsigned)h->expected_frame_family,
                         (unsigned)h->observed_frame_family,
                         (unsigned)h->reject_reason,
                         (unsigned)h->flags,
                         (unsigned)h->status,
                         (unsigned)h->meas_flags,
                         (unsigned)h->additional_status,
                         (unsigned)h->extra,
                         (unsigned)h->aux_freq_hz_i10,
                         (unsigned)h->raw_digits[0],
                         (unsigned)h->raw_digits[1],
                         (unsigned)h->raw_digits[2],
                         (unsigned)h->raw_digits[3]);
    }
}

static void cmd_meter_autoscan(const char *args)
{
    uint32_t settle_ms = 2500;
    uint32_t wait_budget_ms;
    uint8_t best_mode = meter_submode;
    uint8_t best_score = 0;
    size_t candidate_count = 0;
    const uint8_t *candidates = meter_auto_candidates(&candidate_count);

    if (args && *args) {
        if (parse_int(args, &settle_ms) != 0 || settle_ms > 3000) {
            usb_send_str("Usage: meter autoscan [settle_ms<=3000]\r\n");
            return;
        }
    }
    wait_budget_ms = (settle_ms < 2500U) ? 2500U : settle_ms;

    current_mode = MODE_MULTIMETER;
    meter_layout = METER_LAYOUT_FULL;
    usb_debug_printf("autoscan settle_ms=%lu wait_budget_ms=%lu candidates=%u\r\n",
                     settle_ms,
                     wait_budget_ms,
                     (unsigned)candidate_count);

    for (uint8_t i = 0; i < candidate_count; i++) {
        uint8_t submode = candidates[i];
        uint8_t score;

        meter_submode = submode;
        meter_reset_minmaxavg();
        meter_voltage_wave_reset();
        /*
         * Autoscan is a user-visible mode walk, not a transport wake diagnostic.
         * Use the same no-wake transition as button left/right so the scan does
         * not do a DCV preamble before every candidate or double-click relays.
         */
        fpga_set_meter_mode(submode);

        score = 0;
        for (uint32_t waited = 0; waited < wait_budget_ms; waited += 100U) {
            vTaskDelay(pdMS_TO_TICKS(100));
            score = meter_auto_score(submode, (const meter_reading_t *)&meter_reading);
            if (score > 0) break;
        }
        usb_debug_printf("auto candidate sub=%u (%s) score=%u valid=%u cls=%u "
                         "family=%u/%u reject=%u "
                         "disp=%s unit=%s raw=%d dp=%u f8=%02X seq=%u word=%04X apply=%04X\r\n",
                         (unsigned)submode,
                         meter_submode_name(submode),
                         (unsigned)score,
                         meter_reading.valid ? 1U : 0U,
                         (unsigned)meter_reading.result_class,
                         (unsigned)meter_reading.expected_frame_family,
                         (unsigned)meter_reading.observed_frame_family,
                         (unsigned)meter_reading.reject_reason,
                         (meter_reading.valid && meter_reading.submode == submode)
                            ? meter_reading.display_str : "---",
                         (meter_reading.valid && meter_reading.submode == submode &&
                          meter_reading.unit_suffix) ? meter_reading.unit_suffix : "",
                         meter_reading.bcd_value,
                         (unsigned)meter_reading.decimal_pos,
                         (unsigned)meter_reading.dbg_frame[8],
                         (unsigned)fpga.meter_mode_sequence_count,
                         (unsigned)fpga.meter_mode_selector_word,
                         (unsigned)fpga.meter_mode_apply_word);
        if (score > best_score) {
            best_score = score;
            best_mode = submode;
        }
    }

    meter_submode = best_mode;
    meter_reset_minmaxavg();
    meter_voltage_wave_reset();
    fpga_set_meter_mode(best_mode);
    usb_debug_printf("autoscan selected submode=%u (%s) score=%u\r\n",
                     (unsigned)best_mode,
                     meter_submode_name(best_mode),
                     (unsigned)best_score);
}

static void cmd_meter_auto_async(const char *args)
{
    meter_autoselect_status_t st;

    while (args && *args == ' ') args++;
    if (args == NULL || *args == '\0' || strcmp(args, "start") == 0) {
        if (meter_autoselect_start(700U)) {
            usb_send_str("meter auto started settle_ms=700\r\n");
        } else {
            usb_send_str("meter auto start failed\r\n");
        }
    } else if (strncmp(args, "start ", 6) == 0) {
        uint32_t settle_ms;
        if (parse_int(args + 6, &settle_ms) != 0 || settle_ms > 3000U) {
            usb_send_str("Usage: meter auto [start [settle_ms<=3000]|status|cancel]\r\n");
            return;
        }
        if (meter_autoselect_start(settle_ms)) {
            usb_debug_printf("meter auto started settle_ms=%lu\r\n", settle_ms);
        } else {
            usb_send_str("meter auto start failed\r\n");
        }
    } else if (strcmp(args, "status") == 0) {
        /* handled below */
    } else if (strcmp(args, "cancel") == 0) {
        meter_autoselect_cancel();
        usb_send_str("meter auto cancel requested\r\n");
    } else {
        usb_send_str("Usage: meter auto [start [settle_ms<=3000]|status|cancel]\r\n");
        return;
    }

    meter_autoselect_get_status(&st);
    usb_debug_printf("meter auto state=%s current=%u (%s) index=%u/%u "
                     "best=%u (%s) best_score=%u last_score=%u "
                     "settle_ms=%lu wait_budget_ms=%lu cancel=%u\r\n",
                     meter_autoselect_state_name(st.state),
                     (unsigned)st.current_submode,
                     meter_submode_name(st.current_submode),
                     (unsigned)st.current_index,
                     (unsigned)st.candidate_count,
                     (unsigned)st.best_submode,
                     meter_submode_name(st.best_submode),
                     (unsigned)st.best_score,
                     (unsigned)st.last_score,
                     st.settle_ms,
                     st.wait_budget_ms,
                     st.cancel_pending ? 1U : 0U);
}

static uint8_t gpio_level(gpio_type *port, uint16_t pin)
{
    return (port->idt & (1U << pin)) ? 1U : 0U;
}

static int parse_stream_args(const char *args, uint32_t *count, uint32_t *delay_ms,
                             const char *usage)
{
    char buf[32];
    char *tok;
    char *saveptr = NULL;

    if (args == NULL || *args == '\0') return 0;
    if (strlen(args) >= sizeof(buf)) {
        usb_send_str(usage);
        return -1;
    }

    strcpy(buf, args);
    tok = strtok_r(buf, " \t", &saveptr);
    if (tok && (parse_int(tok, count) != 0 || *count > 200)) {
        usb_send_str(usage);
        return -1;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok && (parse_int(tok, delay_ms) != 0 || *delay_ms > 5000)) {
        usb_send_str(usage);
        return -1;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok) {
        usb_send_str(usage);
        return -1;
    }
    return 0;
}

static uint16_t meter_dbg_extra(void)
{
    return ((uint16_t)meter_reading.dbg_frame[10] << 8) |
           meter_reading.dbg_frame[11];
}

static int parse_mux_arm_args(const char *args, uint32_t *portc_porte,
                              uint32_t *porta_portb, uint32_t *settle_ms,
                              const char *usage)
{
    char buf[48];
    char *tok;
    char *saveptr = NULL;

    if (args == NULL || *args == '\0' || strlen(args) >= sizeof(buf)) {
        usb_send_str(usage);
        return -1;
    }

    strcpy(buf, args);
    tok = strtok_r(buf, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, portc_porte) != 0 ||
        *portc_porte > 9) {
        usb_send_str(usage);
        return -1;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok == NULL || parse_int(tok, porta_portb) != 0 ||
        *porta_portb > 9) {
        usb_send_str(usage);
        return -1;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok && (parse_int(tok, settle_ms) != 0 || *settle_ms > 5000)) {
        usb_send_str(usage);
        return -1;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok) {
        usb_send_str(usage);
        return -1;
    }
    return 0;
}

static void print_frame_hex(const char *label, const uint8_t frame[12])
{
    usb_send_str(label);
    for (int i = 0; i < 12; i++) {
        usb_debug_printf("%02X%s", (unsigned)frame[i], i == 11 ? "" : " ");
    }
    usb_send_str("\r\n");
}

static void print_volatile_frame_inline(const volatile uint8_t frame[12])
{
    for (int i = 0; i < 12; i++) {
        usb_debug_printf("%s%02X", i == 0 ? "" : " ", (unsigned)frame[i]);
    }
}

static void print_tx_frame_inline(const uint8_t frame[FPGA_TX_FRAME_SIZE])
{
    for (uint8_t i = 0; i < FPGA_TX_FRAME_SIZE; i++) {
        usb_debug_printf("%s%02X", i == 0 ? "" : " ",
                         (unsigned)frame[i]);
    }
}

static void cmd_meter_trace(void)
{
    meter_reading_t snap;
    bool have_snap = meter_data_snapshot(&snap);
    bool live = have_snap && current_mode == MODE_MULTIMETER &&
                snap.valid && snap.submode == meter_submode;
    fpga_meter_selector_t selectors = fpga_meter_expected_selectors(meter_submode);
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(meter_submode);
    uint16_t plan_probe_word = plan.has_probe_detect ?
        (uint16_t)(gpio_level(GPIOC, 7) ? 0x0507U : 0x050AU) : 0U;
    uint16_t extra = have_snap ?
        (((uint16_t)snap.dbg_frame[10] << 8) | snap.dbg_frame[11]) : 0;
    uint8_t rxh_count;
    uint8_t rxh_frames[FPGA_RX_FRAME_HISTORY][FPGA_RX_FRAME_SIZE];
    uint16_t rxh_data[FPGA_RX_FRAME_HISTORY];
    uint16_t rxh_tx[FPGA_RX_FRAME_HISTORY];
    uint16_t rxh_echo[FPGA_RX_FRAME_HISTORY];
    uint16_t rxh_seq[FPGA_RX_FRAME_HISTORY];
    uint8_t rxh_seq_sub[FPGA_RX_FRAME_HISTORY];
    uint8_t rxh_busy[FPGA_RX_FRAME_HISTORY];
    uint8_t rxh_discard[FPGA_RX_FRAME_HISTORY];
    uint8_t txh_count;
    uint8_t txh_frames[FPGA_TX_FRAME_HISTORY][FPGA_TX_FRAME_SIZE];
    uint16_t txh_tx[FPGA_TX_FRAME_HISTORY];
    uint8_t txc_count;
    uint8_t txc_frames[FPGA_TX_FRAME_HISTORY][FPGA_TX_FRAME_SIZE];
    uint16_t txc_tx[FPGA_TX_FRAME_HISTORY];
    uint8_t mth_count;
    uint8_t mth_submode[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_config[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_selector[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_apply[FPGA_METER_TRANSITION_HISTORY];
    uint8_t mth_bank[FPGA_METER_TRANSITION_HISTORY];
    uint8_t mth_bank_first[FPGA_METER_TRANSITION_HISTORY];
    uint8_t mth_bank_second[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_probe[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_start[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_seq[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_tx_before[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_tx_after[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_frame_before[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_frame_after[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_planned_gpio[FPGA_METER_TRANSITION_HISTORY];
    uint16_t mth_actual_gpio[FPGA_METER_TRANSITION_HISTORY];
    uint8_t first_rx_valid;
    uint8_t first_rx_armed;
    uint8_t first_rx_submode;
    uint16_t first_rx_seq;
    uint16_t first_rx_config;
    uint16_t first_rx_selector;
    uint16_t first_rx_apply;
    uint16_t first_rx_probe;
    uint16_t first_rx_start;
    uint16_t first_rx_planned_gpio;
    uint16_t first_rx_actual_gpio;
    uint16_t first_rx_data;
    uint16_t first_rx_tx;
    uint16_t first_rx_echo;
    uint8_t first_rx_busy;
    uint8_t first_rx_discard;
    uint8_t first_rx_frame[FPGA_RX_FRAME_SIZE];
    uint32_t first_rx_h2_bytes;
    uint8_t first_rx_h2_done;
    uint8_t first_rx_h2_post_run;
    uint8_t first_rx_h2_post_mask;
    uint8_t producer_frame[FPGA_RX_FRAME_SIZE];
    uint16_t rx_sync_data_start;
    uint16_t rx_sync_echo_start;
    uint16_t rx_sync_data_header;
    uint16_t rx_sync_echo_header;
    uint16_t rx_sync_bad_second;
    uint16_t rx_sync_stray;
    uint8_t last_echo_frame[FPGA_RX_ECHO_FRAME_SIZE];
    uint8_t rxraw_count;
    uint8_t rxraw_byte[FPGA_RX_RAW_HISTORY];
    uint16_t rxraw_tx[FPGA_RX_RAW_HISTORY];
    uint8_t rxraw_tx_index[FPGA_RX_RAW_HISTORY];
    uint8_t rxraw_rx_index[FPGA_RX_RAW_HISTORY];
    uint8_t post_h2_trigger[FPGA_POST_H2_TRIGGER_HISTORY];
    uint8_t post_h2_rx_len[FPGA_POST_H2_TRIGGER_HISTORY];
    uint8_t post_h2_rx[FPGA_POST_H2_TRIGGER_HISTORY][FPGA_POST_H2_RX_HISTORY];
    uint32_t h2_rx_00_count;
    uint32_t h2_rx_ff_count;
    uint32_t h2_rx_other_count;
    uint8_t h2_close_rx_len;
    uint8_t h2_close_rx[sizeof(fpga.h2_close_rx)];
    const factory_cal_t *factory_cal = flash_fs_factory_cal();

    taskENTER_CRITICAL();
    memcpy(producer_frame, (const void *)fpga.rx_frame, FPGA_RX_FRAME_SIZE);
    rx_sync_data_start = fpga.rx_sync_data_start_count;
    rx_sync_echo_start = fpga.rx_sync_echo_start_count;
    rx_sync_data_header = fpga.rx_sync_data_header_count;
    rx_sync_echo_header = fpga.rx_sync_echo_header_count;
    rx_sync_bad_second = fpga.rx_sync_bad_second_count;
    rx_sync_stray = fpga.rx_sync_stray_count;
    memcpy(last_echo_frame, (const void *)fpga.last_rx_echo_frame,
           sizeof(last_echo_frame));
    rxraw_count = fpga.rx_raw_history_count;
    if (rxraw_count > FPGA_RX_RAW_HISTORY) {
        rxraw_count = FPGA_RX_RAW_HISTORY;
    }
    for (uint8_t n = 0; n < rxraw_count; n++) {
        uint8_t idx = (uint8_t)((fpga.rx_raw_history_head +
                                 FPGA_RX_RAW_HISTORY - 1U - n) %
                                FPGA_RX_RAW_HISTORY);
        rxraw_byte[n] = fpga.rx_raw_history[idx];
        rxraw_tx[n] = fpga.rx_raw_history_tx_count[idx];
        rxraw_tx_index[n] = fpga.rx_raw_history_tx_index[idx];
        rxraw_rx_index[n] = fpga.rx_raw_history_rx_index[idx];
    }
    memcpy(post_h2_trigger, (const void *)fpga.post_h2_spi3_trigger,
           sizeof(post_h2_trigger));
    memcpy(post_h2_rx_len, (const void *)fpga.post_h2_spi3_rx_len,
           sizeof(post_h2_rx_len));
    memcpy(post_h2_rx, (const void *)fpga.post_h2_spi3_rx,
           sizeof(post_h2_rx));
    h2_rx_00_count = fpga.h2_rx_00_count;
    h2_rx_ff_count = fpga.h2_rx_ff_count;
    h2_rx_other_count = fpga.h2_rx_other_count;
    h2_close_rx_len = fpga.h2_close_rx_len;
    memcpy(h2_close_rx, (const void *)fpga.h2_close_rx, sizeof(h2_close_rx));
    first_rx_valid = fpga.meter_first_rx_after_transition_valid;
    first_rx_armed = fpga.meter_first_rx_after_transition_armed;
    first_rx_submode = fpga.meter_first_rx_after_transition_submode;
    first_rx_seq = fpga.meter_first_rx_after_transition_seq;
    first_rx_config = fpga.meter_first_rx_after_transition_config;
    first_rx_selector = fpga.meter_first_rx_after_transition_selector;
    first_rx_apply = fpga.meter_first_rx_after_transition_apply;
    first_rx_probe = fpga.meter_first_rx_after_transition_probe;
    first_rx_start = fpga.meter_first_rx_after_transition_start;
    first_rx_planned_gpio =
        fpga.meter_first_rx_after_transition_planned_gpio;
    first_rx_actual_gpio = fpga.meter_first_rx_after_transition_actual_gpio;
    first_rx_data = fpga.meter_first_rx_after_transition_data;
    first_rx_tx = fpga.meter_first_rx_after_transition_tx;
    first_rx_echo = fpga.meter_first_rx_after_transition_echo;
    first_rx_busy = fpga.meter_first_rx_after_transition_busy;
    first_rx_discard = fpga.meter_first_rx_after_transition_discard;
    memcpy(first_rx_frame,
           (const void *)fpga.meter_first_rx_after_transition_frame,
           FPGA_RX_FRAME_SIZE);
    first_rx_h2_bytes = fpga.meter_first_rx_after_transition_h2_bytes;
    first_rx_h2_done = fpga.meter_first_rx_after_transition_h2_done;
    first_rx_h2_post_run = fpga.meter_first_rx_after_transition_h2_post_run_count;
    first_rx_h2_post_mask = fpga.meter_first_rx_after_transition_h2_post_mask;
    rxh_count = fpga.rx_frame_history_count;
    if (rxh_count > FPGA_RX_FRAME_HISTORY) rxh_count = FPGA_RX_FRAME_HISTORY;
    for (uint8_t n = 0; n < rxh_count; n++) {
        uint8_t idx = (uint8_t)((fpga.rx_frame_history_head +
                                 FPGA_RX_FRAME_HISTORY - 1U - n) %
                                FPGA_RX_FRAME_HISTORY);
        memcpy(rxh_frames[n], (const void *)fpga.rx_frame_history[idx],
               FPGA_RX_FRAME_SIZE);
        rxh_data[n] = fpga.rx_history_frame_count[idx];
        rxh_tx[n] = fpga.rx_history_tx_count[idx];
        rxh_echo[n] = fpga.rx_history_echo_count[idx];
        rxh_seq[n] = fpga.rx_history_sequence_count[idx];
        rxh_seq_sub[n] = fpga.rx_history_sequence_submode[idx];
        rxh_busy[n] = fpga.rx_history_transition_busy[idx];
        rxh_discard[n] = fpga.rx_history_discard_remaining[idx];
    }
    txh_count = fpga.tx_frame_history_count;
    if (txh_count > FPGA_TX_FRAME_HISTORY) {
        txh_count = FPGA_TX_FRAME_HISTORY;
    }
    for (uint8_t n = 0; n < txh_count; n++) {
        uint8_t idx = (uint8_t)((fpga.tx_frame_history_head +
                                 FPGA_TX_FRAME_HISTORY - 1U - n) %
                                FPGA_TX_FRAME_HISTORY);
        memcpy(txh_frames[n], (const void *)fpga.tx_frame_history[idx],
               FPGA_TX_FRAME_SIZE);
        txh_tx[n] = fpga.tx_frame_history_tx_count[idx];
    }
    txc_count = fpga.tx_control_frame_history_count;
    if (txc_count > FPGA_TX_FRAME_HISTORY) {
        txc_count = FPGA_TX_FRAME_HISTORY;
    }
    for (uint8_t n = 0; n < txc_count; n++) {
        uint8_t idx = (uint8_t)((fpga.tx_control_frame_history_head +
                                 FPGA_TX_FRAME_HISTORY - 1U - n) %
                                FPGA_TX_FRAME_HISTORY);
        memcpy(txc_frames[n], (const void *)fpga.tx_control_frame_history[idx],
               FPGA_TX_FRAME_SIZE);
        txc_tx[n] = fpga.tx_control_frame_history_tx_count[idx];
    }
    mth_count = fpga.meter_transition_history_count;
    if (mth_count > FPGA_METER_TRANSITION_HISTORY) {
        mth_count = FPGA_METER_TRANSITION_HISTORY;
    }
    for (uint8_t n = 0; n < mth_count; n++) {
        uint8_t idx = (uint8_t)((fpga.meter_transition_history_head +
                                 FPGA_METER_TRANSITION_HISTORY - 1U - n) %
                                FPGA_METER_TRANSITION_HISTORY);
        mth_submode[n] = fpga.meter_transition_history_submode[idx];
        mth_config[n] = fpga.meter_transition_history_config[idx];
        mth_selector[n] = fpga.meter_transition_history_selector[idx];
        mth_apply[n] = fpga.meter_transition_history_apply[idx];
        mth_bank[n] = fpga.meter_transition_history_bank[idx];
        mth_bank_first[n] = fpga.meter_transition_history_bank_first[idx];
        mth_bank_second[n] = fpga.meter_transition_history_bank_second[idx];
        mth_probe[n] = fpga.meter_transition_history_probe[idx];
        mth_start[n] = fpga.meter_transition_history_start[idx];
        mth_seq[n] = fpga.meter_transition_history_sequence_count[idx];
        mth_tx_before[n] = fpga.meter_transition_history_tx_before[idx];
        mth_tx_after[n] = fpga.meter_transition_history_tx_after[idx];
        mth_frame_before[n] = fpga.meter_transition_history_frame_before[idx];
        mth_frame_after[n] = fpga.meter_transition_history_frame_after[idx];
        mth_planned_gpio[n] = fpga.meter_transition_history_planned_gpio[idx];
        mth_actual_gpio[n] = fpga.meter_transition_history_actual_gpio[idx];
    }
    taskEXIT_CRITICAL();

    usb_send_str("=== DMM Trace ===\r\n");
    usb_debug_printf("trace v=1 snapshot=%u\r\n", have_snap ? 1U : 0U);
    if (!have_snap) return;

    usb_debug_printf("context mode=%lu startup=%s ui_sub=%u "
                     "reading_sub=%u live=%u valid=%u updates=%lu\r\n",
                     (uint32_t)current_mode,
                     startup_mode_name(startup_mode),
                     (unsigned)meter_submode,
                     (unsigned)snap.submode,
                     live ? 1U : 0U,
                     snap.valid ? 1U : 0U,
                     snap.update_count);
    usb_debug_printf("producer counts tx=%u rx_bytes=%u data=%u echo=%u "
                     "rx_valid=%u\r\n",
                     fpga.tx_count,
                     fpga.rx_byte_count,
                     fpga.frame_count,
                     fpga.echo_count,
                     fpga.rx_frame_valid ? 1U : 0U);
    usb_debug_printf("producer_last_rx data=%u tx=%u echo=%u seq=%u "
                     "seq_sub=%u busy=%u discard=%u\r\n",
                     fpga.last_rx_frame_count,
                     fpga.last_rx_tx_count,
                     fpga.last_rx_echo_count,
                     fpga.last_rx_mode_sequence_count,
                     (unsigned)fpga.last_rx_mode_sequence_submode,
                     (unsigned)fpga.last_rx_transition_busy,
                     (unsigned)fpga.last_rx_discard_remaining);
    usb_debug_printf("rx_sync data_start=%u echo_start=%u data_hdr=%u "
                     "echo_hdr=%u bad_second=%u stray=%u "
                     "data_tx_busy_drop=%u echo_valid=%u echo_bad=%u\r\n",
                     rx_sync_data_start,
                     rx_sync_echo_start,
                     rx_sync_data_header,
                     rx_sync_echo_header,
                     rx_sync_bad_second,
                     rx_sync_stray,
                     fpga.rx_data_tx_busy_drop_count,
                     fpga.rx_echo_valid_count,
                     fpga.rx_echo_bad_count);
    usb_debug_printf("plan stock_mode=%u raw_low=%02X family=%u mux=%u "
                     "portc_porte=%u porta_portb=%u settle_ms=%u discard=%u "
                     "bank=%u/%02X/%02X\r\n",
                     (unsigned)selectors.function_selector,
                     (unsigned)selectors.range_selector,
                     (unsigned)plan.frame_family,
                     (unsigned)plan.mux_index,
                     (unsigned)plan.portc_porte_mux,
                     (unsigned)plan.porta_portb_mux,
                     (unsigned)plan.settle_ms,
                     (unsigned)plan.discard_frames,
                     plan.has_command_bank_prefix ? 1U : 0U,
                     (unsigned)plan.command_bank_first,
                     (unsigned)plan.command_bank_second);
    usb_debug_printf("wire config=%04X has_config=%u selector=%04X "
                     "apply=%04X has_apply=%u probe=%04X start=%04X "
                     "seq_count=%u seq_sub=%u\r\n",
                     (unsigned)plan.config_word,
                     plan.has_config_word ? 1U : 0U,
                     (unsigned)plan.selector_word,
                     (unsigned)plan.apply_word,
                     plan.has_apply_word ? 1U : 0U,
                     (unsigned)plan_probe_word,
                     (unsigned)plan.start_word,
                     (unsigned)fpga.meter_mode_sequence_count,
                     (unsigned)fpga.meter_mode_sequence_submode);
    usb_debug_printf("last_sequence config=%04X selector=%04X apply=%04X "
                     "probe=%04X start=%04X\r\n",
                     (unsigned)fpga.meter_mode_config_word,
                     (unsigned)fpga.meter_mode_selector_word,
                     (unsigned)fpga.meter_mode_apply_word,
                     (unsigned)fpga.meter_mode_probe_word,
                     (unsigned)fpga.meter_mode_start_word);
    usb_debug_printf("decoded display=%s unit=%s value_i10000=%ld raw=%d "
                     "dp=%u class=%u reject=%u family=%u/%u extra=%04X\r\n",
                     snap.valid ? snap.display_str : "---",
                     (snap.valid && snap.unit_suffix) ? snap.unit_suffix : "",
                     (long)scaled_i10000(snap.value),
                     snap.bcd_value,
                     (unsigned)snap.decimal_pos,
                     (unsigned)snap.result_class,
                     (unsigned)snap.reject_reason,
                     (unsigned)snap.expected_frame_family,
                     (unsigned)snap.observed_frame_family,
                     (unsigned)extra);
    usb_debug_printf("stock_fsm mode=%u variant=%u format=%u dc_state=%u "
                     "display_cmd=%u unit_index=%u composite=%u\r\n",
                     (unsigned)snap.stock_mode,
                     (unsigned)snap.stock_variant,
                     (unsigned)snap.stock_format,
                     (unsigned)snap.stock_dc_state,
                     (unsigned)snap.stock_display_cmd,
                     (unsigned)snap.stock_unit_index,
                     (unsigned)snap.stock_composite_index);
    usb_debug_printf("transition busy=%u discard_now=%u skip_count=%lu\r\n",
                     fpga_meter_transition_busy() ? 1U : 0U,
                     (unsigned)meter_frame_discard_count,
                     meter_transition_frame_skip_count);
    print_frame_hex("producer_frame=", producer_frame);
    print_frame_hex("parsed_frame=", snap.dbg_frame);
    usb_debug_printf("first_transition_rx valid=%u armed=%u sub=%u seq=%u "
                     "config=%04X selector=%04X apply=%04X probe=%04X "
                     "start=%04X "
                     "planned_gpio=%03X actual_gpio=%03X data=%u tx=%u "
                     "echo=%u busy=%u discard=%u h2_bytes=%lu h2_done=%u "
                     "h2_post_run=%u h2_post_mask=%02X frame=",
                     (unsigned)first_rx_valid,
                     (unsigned)first_rx_armed,
                     (unsigned)first_rx_submode,
                     first_rx_seq,
                     first_rx_config,
                     first_rx_selector,
                     first_rx_apply,
                     first_rx_probe,
                     first_rx_start,
                     first_rx_planned_gpio,
                     first_rx_actual_gpio,
                     first_rx_data,
                     first_rx_tx,
                     first_rx_echo,
                     (unsigned)first_rx_busy,
                     (unsigned)first_rx_discard,
                     first_rx_h2_bytes,
                     (unsigned)first_rx_h2_done,
                     (unsigned)first_rx_h2_post_run,
                     (unsigned)first_rx_h2_post_mask);
    print_volatile_frame_inline(first_rx_frame);
    usb_send_str("\r\n");
    usb_send_str("last_echo_frame=");
    for (uint8_t i = 0; i < FPGA_RX_ECHO_FRAME_SIZE; i++) {
        usb_debug_printf("%s%02X", i == 0 ? "" : " ",
                         (unsigned)last_echo_frame[i]);
    }
    usb_send_str("\r\n");
    usb_send_str("rx_raw newest_first:\r\n");
    for (uint8_t n = 0; n < rxraw_count; n++) {
        usb_debug_printf("rxraw n=%u tx=%u txi=%u rxi=%u byte=%02X\r\n",
                         (unsigned)n,
                         rxraw_tx[n],
                         (unsigned)rxraw_tx_index[n],
                         (unsigned)rxraw_rx_index[n],
                         (unsigned)rxraw_byte[n]);
    }
    usb_send_str("transition_history newest_first:\r\n");
    for (uint8_t n = 0; n < mth_count; n++) {
        usb_debug_printf("mth n=%u sub=%u seq=%u config=%04X selector=%04X "
                         "apply=%04X bank=%u/%02X/%02X probe=%04X "
                         "start=%04X tx=%u..%u data=%u..%u planned_gpio=%03X "
                         "actual_gpio=%03X\r\n",
                         (unsigned)n,
                         (unsigned)mth_submode[n],
                         mth_seq[n],
                         mth_config[n],
                         mth_selector[n],
                         mth_apply[n],
                         (unsigned)mth_bank[n],
                         (unsigned)mth_bank_first[n],
                         (unsigned)mth_bank_second[n],
                         mth_probe[n],
                         mth_start[n],
                         mth_tx_before[n],
                         mth_tx_after[n],
                         mth_frame_before[n],
                         mth_frame_after[n],
                         mth_planned_gpio[n],
                         mth_actual_gpio[n]);
    }
    usb_send_str("producer_history newest_first:\r\n");
    for (uint8_t n = 0; n < rxh_count; n++) {
        usb_debug_printf("rxh n=%u data=%u tx=%u echo=%u seq=%u seq_sub=%u "
                         "busy=%u discard=%u frame=",
                         (unsigned)n,
                         rxh_data[n],
                         rxh_tx[n],
                         rxh_echo[n],
                         rxh_seq[n],
                         (unsigned)rxh_seq_sub[n],
                         (unsigned)rxh_busy[n],
                         (unsigned)rxh_discard[n]);
        print_volatile_frame_inline(rxh_frames[n]);
        usb_send_str("\r\n");
    }
    usb_send_str("tx_history newest_first:\r\n");
    for (uint8_t n = 0; n < txh_count; n++) {
        usb_debug_printf("txh n=%u tx=%u frame=", (unsigned)n, txh_tx[n]);
        print_tx_frame_inline(txh_frames[n]);
        usb_send_str("\r\n");
    }
    usb_send_str("tx_control_history newest_first:\r\n");
    for (uint8_t n = 0; n < txc_count; n++) {
        usb_debug_printf("txc n=%u tx=%u frame=", (unsigned)n, txc_tx[n]);
        print_tx_frame_inline(txc_frames[n]);
        usb_send_str("\r\n");
    }
    usb_debug_printf("gpio control PC6=%u PB11=%u PC11=%u PC7=%u PC0=%u\r\n",
                     gpio_level(GPIOC, 6),
                     gpio_level(GPIOB, 11),
                     gpio_level(GPIOC, 11),
                     gpio_level(GPIOC, 7),
                     gpio_level(GPIOC, 0));
    usb_debug_printf("gpio_frontend PC12=%u PE4=%u PE5=%u PE6=%u PA15=%u "
                     "PA10=%u PB10=%u PB9=%u PA6=%u\r\n",
                     gpio_level(GPIOC, 12),
                     gpio_level(GPIOE, 4),
                     gpio_level(GPIOE, 5),
                     gpio_level(GPIOE, 6),
                     gpio_level(GPIOA, 15),
                     gpio_level(GPIOA, 10),
                     gpio_level(GPIOB, 10),
                     gpio_level(GPIOB, 9),
                     gpio_level(GPIOA, 6));
    uint16_t h2_close_nonff = count_non_ff_bytes(h2_close_rx, h2_close_rx_len);
    uint16_t post_h2_nonff =
        count_post_h2_non_ff_snapshot(post_h2_rx_len, post_h2_rx);

    usb_debug_printf("h2 bytes=%lu done=%u post_enq=%u post_run=%u "
                     "post_drop=%u post_mask=%02X post_rx_nonff=%u "
                     "spi_ok=%u spi_to=%u rx00=%lu rxff=%lu rxother=%lu "
                     "rx_nonff=%lu close_len=%u close_nonff=%u\r\n",
                     fpga.h2_bytes_sent,
                     fpga.h2_upload_done ? 1U : 0U,
                     (unsigned)fpga.post_h2_spi3_boot_enqueued,
                     (unsigned)fpga.post_h2_spi3_boot_run_count,
                     (unsigned)fpga.post_h2_spi3_boot_dropped,
                     (unsigned)fpga.post_h2_spi3_boot_mask,
                     (unsigned)post_h2_nonff,
                     fpga.spi3_ok_count,
                     fpga.spi3_total_timeouts,
                     h2_rx_00_count,
                     h2_rx_ff_count,
                     h2_rx_other_count,
                     h2_rx_00_count + h2_rx_other_count,
                     (unsigned)h2_close_rx_len,
                     (unsigned)h2_close_nonff);
    usb_debug_printf("factory_cal loaded=%u ch_size=%u channels=%u\r\n",
                     (factory_cal != NULL && factory_cal->loaded) ? 1U : 0U,
                     (unsigned)FACTORY_CAL_CHANNEL_SIZE,
                     (unsigned)FACTORY_CAL_NUM_CHANNELS);
    usb_send_str("h2_close_rx bytes=");
    for (uint8_t i = 0; i < h2_close_rx_len && i < sizeof(h2_close_rx); i++) {
        usb_debug_printf("%s%02X", i == 0 ? "" : " ",
                         (unsigned)h2_close_rx[i]);
    }
    usb_send_str("\r\n");
    for (uint8_t i = 0; i < FPGA_POST_H2_TRIGGER_HISTORY; i++) {
        uint8_t len = post_h2_rx_len[i];
        uint8_t shown = len;
        if (shown > FPGA_POST_H2_RX_HISTORY) shown = FPGA_POST_H2_RX_HISTORY;
        usb_debug_printf("h2_post_rx n=%u trigger=%02X len=%u bytes=",
                         (unsigned)i,
                         (unsigned)post_h2_trigger[i],
                         (unsigned)len);
        for (uint8_t j = 0; j < shown; j++) {
            usb_debug_printf("%s%02X", j == 0 ? "" : " ",
                             (unsigned)post_h2_rx[i][j]);
        }
        if (len > FPGA_POST_H2_RX_HISTORY) usb_send_str(" ...");
        usb_send_str("\r\n");
    }
}

static void cmd_meter_frontend(void)
{
    uint16_t extra = meter_dbg_extra();
    fpga_meter_selector_t selectors = fpga_meter_expected_selectors(meter_submode);
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(meter_submode);
    uint8_t tx_count = fpga.tx_cmd_history_count;

    usb_send_str("=== DMM Frontend ===\r\n");
    usb_debug_printf("mode=%lu startup=%s meter_submode=%u (%s) reading_submode=%u valid=%u class=%u updates=%lu\r\n",
                     (uint32_t)current_mode,
                     startup_mode_name(startup_mode),
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     (unsigned)meter_reading.submode,
                     meter_reading.valid ? 1U : 0U,
                     (unsigned)meter_reading.result_class,
                     meter_reading.update_count);
    usb_debug_printf("expected_selector stock_mode=%u raw_low=%02X voltage_function_axis=%u\r\n",
                     (unsigned)selectors.function_selector,
                     (unsigned)selectors.range_selector,
                     selectors.voltage_function_axis ? 1U : 0U);
    usb_debug_printf("transition_plan family=%u mux=%u portc_porte_mux=%u porta_portb_mux=%u discard=%u settle_ms=%u selector=%04X apply=%04X has_apply=%u\r\n",
                     (unsigned)plan.frame_family,
                     (unsigned)plan.mux_index,
                     (unsigned)plan.portc_porte_mux,
                     (unsigned)plan.porta_portb_mux,
                     (unsigned)plan.discard_frames,
                     (unsigned)plan.settle_ms,
                     (unsigned)plan.selector_word,
                     (unsigned)plan.apply_word,
                     plan.has_apply_word ? 1U : 0U);
    usb_debug_printf("mode_sequence count=%u submode=%u selector=%04X apply=%04X probe=%04X start=%04X\r\n",
                     (unsigned)fpga.meter_mode_sequence_count,
                     (unsigned)fpga.meter_mode_sequence_submode,
                     (unsigned)fpga.meter_mode_selector_word,
                     (unsigned)fpga.meter_mode_apply_word,
                     (unsigned)fpga.meter_mode_probe_word,
                     (unsigned)fpga.meter_mode_start_word);
    usb_debug_printf("display=%s unit=%s bcd_value=%d dp=%u f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%ld beep=%u discard=%u trans_skips=%lu\r\n",
                     meter_reading.valid ? meter_reading.display_str : "---",
                     (meter_reading.valid && meter_reading.unit_suffix) ? meter_reading.unit_suffix : "",
                     meter_reading.bcd_value,
                     (unsigned)meter_reading.decimal_pos,
                     (unsigned)meter_reading.dbg_frame[6],
                     (unsigned)meter_reading.dbg_frame[7],
                     (unsigned)meter_reading.dbg_frame[8],
                     (unsigned)meter_reading.dbg_frame[9],
                     (unsigned)extra,
                     (long)scaled_i100(meter_reading.aux_freq_hz) / 10L,
                     meter_reading.continuity_beep ? 1U : 0U,
                     (unsigned)meter_frame_discard_count,
                     meter_transition_frame_skip_count);
    usb_debug_printf("frame_family expected=%u observed=%u reject=%u\r\n",
                     (unsigned)meter_reading.expected_frame_family,
                     (unsigned)meter_reading.observed_frame_family,
                     (unsigned)meter_reading.reject_reason);
    usb_send_str("tx_recent:");
    for (uint8_t i = 0; i < tx_count; i++) {
        uint8_t idx = (uint8_t)((fpga.tx_cmd_history_head + 16U - tx_count + i) & 0x0FU);
        usb_debug_printf(" %02X%02X",
                         (unsigned)fpga.tx_cmd_hi_history[idx],
                         (unsigned)fpga.tx_cmd_lo_history[idx]);
    }
    usb_send_str("\r\n");
    usb_print_last_tx_frame();
    usb_print_recent_tx_frames();
    usb_debug_printf("control PC6_spi=%u PB11_active=%u PC11_meter_mux=%u PC7_probe=%u PC0_ready=%u probe_tail_override=%d\r\n",
                     gpio_level(GPIOC, 6),
                     gpio_level(GPIOB, 11),
                     gpio_level(GPIOC, 11),
                     gpio_level(GPIOC, 7),
                     gpio_level(GPIOC, 0),
                     (int)fpga_meter_probe_tail_override);
    usb_debug_printf("port_c_e PC12_route=%u PE4=%u PE5=%u PE6=%u\r\n",
                     gpio_level(GPIOC, 12),
                     gpio_level(GPIOE, 4),
                     gpio_level(GPIOE, 5),
                     gpio_level(GPIOE, 6));
    usb_debug_printf("port_a_b PA15=%u PA10=%u PB10=%u PB9=%u PA6=%u\r\n",
                     gpio_level(GPIOA, 15),
                     gpio_level(GPIOA, 10),
                     gpio_level(GPIOB, 10),
                     gpio_level(GPIOB, 9),
                     gpio_level(GPIOA, 6));
}

static void cmd_meter_probe_tail(const char *args)
{
    while (args != NULL && (*args == ' ' || *args == '\t')) args++;

    if (args == NULL || *args == '\0') {
        if (fpga_meter_probe_tail_override < 0) {
            usb_debug_printf("meter probe-tail=auto PC7=%u live=%02X\r\n",
                             gpio_level(GPIOC, 7),
                             gpio_level(GPIOC, 7) ? 0x07U : FPGA_CMD_METER_NOPROBE);
        } else {
            usb_debug_printf("meter probe-tail=%02X override=1 PC7=%u\r\n",
                             (unsigned)fpga_meter_probe_tail_override,
                             gpio_level(GPIOC, 7));
        }
        return;
    }

    if (strcmp(args, "auto") == 0) {
        fpga_meter_probe_tail_override = -1;
        usb_send_str("meter probe-tail=auto\r\n");
    } else if (strcmp(args, "07") == 0 || strcmp(args, "7") == 0) {
        fpga_meter_probe_tail_override = 0x07;
        usb_send_str("meter probe-tail=07\r\n");
    } else if (strcmp(args, "0a") == 0 || strcmp(args, "0A") == 0 ||
               strcmp(args, "10") == 0) {
        fpga_meter_probe_tail_override = FPGA_CMD_METER_NOPROBE;
        usb_send_str("meter probe-tail=0A\r\n");
    } else {
        usb_send_str("Usage: meter probe-tail [auto|07|0a]\r\n");
    }
}

static void print_meter_mux_stream_line(uint32_t index)
{
    meter_reading_t snap;
    bool have_snap = meter_data_snapshot(&snap);
    bool live = current_mode == MODE_MULTIMETER &&
                have_snap &&
                snap.valid &&
                snap.submode == meter_submode;
    fpga_meter_selector_t selectors = fpga_meter_expected_selectors(meter_submode);
    fpga_meter_transition_plan_t plan =
        fpga_meter_transition_plan_for_submode(meter_submode);

    usb_debug_printf("t=%lu upd=%lu ui_sub=%u rd_sub=%u live=%u cls=%u "
                     "stock_mode=%u raw_low=%02X family=%u mux=%u "
                     "portc_porte=%u porta_portb=%u settle=%u ",
                     index,
                     have_snap ? snap.update_count : 0UL,
                     (unsigned)meter_submode,
                     have_snap ? (unsigned)snap.submode : 0U,
                     live ? 1U : 0U,
                     have_snap ? (unsigned)snap.result_class : 0U,
                     (unsigned)selectors.function_selector,
                     (unsigned)selectors.range_selector,
                     (unsigned)plan.frame_family,
                     (unsigned)plan.mux_index,
                     (unsigned)plan.portc_porte_mux,
                     (unsigned)plan.porta_portb_mux,
                     (unsigned)plan.settle_ms);
    usb_debug_printf("obs_family=%u reject=%u seq=%u seq_sub=%u "
                     "seq_word=%04X seq_apply=%04X tx=%u data=%u echo=%u ",
                     have_snap ? (unsigned)snap.observed_frame_family : 0U,
                     have_snap ? (unsigned)snap.reject_reason : 0U,
                     (unsigned)fpga.meter_mode_sequence_count,
                     (unsigned)fpga.meter_mode_sequence_submode,
                     (unsigned)fpga.meter_mode_selector_word,
                     (unsigned)fpga.meter_mode_apply_word,
                     fpga.tx_count,
                     fpga.frame_count,
                     fpga.echo_count);
    usb_debug_printf("disp=%s unit=%s raw=%d dp=%u f6=%02X f7=%02X "
                     "f8=%02X f9=%02X extra=%04X discard=%u ",
                     (have_snap && snap.valid) ? snap.display_str : "---",
                     (have_snap && snap.valid && snap.unit_suffix) ? snap.unit_suffix : "",
                     have_snap ? snap.bcd_value : 0,
                     have_snap ? (unsigned)snap.decimal_pos : 0U,
                     have_snap ? (unsigned)snap.dbg_frame[6] : 0U,
                     have_snap ? (unsigned)snap.dbg_frame[7] : 0U,
                     have_snap ? (unsigned)snap.dbg_frame[8] : 0U,
                     have_snap ? (unsigned)snap.dbg_frame[9] : 0U,
                     (unsigned)meter_dbg_extra(),
                     (unsigned)meter_frame_discard_count);
    usb_debug_printf("PC6=%u PB11=%u PC11=%u PC12=%u PE4=%u PE5=%u PE6=%u "
                     "PA15=%u PA10=%u PB10=%u PB9=%u PA6=%u frame=",
                     gpio_level(GPIOC, 6),
                     gpio_level(GPIOB, 11),
                     gpio_level(GPIOC, 11),
                     gpio_level(GPIOC, 12),
                     gpio_level(GPIOE, 4),
                     gpio_level(GPIOE, 5),
                     gpio_level(GPIOE, 6),
                     gpio_level(GPIOA, 15),
                     gpio_level(GPIOA, 10),
                     gpio_level(GPIOB, 10),
                     gpio_level(GPIOB, 9),
                     gpio_level(GPIOA, 6));
    if (have_snap) {
        print_volatile_frame_inline(snap.dbg_frame);
    }
    usb_send_str("\r\n");
}

static void cmd_meter_mux_stream(const char *args)
{
    uint32_t count = 16;
    uint32_t delay_ms = 250;
    uint32_t last_update = 0xFFFFFFFFu;
    const char *usage = "Usage: meter mux-stream [count<=200] [delay_ms<=5000]\r\n";

    if (parse_stream_args(args, &count, &delay_ms, usage) != 0) return;

    usb_debug_printf("mux-stream count=%lu delay_ms=%lu\r\n", count, delay_ms);
    for (uint32_t i = 0; i < count; i++) {
        if (meter_reading.update_count != last_update) {
            last_update = meter_reading.update_count;
            print_meter_mux_stream_line(i);
        }
        if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void cmd_meter_mux_arms(const char *args)
{
    uint32_t portc_porte = 0;
    uint32_t porta_portb = 0;
    uint32_t settle_ms = 300;
    uint16_t planned_gpio = 0;
    uint16_t actual_gpio = 0;
    const char *usage =
        "Usage: meter mux-arms <portc_porte 0-9> <porta_portb 0-9> [settle_ms<=5000]\r\n";

    if (parse_mux_arm_args(args, &portc_porte, &porta_portb,
                           &settle_ms, usage) != 0) {
        return;
    }
    if (current_mode != MODE_MULTIMETER) {
        usb_send_str("ERR: switch to meter mode before applying DMM mux arms\r\n");
        return;
    }
    if (!fpga_debug_apply_meter_mux_arms((uint8_t)portc_porte,
                                         (uint8_t)porta_portb,
                                         &planned_gpio,
                                         &actual_gpio)) {
        usb_send_str(usage);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(settle_ms));
    (void)fpga_send_cmd(0x05, FPGA_CMD_METER_START);
    vTaskDelay(pdMS_TO_TICKS(350));

    usb_send_str("=== DMM Mux Arms Trace ===\r\n");
    usb_debug_printf("mux_arms portc_porte=%lu porta_portb=%lu settle_ms=%lu "
                     "planned_gpio=%03X actual_gpio=%03X\r\n",
                     portc_porte,
                     porta_portb,
                     settle_ms,
                     (unsigned)planned_gpio,
                     (unsigned)actual_gpio);
    cmd_meter_trace();
}

static void cmd_meter_boot_sequence(const char *args)
{
    uint32_t settle_ms = 300;
    uint16_t planned_gpio = 0;
    uint16_t actual_gpio = 0;
    const char *usage = "Usage: meter boot-sequence [settle_ms<=5000]\r\n";

    if (args != NULL && *args != '\0') {
        if (parse_int(args, &settle_ms) != 0 || settle_ms > 5000U) {
            usb_send_str(usage);
            return;
        }
    }
    if (current_mode != MODE_MULTIMETER) {
        usb_send_str("ERR: switch to meter mode before replaying DMM boot order\r\n");
        return;
    }
    if (!fpga_debug_send_meter_boot_order(meter_submode,
                                          settle_ms,
                                          &planned_gpio,
                                          &actual_gpio)) {
        usb_send_str(usage);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(settle_ms));
    (void)fpga_send_cmd(0x05, FPGA_CMD_METER_START);
    vTaskDelay(pdMS_TO_TICKS(350));

    usb_send_str("=== DMM Boot-Order Trace ===\r\n");
    usb_debug_printf("boot_sequence submode=%u (%s) settle_ms=%lu "
                     "order=0508,0509,probe,selector planned_gpio=%03X actual_gpio=%03X\r\n",
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     settle_ms,
                     (unsigned)planned_gpio,
                     (unsigned)actual_gpio);
    cmd_meter_trace();
}

static void cmd_meter_pc11_timing(const char *args)
{
    uint32_t low_ms = 250;
    uint32_t high_ms = 250;
    uint16_t planned_gpio = 0;
    uint16_t actual_gpio = 0;
    const char *usage = "Usage: meter pc11-timing [low_ms<=5000] [high_ms<=5000]\r\n";

    if (args != NULL && *args != '\0') {
        char buf[48];
        char *saveptr = NULL;
        char *tok;

        strncpy(buf, args, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        tok = strtok_r(buf, " \t", &saveptr);
        if (tok != NULL &&
            (parse_int(tok, &low_ms) != 0 || low_ms > 5000U)) {
            usb_send_str(usage);
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok != NULL &&
            (parse_int(tok, &high_ms) != 0 || high_ms > 5000U)) {
            usb_send_str(usage);
            return;
        }
        if (strtok_r(NULL, " \t", &saveptr) != NULL) {
            usb_send_str(usage);
            return;
        }
    }
    if (current_mode != MODE_MULTIMETER) {
        usb_send_str("ERR: switch to meter mode before probing DMM PC11 timing\r\n");
        return;
    }
    if (!fpga_debug_send_meter_pc11_timing(meter_submode,
                                           low_ms,
                                           high_ms,
                                           &planned_gpio,
                                           &actual_gpio)) {
        usb_send_str(usage);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(high_ms));
    (void)fpga_send_cmd(0x05, FPGA_CMD_METER_START);
    vTaskDelay(pdMS_TO_TICKS(350));

    usb_send_str("=== DMM PC11 Timing Trace ===\r\n");
    usb_debug_printf("pc11_timing submode=%u (%s) low_ms=%lu high_ms=%lu "
                     "planned_gpio=%03X actual_gpio=%03X\r\n",
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     low_ms,
                     high_ms,
                     (unsigned)planned_gpio,
                     (unsigned)actual_gpio);
    cmd_meter_trace();
}

static void cmd_meter_stream(const char *args)
{
    uint32_t count = 16;
    uint32_t delay_ms = 250;
    uint32_t last_update = 0xFFFFFFFFu;
    const char *usage = "Usage: meter stream [count<=200] [delay_ms<=5000]\r\n";

    if (parse_stream_args(args, &count, &delay_ms, usage) != 0) return;

    usb_debug_printf("stream count=%lu delay_ms=%lu\r\n", count, delay_ms);
    for (uint32_t i = 0; i < count; i++) {
        meter_reading_t snap;

        if (meter_data_snapshot(&snap) &&
            snap.update_count != last_update) {
            last_update = snap.update_count;
            uint16_t extra = ((uint16_t)snap.dbg_frame[10] << 8) |
                             snap.dbg_frame[11];
            usb_debug_printf("%lu upd=%lu sub=%u cls=%u family=%u/%u reject=%u "
                             "raw=%d dp=%u unit=%s disp=%s "
                             "f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%ld wave=%lu beep=%u\r\n",
                             i,
                             snap.update_count,
                             (unsigned)snap.submode,
                             (unsigned)snap.result_class,
                             (unsigned)snap.expected_frame_family,
                             (unsigned)snap.observed_frame_family,
                             (unsigned)snap.reject_reason,
                             snap.bcd_value,
                             (unsigned)snap.decimal_pos,
                             snap.unit_suffix ? snap.unit_suffix : "",
                             snap.display_str,
                             (unsigned)snap.dbg_frame[6],
                             (unsigned)snap.dbg_frame[7],
                             (unsigned)snap.dbg_frame[8],
                             (unsigned)snap.dbg_frame[9],
                             (unsigned)extra,
                             (long)scaled_i100(snap.aux_freq_hz) / 10L,
                             meter_voltage_wave_sample_count(),
                             snap.continuity_beep ? 1U : 0U);
        }
        if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void cmd_meter_adc_snapshot(void)
{
    meter_voltage_wave_snapshot(&usb_meter_wave_snap, METER_VOLTAGE_WAVE_RENDER_POINTS,
                                meter_reading.aux_freq_hz);

    usb_send_str("=== DMM ADC Snapshot ===\r\n");
    usb_debug_printf("mode=%lu meter_submode=%u (%s) wave_samples=%lu\r\n",
                     (uint32_t)current_mode,
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     meter_voltage_wave_sample_count());
    usb_debug_printf("sampler=%s spi_path=%s selector_override=%d last_preacq=%02X last_selector=%02X "
                     "preacq_override=%d last_preacq_rx=%02X last_sample=%02X min_sample=%02X max_sample=%02X samples=%lu ff_samples=%lu "
                     "zero_samples=%lu enq_attempts=%lu enq_success=%lu enq_drops=%lu\r\n",
                     fpga_meter_adc_sampler_enabled ? "on" : "off",
                     fpga_meter_adc_use_preacq ? "preacq" : "direct",
                     (int)fpga_meter_adc_selector_override,
                     (unsigned)fpga_meter_adc_last_preacq,
                     (unsigned)fpga_meter_adc_last_selector,
                     (int)fpga_meter_adc_preacq_override,
                     (unsigned)fpga_meter_adc_last_preacq_rx,
                     (unsigned)fpga_meter_adc_last_sample,
                     (unsigned)fpga_meter_adc_min_sample,
                     (unsigned)fpga_meter_adc_max_sample,
                     fpga_meter_adc_samples,
                     fpga_meter_adc_ff_samples,
                     fpga_meter_adc_zero_samples,
                     fpga_meter_adc_enqueue_attempts,
                     fpga_meter_adc_enqueue_success,
                     fpga_meter_adc_enqueue_drops);
    usb_debug_printf("adc_diag trans=%lu not_v=%lu gen=%lu last_gen=%lu first=%02X busy=%u discard=%u probing=%u to=%lu total_to=%lu ch=%u\r\n",
                     fpga_meter_adc_transition_skips,
                     fpga_meter_adc_not_voltage_skips,
                     fpga_meter_adc_reset_generation,
                     fpga_meter_adc_last_reset_generation,
                     (unsigned)fpga_meter_adc_first_sample_after_reset,
                     fpga_meter_transition_busy() ? 1U : 0U,
                     (unsigned)meter_frame_discard_count,
                     fpga.spi3_probing ? 1U : 0U,
                     (unsigned long)fpga.spi3_timeout_count,
                     (unsigned long)fpga.spi3_total_timeouts,
                     (unsigned)active_channel);
    usb_debug_printf("snapshot_count=%u raw_last=%u raw_min=%u raw_max=%u p2p=%u synced=%u\r\n",
                     (unsigned)usb_meter_wave_snap.count,
                     (unsigned)usb_meter_wave_snap.raw_last,
                     (unsigned)usb_meter_wave_snap.raw_min,
                     (unsigned)usb_meter_wave_snap.raw_max,
                     (unsigned)usb_meter_wave_snap.peak_to_peak_raw,
                     usb_meter_wave_snap.synced ? 1U : 0U);
    print_i100("mean_raw=", usb_meter_wave_snap.mean_raw, "");
    print_i100("rms_raw=", usb_meter_wave_snap.rms_raw, "");
    print_i100("freq=", usb_meter_wave_snap.freq_hz, " Hz");
    usb_debug_printf("dmm_display=%s dmm_unit=%s dmm_class=%u dmm_updates=%lu dmm_valid=%u\r\n",
                     meter_reading.valid ? meter_reading.display_str : "---",
                     (meter_reading.valid && meter_reading.unit_suffix) ? meter_reading.unit_suffix : "",
                     (unsigned)meter_reading.result_class,
                     meter_reading.update_count,
                     meter_reading.valid ? 1U : 0U);
}

static void cmd_ui_dump(void)
{
    meter_autoselect_status_t auto_st;
    meter_autoselect_get_status(&auto_st);

    usb_debug_printf("mode=%lu startup=%s meter_submode=%u (%s) layout=%u (%s) "
                     "settings_depth=%d selected=%d sub_selected=%d\r\n",
                     (uint32_t)current_mode,
                     startup_mode_name(startup_mode),
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     (unsigned)meter_layout,
                     meter_layout_name(meter_layout),
                     (int)settings_depth,
                     (int)settings_selected,
                     (int)settings_sub_selected);
    usb_debug_printf("meter_ui draws=%lu full_clears=%lu partial_clears=%lu draw_us=%lu max_draw_us=%lu over_budget=%lu rendered_update=%lu last_full=%u live=%u continuity_flash=%u reading_valid=%u "
                     "reading_submode=%u updates=%lu display_updates=%lu\r\n",
                     meter_screen_draw_count,
                     meter_screen_full_clear_count,
                     meter_screen_partial_clear_count,
                     meter_screen_last_draw_us,
                     meter_screen_max_draw_us,
                     meter_screen_over_budget_count,
                     meter_screen_last_reading_display_update,
                     (unsigned)meter_screen_last_full_clear,
                     (unsigned)meter_screen_last_live,
                     (unsigned)meter_screen_last_continuity_flash,
                     meter_reading.valid ? 1U : 0U,
                     (unsigned)meter_reading.submode,
                     meter_reading.update_count,
                     meter_reading.display_update_count);
    usb_debug_printf("autoselect state=%s current=%u (%s) index=%u/%u best=%u (%s) best_score=%u last_score=%u cancel=%u\r\n",
                     meter_autoselect_state_name(auto_st.state),
                     (unsigned)auto_st.current_submode,
                     meter_submode_name(auto_st.current_submode),
                     (unsigned)auto_st.current_index,
                     (unsigned)auto_st.candidate_count,
                     (unsigned)auto_st.best_submode,
                     meter_submode_name(auto_st.best_submode),
                     (unsigned)auto_st.best_score,
                     (unsigned)auto_st.last_score,
                     auto_st.cancel_pending ? 1U : 0U);
}

static void cmd_meter_wave(void)
{
    extern volatile device_mode_t current_mode;
    extern volatile uint8_t meter_submode;

    uint32_t before = meter_voltage_wave_sample_count();
    vTaskDelay(pdMS_TO_TICKS(250));
    uint32_t after = meter_voltage_wave_sample_count();

    meter_voltage_wave_snapshot(&usb_meter_wave_snap, METER_VOLTAGE_WAVE_RENDER_POINTS, 0.0f);

    usb_send_str("=== SPI3 Meter ADC Probe ===\r\n");
    usb_debug_printf("mode=%lu meter_submode=%u (%s)\r\n",
                     (uint32_t)current_mode,
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode));
    usb_debug_printf("samples_total=%lu delta_250ms=%lu approx_rate=%lu Hz\r\n",
                     after, after - before, (after - before) * 4U);
    usb_debug_printf("sampler=%s spi_path=%s enq=%lu ok=%lu drop=%lu samples=%lu ff=%lu zero=%lu "
                     "selector_mode=%d preacq_mode=%d last_pre=%02X pre_rx=%02X selector=%02X last=%02X min=%02X max=%02X\r\n",
                     fpga_meter_adc_sampler_enabled ? "on" : "off",
                     fpga_meter_adc_use_preacq ? "preacq" : "direct",
                     fpga_meter_adc_enqueue_attempts,
                     fpga_meter_adc_enqueue_success,
                     fpga_meter_adc_enqueue_drops,
                     fpga_meter_adc_samples,
                     fpga_meter_adc_ff_samples,
                     fpga_meter_adc_zero_samples,
                     (int)fpga_meter_adc_selector_override,
                     (int)fpga_meter_adc_preacq_override,
                     (unsigned)fpga_meter_adc_last_preacq,
                     (unsigned)fpga_meter_adc_last_preacq_rx,
                     (unsigned)fpga_meter_adc_last_selector,
                     (unsigned)fpga_meter_adc_last_sample,
                     (unsigned)fpga_meter_adc_min_sample,
                     (unsigned)fpga_meter_adc_max_sample);
    usb_debug_printf("adc_diag trans=%lu not_v=%lu gen=%lu last_gen=%lu first=%02X busy=%u discard=%u probing=%u to=%lu total_to=%lu ch=%u\r\n",
                     fpga_meter_adc_transition_skips,
                     fpga_meter_adc_not_voltage_skips,
                     fpga_meter_adc_reset_generation,
                     fpga_meter_adc_last_reset_generation,
                     (unsigned)fpga_meter_adc_first_sample_after_reset,
                     fpga_meter_transition_busy() ? 1U : 0U,
                     (unsigned)meter_frame_discard_count,
                     fpga.spi3_probing ? 1U : 0U,
                     (unsigned long)fpga.spi3_timeout_count,
                     (unsigned long)fpga.spi3_total_timeouts,
                     (unsigned)active_channel);
    usb_debug_printf("snapshot_count=%u raw_last=%u raw_min=%u raw_max=%u p2p=%u synced=%u\r\n",
                     (unsigned)usb_meter_wave_snap.count,
                     (unsigned)usb_meter_wave_snap.raw_last,
                     (unsigned)usb_meter_wave_snap.raw_min,
                     (unsigned)usb_meter_wave_snap.raw_max,
                     (unsigned)usb_meter_wave_snap.peak_to_peak_raw,
                     usb_meter_wave_snap.synced ? 1U : 0U);
    print_i100("mean_raw=", usb_meter_wave_snap.mean_raw, "");
    print_i100("rms_raw=", usb_meter_wave_snap.rms_raw, "");
    print_i100("freq=", usb_meter_wave_snap.freq_hz, " Hz");
    usb_debug_printf("dmm=%s %s class=%u updates=%lu valid=%u\r\n",
                     meter_reading.display_str,
                     meter_reading.unit_suffix ? meter_reading.unit_suffix : "",
                     (unsigned)meter_reading.result_class,
                     meter_reading.update_count,
                     meter_reading.valid ? 1U : 0U);
    usb_send_str("Experimental SPI3 meter-ADC probe; all-FF means this path is not armed/valid, not a DMM waveform.\r\n");
}

static void cmd_meter_wave_args(const char *args)
{
    if (args == NULL || *args == '\0') {
        cmd_meter_wave();
        return;
    }

    if (strcmp(args, "reset") == 0) {
        meter_voltage_wave_reset();
        fpga_meter_adc_diag_reset();
        usb_send_str("meter wave diagnostics reset\r\n");
        return;
    }

    if (strncmp(args, "sampler", 7) == 0 &&
        (args[7] == '\0' || args[7] == ' ' || args[7] == '\t')) {
        const char *value = args + 7;
        while (*value == ' ' || *value == '\t') value++;
        if (*value == '\0') {
            usb_debug_printf("meter wave sampler=%s\r\n",
                             fpga_meter_adc_sampler_enabled ? "on" : "off");
        } else if (strcmp(value, "on") == 0) {
            fpga_meter_adc_sampler_enabled = true;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave sampler=on\r\n");
        } else if (strcmp(value, "off") == 0) {
            fpga_meter_adc_sampler_enabled = false;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave sampler=off\r\n");
        } else {
            usb_send_str("Usage: meter wave sampler [on|off]\r\n");
        }
        return;
    }

    if (strncmp(args, "path", 4) == 0 &&
        (args[4] == '\0' || args[4] == ' ' || args[4] == '\t')) {
        const char *value = args + 4;
        while (*value == ' ' || *value == '\t') value++;

        if (*value == '\0') {
            usb_debug_printf("meter wave path=%s\r\n",
                             fpga_meter_adc_use_preacq ? "preacq" : "direct");
        } else if (strcmp(value, "direct") == 0) {
            fpga_meter_adc_use_preacq = false;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave path=direct\r\n");
        } else if (strcmp(value, "preacq") == 0) {
            fpga_meter_adc_use_preacq = true;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave path=preacq\r\n");
        } else {
            usb_send_str("Usage: meter wave path [direct|preacq]\r\n");
        }
        return;
    }

    if (strncmp(args, "selector", 8) == 0 &&
        (args[8] == '\0' || args[8] == ' ' || args[8] == '\t')) {
        const char *value = args + 8;
        uint32_t selector;
        while (*value == ' ' || *value == '\t') value++;

        if (*value == '\0') {
            usb_debug_printf("meter wave selector=%d\r\n",
                             (int)fpga_meter_adc_selector_override);
        } else if (strcmp(value, "auto") == 0) {
            fpga_meter_adc_selector_override = -1;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave selector=auto\r\n");
        } else if (parse_int(value, &selector) == 0 && selector <= 255U) {
            fpga_meter_adc_selector_override = (int16_t)selector;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_debug_printf("meter wave selector=%d\r\n",
                             (int)fpga_meter_adc_selector_override);
        } else {
            usb_send_str("Usage: meter wave selector [auto|0..255]\r\n");
        }
        return;
    }

    if (strncmp(args, "preacq", 6) == 0 &&
        (args[6] == '\0' || args[6] == ' ' || args[6] == '\t')) {
        const char *value = args + 6;
        uint32_t preacq;
        while (*value == ' ' || *value == '\t') value++;

        if (*value == '\0') {
            usb_debug_printf("meter wave preacq=%d\r\n",
                             (int)fpga_meter_adc_preacq_override);
        } else if (strcmp(value, "auto") == 0) {
            fpga_meter_adc_preacq_override = -1;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave preacq=auto\r\n");
        } else if (parse_int(value, &preacq) == 0 && preacq <= 255U) {
            fpga_meter_adc_preacq_override = (int16_t)preacq;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_debug_printf("meter wave preacq=%d\r\n",
                             (int)fpga_meter_adc_preacq_override);
        } else {
            usb_send_str("Usage: meter wave preacq [auto|0..255]\r\n");
        }
        return;
    }

    usb_send_str("Usage: meter wave [reset|sampler|path|selector|preacq]\r\n");
}

static void cmd_uptime(void)
{
    extern volatile uint32_t uptime_seconds;
    uint32_t s = uptime_seconds;
    usb_debug_printf("Uptime: %lu:%02lu:%02lu\r\n", s / 3600, (s % 3600) / 60, s % 60);
}

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Acquisition Test — Decomposer Phase 20 Validation
 *
 * Tests whether SPI3 returns non-0xFF data after the H2 cal upload.
 * The decomposer identified Phase 20 as a 40-exchange SPI3 session
 * that constitutes the acquisition interface. This command tries
 * multiple approaches to coax data from the FPGA:
 *   Test 1: Raw SPI3 read (just clock bytes out)
 *   Test 2: SPI3 command byte + read (stock acq pattern)
 *   Test 3: USART2 scope-arm commands, then SPI3 read
 *   Test 4: Full stock-like scope entry sequence, then SPI3 read
 *   Test 5: DAC1 check (Phase 17: DMA2 Ch4 → DAC for offset comp)
 * ═══════════════════════════════════════════════════════════════════ */

/* Inline SPI3 exchange using raw registers (usb_debug.c can't see static spi3_xfer) */
static uint8_t spi3_raw_xfer(uint8_t tx)
{
    volatile uint32_t *sts = (volatile uint32_t *)0x40003C08;  /* SPI3_STS */
    volatile uint32_t *dt  = (volatile uint32_t *)0x40003C0C;  /* SPI3_DT */
    uint32_t timeout;

    timeout = 100000;
    while (!(*sts & 0x02) && --timeout);  /* Wait TXE */
    if (timeout == 0) return 0xEE;  /* Distinguish timeout from FPGA 0xFF */
    *dt = tx;

    timeout = 100000;
    while (!(*sts & 0x01) && --timeout);  /* Wait RXNE */
    if (timeout == 0) return 0xEE;
    return (uint8_t)*dt;
}

/* Claim the SPI3 bus for a raw shell command (audit 2026-08-20, P0.1).
 * The acquisition task runs at priority 3, ABOVE this shell task at 2, so
 * without a park it preempts any spi3_raw_xfer sequence mid-CS-window and
 * both transfers interleave into garbage — a live source of bad experimental
 * data before 2026-08-20. Used at the DISPATCH level around every command
 * that drives the bus raw; pair with fpga_acq_resume() after the command
 * returns (dispatch-level pairing survives the commands' early returns). */
static bool spi3_shell_claim(void)
{
    if (!fpga_acq_pause()) {
        usb_send_str("ERR: SPI3 busy — acquisition task would not park\r\n");
        return false;
    }
    return true;
}

static void cmd_spi3_acqtest(void)
{
    usb_send_str("=== SPI3 Acquisition Path Test (Decomposer Phase 20) ===\r\n\r\n");

    /* --- State report --- */
    usb_send_str("-- Current state --\r\n");
    usb_debug_printf("PC0  (data-ready): %d\r\n", (GPIOC->idt & (1 << 0)) ? 1 : 0);
    usb_debug_printf("PC6  (SPI enable): %d\r\n", (GPIOC->idt & (1 << 6)) ? 1 : 0);
    usb_debug_printf("PB11 (active):     %d\r\n", (GPIOB->idt & (1 << 11)) ? 1 : 0);
    usb_debug_printf("PB6  (CS idle):    %d\r\n", (GPIOB->idt & (1 << 6)) ? 1 : 0);
    usb_debug_printf("SPI3 CTRL1: 0x%08lX\r\n", *(volatile uint32_t *)0x40003C00);
    usb_debug_printf("SPI3 CTRL2: 0x%08lX\r\n", *(volatile uint32_t *)0x40003C04);
    usb_debug_printf("SPI3 STS:   0x%08lX\r\n", *(volatile uint32_t *)0x40003C08);
    usb_debug_printf("H2 tx:   %d  bytes: %lu (no ACK proof)\r\n",
                     fpga.h2_upload_done, fpga.h2_bytes_sent);
    usb_debug_printf("post-H2 SPI3 boot: enq=%u run=%u drop=%u mask=0x%02X\r\n",
                     fpga.post_h2_spi3_boot_enqueued,
                     fpga.post_h2_spi3_boot_run_count,
                     fpga.post_h2_spi3_boot_dropped,
                     fpga.post_h2_spi3_boot_mask);

    /* --- Test 1: Raw read with CS LOW (16 bytes) --- */
    usb_send_str("\r\n-- T1: Raw SPI3 read (CS low, 16x 0xFF) --\r\n");
    GPIOB->clr = (1 << 6);  /* CS assert */
    for (volatile int d = 0; d < 200; d++);
    int t1_nonff = 0;
    for (int i = 0; i < 16; i++) {
        uint8_t rx = spi3_raw_xfer(0xFF);
        usb_debug_printf("%02X ", rx);
        if (rx != 0xFF && rx != 0xEE) t1_nonff++;
    }
    GPIOB->scr = (1 << 6);  /* CS deassert */
    usb_debug_printf(" [%d non-FF]\r\n", t1_nonff);

    /* --- Test 2: Send acq commands then read (stock FPGA task pattern) --- */
    usb_send_str("\r\n-- T2: SPI3 cmd 0x80 → read 16 --\r\n");
    GPIOB->clr = (1 << 6);
    spi3_raw_xfer(0x80);  /* Scope acquire: mode 0, range 0 */
    GPIOB->scr = (1 << 6);
    spi3_raw_xfer(0x00);  /* Flush with CS high (stock pattern) */

    /* Small delay then read data */
    for (volatile int d = 0; d < 10000; d++);

    GPIOB->clr = (1 << 6);
    uint8_t echo = spi3_raw_xfer(0xFF);
    usb_debug_printf("echo=%02X data:", echo);
    int t2_nonff = 0;
    for (int i = 0; i < 15; i++) {
        uint8_t rx = spi3_raw_xfer(0xFF);
        usb_debug_printf(" %02X", rx);
        if (rx != 0xFF && rx != 0xEE) t2_nonff++;
    }
    GPIOB->scr = (1 << 6);
    usb_debug_printf(" [%d non-FF]\r\n", t2_nonff);

    /* --- Test 3: USART2 scope-arm then SPI3 read --- */
    usb_send_str("\r\n-- T3: USART2 arm (0x20,0x21) → SPI3 read --\r\n");
    fpga_send_cmd(0x00, 0x20);  /* Scope timebase cmd */
    vTaskDelay(pdMS_TO_TICKS(30));
    fpga_send_cmd(0x00, 0x21);  /* Scope trigger mode cmd */
    vTaskDelay(pdMS_TO_TICKS(30));

    usb_debug_printf("PC0 after arm: %d\r\n", (GPIOC->idt & (1 << 0)) ? 1 : 0);

    GPIOB->clr = (1 << 6);
    int t3_nonff = 0;
    for (int i = 0; i < 16; i++) {
        uint8_t rx = spi3_raw_xfer(0xFF);
        if (i < 16) usb_debug_printf("%02X ", rx);
        if (rx != 0xFF && rx != 0xEE) t3_nonff++;
    }
    GPIOB->scr = (1 << 6);
    usb_debug_printf(" [%d non-FF]\r\n", t3_nonff);

    /* --- Test 4: Full stock scope entry (0x01..0x08, 0x0B..0x11, 0x20, 0x21) --- */
    usb_send_str("\r\n-- T4: Full scope entry → SPI3 read --\r\n");
    /* Reset sequence */
    fpga_send_cmd(0x00, 0x01);  vTaskDelay(pdMS_TO_TICKS(10));
    fpga_send_cmd(0x00, 0x02);  vTaskDelay(pdMS_TO_TICKS(10));
    fpga_send_cmd(0x00, 0x03);  vTaskDelay(pdMS_TO_TICKS(10));
    fpga_send_cmd(0x00, 0x0B);  vTaskDelay(pdMS_TO_TICKS(10));  /* CH1 gain */
    fpga_send_cmd(0x00, 0x0C);  vTaskDelay(pdMS_TO_TICKS(10));  /* CH1 offset */
    fpga_send_cmd(0x00, 0x0D);  vTaskDelay(pdMS_TO_TICKS(10));  /* CH2 gain */
    fpga_send_cmd(0x00, 0x0E);  vTaskDelay(pdMS_TO_TICKS(10));  /* CH2 offset */
    fpga_send_cmd(0x00, 0x0F);  vTaskDelay(pdMS_TO_TICKS(10));  /* Coupling */
    fpga_send_cmd(0x00, 0x10);  vTaskDelay(pdMS_TO_TICKS(10));  /* Trigger */
    fpga_send_cmd(0x00, 0x11);  vTaskDelay(pdMS_TO_TICKS(10));  /* Timebase */
    fpga_send_cmd(0x00, 0x20);  vTaskDelay(pdMS_TO_TICKS(10));  /* Acq mode */
    fpga_send_cmd(0x00, 0x21);  vTaskDelay(pdMS_TO_TICKS(50));  /* Trigger arm */

    usb_debug_printf("PC0 after full entry: %d\r\n", (GPIOC->idt & (1 << 0)) ? 1 : 0);

    /* Try SPI3 with scope command byte */
    GPIOB->clr = (1 << 6);
    spi3_raw_xfer(0x80);  /* Scope acq cmd */
    GPIOB->scr = (1 << 6);
    spi3_raw_xfer(0x00);  /* Flush */
    for (volatile int d = 0; d < 50000; d++);  /* Wait for FPGA to fill buffer */

    GPIOB->clr = (1 << 6);
    uint8_t first = spi3_raw_xfer(0xFF);
    int t4_nonff = (first != 0xFF && first != 0xEE) ? 1 : 0;
    usb_debug_printf("first=%02X data:", first);
    for (int i = 0; i < 31; i++) {
        uint8_t rx = spi3_raw_xfer(0xFF);
        if (i < 15) usb_debug_printf(" %02X", rx);
        if (rx != 0xFF && rx != 0xEE) t4_nonff++;
    }
    GPIOB->scr = (1 << 6);
    usb_debug_printf("... [%d/32 non-FF]\r\n", t4_nonff);

    /* --- Test 5: DAC1 state (Phase 17: DMA2 Ch4 → DAC for analog offset) --- */
    usb_send_str("\r\n-- T5: DAC/DMA2 state (Phase 17 validation) --\r\n");
    volatile uint32_t *dac_d1dth12r = (volatile uint32_t *)0x40007408;  /* DAC1 12-bit right-aligned */
    volatile uint32_t *dac_d1dth12l = (volatile uint32_t *)0x4000740C;
    volatile uint32_t *dac_ctrl     = (volatile uint32_t *)0x40007400;  /* DAC_CR */
    volatile uint32_t *dma2_sts     = (volatile uint32_t *)0x40020400;  /* DMA2_STS */
    volatile uint32_t *dma2_c4ctrl  = (volatile uint32_t *)0x40020444;  /* DMA2_C4CTRL */
    volatile uint32_t *dma2_srcsel0 = (volatile uint32_t *)0x400204A0;
    volatile uint32_t *dma2_srcsel1 = (volatile uint32_t *)0x400204A4;

    usb_debug_printf("DAC_CR:       0x%08lX\r\n", *dac_ctrl);
    usb_debug_printf("DAC1_D12R:    0x%08lX\r\n", *dac_d1dth12r);
    usb_debug_printf("DAC1_D12L:    0x%08lX\r\n", *dac_d1dth12l);
    usb_debug_printf("DMA2_STS:     0x%08lX\r\n", *dma2_sts);
    usb_debug_printf("DMA2_C4CTRL:  0x%08lX\r\n", *dma2_c4ctrl);
    usb_debug_printf("DMA2_SRCSEL0: 0x%08lX\r\n", *dma2_srcsel0);
    usb_debug_printf("DMA2_SRCSEL1: 0x%08lX\r\n", *dma2_srcsel1);

    usb_send_str("\r\n=== Done. Non-FF in any test = FPGA responding on SPI3 ===\r\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * H2 TX Replay Diagnostic — sample MISO without claiming acceptance
 *
 * Re-sends the 115,638-byte H2 table while sampling the FPGA's simultaneous
 * MISO bytes at key points. Stock-visible evidence proves the TX stream and
 * table layout only; no recovered ACK/apply condition ties this diagnostic to
 * FPGA acceptance or DMM physical calibration.
 * ═══════════════════════════════════════════════════════════════════ */
static void cmd_spi3_h2txdiag(void)
{
    usb_send_str("=== H2 TX Replay Diagnostic ===\r\n");
    usb_send_str("Samples MISO only; no recovered ACK/apply proof.\r\n\r\n");

    /* Pre-upload state */
    usb_debug_printf("PC0 before: %d\r\n", (GPIOC->idt & 1) ? 1 : 0);
    usb_debug_printf("MISO idle:  %d\r\n", (GPIOB->idt & (1 << 4)) ? 1 : 0);

    /* Sampling plan: capture response bytes at strategic points */
    uint8_t resp_start[2];      /* 0x3B opcode + flush */
    uint8_t resp_first16[16];   /* First 16 data bytes */
    uint8_t resp_blk1[4];       /* Block 1 boundary (bytes 160-163) */
    uint8_t resp_blk2[4];       /* Block 2 boundary (bytes 320-323) */
    uint8_t resp_sentinel[8];   /* Around first sentinel (bytes 26-33) */
    uint8_t resp_regB[4];       /* Region B start (byte 87040-87043) */
    uint8_t resp_last16[16];    /* Last 16 data bytes */
    uint8_t resp_end[2];        /* 0x3A opcode + flush */
    uint8_t resp_post[16];      /* Post-upload reads */
    int total_nonff = 0;

    /* --- Do the upload --- */
    usb_send_str("Uploading 115,638 bytes...\r\n");

    GPIOB->clr = (1 << 6);  /* CS assert */

    /* Start opcode */
    resp_start[0] = spi3_raw_xfer(0x3B);
    resp_start[1] = spi3_raw_xfer(0x00);

    /* Stream entire table, sampling at key points */
    for (uint32_t i = 0; i < FPGA_H2_CAL_TABLE_SIZE; i++) {
        uint8_t rx = spi3_raw_xfer(fpga_h2_cal_table[i]);

        if (rx != 0xFF && rx != 0xEE) total_nonff++;

        /* Capture at strategic positions */
        if (i < 16) resp_first16[i] = rx;
        if (i >= 26 && i < 34) resp_sentinel[i - 26] = rx;
        if (i >= 160 && i < 164) resp_blk1[i - 160] = rx;
        if (i >= 320 && i < 324) resp_blk2[i - 320] = rx;
        if (i >= 87040 && i < 87044) resp_regB[i - 87040] = rx;
        if (i >= FPGA_H2_CAL_TABLE_SIZE - 16) resp_last16[i - (FPGA_H2_CAL_TABLE_SIZE - 16)] = rx;
    }

    /* End opcode */
    resp_end[0] = spi3_raw_xfer(0x3A);
    resp_end[1] = spi3_raw_xfer(0x00);

    GPIOB->scr = (1 << 6);  /* CS deassert */

    /* Post-upload: wait then try reading */
    for (volatile int d = 0; d < 500000; d++);  /* ~50ms at 240MHz */

    usb_debug_printf("PC0 after upload: %d\r\n", (GPIOC->idt & 1) ? 1 : 0);

    /* Post-upload SPI3 read */
    GPIOB->clr = (1 << 6);
    for (int i = 0; i < 16; i++) {
        resp_post[i] = spi3_raw_xfer(0xFF);
    }
    GPIOB->scr = (1 << 6);

    /* --- Report --- */
    usb_send_str("\r\n-- Start opcode (0x3B, 0x00) responses --\r\n");
    usb_debug_printf("  %02X %02X\r\n", resp_start[0], resp_start[1]);

    usb_send_str("-- First 16 data bytes responses --\r\n  ");
    for (int i = 0; i < 16; i++) usb_debug_printf("%02X ", resp_first16[i]);
    usb_send_str("\r\n");

    usb_send_str("-- Sentinel area (bytes 26-33) --\r\n  ");
    for (int i = 0; i < 8; i++) usb_debug_printf("%02X ", resp_sentinel[i]);
    usb_send_str("\r\n");

    usb_send_str("-- Block 1 boundary (bytes 160-163) --\r\n  ");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X ", resp_blk1[i]);
    usb_send_str("\r\n");

    usb_send_str("-- Block 2 boundary (bytes 320-323) --\r\n  ");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X ", resp_blk2[i]);
    usb_send_str("\r\n");

    usb_send_str("-- Region B start (byte 87040) --\r\n  ");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X ", resp_regB[i]);
    usb_send_str("\r\n");

    usb_send_str("-- Last 16 data bytes responses --\r\n  ");
    for (int i = 0; i < 16; i++) usb_debug_printf("%02X ", resp_last16[i]);
    usb_send_str("\r\n");

    usb_send_str("-- End opcode (0x3A, 0x00) responses --\r\n");
    usb_debug_printf("  %02X %02X\r\n", resp_end[0], resp_end[1]);

    usb_send_str("-- Post-upload read (16x 0xFF) --\r\n  ");
    for (int i = 0; i < 16; i++) usb_debug_printf("%02X ", resp_post[i]);
    usb_send_str("\r\n");

    usb_debug_printf("\r\nTotal non-FF during TX replay: %d / 115638\r\n", total_nonff);
    usb_send_str("Interpretation: TX/sample diagnostic only; not calibration proof.\r\n");
    usb_debug_printf("PC0 final: %d\r\n", (GPIOC->idt & 1) ? 1 : 0);
    usb_send_str("=== Done ===\r\n");
}

/* spi3 xfer <hex...> — send an arbitrary MOSI byte sequence over SPI3 with
 * software CS and dump the MISO bytes clocked back. This is the generic
 * primitive needed to replay ripcord's literal command frames — e.g.
 * "spi3 xfer 04" to test whether a command-FIRST byte (which our acquisition
 * path never sends) wakes the FPGA, or "spi3 xfer 05" for the ID query.
 * CS (PB6) is asserted LOW only after every byte parses, and is ALWAYS
 * deasserted on exit so the FPGA slave is never left gated off. */
#define SPI3_XFER_MAX 64
static void cmd_spi3_xfer(const char *args)
{
    uint8_t tx[SPI3_XFER_MAX];
    uint8_t rx[SPI3_XFER_MAX];
    char buf[200];
    char *saveptr = NULL;
    char *tok;
    uint32_t n = 0;

    if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
    strcpy(buf, args);

    for (tok = strtok_r(buf, " \t", &saveptr); tok; tok = strtok_r(NULL, " \t", &saveptr)) {
        char *end;
        unsigned long v = strtoul(tok, &end, 16);   /* bare hex; "0x" optional */
        if (n >= SPI3_XFER_MAX) { usb_debug_printf("ERR: max %d bytes\r\n", SPI3_XFER_MAX); return; }
        if (*end != '\0' || v > 0xFF) { usb_debug_printf("ERR: bad hex byte '%s'\r\n", tok); return; }
        tx[n++] = (uint8_t)v;
    }
    if (n == 0) { usb_send_str("Usage: spi3 xfer <b0> <b1> ...\r\n"); return; }

    uint32_t pc0_before = (GPIOC->idt & 1) ? 1 : 0;
    GPIOB->clr = (1 << 6);          /* CS assert (LOW) */
    for (uint32_t i = 0; i < n; i++)
        rx[i] = spi3_raw_xfer(tx[i]);
    GPIOB->scr = (1 << 6);          /* CS deassert (HIGH) — always */
    uint32_t pc0_after = (GPIOC->idt & 1) ? 1 : 0;

    usb_send_str("MOSI:");
    for (uint32_t i = 0; i < n; i++) usb_debug_printf(" %02X", tx[i]);
    usb_send_str("\r\nMISO:");
    uint32_t nonff = 0;
    for (uint32_t i = 0; i < n; i++) {
        usb_debug_printf(" %02X", rx[i]);
        if (rx[i] != 0xFF) nonff++;
    }
    usb_debug_printf("\r\nnon-FF: %lu/%lu  PC0 %lu->%lu\r\n",
                     (unsigned long)nonff, (unsigned long)n, pc0_before, pc0_after);
}

/* spi3 acqread — read one acquisition frame per channel using the REAL
 * stock protocol decoded from the issue-#18 capture: per-channel 1026-byte
 * reads, opcode 0x04 (CH1) / 0x05 (CH2), in a single CS-LOW window each.
 * MISO layout per frame: [resp0][resp1][resp2] then ~1023 unsigned samples.
 * Reports PC0 before/after, the 3 status bytes, the first 16 samples, and
 * min/max/mean of the sample region — enough to tell a real waveform from a
 * flat line (feed the siggen into CH1 for a known signal). Read-only at the
 * FPGA, but it DRIVES THE SHARED SPI3 BUS — the old claim here that it "does
 * not touch the acquisition task" was false (audit 2026-08-20, P0.1): the
 * acq task preempts this task mid-CS-window unless parked. All callers now
 * run under the dispatch-level spi3_shell_claim(). */
static void cmd_spi3_acqread_one(uint8_t opcode)
{
    uint8_t first16[16];
    uint16_t smin = 255, smax = 0;
    uint32_t ssum = 0, scount = 0;

    uint32_t pc0_before = (GPIOC->idt & 1) ? 1 : 0;
    GPIOB->clr = (1 << 6);                 /* CS assert (LOW) */
    uint8_t r0 = spi3_raw_xfer(opcode);    /* MISO during opcode */
    uint8_t r1 = spi3_raw_xfer(0xFF);
    uint8_t r2 = spi3_raw_xfer(0xFF);
    for (uint32_t i = 0; i < 1023; i++) {  /* 3 status + 1023 = 1026-byte frame */
        uint8_t s = spi3_raw_xfer(0xFF);
        if (i < 16) first16[i] = s;
        if (s < smin) smin = s;
        if (s > smax) smax = s;
        ssum += s;
        scount++;
    }
    GPIOB->scr = (1 << 6);                 /* CS deassert (HIGH) */
    uint32_t pc0_after = (GPIOC->idt & 1) ? 1 : 0;

    usb_debug_printf("CH (0x%02X): status %02X %02X %02X  PC0 %lu->%lu\r\n",
                     opcode, r0, r1, r2, pc0_before, pc0_after);
    usb_send_str("  first16:");
    for (int i = 0; i < 16; i++) usb_debug_printf(" %02X", first16[i]);
    usb_debug_printf("\r\n  samples: min=%u max=%u mean=%lu span=%u\r\n",
                     smin, smax, scount ? ssum / scount : 0,
                     (unsigned)(smax - smin));
}

static void cmd_spi3_acqread(void)
{
    usb_send_str("=== acqread (real 0x04/0x05 protocol) ===\r\n");
    cmd_spi3_acqread_one(0x04);
    cmd_spi3_acqread_one(0x05);
    usb_send_str("(span>0 = live signal; span=0 = flat. Feed siggen->CH1 to verify.)\r\n");
}

/* ── Step 0b (bench plan 2026-08-14): the BSRAM_1/2 read-opcode hunt ──────
 *
 * The netlist work (gw1n2-apicula progress log M9–M11) identified BSRAM_1/2
 * as a separately-clocked, accumulate-in-place buffer pair — the shape of a
 * decimated/roll buffer, i.e. plausibly the slow timebase — and showed all
 * four BSRAMs feed the SAME readout mux (→ SPI SO + DRDY). So the MCU can
 * read it, under some opcode other than 0x04/0x05. These commands sweep the
 * opcode space for it on the live coldtrace rig.
 *
 * Framing is byte-identical to the proven 0x04/0x05 path: ONE CS-LOW window,
 * opcode + 2 filler bytes + payload. The continuous acquisition task is
 * parked first via fpga_acq_pause() — a shell CS assert interleaved with its
 * frames is the same desync class as the 30 ms cadence finding (fpga.c),
 * which needed a true FPGA power cycle to clear.
 *
 * Known opcode map on the CONFIGURED user design (not the Gowin config
 * port, which is closed — Exp L): 0x01/0x02/0x06/0x07/0x08 = arm-sequence
 * WRITES (opsweep skips them: the 0xFF payload filler would smash live
 * registers — 0x01 is the run register); 0x03 = status read; 0x04/0x05 =
 * channel reads (kept in the sweep as positive controls). */
#define SPI3_OPREAD_MAX_LEN 4096u

static bool spi3_opread_window(uint8_t opcode, uint32_t len, bool dump)
{
    uint8_t first16[16];
    uint8_t r0, r1, r2;
    uint16_t smin = 255, smax = 0;
    uint32_t ssum = 0, nonff = 0;

    GPIOB->clr = (1 << 6);                 /* CS assert (LOW) */
    r0 = spi3_raw_xfer(opcode);
    r1 = spi3_raw_xfer(0xFF);
    r2 = spi3_raw_xfer(0xFF);
    for (uint32_t i = 0; i < len; i++) {
        uint8_t s = spi3_raw_xfer(0xFF);
        if (i < 16) first16[i] = s;
        if (s < smin) smin = s;
        if (s > smax) smax = s;
        if (s != 0xFF) nonff++;
        ssum += s;
        if (dump) {
            if (i % 16 == 0) usb_debug_printf("%04lX:", (unsigned long)i);
            usb_debug_printf(" %02X", s);
            if (i % 16 == 15 || i == len - 1) usb_send_str("\r\n");
        }
    }
    GPIOB->scr = (1 << 6);                 /* CS deassert (HIGH) */

    usb_debug_printf("op %02X: s=%02X %02X %02X nff=%lu/%lu "
                     "min=%u max=%u mean=%lu span=%u first16:",
                     opcode, r0, r1, r2,
                     (unsigned long)nonff, (unsigned long)len,
                     smin, smax, (unsigned long)(len ? ssum / len : 0),
                     (unsigned)(smax - smin));
    for (int i = 0; i < 16 && (uint32_t)i < len; i++)
        usb_debug_printf(" %02X", first16[i]);
    usb_send_str("\r\n");
    return smax > smin;                    /* payload varies */
}

/* Full proven-shape 0x04 read; returns the sample span. Used as the wedge
 * canary between sweep steps. Full 1026-byte window on purpose — do NOT
 * shorten it (frame shape is part of the protocol; see the 0x05 hazard note
 * on fpga_warmtest_read_channel). Consumes one capture frame. */
static unsigned spi3_canary_span(void)
{
    uint8_t smin = 255, smax = 0;
    GPIOB->clr = (1 << 6);
    (void)spi3_raw_xfer(0x04);
    (void)spi3_raw_xfer(0xFF);
    (void)spi3_raw_xfer(0xFF);
    for (uint32_t i = 0; i < 1023; i++) {
        uint8_t s = spi3_raw_xfer(0xFF);
        if (s < smin) smin = s;
        if (s > smax) smax = s;
    }
    GPIOB->scr = (1 << 6);
    return (unsigned)(smax - smin);
}

/* spi3 opread <op-hex> [len [dump]] — one read window of `len` payload bytes
 * (default 1026, matching the channel-read shape; max 4096) under an
 * arbitrary opcode, with stats and optional full hex dump. */
static void cmd_spi3_opread(const char *args)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok, *end;
    uint32_t len = 1026;
    bool dump = false;
    unsigned long op;

    if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
    strcpy(buf, args);

    tok = strtok_r(buf, " \t", &saveptr);
    if (!tok) { usb_send_str("Usage: spi3 opread <op-hex> [len [dump]]\r\n"); return; }
    op = strtoul(tok, &end, 16);
    if (*end != '\0' || op > 0xFF) { usb_send_str("ERR: bad opcode\r\n"); return; }

    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok) {
        len = strtoul(tok, &end, 0);
        if (*end != '\0' || len == 0 || len > SPI3_OPREAD_MAX_LEN) {
            usb_debug_printf("ERR: len 1..%u\r\n", SPI3_OPREAD_MAX_LEN);
            return;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok && strcmp(tok, "dump") == 0) dump = true;
    }

    if (!fpga_acq_pause()) {
        usb_send_str("ERR: acq task did not park — bus not safe, aborting\r\n");
        return;
    }
    spi3_opread_window((uint8_t)op, len, dump);
    fpga_acq_resume();
}

/* spi3 opsweep [start end [len]] — sweep opcodes (hex bounds, default
 * 00..3F, known writes skipped), one window each, 0x04 canary between
 * steps. Run with the ESP32 feeding a LIVE slow signal into CH1: the
 * canary then has span>0, so a wedged readout mux is detectable and the
 * sweep aborts naming the offending opcode. A hit = an unknown opcode
 * whose payload varies / is non-FF — re-read it twice with different
 * siggen settings (spi3 opread <op> 2048 dump) to confirm it tracks. */
static void cmd_spi3_opsweep(const char *args)
{
    char buf[64];
    char *saveptr = NULL;
    char *tok, *end;
    uint32_t start = 0x00, stop = 0x3F, len = 2048;  /* 1024 words: BSRAM_1/2
                                                        is a word-wide pair */
    static const uint8_t skip[] = { 0x01, 0x02, 0x06, 0x07, 0x08 };
    unsigned flat_streak = 0;

    if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
    strcpy(buf, args);

    tok = strtok_r(buf, " \t", &saveptr);
    if (tok) {
        start = strtoul(tok, &end, 16);
        if (*end != '\0' || start > 0xFF) { usb_send_str("ERR: bad start\r\n"); return; }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (!tok) { usb_send_str("Usage: spi3 opsweep [start end [len]]\r\n"); return; }
        stop = strtoul(tok, &end, 16);
        if (*end != '\0' || stop > 0xFF || stop < start) { usb_send_str("ERR: bad end\r\n"); return; }
        tok = strtok_r(NULL, " \t", &saveptr);
        if (tok) {
            len = strtoul(tok, &end, 0);
            if (*end != '\0' || len == 0 || len > SPI3_OPREAD_MAX_LEN) {
                usb_debug_printf("ERR: len 1..%u\r\n", SPI3_OPREAD_MAX_LEN);
                return;
            }
        }
    }

    if (!fpga_acq_pause()) {
        usb_send_str("ERR: acq task did not park — bus not safe, aborting\r\n");
        return;
    }

    unsigned base_span = spi3_canary_span();
    usb_debug_printf("=== opsweep %02lX..%02lX len=%lu  baseline 0x04 span=%u ===\r\n",
                     (unsigned long)start, (unsigned long)stop,
                     (unsigned long)len, base_span);
    if (base_span == 0)
        usb_send_str("WARN: CH1 is flat — feed the siggen into CH1 first, or the\r\n"
                     "      canary cannot tell a wedged mux from no signal.\r\n");

    for (uint32_t op = start; op <= stop; op++) {
        bool skipped = false;
        for (unsigned i = 0; i < sizeof(skip); i++)
            if (op == skip[i]) { skipped = true; break; }
        if (skipped) {
            usb_debug_printf("op %02lX: SKIP (known write reg)\r\n", (unsigned long)op);
            continue;
        }

        spi3_opread_window((uint8_t)op, len, false);
        vTaskDelay(pdMS_TO_TICKS(150));    /* cadence floor, see fpga.c */
        unsigned cspan = spi3_canary_span();
        usb_debug_printf("      canary span=%u\r\n", cspan);
        if (base_span > 0) {
            flat_streak = (cspan == 0) ? flat_streak + 1 : 0;
            if (flat_streak >= 2) {
                usb_debug_printf("ABORT: canary flat twice — engine likely wedged "
                                 "by op %02lX or %02lX. FPGA power cycle to recover\r\n"
                                 "(POWER->Goodbye->UNPLUG USB->replug).\r\n",
                                 (unsigned long)op, (unsigned long)(op - 1));
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    usb_send_str("=== opsweep done ===\r\n");
    fpga_acq_resume();
}

/* spi3 gowin — read the Gowin SSPI ID / USERCODE / STATUS registers using
 * rosenrot00's PROVEN-WORKING 2C23T framing and decode the status bits.
 *
 * Framing (per fpga_hw4_read_register32 in the 2C23T loader): one flush byte
 * clocked with CS HIGH, then CS LOW, then opcode + 3 zero bytes, then 4 read
 * bytes, then CS HIGH. The opcode sits in the MSB of a 32-bit "address word"
 * (0x11<<24 etc), exactly as their read_register32(0x11000000) does.
 *
 * Why this matters: our `spi3 xfer 41 ...` returned 0x00000000 on a *running*
 * NV-booted FPGA. A configured GW1N must report DONE_FINAL / GOWIN_VLD / READY
 * in its status register — zero means either our READ FRAMING is wrong (this
 * command tests that by also reading IDCODE, which must equal 0x0120681B for
 * the GW1N-2) or the part is genuinely unconfigured. Either answer unblocks us.
 *
 * Gowin status-register bit names from openFPGALoader src/gowin.cpp. */
static uint32_t spi3_gowin_read_reg(uint8_t opcode)
{
    /* flush byte with CS HIGH (CS is idle-high here) */
    (void)spi3_raw_xfer(0x00);
    GPIOB->clr = (1 << 6);                 /* CS assert (PB6 LOW) */
    (void)spi3_raw_xfer(opcode);           /* opcode in MSB position */
    (void)spi3_raw_xfer(0x00);
    (void)spi3_raw_xfer(0x00);
    (void)spi3_raw_xfer(0x00);
    uint8_t b0 = spi3_raw_xfer(0x00);
    uint8_t b1 = spi3_raw_xfer(0x00);
    uint8_t b2 = spi3_raw_xfer(0x00);
    uint8_t b3 = spi3_raw_xfer(0x00);
    GPIOB->scr = (1 << 6);                 /* CS deassert (PB6 HIGH) */
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) | (uint32_t)b3;
}

/* Set SPI3 baud divider on the fly (CTRL1 bits[5:3]); needs SPE off/on.
 * br: 0=/2(60MHz) .. 7=/256(~470kHz). Returns the previous CTRL1. */
static uint32_t spi3_set_baud(uint32_t br)
{
    volatile uint32_t *ctrl1 = (volatile uint32_t *)0x40003C00;
    uint32_t prev = *ctrl1;
    *ctrl1 &= ~(1u << 6);                                   /* SPE = 0 */
    *ctrl1 = (*ctrl1 & ~(7u << 3)) | ((br & 7u) << 3);      /* set MDIV */
    *ctrl1 |= (1u << 6);                                    /* SPE = 1 */
    return prev;
}

static void cmd_spi3_gowin(void)
{
    static const struct { uint32_t mask; const char *name; } STATUS_BITS[] = {
        { 1u << 0,  "CRC Error" },
        { 1u << 1,  "Bad Command" },
        { 1u << 2,  "ID Verify Failed" },
        { 1u << 3,  "Timeout" },
        { 1u << 5,  "Memory Erase" },
        { 1u << 6,  "Preamble" },
        { 1u << 7,  "System Edit Mode" },
        { 1u << 8,  "Program spiFlash directly" },
        { 1u << 10, "Non-JTAG config active" },
        { 1u << 11, "Bypass" },
        { 1u << 12, "Gowin VLD" },
        { 1u << 13, "Done Final" },
        { 1u << 14, "Security Final" },
        { 1u << 15, "Ready" },
        { 1u << 16, "POR" },
        { 1u << 17, "FLASH lock" },
    };

    usb_send_str("=== Gowin SSPI registers (rosenrot00 framing) ===\r\n");

    /* Read at two clock rates: /2 (60MHz, our normal) and /256 (~470kHz,
     * closer to rosenrot00's bit-bang). If the slow read returns the real
     * IDCODE but the fast one doesn't, the SSPI read path is clock-limited. */
    static const struct { uint32_t br; const char *label; } SPEEDS[] = {
        { 0u, "/2  (60MHz)" },
        { 7u, "/256(~470kHz)" },
    };

    uint32_t saved = 0;
    for (unsigned s = 0; s < sizeof(SPEEDS) / sizeof(SPEEDS[0]); s++) {
        if (s == 0) saved = spi3_set_baud(SPEEDS[s].br);
        else        (void)spi3_set_baud(SPEEDS[s].br);

        uint32_t id  = spi3_gowin_read_reg(0x11);   /* READ_IDCODE   */
        uint32_t usr = spi3_gowin_read_reg(0x13);   /* READ_USERCODE */
        uint32_t st  = spi3_gowin_read_reg(0x41);   /* READ_STATUS   */

        /* The IDCODE read in THIS window, at THIS clock, is the anchor: it is
         * the only value here with an independently known correct answer
         * (0x0120681B, from the Gowin .fs preamble at file offset 0x4AD19).
         * If it does not match, the read path is not trustworthy and neither
         * is the STATUS taken through it — so the decode is withheld rather
         * than printed as a list of named bits.
         *
         * This is not hypothetical. The /2 pass below is KNOWN BAD (fpga.c
         * ~1564: at /2 the MISO bit is latched one edge early). Six weeks of
         * this project ran on "0x8001C810 = READY POR", which was 0x00039020
         * shifted by one bit and set bits 4/11/31 that are not defined Gowin
         * bits at all. The old code here printed that decode with no caveat. */
        int anchored = (id == 0x0120681BUL);

        usb_debug_printf("\r\n[clk %s]%s\r\n", SPEEDS[s].label,
                         SPEEDS[s].br == 0
                             ? "  <-- KNOWN-INVALID READ CLOCK (fpga.c:1564);"
                               " kept only as the negative control"
                             : "");
        usb_debug_printf("IDCODE  (0x11): 0x%08lX  %s\r\n", id,
                         anchored ? "== GW1N-2 OK (read path ANCHORED)"
                                  : "!= 0x0120681B -> READ PATH NOT ANCHORED");
        usb_debug_printf("USERCODE(0x13): 0x%08lX%s\r\n", usr,
                         anchored ? "" : "  (unanchored — do not quote)");
        usb_debug_printf("STATUS  (0x41): 0x%08lX%s\r\n", st,
                         anchored ? "" : "  (unanchored — do not quote)");
        if (!anchored) {
            usb_send_str("  decode: WITHHELD — the anchor failed, so this word is not\r\n"
                         "          known to be a register value. A stable wrong number\r\n"
                         "          is indistinguishable from a right one.\r\n");
        } else {
            usb_send_str("  decode:");
            int any = 0;
            for (unsigned i = 0; i < sizeof(STATUS_BITS) / sizeof(STATUS_BITS[0]); i++) {
                if (st & STATUS_BITS[i].mask) {
                    usb_debug_printf(" [%s]", STATUS_BITS[i].name);
                    any = 1;
                }
            }
            usb_send_str(any ? "\r\n" : " (no bits set)\r\n");
        }
    }

    /* Restore the original CTRL1 (baud + SPE) for the rest of the system. */
    {
        volatile uint32_t *ctrl1 = (volatile uint32_t *)0x40003C00;
        *ctrl1 &= ~(1u << 6);
        *ctrl1 = saved;
    }

    usb_send_str("\r\nHow to read this (Exps J and L, 2026-07-28):\r\n"
                 "  IDCODE 0x0120681B at /256 = read path is valid; STATUS can be trusted.\r\n"
                 "  IDCODE silent/zero at /256 = the SSPI config port has CLOSED. That is\r\n"
                 "    what a SUCCESSFULLY CONFIGURED part looks like (the pins belong to the\r\n"
                 "    user design from then on) -- but a dead bus looks identical, so\r\n"
                 "    corroborate with DONE_FINAL from the config path and a live trace.\r\n"
                 "  The /2 pass is expected to be garbage; it is the negative control.\r\n");
}

/* ─── fpga selftest ────────────────────────────────────────────────────────
 *
 * ONE precondition table, printed before an experiment, so the instrument is
 * verified rather than assumed. Every entry here is something that has, at
 * least once, silently invalidated a measurement in this project:
 *
 *   SPI3 baud     reads at /2 are garbage (fpga.c:1564) — six weeks of this
 *                 project ran on a status word sampled one bit early.
 *   IDCODE anchor the only value on this bus with an independently known
 *                 correct answer (0x0120681B, Gowin .fs preamble at file
 *                 offset 0x4AD19). CLAUDE.md makes it mandatory: a stable
 *                 wrong number is indistinguishable from a right one.
 *   USART2        UEN clear on coldtrace builds, and the dvom_TX drain task
 *                 absent, so "queued" frames never reach the wire.
 *   pin modes     PA6 and PD12 boot as floating INPUTS; `gpio set` on them
 *                 used to report success while driving nothing.
 *
 * STATUS (0x41) is deliberately NOT read here. On a configured part a config-
 * port read desynchronises acquisition (Exp L, 2026-07-28), and this command
 * is meant to be safe to run in the middle of a capture session. IDCODE is
 * the safe anchor; its DISAPPEARANCE is itself the "configured" signature.
 */
static void cmd_fpga_selftest(void)
{
    volatile uint32_t *spi3_ctrl1 = (volatile uint32_t *)0x40003C00;
    uint32_t ctrl1, usart_c1;
    uint32_t br, id;
    unsigned inert_pins = 0;
    bool acq_parked;

    usb_send_str("=== fpga selftest — instrument preconditions ===\r\n");

    /* ---- SPI3 --------------------------------------------------------- */
    ctrl1 = *spi3_ctrl1;
    br = (ctrl1 >> 3) & 7u;
    usb_debug_printf("[SPI3 ] CTRL1=%08lX  BR=%lu -> /%lu (%lu kHz)  SPE=%u MSTEN=%u CPOL=%u CPHA=%u\r\n",
                     (unsigned long)ctrl1, (unsigned long)br,
                     (unsigned long)(2u << br),
                     (unsigned long)((system_core_clock / 2u) / (2u << br) / 1000u),
                     (unsigned)((ctrl1 >> 6) & 1u), (unsigned)((ctrl1 >> 2) & 1u),
                     (unsigned)((ctrl1 >> 1) & 1u), (unsigned)(ctrl1 & 1u));
    if (br != 7u)
        usb_send_str("        NOTE: SSPI *config-port* reads are only clean at /256 (BR=7);\r\n"
                     "        at /2 MISO is latched one edge early (fpga.c:1564). The anchor\r\n"
                     "        read below switches to /256 and restores CTRL1 afterwards.\r\n");

    /* ---- IDCODE anchor ------------------------------------------------ */
    acq_parked = fpga_acq_pause();
    if (!acq_parked) {
        usb_send_str("[anchr] SKIPPED — acq task would not park, so a read here would\r\n"
                     "        interleave with its capture frames. Bus state UNVERIFIED.\r\n");
    } else {
        uint32_t saved = spi3_set_baud(7u);       /* /256 — the only valid read clock */
        id = spi3_gowin_read_reg(0x11);           /* READ_IDCODE */
        *spi3_ctrl1 &= ~(1u << 6);                /* SPE off before restoring CTRL1 */
        *spi3_ctrl1 = saved;
        fpga_acq_resume();

        usb_debug_printf("[anchr] IDCODE(0x11) @/256 = 0x%08lX  %s\r\n",
                         (unsigned long)id,
                         id == 0x0120681BUL
                             ? "== GW1N-2, read path ANCHORED (part UNCONFIGURED)"
                             : (id == 0UL
                                    ? "config port CLOSED — expected on a CONFIGURED part (Exp L)"
                                    : "!= 0x0120681B and != 0 -> READ PATH NOT TRUSTWORTHY"));
        if (id != 0x0120681BUL && id != 0UL)
            usb_send_str("        Do not quote any SSPI value taken through this bus until the\r\n"
                         "        anchor reads correctly.\r\n");
    }
    usb_send_str("        (STATUS 0x41 not read — it desyncs a running configured part.)\r\n");

    /* ---- USART2 ------------------------------------------------------- */
    usart_c1 = fpga_usart_ctrl1();
    usb_debug_printf("[USART] CTRL1=%08lX BAUDR=%08lX  UEN=%u TE=%u RE=%u RDBFIEN=%u\r\n",
                     (unsigned long)usart_c1, (unsigned long)fpga_usart_baudr(),
                     (unsigned)((usart_c1 >> 13) & 1u), (unsigned)((usart_c1 >> 3) & 1u),
                     (unsigned)((usart_c1 >> 2) & 1u), (unsigned)((usart_c1 >> 5) & 1u));
    usb_debug_printf("        dvom_TX drain task: %s\r\n",
                     fpga_usart_tx_task_exists()
                         ? "present (queued sends will go out)"
                         : "ABSENT — queued sends would never transmit; the shell now\r\n"
                           "        falls back to the polled path and says so");
    if (((usart_c1 >> 13) & 1u) == 0)
        usb_send_str("        UEN CLEAR: the wire is dark. No USART command can reach the\r\n"
                     "        FPGA, so silence is NOT a measurement. `fpga usart on` first.\r\n");

    /* ---- driven pins -------------------------------------------------- */
    usb_send_str("[pins ] pins this firmware drives (mode nibble / level):\r\n");
    for (unsigned i = 0; i < BENCH_PIN_COUNT; i++) {
        uint8_t nib = gpio_cfg_nibble(BENCH_PINS[i].port, BENCH_PINS[i].pin_no);
        if (!gpio_mode_is_output(nib) || gpio_mode_is_af(nib)) inert_pins++;
        bench_pin_print_row(&BENCH_PINS[i], true);
    }

    usb_debug_printf("\r\nverdict: %u of %u driven pins are NOT plain outputs — `gpio set` on\r\n"
                     "         those is inert. %s\r\n",
                     inert_pins, (unsigned)BENCH_PIN_COUNT,
                     inert_pins ? "Fix with `gpio mode <pin> out` before measuring."
                                : "Frontend posture is drivable.");
    usb_send_str("         Snapshot before an experiment with `bench snapshot`, and put the\r\n"
                 "         posture back with `bench restore` so it cannot leak into the next.\r\n");
}

/* spi3 scopetest [bank] — run the FULL runtime scope-capture sequence the
 * stock firmware uses, independent of the 0x3B bitstream config:
 *   1. send the scope-mode USART config (timebase/trigger/channel) — this is
 *      what switches the FPGA out of meter mode into scope acquisition;
 *   2. wait for PC0 (data-ready, active LOW) to arm — stock free-runs sampling
 *      once in scope mode and pulses PC0 low when a frame is ready;
 *   3. read CH1 (0x04) and CH2 (0x05) with the REAL per-channel protocol.
 *
 * This is the decisive test of whether the NV-resident bitstream can do scope
 * at all. If PC0 arms and the reads show span>0, the bitstream upload was a
 * detour and we have a trace. If PC0 never arms, config really is required. */
static void cmd_spi3_scopetest(const char *args)
{
    uint8_t bank = 0;
    if (args && *args) bank = (uint8_t)strtoul(args, NULL, 0);

    usb_send_str("=== scope test: USART scope-cfg -> PC0 wait -> 0x04/0x05 ===\r\n");
    usb_debug_printf("PB11(active)=%d PC6(spi_en)=%d PC0(rdy)=%d before cfg\r\n",
                     (GPIOB->idt & (1 << 11)) ? 1 : 0,
                     (GPIOC->idt & (1 << 6)) ? 1 : 0,
                     (GPIOC->idt & (1 << 0)) ? 1 : 0);

    usb_debug_printf("Sending scope-mode USART config (bank=%u)...\r\n", bank);
    fpga_wire_scope_sequence(bank);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Wait up to 500ms for PC0 to go LOW (data ready). Stock polls it ~1kHz. */
    int armed = 0, waited = 0;
    for (waited = 0; waited < 500; waited++) {
        if (!(GPIOC->idt & (1U << 0))) { armed = 1; break; }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    usb_debug_printf("PC0 data-ready: %s (waited %dms)\r\n",
                     armed ? "ARMED (went LOW)" : "NEVER (stuck HIGH)", waited);

    usb_send_str("--- CH1 (0x04) / CH2 (0x05) ---\r\n");
    cmd_spi3_acqread_one(0x04);
    cmd_spi3_acqread_one(0x05);
    usb_send_str("(span>0 on either channel = the NV bitstream CAN do scope!)\r\n");
}

/* spi3 armtest [pb11|pc6] — the runtime-arm bench recipe from
 * mcu_fpga_boundary_reconcile_2026-06-13.md (§5). The apicula netlist trace
 * shows MISO (the sole IOBUF's SO.OEN) is gated by a free-running read-window
 * counter that only advances while the FPGA's run/re-arm pad (IOR1B) is driven,
 * and that re-arm runs through an async-preset (DFF.SET) *pulse* path — a held-
 * HIGH level satisfies "active mode" but may never re-pulse the SET nets that
 * restart capture after the first window (the "one buffer then stop" symptom).
 * IOR1B maps to PB11 (ranked #1) or PC6 (#2) MCU-side. This command:
 *   1. baseline acqread (static level — the failure we already see);
 *   2. PULSE the run pin HIGH->LOW->HIGH (rising edge into the re-arm input);
 *   3. re-issue the stock post-config control-register write
 *      (01 08 / 02 03 / 06 00 / 07 00 / 08 AD; one bit feeds capture-enable);
 *   4. acqread again.
 * Predicted: span>0 after but not at baseline -> IOR1B<-this pin, runtime arm
 * cracked. Still all-FF with pb11 -> rerun `spi3 armtest pc6`. Neither arms it
 * -> the run line is an unbonded top-edge IOT pad and needs a board trace.
 * Run on the FPGA_WARM_HANDOFF_TEST build (stock design alive in SRAM = gate 1
 * satisfied) so only this runtime read path is under test. Polarity (HIGH=run)
 * is a bench hypothesis — the netlist read is static-structural. */
static void cmd_spi3_armtest(const char *args)
{
    gpio_type *port = GPIOB;
    uint32_t   mask = (1u << 11);
    const char *name = "PB11";
    if (args && (args[0] == 'p' || args[0] == 'P') &&
                (args[1] == 'c' || args[1] == 'C')) {
        port = GPIOC; mask = (1u << 6); name = "PC6";
    }

    usb_send_str("=== armtest: pulse run pin -> control-reg -> acqread ===\r\n");
    usb_debug_printf("run pin = %s   PB11=%d PC6=%d PC0(rdy)=%d\r\n", name,
                     (GPIOB->idt & (1 << 11)) ? 1 : 0,
                     (GPIOC->idt & (1 << 6)) ? 1 : 0,
                     (GPIOC->idt & (1 << 0)) ? 1 : 0);

    /* 1. Baseline: static level as-is (the held-HIGH "one buffer then stop"). */
    usb_send_str("--- baseline (static level) ---\r\n");
    cmd_spi3_acqread_one(0x04);
    cmd_spi3_acqread_one(0x05);

    /* 2. Pulse: idle-HIGH -> LOW -> HIGH. The LOW->HIGH rising edge is the
     * candidate async-preset re-arm trigger (needs >25ns; we use ms). Ends in
     * the run (HIGH) state. Repeat a few times. */
    usb_send_str("--- pulsing run pin (HIGH->LOW->HIGH x3) ---\r\n");
    for (int i = 0; i < 3; i++) {
        port->clr = mask;                 /* LOW  */
        vTaskDelay(pdMS_TO_TICKS(2));
        port->scr = mask;                 /* HIGH (run) */
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    /* 3. Re-issue the stock post-config control-register write, CS-framed
     * exactly as fpga.c step 7c (one bit of this feeds the capture-enable). */
    static const uint8_t scope_cfg[][2] = {
        { 0x01, 0x08 }, { 0x02, 0x03 }, { 0x06, 0x00 },
        { 0x07, 0x00 }, { 0x08, 0xAD },
    };
    for (unsigned i = 0; i < sizeof(scope_cfg) / sizeof(scope_cfg[0]); i++) {
        GPIOB->clr = (1 << 6);            /* CS LOW (PB6) */
        (void)spi3_raw_xfer(scope_cfg[i][0]);
        (void)spi3_raw_xfer(scope_cfg[i][1]);
        GPIOB->scr = (1 << 6);            /* CS HIGH */
    }
    vTaskDelay(pdMS_TO_TICKS(5));

    /* 4. Re-read. span>0 now (vs flat baseline) = the run-pin pulse armed it. */
    usb_send_str("--- after pulse + control-reg ---\r\n");
    cmd_spi3_acqread_one(0x04);
    cmd_spi3_acqread_one(0x05);
    usb_send_str("(span>0 here but not at baseline = IOR1B is this pin; arm cracked.\r\n"
                 " all-FF with pb11 -> rerun `spi3 armtest pc6`.)\r\n");
}

#if defined(FPGA_ALT_BITSTREAM)
/* ═══════════════════════════════════════════════════════════════════
 * Diagnostic-bitstream debug interface  (guest-debugclk / FPGA_ALT_BITSTREAM)
 * ═══════════════════════════════════════════════════════════════════
 *
 * The debugclk_hw_top image drives SYNTHETIC lanes (CH1 = free-running 8-bit
 * ramp, CH2 = walking one) through the REAL capture/readout path, so the bytes
 * coming back are known a priori. That is what makes the open question
 * answerable: in the 2026-08-15 desk sweep op 0x04 and op 0x05 both returned
 * CH1, and nothing since has separated "the readout mux does not split the
 * channels" from "our framing is wrong". With a known ramp on one lane and a
 * one-hot pattern on the other, the two look nothing alike.
 *
 * Contract for these two commands, from the image's RTL constraints file
 * (fnirsi_2c53t_qn48.cst), not inferred:
 *
 *   PC6  = QN48 pin 30 = IOB7B = dbg_clk. No PLL in this image — the MCU paces
 *          the ENTIRE capture core, ONE SAMPLE PER RISING EDGE. At least 8
 *          clocks must be sent before anything else (an 8-cycle power-on-reset
 *          shifter runs off this clock).
 *   PB11 = QN48 pin 46 = IOR1B = run_enable. capture_enable = run_enable AND
 *          sample_tick; a low->high edge clears the trigger latch and re-arms,
 *          and capture free-runs while it is HIGH. No SPI command is involved.
 *
 * ⚠ Both pins mean something ELSE on a stock-payload build — PC6 is "FPGA SPI
 * enable (must be HIGH)" and PB11 is "FPGA active mode" (which the 2026-08-15
 * desk sweep re-read as a CH2 attenuator relay). So this whole block is
 * compiled ONLY under FPGA_ALT_BITSTREAM: the commands cannot be typed at a
 * stock-payload build, and gating them also keeps every existing image
 * byte-identical, which is what makes the payload swap auditable.
 *
 * Reading the result needs no new code: `spi3 opread 04 1026 dump` prints raw
 * bytes from index 0, so the image's 3-byte "R1V" header is visible directly.
 * Do NOT retarget fpga_warmtest_read_channel at it — that function is
 * stock-framed (2-byte discard, 1024 samples) on purpose, and this image is
 * 3-byte-header/1023-sample. One byte apart, opposite requirements.
 */

/* Busy-wait microseconds off the FREE-RUNNING FreeRTOS SysTick without
 * reprogramming it — fpga.c's systick_delay_us seizes LOAD/VAL/CTRL outright,
 * which is only safe pre-scheduler and would eat the RTOS tick here.
 * Accumulates down-counter deltas so it stays correct across reload wraps.
 * Not jitter-free: the tick ISR or a higher-priority task can stretch a half
 * period, so half_us is a floor, not a guarantee. The FPGA does not care —
 * it counts edges, not time. */
static void dbg_delay_us(uint32_t us)
{
    if (us == 0) return;

    uint32_t load = SysTick->LOAD + 1u;
    if (load <= 1u) {                       /* SysTick not running (pre-scheduler) */
        for (uint32_t i = 0; i < us * 30u; i++) __asm__ volatile("nop");
        return;
    }

    uint64_t target = (uint64_t)us * (system_core_clock / 1000000u);
    uint64_t acc = 0;
    uint32_t prev = SysTick->VAL;
    while (acc < target) {
        uint32_t now = SysTick->VAL;
        acc += (prev >= now) ? (uint32_t)(prev - now)
                             : (uint32_t)(prev + load - now);
        prev = now;
    }
}

#define DBG_CLK_MAX_COUNT    1000000u
#define DBG_CLK_MAX_HALF_US  100000u

/* fpga dbgclk <count> [half_us] — emit `count` RISING edges on PC6, the
 * diagnostic image's sample clock. One rising edge = one sample, so this is
 * the sample-rate knob in its entirety. Default half_us=10 (~50 kHz).
 *
 * Idles HIGH and pulses LOW->HIGH rather than the other way round: that yields
 * exactly `count` rising edges AND leaves PC6 at the level a stock-payload
 * build expects, so the restore below never has to manufacture an extra edge. */
static void cmd_fpga_dbgclk(const char *args)
{
    char buf[48];
    char *saveptr = NULL, *tok;
    uint32_t count = 0, half_us = 10;

    if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
    strcpy(buf, args);

    tok = strtok_r(buf, " \t", &saveptr);
    if (!tok || parse_int(tok, &count) != 0 || count == 0 || count > DBG_CLK_MAX_COUNT) {
        usb_debug_printf("Usage: fpga dbgclk <count 1..%lu> [half_us 0..%lu]\r\n",
                         (unsigned long)DBG_CLK_MAX_COUNT,
                         (unsigned long)DBG_CLK_MAX_HALF_US);
        return;
    }
    tok = strtok_r(NULL, " \t", &saveptr);
    if (tok && (parse_int(tok, &half_us) != 0 || half_us > DBG_CLK_MAX_HALF_US)) {
        usb_debug_printf("ERR: half_us 0..%lu\r\n", (unsigned long)DBG_CLK_MAX_HALF_US);
        return;
    }

    /* PC6 is the SPI-enable line on a stock payload, and on ANY payload the
     * continuous acq task may be mid-CS-frame. Interleaving is the desync class
     * that needed a true FPGA power cycle to clear (see spi3_opread_window). */
    if (!fpga_acq_pause()) {
        usb_send_str("ERR: acq task did not park — bus not safe, aborting\r\n");
        return;
    }

    /* Save PC6's config nibble and output level so this is non-destructive.
     * PC6 is pin 6 => CRL/cfglr bits 27:24. Read-modify-write THAT NIBBLE ONLY:
     * a past session wrote a whole cfghr word for pins that live in cfglr,
     * floated PC8 (POWER) and PC13, and the device read a phantom POWER press
     * and shut down mid-test. gpio_init() with a single-pin mask does the
     * per-nibble RMW for us on the way in; the restore does it by hand. */
    uint32_t saved_nib = (GPIOC->cfglr >> 24) & 0xFu;
    uint32_t saved_lvl = (GPIOC->odt >> 6) & 1u;

    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins            = GPIO_PINS_6;
    gpio_cfg.gpio_mode            = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type        = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_pull            = GPIO_PULL_NONE;
    gpio_cfg.gpio_drive_strength  = GPIO_DRIVE_STRENGTH_STRONGER;
    GPIOC->scr = (1u << 6);          /* stage HIGH before the driver turns on */
    gpio_init(GPIOC, &gpio_cfg);     /* `gpio set` alone does nothing on a pin
                                      * that is not already an output — that has
                                      * cost this project a session before. */

    for (uint32_t i = 0; i < count; i++) {
        GPIOC->clr = (1u << 6);
        dbg_delay_us(half_us);
        GPIOC->scr = (1u << 6);      /* rising edge = one sample */
        dbg_delay_us(half_us);
    }

    /* Restore level first (still driven, so no float glitch), then the mode. */
    if (saved_lvl) GPIOC->scr = (1u << 6);
    else           GPIOC->clr = (1u << 6);
    GPIOC->cfglr = (GPIOC->cfglr & ~(0xFu << 24)) | (saved_nib << 24);

    fpga_acq_resume();

    usb_debug_printf("dbgclk: %lu rising edges on PC6, half=%luus; "
                     "PC6 restored (cfg nibble %lX, level %lu); PC0=%d PB11=%d\r\n",
                     (unsigned long)count, (unsigned long)half_us,
                     (unsigned long)saved_nib, (unsigned long)saved_lvl,
                     (GPIOC->idt & (1u << 0))  ? 1 : 0,
                     (GPIOB->idt & (1u << 11)) ? 1 : 0);
    if (!saved_lvl)
        usb_send_str("NOTE: PC6 was LOW on entry and was restored LOW. On a stock payload\r\n"
                     "      that is the FPGA-SPI-disabled state.\r\n");
}

/* fpga dbgarm [low_ms] — re-arm the diagnostic image: PB11 (run_enable) LOW
 * for low_ms, then HIGH. The low->high edge clears the trigger latch; capture
 * then free-runs for as long as PB11 stays HIGH (gated by dbg_clk ticks).
 * Default 2 ms; 0 gives a ~100 us pulse.
 *
 * Unlike dbgclk this deliberately does NOT restore PB11's previous mode: HIGH
 * and driven IS the run state, and handing the pin back to a floating input
 * would undo the arm we just performed. The previous config nibble is printed
 * so it can be put back by hand if needed. */
static void cmd_fpga_dbgarm(const char *args)
{
    uint32_t low_ms = 2;

    if (args && *args) {
        if (parse_int(args, &low_ms) != 0 || low_ms > 1000u) {
            usb_send_str("Usage: fpga dbgarm [low_ms 0..1000]\r\n");
            return;
        }
    }

    if (!fpga_acq_pause()) {
        usb_send_str("ERR: acq task did not park — bus not safe, aborting\r\n");
        return;
    }

    /* PB11 is pin 11 => CRH/cfghr nibble (11-8)*4 = 12. Single-pin gpio_init,
     * never a whole-word write (see the PC8/PC13 incident noted on dbgclk). */
    uint32_t saved_nib = (GPIOB->cfghr >> 12) & 0xFu;
    uint32_t saved_lvl = (GPIOB->odt >> 11) & 1u;

    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins            = GPIO_PINS_11;
    gpio_cfg.gpio_mode            = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type        = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_pull            = GPIO_PULL_NONE;
    gpio_cfg.gpio_drive_strength  = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    GPIOB->clr = (1u << 11);                     /* run_enable LOW  */
    if (low_ms) vTaskDelay(pdMS_TO_TICKS(low_ms));
    else        dbg_delay_us(100);
    GPIOB->scr = (1u << 11);                     /* LOW->HIGH = re-arm + run */

    fpga_acq_resume();

    usb_debug_printf("dbgarm: PB11 LOW %lums -> HIGH (armed, free-running). "
                     "was cfg nibble %lX level %lu; left as output-pp HIGH. PC0=%d\r\n",
                     (unsigned long)low_ms,
                     (unsigned long)saved_nib, (unsigned long)saved_lvl,
                     (GPIOC->idt & (1u << 0)) ? 1 : 0);
    usb_send_str("Next: `fpga dbgclk <n>` to clock samples in, then "
                 "`spi3 opread 04 1026 dump`.\r\n");
}
#endif /* FPGA_ALT_BITSTREAM */

/* fpga reinit [br] [prelude_gap_ms] [post_close_ms] — replay the full SPI3
 * config handshake on demand (prelude → 0x3B bitstream → 0x3A close → scope
 * config) and report the result. Lets us sweep the handshake parameters in
 * seconds without reflashing, chasing why our upload activates the FPGA slave
 * but never reaches stock's configured state (close F8 / status 00 01 42 2E).
 * Defaults match the stock-captured timing: br=0 (/2), gap=100ms, close=600ms. */
static void cmd_fpga_reinit(const char *args)
{
    /* guest-bringup leaves the bus released (PB3/5/6 Hi-Z, SPE off). reinit means
     * "take the bus and configure", so reacquire it first — otherwise the whole
     * handshake clocks into a dead wire (H7, 2026-08-26). No-op on other builds. */
    if (fpga.bus_released) {
        fpga_spi3_bus_reacquire();
        usb_send_str("reinit: bus was released -> reacquired SPI3 (PB3/4/5/6, SPE)\r\n");
    }

    fpga_cfg_seq_opts_t opt = {
        .upload_br = 0, .prelude_gap_ms = 100, .post_close_ms = 600, .arm_pb11 = 1,
        .reset_port = 0, .reset_pin = 0, .reset_low_ms = 10,
        .prelude_frame_mode = 0, .pre_upload_gap_ms = 0, .cmd_br = 0,
        .strap_pd2 = 0, .strap_pd1213 = 0, .trailing_clocks = 0,
        .probe_edit = 0, .reload_3c = 0,
    };
    char buf[80];
    if (args && *args) {
        if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
        strcpy(buf, args);
        char *save = NULL;
        char *t0 = strtok_r(buf, " \t", &save);
        char *t1 = t0 ? strtok_r(NULL, " \t", &save) : NULL;
        char *t2 = t1 ? strtok_r(NULL, " \t", &save) : NULL;
        if (t0) opt.upload_br      = (uint32_t)strtoul(t0, NULL, 0);
        if (t1) opt.prelude_gap_ms = (uint32_t)strtoul(t1, NULL, 0);
        if (t2) opt.post_close_ms  = (uint32_t)strtoul(t2, NULL, 0);
        /* Remaining tokens are optional, order-independent, prefix-classified:
         *   a..e<pin> = FPGA reset pulse pin (e.g. b9)
         *   f<0|1|2>  = prelude frame mode (split/combined/merge)
         *   u<ms>     = pre-upload digest gap after CONFIG_ENABLE */
        for (char *tk = strtok_r(NULL, " \t", &save); tk;
             tk = strtok_r(NULL, " \t", &save)) {
            if (tk[0] >= 'a' && tk[0] <= 'e' && tk[1]) {   /* <port><pin> */
                opt.reset_port = (uint8_t)(tk[0] - 'a' + 1);
                opt.reset_pin  = (uint8_t)strtoul(tk + 1, NULL, 10);
            } else if (tk[0] == 'f') {
                opt.prelude_frame_mode = (uint8_t)strtoul(tk + 1, NULL, 10);
            } else if (tk[0] == 'u') {
                opt.pre_upload_gap_ms = (uint32_t)strtoul(tk + 1, NULL, 0);
            } else if (tk[0] == 'k') {  /* 'k' (clock) — NOT 'c', which collides
                                         * with reset-port c (a..e) above */
                opt.cmd_br = (uint32_t)strtoul(tk + 1, NULL, 0);
            } else if (tk[0] == 's') {  /* strap-hold: s2[h|l]=PD2, sd[h|l]=PD12+13
                                         * (default HIGH = stock). GPIO-audit lead.
                                         * sb = single-BR (EXP-34): no SPE toggle
                                         * through the config transaction. */
                uint8_t lvl = (tk[2] == 'l') ? 2 : 1;
                if (tk[1] == '2')      opt.strap_pd2    = lvl;
                else if (tk[1] == 'd') opt.strap_pd1213 = lvl;
                else if (tk[1] == 'b') opt.single_br    = 1;
            } else if (tk[0] == 't' && tk[1] == 'c') {  /* tc<N> = trailing clocks
                                         * after bitstream, before 0x3A (sibling ~200) */
                opt.trailing_clocks = (uint16_t)strtoul(tk + 2, NULL, 0);
            } else if (tk[0] == 'p' && tk[1] == 'e') {  /* pe = probe SYSTEM_EDIT_MODE:
                                         * read STATUS at /256 right after 0x15 */
                opt.probe_edit = 1;
            } else if (tk[0] == 'r' && tk[1] == 'l') {  /* rl = send 0x3C RELOAD before
                                         * the prelude (software reconfig trigger) */
                opt.reload_3c = 1;
            }
        }
    }

    static const char *const frame_name[] = { "split", "combined", "merge" };
    const char *fn = opt.prelude_frame_mode < 3 ? frame_name[opt.prelude_frame_mode] : "?";
    if (opt.reset_port)
        usb_debug_printf("reinit: br=%lu gap=%lums close=%lums frame=%s(%u) upgap=%lums cmdbr=%lu RESET=%c%u(%ums)\r\n",
                         opt.upload_br, opt.prelude_gap_ms, opt.post_close_ms,
                         fn, opt.prelude_frame_mode, opt.pre_upload_gap_ms, opt.cmd_br,
                         'A' + opt.reset_port - 1, opt.reset_pin, opt.reset_low_ms);
    else
        usb_debug_printf("reinit: br=%lu prelude_gap=%lums post_close=%lums frame=%s(%u) upgap=%lums cmdbr=%lu (no reset)\r\n",
                         opt.upload_br, opt.prelude_gap_ms, opt.post_close_ms,
                         fn, opt.prelude_frame_mode, opt.pre_upload_gap_ms, opt.cmd_br);
    if (opt.strap_pd2 || opt.strap_pd1213)
        usb_debug_printf("reinit: STRAP PD2=%s PD12/13=%s (held thru handshake)\r\n",
                         opt.strap_pd2 == 1 ? "HIGH" : opt.strap_pd2 == 2 ? "LOW" : "-",
                         opt.strap_pd1213 == 1 ? "HIGH" : opt.strap_pd1213 == 2 ? "LOW" : "-");
    if (opt.trailing_clocks)
        usb_debug_printf("reinit: trailing_clocks=%u (after bitstream, before 0x3A)\r\n",
                         opt.trailing_clocks);
    if (opt.single_br)
        usb_send_str("reinit: SINGLE-BR — no spi3_set_br/SPE toggle through the transaction (EXP-34)\r\n");

    uint8_t close = fpga_spi3_config_sequence(&opt);

    usb_debug_printf("0x3A close: %02X (stock F8)\r\n", close);
    usb_debug_printf("0x03 status: %02X %02X %02X %02X (stock 00 01 42 2E)\r\n",
                     fpga.scope_status[0], fpga.scope_status[1],
                     fpga.scope_status[2], fpga.scope_status[3]);

    /* Gowin STATUS_REGISTER (0x41) — the authoritative config status. Decode the
     * named bits so one reinit run tells us WHICH side of the wall we're on:
     *   all-FF  -> FPGA not driving MISO (never entered config-receive)
     *   CRC/IDV -> bytes reached the config engine (wire/content problem)
     *   DONE    -> config actually took. */
    uint32_t sr = ((uint32_t)fpga.cfg_status_reg[0] << 24) |
                  ((uint32_t)fpga.cfg_status_reg[1] << 16) |
                  ((uint32_t)fpga.cfg_status_reg[2] << 8) |
                  (uint32_t)fpga.cfg_status_reg[3];
    usb_debug_printf("0x41 STATUS: %02X %02X %02X %02X (raw=%08lX)\r\n",
                     fpga.cfg_status_reg[0], fpga.cfg_status_reg[1],
                     fpga.cfg_status_reg[2], fpga.cfg_status_reg[3], sr);
    if (sr == 0xFFFFFFFFu || sr == 0x00000000u) {
        usb_debug_printf("  -> %s (no valid status; FPGA not in SSPI config-receive)\r\n",
                         sr == 0xFFFFFFFFu ? "all-FF" : "all-00");
    } else {
        usb_debug_printf("  flags:%s%s%s%s%s%s%s\r\n",
                         (sr & (1u << 0))  ? " CRC_ERR"   : "",
                         (sr & (1u << 1))  ? " BAD_CMD"   : "",
                         (sr & (1u << 2))  ? " ID_FAIL"   : "",
                         (sr & (1u << 12)) ? " GWVLD"     : "",
                         (sr & (1u << 13)) ? " DONE"      : "",
                         (sr & (1u << 15)) ? " READY"     : "",
                         (sr & (1u << 16)) ? " POR"       : "");
    }
    /* Post-CONFIG_ENABLE STATUS (probe_edit): did 0x15 engage SYSTEM_EDIT_MODE
     * (bit7)? Read at /256 right after 0x15, before the bitstream — the precise
     * wall test (openFPGALoader's enableCfg() polls exactly this bit). */
    if (opt.reload_3c)
        usb_send_str("reinit: sent 0x3C RELOAD before prelude\r\n");
    if (opt.probe_edit) {
        uint32_t es = ((uint32_t)fpga.edit_mode_status[0] << 24) |
                      ((uint32_t)fpga.edit_mode_status[1] << 16) |
                      ((uint32_t)fpga.edit_mode_status[2] << 8) |
                      (uint32_t)fpga.edit_mode_status[3];
        usb_debug_printf("post-0x15 STATUS@/256: %02X %02X %02X %02X (raw=%08lX)\r\n",
                         fpga.edit_mode_status[0], fpga.edit_mode_status[1],
                         fpga.edit_mode_status[2], fpga.edit_mode_status[3], es);
        usb_debug_printf("  EDIT_MODE(bit7)=%s  flags:%s%s%s%s%s%s\r\n",
                         (es & (1u << 7))  ? "YES (config-receive engaged!)" : "no (0x15 did NOT engage)",
                         (es & (1u << 0))  ? " CRC_ERR" : "",
                         (es & (1u << 2))  ? " ID_FAIL" : "",
                         (es & (1u << 12)) ? " GWVLD"   : "",
                         (es & (1u << 13)) ? " DONE"    : "",
                         (es & (1u << 15)) ? " READY"   : "",
                         (es & (1u << 17)) ? " FLASH_LOCK" : "");
    }
    usb_send_str("--- acqread after reinit ---\r\n");
    cmd_spi3_acqread_one(0x04);
    cmd_spi3_acqread_one(0x05);
}

/* spi3 seq <bytes> | <bytes> [| ...] — like "spi3 xfer" but pulse CS (PB6
 * HIGH then LOW, ~tens of us inside this one handler) at each "|" separator.
 * Reproduces ripcord's cmd-09 pattern "09 FF FF | 0A FF FF", where a
 * mid-sequence CS pulse splits the command byte from the embedded 0x0A
 * sub-opcode. CS is always deasserted on exit. */
static void cmd_spi3_seq(const char *args)
{
    uint8_t tx[SPI3_XFER_MAX];
    uint8_t rx[SPI3_XFER_MAX];
    uint8_t pulse_after[SPI3_XFER_MAX];   /* 1 = pulse CS after this byte */
    char buf[200];
    char *saveptr = NULL;
    char *tok;
    uint32_t n = 0;

    if (strlen(args) >= sizeof(buf)) { usb_send_str("ERR: line too long\r\n"); return; }
    strcpy(buf, args);

    for (tok = strtok_r(buf, " \t", &saveptr); tok; tok = strtok_r(NULL, " \t", &saveptr)) {
        if (strcmp(tok, "|") == 0) {
            if (n == 0) { usb_send_str("ERR: '|' before any byte\r\n"); return; }
            pulse_after[n - 1] = 1;       /* pulse CS after the previous byte */
            continue;
        }
        char *end;
        unsigned long v = strtoul(tok, &end, 16);   /* bare hex; "0x" optional */
        if (n >= SPI3_XFER_MAX) { usb_debug_printf("ERR: max %d bytes\r\n", SPI3_XFER_MAX); return; }
        if (*end != '\0' || v > 0xFF) { usb_debug_printf("ERR: bad hex byte '%s'\r\n", tok); return; }
        pulse_after[n] = 0;
        tx[n++] = (uint8_t)v;
    }
    if (n == 0) { usb_send_str("Usage: spi3 seq <b..> | <b..>\r\n"); return; }

    GPIOB->clr = (1 << 6);          /* CS assert */
    for (uint32_t i = 0; i < n; i++) {
        rx[i] = spi3_raw_xfer(tx[i]);
        if (pulse_after[i]) {
            GPIOB->scr = (1 << 6);              /* CS HIGH */
            for (volatile int d = 0; d < 4000; d++);
            GPIOB->clr = (1 << 6);              /* CS LOW */
            for (volatile int d = 0; d < 4000; d++);
        }
    }
    GPIOB->scr = (1 << 6);          /* CS deassert — always */

    usb_send_str("MISO:");
    for (uint32_t i = 0; i < n; i++) {
        usb_debug_printf(" %02X", rx[i]);
        if (pulse_after[i]) usb_send_str(" |");
    }
    usb_send_str("\r\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Stock SPI3 case-8 readback probe — not a DMM correction
 *
 * Stock V1.2.0 `spi3_acquisition_task` dispatches queue trigger byte 9
 * (`trigger_byte - 1 == 8`) to the block at project address 0x0803779C.  That
 * block performs a small SPI3 readback and stores the assembled halfword at
 * `ms+0x46` (`DAT_2000013E`).  RAM-map/decompile consumers classify that word
 * as scope `trigger_position_sample` evidence, not H2 acceptance and not DMM
 * range/calibration state.
 *
 * Keep this command diagnostic-only.  It intentionally does not patch the DMM
 * reading, change selector words, or use the returned bytes as a coefficient.
 * ═══════════════════════════════════════════════════════════════════ */
static void cmd_spi3_stock_readback(void)
{
    uint8_t first_discard;
    uint8_t seed_hi;
    uint8_t cmd_0a_rx;
    uint8_t mid_discard;
    uint8_t low;
    uint16_t ms46_equiv;

    usb_send_str("=== Stock SPI3 Case-8 Readback Diagnostic ===\r\n");
    usb_send_str("Stock refs: 0x0803779C..0x080378F4 stores ms+0x46; scope-shaped, not DMM apply/calibration.\r\n\r\n");

    usb_debug_printf("PC0  (data-ready): %d\r\n", (GPIOC->idt & (1 << 0)) ? 1 : 0);
    usb_debug_printf("PC6  (SPI enable): %d\r\n", (GPIOC->idt & (1 << 6)) ? 1 : 0);
    usb_debug_printf("PB11 (active):     %d\r\n", (GPIOB->idt & (1 << 11)) ? 1 : 0);
    usb_debug_printf("PB6  (CS idle):    %d\r\n", (GPIOB->idt & (1 << 6)) ? 1 : 0);

    /*
     * Mirror the visible stock transaction shape:
     *   CS low:  0xFF discard, 0xFF -> high byte seed
     *   CS high: 1 tick delay
     *   CS low:  0x0A discard, 0xFF discard, 0xFF -> low byte
     *
     * The middle 0xFF read is discarded by the stock block before shifting the
     * earlier seed into the high byte.  Printing it helps catch non-FF activity
     * without pretending the stock app uses it for DMM state.
     */
    GPIOB->clr = (1 << 6);
    first_discard = spi3_raw_xfer(0xFF);
    seed_hi = spi3_raw_xfer(0xFF);
    GPIOB->scr = (1 << 6);

    vTaskDelay(pdMS_TO_TICKS(1));

    GPIOB->clr = (1 << 6);
    cmd_0a_rx = spi3_raw_xfer(0x0A);
    mid_discard = spi3_raw_xfer(0xFF);
    low = spi3_raw_xfer(0xFF);
    GPIOB->scr = (1 << 6);

    ms46_equiv = ((uint16_t)seed_hi << 8) | (uint16_t)low;

    usb_debug_printf("rx first_ff_discard=%02X seed_hi=%02X cmd_0a=%02X mid_ff_discard=%02X low=%02X\r\n",
                     first_discard, seed_hi, cmd_0a_rx, mid_discard, low);
    usb_debug_printf("ms46_equiv=0x%04X\r\n", ms46_equiv);
    usb_debug_printf("PC0 final: %d\r\n", (GPIOC->idt & 1) ? 1 : 0);
    usb_send_str("Interpretation: diagnostic scope readback only; not a DMM multiplier, range writer, H2 ACK, or calibration proof.\r\n");
    usb_send_str("=== Done ===\r\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Command Dispatcher
 * ═══════════════════════════════════════════════════════════════════ */

/* Extracted from the old dispatch chain (was an inline block). SPI3 claim/
 * release is handled by the dispatcher via SC_SPI3. */
static void cmd_spi3_probe(void)
{
    /* Bit-bang SPI3 probe: disable SPI peripheral, manually toggle
     * SCK and read MISO to test if the FPGA drives the line. This pokes the
     * SPI3 registers directly, so an unparked acq task would corrupt far
     * more than one frame — SC_SPI3 on the table row handles the park. */
    usb_send_str("=== SPI3 Bit-Bang Probe ===\r\n");

    /* Read PB4 (MISO) idle state */
    uint32_t miso_idle = (GPIOB->idt & (1 << 4)) ? 1 : 0;
    usb_debug_printf("MISO idle (CS high): %lu\r\n", miso_idle);

    /* Assert CS (PB6 LOW) */
    GPIOB->clr = (1 << 6);
    for (volatile int d = 0; d < 1000; d++);  /* brief delay */
    uint32_t miso_cs = (GPIOB->idt & (1 << 4)) ? 1 : 0;
    usb_debug_printf("MISO after CS assert: %lu\r\n", miso_cs);

    /* Try reading through SPI peripheral */
    volatile uint32_t *spi_sts = (volatile uint32_t *)0x40003C08;
    volatile uint32_t *spi_dt  = (volatile uint32_t *)0x40003C0C;

    /* Clear any pending RX data */
    if (*spi_sts & 0x01) { (void)*spi_dt; }

    /* Send 0x00 and read response */
    uint32_t timeout = 100000;
    while (!(*spi_sts & 0x02) && --timeout);  /* Wait TXE */
    *spi_dt = 0x00;  /* Send dummy byte */
    timeout = 100000;
    while (!(*spi_sts & 0x01) && --timeout);  /* Wait RXNE */
    uint8_t rx = (uint8_t)*spi_dt;
    usb_debug_printf("SPI3 xfer(0x00) = 0x%02X (timeout=%lu)\r\n", rx, timeout);

    /* Send 0x05 (FPGA query cmd) */
    timeout = 100000;
    while (!(*spi_sts & 0x02) && --timeout);
    *spi_dt = 0x05;
    timeout = 100000;
    while (!(*spi_sts & 0x01) && --timeout);
    rx = (uint8_t)*spi_dt;
    usb_debug_printf("SPI3 xfer(0x05) = 0x%02X (timeout=%lu)\r\n", rx, timeout);

    /* Send another 0x00 */
    timeout = 100000;
    while (!(*spi_sts & 0x02) && --timeout);
    *spi_dt = 0x00;
    timeout = 100000;
    while (!(*spi_sts & 0x01) && --timeout);
    rx = (uint8_t)*spi_dt;
    usb_debug_printf("SPI3 xfer(0x00) = 0x%02X (timeout=%lu)\r\n", rx, timeout);

    /* Deassert CS */
    GPIOB->scr = (1 << 6);
    usb_debug_printf("SPI3 STS: 0x%08lX  CTRL1: 0x%08lX\r\n",
                     *spi_sts, *(volatile uint32_t *)0x40003C00);

    /* Also check PC6 state */
    usb_debug_printf("PC6 (SPI enable): %d\r\n",
                     (GPIOC->idt & (1 << 6)) ? 1 : 0);
}

/* ═══════════════════════════════════════════════════════════════════
 * Shell command table (2026-08-20; audit item 8.9)
 *
 * Replaces the ~110-entry if/strcmp dispatch chain. Each row binds ONE
 * command to its handler, its bus-safety requirement and its help text, so
 * the three can never drift apart again — the P0.1 fix had to hand-wrap
 * eleven dispatch entries in spi3_shell_claim(); here that is one flag.
 *
 * Matching: a row matches when its name is a whole-word prefix of the line
 * (next char is NUL/space/tab). The dispatcher picks the LONGEST matching
 * name, so ordering in this table carries no semantics and near-miss pairs
 * ("meter auto" / "meter autoscan", "trig" / "trig2") cannot shadow each
 * other. Flags:
 *   SC_EXACT    command takes no arguments; trailing text = no match
 *               (falls through to "Unknown command", as before).
 *   SC_NEEDARGS command requires arguments; a bare name prints the row's
 *               help instead of running the handler — this preserves the
 *               old chain's "trailing space required" contract and matters
 *               because parse_int("") SUCCEEDS with 0, so e.g. a bare
 *               "fpga cmd" would otherwise send 0x00 0x00 at the FPGA.
 *   SC_SPI3     handler drives the raw SPI3 bus: the dispatcher parks the
 *               acquisition task around the call (audit P0.1).
 *
 * Adding a command = adding ONE row (CMD_A if the handler takes an args
 * string, CMD_V if it takes void). Help is part of the row; keep the
 * left-aligned name+argspec column at 32 chars like the rows around it.
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    const char *name;                    /* full command word(s) */
    void (*fn_args)(const char *args);   /* exactly one of these two is set */
    void (*fn_void)(void);
    uint8_t flags;
    const char *help;                    /* preformatted line(s); NULL = alias row */
} shell_cmd_t;

#define SC_EXACT    0x01u
#define SC_NEEDARGS 0x02u
#define SC_SPI3     0x04u

#define CMD_A(name, fn, flags, help)  { name, fn, NULL, flags, help }
#define CMD_V(name, fn, flags, help)  { name, NULL, fn, flags, help }

static const shell_cmd_t shell_cmds[] = {
    CMD_V("help", cmd_help, SC_EXACT,
          "help                            Show this help\r\n"),
    CMD_V("?", cmd_help, SC_EXACT,
          NULL /* alias */),
    CMD_V("version", cmd_version, SC_EXACT,
          "version                         Firmware info\r\n"),
    CMD_V("status", cmd_status, SC_EXACT,
          "status                          FPGA & system status\r\n"),
    CMD_A("usart raw", cmd_usart_raw, SC_NEEDARGS,
          "usart raw <10 hex bytes>       Send raw 10-byte USART frame\r\n" "  e.g.: usart raw 00 00 00 0B 01 00 00 00 00 0B\r\n"),
    CMD_A("usart tx", cmd_usart_tx, SC_NEEDARGS,
          "usart tx <cmd_hi> <cmd_lo>      Send FPGA command (queued, or direct if no dvom_TX)\r\n"),
    CMD_A("gpio set", cmd_gpio_set, SC_NEEDARGS,
          "gpio set <port><pin> <0|1>      Drive GPIO pin (REFUSES if pin is an input)\r\n" "  e.g.: gpio set B11 1\r\n"),
    CMD_A("gpio mode", cmd_gpio_mode, SC_NEEDARGS,
          "gpio mode <port><pin> <out|in>  Set pin direction (RMW of its CRL/CRH nibble)\r\n" "  e.g.: gpio mode A6 out   (out = push-pull 50MHz, in = floating)\r\n"),
    CMD_V("bench snapshot", cmd_bench_snapshot, SC_EXACT,
          "bench snapshot                  Save frontend/FPGA pin modes+levels\r\n"),
    CMD_V("bench restore", cmd_bench_restore, SC_EXACT,
          "bench restore                   Put that saved pin posture back (hermetic tests)\r\n"),
    CMD_A("gpio read", cmd_gpio_read, SC_NEEDARGS,
          "gpio read <port><pin>           Read GPIO pin\r\n"),
    CMD_V("gpio scan", cmd_gpio_scan, SC_EXACT,
          "gpio scan                       Scan FPGA-related pins\r\n"),
    CMD_A("buzzer test", cmd_buzzer_test, 0,
          "buzzer test [ms]                Force continuity buzzer briefly\r\n"),
    CMD_A("mem read", cmd_mem_read, SC_NEEDARGS,
          "mem read <addr> [count]         Read 32-bit words\r\n" "  e.g.: mem read 0x40021000 4\r\n"),
    CMD_A("mem write", cmd_mem_write, SC_NEEDARGS,
          "mem write <addr> <value>        Write 32-bit word\r\n"),
    CMD_A("trig2", cmd_scope_trig2, SC_NEEDARGS,
          "trig2 raw <0-4095>              Write CH2 ref: TMR13 CH1 PWM-DAC (PA6)\r\n" "trig2 <range> <level>           CH2 vertical-offset ref via the CH1 cal formula\r\n"),
    CMD_A("trig", cmd_scope_trig, SC_NEEDARGS,
          "trig raw <0-4095>               Write DAC1 (PA4) directly + sw trigger\r\n" "trig <range> <level>            Scope trigger DAC: range 0-9, level -100..100\r\n"),
    CMD_V("flash jedec", cmd_flash_jedec, SC_EXACT,
          "flash jedec                     Read external W25Q128 JEDEC ID\r\n"),
    CMD_A("flash read", cmd_flash_read, SC_NEEDARGS,
          "flash read <addr> <len>         Read external flash bytes (max 256)\r\n"),
    CMD_A("flash dump", cmd_flash_dump, SC_NEEDARGS,
          "flash dump <addr> <len>         Stream external flash bytes (max 4096)\r\n"),
    CMD_A("flash wtest", cmd_flash_wtest, SC_NEEDARGS,
          "flash wtest <addr> CONFIRM      Non-destructive write-primitive self-test (blank 4KB sector)\r\n"),
    CMD_V("flash diag", cmd_flash_diag, SC_EXACT,
          "flash diag                      W25Q SR1/SR2/SR3 + WEL-latch check\r\n"),
    CMD_V("cal status", cmd_cal_status, SC_EXACT,
          "cal status                      Factory-cal page + W25Q backup state\r\n"),
    CMD_V("cal backup", cmd_cal_backup, SC_EXACT,
          "cal backup                      Copy MCU factory-cal page to W25Q (safe)\r\n"),
    CMD_A("cal restore", cmd_cal_restore, SC_NEEDARGS,
          "cal restore [force] CONFIRM     Write W25Q backup back to MCU flash 0x08006000\r\n"),
    CMD_A("screen dump", cmd_screen_dump, 0,
          "screen dump [shadow] [x y w h]  Dump text indexed4 LCD shadow\r\n"),
    CMD_A("screen dumpbin", cmd_screen_dumpbin, 0,
          "screen dumpbin [x y w h]        Binary indexed4 LCD shadow dump\r\n"),
    CMD_A("screen shadow", cmd_screen_shadow, 0,
          "screen shadow page [y]          Clear full-screen shadow capture\r\n"),
    CMD_A("fpga cmd", cmd_fpga_cmd, SC_NEEDARGS,
          "fpga cmd <hi> <lo>              Send FPGA command bytes\r\n" "  e.g.: fpga cmd 0 9   (sends 0x00 0x09)\r\n" "        fpga cmd 0x0509 (sends 0x05 0x09)\r\n"),
    CMD_A("fpga frame", cmd_fpga_frame, SC_NEEDARGS,
          "fpga frame <hi> <lo> [p1..p5 [ck]]  Build/send full 10-byte frame\r\n" "  e.g.: fpga frame 00 0B 01 00 00 00 00\r\n"),
    CMD_V("fpga diag clear", cmd_fpga_diag_clear, SC_EXACT,
          "fpga diag clear                 Clear FPGA bench counters/state\r\n"),
    CMD_V("fpga selftest", cmd_fpga_selftest, SC_EXACT,
          "fpga selftest                   Precondition table: SPI3 clk, IDCODE anchor,\r\n" "                                USART2 state, every driven pin's mode+level\r\n"),
#if defined(FPGA_ALT_BITSTREAM)
    CMD_A("fpga dbgclk", cmd_fpga_dbgclk, 0,
          "fpga dbgclk <count> [half_us]   debugclk image: N rising edges on PC6 (=N samples)\r\n"),
    CMD_A("fpga dbgarm", cmd_fpga_dbgarm, 0,
          "fpga dbgarm [low_ms]            debugclk image: re-arm via PB11 low->high (run_enable)\r\n"),
#endif
    CMD_V("fpga busrelease", cmd_fpga_bus_release, SC_EXACT,
          "fpga busrelease                 Hand SPI3 to an external master (one-way, EXPERIMENTAL)\r\n"),
    CMD_V("fpga busreacquire", cmd_fpga_bus_reacquire, SC_EXACT,
          "fpga busreacquire               Take SPI3 back after busrelease (H7 capture prep)\r\n"),
    CMD_V("fpga configbb", cmd_fpga_configbb, SC_EXACT,
          "fpga configbb                   Run GPIO bit-bang SSPI config on demand (H7 step 1)\r\n"),
    CMD_V("fpga stock diag", cmd_fpga_stock_diag, SC_EXACT,
          "fpga stock diag                Show stock-state bench shadow\r\n"),
    CMD_V("fpga stock clear", cmd_fpga_stock_clear, SC_EXACT,
          "fpga stock clear               Reset stock-state bench shadow\r\n"),
    CMD_A("fpga stock set", cmd_fpga_stock_set, SC_NEEDARGS,
          "fpga stock set <9 bytes>       Set F68/F69/F6A/F6B/E1A/E1B/E1C/E1D/355\r\n"),
    CMD_A("fpga stock preset", cmd_fpga_stock_preset, SC_NEEDARGS,
          "fpga stock preset <4|5 bytes>  Set F68/F69/F6A/F6B [355]\r\n"),
    CMD_V("fpga stock base2", cmd_fpga_stock_base2, SC_EXACT,
          "fpga stock base2               Seed visible state 2 scope posture\r\n"),
    CMD_A("fpga stock state5", cmd_fpga_stock_state5, 0,
          "fpga stock state5 [E1B] [E1D]  Seed visible state 5 editor posture\r\n"),
    CMD_A("fpga stock state6", cmd_fpga_stock_state6, 0,
          "fpga stock state6 [E1B] [E1D]  Seed visible state 6 pre-entry posture\r\n"),
    CMD_V("fpga stock prev", cmd_fpga_stock_prev, SC_EXACT,
          "fpga stock prev                Drive stock-like adjust-prev family\r\n"),
    CMD_V("fpga stock next", cmd_fpga_stock_next, SC_EXACT,
          "fpga stock next                Drive stock-like adjust-next family\r\n"),
    CMD_V("fpga stock select", cmd_fpga_stock_select, SC_EXACT,
          "fpga stock select              Stage single detail selection\r\n"),
    CMD_V("fpga stock toggle", cmd_fpga_stock_toggle, SC_EXACT,
          "fpga stock toggle              Toggle staged detail bitmap\r\n"),
    CMD_V("fpga stock commit", cmd_fpga_stock_commit, SC_EXACT,
          "fpga stock commit              Walk E1C 0->2->1->0x2B commit path\r\n"),
    CMD_V("fpga stock consume", cmd_fpga_stock_consume, SC_EXACT,
          "fpga stock consume             Consume packed state-9 preset path\r\n"),
    CMD_V("fpga stock bridge fixed", cmd_fpga_stock_bridge_fixed, SC_EXACT,
          "fpga stock bridge fixed        Probe post-13/14 fixed 0x0501 path\r\n"),
    CMD_A("fpga stock bridge dynamic", cmd_fpga_stock_bridge_dynamic, 0,
          "fpga stock bridge dynamic [ch1|ch2|both]  Probe post-13/14 0x050x path\r\n"),
    CMD_V("fpga stock reenter", cmd_fpga_stock_reenter, SC_EXACT,
          "fpga stock reenter             Re-enter scope path with staged shadow\r\n"),
    CMD_A("fpga wire words", cmd_fpga_wire_words, SC_NEEDARGS,
          "fpga wire words <w...>         Send final 16-bit wire words directly\r\n"),
    CMD_A("fpga wire entry", cmd_fpga_wire_entry, 0,
          "fpga wire entry [ch1|ch2|both] Send candidate scope-entry wire-word bank\r\n"),
    CMD_A("fpga wire scope", cmd_fpga_wire_scope, 0,
          "fpga wire scope [ch1|ch2|both] Wire-word entry + runtime scope blocks\r\n"),
    CMD_A("fpga scope range", cmd_fpga_scope_range, SC_NEEDARGS,
          "fpga scope range <0-9> <1|2|both>  Coarse frontend range; channel is MANDATORY\r\n"),
    CMD_V("fpga scope cal", cmd_fpga_scope_cal, SC_EXACT,
          "fpga scope cal                  Dump the compiled vertical cal table (mV/count, V/div, tier)\r\n"),
    CMD_A("fpga scope vdiv", cmd_fpga_scope_vdiv, 0,
          "fpga scope vdiv <1|2> <0-9>     Set volts/div range: display AND relays together\r\n"),
    CMD_A("fpga scope measure", cmd_fpga_scope_measure, 0,
          "fpga scope measure [reps]       Badge values raw (uV/permille/mHz) for bench validation\r\n"),
    CMD_A("fpga scope freq", cmd_fpga_scope_freq, 0,
          "fpga scope freq [n]             Run the shipped frequency estimator n times, with diagnostics\r\n"),
    CMD_A("fpga scope graticule", cmd_fpga_scope_graticule, 0,
          "fpga scope graticule [auto|true|toggle]  Trace at true volts/div vs autofit-to-band\r\n"),
    CMD_A("fpga scope softtrig", cmd_fpga_scope_softtrig, 0,
          "fpga scope softtrig [on|off|toggle]      Lock trace to trigger crossing vs free-run\r\n"),
    CMD_V("settings", cmd_settings, SC_EXACT,
          "settings                        Persistence status: bound, load result, writes, failures\r\n"),
    CMD_A("fpga scope timebase", cmd_fpga_scope_timebase, 0,
          "fpga scope timebase [code]      Set timebase in BOTH display state and reg 0x01 (hex)\r\n"),
    CMD_A("fpga scope center", cmd_fpga_scope_center, 0,
          "fpga scope center [ch2] [0-9]   Auto-center offset ref per range (CH1/DAC1 default); all if omitted\r\n"),
    CMD_A("fpga usart", cmd_fpga_usart, 0,
          "fpga usart [on|off]             Bring USART2 up post-config; show CTRL1+RX\r\n"),
    CMD_A("fpga rearm", cmd_fpga_rearm, 0,
          "fpga rearm [on|off]             Stock post-read re-arm (reg01) A/B toggle\r\n"),
    CMD_A("fpga rate", cmd_fpga_rate, 0,
          "fpga rate [hexidx]              reg-0x01 rate index the re-arm rewrites\r\n"),
    CMD_V("fpga scope reinit", cmd_fpga_scope_reinit, SC_EXACT,
          "fpga scope reinit               Re-apply scope frontend + FPGA cfg\r\n"),
    CMD_A("fpga meter reinit", cmd_fpga_meter_reinit, 0,
          "fpga meter reinit [submode]     Re-apply meter frontend + FPGA cfg\r\n"),
    CMD_V("fpga scope wake", cmd_fpga_scope_wake, SC_EXACT,
          "fpga scope wake                 Meter wake preamble then scope cfg\r\n"),
    CMD_V("fpga scope acqmode", cmd_fpga_scope_acqmode, SC_EXACT,
          "fpga scope acqmode              Send stock-like 0x20/0x21 block\r\n"),
    CMD_A("fpga scope beat", cmd_fpga_scope_beat, 0,
          "fpga scope beat [count] [ms]    Send stock-like cmd-3 heartbeat(s)\r\n"),
    CMD_A("fpga scope entry", cmd_fpga_scope_entry, SC_NEEDARGS,
          "fpga scope entry <8 bytes>      Reset + send 0x01,0B..11 params\r\n"),
    CMD_A("fpga scope timing", cmd_fpga_scope_timing, SC_NEEDARGS,
          "fpga scope timing <5 bytes>     Send 0x20,0x21,0x26..0x28 params\r\n"),
    CMD_A("fpga scope trig", cmd_fpga_scope_trig, SC_NEEDARGS,
          "fpga scope trig <4 bytes>       Send 0x07/0x0A,0x16..0x19\r\n"),
    CMD_A("mode", cmd_mode, 0,
          "mode meter [submode] [layout]   Switch UI + FPGA to DMM frontend\r\n" "mode scope                      Switch UI + FPGA to scope frontend\r\n" "mode startup [scope|meter]      Get/set Settings > Startup on Boot\r\n"),
    CMD_A("meter dump", cmd_meter_dump, 0,
          "meter dump [delay_ms]           Show parsed DMM/UI/raw frame state\r\n"),
    CMD_A("meter autoscan", cmd_meter_autoscan, 0,
          "meter autoscan [settle_ms]      Probe DMM submodes and select best live mode\r\n"),
    CMD_A("meter auto", cmd_meter_auto_async, 0,
          "meter auto [start|status|cancel] Async DMM function auto-select\r\n"),
    CMD_V("meter trace", cmd_meter_trace, SC_EXACT,
          "meter trace                     One machine-readable DMM producer record\r\n"),
    CMD_V("meter frontend", cmd_meter_frontend, SC_EXACT,
          "meter frontend                  Show DMM analog frontend GPIO state\r\n"),
    CMD_A("meter probe-tail", cmd_meter_probe_tail, 0,
          "meter probe-tail [auto|07|0a]   Override stock PC7-gated DMM tail for diagnostics\r\n"),
    CMD_A("meter boot-sequence", cmd_meter_boot_sequence, 0,
          "meter boot-sequence [ms]        Replay stock DMM boot word order + trace\r\n"),
    CMD_A("meter pc11-timing", cmd_meter_pc11_timing, 0,
          "meter pc11-timing [lo hi]       Probe DMM PC11 gate timing + trace\r\n"),
    CMD_A("meter mux-arms", cmd_meter_mux_arms, SC_NEEDARGS,
          "meter mux-arms <ce> <ab> [ms]   Apply stock mux arms, poll, trace\r\n"),
    CMD_A("meter mux-stream", cmd_meter_mux_stream, 0,
          "meter mux-stream [count] [ms]   Stream DMM frames plus frontend GPIOs\r\n"),
    CMD_A("meter stream", cmd_meter_stream, 0,
          "meter stream [count] [delay_ms] Print compact DMM frame stream\r\n"),
    CMD_V("meter adc-snapshot", cmd_meter_adc_snapshot, SC_EXACT,
          "meter adc-snapshot              Show read-only DMM waveform sampler state\r\n"),
    CMD_V("ui dump", cmd_ui_dump, SC_EXACT,
          "ui dump                         Show current UI mode/redraw state\r\n"),
    CMD_A("meter wave", cmd_meter_wave_args, 0,
          "meter wave                      Show DMM voltage waveform sample stats\r\n" "meter wave reset                Reset DMM waveform diagnostics\r\n" "meter wave sampler [on|off]     Enable/disable experimental SPI3 sampler\r\n" "meter wave path [direct|preacq] Get/set DMM waveform SPI path\r\n" "meter wave selector [auto|N]    DMM wave selector byte\r\n" "meter wave preacq [auto|N]      DMM wave pre-acq byte\r\n"),
    CMD_A("fpga acq", cmd_fpga_acq, 0,
          "fpga acq [mode]                 Trigger SPI3 acquisition\r\n"),
    CMD_A("fpga reinit", cmd_fpga_reinit, SC_SPI3,
          "fpga reinit [br][gap][close][f0|1|2][u<ms>][a-e<pin>][s2|sd[h|l]][tc<n>] Replay cfg\r\n" "    f=prelude frame: 0 split(stock) 1 combined 2 merge15+3B; u=pre-upload gap; k<br>=cmd-phase clk div; tc<n>=trailing clocks\r\n" "    pe=probe SYSTEM_EDIT_MODE after 0x15 (STATUS@/256); rl=send 0x3C RELOAD before prelude; reports 0x41 STATUS\r\n"),
    CMD_A("spi3 xfer", cmd_spi3_xfer, SC_SPI3,
          "spi3 xfer <hex...>              Send arbitrary MOSI bytes, dump MISO\r\n"),
    CMD_A("spi3 seq", cmd_spi3_seq, SC_SPI3,
          "spi3 seq <b..> | <b..>          xfer w/ mid-sequence CS pulse at '|'\r\n"),
    CMD_A("spi3 read", cmd_spi3_read, 0,
          "spi3 read [len]                 Raw SPI3 read + hex dump\r\n"),
    CMD_V("reboot bootloader", cmd_reboot_bootloader, SC_EXACT,
          "reboot bootloader               Reboot into USB HID updater\r\n"),
    CMD_V("spi3 acqread", cmd_spi3_acqread, SC_EXACT | SC_SPI3,
          "spi3 acqread                    Read CH1/CH2 via real 0x04/0x05 protocol\r\n"),
    CMD_A("spi3 opread", cmd_spi3_opread, 0,
          "spi3 opread <op> [len [dump]]   One read window under any opcode + stats\r\n"),
    CMD_A("spi3 opsweep", cmd_spi3_opsweep, 0,
          "spi3 opsweep [a b [len]]        Sweep read opcodes (BSRAM_1/2 hunt, Step 0b)\r\n"),
    CMD_A("spi3 armtest", cmd_spi3_armtest, SC_SPI3,
          "spi3 armtest [pb11|pc6]         Pulse FPGA run/re-arm pin, re-cfg, acqread\r\n"),
    CMD_V("spi3 gowin", cmd_spi3_gowin, SC_EXACT | SC_SPI3,
          "spi3 gowin                      Read+decode Gowin ID/USERCODE/STATUS regs\r\n"),
    CMD_A("spi3 scopetest", cmd_spi3_scopetest, SC_SPI3,
          "spi3 scopetest [bank]           Full scope seq: USART cfg->PC0->0x04/05 read\r\n"),
    CMD_V("spi3 acqtest", cmd_spi3_acqtest, SC_EXACT | SC_SPI3,
          "spi3 acqtest                    Decomposer Phase 20 validation test\r\n"),
    CMD_V("spi3 stock-readback", cmd_spi3_stock_readback, SC_EXACT | SC_SPI3,
          "spi3 stock-readback             Stock case-8 SPI3 readback; not DMM proof\r\n"),
    CMD_V("spi3 h2txdiag", cmd_spi3_h2txdiag, SC_EXACT | SC_SPI3,
          "spi3 h2txdiag                   Replay H2 TX + sample MISO; no ACK/apply proof\r\n"),
    CMD_V("spi3 h2verify", cmd_spi3_h2txdiag, SC_EXACT | SC_SPI3,
          NULL /* alias */),
    CMD_V("spi3 probe", cmd_spi3_probe, SC_EXACT | SC_SPI3,
          "spi3 probe                      Bit-bang MISO probe; pokes SPI3 registers directly\r\n"),
    CMD_V("uptime", cmd_uptime, SC_EXACT,
          "uptime                          Show uptime\r\n"),
    { NULL, NULL, NULL, 0, NULL }   /* terminator */
};

static void cmd_help(void)
{
    usb_send_str("\r\n=== OpenScope 2C53T Debug Shell ===\r\n");
    for (const shell_cmd_t *c = shell_cmds; c->name != NULL; c++) {
        if (c->help != NULL)
            usb_send_str(c->help);
    }
    usb_send_str("\r\n");
}

static void dispatch_command(char *line)
{
    /* Strip trailing \r\n */
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n'))
        line[--len] = '\0';

    if (len == 0) return;

    /* Longest whole-word-prefix match over the table (see table comment). */
    const shell_cmd_t *best = NULL;
    size_t best_len = 0;
    for (const shell_cmd_t *c = shell_cmds; c->name != NULL; c++) {
        size_t n = strlen(c->name);
        if (strncmp(line, c->name, n) != 0)
            continue;
        char next = line[n];
        if (next != '\0' && next != ' ' && next != '\t')
            continue;                     /* word boundary required */
        if ((c->flags & SC_EXACT) && next != '\0')
            continue;                     /* takes no arguments */
        if (n > best_len) {
            best = c;
            best_len = n;
        }
    }

    if (best == NULL) {
        usb_debug_printf("Unknown command: '%s'  (type 'help')\r\n", line);
        return;
    }

    const char *args = line + best_len;
    while (*args == ' ' || *args == '\t') args++;

    if ((best->flags & SC_NEEDARGS) && *args == '\0') {
        usb_send_str("Missing arguments. Usage:\r\n");
        if (best->help != NULL)
            usb_send_str(best->help);
        return;
    }

    if (best->flags & SC_SPI3) {
        if (!spi3_shell_claim())
            return;
        if (best->fn_args) best->fn_args(args); else best->fn_void();
        fpga_acq_resume();
        return;
    }

    if (best->fn_args) best->fn_args(args); else best->fn_void();
}

/* ═══════════════════════════════════════════════════════════════════
 * FreeRTOS Task — USB Debug Shell
 * ═══════════════════════════════════════════════════════════════════ */

#define CMD_BUF_SIZE 128

/* Line accumulator shared by both transports. `echo` is on for USB (a raw
 * serial terminal shows nothing otherwise) and off for RTT, where the host
 * telnet client is line-buffered and echoes locally — echoing there would
 * double every character. */
static char cmd_buf[CMD_BUF_SIZE];
static int  cmd_pos = 0;

static void shell_feed(const uint8_t *bytes, uint16_t len, bool echo)
{
    for (uint16_t i = 0; i < len; i++) {
        char c = (char)bytes[i];

        if (c == '\r' || c == '\n') {
            if (echo) usb_send_str("\r\n");
            cmd_buf[cmd_pos] = '\0';
            if (cmd_pos > 0) {
                dispatch_command(cmd_buf);
            }
            cmd_pos = 0;
            usb_send_str("> ");
        } else if (c == '\b' || c == 0x7F) {
            if (cmd_pos > 0) {
                cmd_pos--;
                if (echo) usb_send_str("\b \b");
            }
        } else if (c >= ' ' && cmd_pos < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_pos++] = c;
            if (echo) usb_send_bytes((const uint8_t *)&c, 1);
        }
    }
}

static const char shell_banner[] =
    "\r\n\r\n"
    "+----------------------------------+\r\n"
    "|  OpenScope 2C53T Debug Shell     |\r\n"
    "|  Type 'help' for commands        |\r\n"
    "+----------------------------------+\r\n"
    "\r\n> ";

/* Instrumentation for the 2026-08-11 "shell task never polls RTT" hunt.
 * Non-static so the addresses come out of the ELF with nm and can be read over
 * SWD on a running target — no console needed, which is the whole problem.
 *   dbg_shell_entered == 0  -> the task never ran; look at task creation
 *   dbg_shell_loops   == 0  -> it started but never completed an iteration
 *   dbg_shell_loops growing -> the loop is fine and rtt_read() is the fault */
volatile uint32_t dbg_shell_entered;
volatile uint32_t dbg_shell_loops;
volatile uint32_t dbg_shell_rtt_bytes;

static void vUsbDebugTask(void *pvParameters)
{
    (void)pvParameters;

    dbg_shell_entered = 0xA5A5A5A5u;

    uint8_t rx_buf[USBD_CDC_OUT_MAXPACKET_SIZE];
    uint8_t rtt_buf[64];
    bool usb_banner_sent = false;
    bool rtt_banner_sent = false;
    uint32_t usb_settle = 0;

    for (;;) {
        bool did_work = false;
        dbg_shell_loops++;

#if defined(FAULT_SELFTEST) && FAULT_SELFTEST
        /* Deliberate, self-inflicted fault ~10 s after boot, from a task (so the
         * frame lands on the PSP exactly like the fault under investigation).
         *
         * Two questions at once:
         *  1. does the fault handler actually record anything? If this writes a
         *     valid g_fault, the handler and vector wiring are sound and the
         *     ~55 s fault is special in some way (most likely an unusable stack
         *     frame). If it records nothing, the instrument itself is broken.
         *  2. does the device freeze at ~10 s WITH NO DEBUGGER ATTACHED? The UI
         *     stopping on its own is proof the fault is real and not something
         *     the OpenOCD attach provokes — which is the confound that makes the
         *     whole "~55 s HardFault" reading suspect.
         *
         * udf #0 is the permanently-undefined encoding: guaranteed UNDEFINSTR
         * -> UsageFault, no memory access involved, nothing ambiguous. */
        if (dbg_shell_loops == 1000u) {
            __asm volatile ("udf #0");
        }
#endif

        /* ---- RTT (SWD) transport ---- */
        if (!rtt_banner_sent && rtt_host_attached()) {
            usb_send_str(shell_banner);
            rtt_banner_sent = true;
        }

        size_t rtt_len = rtt_read(rtt_buf, sizeof(rtt_buf));
        dbg_shell_rtt_bytes += (uint32_t)rtt_len;
        if (rtt_len > 0) {
            /* Attaching mid-session: the host may never have moved read_pos
             * before typing, so print the banner on first input too. */
            if (!rtt_banner_sent) {
                usb_send_str(shell_banner);
                rtt_banner_sent = true;
            }
            shell_feed(rtt_buf, (uint16_t)rtt_len, false);
            did_work = true;
        }

        /* ---- USB CDC transport ---- */
#if !DEBUG_SHELL_RTT_ONLY
        if (usb_debug_connected()) {
            if (!usb_banner_sent) {
                /* Let the host finish enumerating before the first write. */
                if (usb_settle < 50) {
                    usb_settle++;
                } else {
                    usb_send_str(shell_banner);
                    usb_banner_sent = true;
                }
            } else {
                uint16_t rx_len = usb_vcp_get_rxdata(&usb_core_dev, rx_buf);
                if (rx_len > 0) {
                    shell_feed(rx_buf, rx_len, true);
                    did_work = true;
                }
            }
        } else {
            usb_banner_sent = false;
            usb_settle = 0;
        }
#else
        (void)rx_buf; (void)usb_banner_sent; (void)usb_settle;
#endif

        if (!did_work) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void usb_debug_create_task(void)
{
#ifndef EMULATOR_BUILD
    xTaskCreate(vUsbDebugTask, "usb_dbg", 768, NULL, 2, NULL);
#endif
}
