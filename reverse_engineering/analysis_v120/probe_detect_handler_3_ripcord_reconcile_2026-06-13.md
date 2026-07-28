# Reconcile: `probe_detect_handler_3` 0x0800bc98 ↔ ripcord `FUN_0800f908` — mis-cut resolved, context decoded, NOT execution-verified

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`): `functions` /
`decompiled` / `peripheral_xrefs` tables, `scripts/analysis/disasm.py`, and the
execution-verified contract ledger (`build/contracts.sqlite`).
**Why this matters:** `0x0800bc98` was booked in `coverage_ledger.csv` as
**D3/R1/V0, FPGA_BLOCKED** with the note *"Minimal (36B): reads *param_1 (GPIOC IDR),
applies the same PC7 bit-7 test (`-1 < *param_1<<0x18`) -> cmd=0x07 if probe present
else 0x0A, then queues that single byte via xQueueGenericSend and return."* This note
**confirms the byte-level decode is exactly right**, **resolves `*param_1` to a concrete
GPIOC register**, and **corrects the function boundary** — `0x0800bc98` is *not* a
standalone 36-byte function; it is one case of a 10-way FreeRTOS-queue dispatcher. There
is **no execution evidence** for this addr, so it stays **V0** (V1 on the structural
agreements). Honest outcome: ripcord adds boundary + register resolution, not verification.

---

## 0. Address crosswalk (retire the manual `+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `probe_detect_handler_3 @0x0800bc98` | **inside** `FUN_0800f908 @0x0800f908` (size 950 B) |

`0x0800bc98 + 0x4000 = 0x0800fc98`, which lands **+0x390 inside** ripcord's
`FUN_0800f908` (`0x0800f908`, 950 B), **not** at a function entry. **osc's Ghidra carved
`0x0800bc98` out as a standalone `probe_detect_handler_3`; it is not one** — it is the
tail region of a single 10-way dispatch function (the `param_1`/`unaff` artifact is the
direct symptom of the mis-cut; the real switch has no parameter, it reads a global).
`was_miscut = true`. General rule: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — boundary + context resolution (ripcord's net-new contribution)

**(a) It is a switch case, not a 36 B leaf with a `param_1`.** The enclosing
`FUN_0800f908` is a 10-way dispatch on a global state byte, queuing one command byte per
case onto a FreeRTOS queue. Prologue (ripcord disasm `0x0800f908`):

```
0x0800F908:  push  {r4,r5,r7,lr}
0x0800F90C:  movw  r0,#0xf8 ; movt r0,#0x2000   ; r0 = 0x200000F8  (global state base)
0x0800F914:  ldrb.w r0,[r0,#0xf68]              ; selector = state+0xF68  (= DAT_20001060)
0x0800F918:  cmp   r0,#9 ; bhi 0x800fcce        ; >9 -> default/return
0x0800F91E:  movw  r5,#0x2d6c ; movt r5,#0x2000 ; r5 = 0x20002D6C  (queue handle ptr)
0x0800F926:  tbh   [pc,r0,lsl #1]               ; 10-way TableBranch
```

So the osc `param_1` is an artifact of the island decode. The real selector is the
state-struct byte at `state+0xF68` (`DAT_20001060`); the queue handle is `_DAT_20002D6C`.

**(b) `*param_1` resolves to GPIOC IDT `0x40011008`.** ripcord disasm at the osc cut point:

```
0x0800FC90:  movw r0,#0x1008 ; movt r0,#0x4001  ; r0 = 0x40011008  = GPIOC->IDT
0x0800FC98:  ldr  r0,[r0]                       ; read GPIOC input data register
0x0800FC9A:  lsls r0,r0,#0x18                   ; PC7 -> sign bit (N)
0x0800FC9C:  mov.w r0,#7
0x0800FCA0:  it pl ; movpl r0,#0xa              ; bit7 CLEAR -> 0x0A ; bit7 SET -> 0x07
0x0800FCA4:  b 0x800fcbc                        ; -> strb + xQueueGenericSend(handle,&byte,-1)
```

`peripheral_xrefs` for `FUN_0800f908` corroborates: the only MMIO it touches is **GPIOC
IDT, READ ×4** (from_addr 0x800fb10/0x800fba6/0x800fc00/0x800fc98) — the probe-sense
reads. ripcord thus resolves the one register osc left as an unnamed pointer.

**Ledger action:** `0x0800bc98` note "reads *param_1 (GPIOC IDR)" → **resolved to GPIOC
IDT `0x40011008`, PC7**; and "Minimal (36B)" → **inner region of the 10-way dispatch
`FUN_0800f908` (ripcord `0x0800f908`)**. Keep D3.

---

## 2. CONFIRM — independent agreement (static-only → **V1**, NOT V2)

These are asserted by **both** osc's static decode **and** ripcord's static decode. There
is **no `verified=1` contract covering `0x0800fc98`** (the nearest acquisition contracts
#18/#19 start at `0x0803A...`/`0x0803B...`; none of `[addr_start,addr_end)` covers
134282392), so this is **two-static-method agreement = V1**, not execution-confirmed.

| claim | osc decode | ripcord decode | tier |
|---|---|---|---|
| PC7 bit-7 test drives the command byte | `-1 < *param_1<<0x18` | `lsls #0x18`+`it pl` on GPIOC IDT 0x40011008 | static-inferred, **V1** |
| PC7 **set** → `0x07` (probe present); **clear** → `0x0A` | `0x07 if probe present else 0x0A` | `movpl r0,#0xa` fires when N clear (bit7=0) | static-inferred, **V1** |
| byte is queued, not SPI-written here | `xQueueGenericSend` single byte | `bl 0x803ecf0(handle,&byte,0xffffffff)` = `xQueueGenericSend`, portMAX_DELAY | static-inferred, **V1** |

**Wire-level vs selector discipline:** `0x07`/`0x0A` here are *internal command bytes
enqueued onto a FreeRTOS dispatch queue* — they are **not** observed SPI3/USART2 wire
transactions at this site. Whether/how a consumer task later puts them on the FPGA wire,
and the FPGA's reply, are unobserved and hardware-bound. Do **not** promote `0x07`/`0x0A`
to wire-level facts from this function.

**Ledger action:** keep **V0** for the row's headline verdict (no execution evidence);
optionally annotate the three sub-claims above as **V1 (two-static-method agreement)**.

---

## 3. GAP — R-side corrections for `firmware/src/drivers/fpga.c`

The clean-room `fpga_probe_cmd_byte()` (fpga.c:808) matches the **bit test** exactly but
diverges from stock on **structure**:

1. **Polarity + value: byte-exact match.** `(GPIOC->idt & (1U<<7)) ? 0x07 :
   FPGA_CMD_METER_NOPROBE` with `FPGA_CMD_METER_NOPROBE == 0x0A` (fpga.h:61) reproduces
   stock's PC7-set→0x07 / PC7-clear→0x0A precisely. **No correction needed here.**

2. **Delivery path diverges: synchronous SPI send vs FreeRTOS queue.** R-side consumes
   the byte inside `fpga_send_meter_wake_preamble()` via
   `fpga_timed_send_cmd(0x05, probe_cmd, 10)` — a direct, blocking SPI framing with a
   `0x05` cmd-hi prefix. **Stock does not send it here:** it `strb`s the bare byte and
   `xQueueGenericSend`s it onto the dispatch queue `_DAT_20002D6C` with `portMAX_DELAY`,
   decoupling probe-detect from the wire. To match stock control flow, the probe byte
   should be **enqueued** to the same command queue the rest of `FUN_0800f908` feeds, not
   pushed synchronously down SPI3 with a `0x05` prefix at the detect site.

3. **Missing enclosing dispatcher.** Stock's `FUN_0800f908` is a **10-way queue
   dispatcher** (selector = `state+0xF68`) that emits, per case, a single queued command
   byte; the probe-detect path is one terminal case. The R-side has the probe helper but
   **not** this selector-driven queue dispatcher. Reimplementation item: model
   `FUN_0800f908` as a `state+0xF68`-keyed switch that enqueues command bytes (cases
   observed in the decompile: 0/1/3/4/5/8/9 → `0x00`; case 2 → `0x02`; case 6 → `0x29`;
   case 7 → `0x15`; probe-detect tail → `0x07`/`0x0A`).

   **CORRECTION (adversarial verify 2026-06-13):** the earlier draft claimed the
   probe-detect path queues a *trailing* `0x2C` (`FPGA_CMD_CONT_DIODE`) immediately after
   the probe byte. **This is wrong.** The probe path branches `0x0800fca4: b 0x800fcbc`,
   which jumps **past** `0x0800fcba: movs r0,#0x2c` straight to `0x0800fcbc: strb.w
   r0,[sp,#7]` with the probe byte (0x07/0x0A) still in r0, queues that one byte, then
   `pop {…,pc}` returns. The `0x2C` enqueue belongs to a **different, fall-through case**
   (the one that queues `0x00` at 0x800fca6–0xfcb6 and then sets/queues 0x2C), not the
   probe-detect tail. The probe case queues a **single** byte (0x07/0x0A) and returns.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0800bc98` | note | "Minimal (36B): reads *param_1 (GPIOC IDR) …" | "inner region of 10-way queue dispatcher FUN_0800f908 (ripcord 0x0800f908); *param_1 resolved = GPIOC IDT 0x40011008 PC7; queues a SINGLE byte 0x07(set)/0x0A(clear) via xQueueGenericSend(_DAT_20002D6C,portMAX_DELAY) then returns (no trailing 0x2C on this path — that belongs to a sibling case); selector=state+0xF68. NOT execution-verified. See probe_detect_handler_3_ripcord_reconcile" | §1 |
| `0800bc98` | D | D3 | D3 (unchanged) | — |
| `0800bc98` | R | R1 | R1 (unchanged; bit test already byte-exact in fpga.c:808) | §3.1 |
| `0800bc98` | V | V0 | **V0** (no covering verified contract; sub-claims are V1 two-static-agreement only) | §2 |
| `0800bc98` | klass | FPGA_BLOCKED | FPGA_BLOCKED (unchanged — no emulation reached this addr) | §2 |

**Honest scope / new_evidence:** ripcord does **not** execution-verify this function — no
`verified=1` contract covers `0x0800fc98`, so the row stays **V0**. What ripcord *does*
add over the existing osc row: (1) corrects the function boundary (mis-cut island →
inner region of the 10-way dispatcher `FUN_0800f908`), (2) resolves `*param_1` to a
concrete register (GPIOC IDT `0x40011008`, confirmed by `peripheral_xrefs` GPIOC-IDT-READ),
and (3) hands `fpga.c` two faithful-reimplementation items (queue the byte instead of
SPI-sending it; supply the missing selector-driven dispatcher + paired `0x2C`). The byte
values `0x07`/`0x0A` remain **internal enqueued command codes**, not verified wire
transactions, and the FPGA's reaction is hardware-bound. `new_evidence = true` (boundary +
register resolution), but strictly static — no V-promotion.

---

## Verification (adversarial, 2026-06-13)

Re-checked every load-bearing claim against the ripcord warehouse. Skeptical default.

**Checked and HELD:**
- **Crosswalk.** `functions` (stock_v120): 0x800fc98 (134282392) lands inside
  `FUN_0800f908 @0x800f908`, size 950 (ends 0x800fcde). `was_miscut=true` and the
  `+0x4000` rule both confirmed. `crosswalk_ok=true`.
- **CORRECT(b) register decode.** `disasm.py --start 0x0800fc90` reproduces the block
  byte-for-byte: `movw r0,#0x1008 / movt r0,#0x4001` (=0x40011008 GPIOC IDT), `ldr r0,[r0]`,
  `lsls r0,r0,#0x18`, `mov.w r0,#7`, `it pl / movpl r0,#0xa`. PC7→0x07(set)/0x0A(clear)
  confirmed.
- **CONFIRM tier discipline.** No `verified=1` contract covers 0x800fc98 (contract #18
  starts at 0x802a9c4 > 0x800fc98; nearest acq contracts #18/#19/#13 are all > 0x802xxxx).
  `contract_id=null` is accurate — no hallucinated contract. The three CONFIRM sub-claims
  are two-static-method agreement only → correctly held at **V1**, not promoted to V2.
  Headline **V0** stands (no execution evidence). No V-demotions needed (nothing was V2).
- **Queue-not-SPI.** `bl 0x803ecf0(handle,&byte, r2=-1)` confirmed; portMAX_DELAY framing
  consistent with xQueueGenericSend (name attribution remains static-inferred V1).

**OVERTURNED:**
- **"trailing 0x2C immediately after the probe byte" — FALSE.** The probe path executes
  `0x0800fca4: b 0x800fcbc`, which jumps **past** `0x0800fcba: movs r0,#0x2c` to
  `0x0800fcbc: strb.w r0,[sp,#7]` with the probe byte still in r0, queues that **one**
  byte, and returns (`pop {…,pc}`). The 0x2C enqueue is a **separate fall-through case**
  (the 0x00→0x2C path at 0x800fca6–0xfcba), not the probe tail. Note body §3.3,
  resolved_symbols(0x2C), and the ledger-delta note are corrected above. The probe case
  queues a SINGLE byte.
- **"GPIOC IDT READ ×5" — miscount, actual ×4.** `peripheral_xrefs` for FUN_0800f908
  shows exactly 4 GPIOC IDT READs (from_addr 0x800fb10/0x800fba6/0x800fc00/0x800fc98),
  and GPIOC IDT is indeed the only MMIO peripheral the function touches. Corrected to ×4.

**FINAL approved verdict:** **D3 / R1 / V0** (sub-claims V1 two-static-agreement).
Unchanged from the draft's headline — the draft's V-discipline was sound; the overturns
are a control-flow factual error (0x2C) and an evidence miscount (×5→×4), not a tier
inflation. The boundary fix and register resolution stand.
