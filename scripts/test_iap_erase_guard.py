#!/usr/bin/env python3
"""Behavioural test for the bootloader's IAP erase/write guard.

WHY THIS EXISTS
---------------
The other flash-safety suites assert on the *text* of hid_iap_user.c -- they grep
for the source strings a correct implementation is expected to contain. That
catches a function being deleted, but it cannot tell correct logic from
incorrect logic, because the strings are equally present in both.

On 2026-08-11 that gap shipped a real regression through review. Commit
11e4f76 gated *writes* on the *erase*-alignment predicate:

    if (iap_erase_address_valid(address))   /* 2K sectors: (address & 2047) == 0 */

but hid_flash.py sends an ADDR packet per 1KB block, and any part with >=256KB
of flash uses 2K sectors. Half of every flash would have been NACKed -- the
device would have failed on block 2 of 498. It passed a clean build, the full
test suite, and a manual regression hunt, and was caught only when a
contributor ran it on real hardware.

So this test does not read the code. It EXECUTES it:

  1. extracts the guard functions verbatim from hid_iap_user.c (no retyping --
     if the source changes, this test runs the changed source),
  2. compiles them for the host against a simulated flash array, with the
     AT32 flash/USB/LCD calls stubbed,
  3. drives realistic ADDR sequences through the real iap_address() and checks
     what actually happened to the simulated flash.

The property being defended is the one that matters: *every byte written must
land on flash that was erased and not already programmed in this session.*

VERIFIED BY MUTATION
--------------------
A test that never fails is worth nothing, so each assertion here was checked by
reintroducing the bug it is supposed to catch. Seven mutations, seven caught:

  M1  gate writes on erase alignment (the actual 11e4f76 bug)  -> caught
  M2  erase every block instead of every sector                -> caught
  M3  iap_init() forgets to clear the erase record             -> caught
  M4  drop the sector-base match in same_erased_sector         -> caught
  M5  drop the HID buffer alignment check                      -> caught
  M6  skip the erase when one is required                      -> caught
  M7  drop the write-window bounds check                       -> caught

An eighth mutation -- making a failed erase respond ACK instead of NACK -- is
NOT caught, and deliberately so: iap_erase_sector() returns IAP_FAILED only when
iap_erase_required() is false, but it is called only when that same predicate is
true. The else branch at the erase call site is unreachable by construction. If
a future change makes it reachable, add a case here.

Two of these (M3, M5) were missed by the first draft of this file, because the
harness reset state itself instead of calling iap_init(), and because the
alignment probe was also being rejected by a different check. That is the same
"tested something other than the thing" failure the file exists to prevent, so:
if you add an assertion, add the mutation that breaks it.

Run: python3 scripts/test_iap_erase_guard.py
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IAP_SOURCE = ROOT / "firmware" / "bootloader" / "src" / "hid_iap_user.c"
VENDOR_HEADER = (
    ROOT / "firmware" / "at32f403a_lib" / "middlewares" / "usbd_class" / "hid_iap" / "hid_iap_class.h"
)

# Functions lifted verbatim out of hid_iap_user.c, in dependency order.
# iap_init() is included deliberately: the session-reset invariant (that a new
# flash session must not inherit the previous session's erase record) lives in
# iap_init(), so a harness that reset that state itself would be testing the
# harness rather than the firmware.
EXTRACTED_FUNCTIONS = (
    "iap_erase_sector_base",
    "iap_write_address_valid",
    "iap_erase_required",
    "iap_erase_sector",
    "iap_init",
    "iap_address",
)

# Stable ArteryTek SDK constants. The vendor lib is gitignored, so they are
# duplicated here -- but when it *is* present, test_vendor_constants_match()
# asserts these against it, so they cannot drift silently.
SDK_CONSTANTS = {
    "SECTOR_SIZE_1K": 0x400,
    "SECTOR_SIZE_2K": 0x800,
    "SECTOR_SIZE_4K": 0x1000,
    "HID_IAP_BUFFER_LEN": 1024,
}

# Flash geometry under test: the 2C53T's 1MB AT32F403A, which selects 2K sectors.
APP_ADDRESS = 0x08004000
APP_END_ADDRESS = 0x08100000
BLOCK = SDK_CONSTANTS["HID_IAP_BUFFER_LEN"]
SECTOR = SDK_CONSTANTS["SECTOR_SIZE_2K"]
SIM_FLASH_BYTES = 1024 * 1024


def extract_c_function(source: str, name: str) -> str:
    """Return the full text of a C function definition, by brace matching.

    Deliberately dumb and exact: it finds the definition line, then walks braces.
    No regex-based body parsing, so it cannot silently truncate a function.
    """
    pattern = re.compile(
        r"^[A-Za-z_][A-Za-z0-9_ \t\*]*\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(source)
    if not match:
        raise AssertionError(
            f"could not find a definition of {name}() in {IAP_SOURCE.name}. "
            "If it was renamed, update EXTRACTED_FUNCTIONS -- do not delete this test."
        )
    start = match.start()
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start : i + 1]
    raise AssertionError(f"unbalanced braces while extracting {name}()")


HARNESS_PROLOGUE = """
/* Generated by scripts/test_iap_erase_guard.py -- do not edit by hand.
   The guard functions below are copied VERBATIM from hid_iap_user.c. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define SECTOR_SIZE_1K %(SECTOR_SIZE_1K)#x
#define SECTOR_SIZE_2K %(SECTOR_SIZE_2K)#x
#define SECTOR_SIZE_4K %(SECTOR_SIZE_4K)#x
#define HID_IAP_BUFFER_LEN %(HID_IAP_BUFFER_LEN)d
#define IAP_ERASED_SECTOR_INVALID 0xFFFFFFFFu

typedef enum { IAP_SUCCESS = 0, IAP_FAILED } iap_result_type;
enum { IAP_STS_IDLE, IAP_STS_START, IAP_STS_ADDR, IAP_STS_DATA };
#define IAP_ACK  0x00AAu
#define IAP_NACK 0x00FFu
#define IAP_CMD_ADDR 0x5AA2u

typedef struct {
  uint32_t flash_size;
  uint32_t sector_size;
  uint32_t flash_start_address;
  uint32_t flash_end_address;
  uint32_t app_address;
  uint32_t iap_address;
  uint32_t flag_address;
  uint32_t fifo_length;
  int      state;
  uint8_t  iap_tx[64];
} iap_info_type;

iap_info_type iap_info;
static uint32_t iap_write_start, iap_write_end;
static uint8_t  iap_native_write;
static uint32_t iap_erased_sector_address;

/* Values iap_init() reads. sim_flash_size_kb is settable so the 1K-sector
   branch (parts under 256KB) can be exercised through the real iap_init(). */
static uint32_t sim_flash_size_kb = 1024;
#define FLASH_SIZE_REG()      (sim_flash_size_kb)
#define KB_TO_B(kb)           ((kb) * 1024u)
#define FLASH_BASE            0x08000000u
#define FLASH_APP_ADDRESS     %(APP_ADDRESS)#x
#define FLASH_APP_END_ADDRESS %(APP_END_ADDRESS)#x

/* ---- simulated flash ------------------------------------------------- */
#define SIM_BASE  0x08000000u
#define SIM_BYTES (1024u * 1024u)
static uint8_t sim_erased[SIM_BYTES];   /* 1 = erased since session start   */
static uint8_t sim_written[SIM_BYTES];  /* 1 = programmed since last erase  */
static int violations;                  /* writes onto unerased/reprogrammed */
static int destroyed;                   /* programmed bytes wiped by a later erase */
static int last_response;               /* IAP_ACK / IAP_NACK               */

/* Erases of a sector outside [iap_write_start, iap_write_end). The upgrade-flag
   sector sits deliberately just below the app window, so it is not counted. */
static int stray_erases;

static void flash_unlock(void) {}
static void flash_lock(void) {}
static void flash_sector_erase(uint32_t addr)
{
  uint32_t base = addr - (addr %% iap_info.sector_size);
  int in_window = (base >= iap_write_start && base + iap_info.sector_size <= iap_write_end);
  if (!in_window && base != iap_info.flag_address) stray_erases++;
  /* A real device would erase whatever it was told to; the simulator records
     the trespass rather than corrupting its own memory. */
  if (base < SIM_BASE || base + iap_info.sector_size > SIM_BASE + SIM_BYTES) return;
  for (uint32_t i = 0; i < iap_info.sector_size; i++)
    if (sim_written[(base - SIM_BASE) + i]) destroyed++;
  memset(sim_erased  + (base - SIM_BASE), 1, iap_info.sector_size);
  memset(sim_written + (base - SIM_BASE), 0, iap_info.sector_size);
}
static void iap_clear_upgrade_flag(void) { flash_sector_erase(iap_info.flag_address); }
static void lcd_draw_iap_status(const char *s) { (void)s; }
static void iap_respond(uint8_t *tx, uint16_t cmd, uint16_t res)
{ (void)tx; (void)cmd; last_response = res; }

/* Programs one HID block at the address the guard accepted, exactly as
   iap_data_write() would, and flags any byte that was not erased first or
   that is being programmed a second time. */
static void sim_program_block(uint32_t addr)
{
  for (uint32_t i = 0; i < HID_IAP_BUFFER_LEN; i++) {
    uint32_t off = (addr - SIM_BASE) + i;
    if (!sim_erased[off] || sim_written[off]) { violations++; return; }
    sim_written[off] = 1;
  }
}
"""

HARNESS_EPILOGUE = """
/* ---- test driver ------------------------------------------------------ */

/* Start a flash session the way the bootloader does: iap_start()/iap_idle()
   both set state and call the REAL iap_init(). Nothing here resets
   iap_erased_sector_address -- if iap_init() stops clearing it, the
   session-reset test must fail, and that is the point. */
static void session_reset(uint32_t flash_size_kb)
{
  memset(sim_erased, 0, sizeof sim_erased);
  memset(sim_written, 0, sizeof sim_written);
  violations = 0;
  destroyed = 0;
  stray_erases = 0;
  sim_flash_size_kb = flash_size_kb;
  iap_info.state = IAP_STS_START;
  iap_init();
}

/* Same, but preserving simulated flash contents -- models a second flash
   session on a device that was not power-cycled. */
static void session_restart(void)
{
  iap_info.state = IAP_STS_START;
  iap_init();
}

/* Feed one ADDR packet, in the on-wire big-endian layout iap_address() parses. */
static int send_addr(uint32_t addr)
{
  uint8_t pkt[8];
  pkt[0] = 0; pkt[1] = 0;
  pkt[2] = (uint8_t)(addr >> 24); pkt[3] = (uint8_t)(addr >> 16);
  pkt[4] = (uint8_t)(addr >> 8);  pkt[5] = (uint8_t)(addr);
  last_response = 0;
  iap_address(pkt, sizeof pkt);
  return last_response == IAP_ACK;
}

#define FLASH_1MB_KB 1024u   /* -> 2K sectors */
#define FLASH_128K_KB 128u   /* -> 1K sectors */

int main(void)
{
  uint32_t base = %(APP_ADDRESS)#x;

  /* 1. A full contiguous image, 1KB per ADDR, exactly as hid_flash.py sends it. */
  session_reset(FLASH_1MB_KB);
  printf("sector_size=%%u\\n", (unsigned)iap_info.sector_size);
  int blocks = %(BLOCKS)d, nacks = 0, erases_seen = 0;
  uint32_t prev_erased = IAP_ERASED_SECTOR_INVALID;
  for (int b = 0; b < blocks; b++) {
    uint32_t a = base + (uint32_t)b * HID_IAP_BUFFER_LEN;
    if (!send_addr(a)) { nacks++; continue; }
    if (iap_erased_sector_address != prev_erased) { erases_seen++; prev_erased = iap_erased_sector_address; }
    sim_program_block(a);
  }
  printf("contiguous_nacks=%%d\\n", nacks);
  printf("contiguous_erases=%%d\\n", erases_seen);
  printf("contiguous_violations=%%d\\n", violations);
  printf("contiguous_destroyed=%%d\\n", destroyed);

  /* 2. Writing the upper half of a sector that was never erased (the shape a
        partially-preserved settings hole would produce) must be refused. */
  session_reset(FLASH_1MB_KB);
  send_addr(base + iap_info.sector_size);              /* erase that sector   */
  printf("partial_preserve_ack=%%d\\n",
         send_addr(base + 4u * iap_info.sector_size + HID_IAP_BUFFER_LEN));

  /* 3. Sub-block alignment must be refused -- in BOTH the unerased and the
        already-erased case. The second is the one that isolates the alignment
        check: the sector is erased, so an alignment-blind guard accepts it and
        then programs a 1KB block straddling data already written at `base`. */
  session_reset(FLASH_1MB_KB);
  printf("misaligned_ack=%%d\\n", send_addr(base + 0x200u));

  session_reset(FLASH_1MB_KB);
  send_addr(base);                                     /* erases sector 0     */
  sim_program_block(base);
  printf("misaligned_in_erased_sector_ack=%%d\\n", send_addr(base + 0x200u));

  /* 4. Out-of-window addresses must be refused, both ends. Both probes are
        deliberately SECTOR-aligned: an unaligned probe gets refused by the
        alignment logic no matter what the bounds check does, which would let a
        missing bounds check pass unnoticed. */
  session_reset(FLASH_1MB_KB);
  printf("below_window_ack=%%d\\n", send_addr(base - 2u * iap_info.sector_size));
  printf("above_window_ack=%%d\\n", send_addr(%(APP_END_ADDRESS)#x));
  printf("stray_erases=%%d\\n", stray_erases);

  /* 5. A fresh session must not inherit the previous session's erase record.
        The restart goes through the real iap_init(); if that stops clearing
        iap_erased_sector_address, this write into the already-programmed upper
        half of sector 0 is wrongly accepted. */
  session_reset(FLASH_1MB_KB);
  send_addr(base);                                     /* erases sector 0     */
  sim_program_block(base);                             /* ...and programs it  */
  session_restart();                                   /* flash contents kept */
  int ack5 = send_addr(base + HID_IAP_BUFFER_LEN);
  if (ack5) sim_program_block(base + HID_IAP_BUFFER_LEN);
  printf("stale_record_ack=%%d\\n", ack5);

  /* 6. Same walk on a 1K-sector part (<256KB flash), where every block erases. */
  session_reset(FLASH_128K_KB);
  printf("sector_size_small=%%u\\n", (unsigned)iap_info.sector_size);
  int nacks_1k = 0;
  for (int b = 0; b < 64; b++) {
    uint32_t a = base + (uint32_t)b * HID_IAP_BUFFER_LEN;
    if (!send_addr(a)) nacks_1k++; else sim_program_block(a);
  }
  printf("sector1k_nacks=%%d\\n", nacks_1k);
  printf("sector1k_violations=%%d\\n", violations);
  printf("sector1k_destroyed=%%d\\n", destroyed);
  return 0;
}
"""


def build_harness(blocks: int) -> str:
    source = IAP_SOURCE.read_text()
    subs = dict(SDK_CONSTANTS)
    subs.update(
        {
            "APP_ADDRESS": APP_ADDRESS,
            "APP_END_ADDRESS": APP_END_ADDRESS,
            "SECTOR": SECTOR,
            "BLOCKS": blocks,
        }
    )
    parts = [HARNESS_PROLOGUE % subs]
    for name in EXTRACTED_FUNCTIONS:
        parts.append(f"\n/* ---- verbatim from hid_iap_user.c: {name}() ---- */\n")
        parts.append(extract_c_function(source, name))
        parts.append("\n")
    parts.append(HARNESS_EPILOGUE % subs)
    return "".join(parts)


class IapEraseGuardTests(unittest.TestCase):
    results: dict[str, int] = {}

    @classmethod
    def setUpClass(cls) -> None:
        if not IAP_SOURCE.exists():
            raise unittest.SkipTest(f"{IAP_SOURCE} not found")
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if cc is None:
            raise unittest.SkipTest("no host C compiler (cc/gcc/clang) available")

        harness = build_harness(blocks=498)
        with tempfile.TemporaryDirectory() as td:
            csrc = Path(td) / "iap_guard_harness.c"
            binary = Path(td) / "iap_guard_harness"
            csrc.write_text(harness)
            compile_proc = subprocess.run(
                [cc, "-O1", "-std=c11", "-Wall", "-o", str(binary), str(csrc)],
                capture_output=True,
                text=True,
            )
            if compile_proc.returncode != 0:
                raise AssertionError(
                    "the guard harness failed to compile -- this usually means "
                    "hid_iap_user.c gained a dependency the stubs don't provide.\n\n"
                    + compile_proc.stderr
                )
            run_proc = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
            if run_proc.returncode != 0:
                raise AssertionError(f"harness crashed:\n{run_proc.stderr}")

        cls.results = {
            k: int(v)
            for k, _, v in (line.partition("=") for line in run_proc.stdout.strip().splitlines())
            if v
        }

    # -- the regression that shipped through review on 2026-08-11 -------------

    def test_every_block_of_a_full_image_is_accepted(self) -> None:
        """498 x 1KB ADDR packets, 2K sectors: none may be NACKed.

        The 11e4f76 guard NACKed 249 of these -- every address that was not
        also sector-aligned -- which fails the flash on block 2.
        """
        self.assertEqual(
            self.results["contiguous_nacks"],
            0,
            "the bootloader rejected ADDR packets during a normal contiguous "
            "flash; a write-side guard is being gated on erase alignment again",
        )

    def test_each_sector_is_erased_exactly_once(self) -> None:
        """498KB over 2K sectors is 249 erases -- one per sector, not per block.

        Erasing per block would wipe the first half of each sector after it had
        just been written.
        """
        self.assertEqual(self.results["contiguous_erases"], 249)

    def test_no_byte_is_written_to_unerased_or_already_programmed_flash(self) -> None:
        """The property that actually matters, checked against simulated flash."""
        self.assertEqual(self.results["contiguous_violations"], 0)

    def test_no_erase_destroys_data_written_earlier_in_the_session(self) -> None:
        """An erase must never wipe bytes this session already programmed.

        Erasing per 1KB block instead of per 2K sector passes an alignment
        check and a NACK count, but silently wipes the first half of every
        sector right after writing it.
        """
        self.assertEqual(
            self.results["contiguous_destroyed"],
            0,
            "a sector erase wiped data programmed earlier in the same flash",
        )

    def test_the_geometry_under_test_is_the_real_one(self) -> None:
        """Guard against the harness quietly testing the wrong flash layout."""
        self.assertEqual(self.results["sector_size"], SECTOR)
        self.assertEqual(self.results["sector_size_small"], SDK_CONSTANTS["SECTOR_SIZE_1K"])

    # -- the invariants the guard is there to enforce -------------------------

    def test_partially_preserved_sector_is_refused(self) -> None:
        """Writing the upper half of a never-erased sector must NACK.

        Until 52a61d1 this invariant lived only in the host tooling
        (hid_flash.py validate_preserved_sectors); it is now enforced on-device.
        """
        self.assertEqual(self.results["partial_preserve_ack"], 0)

    def test_sub_block_alignment_is_refused(self) -> None:
        """An address that is not a whole HID buffer boundary must NACK.

        Checked twice: once in a virgin sector, and once inside a sector that
        was already erased -- the latter is what actually exercises the
        alignment check, since the erase-record branch would otherwise accept
        it and program a block straddling data already written.
        """
        self.assertEqual(self.results["misaligned_ack"], 0)
        self.assertEqual(self.results["misaligned_in_erased_sector_ack"], 0)

    def test_addresses_outside_the_write_window_are_refused(self) -> None:
        """Both probes are sector-aligned, so only the bounds check can reject them."""
        self.assertEqual(self.results["below_window_ack"], 0)
        self.assertEqual(self.results["above_window_ack"], 0)

    def test_no_sector_outside_the_app_window_is_ever_erased(self) -> None:
        """Only the app window and the upgrade-flag sector may be erased.

        The flag sector sits just below the app window by design; anything else
        outside it means the bootloader is erasing itself or the stock image.
        """
        self.assertEqual(self.results["stray_erases"], 0)

    def test_erase_record_does_not_survive_a_session_reset(self) -> None:
        """iap_init() must clear iap_erased_sector_address.

        Otherwise a new flash session could write into the second half of a
        sector erased by the previous one, over data already programmed there.
        """
        self.assertEqual(self.results["stale_record_ack"], 0)

    # -- the other flash geometry the SDK supports ---------------------------

    def test_one_kilobyte_sector_parts_still_flash(self) -> None:
        """Parts under 256KB use 1K sectors, where block size == sector size."""
        self.assertEqual(self.results["sector1k_nacks"], 0)
        self.assertEqual(self.results["sector1k_violations"], 0)
        self.assertEqual(self.results["sector1k_destroyed"], 0)

    # -- keep the duplicated SDK constants honest ----------------------------

    def test_vendor_constants_match(self) -> None:
        if not VENDOR_HEADER.exists():
            self.skipTest(f"{VENDOR_HEADER.name} not present (at32f403a_lib is gitignored)")
        header = VENDOR_HEADER.read_text()
        for name, expected in SDK_CONSTANTS.items():
            match = re.search(rf"#define\s+{name}\s+(0x[0-9A-Fa-f]+|\d+)", header)
            self.assertIsNotNone(match, f"{name} not found in {VENDOR_HEADER.name}")
            self.assertEqual(
                int(match.group(1), 0),
                expected,
                f"{name} in the vendor SDK no longer matches this test's copy",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
