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
#include "rtt.h"

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
#include "meter_data.h"
#include "ui.h"
#include "../ui/scope_state.h"
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
    fpga.spi3_ok_count = 0;
    fpga.spi3_timeout_count = 0;
    fpga.spi3_total_timeouts = 0;
    fpga.rx_frame_valid = false;
    memset((void *)fpga.rx_frame, 0, sizeof(fpga.rx_frame));
    memset((void *)fpga.last_tx_frame, 0, sizeof(fpga.last_tx_frame));
    memset((void *)fpga.tx_frame_history, 0, sizeof(fpga.tx_frame_history));
    fpga.tx_frame_history_head = 0;
    fpga.tx_frame_history_count = 0;
    memset((void *)fpga.diag_ch1_raw, 0, sizeof(fpga.diag_ch1_raw));
    memset((void *)fpga.diag_ch2_raw, 0, sizeof(fpga.diag_ch2_raw));
    fpga.diag_data_varies = 0;
    taskEXIT_CRITICAL();
    fpga_stock_diag_reset();
}

static void fpga_stock_diag_print(void)
{
    usb_debug_printf(
        "\r\n=== Stock Shadow ===\r\n"
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

static void cmd_help(void)
{
    usb_send_str(
        "\r\n=== OpenScope 2C53T Debug Shell ===\r\n"
        "help                            Show this help\r\n"
        "version                         Firmware info\r\n"
        "status                          FPGA & system status\r\n"
        "usart tx <cmd_hi> <cmd_lo>      Send FPGA command (queued)\r\n"
        "usart raw <10 hex bytes>       Send raw 10-byte USART frame\r\n"
        "  e.g.: usart raw 00 00 00 0B 01 00 00 00 00 0B\r\n"
        "gpio set <port><pin> <0|1>      Set GPIO pin\r\n"
        "  e.g.: gpio set B11 1\r\n"
        "gpio read <port><pin>           Read GPIO pin\r\n"
        "gpio scan                       Scan FPGA-related pins\r\n"
        "mem read <addr> [count]         Read 32-bit words\r\n"
        "  e.g.: mem read 0x40021000 4\r\n"
        "mem write <addr> <value>        Write 32-bit word\r\n"
        "flash jedec                     Read external W25Q128 JEDEC ID\r\n"
        "flash read <addr> <len>         Read external flash bytes (max 256)\r\n"
        "flash dump <addr> <len>         Stream external flash bytes (max 4096)\r\n"
        "flash wtest <addr> CONFIRM      Non-destructive write-primitive self-test (blank 4KB sector)\r\n"
        "trig raw <0-4095>               Write DAC1 (PA4) directly + sw trigger\r\n"
        "trig <range> <level>            Scope trigger DAC: range 0-9, level -100..100\r\n"
        "screen dump [shadow] [x y w h]  Dump 4-bit software LCD shadow\r\n"
        "screen shadow page [y]          Set/clear 16-row shadow capture page\r\n"
        "fpga cmd <hi> <lo>              Send FPGA command bytes\r\n"
        "  e.g.: fpga cmd 0 9   (sends 0x00 0x09)\r\n"
        "        fpga cmd 0x0509 (sends 0x05 0x09)\r\n"
        "fpga frame <hi> <lo> [p1..p5 [ck]]  Build/send full 10-byte frame\r\n"
        "  e.g.: fpga frame 00 0B 01 00 00 00 00\r\n"
        "fpga diag clear                 Clear FPGA bench counters/state\r\n"
        "fpga stock diag                Show stock-state bench shadow\r\n"
        "fpga stock clear               Reset stock-state bench shadow\r\n"
        "fpga stock set <9 bytes>       Set F68/F69/F6A/F6B/E1A/E1B/E1C/E1D/355\r\n"
        "fpga stock preset <4|5 bytes>  Set F68/F69/F6A/F6B [355]\r\n"
        "fpga stock base2               Seed visible state 2 scope posture\r\n"
        "fpga stock state5 [E1B] [E1D]  Seed visible state 5 editor posture\r\n"
        "fpga stock state6 [E1B] [E1D]  Seed visible state 6 pre-entry posture\r\n"
        "fpga stock prev                Drive stock-like adjust-prev family\r\n"
        "fpga stock next                Drive stock-like adjust-next family\r\n"
        "fpga stock select              Stage single detail selection\r\n"
        "fpga stock toggle              Toggle staged detail bitmap\r\n"
        "fpga stock commit              Walk E1C 0->2->1->0x2B commit path\r\n"
        "fpga stock consume             Consume packed state-9 preset path\r\n"
        "fpga stock bridge fixed        Probe post-13/14 fixed 0x0501 path\r\n"
        "fpga stock bridge dynamic [ch1|ch2|both]  Probe post-13/14 0x050x path\r\n"
        "fpga stock reenter             Re-enter scope path with staged shadow\r\n"
        "fpga wire words <w...>         Send final 16-bit wire words directly\r\n"
        "fpga wire entry [ch1|ch2|both] Send candidate scope-entry wire-word bank\r\n"
        "fpga wire scope [ch1|ch2|both] Wire-word entry + runtime scope blocks\r\n"
        "fpga scope reinit               Re-apply scope frontend + FPGA cfg\r\n"
        "fpga meter reinit [submode]     Re-apply meter frontend + FPGA cfg\r\n"
        "fpga scope wake                 Meter wake preamble then scope cfg\r\n"
        "fpga scope acqmode              Send stock-like 0x20/0x21 block\r\n"
        "fpga scope beat [count] [ms]    Send stock-like cmd-3 heartbeat(s)\r\n"
        "fpga scope entry <8 bytes>      Reset + send 0x01,0B..11 params\r\n"
        "fpga scope timing <5 bytes>     Send 0x20,0x21,0x26..0x28 params\r\n"
        "fpga scope trig <4 bytes>       Send 0x07/0x0A,0x16..0x19\r\n"
        "mode meter [submode] [layout]   Switch UI + FPGA to DMM frontend\r\n"
        "mode scope                      Switch UI + FPGA to scope frontend\r\n"
        "mode startup [scope|meter]      Get/set Settings > Startup on Boot\r\n"
        "meter dump [delay_ms]           Show parsed DMM/UI/raw frame state\r\n"
        "meter frontend                  Show DMM analog frontend GPIO state\r\n"
        "meter mux-stream [count] [ms]   Stream DMM frames plus frontend GPIOs\r\n"
        "meter stream [count] [delay_ms] Print compact DMM frame stream\r\n"
        "meter adc-snapshot              Show read-only DMM waveform sampler state\r\n"
        "ui dump                         Show current UI mode/redraw state\r\n"
        "meter wave                      Show DMM voltage waveform sample stats\r\n"
        "meter wave reset                Reset DMM waveform diagnostics\r\n"
        "meter wave path [direct|preacq] Get/set DMM waveform SPI path\r\n"
        "meter wave selector [auto|0|1]  Override DMM waveform selector byte\r\n"
        "fpga acq [mode]                 Trigger SPI3 acquisition\r\n"
        "spi3 read [len]                 Raw SPI3 read + hex dump\r\n"
        "spi3 xfer <hex...>              Send arbitrary MOSI bytes, dump MISO\r\n"
        "spi3 seq <b..> | <b..>          xfer w/ mid-sequence CS pulse at '|'\r\n"
        "fpga reinit [br][gap][close][f0|1|2][u<ms>][a-e<pin>][s2|sd[h|l]][tc<n>] Replay cfg\r\n"
        "    f=prelude frame: 0 split(stock) 1 combined 2 merge15+3B; u=pre-upload gap; k<br>=cmd-phase clk div; tc<n>=trailing clocks\r\n"
        "    pe=probe SYSTEM_EDIT_MODE after 0x15 (STATUS@/256); rl=send 0x3C RELOAD before prelude; reports 0x41 STATUS\r\n"
        "spi3 acqread                    Read CH1/CH2 via real 0x04/0x05 protocol\r\n"
        "spi3 armtest [pb11|pc6]         Pulse FPGA run/re-arm pin, re-cfg, acqread\r\n"
        "spi3 gowin                      Read+decode Gowin ID/USERCODE/STATUS regs\r\n"
        "spi3 scopetest [bank]           Full scope seq: USART cfg->PC0->0x04/05 read\r\n"
        "spi3 acqtest                    Decomposer Phase 20 validation test\r\n"
        "spi3 h2verify                   Re-upload H2 + capture FPGA responses\r\n"
        "reboot bootloader               Reboot into USB HID updater\r\n"
        "uptime                          Show uptime\r\n"
        "\r\n"
    );
}

static void cmd_version(void)
{
    usb_debug_printf(
        "OpenScope 2C53T\r\n"
        "Build: " __DATE__ " " __TIME__ "\r\n"
        "MCU: AT32F403A @ %uMHz\r\n"
        "SRAM: 224KB (EOPB0=0xFE)\r\n",
        system_core_clock / 1000000
    );
}

static void cmd_status(void)
{
    extern volatile uint32_t uptime_seconds;

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
        "SPI3 OK: %u\r\n"
        "SPI3 timeouts: %u (total %u)\r\n"
        "SPI3 first byte: 0x%02X\r\n",
        (unsigned long)uptime_seconds,
        system_core_clock / 1000000,
        fpga.initialized ? "YES" : "NO",
        fpga.spi3_active ? "YES" : "NO",
        fpga.tx_count,
        fpga.rx_byte_count,
        fpga.frame_count,
        fpga.echo_count,
        fpga.spi3_ok_count,
        fpga.spi3_timeout_count, fpga.spi3_total_timeouts,
        fpga.spi3_first_byte
    );
    usb_print_last_tx_frame();
    usb_print_recent_tx_frames();

    /* Split FPGA diag into separate printf to avoid buffer overflow */
    usb_debug_printf(
        "\r\n=== FPGA Diag ===\r\n"
        "IOMUX remap: 0x%08lX (init)\r\n"
        "IOMUX remap5: 0x%08lX (init)\r\n"
        "IOMUX remap LIVE: 0x%08lX\r\n"
        "IOMUX remap5 LIVE: 0x%08lX\r\n"
        "SPI3 CTRL1: 0x%04lX  STS: 0x%04lX\r\n"
        "PB4(MISO) IDT: %d  PC6(EN): %d  PB6(CS): %d\r\n",
        fpga.diag_remap5,
        fpga.diag_remap7,
        (unsigned long)IOMUX->remap,
        (unsigned long)IOMUX->remap5,
        fpga.diag_spi_ctrl1,
        fpga.diag_spi_sts,
        (GPIOB->idt & (1 << 4)) ? 1 : 0,
        (GPIOC->idt & (1 << 6)) ? 1 : 0,
        (GPIOB->idt & (1 << 6)) ? 1 : 0
    );

    usb_debug_printf(
        "\r\n=== SPI3 Handshake (11 bytes) ===\r\n"
        "G1: %02X %02X %02X %02X  G2: %02X %02X %02X\r\n"
        "G3: %02X %02X %02X %02X  Probe: %02X\r\n"
        "BB: idle=%02X cs=%02X byte=%02X marker=%02X\r\n",
        fpga.init_hs[0], fpga.init_hs[1], fpga.init_hs[2], fpga.init_hs[3],
        fpga.init_hs[4], fpga.init_hs[5], fpga.init_hs[6],
        fpga.init_hs[7], fpga.init_hs[8], fpga.init_hs[9], fpga.init_hs[10],
        fpga.init_hs[11],
        fpga.bb_idle, fpga.bb_cs, fpga.bb_byte, fpga.bb_marker
    );

    usb_debug_printf(
        "\r\n=== H2 Bitstream Upload ===\r\n"
        "Bytes sent: %lu / 115638\r\n"
        "Upload done: %s\r\n"
        "0x3A close status: %02X (stock: F8)\r\n"
        "0x03 scope status: %02X %02X %02X %02X (stock: 00 01 42 2E)\r\n",
        fpga.h2_bytes_sent,
        fpga.h2_upload_done ? "YES" : "NO",
        fpga.h2_close_status,
        fpga.scope_status[0], fpga.scope_status[1],
        fpga.scope_status[2], fpga.scope_status[3]
    );

    fpga_stock_diag_print();

    /* Show last RX frame if valid */
    if (fpga.rx_frame_valid) {
        usb_debug_printf("Last RX frame:");
        for (int i = 0; i < FPGA_RX_FRAME_SIZE; i++)
            usb_debug_printf(" %02X", fpga.rx_frame[i]);
        usb_send_str("\r\n");
    }
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

    /* Use fpga_send_cmd for the standard 2-byte command path */
    BaseType_t ok = fpga_send_cmd(bytes[0], bytes[1]);
    usb_debug_printf("TX [%02X %02X]: %s\r\n",
                     bytes[0], bytes[1],
                     ok == pdTRUE ? "queued" : "FULL");

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

    if (val)
        port->scr = pin;    /* Set */
    else
        port->clr = pin;    /* Clear */

    usb_debug_printf("P%c%d -> %s\r\n",
                     (int)('A' + ((uint32_t)port - (uint32_t)GPIOA) / 0x400),
                     __builtin_ctz(pin),
                     val ? "HIGH" : "LOW");
}

static void cmd_gpio_read(const char *args)
{
    gpio_type *port;
    uint16_t pin;

    if (parse_gpio(args, &port, &pin) != 0) {
        usb_send_str("ERR: bad pin (e.g. A7, B11, C6)\r\n");
        return;
    }

    uint32_t val = (port->idt & pin) ? 1 : 0;
    usb_debug_printf("P%c%d = %lu\r\n",
                     (int)('A' + ((uint32_t)port - (uint32_t)GPIOA) / 0x400),
                     __builtin_ctz(pin),
                     val);
}

static void cmd_gpio_scan(void)
{
    usb_send_str("=== FPGA Control Pins ===\r\n");
    usb_debug_printf("PC6  (SPI enable):  %d\r\n", (GPIOC->idt & (1 << 6))  ? 1 : 0);
    usb_debug_printf("PB11 (active mode): %d\r\n", (GPIOB->idt & (1 << 11)) ? 1 : 0);
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
        x + w > LCD_WIDTH || y + h > LCD_HEIGHT) {
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
    BaseType_t ok = fpga_send_cmd((uint8_t)cmd_hi, (uint8_t)cmd_lo);
    usb_debug_printf("FPGA cmd %02lX %02lX: %s\r\n",
                     cmd_hi, cmd_lo,
                     ok == pdTRUE ? "queued" : "FULL");

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

static void cmd_fpga_scope_reinit(void)
{
    fpga_request_scope_reinit();
    usb_send_str("Scope reinit queued\r\n");
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

    usb_debug_printf("CH1 buffer (%lu bytes):\r\n", len);
    for (uint32_t i = 0; i < len; i++) {
        if (i % 16 == 0) usb_debug_printf("%04lX:", i);
        usb_debug_printf(" %02X", ch1[i]);
        if (i % 16 == 15 || i == len - 1) usb_send_str("\r\n");
    }
}

static int32_t scaled_i100(float value)
{
    float scaled = value * 100.0f;
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
        fpga_meter_reinit((uint8_t)submode);
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

    if (args && *args) {
        if (parse_int(args, &delay) != 0 || delay > 5000) {
            usb_send_str("Usage: meter dump [delay_ms<=5000]\r\n");
            return;
        }
    }
    if (delay > 0) vTaskDelay(pdMS_TO_TICKS(delay));

    bool live = meter_reading.valid && meter_reading.submode == meter_submode;

    usb_send_str("=== DMM State ===\r\n");
    usb_debug_printf("mode=%lu startup=%s meter_submode=%u (%s) layout=%u (%s)\r\n",
                     (uint32_t)current_mode,
                     startup_mode_name(startup_mode),
                     (unsigned)meter_submode,
                     meter_submode_name(meter_submode),
                     (unsigned)meter_layout,
                     meter_layout_name(meter_layout));
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
                     meter_reading.valid ? 1U : 0U,
                     (unsigned)meter_reading.submode,
                     (unsigned)meter_reading.result_class,
                     meter_reading.update_count,
                     live ? meter_reading.display_str : "---",
                     (live && meter_reading.unit_suffix) ? meter_reading.unit_suffix : "");
    usb_debug_printf("transition_discard_remaining=%u\r\n",
                     (unsigned)meter_frame_discard_count);
    usb_debug_printf("raw_bcd=%d decimal_pos=%u negative=%u unit_variant=%u bar_i100=%ld aux_freq_i10=%ld\r\n",
                     meter_reading.raw_bcd,
                     (unsigned)meter_reading.decimal_pos,
                     meter_reading.negative ? 1U : 0U,
                     (unsigned)meter_reading.unit_variant,
                     (long)scaled_i100(meter_reading.bar_fraction),
                     (long)scaled_i100(meter_reading.aux_freq_hz) / 10L);
    usb_debug_printf("flags ac=%u auto=%u hold=%u probe=%u range_ind=%u range_cmd=%u beep=%u\r\n",
                     meter_reading.is_ac ? 1U : 0U,
                     meter_reading.is_auto_range ? 1U : 0U,
                     meter_reading.is_hold ? 1U : 0U,
                     (unsigned)meter_reading.probe_type,
                     (unsigned)meter_reading.range_indicator,
                     (unsigned)meter_reading.range_cmd,
                     meter_reading.continuity_beep ? 1U : 0U);
    usb_debug_printf("stock_fsm mode=%u variant=%u format=%u dc_state=%u display_cmd=%u unit_index=%u composite=%u\r\n",
                     (unsigned)meter_reading.stock_mode,
                     (unsigned)meter_reading.stock_variant,
                     (unsigned)meter_reading.stock_format,
                     (unsigned)meter_reading.stock_dc_state,
                     (unsigned)meter_reading.stock_display_cmd,
                     (unsigned)meter_reading.stock_unit_index,
                     (unsigned)meter_reading.stock_composite_index);
    usb_send_str("frame=");
    for (int i = 0; i < 12; i++) usb_debug_printf("%02X%s", meter_reading.dbg_frame[i], i == 11 ? "" : " ");
    usb_send_str("\r\nnibbles=");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X%s", meter_reading.dbg_nibbles[i], i == 3 ? "" : " ");
    usb_send_str(" raw_digits=");
    for (int i = 0; i < 4; i++) usb_debug_printf("%02X%s", meter_reading.dbg_raw_digits[i], i == 3 ? "" : " ");
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
                         "f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%u digits=%02X,%02X,%02X,%02X\r\n",
                         (unsigned)n,
                         h->update_count,
                         (unsigned)h->submode,
                         (unsigned)h->result_class,
                         h->raw_bcd,
                         (unsigned)h->decimal_pos,
                         h->unit_suffix ? h->unit_suffix : "",
                         h->display_str,
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

static void cmd_meter_frontend(void)
{
    uint16_t extra = meter_dbg_extra();
    fpga_meter_selector_t selectors = fpga_meter_expected_selectors(meter_submode);
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
    usb_debug_printf("mode_sequence count=%u submode=%u selector=%04X apply=%04X probe=%04X start=%04X\r\n",
                     (unsigned)fpga.meter_mode_sequence_count,
                     (unsigned)fpga.meter_mode_sequence_submode,
                     (unsigned)fpga.meter_mode_selector_word,
                     (unsigned)fpga.meter_mode_apply_word,
                     (unsigned)fpga.meter_mode_probe_word,
                     (unsigned)fpga.meter_mode_start_word);
    usb_debug_printf("display=%s unit=%s raw_bcd=%d dp=%u f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%ld beep=%u discard=%u\r\n",
                     meter_reading.valid ? meter_reading.display_str : "---",
                     (meter_reading.valid && meter_reading.unit_suffix) ? meter_reading.unit_suffix : "",
                     meter_reading.raw_bcd,
                     (unsigned)meter_reading.decimal_pos,
                     (unsigned)meter_reading.dbg_frame[6],
                     (unsigned)meter_reading.dbg_frame[7],
                     (unsigned)meter_reading.dbg_frame[8],
                     (unsigned)meter_reading.dbg_frame[9],
                     (unsigned)extra,
                     (long)scaled_i100(meter_reading.aux_freq_hz) / 10L,
                     meter_reading.continuity_beep ? 1U : 0U,
                     (unsigned)meter_frame_discard_count);
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
    usb_debug_printf("control PC6_spi=%u PB11_active=%u PC11_meter_mux=%u PC7_probe=%u PC0_ready=%u\r\n",
                     gpio_level(GPIOC, 6),
                     gpio_level(GPIOB, 11),
                     gpio_level(GPIOC, 11),
                     gpio_level(GPIOC, 7),
                     gpio_level(GPIOC, 0));
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

static void print_meter_mux_stream_line(uint32_t index)
{
    bool live = current_mode == MODE_MULTIMETER &&
                meter_reading.valid &&
                meter_reading.submode == meter_submode;
    fpga_meter_selector_t selectors = fpga_meter_expected_selectors(meter_submode);

    usb_debug_printf("t=%lu upd=%lu ui_sub=%u rd_sub=%u live=%u cls=%u "
                     "stock_mode=%u raw_low=%02X "
                     "seq=%u seq_sub=%u seq_word=%04X seq_apply=%04X "
                     "disp=%s unit=%s raw=%d dp=%u f6=%02X f7=%02X f8=%02X f9=%02X "
                     "extra=%04X discard=%u PC6=%u PB11=%u PC11=%u PC12=%u "
                     "PE4=%u PE5=%u PE6=%u PA15=%u PA10=%u PB10=%u PB9=%u PA6=%u\r\n",
                     index,
                     meter_reading.update_count,
                     (unsigned)meter_submode,
                     (unsigned)meter_reading.submode,
                     live ? 1U : 0U,
                     (unsigned)meter_reading.result_class,
                     (unsigned)selectors.function_selector,
                     (unsigned)selectors.range_selector,
                     (unsigned)fpga.meter_mode_sequence_count,
                     (unsigned)fpga.meter_mode_sequence_submode,
                     (unsigned)fpga.meter_mode_selector_word,
                     (unsigned)fpga.meter_mode_apply_word,
                     meter_reading.valid ? meter_reading.display_str : "---",
                     (meter_reading.valid && meter_reading.unit_suffix) ? meter_reading.unit_suffix : "",
                     meter_reading.raw_bcd,
                     (unsigned)meter_reading.decimal_pos,
                     (unsigned)meter_reading.dbg_frame[6],
                     (unsigned)meter_reading.dbg_frame[7],
                     (unsigned)meter_reading.dbg_frame[8],
                     (unsigned)meter_reading.dbg_frame[9],
                     (unsigned)meter_dbg_extra(),
                     (unsigned)meter_frame_discard_count,
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

static void cmd_meter_stream(const char *args)
{
    uint32_t count = 16;
    uint32_t delay_ms = 250;
    uint32_t last_update = 0xFFFFFFFFu;
    const char *usage = "Usage: meter stream [count<=200] [delay_ms<=5000]\r\n";

    if (parse_stream_args(args, &count, &delay_ms, usage) != 0) return;

    usb_debug_printf("stream count=%lu delay_ms=%lu\r\n", count, delay_ms);
    for (uint32_t i = 0; i < count; i++) {
        if (meter_reading.update_count != last_update) {
            last_update = meter_reading.update_count;
            uint16_t extra = ((uint16_t)meter_reading.dbg_frame[10] << 8) |
                             meter_reading.dbg_frame[11];
            usb_debug_printf("%lu upd=%lu sub=%u cls=%u raw=%d dp=%u unit=%s disp=%s "
                             "f6=%02X f7=%02X f8=%02X f9=%02X extra=%04X aux_freq_i10=%ld wave=%lu beep=%u\r\n",
                             i,
                             meter_reading.update_count,
                             (unsigned)meter_reading.submode,
                             (unsigned)meter_reading.result_class,
                             meter_reading.raw_bcd,
                             (unsigned)meter_reading.decimal_pos,
                             meter_reading.unit_suffix ? meter_reading.unit_suffix : "",
                             meter_reading.display_str,
                             (unsigned)meter_reading.dbg_frame[6],
                             (unsigned)meter_reading.dbg_frame[7],
                             (unsigned)meter_reading.dbg_frame[8],
                             (unsigned)meter_reading.dbg_frame[9],
                             (unsigned)extra,
                             (long)scaled_i100(meter_reading.aux_freq_hz) / 10L,
                             meter_voltage_wave_sample_count(),
                             meter_reading.continuity_beep ? 1U : 0U);
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
    usb_debug_printf("spi_path=%s selector_override=%d last_preacq=%02X last_selector=%02X "
                     "last_sample=%02X min_sample=%02X max_sample=%02X samples=%lu ff_samples=%lu "
                     "zero_samples=%lu enq_attempts=%lu enq_success=%lu enq_drops=%lu\r\n",
                     fpga_meter_adc_use_preacq ? "preacq" : "direct",
                     (int)fpga_meter_adc_selector_override,
                     (unsigned)fpga_meter_adc_last_preacq,
                     (unsigned)fpga_meter_adc_last_selector,
                     (unsigned)fpga_meter_adc_last_sample,
                     (unsigned)fpga_meter_adc_min_sample,
                     (unsigned)fpga_meter_adc_max_sample,
                     fpga_meter_adc_samples,
                     fpga_meter_adc_ff_samples,
                     fpga_meter_adc_zero_samples,
                     fpga_meter_adc_enqueue_attempts,
                     fpga_meter_adc_enqueue_success,
                     fpga_meter_adc_enqueue_drops);
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
    usb_debug_printf("spi_path=%s enq=%lu ok=%lu drop=%lu samples=%lu ff=%lu zero=%lu "
                     "selector_mode=%d last_pre=%02X selector=%02X last=%02X min=%02X max=%02X\r\n",
                     fpga_meter_adc_use_preacq ? "preacq" : "direct",
                     fpga_meter_adc_enqueue_attempts,
                     fpga_meter_adc_enqueue_success,
                     fpga_meter_adc_enqueue_drops,
                     fpga_meter_adc_samples,
                     fpga_meter_adc_ff_samples,
                     fpga_meter_adc_zero_samples,
                     (int)fpga_meter_adc_selector_override,
                     (unsigned)fpga_meter_adc_last_preacq,
                     (unsigned)fpga_meter_adc_last_selector,
                     (unsigned)fpga_meter_adc_last_sample,
                     (unsigned)fpga_meter_adc_min_sample,
                     (unsigned)fpga_meter_adc_max_sample);
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
        while (*value == ' ' || *value == '\t') value++;

        if (*value == '\0') {
            usb_debug_printf("meter wave selector=%d\r\n",
                             (int)fpga_meter_adc_selector_override);
        } else if (strcmp(value, "auto") == 0) {
            fpga_meter_adc_selector_override = -1;
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_send_str("meter wave selector=auto\r\n");
        } else if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0) {
            fpga_meter_adc_selector_override = (int8_t)(value[0] - '0');
            meter_voltage_wave_reset();
            fpga_meter_adc_diag_reset();
            usb_debug_printf("meter wave selector=%d\r\n",
                             (int)fpga_meter_adc_selector_override);
        } else {
            usb_send_str("Usage: meter wave selector [auto|0|1]\r\n");
        }
        return;
    }

    usb_send_str("Usage: meter wave [reset|path [direct|preacq]|selector [auto|0|1]]\r\n");
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
    usb_debug_printf("H2 done: %d  bytes: %lu\r\n", fpga.h2_upload_done, fpga.h2_bytes_sent);

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
 * H2 Upload Verification — Re-upload with response capture
 *
 * Re-sends the 115,638-byte H2 cal table while capturing the FPGA's
 * simultaneous responses at key points. If the FPGA is actually
 * accepting the data, we might see non-FF responses (ACK bytes,
 * status changes, or block-boundary markers).
 * ═══════════════════════════════════════════════════════════════════ */
static void cmd_spi3_h2verify(void)
{
    usb_send_str("=== H2 Upload Verification ===\r\n\r\n");

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

    usb_debug_printf("\r\nTotal non-FF during upload: %d / 115638\r\n", total_nonff);
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
 * flat line (feed the siggen into CH1 for a known signal). Read-only probe;
 * does not touch the acquisition task. */
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

        usb_debug_printf("\r\n[clk %s]\r\n", SPEEDS[s].label);
        usb_debug_printf("IDCODE  (0x11): 0x%08lX  %s\r\n", id,
                         id == 0x0120681BUL ? "== GW1N-2 OK (read path works!)"
                                            : "!= 0x0120681B");
        usb_debug_printf("USERCODE(0x13): 0x%08lX\r\n", usr);
        usb_debug_printf("STATUS  (0x41): 0x%08lX\r\n", st);
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

    /* Restore the original CTRL1 (baud + SPE) for the rest of the system. */
    {
        volatile uint32_t *ctrl1 = (volatile uint32_t *)0x40003C00;
        *ctrl1 &= ~(1u << 6);
        *ctrl1 = saved;
    }

    usb_send_str("\r\nHealthy running FPGA expects IDCODE 0x0120681B + Done Final/VLD/Ready.\r\n"
                 "If slow clock reads it but fast doesn't -> SSPI read path is clock-limited.\r\n");
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

/* fpga reinit [br] [prelude_gap_ms] [post_close_ms] — replay the full SPI3
 * config handshake on demand (prelude → 0x3B bitstream → 0x3A close → scope
 * config) and report the result. Lets us sweep the handshake parameters in
 * seconds without reflashing, chasing why our upload activates the FPGA slave
 * but never reaches stock's configured state (close F8 / status 00 01 42 2E).
 * Defaults match the stock-captured timing: br=0 (/2), gap=100ms, close=600ms. */
static void cmd_fpga_reinit(const char *args)
{
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
                                         * (default HIGH = stock). GPIO-audit lead. */
                uint8_t lvl = (tk[2] == 'l') ? 2 : 1;
                if (tk[1] == '2')      opt.strap_pd2    = lvl;
                else if (tk[1] == 'd') opt.strap_pd1213 = lvl;
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
 * Command Dispatcher
 * ═══════════════════════════════════════════════════════════════════ */

static void dispatch_command(char *line)
{
    /* Strip trailing \r\n */
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n'))
        line[--len] = '\0';

    if (len == 0) return;

    /* Match command and dispatch */
    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        cmd_help();
    } else if (strcmp(line, "version") == 0) {
        cmd_version();
    } else if (strcmp(line, "status") == 0) {
        cmd_status();
    } else if (strncmp(line, "usart raw ", 10) == 0) {
        cmd_usart_raw(line + 10);
    } else if (strncmp(line, "usart tx ", 9) == 0) {
        cmd_usart_tx(line + 9);
    } else if (strncmp(line, "gpio set ", 9) == 0) {
        cmd_gpio_set(line + 9);
    } else if (strncmp(line, "gpio read ", 10) == 0) {
        cmd_gpio_read(line + 10);
    } else if (strcmp(line, "gpio scan") == 0) {
        cmd_gpio_scan();
    } else if (strncmp(line, "mem read ", 9) == 0) {
        cmd_mem_read(line + 9);
    } else if (strncmp(line, "mem write ", 10) == 0) {
        cmd_mem_write(line + 10);
    } else if (strncmp(line, "trig ", 5) == 0) {
        cmd_scope_trig(line + 5);
    } else if (strcmp(line, "flash jedec") == 0) {
        cmd_flash_jedec();
    } else if (strncmp(line, "flash read ", 11) == 0) {
        cmd_flash_read(line + 11);
    } else if (strncmp(line, "flash dump ", 11) == 0) {
        cmd_flash_dump(line + 11);
    } else if (strncmp(line, "flash wtest ", 12) == 0) {
        cmd_flash_wtest(line + 12);
    } else if (strcmp(line, "flash diag") == 0) {
        cmd_flash_diag();
    } else if (strcmp(line, "screen dump") == 0) {
        cmd_screen_dump("");
    } else if (strncmp(line, "screen dump ", 12) == 0) {
        cmd_screen_dump(line + 12);
    } else if (strcmp(line, "screen shadow") == 0) {
        cmd_screen_shadow("");
    } else if (strncmp(line, "screen shadow ", 14) == 0) {
        cmd_screen_shadow(line + 14);
    } else if (strncmp(line, "fpga cmd ", 9) == 0) {
        cmd_fpga_cmd(line + 9);
    } else if (strncmp(line, "fpga frame ", 11) == 0) {
        cmd_fpga_frame(line + 11);
    } else if (strcmp(line, "fpga diag clear") == 0) {
        cmd_fpga_diag_clear();
    } else if (strcmp(line, "fpga busrelease") == 0) {
        cmd_fpga_bus_release();
    } else if (strcmp(line, "fpga stock diag") == 0) {
        cmd_fpga_stock_diag();
    } else if (strcmp(line, "fpga stock clear") == 0) {
        cmd_fpga_stock_clear();
    } else if (strncmp(line, "fpga stock set ", 15) == 0) {
        cmd_fpga_stock_set(line + 15);
    } else if (strncmp(line, "fpga stock preset ", 18) == 0) {
        cmd_fpga_stock_preset(line + 18);
    } else if (strcmp(line, "fpga stock base2") == 0) {
        cmd_fpga_stock_base2();
    } else if (strncmp(line, "fpga stock state5", 17) == 0) {
        cmd_fpga_stock_state5(line[17] == ' ' ? line + 18 : "");
    } else if (strncmp(line, "fpga stock state6", 17) == 0) {
        cmd_fpga_stock_state6(line[17] == ' ' ? line + 18 : "");
    } else if (strcmp(line, "fpga stock prev") == 0) {
        cmd_fpga_stock_prev();
    } else if (strcmp(line, "fpga stock next") == 0) {
        cmd_fpga_stock_next();
    } else if (strcmp(line, "fpga stock select") == 0) {
        cmd_fpga_stock_select();
    } else if (strcmp(line, "fpga stock toggle") == 0) {
        cmd_fpga_stock_toggle();
    } else if (strcmp(line, "fpga stock commit") == 0) {
        cmd_fpga_stock_commit();
    } else if (strcmp(line, "fpga stock consume") == 0) {
        cmd_fpga_stock_consume();
    } else if (strcmp(line, "fpga stock bridge fixed") == 0) {
        cmd_fpga_stock_bridge_fixed();
    } else if (strncmp(line, "fpga stock bridge dynamic", 25) == 0) {
        cmd_fpga_stock_bridge_dynamic(line[25] == ' ' ? line + 26 : "");
    } else if (strcmp(line, "fpga stock reenter") == 0) {
        cmd_fpga_stock_reenter();
    } else if (strncmp(line, "fpga wire words ", 16) == 0) {
        cmd_fpga_wire_words(line + 16);
    } else if (strncmp(line, "fpga wire entry", 15) == 0) {
        cmd_fpga_wire_entry(line[15] == ' ' ? line + 16 : "");
    } else if (strncmp(line, "fpga wire scope", 15) == 0) {
        cmd_fpga_wire_scope(line[15] == ' ' ? line + 16 : "");
    } else if (strcmp(line, "fpga scope reinit") == 0) {
        cmd_fpga_scope_reinit();
    } else if (strncmp(line, "fpga meter reinit", 17) == 0) {
        cmd_fpga_meter_reinit(line[17] == ' ' ? line + 18 : "");
    } else if (strcmp(line, "fpga scope wake") == 0) {
        cmd_fpga_scope_wake();
    } else if (strcmp(line, "fpga scope acqmode") == 0) {
        cmd_fpga_scope_acqmode();
    } else if (strncmp(line, "fpga scope beat", 15) == 0) {
        cmd_fpga_scope_beat(line[15] == ' ' ? line + 16 : "");
    } else if (strncmp(line, "fpga scope entry ", 17) == 0) {
        cmd_fpga_scope_entry(line + 17);
    } else if (strncmp(line, "fpga scope timing ", 18) == 0) {
        cmd_fpga_scope_timing(line + 18);
    } else if (strncmp(line, "fpga scope trig ", 16) == 0) {
        cmd_fpga_scope_trig(line + 16);
    } else if (strncmp(line, "mode", 4) == 0 && (line[4] == '\0' || line[4] == ' ' || line[4] == '\t')) {
        const char *args = line + 4;
        while (*args == ' ' || *args == '\t') args++;
        cmd_mode(args);
    } else if (strcmp(line, "meter dump") == 0) {
        cmd_meter_dump("");
    } else if (strncmp(line, "meter dump ", 11) == 0) {
        cmd_meter_dump(line + 11);
    } else if (strcmp(line, "meter frontend") == 0) {
        cmd_meter_frontend();
    } else if (strcmp(line, "meter mux-stream") == 0) {
        cmd_meter_mux_stream("");
    } else if (strncmp(line, "meter mux-stream ", 17) == 0) {
        cmd_meter_mux_stream(line + 17);
    } else if (strcmp(line, "meter stream") == 0) {
        cmd_meter_stream("");
    } else if (strncmp(line, "meter stream ", 13) == 0) {
        cmd_meter_stream(line + 13);
    } else if (strcmp(line, "meter adc-snapshot") == 0) {
        cmd_meter_adc_snapshot();
    } else if (strcmp(line, "ui dump") == 0) {
        cmd_ui_dump();
    } else if (strcmp(line, "meter wave") == 0) {
        cmd_meter_wave_args("");
    } else if (strncmp(line, "meter wave ", 11) == 0) {
        cmd_meter_wave_args(line + 11);
    } else if (strncmp(line, "fpga acq", 8) == 0) {
        cmd_fpga_acq(line[8] == ' ' ? line + 9 : "");
    } else if (strncmp(line, "fpga reinit", 11) == 0) {
        cmd_fpga_reinit(line[11] == ' ' ? line + 12 : "");
    } else if (strncmp(line, "spi3 xfer", 9) == 0) {
        cmd_spi3_xfer(line[9] == ' ' ? line + 10 : "");
    } else if (strncmp(line, "spi3 seq", 8) == 0) {
        cmd_spi3_seq(line[8] == ' ' ? line + 9 : "");
    } else if (strncmp(line, "spi3 read", 9) == 0) {
        cmd_spi3_read(line[9] == ' ' ? line + 10 : "");
    } else if (strcmp(line, "reboot bootloader") == 0) {
        cmd_reboot_bootloader();
    } else if (strcmp(line, "spi3 acqread") == 0) {
        cmd_spi3_acqread();
    } else if (strncmp(line, "spi3 armtest", 12) == 0) {
        cmd_spi3_armtest(line[12] == ' ' ? line + 13 : "");
    } else if (strcmp(line, "spi3 gowin") == 0) {
        cmd_spi3_gowin();
    } else if (strncmp(line, "spi3 scopetest", 14) == 0) {
        cmd_spi3_scopetest(line[14] == ' ' ? line + 15 : "");
    } else if (strcmp(line, "spi3 acqtest") == 0) {
        cmd_spi3_acqtest();
    } else if (strcmp(line, "spi3 h2verify") == 0) {
        cmd_spi3_h2verify();
    } else if (strcmp(line, "spi3 probe") == 0) {
        /* Bit-bang SPI3 probe: disable SPI peripheral, manually toggle
         * SCK and read MISO to test if the FPGA drives the line. */
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
    } else if (strcmp(line, "uptime") == 0) {
        cmd_uptime();
    } else {
        usb_debug_printf("Unknown command: '%s'  (type 'help')\r\n", line);
    }
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
