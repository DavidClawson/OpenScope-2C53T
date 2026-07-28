# Reconcile: `scope_autoset_trigger_track` 0x08001c60 ↔ ripcord `FUN_08005c60` — clean decode, no V-promotion available

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`).
**Why this matters / honest headline:** this row was already correctly re-read in
`coverage_ledger.csv` ("MISLABELED in function_names.md … NOT signal-generator setup …
loops over scope channels, scans a 300-byte sample buffer … computing per-channel
min/max"). **Ripcord confirms that corrected read and resolves the register context, but
adds no execution evidence: no verified contract covers this address.** This is an honest
*null-promotion* result — the function stays **V0/V1**, and that is the correct outcome,
not a finding to manufacture. `new_evidence = false`.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `scope_autoset_trigger_track @0x08001c60` | `FUN_08005c60 @0x08005c60` (size 1634 B) |

`0x08001c60 + 0x4000 = 0x08005c60`, which is **exactly** ripcord's `FUN_08005c60` entry.
**Not a mis-cut** — osc carved a real function boundary; the ripcord warehouse fn starts at
the same instruction. Rule confirmed: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. Clean decode — context is self-contained, no corruption to resolve

Unlike the `acq_engine_task` inner-region case, this function has a normal prologue that
sources its own state-base register, so there are **no** `unaff_*`/`in_ZR` artifacts to fix:

```
0x08005C60:  push.w {r4..fp, lr}; sub sp,#4; vpush {d8}; sub sp,#0x18
0x08005C6C:  movw   fp, #0xf8
0x08005C70:  movt   fp, #0x2000       ; fp = 0x200000F8  → global state-struct base
0x08005C74:  ldrb.w r2, [fp, #0x14]   ; state+0x14  (DAT_2000010c, mode byte)
0x08005C78:  ldrb.w r1, [fp, #0x15]   ; state+0x15  (DAT_2000010d, channel mask/count)
0x08005CC4:  movw   sb, #0x12d        ; per-channel stride 0x12d = 301
0x08005CCA:  smlabb r0, sl, sb, fp    ; base = chan*0x12d + 0x200000F8
0x08005CD2:  addw   r8, r0, #0x356    ; + 0x356  → sample ring at state+0x356
```

Resolved context:

| symbol | actual value | meaning |
|---|---|---|
| `fp` (state base) | `0x200000F8` | global state-struct base (same base as contracts #18/#19) |
| `DAT_2000010c` | `state+0x14` | capture-mode byte (`== 3` selects the dual/alt path) |
| `DAT_2000010d` | `state+0x15` | channel descriptor: low nibble = active count, high nibble = start index |
| sample buffer | `0x2000044e` = `0x200000F8 + 0x356` | per-channel 300-byte sample ring, stride `0x12d` (=301) |
| `_DAT_40007408` | DAC `D1DTH12R` | DAC CH1 12-bit data (trigger-comparator DAC) — **write** |
| `_DAT_40007404` | DAC `SWTRG` | DAC CH1 software-trigger (`|= 1`) — **write** |

**What the body does** (matches the osc ledger note byte-for-byte):
1. iterate channels `(state+0x15 & 0xf)` starting at `(state+0x15 >> 4)`;
2. for each, scan its 300-byte ring at `state+0x356` (unrolled ×15, `iVar10=300`) computing
   per-channel **min** (`uVar6`) and **max** (`uVar8`);
3. `switch(DAT_20000127 >> (chan*4) & 0xf)` auto-set state machine:
   - **case 1** — over-range debounce: increment `(&DAT_200000fa)[chan]` (cap 9), call
     `FUN_080058a4`/`FUN_08005a58`, emit selector code `local_31 = 4`;
   - **case 2** — vertical-position trim: float-normalize `(max-min)` against the inline
     divisor `DAT_08005cd8` (`(fVar19/fVar4)*fVar20 + fVar21`), write the result to DAC
     `D1DTH12R` (`_DAT_40007408`) and fire `SWTRG` (`_DAT_40007404 |= 1`); plus an integer
     offset path clamped to `[-100, +100]` writing back to `&DAT_200000fc[chan]`.

So the name `scope_autoset_trigger_track` is apt: it is the **autoset / auto-range vertical
tracker** that reads acquired samples and nudges the trigger-comparator DAC baseline.

**Peripheral xrefs (corrected reading):** `peripheral_xrefs` lists DAC `D1DTH12R`/`SWTRG`
(READ+WRITE) — these are **real** (case-2 DAC repositioning above), not phantom. The
`TMR13 C1DT` (2 hits @0x8006126/0x8006174) and `GPIOD SCR` (1 hit @0x8005e66) xrefs are
**phantom** — but the mechanism is *register-misresolution on a computed pointer*, not
literal-pool junk: the TMR13 hits are `vcvt.u32.f32 s0,s0; vstr s0,[r0]` where `r0` is the
RAM writeback pointer for the integer-offset path (`&DAT_200000fc[chan]`), which the SVD
classifier mis-snapped to `0x40001c34`; the GPIOD hit is a `str r0,[r1]` whose `r1` low half
is set out-of-window. None sit at the `DAT_08005cd8` float-pool region (that region *does*
disassemble as junk `movs/muls`, but it produces no peripheral xref). Verified by disasm
2026-06-13; the phantom conclusion stands, the count/location framing is corrected. (Earlier
draft called TMR13 "lone single-hit" and located both "around DAT_08005cd8" — both wrong.)

---

## 2. Execution evidence — NONE covers this address

Querying `build/contracts.sqlite` for any `verified=1` contract whose `[addr_start,addr_end)`
brackets `134241376` (= `0x08005c60` runtime): **no match.** The nearest verified contracts
are #1 `memset` (134238908–134238976), #12/#13 (LCD), #18 (134392260–134668495, bitstream
upload — does not start until well past this fn), and #19 `acq_engine_runtime`
(134460500–134467592). None overlaps `FUN_08005c60`.

→ `contract_id = null`, `contract_verified = false`. **No sub-claim here is
execution-verified.** Every claim below is tagged `static-inferred`.

The one adjacent ripcord fact is contract #19's **static** description of the roll ring as
`state+0x356/+0x483`. That corroborates the `state+0x356` buffer address my decode reads —
but #19's roll-ring naming is itself decompile-derived (its *execution*-verified portion is
the burst path at `state+0x5B0/+0x9B0` and the mode-dispatch map, not the roll ring scanned
here). So it is **static-on-static** agreement: confidence-raising, **not** a V-promotion.

---

## 3. CONFIRM — osc decode and ripcord agree (all static-inferred)

| claim | osc decode | ripcord corroboration | tier |
|---|---|---|---|
| reads global state at `0x200000F8` | yes | prologue `fp=0x200000F8` | static-inferred |
| sample buffer is `state+0x356`, stride 0x12d (301), 300 bytes scanned | `0x2000044e` ring | `addw r8,r0,#0x356`; contract #19 names roll ring `state+0x356` (static) | static-inferred |
| iterates `(state+0x15 & 0xf)` channels from `(>>4)` index | yes | `ldrb [fp,#0x15]; and #0xf; lsr #4` | static-inferred |
| computes per-channel min/max over the ring | yes | unrolled ×15 min/max body | static-inferred |
| writes the trigger-comparator DAC (CH1 data + SW-trigger) | yes | `peripheral_xrefs` DAC `D1DTH12R`+`SWTRG` WRITE | static-inferred |
| this is autoset/range tracking, **not** siggen setup | yes (ledger correction) | body is a sample-driven DAC-baseline tracker | static-inferred |

No CORRECT bucket: ripcord does not overturn anything in the osc decode, and there was no
corrupt context to repair (§1).

---

## 4. GAP — R-side (`firmware/src/`) divergences

The clean-room tree has the **leaf** this function drives but **not the function itself**:

1. **No autoset / min-max trace-tracker exists.** `drivers/scope_trigger.c` implements only
   the DAC primitive (`scope_trigger_dac_compute/raw/set`, writing `0x40007408` data +
   `0x40007404` SWTRG — the exact registers stock writes in case 2), and it is called
   **only from `drivers/usb_debug.c`** (a debug shim), never from a real acquisition loop.
   `ui/scope_state.c` is pure state-struct management. **Missing:** the per-channel
   sample-ring scan → min/max → over-range debounce (`state+0xfa` counters, cap 9) →
   DAC-reposition state machine that `FUN_08005c60` is. Concrete reimplementation item:
   a `scope_autoset_track(scope_state_t*)` over the `state+0x356` ring driving
   `scope_trigger_dac_set`.

2. **Divisor mismatch to verify on bench.** `scope_trigger.c` hard-codes `TRIG_DIVISOR =
   200.0f` (from the DAC leaf `FUN_080018a4` @`0x08001a54`). The autoset float normalization
   here uses a **different** inline constant `DAT_08005cd8` (in `FUN_08005c60`'s own literal
   pool). Do not assume they are equal — the autoset gain is its own constant and must be
   read from `0x08005cd8`, not reused from the trigger-DAC leaf.

3. **Over-range debounce + selector code unmodeled.** The case-1 path (counter cap 9, calls
   `FUN_080058a4`/`FUN_08005a58`, emits internal selector `local_31 = 4`) has no R-side
   analogue. Note `local_31 = 4` is an **internal selector/return code**, not a wire-level
   FPGA transaction — keep that distinction when reimplementing.

---

## 5. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08001c60` | note | "FPGA_BLOCKED … MISLABELED … scans a 300-byte sample buffer …" | append: "ripcord crosswalk FUN_08005c60 (not a mis-cut): context clean, fp=state 0x200000F8, ring=state+0x356 stride 0x12d, DAC D1DTH12R+SWTRG writes real (autoset vertical tracker). NO verified contract covers it — stays static; see scope_autoset_trigger_track_ripcord_reconcile" | §1–§3 |
| `08001c60` | D | D3 | **D3** (unchanged — decode was already correct) | §3 |
| `08001c60` | R | R1 | **R1** (unchanged — leaf `scope_trigger.c` exists, tracker fn does not) | §4 |
| `08001c60` | V | V0 | **V0** (no execution evidence; corroboration is static-on-static, V1 ceiling at best) | §2 |
| `08001c60` | klass | hardware / FPGA_BLOCKED | reclassify: **MCU-side DSP/autoset**, not FPGA-blocked — it consumes already-acquired samples and writes an on-chip DAC; nothing here needs the FPGA reply silicon | §1 |

**Honest scope:** ripcord did **not** advance this row's verification. It confirms the
already-corrected decode, resolves the register names (state base, sample ring, the two DAC
registers), and reclassifies it out of the FPGA_BLOCKED bucket (this is MCU-local autoset
DSP, not an FPGA-boundary transaction). But because no `verified=1` contract covers
`0x08005c60`, **no V0→V2 promotion is available** and the `state+0x356` agreement is
static-on-static. `new_evidence = false`: the osc ledger already had the substantive read.
The actionable output is the R-side GAP — the autoset trace-tracker is a genuinely missing
clean-room function, and its float divisor must be read from `0x08005cd8`, not borrowed from
the `200.0f` trigger-DAC leaf.

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check against the live ripcord warehouse + contract ledger.

**1. CROSSWALK — PASS.** `SELECT … FROM functions WHERE source='stock_v120' AND addr<=134241376
AND addr+size>134241376` returns exactly one row: `FUN_08005c60 @0x8005c60` size 1634. Runtime
addr is the function *entry* (addr == 134241376), so "not a mis-cut / entry == runtime addr" is
literally true. `crosswalk_ok = true`.

**2. CONTRACT REALITY — PASS (null is honest).** `contract_id = null`; the draft claims no
`verified=1` contract covers `134241376`. Confirmed: `SELECT … WHERE addr_start<=134241376 AND
addr_end>134241376` returns **zero rows**. Cited neighbors all exist with the stated bounds and
all have `verified=1` but none bracket this addr: #1 (134238908–134238976), #12 (134244328–…),
#13, #18 (134392260–134668495), #19 (134460500–134467592). No hallucinated contract id/addr.

**3. V-PROMOTION — no demotions needed; V stays V0.** The draft scored nothing V2. The one lean
on contract #19 is correctly characterized: #19's *claim* text names the roll ring `state+0x356/
+0x483`, but its **execution-verified** payload (Run-set 2026-05-30, Runs through 8) is the burst
path `state+0x5B0/+0x9B0`, the 9-mode TBH dispatch map, mode-1 flash-LUT range gate, mode-9 0x0A,
and the PB6/CS handshake — **not** the `state+0x356` scan this function performs. So the `state+
0x356` agreement is genuinely static-on-static (V1 ceiling), not V2. `approved_V = V0`.

**4. SPOT-CHECK (disasm) — confirms decode, corrects two supporting details.**
`disasm.py` over `FUN_08005c60` verifies the load-bearing instructions byte-for-byte:
`movw fp,#0xf8; movt fp,#0x2000` (=0x200000F8), `ldrb [fp,#0x14]`/`[fp,#0x15]`, `and r2,r1,#0xf`
+ `lsr sl,r1,#4`, `movw sb,#0x12d`, `smlabb r0,sl,sb,fp`, `addw r8,r0,#0x356`. All CONFIRM-bucket
claims hold. **Overturned (factual, not verdict-changing):** the §1 "Peripheral xrefs" note said
`TMR13 C1DT` was a *lone single-hit* and that both phantom xrefs sit *around DAT_08005cd8*. The
warehouse shows TMR13 C1DT has **2** WRITE hits at 0x8006126/0x8006174 (`vstr s0,[r0]` to a
computed RAM pointer, the integer-offset writeback — SVD mis-snap to 0x40001c34), and GPIOD SCR's
single hit is at 0x8005e66 — none at the float pool. Phantom **conclusion stands**; count/location
framing corrected in-body above.

**Final verdict:** D3 / R1 / **V0** — all approved unchanged. The draft was already maximally
skeptical on V; nothing to demote. Only the peripheral-xref *rationale* was overstated and has
been fixed. No hallucinated contract or address. `new_evidence = false` confirmed.
