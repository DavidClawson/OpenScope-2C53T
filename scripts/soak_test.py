#!/usr/bin/env python3
"""
OpenScope 2C53T — Soak Test Harness

Builds firmware, launches it in Renode with random button fuzzing,
and monitors for failures (freezes, crashes, faults).

Usage:
    python3 scripts/soak_test.py [OPTIONS]

Options:
    --duration SECONDS   How long to run (default: 120)
    --seed NUMBER        PRNG seed for reproducible button sequences (default: 42)
    --stale-timeout SEC  Framebuffer stale timeout in seconds (default: 10)
    --verbose            Print real-time button events
    --no-build           Skip firmware build (use existing binary)

Exit codes:
    0  All clear — no faults detected
    1  Fault detected (see report)
    2  Setup/build failure
"""

import argparse
import os
import re
import signal
import subprocess
import sys
import time

# --- Paths ---
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
FIRMWARE_DIR = os.path.join(PROJECT_ROOT, "firmware")
RENODE_DIR = os.path.join(PROJECT_ROOT, "emulator", "renode")
RENODE_BIN = "/Applications/Renode.app/Contents/MacOS/renode"

# Temp files used for communication with Renode
FB_PATH = "/tmp/openscope_fb.bin"
HEARTBEAT_PATH = "/tmp/openscope_soak_heartbeat"
FAULT_LOG = "/tmp/openscope_soak_faults.log"
BUTTON_LOG = "/tmp/openscope_soak_buttons.log"
SEED_FILE = "/tmp/openscope_soak_seed.txt"
STATE_FILE = "/tmp/openscope_soak_state.txt"
MAP_FILE = os.path.join(FIRMWARE_DIR, "build", "firmware.map")
RESC_TEMPLATE = os.path.join(RENODE_DIR, "run_soak_test.resc")
RESC_PATCHED = "/tmp/openscope_soak_test.resc"


def log(msg, level="INFO"):
    timestamp = time.strftime("%H:%M:%S")
    print(f"[{timestamp}] [{level}] {msg}")


def cleanup_tmp_files():
    """Remove stale temp files from previous runs."""
    for path in [FB_PATH, HEARTBEAT_PATH, FAULT_LOG, BUTTON_LOG, STATE_FILE]:
        try:
            os.remove(path)
        except FileNotFoundError:
            pass


def build_firmware():
    """Build firmware with 'make emu'."""
    log("Building firmware (make emu)...")
    result = subprocess.run(
        ["make", "emu"],
        cwd=FIRMWARE_DIR,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if result.returncode != 0:
        log("Build failed!", "ERROR")
        print(result.stderr[-2000:] if len(result.stderr) > 2000 else result.stderr)
        return False

    # Extract size from output
    for line in result.stdout.split("\n"):
        if "text" in line and "data" in line and "bss" in line:
            log(f"  {line.strip()}")
        if "Built:" in line:
            log(f"  {line.strip()}")
    return True


def find_symbol_address(symbol_name):
    """Look up a symbol address from the firmware .map file."""
    if not os.path.exists(MAP_FILE):
        return None
    try:
        with open(MAP_FILE, "r") as f:
            for line in f:
                # Match lines like: 0x08001234    vApplicationStackOverflowHook
                m = re.search(
                    r"(0x[0-9a-fA-F]+)\s+" + re.escape(symbol_name) + r"\b",
                    line,
                )
                if m:
                    return m.group(1)
    except Exception:
        pass
    return None


def patch_resc_file(seed):
    """
    Read the .resc template and patch in fault hook addresses
    from the firmware .map file.
    """
    with open(RESC_TEMPLATE, "r") as f:
        resc = f.read()

    # Patch Default_Handler address (used for skip_default_irq hook)
    default_addr = find_symbol_address("Default_Handler")
    if default_addr:
        resc = re.sub(
            r'cpu AddHook 0x[0-9a-fA-F]+ \$skip_default_irq',
            f'cpu AddHook {default_addr} $skip_default_irq',
            resc,
        )
        log(f"  Hook: Default_Handler @ {default_addr}")

    # Look up fault handler addresses
    hooks = {
        "vApplicationStackOverflowHook": "stack_overflow_hook",
        "vApplicationMallocFailedHook": "malloc_failed_hook",
        "HardFault_Handler": "hardfault_hook",
    }

    hook_lines = []
    for symbol, var_name in hooks.items():
        addr = find_symbol_address(symbol)
        if addr:
            hook_lines.append(f'cpu AddHook {addr} ${var_name}')
            log(f"  Hook: {symbol} @ {addr}")
        else:
            log(f"  Warning: {symbol} not found in .map", "WARN")

    # Heartbeat hook — hooks health check timer (500ms) for liveness
    # Falls back to 1-second timer if health check not found
    heartbeat_addr = find_symbol_address("vHealthCheckCallback")
    if not heartbeat_addr:
        heartbeat_addr = find_symbol_address("vOneSecondTimerCallback")
    if heartbeat_addr:
        hook_lines.append(f'cpu AddHook {heartbeat_addr} $heartbeat_hook')
        log(f"  Hook: heartbeat @ {heartbeat_addr}")

    # Append hook registrations before the 'start' command
    if hook_lines:
        hook_block = "\n".join(hook_lines)
        resc = resc.replace("# --- Start ---\nstart",
                            hook_block + "\n\n# --- Start ---\nstart")

    with open(RESC_PATCHED, "w") as f:
        f.write(resc)

    return RESC_PATCHED


def write_seed(seed):
    """Write PRNG seed for the GPIO fuzzer to read."""
    with open(SEED_FILE, "w") as f:
        f.write(str(seed))


def get_heartbeat_mtime():
    """Get heartbeat file modification time, or 0 if not present."""
    try:
        return os.path.getmtime(HEARTBEAT_PATH)
    except FileNotFoundError:
        return 0.0


def get_fb_mtime():
    """Get framebuffer file modification time, or 0 if not present."""
    try:
        return os.path.getmtime(FB_PATH)
    except FileNotFoundError:
        return 0.0


def read_faults():
    """Read fault log entries."""
    try:
        with open(FAULT_LOG, "r") as f:
            return [line.strip() for line in f if line.strip()]
    except FileNotFoundError:
        return []


def count_button_presses():
    """Count total button press events from the log."""
    try:
        with open(BUTTON_LOG, "r") as f:
            return sum(1 for line in f if "PRESS" in line)
    except FileNotFoundError:
        return 0


def tail_button_log(last_pos):
    """Read new button log entries since last_pos. Returns (new_pos, lines)."""
    try:
        with open(BUTTON_LOG, "r") as f:
            f.seek(last_pos)
            new_lines = f.readlines()
            new_pos = f.tell()
        return new_pos, new_lines
    except FileNotFoundError:
        return last_pos, []


def run_soak_test(duration, seed, stale_timeout, verbose):
    """Main soak test loop."""

    cleanup_tmp_files()
    write_seed(seed)

    log(f"Patching Renode script with fault hook addresses...")
    resc_path = patch_resc_file(seed)

    log(f"Launching Renode (duration={duration}s, seed={seed})...")
    renode_proc = subprocess.Popen(
        [RENODE_BIN, "--disable-gui", "-e",
         f"include @{resc_path}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        cwd=RENODE_DIR,
    )

    start_time = time.time()
    last_heartbeat = start_time
    last_hb_mtime = 0.0
    fw_started = False
    button_log_pos = 0
    faults = []
    stale_detected = False
    renode_crashed = False

    # Give Renode a few seconds to boot
    log("Waiting for firmware to boot...")
    time.sleep(3)

    log("Monitoring started (fast mode — heartbeat liveness).")
    log("Press Ctrl+C to stop early.")
    print()
    print(f"  {'Elapsed':>8}  {'Buttons':>8}  {'HB Age':>7}  {'Status'}")
    print(f"  {'-------':>8}  {'-------':>8}  {'------':>7}  {'------'}")

    try:
        while True:
            elapsed = time.time() - start_time

            # Check if Renode is still running
            if renode_proc.poll() is not None:
                log("Renode process exited!", "WARN")
                renode_crashed = True
                break

            # Check duration
            if elapsed >= duration:
                log(f"Duration reached ({duration}s). Stopping.")
                break

            # Check firmware heartbeat (1-second timer callback)
            hb_mtime = get_heartbeat_mtime()
            if hb_mtime != last_hb_mtime:
                last_hb_mtime = hb_mtime
                last_heartbeat = time.time()
                if not fw_started:
                    fw_started = True
                    log("Heartbeat detected — firmware is running.")

            hb_age = time.time() - last_heartbeat

            if fw_started and hb_age > stale_timeout:
                log(f"Heartbeat stale for {hb_age:.0f}s — "
                    f"firmware may be frozen!", "FAULT")
                stale_detected = True

            # Check fault log
            new_faults = read_faults()
            if len(new_faults) > len(faults):
                for fault in new_faults[len(faults):]:
                    log(f"Fault detected: {fault}", "FAULT")
                faults = new_faults

            # Verbose: show button events
            if verbose:
                button_log_pos, new_lines = tail_button_log(button_log_pos)
                for line in new_lines:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        print(f"  [BTN] {line}")

            # Status line (every 5 seconds)
            if int(elapsed) % 5 == 0:
                btn_count = count_button_presses()
                status = "OK"
                if faults:
                    status = f"FAULTS: {len(faults)}"
                elif stale_detected:
                    status = "STALE (possible freeze)"
                elif not fw_started:
                    status = "waiting for boot..."

                print(f"  {elapsed:7.0f}s  {btn_count:>8}  {hb_age:5.1f}s  {status}")

            time.sleep(1)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        log(f"Interrupted after {elapsed:.0f}s.")

    # --- Shutdown ---
    log("Stopping Renode...")
    if renode_proc.poll() is None:
        renode_proc.send_signal(signal.SIGTERM)
        try:
            renode_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            renode_proc.kill()

    # --- Report ---
    print()
    print("=" * 60)
    print("  SOAK TEST REPORT")
    print("=" * 60)

    total_elapsed = time.time() - start_time
    total_buttons = count_button_presses()

    rate = total_buttons / total_elapsed * 60 if total_elapsed > 0 else 0
    print(f"  Wall clock:      {total_elapsed:.0f}s")
    print(f"  PRNG seed:       {seed}")
    print(f"  Button presses:  {total_buttons} ({rate:.1f}/min)")
    print(f"  Faults detected: {len(faults)}")
    if faults:
        for fault in faults:
            print(f"    - {fault}")
    print(f"  Stale heartbeat: {'YES' if stale_detected else 'no'}")
    print(f"  Renode crashed:  {'YES' if renode_crashed else 'no'}")
    print()

    if faults or stale_detected or renode_crashed:
        print("  RESULT: FAIL")
        print()
        print(f"  Reproduce with: python3 scripts/soak_test.py --seed {seed}")
        if os.path.exists(BUTTON_LOG):
            print(f"  Button log:      {BUTTON_LOG}")
        print("=" * 60)
        return 1
    else:
        print("  RESULT: PASS")
        print("=" * 60)
        return 0


def main():
    parser = argparse.ArgumentParser(
        description="OpenScope 2C53T Soak Test — "
                    "random button fuzzing with fault monitoring"
    )
    parser.add_argument("--duration", type=int, default=120,
                        help="Test duration in seconds (default: 120)")
    parser.add_argument("--seed", type=int, default=42,
                        help="PRNG seed for reproducibility (default: 42)")
    parser.add_argument("--stale-timeout", type=int, default=300,
                        help="Heartbeat stale timeout in wall-clock seconds "
                             "(default: 300 — accounts for slow emulation)")
    parser.add_argument("--verbose", action="store_true",
                        help="Print real-time button events")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip firmware build")
    args = parser.parse_args()

    print()
    print("=" * 60)
    print("  OpenScope 2C53T — Soak Test Harness")
    print("=" * 60)
    print()

    # Check Renode
    if not os.path.exists(RENODE_BIN):
        log(f"Renode not found at {RENODE_BIN}", "ERROR")
        log("Install Renode or update RENODE_BIN path.")
        return 2

    # Build firmware
    if not args.no_build:
        if not build_firmware():
            return 2
        print()

    # Run soak test
    return run_soak_test(
        duration=args.duration,
        seed=args.seed,
        stale_timeout=args.stale_timeout,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    sys.exit(main())
