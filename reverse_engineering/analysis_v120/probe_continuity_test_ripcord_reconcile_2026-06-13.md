# Reconcile: `probe_continuity_test` 0x0800bba6 ↔ ripcord `FUN_0800f908` — misnamed + mis-cut, "no register read" corrected, NOT execution-verified

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`), reconciled against `firmware/src/drivers/fpga.c`
and `fpga.h`.
**Why this matters:** `0x0800bba6` was booked in `coverage_ledger.csv` as **D3/R1/V0,
FPGA_BLOCKED** with the note *"Smallest of the set (24B): NO register read. Pushes exactly two
constant command bytes 0x20 then 0x21 onto the usart_cmd queue … Per our command map
(fpga.h:89-90) 0x20/0x21 are F[req counter param]."* The ripcord cross-walk **fixes two errors**:
(1) the "NO register read" verdict is wrong — there is a **GPIOC_IDT read at the exact cut
address**; (2) the function name `probe_continuity_test` is wrong — 0x20/0x21 are the
**frequency-counter** params (system_mode 4), not continuity (which is 0x2C / system_mode 8).
**No new execution evidence:** no verified contract covers this address, so nothing here promotes
past V1. This is an honest "ripcord corrects the static decode but does not crack anything new."

---

## 0. Address crosswalk (retire the manual `+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `probe_continuity_test @0x0800bba6` (24 B) | inside `FUN_0800f908 @0x0800f908` |

`0x0800bba6 + 0x4000 = 0x0800fba6`, which lands **inside** ripcord's `FUN_0800f908`
(`0x0800f908`, size 950 B). **osc's Ghidra carved `0x0800bba6` out as a standalone 24-byte
function; it is not one** — it is the **case-5 arm** of a single 10-way `TBH` meter/FPGA
command dispatcher. General rule: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

What `FUN_0800f908` actually is (prologue + decompile):

```
0x0800F908:  push   {r4, r5, r7, lr}
0x0800F90C:  movw   r0, #0xf8 ; movt r0,#0x2000   ; r0 = 0x200000F8  (state base)
0x0800F914:  ldrb.w r0, [r0, #0xf68]              ; selector = state+0xF68 (= DAT_20001060)
0x0800F918:  cmp    r0, #9 ; bhi default
0x0800F91E:  movw   r5, #0x2d6c ; movt r5,#0x2000  ; r5 = &_DAT_20002D6C (cmd queue handle)
0x0800F926:  tbh    [pc, r0, lsl #1]              ; 10-way table branch (cases 0..9)
```

Each case stores one constant command byte to `[sp,#7]` and calls
`FUN_0803ecf0(_DAT_20002d6c, &local_11, 0xffffffff)` = **`xQueueGenericSend(queue, &byte,
portMAX_DELAY)`** — it enqueues meter/FPGA command bytes for a downstream USART2/SPI3 service
task. Selector `state+0xF68` is the meter **system_mode** (the same submode→system_mode map
documented in `fpga.h:402-406`).

---

## 1. CORRECT — ripcord fixes two osc errors

### 1a. "NO register read" is wrong — there is a GPIOC_IDT read at the cut address
The byte at the cut point decodes as a register read, not a bare queue push:

```
0x0800FB9E:  movw  r0, #0x1008 ; movt r0, #0x4001   ; r0 = 0x40011008  → GPIOC_IDT
0x0800FBA6:  ldr   r0, [r0]                          ; READ GPIOC input data register
0x0800FBA8:  movs  r1, #0x20                          ; command byte 0x20
0x0800FBAA:  ldr   r0, [r5]                           ; queue handle
0x0800FBAC:  strb.w r1, [sp, #7]
0x0800FBB6:  bl    #0x803ecf0                          ; xQueueGenericSend(.., 0x20, MAX_DELAY)
0x0800FBBA:  movs  r0, #0x21                           ; (then 0x21, enqueued by the shared tail)
```

ripcord's `peripheral_xrefs` for `FUN_0800f908` independently confirms **GPIOC / IDT / READ ×5**
across this dispatcher — the read at `0x0800fba6` is one of them. So the ledger's "NO register
read" verdict for this row is a decode artifact of the 24-byte mis-cut (the `movw/movt` that
builds `0x40011008` sits one instruction *before* the cut boundary; osc's island started at the
`ldr`'s operand and lost the address-build, so the read looked like a bare load of an
unresolved pointer). The read result (GPIOC IDT) is discarded into `r0` and then immediately
overwritten by the queue handle (`ldr r0,[r5]`) — consistent with stock's pattern of a
**probe/MUX-pin poll whose result is consumed elsewhere**, not by this arm.

### 1b. The name `probe_continuity_test` is wrong — this is the frequency-counter arm
`0x20`/`0x21` are `FPGA_CMD_FREQ_20` / `FPGA_CMD_FREQ_21` (`fpga.h:89-90`), the **frequency
counter** params for **system_mode 4** (`fpga.h:406`, `fpga.c:2451` case 5). Continuity is a
different command entirely — `FPGA_CMD_CONT_DIODE = 0x2C`, system_mode 8 (`fpga.h:92-93`,
`fpga.c:2476`). osc's own ledger note already pointed at "0x20/0x21 are F[req]"; the function
*name* simply contradicts it. **Rename `probe_continuity_test` → `meter_mode_cmd_dispatch`
(or, for this arm specifically, the freq-counter enqueue path).** osc did not mis-cut a
continuity routine; it cut one case out of the meter command dispatcher.

---

## 2. CONFIRM — static decode and ripcord agree (NONE execution-verified)

| claim | osc decode | ripcord static evidence | tag |
|---|---|---|---|
| enqueues two constant bytes 0x20 then 0x21 | ledger note | disasm `movs r1,#0x20` … `movs r0,#0x21`; decompile two `FUN_0803ecf0` pushes | **static-inferred** (V1) |
| via `xQueueGenericSend(queue,&byte,portMAX_DELAY)` | "usart_cmd queue" | `FUN_0803ecf0(_DAT_20002d6c, &local_11, 0xffffffff)` | **static-inferred** (V1) |
| 0x20/0x21 = freq-counter params, system_mode 4 | "0x20/0x21 are Freq…" | `fpga.h:89-90,406` + `fpga.c:2451` case 5 | **static-inferred** (V1) |

**Execution evidence: NONE.** No `verified=1` contract in `build/contracts.sqlite` has
`[addr_start,addr_end)` covering `134282150` (0x800fba6). The nearest acquisition contract,
#19 `acq_engine_runtime` (`verified=1`), covers `134460500..134467592` (`FUN_0803B454`) — a
**different** dispatcher (keyed on the USART2 command byte via the TBH at `0x0803B536`), not this
one (keyed on `state+0xF68`). Do not borrow #19's verification for this address.
**`contract_id = null`. No V0→V2 promotion is available.** Best attainable here is **V1**
(two independent static methods — disassembly and the R-side header — agree on the command
bytes and their meaning), and only for the *internal dispatch/command bytes*, never for any
wire-level FPGA reply.

---

## 3. GAP — R-side (`firmware/src/drivers/fpga.c`) divergences

`fpga.c` already implements the freq-counter command sequence, but the **dispatch shape and the
GPIOC read differ** from stock:

1. **The GPIOC_IDT read in this arm is not reflected.** Stock reads `GPIOC_IDT` (0x40011008)
   inside the freq-counter dispatch arm at `0x0800fba6`. `fpga.c`'s freq case (`case 5`,
   line 2451) emits `0x00,0x1F,0x09,0x20,0x21` with **no `GPIOC->idt` read in that path**.
   `fpga.c` does read `GPIOC->idt & (1U<<7)` for **probe-presence detection**
   (`fpga_probe_cmd_byte` line 810, and lines 2141/2391), but bit 7, and not gated to the freq
   arm. Determine whether the stock freq-arm `GPIOC_IDT` read is (a) the same probe/MUX poll
   (different bit) folded into the meter dispatcher, or (b) a distinct sense — then route it
   into the freq path. Concrete item: confirm which GPIOC bit `0x0800fba6` consumes downstream
   (the read value is discarded locally, so the consumer is elsewhere in the meter task).

2. **Stock dispatches all 10 meter submodes through one `state+0xF68`-keyed `TBH` that enqueues
   single command bytes to a queue; `fpga.c` dispatches via a C `switch(submode)` that calls
   `fpga_send_cmd(high,low)` directly (synchronous), not via `xQueueGenericSend`.** This is a
   faithful-behavior question, not a bug: stock decouples command *generation* (this dispatcher)
   from command *transmission* (the queue-consuming USART2/SPI3 task), whereas `fpga.c` sends
   inline. If timing/ordering vs stock matters, model the queue hop.

3. **Selector source.** Stock keys on `state+0xF68` (`DAT_20001060`); `fpga.c` keys on the
   meter `submode` argument. Verify `state+0xF68` is the same submode value (the `fpga.h:402-406`
   map suggests it is the post-translation system_mode, i.e. 1/4/8/9/3, while `fpga.c` switches
   on the raw 0..9 submode and translates inside each case). If `FUN_0800f908`'s selector is the
   *system_mode* (1/4/8/9) rather than the raw submode, the case indices here are system_mode
   indices — re-check the case→command mapping against that.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0800bba6` | name | `probe_continuity_test` | `meter_mode_dispatch_freq_arm` (inner region of `FUN_0800f908`) | §1b |
| `0800bba6` | note | "NO register read … 0x20/0x21 are F[req]" | "inner region of `FUN_0800f908` (ripcord 0x0800f908) — state+0xF68-keyed 10-way TBH meter cmd dispatcher; THIS arm = freq-counter (sysmode 4): reads GPIOC_IDT @0x40011008 then enqueues 0x20,0x21 via xQueueGenericSend. 'NO register read' was a mis-cut artifact; NOT continuity (that is 0x2C). See probe_continuity_test_ripcord_reconcile" | §1, §3 |
| `0800bba6` | R | R1 | **R1** (unchanged — freq seq already in fpga.c:2451; gap is the GPIOC read + queue-hop model) | §3 |
| `0800bba6` | V | V0 | **V1** (two static methods agree on cmd bytes/meaning; NO execution contract covers this addr) | §2 |
| `0800bba6` | klass | FPGA_BLOCKED | not-FPGA-blocked: this is MCU-side command *generation*; nothing here needs the FPGA. The freq-counter *result* path is the FPGA-bound piece, and is elsewhere. | §1,§3 |

**Honest scope:** ripcord adds **no execution evidence** for this address — no verified contract
covers it, and the wire-level FPGA reply values for the freq-counter remain hardware-bound. What
the cross-walk *does* deliver is two static corrections (the "no register read" verdict is false;
the `continuity` name is false — it is the frequency-counter arm) and three concrete `fpga.c`
reimplementation items (route the GPIOC_IDT read into the freq arm, model the command-queue hop,
verify the `state+0xF68` selector semantics). V moves V0→V1 on strengthened static agreement
only; it does **not** reach V2.

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check against the ripcord warehouse and contract ledger.
Default posture was to overturn anything not solidly backed. **Result: nothing overturned;
all checked claims hold. Final D3 / R1 / V1 confirmed.**

**Checks performed:**

1. **Crosswalk (PASS).** `scripts/query` over `functions` WHERE
   `addr <= 134282150 AND addr+size > 134282150` returns exactly one row:
   `FUN_0800f908 @0x800f908`, size 950. `0x0800fba6` genuinely lands inside the claimed
   containing function. `crosswalk_ok = true`.

2. **Contract reality (PASS — null is correct).** Draft cites `contract_id = null`,
   `contract_verified = false`. Ledger query for any contract with
   `addr_start <= 134282150 AND addr_end > 134282150` returns **0 rows** — no contract
   covers this address, confirming the "NONE execution-verified" claim. The two
   `verified=1` contracts whose ranges are non-trivial — #18 (`0x802a9c4..0x806e0cf`,
   FPGA config bitstream upload) and #19 (`0x803b454..0x803d008`, `acq_engine_runtime`) —
   both start **above** `0x800fba6` and are unrelated dispatchers. The note's refusal to
   borrow #19's verification is correct. No hallucinated contract id. `hallucination_found = false`.

3. **V-promotion audit (PASS — no promotions to overturn).** Every CONFIRM row in §2 is
   tagged **V1 / static-inferred**; no claim is scored V2. There is therefore nothing to
   demote. Best attainable V for this address is **V1** and the note already sits there.

4. **Spot-check of the load-bearing CORRECT (PASS).** Disassembly at the cut address
   (`disasm.py --target stock_v120 --start 0x0800fb98 --end 0x0800fbc4`) shows:
   `0x0800FB9E movw r0,#0x1008 ; 0x0800FBA2 movt r0,#0x4001 ; 0x0800FBA6 ldr r0,[r0]` —
   i.e. a genuine **GPIOC_IDT (0x40011008) READ** at exactly `0x0800fba6`, with the
   address-build one instruction before the osc cut boundary. `peripheral_xrefs`
   independently attributes **five** GPIOC/IDT/READ accesses to `FUN_0800f908`
   (sites `0x800fa06, 0x800fb10, 0x800fba6, 0x800fc00, 0x800fc98`); `0x800fba6` is one of
   them. osc's "NO register read" verdict is decisively a mis-cut artifact, as claimed.
   The read result is overwritten by `ldr r0,[r5]` immediately after, matching the
   note's "discarded locally" caveat.

5. **osc-side citations (PASS — none hallucinated).** `fpga.h:89-90` define
   `FPGA_CMD_FREQ_20=0x20` / `FPGA_CMD_FREQ_21=0x21` under "Frequency counter (system_mode 4)";
   `fpga.h:92-93` define `FPGA_CMD_CONT_DIODE=0x2C` (system_mode 8); `fpga.h:406` and
   `fpga.c:2451 case 5` emit `0x00,0x1F,0x09,0x20,0x21` with **no** GPIOC_IDT read in that
   C path — exactly as the GAP section states. The `probe_continuity_test` name is wrong
   (continuity is 0x2C, not 0x20/0x21); rename stands.

**Minor note (no change required):** the symbol gloss "`r5 = _DAT_20002d6c` = queue handle"
is slightly loose — disasm shows `r5 = &_DAT_20002D6C` and the handle is the *dereference*
`[r5]`. The note body (§0, line `r5 = &_DAT_20002D6C` and the `ldr r0,[r5]` step) already
states this precisely; only an external summary glossed it as the handle itself. Does not
affect any D/R/V scoring.

**Final approved disposition: D3 / R1 / V1** (unchanged). The note is honest, does not
overstate, cites no nonexistent contract, and promotes nothing past static agreement.
