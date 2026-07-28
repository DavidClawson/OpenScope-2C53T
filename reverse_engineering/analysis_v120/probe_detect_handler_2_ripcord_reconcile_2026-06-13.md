# Reconcile: `probe_detect_handler_2` 0x0800bb10 ↔ ripcord `FUN_0800f908` — mis-cut resolved, no new execution evidence

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`) and the R-side clean-room tree
(`firmware/src/drivers/fpga.c`).
**Why this matters:** `0x0800bb10` is booked in `coverage_ledger.csv` as **D3/R1/V0,
FPGA_BLOCKED** with the note that it is the "scope trigger block" twin of
`probe_detect_handler_1` (same PC7 bit-7 → 0x07/0x0A decode, queues 0x16/0x17/0x18/0x19).
ripcord **confirms the decode and corrects the function boundary** — but **adds no execution
evidence**: no verified contract covers this address. This is an honest V0→V1 note, not a
V2 promotion. `new_evidence = false`.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `probe_detect_handler_2 @0x0800bb10` | inside `FUN_0800f908 @0x0800f908` |

`0x0800bb10 + 0x4000 = 0x0800fb10` (decimal 134282000), which lands **inside**
ripcord's `FUN_0800f908` (`0x0800f908`, size 950 B, ends `0x0800fcbe`). **osc's Ghidra
carved `0x0800bb10` out as a standalone `probe_detect_handler_2`; it is not one** — it is
the **case-block for one UI sub-mode** inside a single 950-byte command-queue producer.
`was_miscut = true`. General rule confirmed: `ripcord_runtime = osc_project + 0x4000`.

**No contract covers 134282000.** Nearest contracts: #18 `fpga_bitstream_upload`
[134392260, 134668495) starts *above* it; #19 `acq_engine_runtime` [134460500, 134467592)
is a different function (0x0803B454). `contract_id = null`, `contract_verified = false`.

---

## 1. CORRECT — `FUN_0800f908` is a UI-mode-dispatched command-queue producer; handler_2 is one case

ripcord's enclosing function resolves what osc could only see as a fragment. The real
function is a 10-way dispatch:

```
0x0800f908:  push  {r4,r5,r7,lr}
0x0800f90c:  movw  r0, #0xf8
0x0800f910:  movt  r0, #0x2000          ; r0 = 0x200000F8 = global state base
0x0800f914:  ldrb.w r0, [r0, #0xf68]    ; selector = *(u8*)0x20001060  (UI sub-mode byte)
0x0800f918:  cmp   r0, #9
0x0800f91a:  bhi.w #0x800fcce           ; default: return
0x0800f91e:  movw  r5, #0x2d6c
0x0800f922:  movt  r5, #0x2000          ; r5 = &0x20002d6c  (FreeRTOS queue handle ptr)
0x0800f926:  tbh   [pc, r0, lsl #1]     ; 10-way TableBranch on the sub-mode byte
```

Each case stores a 1-byte command into `[sp,#7]` and calls
`FUN_0803ecf0(_DAT_20002d6c, &byte, 0xffffffff)` — i.e. `xQueueSend(cmdQueue, &byte,
portMAX_DELAY)`. The queued bytes are FPGA **command/selector codes**, drained by a
downstream consumer (the USART2 command path — `_DAT_20002d6c` is the same kind of queue
handle the USART2 message bus uses, contract #11). Resolution of osc's flagged context:

| osc symbol | actual value | meaning |
|---|---|---|
| `*param_1` (handler_1/2 input) | `0x40011008` = **GPIOC IDT** read live in each case | probe-presence pin sense register (disasm: 5× `movw 0x1008; movt 0x4001; ldr` in the fn body; see Verification §) |
| `_DAT_20002d6c` | FreeRTOS queue handle at `0x20002d6c` | the FPGA command queue (1-byte items) |
| `DAT_20001060` / `state+0xf68` | UI sub-mode selector byte | picks which command-block to enqueue |
| `FUN_0803ecf0` | `xQueueSend(...,0xffffffff)` | blocking enqueue (timeout = portMAX_DELAY) |

The **handler_2 inner region** (`0x0800fb08`–`0x0800fb62`, osc `0x0800bb08`) decodes
exactly as osc stated:

```
0x0800fb08:  movw  r0,#0x1008 ; movt r0,#0x4001 ; ldr r0,[r0]   ; r0 = GPIOC->IDT
0x0800fb12:  movs  r1, #7
0x0800fb14:  lsls  r0, r0, #0x18        ; shift IDT bit7 → sign (N)
0x0800fb16:  it    pl
0x0800fb18:  movpl r1, #0xa             ; bit7 clear (no probe) → 0x0A; else r1 stays 0x07
0x0800fb1a:  ... strb [sp,#7]=r1 ; bl 0x803ecf0   ; enqueue prefix (0x07 | 0x0A)
0x0800fb2a:  movs r1,#0x16 ; ... enqueue 0x16
0x0800fb3c:  movs r1,#0x17 ; ... enqueue 0x17
0x0800fb4e:  movs r1,#0x18 ; ... enqueue 0x18
0x0800fb60:  movs r0,#0x19 ; b 0x800fcbc           ; enqueue 0x19 via the shared tail
```

**Ledger action:** keep D3; mark the boundary corrected — `0x0800bb10` is **not a standalone
function**, it is the trigger-block case inside the UI command-queue producer
`FUN_0800f908` (ripcord `0x0800f908`). osc's decode of *this region's bytes* needed no
correction; only the function attribution did.

---

## 2. CONFIRM — static agreement only (osc ↔ R-side ↔ ripcord decode), NOT execution-verified → V0 stays, propose V1

These claims are asserted by osc's decode, by ripcord's disassembly/peripheral census, and
by the R-side clean-room — **three static methods, zero execution**. There is no verified
contract covering this address, so the strongest honest tier is **V1 (multi-method static
agreement)**, not V2.

| claim | osc decode | ripcord decode | R-side (`fpga.c`) | tier |
|---|---|---|---|---|
| prefix byte chosen by GPIOC PC7 bit-7: set→0x07, clear→0x0A | yes | `ldr GPIOC->IDT; lsls #0x18; it pl; movpl 0x0A` | `fpga_probe_cmd_byte(): (GPIOC->idt & (1<<7)) ? 0x07 : 0x0A` | **V1 static-inferred** |
| trigger block = queue 0x16,0x17,0x18,0x19 after the prefix | yes | enqueues 0x16→0x17→0x18→0x19 via xQueueSend | `fpga_send_scope_runtime_blocks()` sends `…0x16,0x17,0x18,0x19` | **V1 static-inferred** |
| 0x16 = trigger level LSB | inferred | byte payload, value not fixed here | `fpga_timed_send_cmd(fpga_scope_trigger_lsb(ss), 0x16,…)` | **V1 static-inferred** |
| 0x18 = trigger mode/edge | inferred | byte payload | `fpga_timed_send_cmd(fpga_scope_trigger_mode_byte(ss),0x18,…)` | **V1 static-inferred** |
| commands enqueued via FreeRTOS queue, not direct SPI3/USART2 wire | (osc saw xQueueGenericSend) | `xQueueSend(_DAT_20002d6c,…,portMAX_DELAY)` | `xQueueSend(usart_tx_queue,…)` in `fpga_timed_send_cmd` | **V1 static-inferred** |

**Why not V2:** the burst/dispatch contract #19 (`acq_engine_runtime`, verified=1) covers a
**different** function (`0x0803B454`, the SPI3 acquisition task). It says nothing about this
UI command-queue producer at `0x0800f908`. The 0x16–0x19 codes are *internal command-queue
selector bytes* here; their **wire-level effect on the FPGA is unverified** (the queue
consumer would have to be emulated to even reach SPI3/USART2, and the FPGA's reply is
hardware-bound regardless). Keep V0 in the ledger or, at most, move to **V1** to record the
three-way static agreement. Do **not** claim execution.

---

## 3. GAP — R-side corrections for `firmware/src/drivers/fpga.c`

The clean-room reproduces the *bytes* faithfully but **diverges on the probe-prefix source**,
and structures the dispatch differently:

1. **Scope-path prefix is keyed off software trigger state, not the live PC7 pin.**
   Stock `probe_detect_handler_2` re-reads `GPIOC->IDT bit7` (physical probe-presence pin)
   *inside the trigger block* to pick 0x07/0x0A. The R-side `fpga_send_scope_runtime_blocks`
   calls `fpga_scope_prefix_cmd(ss)` →
   ```c
   return (ss->trigger.source == TRIG_SRC_CH2) ? 0x0A : 0x07;
   ```
   i.e. it keys the prefix on the **software trigger source field**, not the hardware pin.
   The R-side *does* have the correct hardware read — `fpga_probe_cmd_byte()` returns
   `(GPIOC->idt & (1<<7)) ? 0x07 : 0x0A` — but it is wired into the **meter wake preamble**
   (`fpga_send_meter_wake_preamble`), not the scope trigger block. **Correction:** the scope
   trigger-block prefix should read the PC7 pin (probe-present), matching stock; the
   CH1/CH2 selection in stock comes from the *range/bank* block (handler_1 / `fpga_wire_*`),
   not from this prefix byte. Until fixed, scope-mode 0x07/0x0A will track the configured
   trigger source instead of actual probe presence.

2. **Dispatch shape differs: stock is one 10-way TBH over a UI sub-mode byte; R-side splits
   into separate functions.** Stock `FUN_0800f908` is a single `tbh`-dispatched producer
   keyed on `*(u8*)0x20001060` that emits a *different command block per sub-mode*
   (probe/range, trigger, freq-counter, etc.) into one queue. The R-side spreads these
   across `fpga_send_scope_range_block` / `fpga_send_scope_runtime_blocks` /
   `fpga_send_meter_wake_preamble`. Functionally close; the GAP is that there is **no single
   UI-sub-mode → command-block table** mirroring the stock `0x20001060`-keyed dispatch, so
   the R-side cannot reproduce stock's exact per-sub-mode block ordering from one selector.

3. **handler_2's trailing codes past 0x19 are not modeled.** The same `FUN_0800f908` case
   continues past 0x19 into more enqueues (`0x1f`, `0x09`, then re-reads GPIOC IDT and
   queues `0x20`, …) — a longer block than osc cut. The R-side trigger block stops at 0x19.
   These tail codes (0x1f/0x09/0x20) are an unmapped extension of the trigger block; mark
   for follow-up decode before claiming the block is complete.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0800bb10` | boundary note | "probe_detect_handler_2" (standalone) | "inner trigger-block case of UI command-queue producer FUN_0800f908 (ripcord 0x0800f908, 950B); dispatched by 10-way TBH on UI sub-mode byte 0x20001060; enqueues prefix(PC7→0x07/0x0A) + 0x16/0x17/0x18/0x19 via xQueueSend; see probe_detect_handler_2_ripcord_reconcile" | §1 |
| `0800bb10` | V | V0 | **V1** (three-method static agreement: osc decode ↔ ripcord disasm/census ↔ R-side fpga.c; NOT execution — no contract covers this addr) | §2 |
| `0800bb10` | klass note | FPGA_BLOCKED | still FPGA_BLOCKED for wire-level effect; MCU-side decode resolved, but 0x16–0x19 are internal queue selector bytes — their FPGA semantics remain unverified | §2/§3 |

---

## 5. Honest scope

**ripcord adds no new execution evidence for this function — `new_evidence = false`.** What
it adds is purely static: (a) the **mis-cut correction** (this is one TBH case inside the
950-byte command-queue producer `FUN_0800f908`, not a standalone handler), and (b) **clean
context** (the input is GPIOC IDT, the sink is `xQueueSend` on queue handle `0x20002d6c`,
the selector is UI sub-mode byte `0x20001060`). osc's decode of the region's *bytes* was
already correct; ripcord confirms it and fixes the function boundary.

This does **not** promote to V2: the verified contracts (#19 acq engine, #18 bitstream
upload) cover other functions; none covers `0x0800fb10`. The 0x16/0x17/0x18/0x19 codes are
**internal command-queue selector bytes** at this layer — separating internal selectors from
wire-level transactions is exactly the discipline here, and the FPGA's actual response to
them remains hardware-bound. The real numerator-growth item is the R-side GAP: the scope
trigger-block prefix should read the live PC7 probe pin (as stock does), not the software
`trigger.source` field.

---

## 6. Recipe (per-function template)

1. **crosswalk** osc addr `+0x4000` → ripcord runtime addr; find the containing warehouse
   function (`functions` where `addr ≤ X < addr+size`) — catches mis-cuts.
2. **clean decode**: `disasm.py --start <containing_entry>` over the prologue + dispatch +
   `decompiled_c` + `peripheral_xrefs` to resolve `unaff_*`/`in_ZR` and identify sinks.
3. **execution evidence**: query `build/contracts.sqlite` for a `verified=1` contract whose
   `[addr_start,addr_end)` covers the addr. **If none does, `contract_id=null` and the claim
   stays static (V1 at best). Do not borrow another function's contract.**
4. **diff vs R-side**: locate the `firmware/src/**` equivalent; list CONFIRM / CORRECT / GAP.
5. **score + write**: propose D/R/V deltas; if ripcord added nothing past osc, say so.

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check against the ripcord warehouse + contract ledger.
Default posture was to overturn anything not solidly backed.

**Checked and CONFIRMED:**

- **Crosswalk (ok).** `functions WHERE source='stock_v120' AND addr ≤ 134282000 < addr+size`
  returns exactly `FUN_0800f908 @0x0800f908, size 950, end 0x0800fcbe`. Runtime
  `0x0800fb10` genuinely lands inside the claimed containing function. `was_miscut=true`
  is correct.
- **Contract reality (ok).** Draft cites `contract_id=null`. Confirmed: no contract in
  `build/contracts.sqlite` has `[addr_start,addr_end)` covering 134282000. The note does
  **not** borrow #18/#19 — it explicitly states they cover other functions. No hallucinated
  contract. No V2 promotion attempted.
- **Prologue + 10-way dispatch (ok).** `disasm.py` over `0x0800f908–0x0800f928` matches:
  `ldrb.w r0,[r0,#0xf68]` (selector = `*(u8*)0x20001060`), `cmp r0,#9; bhi.w`,
  `r5 = &0x20002d6c`, `tbh [pc, r0, lsl #1]` at `0x0800f926`. Confirmed instruction-for-
  instruction.
- **handler_2 prefix decode + 0x16/0x17/0x18/0x19 (ok).** `disasm.py` over
  `0x0800fb08–0x0800fb64`: `ldr GPIOC->IDT (0x40011008); lsls #0x18; it pl; movpl r1,#0xa`
  (PC7 clear→0x0A, else r1 stays 0x07), then four `xQueueSend(_DAT_20002d6c,&byte,-1)`
  with bytes 0x16,0x17,0x18,0x19. Sink `bl 0x803ecf0`, timeout `mov.w r2,#-1`
  (portMAX_DELAY) confirmed.
- **GAP #3 tail extension (ok).** `disasm.py` over `0x0800fb64–0x0800fbc0` confirms the
  block continues `0x00, 0x1f, 0x09`, then **re-reads `0x40011008` (GPIOC->IDT) at
  0x0800fb9e** and enqueues `0x20` (0x0800fba8), then `0x21`. Matches the GAP text.

**OVERTURNED / corrected in body:**

- **Evidence-provenance overstatement (fixed).** §1 and §2 cited
  "(peripheral_xrefs: GPIOC IDT READ ×5)". `peripheral_xrefs WHERE function_addr=134281992`
  returns **0 rows** — this raw-binary mis-cut function carries no classified xref rows at
  all (GPIOC IDT *does* appear in `peripheral_xrefs` for other functions, 28 GPIOC rows
  target-wide, just not this one). The *count itself is real* — `disasm.py` over the full
  body shows exactly 5 live `ldr 0x40011008` loads — so the conclusion stands, but the cited
  source was wrong. Both citations re-attributed to the disassembly. This is the single
  overturn: a CORRECT-tier observation with a hallucinated evidence pointer, not a wrong
  finding.

**Final approved scoring: D3 (keep), R1 (keep), V1.** V1 is defensible — three-method
static agreement (osc decode ↔ ripcord disasm ↔ R-side `fpga.c`), no execution. V2 was
correctly refused: no verified contract covers `0x0800fb10`, and the 0x16–0x19 codes are
internal command-queue selector bytes whose wire-level FPGA effect is unverified. No V2
claim to demote — the note already held the V1 line.
