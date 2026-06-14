# Reconcile: `fpga_spi3_transfer` 0x08037800 ↔ ripcord `acq_engine_task` — corrected decode + V-promotion

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`) and Renode oracle run log
(`notes/renode-at32-bringup.md`, Runs 5–8).
**Why this matters:** `0x08037800` is one of the spine's load-bearing nodes and was
booked in `coverage_ledger.csv` as **D3/R1/V0, FPGA_BLOCKED (squeeze)** with the note
*"enters via code_r0x label with corrupt context (in_ZR, unaff_r7/r9, unaff_s16..s28) …
float scale/cal body decode-corrupt + needs live ADC stream to verify."* This note
**resolves the corruption** and **promotes the MCU-side structure from V0 to emulation-
confirmed (V2)** — without a bench. It is the worked template for doing the same to the
rest of the FPGA_BLOCKED bucket.

---

## 0. Address crosswalk (retire the manual `+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `fpga_spi3_transfer @0x08037800` | inside `acq_engine_task @0x0803B454` |

`0x08037800 + 0x4000 = 0x0803B800`, which lands **inside** ripcord's
`acq_engine_task` (`0x0803B454`, size 7016 B, 503 basic blocks). **osc's Ghidra carved
`0x08037800` out as a standalone function; it is not one** — it is an inner region of a
single 7 KB FreeRTOS task. That mis-cut is the direct cause of the corrupt register
context (see §1). General rule for cross-walking osc decode addresses to ripcord:
`ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — the "corrupt context" is a decode artifact of the mis-cut, fully resolvable

Ghidra decompiled `0x08037800` as an island reached by a computed branch, so it had no
prologue to source `r7`, `r9`, or `s16..s28` — hence `unaff_*`/`in_ZR`. ripcord's
disassembly of the **enclosing** prologue at `0x0803B454` (= osc `0x08037454`) sets all of
them:

```
0x0803B454:  sub   sp, #0x38
0x0803B456:  movw  r7, #0x3c08
0x0803B45A:  movw  r4, #0x2d78
0x0803B45E:  movw  sb, #0xf8           ; sb = r9
0x0803B462:  vldr  s18, [pc, #0x210]
0x0803B466:  vldr  s20, [pc, #0x210]
0x0803B46A:  vldr  s22, [pc, #0x210]
0x0803B46E:  vldr  s24, [pc, #0x210]
0x0803B472:  vldr  s26, [pc, #0x210]
0x0803B476:  vldr  s28, [pc, #0x210]
0x0803B47A:  movt  r7, #0x4000         ; r7 = 0x40003C08  → SPI3_STS
0x0803B480:  movt  r4, #0x2000         ; r4 = 0x20002D78  → acq command queue handle
0x0803B488:  movt  sb, #0x2000         ; r9 = 0x200000F8  → global state-struct base
0x0803B48C:  vmov.f32 s16, #-2.8e+01   ; s16 = -28.0      → ADC zero_offset
```

Resolution of every symbol osc flagged corrupt:

| osc symbol | actual value | meaning |
|---|---|---|
| `unaff_r7` | `0x40003C08` | **SPI3_STS** (status reg; the TDBE/RDBF polls are SPI3, not a generic timeout loop) |
| `unaff_r9` / `in_ZR` (sb) | `0x200000F8` | **global state-struct base** — so `state+0x5B0` etc. resolve (see §2) |
| `r4` | `0x20002D78` | acquisition command queue handle |
| `s16` | `-28.0` | ADC **zero_offset** — this is your `FPGA_ADC_OFFSET` |
| `s18..s28` | VFP cal constants | gain / clamp constants for the float calibration tail (§3) |

**Ledger action:** `0x08037800` decode note "corrupt context" → **resolved**; the function
is the SPI3-poll + capture-mode-dispatch + VFP-calibration body of `acq_engine_task`, not a
separate `fpga_spi3_transfer`. Keep D3.

---

## 2. CONFIRM — independent agreement, execution-verified → promote V0 → V2

These claims are asserted **both** by your static decode **and** by ripcord's Renode
execution oracle (contract #19 `acq_engine_runtime`, `verified=1`, conf 0.95; Runs 5–8).
Independent agreement from two methods, one of them execution, is the strongest evidence
tier you have — this is exactly the "emulation trace of stock execution" your `V` rubric
names as the best-case verifier, and it does **not** need the microscope/JTAG you booked
this row as blocked on.

| claim | your decode | ripcord execution evidence | verdict |
|---|---|---|---|
| burst sample buffer is `state+0x5B0` | `fpga_acquisition_task` comment: `ms[0x5B0 + i]` | Run 5/8: mode-4 burst wrote 1024 contiguous bytes to `0x200006A8 = 0x200000F8+0x5B0`, byte-exact | **CONFIRMED** |
| interleaved CH1/CH2 8-bit pairs, 512 each | case 3 reads 512×{ch1,ch2} | Run 6: cmd4/5 burst → `state+0x5B0`/`+0x9B0`, 1024 each = 2048 B two-half buffer | **CONFIRMED** |
| PB6 = per-command SPI3 CS, active-LOW | `SPI3_CS_ASSERT()=GPIOB->clr=PB6` | Run 8: GPIOB PB6 CLR asserted before each command's SPI3 activity, SCR at loop bottom; CS-gated sample pattern proved the firmware's PB6 write reaches the model | **CONFIRMED** |
| SPI3 @ `0x40003C00`, STS-polled TDBE/RDBF | `spi3_xfer` TDBE→DT→RDBF | prologue `r7=0x40003C08`; 233 SPI3 accesses traced | **CONFIRMED** |

**Ledger action:** for the acquisition-read **structure** (dispatch, CS framing, buffer
addresses, channel interleave) move **V0 → V2 (emulation-confirmed)**. Residual still-V0:
the real FPGA *reply byte values* — genuinely hardware-bound (the oracle's stated ceiling,
and consistent with your `FIRMWARE_FINDINGS_2026_05_30` MISO-inert silicon result).

---

## 3. GAP — R-side corrections for `firmware/src/drivers/fpga.c`

ripcord's execution-verified decode shows three places where the clean-room
`fpga_acquisition_task` diverges from stock. These are concrete numerator-growth items
(reimplement-faithfully), not blockers:

1. **Calibration is a VFP float path, not integer offset+clamp.** Your case-3 loop does
   `cal = raw + FPGA_ADC_OFFSET; clamp[0,255]` (integer). Stock's tail (the `s16..s28`
   body) is: re-add `zero_offset` (`s16 = -28.0`), **multiply by a gain** (`/150.0` for
   burst, `/192.0` for roll — contract #19), then clamp to `[0.0, 255.0]` in float before
   the byte store. Your `FPGA_ADC_OFFSET` matches `s16`, but the gain scaling is missing —
   waveforms will be amplitude-wrong vs stock until the `/150.0` (burst) `//192.0` (roll)
   divide is reinstated.

2. **The acquisition trigger is a USART2-command→semaphore→TBH dispatch, not an SPI
   `0x80|range` prefix.** Your Transaction-1 (`spi3_xfer(0x80 | voltage_range)` "tell the
   FPGA what to acquire") is not what stock does and is the same *command-first* shape that
   `FIRMWARE_FINDINGS_2026_05_30` already showed returns MISO `0xFF` on silicon. Stock:
   `FPGA→USART2 1-byte command` raises a binary semaphore at `0x20002D7C` that wakes the
   task; the task then dispatches via a **9-way TableBranch (TBH) at `0x0803B536` keyed on
   `(USART2 cmd byte − 1)`**. The SPI3 read mode is selected by that command, not by an
   SPI prefix byte. Re-model the trigger path accordingly.

3. **Only 3 of 9 capture modes are implemented.** Your switch handles cases 2/3/4
   (roll / normal / dual). ripcord Run 6–7 execution-verified all nine:

   | cmd | execution-verified effect |
   |---|---|
   | 1 | range-gate: read `range_index=state+0x2D`, index flash LUT `0x0804D833`, debounce vs `state+0xDB8`; SPI3 `01 05` hold / `01 06` advance / `01 12` clamp |
   | 2 | SPI3-write `state+0x14` (`02 55`) |
   | 3 | roll → `state+0x482`/`+0x5AF`, fill-count `state+0xDB6` |
   | 4/5 | burst → `state+0x5B0`/`+0x9B0` (1024 contiguous each) |
   | 6/7 | write `state+0x16`/`+0x18` (`06 66` / `07 77`) |
   | 8 | computed trim from `state+0x1C` written to SPI3 (`08` …) — MCU→FPGA write, not cal read-back |
   | 9 | `state+0x46` 16-bit ADC-ref; full 6-byte `09 FF FF 0A FF FF` with mid-sequence PB6 pulse |

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08037800` | note | "corrupt context … decode-corrupt" | "inner region of acq_engine_task (ripcord 0x0803B454); context resolved — r7=SPI3_STS, r9=state base, s16=-28.0 zero_offset; see acq_engine_task_ripcord_reconcile" | §1 |
| `08037800` | V (read structure) | V0 | **V2** (emulation-confirmed, ripcord contract #19 / Runs 5–8) | §2 |
| `08037800` | klass note | FPGA_BLOCKED "needs microscope/JTAG" | partially unblocked by emulation; residual = real FPGA reply *values* only | §2 |

**Honest scope:** this does not crack the FPGA secret — the FPGA's *reply values* remain
silicon-bound, matching your MISO-inert finding. What it does: kills the "decode-corrupt"
verdict, converts the MCU-side acquisition **structure** from unverified to emulation-
confirmed, and hands `fpga.c` three faithful-reimplementation corrections (gain scaling,
trigger-dispatch model, 6 missing modes).

---

## 5. Recipe (this is the per-function template the batch workflow fans out)

1. **crosswalk** osc addr `+0x4000` → ripcord runtime addr; find the containing warehouse
   function (`functions` where `addr ≤ X < addr+size`) — catches mis-cuts.
2. **clean decode**: `scripts/analysis/disasm.py --target stock_v120 --start <addr>` over
   the prologue + `decompiled_c` + `peripheral_xrefs` to resolve any `unaff_*`/`in_ZR`.
3. **execution evidence**: query `build/contracts.sqlite` for a `verified=1` contract whose
   `[addr_start,addr_end)` covers the addr; extract its verified sub-claims.
4. **diff vs R-side**: locate the `firmware/src/**` equivalent; list CONFIRM / CORRECT / GAP.
5. **score + write**: propose D/R/V deltas and emit `analysis_v120/<name>_ripcord_reconcile.md`.
