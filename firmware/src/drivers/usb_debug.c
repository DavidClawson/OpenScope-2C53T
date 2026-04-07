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
#include "fpga.h"

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
        "fpga cmd <cmd> [param]          Send FPGA command\r\n"
        "  e.g.: fpga cmd 0 9   (cmd=0x00, param=0x09)\r\n"
        "fpga acq [mode]                 Trigger SPI3 acquisition\r\n"
        "spi3 read [len]                 Raw SPI3 read + hex dump\r\n"
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
        "SPI3 first byte: 0x%02X\r\n"
        "\r\n=== FPGA Diag ===\r\n"
        "IOMUX remap5: 0x%08lX\r\n"
        "IOMUX remap7: 0x%08lX\r\n"
        "SPI3 CTRL1: 0x%08lX\r\n"
        "SPI3 STS: 0x%08lX\r\n",
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
        fpga.spi3_first_byte,
        fpga.diag_remap5,
        fpga.diag_remap7,
        fpga.diag_spi_ctrl1,
        fpga.diag_spi_sts
    );

    usb_debug_printf(
        "\r\n=== SPI3 Handshake ===\r\n"
        "HS: %02X %02X %02X %02X %02X %02X %02X %02X\r\n"
        "BB: idle=%02X cs=%02X byte=%02X marker=%02X\r\n",
        fpga.init_hs[0], fpga.init_hs[1], fpga.init_hs[2], fpga.init_hs[3],
        fpga.init_hs[4], fpga.init_hs[5], fpga.init_hs[6], fpga.init_hs[7],
        fpga.bb_idle, fpga.bb_cs, fpga.bb_byte, fpga.bb_marker
    );

    usb_debug_printf(
        "\r\n=== H2 Cal Upload ===\r\n"
        "Bytes sent: %lu / 115638\r\n"
        "Upload done: %s\r\n",
        fpga.h2_bytes_sent,
        fpga.h2_upload_done ? "YES" : "NO"
    );

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
        usb_send_str("Usage: usart tx <cmd_hi> <cmd_lo> [p1..p6]\r\n"
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

    fpga_send_raw_frame(frame);

    /* Wait for response */
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

static void cmd_fpga_cmd(const char *args)
{
    uint32_t cmd_val, param = 0;
    const char *space = strchr(args, ' ');

    char cmd_str[8];
    if (space) {
        int len = space - args;
        if (len >= (int)sizeof(cmd_str)) { usb_send_str("ERR\r\n"); return; }
        memcpy(cmd_str, args, len);
        cmd_str[len] = '\0';
        parse_int(space + 1, &param);
    } else {
        strncpy(cmd_str, args, sizeof(cmd_str) - 1);
        cmd_str[sizeof(cmd_str) - 1] = '\0';
    }

    if (parse_int(cmd_str, &cmd_val) != 0) {
        usb_send_str("Usage: fpga cmd <cmd> [param]\r\n");
        return;
    }

    BaseType_t ok = fpga_send_cmd((uint8_t)(cmd_val >> 8), (uint8_t)(cmd_val & 0xFF));
    usb_debug_printf("FPGA cmd 0x%02lX param 0x%02lX: %s\r\n",
                     cmd_val, param,
                     ok == pdTRUE ? "queued" : "FULL");

    /* Wait for response */
    vTaskDelay(pdMS_TO_TICKS(200));
    if (fpga.rx_frame_valid) {
        usb_debug_printf("RX:");
        for (int i = 0; i < FPGA_RX_FRAME_SIZE; i++)
            usb_debug_printf(" %02X", fpga.rx_frame[i]);
        usb_send_str("\r\n");
    }
}

static void cmd_fpga_acq(const char *args)
{
    uint32_t mode = FPGA_ACQ_NORMAL + 1;  /* Default: normal (trigger byte = mode + 1) */
    if (args && *args) parse_int(args, &mode);

    BaseType_t ok = fpga_trigger_acquisition((uint8_t)mode);
    usb_debug_printf("Acquisition trigger mode %lu: %s\r\n",
                     mode, ok == pdTRUE ? "queued" : "FULL");

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
    } else if (strncmp(line, "fpga cmd ", 9) == 0) {
        cmd_fpga_cmd(line + 9);
    } else if (strncmp(line, "fpga acq", 8) == 0) {
        cmd_fpga_acq(line[8] == ' ' ? line + 9 : "");
    } else if (strncmp(line, "spi3 read", 9) == 0) {
        cmd_spi3_read(line[9] == ' ' ? line + 10 : "");
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
