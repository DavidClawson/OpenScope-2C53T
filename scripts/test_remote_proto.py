#!/usr/bin/env python3
"""Build and run the remote-protocol host tests, then prove they can fail.

firmware/tests/test_remote_proto.c exercises esp_comm.c the way the USB
binding (issue #10) uses it. Every guard in that path exists to stop one
specific lie or deafness: a false ACK, a silently dropped frame, an oversize
frame leaking into the shell, a truncated frame leaving the shell deaf.

Same method as test_cal_backup.py: for each guard, copy the source, remove
the guard, rebuild, and REQUIRE the C suite to go red. A mutation that still
passes is a failure here. If a substitution string is no longer found (the
code was refactored), this suite fails loudly instead of skipping.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "firmware" / "src" / "drivers" / "esp_comm.c"
HDR = REPO / "firmware" / "src" / "drivers" / "esp_comm.h"
TEST_C = REPO / "firmware" / "tests" / "test_remote_proto.c"

CFLAGS = ["-std=gnu11", "-Wall", "-Wextra", "-O1"]


@dataclass
class Mutation:
    name: str
    old: str
    new: str


MUTATIONS: tuple[Mutation, ...] = (
    Mutation(
        name="bad checksum dropped silently instead of NAKed",
        old="case RX_BAD_CHECKSUM: esp_comm_send_nak(ESP_ERR_BAD_CHECKSUM); break;",
        new="case RX_BAD_CHECKSUM: break;",
    ),
    Mutation(
        name="oversize length dropped silently instead of NAKed",
        old="case RX_BAD_LENGTH:   esp_comm_send_nak(ESP_ERR_BAD_LENGTH); break;",
        new="case RX_BAD_LENGTH:   break;",
    ),
    Mutation(
        name="oversize frame payload no longer swallowed (leaks into shell)",
        old="rx_discard = (uint32_t)rx_packet.payload_len + ESP_CHECKSUM_SIZE;",
        new="rx_discard = 0;",
    ),
    Mutation(
        name="gap timeout never fires (truncated frame deafens the shell)",
        old="if ((uint32_t)(now_ms - rx_last_ms) < ESP_RX_GAP_MS)\n        return false;",
        new="if (1)\n        return false;",
    ),
    Mutation(
        name="abandoned frame not reported",
        old="    rx_stats.gap_timeouts++;\n    esp_comm_send_nak(ESP_ERR_TIMEOUT);",
        new="    rx_stats.gap_timeouts++;",
    ),
    Mutation(
        name="button ACKed without an injector (false success)",
        old="    if (!button_injector) {\n        esp_comm_send_nak(ESP_ERR_UNSUPPORTED);\n        return;\n    }",
        new="    if (!button_injector) {\n        esp_comm_send_ack();\n        return;\n    }",
    ),
    Mutation(
        name="button ACKed even when the queue refused it",
        old="    if (!button_injector(pkt->payload[0])) {\n        esp_comm_send_nak(ESP_ERR_NOT_READY);\n        return;\n    }",
        new="    (void)button_injector(pkt->payload[0]);",
    ),
    Mutation(
        name="button id range unchecked",
        old="if (pkt->payload[0] < ESP_BTN_CH1 || pkt->payload[0] > ESP_BTN_POWER) {",
        new="if (0) {",
    ),
    Mutation(
        name="stub commands ACK again",
        old="    case ESP_CMD_SIGNAL_CONFIG:\n        esp_comm_send_nak(ESP_ERR_UNSUPPORTED);",
        new="    case ESP_CMD_SIGNAL_CONFIG:\n        esp_comm_send_ack();",
    ),
    Mutation(
        name="STATUS mode hardcoded again",
        old="    out[1] = st.current_mode;",
        new="    out[1] = 0;",
    ),
    Mutation(
        name="router passes frame bytes to the shell",
        old="        if (!esp_comm_rx_in_frame() && b != ESP_SYNC_BYTE)\n            continue;",
        new="        if (b != ESP_SYNC_BYTE && !esp_comm_rx_in_frame())\n            continue;\n        if (passthrough) passthrough(&b, 1, ctx);",
    ),
)


def compile_and_run(source_text: str, workdir: Path) -> subprocess.CompletedProcess:
    (workdir / "esp_comm.c").write_text(source_text)
    shutil.copy(HDR, workdir / "esp_comm.h")
    shutil.copy(TEST_C, workdir / "test_remote_proto.c")
    binary = workdir / "t"
    build = subprocess.run(
        ["cc", *CFLAGS, "-o", str(binary), str(workdir / "test_remote_proto.c"),
         str(workdir / "esp_comm.c"), "-I", str(workdir)],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        return build
    return subprocess.run([str(binary)], capture_output=True, text=True)


class TestRemoteProto(unittest.TestCase):
    def test_source_files_present(self):
        for path in (SRC, HDR, TEST_C):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_base_suite_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(SRC.read_text(), Path(tmp))
            self.assertEqual(proc.returncode, 0,
                             f"base suite failed:\n{proc.stdout}\n{proc.stderr}")

    def test_mutations_are_caught(self):
        base = SRC.read_text()
        for m in MUTATIONS:
            with self.subTest(mutation=m.name):
                self.assertIn(m.old, base, f"mutation string not found (refactor?): {m.name}")
                mutated = base.replace(m.old, m.new, 1)
                self.assertNotEqual(mutated, base, "replacement was a no-op")
                with tempfile.TemporaryDirectory() as tmp:
                    proc = compile_and_run(mutated, Path(tmp))
                self.assertNotEqual(
                    proc.returncode, 0,
                    f"mutation '{m.name}' did NOT make the suite fail — the guard is not tested.\n"
                    f"{proc.stdout}{proc.stderr}")


if __name__ == "__main__":
    if shutil.which("cc") is None:
        print("cc not found: the remote-protocol host tests cannot run", file=sys.stderr)
        sys.exit(0)
    unittest.main(verbosity=2)
