# Reconcile: `probe_detect_handler_1` 0x0800ba06 ↔ ripcord `FUN_0800f908` case 6 — corrected cut + register resolve

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`). Disasm via `scripts/analysis/disasm.py`,
decompile + `peripheral_xrefs` from the warehouse.
**Why this matters:** `0x0800ba06` was booked in `coverage_ledger.csv` as
**D3/R1/V0, FPGA_BLOCKED**, with a note that already had the GPIOC/PC7 probe-detect
logic essentially right. This reconcile confirms that logic against the ripcord decode,
**fixes the function boundary** (it is not a standalone handler — it is one `case` of a
10-way UI-dispatch switch), **resolves the `*param_1` register** to GPIOC_IDT
@0x40011008, and **separates the internal command byte from the wire transaction** (the
0x07/0x0A goes into a FreeRTOS *queue*, not onto SPI3). No verified contract covers this
address, so nothing here promotes to V2 — this is a CONFIRM/CORRECT result, not an
execution-verification.

---

## 0. Address crosswalk (retire the manual `+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `probe_detect_handler_1 @0x0800ba06` | inside `FUN_0800f908 @0x0800f908` |

`0x0800ba06 + 0x4000 = 0x0800fa06`, which lands **inside** ripcord's `FUN_0800f908`
(`0x0800f908`, size 950 B). **osc's Ghidra carved `0x0800ba06` out as a standalone
function `probe_detect_handler_1`; it is not one** — it is the GPIOC-read instruction
(`ldr r0,[r0]` at `0x0800fa06`) of **case 6** inside `FUN_0800f908`, a 10-way
`switch(DAT_20001060)` UI-event dispatcher that pushes USART2-TX command sequences into
a FreeRTOS queue. `was_miscut=true`. General rule:
`ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — the `*param_1` pointer and the function shape

osc decoded the handler from an island entry, so the GPIOC base arrived as an opaque
`*param_1`. ripcord's enclosing decode resolves it from the instruction stream:

```
0x0800f9fe:  movw   r0, #0x1008
0x0800fa02:  movt   r0, #0x4001        ; r0 = 0x40011008  → GPIOC_IDT
0x0800fa06:  ldr    r0, [r0]           ; <-- osc cut point: read GPIOC input data reg
0x0800fa08:  movs   r1, #7             ; default cmd = 0x07 (probe present)
0x0800fa0a:  lsls   r0, r0, #0x18      ; PC7 (bit 7) -> N flag
0x0800fa0c:  it     pl
0x0800fa0e:  movpl  r1, #0xa           ; if PC7 == 0 (sign clear): cmd = 0x0A (no-probe)
0x0800fa10:  ldr    r0, [r5]           ; r5 = _DAT_20002d6c (queue handle)
0x0800fa12:  strb.w r1, [sp, #7]       ; store cmd byte to the 1-byte queue item
0x0800fa1c:  bl     #0x803ecf0         ; xQueueSend(handle, &byte, portMAX_DELAY)
```

| osc symbol | actual value | meaning |
|---|---|---|
| `*param_1` | `[0x40011008]` | **GPIOC_IDT** (input data register; `peripheral_xrefs`: GPIOC IDT READ ×5 across the switch) |
| `-1 < *param_1<<0x18` test | `lsls #0x18` + `it pl` | PC7 = bit 7; the `<<0x18` shifts bit 7 to the sign bit, branch-on-positive = bit7-clear |
| `r5` / `_DAT_20002d6c` | queue handle ptr | FreeRTOS queue used by `FUN_0803ecf0` |
| `FUN_0803ecf0` | queue send | `xQueueSend(handle, item, 0xffffffff=portMAX_DELAY)` — the USART2-TX command queue feeder |

**Correction to the osc note's polarity wording:** osc wrote "bit7==0 -> cmd=0x0A
(no-probe), bit7 set -> cmd=0x07 (probe)." That polarity is **correct** and is now
confirmed at the instruction level (`it pl / movpl #0xa`, default `#7`). No change to the
semantics; the correction is to the *function identity* — this is `FUN_0800f908` case 6,
not a discrete handler.

**Correction to the wire claim:** the osc note says it then "pushes the channel-bank
prefix (0x07/0x0A)…". ripcord shows the push target is a **FreeRTOS queue**
(`xQueueSend` on `_DAT_20002d6c`), i.e. the byte enters the **USART2 command bus**
(verified contract #11 `usart2_fpga_msgbus`), **not** an SPI3 transaction. The full
case-6 sequence pushed is: prefix `0x07`/`0x0A`, then data-index bytes
`0x1A,0x1B,0x1C,0x1D` (each its own queue item), returning `0x1E`. These are internal
command/param indices on the message bus, not wire-level SPI3 bytes — keep that
separation.

---

## 2. CONFIRM — independent agreement (static only; NO V-promotion)

osc static decode, the ripcord static decode, and the clean-room `firmware/src` all agree
on the probe-detect logic. **No execution evidence exists for this address** — see §4 —
so this stays **V1 (cross-method static agreement), not V2.**

| claim | osc decode | ripcord decode | firmware/src | tier |
|---|---|---|---|---|
| probe sense reads GPIOC_IDT, bit PC7 | `*param_1` IDR snapshot, bit-7 | `ldr [0x40011008]`, `lsls #0x18` | `fpga_probe_cmd_byte()`: `GPIOC->idt & (1U<<7)` (fpga.c:810) | static-inferred |
| PC7 set → 0x07, PC7 clear → 0x0A | bit7 set→0x07, ==0→0x0A | `movs r1,#7; it pl; movpl r1,#0xa` | `? 0x07 : FPGA_CMD_METER_NOPROBE` (=0x0A) | static-inferred |
| byte goes onto USART2 cmd path | "pushes prefix" | `xQueueSend(_DAT_20002d6c,…)` | `xQueueSend(usart_tx_queue,…)` (fpga.c:397) | static-inferred |
| case-6 sequence = prefix + 0x1A..0x1E | (range/coupling block) | queue pushes 0x07/0x0A, 0x1A,0x1B,0x1C,0x1D→0x1E | fpga.c:995-998 comment "0x07/0x0A + 0x1A..0x1E" | static-inferred |

All three methods agreeing is strong, but two of them are static decompiles of the same
image and the third is a clean-room reimplementation derived from them — this is **not**
independent execution. The 0x07/0x0A bytes are *internal command selectors*; their effect
on the FPGA (and the FPGA's reply) is hardware-bound and unverified.

---

## 3. GAP — R-side corrections for `firmware/src/drivers/fpga.c`

ripcord's resolved decode of case 6 exposes divergences from the clean-room range/coupling
sender. Concrete reimplementation items:

1. **The stock range/coupling block uses cmd-hi indices `0x1A..0x1E`, not
   `0x10..0x14`.** `fpga_send_scope_range_block()` (fpga.c:913-925) emits the channel
   prefix then `FPGA_CMD_CH1_GAIN/CH1_OFFSET/CH2_GAIN/CH2_OFFSET/COUPLING`. ripcord case 6
   pushes prefix then literally `0x1A,0x1B,0x1C,0x1D` and returns `0x1E` — five
   consecutive indices. The clean-room comment at fpga.c:996 already *names* `0x1A..0x1E`
   as the stock sequence, but the actual `fpga_timed_send_cmd` calls in
   `fpga_send_scope_range_block` do not use those literals. Align the emitted command-hi
   bytes to `0x1A,0x1B,0x1C,0x1D,0x1E`.

2. **This is one arm of a 10-way `switch(DAT_20001060)` (a UI-mode / frontend-event
   selector at SRAM `0x20001060`), not a freestanding probe routine.** The clean-room
   splits the equivalent behavior across `fpga_send_scope_range_block` /
   `fpga_probe_cmd_byte` / meter-wake helpers. Modeling `DAT_20001060` as the dispatch
   key (which UI/range event fires which case) is missing — the other nine cases push
   different byte sequences (cases 0-5/8/9 each push their own and tail-call the queue
   send; case 7 pushes 0x15). Reconstruct the `DAT_20001060` → case map to know *when*
   the 0x1A..0x1E block is emitted vs. the others.

3. **The probe byte and the range block are coupled in stock, decoupled in R.** In case 6
   the GPIOC_IDT probe read produces the prefix (0x07/0x0A) that *heads* the same
   range/coupling push. In `firmware/src`, `fpga_probe_cmd_byte()` (meter path) and
   `fpga_scope_prefix_cmd()` (scope path, `ss->trigger.source==CH2 ? 0x0A : 0x07`,
   fpga.c:877) are two different prefix sources. Stock case 6 derives the prefix from the
   PC7 hardware line, not from `ss->trigger.source`. If case 6 is the scope-range path,
   the R-side prefix should come from GPIOC_IDT bit 7, not trigger source — verify which
   selector stock actually uses here.

---

## 4. Execution evidence — none covers this address

`build/contracts.sqlite`: no `verified=1` contract's `[addr_start, addr_end)` covers
`134281734` (`0x0800fa06`).
- #18 `fpga_bitstream_upload` covers `[134392260, 134668495)` = `0x0802a9c4..` — starts
  **above** our address.
- #19 `acq_engine_runtime` covers `[134460500, 134467592)` = `0x0803b454..` — far above.
- #11 `usart2_fpga_msgbus` (the relevant *mechanism*, queue→USART2) is `verified=` (blank,
  conf 0.85) — census-confirmed, not execution-verified, and address-scoped to USART2
  `0x40004400`, not to this function.

`contract_id = null`, `contract_verified = false`. The queue→USART2 routing is consistent
with contract #11's model but is not asserted by any verified contract at this PC.

---

## 5. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0800ba06` | note | "FPGA_BLOCKED … pushes the channel-bank prefix" | "inner region of FUN_0800f908 case 6 (ripcord 0x0800f908); *param_1 resolved = GPIOC_IDT 0x40011008, PC7 bit7→ cmd 0x07(set)/0x0A(clear); byte goes to FreeRTOS USART2-TX queue _DAT_20002d6c via xQueueSend, NOT SPI3; case-6 seq = prefix,0x1A,0x1B,0x1C,0x1D→0x1E. See probe_detect_handler_1_ripcord_reconcile" | §1 |
| `0800ba06` | D | D3 | **D4** (register + queue-handle + dispatch-key resolved; mis-cut corrected) | §1 |
| `0800ba06` | R | R1 | R1 (unchanged — R-side has the prefix/probe logic; gaps in §3 are index-literal + dispatch-map, not absence) | §3 |
| `0800ba06` | V | V0 | **V1** (cross-method static agreement osc⇄ripcord⇄src; NOT execution-verified — no covering contract) | §2, §4 |
| `0800ba06` | klass note | FPGA_BLOCKED | reclassify: this is a **USART2 message-bus** command-emit, not an FPGA wire transaction; the only hardware-bound residual is what the FPGA does with cmd 0x07/0x0A | §1, §4 |

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check of this reconcile against the ripcord warehouse
(`stock_v120`) and `build/contracts.sqlite`. Default posture: overturn anything not
solidly backed.

**CROSSWALK — PASS.** `scripts/query` for the fn containing runtime addr `134281734`
(`0x0800fa06`) returns exactly one row: `FUN_0800f908 @0x800f908`, size 950
(`[0x0800f908, 0x0800fcbe)`). `0x0800ba06 + 0x4000 = 0x0800fa06` lands inside it.
`crosswalk_ok = true`. The mis-cut claim (osc carved an island entry) is consistent: the
address is mid-function, not a function head.

**INSTRUCTION STREAM — PASS (load-bearing).** `disasm.py --target stock_v120 --start
0x0800f9f8 --end 0x0800fa30` reproduces §1 byte-for-byte:
`movw r0,#0x1008 / movt r0,#0x4001` (→0x40011008), `ldr r0,[r0]` at `0x0800fa06`,
`movs r1,#7`, `lsls r0,#0x18`, `it pl`, `movpl r1,#0xa`, `ldr r0,[r5]`, `strb.w r1,[sp,#7]`,
`bl #0x803ecf0`, then `movs r1,#0x1a` (first range index). The polarity (PC7 set→0x07,
clear→0x0A), the GPIOC_IDT resolve, the queue-send target, and the 0x1A.. sequence start are
all confirmed at the instruction level.

**PERIPHERAL XREF — PASS.** `peripheral_xrefs` for `FUN_0800f908` shows IDT READ @0x40011008
×5 (GPIOC base 0x40011000 + IDT 0x08), matching "GPIOC IDT READ ×5 across the switch".

**CONTRACT REALITY — PASS (no hallucination).** Draft cites `contract_id=null`. The note
body references #11/#18/#19 only to show none cover this PC:
- #11 `usart2_fpga_msgbus`: exists, `verified` blank (NOT =1), addr range NULL — correctly
  described as mechanism-level/census, not address-scoped or execution-verified here.
- #18: `verified=1`, `[134392260, 134668495)` — starts **above** 134281734. Confirmed.
- #19: `verified=1`, `[134460500, 134467592)` — far above. Confirmed.
No `verified=1` contract's `[addr_start, addr_end)` covers `134281734`. The note's claim of
no covering execution evidence holds. `hallucination_found = false`.

**V-PROMOTION — nothing to demote.** The draft scores every CONFIRM at V1 (cross-method
static agreement) and promotes nothing to V2. The three agreeing methods are two static
decompiles of the same image plus a clean-room reimpl derived from them — correctly NOT
independent execution. V1 is the defensible ceiling; no V2 claim exists to scrutinize.

**Overturned:** none. Every draft CONFIRM/CORRECT is backed by the disassembly,
the peripheral-xref table, or the contract ledger. The note was already conservative
(self-capped at V1, explicit "no execution-verification").

**FINAL approved D/R/V: D4 / R1 / V1.** Unchanged from the proposal — and defensible:
- D4: register (`*param_1`→GPIOC_IDT), queue handle (`_DAT_20002d6c`), and dispatch key
  (`DAT_20001060`) resolved; mis-cut corrected. Backed by disasm + xrefs.
- R1: clean-room has the probe/prefix logic; §3 gaps are index-literal + dispatch-map, not
  absence.
- V1: cross-method static agreement only; no `verified=1` contract covers this PC.

---

**Honest scope:** ripcord adds real value here but does **not** verify anything by
execution. What it adds over the existing osc row: (1) fixes the function boundary — this
is `FUN_0800f908` case 6, not a standalone handler, which explains the island/`*param_1`
decode; (2) resolves `*param_1` to GPIOC_IDT `0x40011008`; (3) corrects the wire claim —
the 0x07/0x0A is an *internal* command byte pushed to a FreeRTOS USART2-TX queue, not an
SPI3 transaction; (4) hands `fpga.c` three faithful-reimplementation items (use 0x1A..0x1E
literals, model the `DAT_20001060` dispatch key, source the prefix from PC7 not trigger
source). It does **not** crack what the FPGA does with cmd 0x07 vs 0x0A — that reply is
silicon-bound and no verified contract covers this PC. `new_evidence=true` (boundary fix +
register resolve + wire/queue separation), but strictly static; V stays at V1.
