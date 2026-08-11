#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
BUILDER_PATH = ROOT / "scripts" / "build_stock_hybrid_image.py"


def load_builder():
    spec = importlib.util.spec_from_file_location("build_stock_hybrid_image", BUILDER_PATH)
    if spec is None or spec.loader is None:
        raise SystemExit("could not load stock hybrid builder")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    builder = load_builder()

    # Skip cleanly rather than dumping a FileNotFoundError traceback: the stock
    # archive is not redistributable and the dispatcher must be built first, so
    # a fresh clone legitimately has neither. Report it as a skip so it is
    # counted as missing coverage, not mistaken for a broken test.
    for label, path in (
        ("stock archive image", builder.DEFAULT_STOCK),
        ("stock dispatcher build", builder.DEFAULT_STOCK_DISPATCHER),
    ):
        if not path.exists():
            print(f"skipped '{label} is not present: {path}'")
            print("0 tests OK (1 skipped)")
            return 0

    stock = builder.DEFAULT_STOCK.read_bytes()
    dispatcher = builder.DEFAULT_STOCK_DISPATCHER.read_bytes()

    clear = slice(
        builder.STOCK_PC9_CLEAR_OFFSET,
        builder.STOCK_PC9_CLEAR_OFFSET + len(builder.STOCK_PC9_CLEAR_ORIGINAL),
    )
    reassert = slice(
        builder.STOCK_PC9_REASSERT_OFFSET,
        builder.STOCK_PC9_REASSERT_OFFSET + len(builder.STOCK_PC9_REASSERT_ORIGINAL),
    )
    splash = slice(
        builder.STOCK_SPLASH_INITIAL_STEP_OFFSET,
        builder.STOCK_SPLASH_INITIAL_STEP_OFFSET + len(builder.STOCK_SPLASH_INITIAL_STEP_ORIGINAL),
    )

    if stock[clear] != builder.STOCK_PC9_CLEAR_ORIGINAL:
        raise SystemExit("stock fixture no longer has the guarded PC9 clear")
    if stock[reassert] != builder.STOCK_PC9_REASSERT_ORIGINAL:
        raise SystemExit("stock fixture no longer has the guarded PC9 reassert")
    if stock[splash] != builder.STOCK_SPLASH_INITIAL_STEP_ORIGINAL:
        raise SystemExit("stock fixture no longer has the guarded splash initial step")

    patched = builder.patch_stock_power_hold(stock)
    if stock[clear] != builder.STOCK_PC9_CLEAR_ORIGINAL:
        raise SystemExit("patch mutated the source stock bytes")
    if patched[clear] != builder.STOCK_PC9_CLEAR_PATCH:
        raise SystemExit("patch did not NOP the PC9 clear instruction")
    if patched[reassert] != builder.STOCK_PC9_REASSERT_ORIGINAL:
        raise SystemExit("patch changed the later PC9 reassert")

    fast = builder.patch_stock_fast_splash(stock)
    if stock[splash] != builder.STOCK_SPLASH_INITIAL_STEP_ORIGINAL:
        raise SystemExit("splash patch mutated the source stock bytes")
    if fast[splash] != builder.STOCK_SPLASH_INITIAL_STEP_PATCH:
        raise SystemExit("splash patch did not jump to the final logo step")

    image, layout = builder.build(stock, dispatcher)
    image_clear = slice(
        builder.STOCK_APP_OFFSET + builder.STOCK_PC9_CLEAR_OFFSET,
        builder.STOCK_APP_OFFSET + builder.STOCK_PC9_CLEAR_OFFSET + 2,
    )
    image_splash = slice(
        builder.STOCK_APP_OFFSET + builder.STOCK_SPLASH_INITIAL_STEP_OFFSET,
        builder.STOCK_APP_OFFSET + builder.STOCK_SPLASH_INITIAL_STEP_OFFSET + 2,
    )
    if image[image_clear] != builder.STOCK_PC9_CLEAR_PATCH:
        raise SystemExit("built stock-user image lacks the PC9 clear patch")
    if image[image_splash] != builder.STOCK_SPLASH_INITIAL_STEP_PATCH:
        raise SystemExit("built stock-user image lacks the fast splash patch")
    if not any("PC9 clear NOP patched" in item for item in layout):
        raise SystemExit("layout output does not mention the PC9 patch")
    if not any("fast splash patched" in item for item in layout):
        raise SystemExit("layout output does not mention the fast splash patch")

    slow_image, slow_layout = builder.build(stock, dispatcher, fast_splash=False)
    if slow_image[image_clear] != builder.STOCK_PC9_CLEAR_PATCH:
        raise SystemExit("keep-splash image lost the PC9 clear patch")
    if slow_image[image_splash] != builder.STOCK_SPLASH_INITIAL_STEP_ORIGINAL:
        raise SystemExit("keep-splash image still changed the stock splash step")
    if not any("stock splash kept" in item for item in slow_layout):
        raise SystemExit("layout output does not mention the kept stock splash")

    report = builder.runtime_patch_report()
    if report["estimated_fast_splash_saved_ms"] != 768:
        raise SystemExit(f"wrong splash delay estimate: {report}")
    if report["hardware_init_skipped_by_fast_splash"] is not False:
        raise SystemExit(f"fast splash report overclaims hardware init changes: {report}")
    if not any(item["name"] == "pc9_power_hold" and item["enabled"] for item in report["patches"]):
        raise SystemExit(f"patch report lost mandatory PC9 power-hold patch: {report}")
    slow_report = builder.runtime_patch_report(fast_splash=False)
    if slow_report["estimated_fast_splash_saved_ms"] != 0:
        raise SystemExit(f"keep-splash report still claims boot-time savings: {slow_report}")
    if any(item["name"] == "fnirsi_fast_splash" and item["enabled"] for item in slow_report["patches"]):
        raise SystemExit(f"keep-splash report still enables the splash patch: {slow_report}")

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
