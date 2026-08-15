# Reconstruction map

This map separates recovered facts from proposed editable boundaries. Names in
`fpga/rtl/` are ours; they are not recovered vendor module names.

## Runtime interface

The scope MCU reads one channel per SPI transaction while chip select remains
low for the full **1026-byte** window:

| Byte clocks | MOSI | MISO meaning |
|---|---|---|
| 0 | `0x04` for CH1 or `0x05` for CH2 | first status/marker byte |
| 1..2 | `0xFF` | two more status bytes |
| 3..1025 | `0xFF` | 1023 sample bytes |

The firmware duplicates sample 1022 into its 1024th UI slot; the wire frame
contains 1023 samples, not 1024. The `0x05` transaction must never be shortened:
a short `0x05` frame collides with the Gowin `ERASE_SRAM` configuration opcode,
whereas the 1026-byte shape is the observed CH2 read.

Structural tracing places all four BSRAM read paths behind one mux onto the
serial `SO` output. The exact read start address, stride, lane order, and status
semantics remain unproven. The SRAM-only R1 validator pattern is the intended
hardware oracle for those details.

## Recovered block map

| Evidence block | Recovered role | Proposed editable boundary | Confidence |
|---|---|---|---|
| SPI/SSPI input and `SO` mux | shifts commands/control and selects channel/status readback | `runtime_spi_frontend` | high structure, incomplete protocol |
| SPI-fed control flop plus run inputs | participates in capture enable/re-arm | `capture_control` | medium-high; polarity and complete register map unknown |
| large counter/control cone | sequences capture and read addresses | `capture_sequencer` | medium |
| `BSRAM_0` at `R10C2` | raw CH1 capture store on PLL clock | `capture_channel` instance | high role, byte mapping unknown |
| `BSRAM_3` at `R10C17` | raw CH2 capture store on PLL clock | `capture_channel` instance | high role, byte mapping unknown |
| `BSRAM_1` at `R10C5` and `BSRAM_2` at `R10C14` | shared SDR/computed path on a gated fabric clock; read-modify-write evidence | `slow_capture_path` | medium structure, function only a hypothesis |
| registered output from capture cone | likely data-ready/buffer status | `capture_status` | medium; board net and polarity need proof |
| PLL/global-clock network | clocks DDR input and capture memories | `clocking` wrapper | high existence, incomplete PLL configuration |

The generated reference contains 847 LUT4 primitives, 192 ALUs, four BSRAMs,
15 IDDRCs, and one ODDRC under the current raw primitive-inventory oracle.
Flip-flop totals depend on whether unpacker artifacts are excluded, so the
verifier reports each primitive class instead of publishing one ambiguous sum.
Counts help detect gross drift; they do not prove behavior.

## Evidence confidence ledger

| Claim | Evidence | Confidence / limit |
|---|---|---|
| target is GW1N-UV2/QN48, GW1N-2 family | physical marking, official device family, stream IDCODE | high identity; no board-net assignment implied |
| stock payload is the 115638-byte slice at `0x4AD19` | byte extraction and SHA-256 reproduction | high for the named archive only |
| `scope_unpacked.v` represents that stock design | pinned Apicula artifact and immutable hash | high provenance; generated and partly incomplete netlist |
| channel reads use full 1026-byte `0x04`/`0x05` windows | stock capture plus firmware bench path | high framing; status and sample ordering unresolved |
| `BSRAM_0/3` are raw CH1/CH2 stores | ADC-to-BSRAM and BSRAM-to-`SO` structural traces | high structural role; exact lane mapping unknown |
| `BSRAM_1/2` are a slow/computed path | SDR inputs, gated clock, shared counter, read-modify-write | medium; decimation/roll meaning is a hypothesis |
| handwritten `capture_channel` matches stock | unit simulation only | low equivalence confidence; no synthesis/P&R/hardware proof |

## Current editable RTL

`capture_channel.sv` models one circular sample memory with independent write and
read clocks. It intentionally exposes physical addresses and keeps ordering
policy above the primitive. It has no memory reset because clearing a BRAM array
would distort inference and is not supported by the recovered evidence.

This block is a clean implementation hypothesis, not a transliteration of a
specific `BSRAM_*` macro. The next integration layer must decide how trigger,
freeze, read-pointer translation, channel muxing, and CDC are represented.

## Explicit unknowns

- complete runtime opcode and writable-register map;
- exact polarity and board ownership of run, co-enable, and data-ready signals;
- QN48 board-pin mapping until official package data is reconciled with board
  continuity (proxy-package pin numbers are not acceptable);
- raw sample bit order, DDR lane meaning, signedness, first address, and wrap;
- meaning of all three status bytes in `0x04`/`0x05` reads;
- trigger FSM, threshold encoding, pre/post-trigger split, and re-arm sequence;
- whether `BSRAM_1/2` implement decimation, min/max, roll mode, or another
  computed record;
- PLL parameters, clock frequencies, phase relationships, and timing constraints;
- CDC behavior between sample, control-SPI, and readout domains;
- DMM and signal-generator islands and their interfaces;
- fit, routing, timing, power, analog fidelity, and hardware equivalence of the
  handwritten RTL.

Do not fill these gaps with convenient pin numbers or names from another Gowin
package. Record the evidence gap and add a falsifiable test.
