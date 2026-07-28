# Reconcile: `usart_tx_config_writer` 0x08039734 ↔ ripcord `FUN_0803d734` — name + direction correction, no execution gain

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) — disasm
(`scripts/analysis/disasm.py`), `decompiled` table, `peripheral_xrefs`, `calls` —
plus its execution-verified contract ledger (`build/contracts.sqlite`).
**Why this matters:** `0x08039734` was booked in `coverage_ledger.csv` as
**D2/R1/V0, FPGA_BLOCKED** with the note *"the upstream encoder that reads
scope/meter state … and produces a 16-bit [TX word]"*. ripcord's clean decode
**corrects two structural errors** in that description (the data direction and the
peripheral attribution) and **confirms** the bit-field map already in
`SCOPE_CMD_PARAMETERS.md`. It does **not** add execution evidence:
`new_evidence = false` for the V column. Stated plainly so the row is not
over-promoted.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `usart_tx_config_writer @0x08039734` | `FUN_0803d734 @0x0803D734` (size 312 B) |

`0x08039734 + 0x4000 = 0x0803D734`, which is the **exact entry** of ripcord's
`FUN_0803D734`. **This is not a mis-cut** — osc carved the function at the same
boundary ripcord sees. `was_miscut = false`. Caller (ripcord `calls`):
`FUN_0802a9c4 @0x0802A9C4` — the large command-dispatched SPI3 service region
(the same region contract #18 describes; see §3).

---

## 1. CORRECT — the function is a config-bitfield *applier*, not a wire-word *encoder*, and touches no USART

osc's name (`usart_tx_config_writer`) and ledger note assert two things the
decode refutes:

**(a) Direction is backwards.** The note says it *reads* state and *produces* a
16-bit TX word. The decode shows the opposite: `FUN_0803d734(param_1, param_2)`
takes a **base pointer `param_1`** and a **4-byte command-descriptor buffer
`param_2`**, switches on the selector byte `*param_2` (values 0–6), and
**read-modify-writes packed bit-fields INTO** `*(param_1+0x20)`, `*(param_1+0x18)`,
`*(param_1+0x1c)`. The new bit *values* come from `param_2[1..3]` (the input
buffer), not from reading the state struct. It is a **descriptor → register-image
bit packer / applier**, not a state → wire-word emitter.

```
0x0803D734:  push  {r4, lr}
0x0803D736:  ldrb  r2, [r1]          ; r2 = *param_2  (selector 0..6)
0x0803D738:  cmp   r2, #6
0x0803D73A:  it hi / pophi {r4,pc}   ; selector > 6 → return (decompile: default: return)
0x0803D73E:  tbb   [pc, r2]          ; 7-way TBB dispatch
...case 0:  *(p+0x20) bit1 = param_2[1]&1 ;  bit3 = (param_2[1]>>1)&1 ; then p+0x18
...case 2:  *(p+0x20) bit5,bit7         ; *(p+0x18) bits[9:8] = param_2[2]&3
...case 4:  *(p+0x20) bit9,bit11        ; *(p+0x1c) ...
...case 6:  *(p+0x20) bit13             ; *(p+0x1c) bits[9:8]
```

**(b) Zero peripheral access.** `peripheral_xrefs` for `function_addr=134469428`
returns **0 rows**. There is no USART2, no SPI3, no MMIO anywhere in the body.
The "16-bit TX word" / "USART" framing is a mislabel: `param_1` is a RAM/struct
register-image base (the `+0x20 / +0x18 / +0x1c` words SCOPE_CMD_PARAMETERS.md
itself calls `config_bitfield_a`, `trigger_edge`, `trigger_level`), not a USART
data register. Whatever later turns these words into a wire frame is a *different*
function downstream (the `dvom_TX` frame builder / `0x08044E74` dispatch family the
osc note already separately tracks).

> Note the tail past `0x0803D872` (`pop {r4,pc}`) is a **separate** function
> starting `0x0803D874: push {r7,lr}` that *does* hit `0x40005c00` — do not fold
> its peripheral writes into this one.

**Ledger action:** rename intent from "usart_tx_config_writer (state→wire
encoder)" to **"config_bitfield_applier"** (descriptor→register-image bit packer,
no peripheral I/O); flip the direction in the note; drop the `FPGA_BLOCKED` /
"needs vendor image" framing — nothing here is FPGA- or silicon-bound. Keep R1.

---

## 2. CONFIRM — static/static agreement on the bit map (V1, NOT execution-verified)

The bit-offset semantics in `SCOPE_CMD_PARAMETERS.md` §"State Structure Offsets"
are **confirmed exactly** by ripcord's decode. This is two **static** methods
agreeing (osc's prior decode + ripcord's Ghidra decode of the same binary) — it
is **V1 (static-inferred), not V2**. No execution oracle touched these bits.

| osc table claim | ripcord decode | verdict |
|---|---|---|
| Type 0: `+0x20` bit 1 = coupling, bit 3 = BW limit | case 0: `+0x20` bit1 = `param_2[1]&1`, bit3 = `(param_2[1]>>1)&1` | CONFIRMED (static) |
| CH2: `+0x20` bit 5 / bit 7; `+0x18` bits[9:8] = range | case 2: `+0x20` bit5/bit7; `+0x18` bits[9:8] = `param_2[2]&3` | CONFIRMED (static) |
| Type 2 trigger: `+0x20` bit 9 = edge, bit 11 = source; `+0x1c` level | case 4: `+0x20` bit9/bit11; `+0x1c` body | CONFIRMED (static) |
| (un-tabled) selector 6: `+0x20` bit 13; `+0x1c` bits[9:8] | case 6: `+0x20` bit13; `+0x1c` bits[9:8] = `param_2[2]&3` | CONFIRMED (static); fills a row osc left blank |
| selectors 1/3/5 are no-ops here (`goto caseD_1` → fall-through) | decode: `case 1/3/5: goto switchD_..._caseD_1` | CONFIRMED (static) — odd selectors are handled elsewhere |

**No V-promotion.** Static agreement is the strongest tier available *for this
function* and it is V1. See §3 for why execution does not reach here.

---

## 3. Execution evidence — a contract covers the address but asserts nothing about it

Contract **#18** (`fpga_bitstream_upload`, `verified=1`, conf 0.97) has
`addr_start=134392260 … addr_end=134668495`, a span that **numerically covers**
`134469428`. But the rule is: a claim is execution-verified only if the covering
contract's *evidence actually asserts it*. Contract #18's claim and evidence are
**entirely** about `FUN_0802a9c4` streaming a 115638-byte flash blob over SPI3
(DT writes, STS-polled handshake, `0x3b`/`0x3a` framing, GPIOB.PB6 CS) — the
**bitstream upload pump**. It says **nothing** about the `FUN_0803d734` bit-packer,
which is merely *called by* that region for a different purpose. Its wide
`addr_end` is an artifact of the mis-cut span, not a behavioral assertion.

Therefore: `contract_id = 18`, `contract_verified = true` (the covering contract
is verified), **but no sub-claim of `FUN_0803d734` is execution-verified.** The
Renode runs (5–8) exercised the acquisition task and the upload pump, not this
config-applier. **`new_evidence = false`** for this function's V column.

---

## 4. GAP — R-side corrections for `firmware/src/drivers/fpga.c`

The clean-room reimplementation models this step as
`fpga_scope_coupling_param()` (fpga.c:900) building a **byte param** from a
`scope_state_t`, then sending it. Two faithful-reimplementation deltas (numerator
growth, not blockers):

1. **Apply-into-register-image vs. read-and-encode.** Stock does not synthesize a
   parameter byte from `scope_state` fields and ship it. It receives a 4-byte
   **command descriptor** (`{selector, val_a, val_b, val_c}`) and RMW-packs those
   values into a **persistent register-image** at `param_1+{0x20,0x18,0x1c}`. The
   clean-room `fpga_scope_coupling_param` collapses CH1+CH2 coupling+BW into one
   byte (`ch1.coupling | ch2.coupling<<2 | bw<<4/5`); stock keeps them as
   **distinct bits in distinct selector cases** (bit1/3 = CH1 group, bit5/7 = CH2
   group, bit9/11 = trigger group, bit13 = selector-6 group). Model the per-selector
   bit layout (table in §2) and the register-image as the persisted object.

2. **No peripheral side effect in this step.** `fpga_scope_coupling_param` is a
   pure helper, which is *directionally* right, but the surrounding clean-room flow
   immediately `fpga_timed_send_cmd(...)`s. In stock, the packer and the wire send
   are separated by at least one queue/handoff (the register image is consumed
   later by the `dvom_TX`/`0x08044E74` path). Keep the bit-pack as a pure mutation
   of a shadow register image; do the USART2 send in the downstream emitter, not
   here.

---

## 5. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08039734` | name | `usart_tx_config_writer` | `config_bitfield_applier` | §1 |
| `08039734` | note | "upstream encoder that reads scope/meter state … produces a 16-bit [TX word]" | "descriptor→register-image bit-field applier (ripcord FUN_0803d734, NOT mis-cut). 7-way TBB on selector param_2[0]=0..6; RMW-packs param_2[1..3] into image words +0x20/+0x18/+0x1c (bit map per SCOPE_CMD_PARAMETERS confirmed). NO peripheral I/O (0 peripheral_xrefs) — 'usart_tx' is a misnomer; called by FUN_0802a9c4. Wire frame is built downstream." | §1, §2 |
| `08039734` | D | D2 | **D3** (full bit-level decode of all 7 selector cases; direction corrected) | §1–2 |
| `08039734` | R | R1 | R1 (unchanged; two reimpl deltas filed, not yet applied) | §4 |
| `08039734` | V | V0 | **V0** (no change — static/static agreement is V1 at most; **no execution evidence reaches this fn**, contract #18 covers the addr but asserts nothing about it) | §3 |
| `08039734` | klass | FPGA_BLOCKED | not blocked — pure RAM bit-packer, nothing silicon-bound | §1, §3 |
| `08039734` | resolved | no | no (decode resolved; reimpl deltas open) | §4 |

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check against the ripcord warehouse + contract ledger.
All four checks pass; the note body did not over-promote.

**1. Crosswalk — PASS.** `SELECT name,addr,size FROM functions WHERE source='stock_v120'
AND addr<=134469428 AND addr+size>134469428` → exactly one row: `FUN_0803d734
@0x803d734`, size 312. Runtime addr `0x803d734` lands in the claimed containing
fn (self). `crosswalk_ok = true`.

**2. Contract reality — PASS, no hallucination.** Contract #18 exists, `verified=1`,
`addr_start=134392260 (0x0802a9c4)`, `addr_end=134668495 (0x0806e0cf)` — so it
*numerically* spans 134469428. But its claim+evidence are wholly about
`FUN_0802a9c4` streaming a 115638-byte flash blob over **SPI3** (DT writes,
STS-polled handshake, 0x3b/0x3a framing, GPIOB.PB6 CS), execution-verified by
`emulate_function.py` for *that* pump. It asserts **nothing** about the
`FUN_0803d734` RAM bit-packer. The wide `addr_end` is the bitstream-blob extent,
not a behavioral assertion about this fn. So `contract_verified=true` is honest
only in the narrow "the covering contract is itself verified" sense — **no
sub-claim of this function is execution-verified**, exactly as §3 states.
`hallucination_found = false` (the cited contract id/addr/evidence all exist and
are not mis-attributed in the note body).

**3. V-promotion audit — PASS.** No claim was scored V2. The bit-map agreement is
correctly tagged **V1 (static/static)**; the note explicitly refuses V2. Nothing
to demote. **Overturned: the draft JSON metadata field `new_evidence` — the draft
carried `new_evidence=true`, which contradicts this note's own (correct) body
(`new_evidence=false` at lines 12/114/159). No execution oracle touched this fn;
the corrected value is `new_evidence = false`.** This is a metadata fix, not a
behavioral overturn.

**4. CORRECT spot-check (disasm 0x0803d734–0x0803d87a) — PASS.** Verified the
load-bearing instructions: entry `push {r4,lr}`; `ldrb r2,[r1]; cmp r2,#6; pophi`
then `tbb [pc,r2]` (selector 0..6, >6 returns); case 0 `bfi r3,ip,#1,#1` /
`bfi ...#3,#1` into `[r2,#0x20]` (bit1/bit3); case 2 bit5/bit7 + `[r0,#0x18]`
bits[9:8]; case 4 +0x20 bit9/bit11 + `[r0,#0x1c]`; case 6 +0x20 bit13 + `[r0,#0x1c]`
bits[9:8]. Direction confirmed: values come **from** `param_2[1..3]` (the `[r1,#1]`
/`[r1,#2]`/`[r1,#3]` loads) and are RMW-packed **into** the `param_1` image words.
**Zero MMIO addresses appear** (peripheral_xrefs=0 confirmed); the only literal
peripheral hit (0x40005c00) is at `0x0803d874` which is reached only after
`pop {r4,pc}` (0x803d872) + `push {r7,lr}` — a **separate** function, so the
"do not fold the tail's writes" caution is correct. Caller verified:
`callee_addr=134469428` is called only from `0x0802a9c4` (sites 0x802b2d4 /
0x802b344), confirming §0.

**Final approved scoring:** **D3 / R1 / V0** (unchanged from the proposal). The
direction correction, peripheral-attribution correction (`FPGA_BLOCKED` dropped),
and full 7-case bit decode justify D2→D3. V stays **V0** — static/static agreement
is **V1 at most** for this fn and there is **no execution evidence reaching it**;
the proposal's `proposed_V=V0` and the §5 ledger delta are upheld. The single
overturn is the draft metadata `new_evidence: true → false`.

---

**Honest scope:** ripcord adds **no execution evidence** for this function
(`new_evidence = false` on the V column) — the one contract whose range covers the
address is about a different routine in the same mis-cut span. What ripcord *does*
add is a clean static decode that (a) **corrects** the osc name and data-direction
(it applies bits *into* a register image from an input descriptor; it does not read
state to emit a wire word), (b) **corrects** the peripheral attribution (zero
MMIO — not a USART writer, not FPGA-blocked), and (c) **confirms** the
SCOPE_CMD_PARAMETERS bit map by independent static decode (V1, not V2). The D
score earns a bump (D2→D3); V stays V0/V1 honestly. The FPGA's reply values were
never in play here — this routine never touches hardware.
