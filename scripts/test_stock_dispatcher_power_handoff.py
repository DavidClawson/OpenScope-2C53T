#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "firmware" / "stock_dispatcher" / "src" / "main.c"


def main() -> int:
    text = SOURCE.read_text(encoding="utf-8")
    if "static void wait_for_power_release(void)" not in text:
        raise SystemExit("stock dispatcher lacks POWER release wait")
    if "power_button_pressed()" not in text:
        raise SystemExit("stock dispatcher lacks PC8 POWER helper")

    stock_path = re.search(
        r"if \(!enter_recovery && stock_tail_valid\(\)\) \{(?P<body>.*?)\n    \}",
        text,
        re.S,
    )
    if not stock_path:
        raise SystemExit("stock dispatcher stock launch block not found")

    body = stock_path.group("body")
    wait_at = body.find("wait_for_power_release();")
    jump_at = body.find("jump_to_entry(STOCK_APP_ADDRESS")
    if wait_at == -1:
        raise SystemExit("stock path does not wait for POWER release")
    if jump_at == -1:
        raise SystemExit("stock path does not jump to stock app")
    if wait_at > jump_at:
        raise SystemExit("POWER release wait happens after stock jump")

    recovery_path = re.search(
        r"static int recovery_combo_pressed\(void\).*?return \(\(GPIOC->idt & \(1u << 8\)\) == 0u\) && "
        r"\(\(GPIOB->idt & \(1u << 7\)\) == 0u\);",
        text,
        re.S,
    )
    if not recovery_path:
        raise SystemExit("POWER+PRM recovery combo guard changed unexpectedly")

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
