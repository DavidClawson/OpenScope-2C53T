#!/usr/bin/env python3
"""
OpenScope 2C53T USB Debug Shell — host-side script.

Usage:
    python3 scripts/usb_debug.py                    # interactive
    python3 scripts/usb_debug.py "status"            # single command
    python3 scripts/usb_debug.py "gpio scan" "status" # multiple commands
"""

import sys
import time
import glob
import os
import termios

try:
    import serial  # type: ignore
except ImportError:
    serial = None

def find_device():
    """Find the OpenScope USB CDC device."""
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*"))
    if not ports:
        print("ERROR: No USB modem device found. Is the scope connected?")
        sys.exit(1)
    if len(ports) > 1:
        print(f"Multiple devices found: {ports}")
        print(f"Using: {ports[0]}")
    return ports[0]


class StdlibSerial:
    """Tiny pyserial-like wrapper using only Python's stdlib."""

    def __init__(self, port, baudrate=115200, timeout=0.5):
        self.port = port
        self.timeout = timeout
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = attrs[2] | termios.CLOCAL | termios.CREAD | termios.CS8
        attrs[3] = 0
        attrs[4] = termios.B115200 if baudrate == 115200 else termios.B9600
        attrs[5] = termios.B115200 if baudrate == 115200 else termios.B9600
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    @property
    def in_waiting(self):
        try:
            return len(os.read(self.fd, 4096))
        except BlockingIOError:
            return 0

    def read(self, n=1):
        deadline = time.time() + self.timeout
        out = bytearray()
        while len(out) < n and time.time() < deadline:
            try:
                chunk = os.read(self.fd, n - len(out))
                if chunk:
                    out.extend(chunk)
                else:
                    time.sleep(0.01)
            except BlockingIOError:
                time.sleep(0.01)
        return bytes(out)

    def read_available(self):
        out = bytearray()
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            try:
                chunk = os.read(self.fd, 4096)
                if chunk:
                    out.extend(chunk)
                    deadline = time.time() + 0.1
                else:
                    time.sleep(0.01)
            except BlockingIOError:
                time.sleep(0.01)
        return bytes(out)

    def write(self, data):
        return os.write(self.fd, data)

    def reset_input_buffer(self):
        termios.tcflush(self.fd, termios.TCIFLUSH)

    def close(self):
        os.close(self.fd)


def open_serial(port):
    if serial is not None:
        return serial.Serial(port, 115200, timeout=0.5)
    return StdlibSerial(port, 115200, timeout=0.5)

def send_command(ser, cmd, timeout=2.0):
    """Send a command and return the response."""
    # Flush any pending data
    ser.reset_input_buffer()

    # Send command + CR
    ser.write((cmd + "\r").encode())

    # Read response until we see the next prompt "> "
    response = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if hasattr(ser, "read_available"):
            chunk = ser.read_available()
        elif ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
        else:
            chunk = b""
        if chunk:
            response += chunk
            # Check if we got a prompt back
            if b"\n> " in response or response.endswith(b"> "):
                break
        else:
            time.sleep(0.05)

    text = response.decode("utf-8", errors="replace")

    # Strip the echo of our command and the trailing prompt
    lines = text.split("\r\n")
    # Remove echo line (first line that matches our command)
    cleaned = []
    found_echo = False
    for line in lines:
        if not found_echo and cmd in line:
            found_echo = True
            continue
        if line.strip() == ">":
            continue
        cleaned.append(line)

    return "\r\n".join(cleaned).strip()

def main():
    port = find_device()
    print(f"Connecting to {port}...")
    if serial is None:
        print("pyserial not installed; using stdlib serial fallback.")

    ser = open_serial(port)
    time.sleep(1)  # Wait for banner

    # Read and discard banner
    if hasattr(ser, "read_available"):
        banner_bytes = ser.read_available()
    elif ser.in_waiting:
        banner_bytes = ser.read(ser.in_waiting)
    else:
        banner_bytes = b""
    if banner_bytes:
        banner = banner_bytes.decode("utf-8", errors="replace")
        # Print banner but strip excessive whitespace
        for line in banner.strip().split("\r\n"):
            if line.strip():
                print(line.rstrip())

    if len(sys.argv) > 1:
        # Command-line mode: run each argument as a command
        for cmd in sys.argv[1:]:
            print(f"\n> {cmd}")
            result = send_command(ser, cmd)
            print(result)
    else:
        # Interactive mode
        print("\nType commands (Ctrl-C to exit):\n")
        try:
            while True:
                try:
                    cmd = input("> ")
                except EOFError:
                    break
                if cmd.strip().lower() in ("exit", "quit"):
                    break
                if not cmd.strip():
                    continue
                result = send_command(ser, cmd)
                print(result)
        except KeyboardInterrupt:
            print("\nDisconnected.")

    ser.close()

if __name__ == "__main__":
    main()
