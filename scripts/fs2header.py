#!/usr/bin/env python3
"""Convert a Gowin bitstream into the C header shape the firmware compiles in.

The firmware uploads its FPGA configuration from a `static const uint8_t[]`
compiled into the image (`firmware/src/drivers/fpga_cal_table.h`, extracted from
stock).  This script produces a header of the *same shape* from any other
bitstream, so an alternate image (e.g. a diagnostic design) can be built in via
`-DFPGA_ALT_BITSTREAM=1` without touching the default payload.

Input formats
-------------
  *.fs   Gowin ASCII bitstream — the format apicula writes and
         `~/gw1n2-apicula/tools/bin2fs.py` produces.  Each significant line is a
         run of '0'/'1' characters, MSB first, one command or frame per line;
         '//' comment lines and blank lines are ignored.  All bit lines are
         concatenated and packed MSB-first into bytes (total bits must be a
         multiple of 8 — a real Gowin stream is byte-aligned per line).
  *.bin  Raw bitstream bytes, already packed (use --raw, or let the .bin
         extension select it).

The output is byte-identical in structure to fpga_cal_table.h: a size macro, a
`static const uint8_t` array, 16 bytes per line, `0xNN` uppercase hex.

Usage
-----
    python3 scripts/fs2header.py IN.fs OUT.h [options]

    --symbol NAME     array name        (default: fpga_h2_cal_table)
    --size-macro NAME size macro name   (default: FPGA_H2_CAL_TABLE_SIZE)
    --name TEXT       human label baked in as FPGA_BITSTREAM_NAME
                      (default: input file basename)
    --guard NAME      include guard     (default: derived from OUT.h)
    --note TEXT       extra comment line in the header banner (repeatable)
    --notes-file F    file of extra comment lines, appended verbatim
    --placeholder     emit a `#warning` so a build that still carries the
                      stand-in payload says so at compile time
    --raw             force raw-bytes input regardless of extension

Sanity checks (warnings, not errors — an arbitrary diagnostic image is allowed
to look unusual): reports whether the Gowin sync word A5 C3 is present and what
IDCODE follows it (0x0120681B = GW1N-2 family, this board's GW1N-UV2).
"""

import argparse
import hashlib
import os
import sys


def read_fs_bits(path):
    """Parse a Gowin ASCII .fs into raw bytes."""
    bits = []
    lines_used = 0
    with open(path, "r") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("//") or line.startswith("#"):
                continue
            if set(line) - {"0", "1"}:
                bad = "".join(sorted(set(line) - {"0", "1"}))[:8]
                raise SystemExit(
                    f"{path}:{lineno}: not a bit line (unexpected chars {bad!r}). "
                    "Only '0'/'1' lines, '//' comments and blank lines are allowed."
                )
            if len(line) % 8:
                raise SystemExit(
                    f"{path}:{lineno}: {len(line)} bits is not a whole number of bytes"
                )
            bits.append(line)
            lines_used += 1
    joined = "".join(bits)
    if not joined:
        raise SystemExit(f"{path}: no bit lines found — is this really a .fs?")
    data = bytes(int(joined[i:i + 8], 2) for i in range(0, len(joined), 8))
    return data, lines_used


def describe(data):
    """Best-effort structural notes for the header banner."""
    notes = []
    sync = data.find(b"\xa5\xc3")
    if sync < 0:
        notes.append("NO a5c3 sync word found — not a recognisable Gowin bitstream.")
    else:
        notes.append(f"Gowin sync word a5c3 at offset 0x{sync:X}.")
        # bin2fs: sync is inside cmd_hdr; the IDCODE command (0x06) follows.
        idx = data.find(b"\x06\x00\x00\x00", sync)
        if idx >= 0 and idx + 8 <= len(data):
            idcode = int.from_bytes(data[idx + 4:idx + 8], "big")
            fam = " (GW1N-2 family — matches this board)" if idcode == 0x0120681B else ""
            notes.append(f"IDCODE 0x{idcode:08X}{fam}.")
        else:
            notes.append("IDCODE command (06 00 00 00) not located.")
    notes.append(f"sha256: {hashlib.sha256(data).hexdigest()}")
    return notes


def emit(data, out_path, symbol, size_macro, guard, name, banner, placeholder):
    with open(out_path, "w") as f:
        f.write("/* GENERATED FILE — do not edit by hand.\n")
        f.write(" * Produced by scripts/fs2header.py; regenerate rather than patching.\n")
        for line in banner:
            f.write(f" * {line}\n" if line.strip() else " *\n")
        f.write(" */\n\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        if placeholder:
            f.write(
                "#warning \"FPGA payload is the PLACEHOLDER bitstream — "
                "regenerate this header from the real .fs before flashing\"\n\n"
            )
        f.write(f"#define FPGA_BITSTREAM_NAME  \"{name}\"\n")
        f.write(f"#define {size_macro}  {len(data)}u\n\n")
        f.write(f"static const uint8_t {symbol}[{size_macro}] = {{\n")
        for i in range(0, len(data), 16):
            row = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
            f.write(f"    {row},\n")
        f.write("};\n\n")
        f.write(f"#endif /* {guard} */\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("infile")
    ap.add_argument("outfile")
    ap.add_argument("--symbol", default="fpga_h2_cal_table")
    ap.add_argument("--size-macro", default="FPGA_H2_CAL_TABLE_SIZE")
    ap.add_argument("--name", default=None)
    ap.add_argument("--guard", default=None)
    ap.add_argument("--note", action="append", default=[])
    ap.add_argument("--notes-file", default=None,
                    help="file whose lines are appended to the header banner verbatim")
    ap.add_argument("--placeholder", action="store_true")
    ap.add_argument("--raw", action="store_true")
    args = ap.parse_args()

    raw = args.raw or args.infile.lower().endswith(".bin")
    if raw:
        data = open(args.infile, "rb").read()
        lines_used = 0
    else:
        data, lines_used = read_fs_bits(args.infile)

    name = args.name or os.path.basename(args.infile)
    guard = args.guard or (
        os.path.basename(args.outfile).upper().replace(".", "_").replace("-", "_")
    )

    banner = [f"Source: {os.path.basename(args.infile)}"
              f"{'' if raw else f' ({lines_used} bit lines)'}",
              f"Size: {len(data)} bytes (0x{len(data):X})."]
    banner += describe(data)
    banner += list(args.note)
    if args.notes_file:
        with open(args.notes_file) as nf:
            banner += [l.rstrip("\n") for l in nf]

    emit(data, args.outfile, args.symbol, args.size_macro, guard, name,
         banner, args.placeholder)

    print(f"wrote {args.outfile}: {len(data)} bytes", file=sys.stderr)
    for line in describe(data):
        print("  " + line, file=sys.stderr)


if __name__ == "__main__":
    main()
