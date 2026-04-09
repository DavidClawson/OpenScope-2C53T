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
#include "fpga.h"
#include "ui.h"
#include "../ui/scope_state.h"

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

/* Send raw bytes over CDC, waiting for TX complete if needed */
static void usb_send_bytes(const uint8_t *data, uint16_t len)
{
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
        "fpga acq [mode]                 Trigger SPI3 acquisition\r\n"
        "spi3 read [len]                 Raw SPI3 read + hex dump\r\n"
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
        "\r\n=== H2 Cal Upload ===\r\n"
        "Bytes sent: %lu / 115638\r\n"
        "Upload done: %s\r\n",
        fpga.h2_bytes_sent,
        fpga.h2_upload_done ? "YES" : "NO"
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
    } else if (strcmp(line, "flash jedec") == 0) {
        cmd_flash_jedec();
    } else if (strncmp(line, "flash read ", 11) == 0) {
        cmd_flash_read(line + 11);
    } else if (strncmp(line, "flash dump ", 11) == 0) {
        cmd_flash_dump(line + 11);
    } else if (strncmp(line, "fpga cmd ", 9) == 0) {
        cmd_fpga_cmd(line + 9);
    } else if (strncmp(line, "fpga frame ", 11) == 0) {
        cmd_fpga_frame(line + 11);
    } else if (strcmp(line, "fpga diag clear") == 0) {
        cmd_fpga_diag_clear();
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
    } else if (strncmp(line, "fpga acq", 8) == 0) {
        cmd_fpga_acq(line[8] == ' ' ? line + 9 : "");
    } else if (strncmp(line, "spi3 read", 9) == 0) {
        cmd_spi3_read(line[9] == ' ' ? line + 10 : "");
    } else if (strcmp(line, "reboot bootloader") == 0) {
        cmd_reboot_bootloader();
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

static void vUsbDebugTask(void *pvParameters)
{
    (void)pvParameters;

    uint8_t rx_buf[USBD_CDC_OUT_MAXPACKET_SIZE];
    char cmd_buf[CMD_BUF_SIZE];
    int cmd_pos = 0;
    bool banner_sent = false;

    for (;;) {
        /* Wait for USB to be configured */
        if (!usb_debug_connected()) {
            banner_sent = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Send welcome banner once on connect */
        if (!banner_sent) {
            vTaskDelay(pdMS_TO_TICKS(500));  /* Let host enumerate */
            usb_send_str("\r\n\r\n"
                         "╔══════════════════════════════════╗\r\n"
                         "║  OpenScope 2C53T Debug Shell     ║\r\n"
                         "║  Type 'help' for commands        ║\r\n"
                         "╚══════════════════════════════════╝\r\n"
                         "\r\n> ");
            banner_sent = true;
        }

        /* Poll for received data */
        uint16_t rx_len = usb_vcp_get_rxdata(&usb_core_dev, rx_buf);
        if (rx_len > 0) {
            for (uint16_t i = 0; i < rx_len; i++) {
                char c = (char)rx_buf[i];

                if (c == '\r' || c == '\n') {
                    /* Execute command */
                    usb_send_str("\r\n");
                    cmd_buf[cmd_pos] = '\0';
                    if (cmd_pos > 0) {
                        dispatch_command(cmd_buf);
                    }
                    cmd_pos = 0;
                    usb_send_str("> ");
                } else if (c == '\b' || c == 0x7F) {
                    /* Backspace */
                    if (cmd_pos > 0) {
                        cmd_pos--;
                        usb_send_str("\b \b");
                    }
                } else if (c >= ' ' && cmd_pos < CMD_BUF_SIZE - 1) {
                    /* Echo and accumulate */
                    cmd_buf[cmd_pos++] = c;
                    usb_send_bytes((const uint8_t *)&c, 1);
                }
            }
        } else {
            /* No data — yield to other tasks */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void usb_debug_create_task(void)
{
#ifndef EMULATOR_BUILD
    xTaskCreate(vUsbDebugTask, "usb_dbg", 512, NULL, 2, NULL);
#endif
}
