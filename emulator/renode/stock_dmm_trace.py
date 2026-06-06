# Stock DMM trace hooks for Renode.
#
# This script is intentionally trace-only. It records stock firmware writes and
# state transitions around the DMM command/mux path, and feeds only generic
# USART2 ACK/status responses so stock code can keep running far enough to show
# its command cadence. It is not a fake analog frontend and not HIL proxy glue.

from Antmicro.Renode.Peripherals.UART import IUART

machine = self.Machine
sysbus = machine.SystemBus
uart = clr.Convert(machine["sysbus.usart2"], IUART)

state = {
    "tx_buffer": [],
    "tx_frames": 0,
}


def log(msg):
    System.Console.WriteLine("[STOCK_DMM_TRACE] " + msg)


def rb(addr):
    try:
        return sysbus.ReadByte(addr) & 0xFF
    except Exception:
        return -1


def snapshot(label):
    log(
        "%s ms02/fa=%02X ms03/fb=%02X sel=%02X unit=%02X variant=%02X "
        "fmt=%02X dc_state=%02X disp_shift=%02X" % (
            label,
            rb(0x200000FA),
            rb(0x200000FB),
            rb(0x20001025),
            rb(0x20001026),
            rb(0x2000102E),
            rb(0x20001030),
            rb(0x20001027),
            rb(0x2000102F),
        )
    )


def send_bytes_to_firmware(data):
    for b in data:
        uart.WriteChar(b & 0xFF)


def make_status_response(echo_byte):
    return [0xAA, 0x55, 0x00, echo_byte, 0x00, 0x00, 0x00, 0xAA, 0x00, 0x00]


def on_char_received(byte_val):
    b = byte_val & 0xFF
    state["tx_buffer"].append(b)
    buf = state["tx_buffer"]

    if len(buf) < 10:
        return

    frame = buf[:10]
    state["tx_buffer"] = buf[10:]
    state["tx_frames"] += 1
    echo_byte = frame[3]

    log(
        "USART2_TX[%04d] %s cmd=%02X%02X sum=%02X" % (
            state["tx_frames"],
            " ".join("%02X" % x for x in frame),
            frame[2],
            frame[3],
            frame[9],
        )
    )
    snapshot("after_usart2_tx")

    # Generic transport progress only. Do not inject value-shaped DMM frames
    # here; those would make the trace depend on our simulator instead of stock
    # state transitions.
    send_bytes_to_firmware([0x5A, 0xA5])
    send_bytes_to_firmware(make_status_response(echo_byte))


uart.CharReceived += on_char_received
log("stock DMM USART2/RAM trace hook installed")
