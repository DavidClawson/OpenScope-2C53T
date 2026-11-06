"""Transport: find the OpenScope CDC port, open it, survive it going away.

The only layer that knows about USB (remote_protocol.md §3.1). Design rules
from §2.4: "no device" is a normal outcome, device paths are not stable,
the port may vanish (IAP flash, reboot, or the firmware's CDC self-heal from
issue #39 — which the host sees as a replug) and must be reopened.
"""
from __future__ import annotations

import glob
import time
from typing import List, Optional

AT32_VCP = (0x2E3C, 0x5740)    # Artery virtual COM port (the app's CDC shell)
FALLBACK_GLOBS = ("/dev/cu.usbmodem*", "/dev/ttyACM*")


class FlushFailed(OSError):
    """The write returned but draining failed (device vanished). Treat as
    'possibly delivered': never re-send something that can act twice."""


class NoDevice(Exception):
    """No OpenScope port found. The message says where we looked."""


def candidate_ports() -> List[str]:
    """OpenScope ports by VID:PID; falls back to the usual globs if pyserial's
    port listing is unavailable. Other USB serial devices are ignored."""
    try:
        from serial.tools import list_ports
        ports = [p for p in list_ports.comports() if (p.vid, p.pid) == AT32_VCP]
        cu = sorted(p.device for p in ports if p.device.startswith("/dev/cu."))
        return cu or sorted(p.device for p in ports)
    except Exception:
        hits: List[str] = []
        for g in FALLBACK_GLOBS:
            hits += sorted(glob.glob(g))
        return hits


def looked_where() -> str:
    return f"USB {AT32_VCP[0]:04x}:{AT32_VCP[1]:04x} (AT32 VCP)"


class SerialLink:
    """Byte pipe over pyserial with reopen-on-loss."""

    def __init__(self, port: Optional[str] = None, reopen_wait: float = 6.0,
                 drain_timeout: float = 2.0):
        self.requested = port
        self.drain_timeout = drain_timeout
        self.port: Optional[str] = None
        self.reopen_wait = reopen_wait
        self._ser = None
        self.reopens = 0

    def open(self) -> None:
        import serial  # imported late so `openscope --help` works without pyserial
        port = self.requested
        if port is None:
            ports = candidate_ports()
            if not ports:
                raise NoDevice(f"no OpenScope found on {looked_where()}")
            port = ports[0]
        try:
            # exclusive: the protocol has no request ids, so a second process on
            # the same port would steal (and drain away) this one's replies.
            self._ser = serial.Serial(port, 115200, timeout=0.05, write_timeout=2.0,
                                      exclusive=True)
        except (OSError, serial.SerialException) as e:
            busy = "busy" in str(e).lower() or "lock" in str(e).lower()
            raise NoDevice(f"cannot open {port}: "
                           + ("in use by another process (only one client at a time)" if busy else str(e))) from None
        self.port = port

    def close(self) -> None:
        if self._ser is not None:
            try:
                self._ser.close()
            finally:
                self._ser = None

    def reopen(self) -> None:
        """After the port vanished: wait for it to come back (any path)."""
        self.close()
        deadline = time.time() + self.reopen_wait
        last: Exception = NoDevice("not found")
        while time.time() < deadline:
            try:
                self.open()
                self.reopens += 1
                return
            except NoDevice as e:
                last = e
                time.sleep(0.2)
        raise NoDevice(f"device did not come back within {self.reopen_wait:.0f} s ({last})")

    def write(self, data: bytes) -> None:
        """OSError = nothing may have been sent; FlushFailed = the bytes were
        handed to the OS and may well have reached the device."""
        self._ser.write(data)
        # Not ser.flush(): on POSIX that is an unbounded tcdrain, and the device
        # only re-arms its OUT endpoint when the shell task polls, so a busy or
        # wedged device would hang the host with no timeout at all. Wait for
        # the OS queue to empty, but only up to a deadline.
        deadline = time.time() + self.drain_timeout
        try:
            while self._ser.out_waiting:
                if time.time() > deadline:
                    raise FlushFailed(f"device not accepting data after {self.drain_timeout:.0f} s")
                time.sleep(0.005)
        except FlushFailed:
            raise
        except Exception as e:      # port vanished while draining (termios/serial errors)
            raise FlushFailed(f"port lost after write: {e}") from e

    def read(self, n: int = 4096) -> bytes:
        return self._ser.read(max(1, min(n, self._ser.in_waiting or 1)))

    def drain(self, quiet: float = 0.15, max_wait: float = 1.5) -> bytes:
        """Swallow whatever the device is still saying (banner, old output)."""
        out = bytearray()
        start = last = time.time()
        while time.time() - start < max_wait:
            chunk = self.read()
            if chunk:
                out += chunk
                last = time.time()
            elif time.time() - last >= quiet:
                break
        return bytes(out)
