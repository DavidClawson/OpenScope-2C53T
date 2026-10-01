# Verification levels

Use the weakest claim supported by the strongest completed row. Passing a lower
row never implies a higher one.

| Level | Required evidence | What may be claimed |
|---|---|---|
| Unit simulation | deterministic testbench pass plus a negative control for each editable block | the modeled RTL behavior passes those cases |
| Reference parse | expected input hashes and successful parse of the generated stock netlist | the named immutable artifact was read successfully |
| Structural match | both input hashes, identical canonical named connectivity, and matching primitive inventory | equality under that structural oracle only |
| Synthesis | pinned Yosys/tool revision, exact RTL commit, warnings reviewed, resource report retained | the RTL synthesizes for the selected device model |
| Place and route | exact GW1N-2 device/package, constraints, pinned nextpnr or Gowin EDA revision, routed design, and timing report | the candidate fits and meets the reported constraints |
| Bitstream round-trip | immutable input/output hashes and unpack/repack comparison | the tested encoding round-trips under the stated oracle |
| Hardware proof | exact board identity; SRAM-only load receipt; DONE/status readback; observed `0x04`/`0x05` frames; known waveform stimulus; negative control; reboot restores stock | only the exercised target behavior works on that board |

## Structural verifier

`tools/verify_structural_netlists.py` compares Yosys JSON after canonicalizing
named connectivity. It requires both SHA-256 values and reports primitive
inventories. A pass deliberately excludes behavioral, timing, placement,
routing, and bitstream equivalence.

Example:

```sh
python3 fpga/tools/verify_structural_netlists.py \
  --archived /path/to/reference/scope_unpacked.v \
  --archived-sha256 982a94ed69cadb16b45a18a06bc3412895a933f6e753f3260a16a1c8aca9ee32 \
  --fresh /path/to/fresh/scope_unpacked.v \
  --fresh-sha256 <fresh-sha256>
```

If Yosys is absent, this level is blocked. A matching line count or primitive
count is not a substitute.

## Hardware acceptance for a reconstructed capture path

Minimum acceptance is one named FNIRSI 2C53T board, loaded to volatile SRAM only:

1. Read back the GW1N-2 IDCODE `0x0120681B` and record board and loader identity.
2. Load the exact candidate to SRAM; record its hash and DONE/status result.
3. Drive a known DC level and periodic waveform into CH1 and CH2 separately.
4. Capture full 1026-byte `0x04` and `0x05` transactions and retain raw bytes.
5. Prove channel isolation, sample ordering, wrap, trigger/re-arm, and repeated
   acquisition against the known stimulus.
6. Negative control: disabling capture must leave memory and readiness stable;
   changing only CH1 must not produce the same change in CH2.
7. Power-cycle and prove the stock non-volatile design returns.

LCD traces, a green DONE pin, or one plausible buffer are supporting evidence,
not end-to-end proof. No NV-flash write belongs in this procedure.
