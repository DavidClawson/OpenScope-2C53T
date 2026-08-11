# Reconcile: `probe_component_test` 0x0800bc00 ↔ ripcord `FUN_0800f908` — mis-cut correction, no execution evidence

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`).
**Why this matters:** `0x0800bc00` was booked in `coverage_ledger.csv` as
**D3/R1/V0, FPGA_BLOCKED** with the note *"42B, NO register read. Pushes three constant
command bytes 0x26, 0x27, 0x28 onto the usart_cmd queue. These are the SCOPE TIMEBASE
BLOCK (prescaler/period/mode) — our firmware emits the identical triple."* The crosswalk
**corrects two factual errors in that note** (the "42B standalone fn" mis-cut and the "NO
register read" claim) and **clarifies the layer** (queue-opcode enqueue, not FPGA wire
transaction). It does **not** add any execution evidence: no verified contract covers this
address. `new_evidence=true` only for the static corrections; the 0x26/0x27/0x28 ==
timebase semantics stay **V1 (static-inferred, two-method agreement)**, not V2.

---

## 0. Address crosswalk (retire the manual `+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `probe_component_test @0x0800bc00` | **inner region of** `FUN_0800f908 @0x0800f908` |

`0x0800bc00 + 0x4000 = 0x0800fc00 = 134282240`, which lands **760 bytes (0x2f8) inside**
ripcord's `FUN_0800f908` (`addr 0x0800f908`, size **950 B**, entry != target).
**osc's Ghidra carved `0x0800bc00` out as a standalone 42-byte `probe_component_test`; it
is not one** — it is one arm of a single switch-dispatch function. `was_miscut=true`.
General rule: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — what `FUN_0800f908` actually is, and the two osc errors

`FUN_0800f908` is a **command-opcode queue producer** dispatched on a mode/state byte:

```
0x0800F90C:  movw r0,#0xf8 ; movt r0,#0x2000     ; r0 = 0x200000F8 (global state base)
0x0800F914:  ldrb.w r0,[r0,#0xf68]               ; selector = state+0xF68  (= DAT_20001060)
0x0800F918:  cmp r0,#9 ; bhi.w default
0x0800F91E:  movw r5,#0x2d6c ; movt r5,#0x2000    ; r5 = &_DAT_20002d6c  (FreeRTOS queue handle)
0x0800F926:  tbh [pc,r0,lsl #1]                   ; 10-way table branch on selector 0..9
...
0x0800FCBC:  strb.w r0,[sp,#7]                     ; shared tail: store the chosen opcode byte
0x0800FCC0:  ldr  r0,[r5]                          ; r0 = queue handle (*0x20002d6c)
0x0800FCC6:  mov.w r2,#-1                          ; xTicksToWait = portMAX_DELAY
0x0800FCCA:  bl   #0x803ecf0                       ; FUN_0803ecf0(queue, &byte, portMAX_DELAY) = xQueueSend
```

Each `bl 0x803ecf0` enqueues **one bare command byte** (`local_11`) with no data payload.
The osc-cut arm (entry around `0x0800fbf8`, the region osc labelled `probe_component_test`):

```
0x0800FBF8:  movw r0,#0x1008 ; movt r0,#0x4001    ; r0 = 0x40011008 = GPIOC->IDT
0x0800FC00:  ldr  r0,[r0]                          ; READ GPIOC input data register  (osc addr 0x0800bc00)
0x0800FC02:  movs r1,#0x26 ; ... bl 0x803ecf0      ; enqueue opcode 0x26
0x0800FC14:  movs r1,#0x27 ; ... bl 0x803ecf0      ; enqueue opcode 0x27
0x0800FC26:  movs r0,#0x28 ; b 0x800fcbc           ; enqueue opcode 0x28 via shared tail
...
0x0800FC90:  movw r0,#0x1008 ; movt r0,#0x4001 ; ldr r0,[r0]   ; READ GPIOC->IDT again
0x0800FC9A:  lsls r0,r0,#0x18 ; it pl ; movpl r0,#0xa          ; sign-bit (pin) test -> opcode 7 or 0x0A
```

**Error 1 — "42B standalone function."** Mis-cut. It is an inner arm of a 950 B dispatcher.
**Error 2 — "NO register read."** **False.** This arm reads **GPIOC->IDT (`0x40011008`)**
at `0x0800fc00` (value discarded here) and again at `0x0800fc90` where the top bit
(`lsls #0x18; it pl; movpl`) is a **GPIOC input-pin test** that selects opcode `7` vs `0x0A`.
ripcord's `peripheral_xrefs` for this function confirms **GPIOC / IDT / READ × 5**. The
register read is the dispatch input, exactly the kind of thing the "NO register read" note
would mislead a reimplementer into dropping.

**Layer clarification.** The bytes 0x26/0x27/0x28 here are **FreeRTOS queue opcodes**
pushed to `_DAT_20002d6c` (a distinct handle from the acquisition queue `0x20002d78`
in contract #19 and the USART2 doorbell semaphore `0x20002d7c` in contract #11). They are
**command selectors**, not the wire-level `(data,opcode)` pairs that `fpga_timed_send_cmd`
emits down the FPGA transport. Keep the internal-selector vs hardware-transaction
distinction: nothing here touches SPI3/USART2 silicon directly.

---

## 2. CONFIRM — two-method agreement, but **static-only (V1, NOT V2)**

| claim | osc decode | R-side (`firmware/src/drivers/fpga.c`) | tier |
|---|---|---|---|
| 0x26/0x27/0x28 == timebase prescaler / period / mode | osc note labels the triple "SCOPE TIMEBASE BLOCK" | `fpga.c:458-460` `fpga_timed_send_cmd(tb_prescaler,0x26,..)`, `(tb_period,0x27,..)`, `(tb_mode,0x28,..)` | **V1 static-inferred** |
| these opcodes flow through a FreeRTOS command queue | stock enqueues via `xQueueSend(_DAT_20002d6c,&op,portMAX_DELAY)` | R-side `send_cmd(q,cmd)=xQueueSend(q,&cmd,0)` pattern (`input_handler.c:183`) | **V1 static-inferred** |

Two independent static methods (stock decode + clean-room source) agreeing on the
0x26/0x27/0x28 == timebase mapping is real corroboration and warrants **V1**. It is **not
V2**: there is **no verified contract** covering `0x0800fc00`. The covering-contract query
returns nothing — the nearest verified contracts are #1 memset (`...52bc`, far below),
#12/#13 (LCD/blit), #18 (bitstream upload `0x0802A9C4..`, above) and #19 (acq engine
`0x0803B454..`, above). **`contract_id=null`, `contract_verified=false`.** Do not promote
past V1.

---

## 3. GAP — R-side divergence (`firmware/src/drivers/fpga.c`)

1. **Wrong abstraction layer.** The R-side emits the 0x26/0x27/0x28 triple via
   `fpga_timed_send_cmd(data, opcode, delay_ms)` — a *paired* (data-byte, opcode-byte)
   wire send with an inter-command delay. Stock's `FUN_0800f908` arm instead enqueues
   **bare single opcode bytes** (0x26, 0x27, 0x28) onto queue `_DAT_20002d6c` with **no
   data byte and no per-command delay** — a producer feeding a consumer task that later
   does the wire send. The clean-room code collapses producer + wire-send into one call;
   stock splits them across a queue boundary. Re-model the producer/consumer split if the
   queue-timing behavior matters.

2. **Missing GPIOC input-pin gate.** Stock reads `GPIOC->IDT` (`0x40011008`) and tests its
   top bit to choose between opcode `7` and `0x0A` (`0x0800fc90`). No equivalent GPIOC
   pin-conditioned opcode selection appears in `fpga_send_scope_runtime_blocks`. Identify
   which physical input PC pin this is (likely a probe-presence / range / coupling sense
   line) and gate the corresponding command on it.

3. **Selector is `state+0xF68`, not a parameter.** Stock dispatches the whole 10-arm block
   on `state+0xF68` (`DAT_20001060`). The clean-room path is reached by direct call with a
   `scope_state_t*`; map `state+0xF68` to the corresponding `scope_state_t` field so the
   arm selection is faithful (the arm reached here is the one that emits the timebase
   triple plus the GPIOC-gated 7/0x0A opcode).

*(`firmware/src/tasks/component_test.c` is unrelated despite the osc name `probe_component_test`
— it has no queue-opcode emission; the name is an osc mis-label, not the R-side twin.)*

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0800bc00` | note | "42B, NO register read. Pushes three constant command bytes 0x26, 0x27, 0x28 onto the usart_cmd queue…" | "inner arm of FUN_0800f908 (ripcord 0x0800f908, 950B, 10-way xQueueSend dispatch on state+0xF68); DOES read GPIOC->IDT (0x40011008) twice — pin-test selects opcode 7 vs 0x0A. 0x26/0x27/0x28 are bare queue opcodes (handle _DAT_20002d6c), not (data,opcode) wire pairs. 0x26/0x27/0x28==timebase corroborated by R-side fpga.c:458-460 (V1). See probe_component_test_ripcord_reconcile." | §1, §2 |
| `0800bc00` | R | R1 | **R2** (R-side timebase mapping 0x26/0x27/0x28 corroborates; producer/consumer split + GPIOC gate still to reimplement) | §2, §3 |
| `0800bc00` | V | V0 | **V1** (two-method static agreement; NOT execution-verified — no contract covers 0x0800fc00) | §2 |
| `0800bc00` | klass note | FPGA_BLOCKED | not FPGA-blocked — this is an MCU-side queue producer; no FPGA silicon dependency to verify here | §1 |

**Honest scope.** ripcord adds **no execution evidence** for this address — no verified
contract covers it. What it adds is purely static: it (a) corrects the mis-cut (this is an
inner arm of a 950 B dispatcher, not a 42 B standalone fn), (b) refutes "NO register read"
(GPIOC->IDT is read twice and gates opcode selection), and (c) fixes the layer (queue
opcodes, not wire `(data,opcode)` pairs). The 0x26/0x27/0x28 == timebase semantics the osc
note already asserted are corroborated by the clean-room source, so they earn **V1**, not
V2. This is a static-correction result, not a new-evidence breakthrough.

---

## Verification (adversarial, 2026-06-13)

Independent adversarial re-check (default-to-skepticism, per the osc verifier
discipline). Every load-bearing claim was re-derived from the ripcord warehouse;
nothing was overturned, but two minor imprecisions are noted.

**Checks run and results:**

1. **CROSSWALK — PASS.** `functions WHERE addr<=134282240 AND addr+size>134282240`
   returns exactly `FUN_0800f908 @0x800f908, size=950`. `0x0800fc00` (134282240)
   lands 0x2f8 inside it. Mis-cut and containing-fn claims confirmed.

2. **CONTRACT REALITY — N/A (correctly).** Draft cites `contract_id=null,
   contract_verified=false`. `contracts WHERE addr_start<=134282240 AND
   addr_end>134282240` returns **zero rows** — no verified contract covers this
   address. The nearest verified contracts (#18 @0x0802A9C4.., #19 @0x0803B454..)
   are above it and unrelated. No hallucinated contract id. ✓

3. **V-PROMOTION — no V2 to demote.** The note caps itself at **V1** and frames
   the result as "no execution evidence." Both CONFIRM claims are explicitly
   tagged STATIC-ONLY. No over-promotion to overturn.

4. **INSTRUCTION SPOT-CHECK (disasm.py) — PASS.**
   - `0x0800FC00: ldr r0,[r0]` after `movw r0,#0x1008; movt r0,#0x4001`
     (= GPIOC->IDT 0x40011008) — first register read confirmed.
   - `0x0800FC02..FC28`: opcodes 0x26/0x27 enqueued via `bl #0x803ecf0` with
     `r2=#-1` (portMAX_DELAY); 0x28 via shared tail `b #0x800fcbc` — confirmed.
   - Second GPIOC->IDT read: `ldr` is at **0x0800FC98** (the `movw #0x1008` is at
     0x0800FC90), then `lsls r0,#0x18; mov r0,#7; it pl; movpl r0,#0xa` — pin-test
     gate selecting opcode 7 (bit set) vs 0x0A (bit clear) confirmed.
   - `peripheral_xrefs` for `function_addr=0x800f908`: **GPIOC / IDT / READ × 5**
     confirmed (the access-addr count, not 5 distinct sites — two are this arm).
   - `FUN_0803ecf0` is real code with a `push {r7,lr}` prologue taking r0/r1/r2
     (queue, &byte, ticks); it is **not** carved as a discrete row in the
     `functions` table for stock_v120, but the address disassembles to a valid
     queue-send body. The xQueueSend label is a sound static inference from the
     r2=-1 calling convention, not an over-claim.

**Imprecisions (cosmetic, not overturned):** the body says the second GPIOC read
is "at 0x0800fc90"; the actual `ldr` is at 0x0800fc98 (0x0800fc90 is the address-
load `movw`). The "×5" is an access-row count for the whole function, of which
two rows are this arm's two reads. Neither changes any conclusion.

**FINAL approved D/R/V:** **D3 / R2 / V1.** Defensible. No execution evidence
exists for 0x0800fc00, so V1 is the ceiling; R2 is warranted by the two-method
static agreement (stock decode + clean-room fpga.c:458-460) on the
0x26/0x27/0x28==timebase mapping. Nothing overturned.
