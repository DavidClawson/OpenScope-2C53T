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

CFLAGS = ["-std=gnu11", "-Wall", "-Wextra", "-O1"]   # no -Werror: a mutant may leave an unused variable


class BuildFailed(Exception):
    """A mutant that does not compile proves nothing about the guard."""


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
        name="abandoned frame answered with an unsolicited NAK",
        old="    rx_stats.gap_timeouts++;\n",
        new="    rx_stats.gap_timeouts++;\n    esp_comm_send_nak(ESP_ERR_BAD_LENGTH);\n",
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
        name="button id lower bound unchecked (id 0 queued)",
        old="if (pkt->payload[0] < ESP_BTN_CH1 || pkt->payload[0] > ESP_BTN_POWER) {",
        new="if (pkt->payload[0] > ESP_BTN_POWER) {",
    ),
    Mutation(
        name="button id upper bound unchecked",
        old="if (pkt->payload[0] < ESP_BTN_CH1 || pkt->payload[0] > ESP_BTN_POWER) {",
        new="if (pkt->payload[0] < ESP_BTN_CH1) {",
    ),
    Mutation(
        name="fw_version cap removed (stack overflow in handle_status)",
        old="return (uint8_t)(n > ESP_FW_VERSION_MAX ? ESP_FW_VERSION_MAX : n);",
        new="return (uint8_t)n;",
    ),
    Mutation(
        name="gap poll leaves an oversize discard running (lying length deafens the shell)",
        old="    rx_state = RX_WAIT_SYNC;\n    rx_discard = 0;\n    rx_stats.gap_timeouts++;",
        new="    rx_state = RX_WAIT_SYNC;\n    rx_stats.gap_timeouts++;",
    ),
    Mutation(
        name="gap clock refreshed only at the sync byte (timeout from frame start)",
        old="        rx_last_ms = now_ms;\n        switch (rx_step(b)) {",
        new="        if (b == ESP_SYNC_BYTE) rx_last_ms = now_ms;\n        switch (rx_step(b)) {",
    ),
    Mutation(
        name="rx_touch does nothing",
        old="    if (esp_comm_rx_in_frame())\n        rx_last_ms = now_ms;",
        new="    (void)now_ms;",
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
        name="meter with no reading answers a zero frame instead of NOT_READY",
        old="    default:\n        esp_comm_send_nak(ESP_ERR_NOT_READY);             /* no reading yet: say so */\n        return;",
        new="    default:\n        break;",
    ),
    Mutation(
        name="frozen reading sent outside meter mode",
        old="    case ESP_METER_WRONG_MODE:\n        esp_comm_send_nak(ESP_ERR_UNSUPPORTED_IN_MODE);   /* frozen, not live */\n        return;",
        new="    case ESP_METER_WRONG_MODE:\n        break;",
    ),
    Mutation(
        name="raw BCD dropped from METER_FRAME",
        old="    put_u16(&out[8], (uint16_t)m.raw_bcd);",
        new="    put_u16(&out[8], 0);",
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
        raise BuildFailed(build.stderr)
    return subprocess.run([str(binary)], capture_output=True, text=True)


class TestRemoteProto(unittest.TestCase):
    def test_source_files_present(self):
        for path in (SRC, HDR, TEST_C):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_base_suite_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(SRC.read_text(), Path(tmp))   # BuildFailed = error
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
                    try:
                        proc = compile_and_run(mutated, Path(tmp))
                    except BuildFailed as e:
                        self.fail(f"mutant '{m.name}' did not build, so it proves nothing:\n{e}")
                self.assertNotEqual(
                    proc.returncode, 0,
                    f"mutation '{m.name}' did NOT make the suite fail — the guard is not tested.\n"
                    f"{proc.stdout}{proc.stderr}")


def setUpModule():
    # A skip, not a silent exit 0: run_tests.py counts skips, and --strict
    # must not report "all tests ran" on a machine that ran none.
    if shutil.which("cc") is None:
        raise unittest.SkipTest("cc not found: the remote-protocol host tests cannot run")


if __name__ == "__main__":
    unittest.main(verbosity=2)
