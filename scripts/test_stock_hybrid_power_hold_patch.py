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

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
