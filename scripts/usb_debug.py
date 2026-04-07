#!/usr/bin/env python3
"""
OpenScope 2C53T USB Debug Shell — host-side script.

Usage:
    python3 scripts/usb_debug.py                    # interactive
    python3 scripts/usb_debug.py "status"            # single command
    python3 scripts/usb_debug.py "gpio scan" "status" # multiple commands
"""

import serial
import sys
import time
import glob

def find_device():
    """Find the OpenScope USB CDC device."""
    ports = glob.glob("/dev/tty.usbmodem*")
    if not ports:
        print("ERROR: No USB modem device found. Is the scope connected?")
        sys.exit(1)
    if len(ports) > 1:
        print(f"Multiple devices found: {ports}")
        print(f"Using: {ports[0]}")
    return ports[0]

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
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
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

    ser = serial.Serial(port, 115200, timeout=0.5)
    time.sleep(1)  # Wait for banner

    # Read and discard banner
    if ser.in_waiting:
        banner = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
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
