#!/usr/bin/env python3
"""
Extract simple FAT12 volumes from the dumped W25Q128 image.

This is intentionally narrow: it understands the 4KB-sector FAT12 layout
observed on the 2C53T external flash dump and is meant for offline analysis,
not general-purpose filesystem recovery.
"""

from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass
class DirEntry:
    name: str
    attr: int
    cluster: int
    size: int
    is_dir: bool


class Fat12Volume:
    def __init__(self, image: bytes, base: int):
        self.image = image
        self.base = base
        self.boot = image[base : base + 4096]
        if self.boot[3:11] != b"MSDOS5.0":
            raise ValueError(f"no FAT boot sector at 0x{base:X}")
        self.bytes_per_sector = struct.unpack_from("<H", self.boot, 11)[0]
        self.sectors_per_cluster = self.boot[13]
        self.reserved_sectors = struct.unpack_from("<H", self.boot, 14)[0]
        self.fat_count = self.boot[16]
        self.root_entries = struct.unpack_from("<H", self.boot, 17)[0]
        self.total_sectors_16 = struct.unpack_from("<H", self.boot, 19)[0]
        self.sectors_per_fat = struct.unpack_from("<H", self.boot, 22)[0]
        self.root_dir_sectors = (
            (self.root_entries * 32) + (self.bytes_per_sector - 1)
        ) // self.bytes_per_sector
        self.cluster_size = self.bytes_per_sector * self.sectors_per_cluster
        self.fat_off = base + self.reserved_sectors * self.bytes_per_sector
        self.root_off = base + (
            self.reserved_sectors + self.fat_count * self.sectors_per_fat
        ) * self.bytes_per_sector
        self.data_off = base + (
            self.reserved_sectors
            + self.fat_count * self.sectors_per_fat
            + self.root_dir_sectors
        ) * self.bytes_per_sector
        self.fat = image[self.fat_off : self.fat_off + self.sectors_per_fat * self.bytes_per_sector]

    def fat12_entry(self, cluster: int) -> int:
        off = (cluster * 3) // 2
        if cluster & 1:
            return ((self.fat[off] >> 4) | (self.fat[off + 1] << 4)) & 0xFFF
        return (self.fat[off] | ((self.fat[off + 1] & 0x0F) << 8)) & 0xFFF

    def cluster_chain(self, start: int) -> list[int]:
        chain: list[int] = []
        cluster = start
        while 2 <= cluster < 0xFF8 and cluster not in chain:
            chain.append(cluster)
            cluster = self.fat12_entry(cluster)
        return chain

    def read_cluster_chain(self, start: int) -> bytes:
        if start < 2:
            return b""
        out = bytearray()
        for cluster in self.cluster_chain(start):
            off = self.data_off + (cluster - 2) * self.cluster_size
            out.extend(self.image[off : off + self.cluster_size])
        return bytes(out)

    @staticmethod
    def _parse_lfn_piece(ent: bytes) -> str:
        chars: list[str] = []
        for a, b in ((1, 11), (14, 26), (28, 32)):
            for i in range(a, b, 2):
                code = ent[i] | (ent[i + 1] << 8)
                if code in (0x0000, 0xFFFF):
                    continue
                chars.append(chr(code))
        return "".join(chars)

    def parse_dir(self, buf: bytes) -> list[DirEntry]:
        entries: list[DirEntry] = []
        lfn_parts: list[str] = []
        for off in range(0, len(buf), 32):
            ent = buf[off : off + 32]
            if len(ent) < 32:
                break
            first = ent[0]
            if first in (0x00, 0xE5, 0xFF):
                lfn_parts = []
                continue
            attr = ent[11]
            if attr == 0x0F:
                lfn_parts.insert(0, self._parse_lfn_piece(ent))
                continue
            if attr == 0xFF:
                lfn_parts = []
                continue
            short_name = ent[:8].decode("ascii", "replace").rstrip()
            short_ext = ent[8:11].decode("ascii", "replace").rstrip()
            short = short_name + (("." + short_ext) if short_ext else "")
            name = "".join(lfn_parts) if lfn_parts else short
            lfn_parts = []
            cluster = struct.unpack_from("<H", ent, 26)[0]
            size = struct.unpack_from("<I", ent, 28)[0]
            entries.append(
                DirEntry(
                    name=name,
                    attr=attr,
                    cluster=cluster,
                    size=size,
                    is_dir=bool(attr & 0x10),
                )
            )
        return entries

    def root_entries_list(self) -> list[DirEntry]:
        size = self.root_dir_sectors * self.bytes_per_sector
        return self.parse_dir(self.image[self.root_off : self.root_off + size])


def sanitize_name(name: str) -> str:
    bad = '<>:"\\|?*'
    out = "".join("_" if ch in bad else ch for ch in name)
    return out.strip().rstrip(".")


def extract_tree(vol: Fat12Volume, out_dir: Path, root_entries: list[DirEntry]) -> list[dict]:
    manifest: list[dict] = []

    def walk(entries: list[DirEntry], current_dir: Path, rel_prefix: str) -> None:
        current_dir.mkdir(parents=True, exist_ok=True)
        for entry in entries:
            if entry.name in (".", ".."):
                continue
            safe = sanitize_name(entry.name)
            rel_path = f"{rel_prefix}/{safe}" if rel_prefix else safe
            target = current_dir / safe
            manifest.append(
                {
                    "path": rel_path,
                    "name": entry.name,
                    "attr": entry.attr,
                    "cluster": entry.cluster,
                    "size": entry.size,
                    "is_dir": entry.is_dir,
                }
            )
            if entry.is_dir:
                sub_buf = vol.read_cluster_chain(entry.cluster)
                sub_entries = vol.parse_dir(sub_buf)
                walk(sub_entries, target, rel_path)
            else:
                data = vol.read_cluster_chain(entry.cluster)
                if entry.size:
                    data = data[: entry.size]
                target.write_bytes(data)

    walk(root_entries, out_dir, "")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract FAT12 volumes from w25q128 dump")
    parser.add_argument("dump", type=Path, help="Path to raw W25Q128 dump")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("/Users/david/Desktop/osc/archive/w25q128_extract"),
        help="Directory to extract into",
    )
    parser.add_argument(
        "--bases",
        nargs="*",
        default=["0x0", "0x200000"],
        help="Volume base offsets to probe",
    )
    args = parser.parse_args()

    image = args.dump.read_bytes()
    args.out.mkdir(parents=True, exist_ok=True)
    summary: list[dict] = []

    for base_str in args.bases:
        base = int(base_str, 0)
        try:
            vol = Fat12Volume(image, base)
        except ValueError:
            continue
        root_entries = vol.root_entries_list()
        vol_dir = args.out / f"volume_{base:06X}"
        manifest = extract_tree(vol, vol_dir, root_entries)
        summary.append(
            {
                "base": f"0x{base:06X}",
                "bytes_per_sector": vol.bytes_per_sector,
                "sectors_per_cluster": vol.sectors_per_cluster,
                "total_sectors_16": vol.total_sectors_16,
                "volume_size_bytes": vol.total_sectors_16 * vol.bytes_per_sector,
                "root_entries": len(root_entries),
                "extract_dir": str(vol_dir),
                "manifest_entries": len(manifest),
            }
        )
        (vol_dir / "manifest.json").write_text(json.dumps(manifest, indent=2))

    (args.out / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"Extracted {len(summary)} volume(s) to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
