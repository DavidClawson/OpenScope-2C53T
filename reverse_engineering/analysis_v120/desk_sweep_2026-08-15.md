# Desk sweep — the 2026-08-15 open questions, answered from static evidence

Five parallel research agents over the stock decompile, the unpacked FPGA netlist and
the boot captures; every non-low-confidence claim then handed to an adversarial verifier
told to re-derive it from the primary artifact. 32 agents, 0 errors, 27 claims verified
(23 confirmed, 4 partially — every "partially" is a refinement, none a refutation).

Addresses are FLASH convention (stock app linked at `0x08007000`).
Full transcripts: workflow `wf_eec81089-623`.

## 1. Stock's runtime SPI3 path — there is exactly ONE

**Two code sites in the whole image touch SPI3** (`0x40003C08` = STS, DT at +4): the
inline config/upload block in `master_init` (`r6` loaded at `0x0802D548`) and the **fpga
task at `0x0803E454`** (`r7` at `0x0803E456`). No literal-pool `0x40003Cxx` exists
anywhere — the verifier even decompressed the Keil LZ77 RW-init image to rule out a
hidden pointer. So **every runtime byte the FPGA sees goes through one task**, fed by a
1-byte FreeRTOS queue at `0x20002D78` (`spi3_data`, 15×1 B).

The queued byte **is** the opcode. It is clocked out *before* any range check, then
`subs #1 / cmp #8 / bhi` selects a case via `TBH` at `0x0803E536`:

| op | handler | meaning |
|---|---|---|
| 01 | 0x0803E54C | timebase index |
| 02 | 0x0803E970 | channel enable mask |
| 03 | 0x0803E9F2 | status/live-sample read |
| 04 | 0x0803E5A4 | CH1 capture read |
| 05 | 0x0803E68C | CH2 capture read |
| 06 | 0x0803ED1C | trigger source channel |
| 07 | 0x0803ED5C | trigger edge |
| 08 | 0x0803E75C | trigger level |
| 09 | 0x0803E79C | — |

**The five boot "arm" writes are not a constant.** `master_init` (`0x0802DDC8`–
`0x0802DE2A`) just queues bytes 1,2,6,7,8; the task then emits `01 <ms[0x2d]>`,
`02 <ms[0x14]>`, `06 <ms[0x16]>`, `07 <ms[0x18]>`, `08 <trig>` **from current state**.
So `01 08 / 02 03 / 06 00 / 07 00 / 08 AD` are just the power-on defaults, not a magic
sequence.

## 2. Register semantics

- **`0x01` = TIMEBASE INDEX**, not a rate code. It is the same index that selects the
  time/div label from the 29-entry table at `0x0804C988`:
  `0`=10 nS/div … `8`=5 uS (boot) … `15(0x0F)`=1 mS, `16`=2 mS, `18(0x12)`=10 mS …
  `28`=20 S. Our measured ladder maps consistently at ~30 samples/div:
  `0x0F` 1 mS/div → 30 kS/s (bench 30 k), `0x0E` 500 uS → 60 k (60 k), `0x0D` 200 uS →
  150 k (150 k), `0x0C` 100 uS → 300 k (300 k). **The 1-2-5 shape we measured is the
  1-2-5 time/div menu.**
- **`0x02` = channel enable mask** `ms[0x14]` (bit0 CH1, bit1 CH2; sent as 3 if 0).
- **`0x06` = trigger source channel** `ms[0x16]`; **`0x07` = trigger edge** `ms[0x18]`.
  Written only on user change and at boot — never per frame.
- **`0x08` = trigger level** (already known).

## 3. How stock re-arms — and it is a reg-01 write

Stock issues **no per-frame writes at all**: in 5.4 s of the June capture, all 348
post-boot CS windows are 1026-byte `0x04`/`0x05` reads (174 each), no re-arm, no
timebase or channel write. Its *only* force/re-arm mechanism is a **pair of reg-01
writes, `01 <tb+1>` then `01 <tb>`**, issued by the 10 ms Timer1 callback
(`input_and_housekeeping`, `0x080400B8`) when **PC0 has stayed LOW** for
`tbl[tb]+50` ticks (table at `0x0804D833`: 510 ms for tb≤13, rising to 1320 ms at
tb=19), in Normal/Auto trigger modes only.

⇒ **A reg-01 write appears to restart the capture engine.** That is the cheapest
candidate for our one-shot/seam problem, and it needs no new firmware.

## 4. The analog front end — two corrections that explain today's bench

- **AC/DC coupling is PD12 (CH1) and PD13 (CH2); HIGH = DC, LOW = AC.** State bytes
  `ms[0x00]`/`ms[0x01]` (0=AC, 1=DC) at `0x200000F8/F9`; menu handler `0x0800BD60`
  item 2 toggles and writes GPIOD BSRR/BRR (`0x0800C846`…`0x0800C93E`); boot restores
  them at `0x0802C58E`. Confirmed non-AF in the Exp E SWD dump (GPIOD CRH nibbles
  PD12=PD13=1). **Our firmware never drives PD12 — that is exactly the ~9 Hz high-pass
  measured today.**
- **The CH2 front end is a pin-for-pin isomorph of CH1**: `gpio_mux_porta_portb`
  (`0x08008A58`, arg `ms[0x03]`) drives PA15/PB11/PB10/PA10 with the *same 10-case
  table* as `gpio_mux_portc_porte` (`0x080088A4`, arg `ms[0x02]`) drives
  PC12/PE4/PE5/PE6, under the mapping **PC12↔PA15, PE4↔PB11, PE5↔PB10, PE6↔PA10**.
  So **PA15 is CH2's input-connect relay** (the PC12 analog), HIGH for ranges 0-4 and
  LOW for 5-9. Our relay table drives PA15 LOW at range 8 — **the CH2 input is
  disconnected in exactly the state we tested CH2 in.**
- ⇒ **PB11 is a CH2 gain relay, not an FPGA control line.** Three independent supports:
  stock writes it only from CH2 range/cal functions; the netlist (below) has no MCU
  control input there; and on the bench today PB11 LOW did not stop capture.

## 5. Netlist — the capture path, and a belief that has to go

- **Two hard-wired ADC buses, no mux.** BSRAM_0 (`R10C2`, op `0x04`) takes its DIA bits
  from the IDDRC of the **left-edge** pads; BSRAM_3 (`R10C17`, op `0x05`) from the
  **right-edge** pads. There is no fabric path that routes CH1's converter into
  memory 2 — so "op 05 carries CH1" cannot be a fabric mode; it is an analog/front-end
  condition.
- **`R1C20_IB` — the pad this project has called "IOR1B = the run/re-arm line = PB11" —
  is the 8th data bit of the right-edge ADC bus.** The unpacker emits a plain IBUF
  instead of the corner tile's IDDRC, which is why BSRAM_3 `DIA7/DIA16` looked dangling.
  Its fanout signature (70 DFF.CE / 8 DFF.SET / 156 DFF.D) is identical to every other
  right-edge data pad. **The "IOR1B ∧ IOB7B ∧ SPI-bit" arm condition was an artifact of
  tracing an ADC data bit through the compare cone.** The only discrete non-SPI
  MCU→fabric input with fanout is `R19C7_IB`. (Also: `tools/m_capture.py` has been
  driving the right ADC bus 7 bits wide, missing this MSB.)
- **Fast vs slow buffers.** BSRAM_0/3 fill at the **raw GB20 clock**, 8-bit write
  address = 256 words × 2 DDR samples = **512 samples per channel per fill**, with
  `CEA = run & ~done`. BSRAM_1/2 share a **10-bit** counter on the **gated GB40** clock
  = 1024 words. Reg `0x01`'s forward cone reaches the GB70 domain and a 24-bit
  comparator against a GB70 counter (the divider), plus BSRAM_2 `DIA8` — i.e. **the
  rate control sits on the slow pair, not on the op-04/05 path.**
  ⚠ **Open tension:** the bench plainly shows reg `0x01` changing the sample spacing of
  op-04 data. Either the fast buffers are fed a decimated stream at slow timebases, or
  the read path switches to BSRAM_1/2 for slow indices. Unresolved — do not paper over.
- **PC0/DRDY** is a registered output whose D is the terminal count of the **shared
  BSRAM_1/2 write-address counter** ANDed with a 3-flop state — i.e. **PC0 tracks the
  slow record, not the fast buffers.**

## 6. Capture-decode correction (affects every MISO byte we have quoted)

The FPGA updates MISO **on the SCK rising edge**, so decoders that sample MISO at the
rising edge (`analyze_capture.py`, `dec.py`) race the transition. Re-decoded with the
pre-edge value:

- `0x3A` close returns **`FF FC`**, not `F8`.
- The `0x03` read returns **`80 00 <CH1 sample> <CH2 sample> <CH2>`** — a **live-sample
  readback**, not the config-state register `00 01 42 2E` we have been asserting.
- Each of the five arm writes returns `80 00`.
- **Every one of the 348 stock capture frames has header `80 00 00`** — constant, both
  channels. Our reads show a *varying* third byte (0x50–0xA9), which is now a
  stock-vs-ours difference rather than a stock feature to decode.
- **Stock's op 04 and op 05 return clearly different data** (04 flat at 164-169; 05 at
  54-59 then settling 76→83), confirming two converters on stock.
- Read cadence: 04 (5.14 ms) → 0.21 ms → 05 (5.49 ms) → ~18 ms idle; **pair period
  28.9 ms median**. Seams are undetectable in the capture because the input is flat.

## 7. Bench list this produces (cheapest first)

1. **`gpio set D12 1`** → DC coupling on CH1. One command; predicts the DC step that
   failed this morning now moves the mean. (And PD13 for CH2.)
2. **Revive CH2**: put the working generator lead in the CH2 jack, force **PA15 HIGH**
   (or use range ≤4), **PD13 HIGH**, and drive stock's channel-mask GPIO code
   **PC2 HIGH + PC1 LOW** (mask 3) — our firmware never drives PC1. Read op 05.
3. **One-shot / seam**: write `01 <tb+1>` then `01 <tb>` between reads (stock's own
   re-arm) and see whether frames freeze / the seam disappears.
4. **Stop holding PB11 HIGH** (it is a CH2 relay) and confirm capture is unaffected —
   then hand PB11 back to the CH2 range table.
5. **Re-measure the rate ladder as a timebase index** (0-28), and resolve the
   fast-vs-slow buffer tension in §5.
