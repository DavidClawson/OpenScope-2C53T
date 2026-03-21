# FPGA/DVOM USART2 Simulator for FNIRSI 2C53T
#
# This script hooks into the USART2 peripheral to simulate FPGA responses.
# Loaded via: python "execfile('/path/to/fpga_dvom_sim.py')"
#
# In Renode monitor python context:
#   - self.Machine gives the current machine
#   - monitor.Machine also works
#   - For logging, use print() or System.Console.WriteLine()

from Antmicro.Renode.Peripherals.UART import IUART

# Get references to machine components
machine = self.Machine
sysbus = machine.SystemBus
usart2_periph = machine["sysbus.usart2"]

# Cast to IUART interface
uart = clr.Convert(usart2_periph, IUART)

# State tracking - using mutable containers for closure access
state = {
    'tx_buffer': [],
    'frame_count': 0,
    'byte_count': 0,
}

def log(msg):
    System.Console.WriteLine("[FPGA_SIM] " + msg)

def send_bytes_to_firmware(data):
    """Inject bytes into USART2 RX (firmware receives these)"""
    for b in data:
        uart.WriteChar(b & 0xFF)
    log("  -> Injected %d bytes: [%s]" % (len(data), ", ".join("0x%02X" % (b & 0xFF) for b in data)))

def make_ack():
    """Create a 2-byte ACK response [0x5A, 0xA5]"""
    return [0x5A, 0xA5]

def make_status_response(echo_byte):
    """Create a 10-byte status response matching expected validation:
       RX_buf[3] must match TX_buf[3] (echo verification)
       RX_buf[7] must be 0xAA (integrity marker)
    """
    return [0xAA, 0x55, 0x00, echo_byte, 0x00, 0x00, 0x00, 0xAA, 0x00, 0x00]

def on_char_received(byte_val):
    """Called when firmware transmits a byte via USART2"""
    try:
        b = byte_val & 0xFF
        state['byte_count'] += 1
        state['tx_buffer'].append(b)

        buf = state['tx_buffer']

        # Log accumulation
        if len(buf) <= 12:
            log("TX byte #%d: 0x%02X (buf len=%d)" % (state['byte_count'], b, len(buf)))

        # Check if we have a complete 10-byte frame
        if len(buf) >= 10:
            frame = buf[:10]
            state['tx_buffer'] = buf[10:]
            state['frame_count'] += 1

            echo_byte = frame[3]

            log("=== TX Frame #%d: [%s] ===" % (
                state['frame_count'],
                ", ".join("0x%02X" % x for x in frame)))
            log("  Header: 0x%02X 0x%02X | Param: 0x%02X 0x%02X | Data: [%s] | Chk: 0x%02X" % (
                frame[0], frame[1], frame[2], frame[3],
                " ".join("0x%02X" % x for x in frame[4:9]),
                frame[9]))

            # Send ACK
            send_bytes_to_firmware(make_ack())

            # Send status response with echo byte
            send_bytes_to_firmware(make_status_response(echo_byte))

            log("  Frame #%d: ACK + Status sent (echo=0x%02X)" % (state['frame_count'], echo_byte))

    except Exception as e:
        log("ERROR: %s" % str(e))

# Register the CharReceived handler
uart.CharReceived += on_char_received

log("=== FPGA/DVOM USART2 simulator initialized ===")
log("Monitoring USART2 TX, will respond with ACK + Status frames")
