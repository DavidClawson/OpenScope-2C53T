# Provenance and reproducibility

## Immutable stock inputs

The canonical source is the stock MCU application archive, not a generated
Verilog file:

| Artifact | Size | SHA-256 | Provenance |
|---|---:|---|---|
| `APP_2C53T_V1.2.0_251015.bin` | 751232 bytes | `a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760` | FNIRSI V1.2.0 application archive |
| raw FPGA slice | 115638 bytes | `5a0e73384e496bdb3b3d591b852bec2e806e70cbc71439c9829695324efd5c3b` | exact bytes at file offset `0x4AD19` in the archive |

Reproduce the second hash without creating a file:

```sh
dd if='APP_2C53T_V1.2.0_251015.bin' bs=1 \
  skip=$((0x4AD19)) count=115638 status=none | sha256sum
```

The stream header identifies the device family with IDCODE `0x0120681B`.
Physical inspection identifies the installed part as `GW1N-UV2`, package
`QN48`; `GW1N-UV2` uses the GW1N-2 fabric/tool device.

The stock payload and generated netlist may retain FNIRSI rights. Their presence
or reproducibility does not grant a redistribution license. Keep provenance and
licensing separate.

## Published reference artifacts

The immutable published reference is
`DavidClawson/gw1n2-apicula@7e44f773ccdabe8cce16b13d85b491e1321c8949`:

| Path at that commit | Size | SHA-256 | Meaning |
|---|---:|---|---|
| `tools/m5/scope.fs` | 925842 bytes | `f2fde7b6458bacb4d8d607e97277531e36823a715bcb2bee11380a082742fd44` | ASCII-form configuration stream; decoded payload matches the raw FPGA-slice hash above |
| `tools/m5/scope_unpacked.v` | 2179558 bytes | `982a94ed69cadb16b45a18a06bc3412895a933f6e753f3260a16a1c8aca9ee32` | Apicula-generated primitive netlist |

`scope_unpacked.v` is staged evidence, not source truth. It is a single anonymous
top-level fabric netlist made of Gowin primitives. It preserves useful
connectivity, LUT initialization, memories, and I/O-cell structure, but not the
original hierarchy, names, comments, intent, clock constraints, or HDL. Some
primitive inputs and PLL semantics are not recovered. Editing it would create a
gate patch, not maintainable reconstructed RTL.

## Tool revisions

Pin every reproduction rather than following moving branch heads:

| Purpose | Repository branch | Commit |
|---|---|---|
| stock unpack and GW1N-2 chipdb baseline | `DavidClawson/apicula`, `gw1n2-build` | `2987a6a323107e54d02b2d47a452f0fd09c95a07` |
| GW1N-2 pinout/IO development | `DavidClawson/apicula`, `gw1n2-io` | `cf5ab40d203a3f79dc233bdd8000b7c5fc2dfacb` |
| GW1N-2 place-and-route development | `DavidClawson/nextpnr`, `gw1n2` | `59c8f93a62ee17adc8d1c5d7f399697ffeb85c0f` |
| exact GW1N-UV2/QN48 chipdb, packing, and the debug-clock image build | `Komzpa/apicula`, `agent/gw1n2-qn48-chipdb-20260815` | `d978cad` (atop `cea1618`) |

The chipdb build requires Gowin EDA device data for `GW1N-1P5C`:
`GW1N-1P5C.fse`, `.dat`, and `.tm`. Those vendor inputs are not checked in.
Without the exact generated `GW1N-2.msgpack.xz`, a fresh unpack is blocked and
must not be reported as reproduced.

The expected unpack command at the pinned baseline is:

```sh
PYTHONPATH=/path/to/apicula python3 -m apycula.gowin_unpack \
  -d GW1N-2 scope.fs -o scope_unpacked.v
sha256sum scope_unpacked.v
```

An exact byte hash is the strongest reproduction result. If emission order
changes across Python or Apicula revisions, use the repository's structural
verifier and report only structural equality; never silently substitute that
for a byte-identical result.
