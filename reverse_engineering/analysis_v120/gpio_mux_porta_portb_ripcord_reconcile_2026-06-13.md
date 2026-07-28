# Reconcile: `gpio_mux_porta_portb` 0x08001a58 ↔ ripcord `FUN_08005a58` — clean decode, NO execution coverage

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`, contracts #1–#19) and the disassembler/decompile
tables. R-side compared against `firmware/src/drivers/fpga.c`.
**Bottom line up front:** ripcord **confirms and cleans** osc's static decode of this routine
(it is a genuine standalone 506-byte function — NOT a mis-cut, NOT corrupt-context), but adds
**no execution evidence**: no `verified=1` contract covers `0x8005a58`. So this is a
**static-on-static CONFIRM**, not a V-promotion. `new_evidence=false` in the strict sense
(no oracle trace), but ripcord does **correct one R-side structural error** and surfaces a
**concrete GAP** (this function's "Part 2" — a per-range cal/offset computation — is absent
from the R-side `fpga_set_scope_frontend_range`). Booked V0 stays V0.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `gpio_mux_porta_portb @0x08001a58` | `FUN_08005a58 @0x08005a58`, size **506 B** |

`0x08001a58 + 0x4000 = 0x08005a58`, which is the **exact entry** of ripcord's
`FUN_08005a58` (addr 134240856, size 506). **Not a mis-cut** — osc and ripcord agree on the
function boundary. `was_miscut=false`. (Contrast `acq_engine_task`, where osc's `0x08037800`
landed inside a 7 KB task.)

---

## 1. CONFIRM — independent static agreement (osc decode ↔ ripcord decode)

ripcord's `decompiled_c` + disasm of `0x08005a58` reproduces osc's structure exactly. Two
parts:

**Part 1 — `switch(param_1)` over 0..9, writes GPIOA/GPIOB only** (peripheral_xrefs:
`GPIOB SCR ×9, GPIOA CLR ×7, GPIOA SCR ×7, GPIOB CLR ×5`; no GPIOC/GPIOE/GPIOD):

| stock reg (decompile) | AT32 meaning | bits seen |
|---|---|---|
| `_DAT_40010810` | **GPIOA SCR** (set, 0x40010800+0x10) | `0x8000`=PA15 |
| `_DAT_40010814` | **GPIOA CLR** (0x40010800+0x14) | `0x8000`=PA15 |
| `_DAT_40010c10` | **GPIOB SCR** (0x40010c00+0x10) | `0x800`=PB11, `0x400`=PB10 |
| `_DAT_40010c14` | **GPIOB CLR** (0x40010c00+0x14) | `0x400`=PB10 |
| `(&DAT_40010c10 + (0xffff0000\|uVar2))` | GPIOA reg reached via `r2=0x40010c10 + r1` where `r1=0xfffffc00/fc04` → **0x40010810/0x40010814** | GPIOA SCR/CLR |

So Part 1 is the **per-range front-end gain/attenuator relay mux** on the PA/PB bank, exactly
as osc's note states ("per-range switch sets the gain-select pins; matches PA15/PA10 gain,
PB10/PB11"). The `0xffff0000`-offset trick is the compiler folding the GPIOA write through the
GPIOB base pointer — ripcord and osc both decode it to the same GPIOA SCR/CLR targets.

**Part 2 — `switchD_..._default:` (always runs after the switch), reads state struct +
computes a float** (disasm `0x08005bd4`+): loads `range_index = state[0x2D]` (state base
`0x200000F8`), selects one of **three** sample-buffer pairs in the state struct by
`(range<5)`, `(range==4 || state[0x14]==3)` predicates:

| predicate | ptr1 (offset from `state + param_1*2`) | ptr2 |
|---|---|---|
| `range>=5` | `+0x314` | `+0x2d8` |
| `range==4 \|\| state[0x14]==3` | `+0x328` | `+0x2ec` |
| else | `+0x33c` | `+0x300` |

then `fVar = (s32)(u16[ptr1] - u16[ptr2]) / 200.0` and a VFP tail
(`ldrsb state[5]; +0x64; vcvt; vmul; vadd u16[ptr2]; ...`) — a per-range **scale/offset
calibration** of a measured delta, whose final `(u32)` result is **stored to TMR13_C1DT
(`0x40001c34`)** via `vstr s0,[r0]; bx lr` (disasm `0x08005c56`). ripcord's decompile
(`fVar4/fVar5/fVar6`, `in_fpscr`) matches osc's read of this tail. **No corrupt context to
resolve** — this function has a normal prologue and Ghidra decoded it cleanly in both
projects, so unlike the `acq_engine_task` case there are no `unaff_*`/`in_ZR` artifacts to
fix.

> **Constant correction (adversarial verification, see §7):** the divisor is **`200.0`**
> (literal `0x43480000` in the function's literal pool at runtime `0x08005c5c`, the
> `vldr s2,[pc,#0x40]` target), **not `1023.0`**. The draft's `/1023.0` (and its "10-bit
> full-scale normalize" rationale) was a hallucinated constant; no `0x447fc000` appears
> anywhere in the function body. Read everywhere below as `/200.0`.

**Verdict:** every osc sub-claim on this row is independently re-derived by ripcord's static
decode. This is **two static methods agreeing** — strong, but **not execution**. Tag all of
the above **static-inferred**.

---

## 2. EXECUTION EVIDENCE — none. No contract covers this address.

Queried `build/contracts.sqlite` (contracts #1–#19). The address `0x8005a58`
(134240856 .. 134241362) is **not covered by any contract's `[addr_start, addr_end)`**:

- #1 `memset` 134238908–134238976 — below, no overlap.
- #19 `acq_engine_runtime` 134460500–134467592 — far above (the SPI3 acquisition task).
- #18 `fpga_bitstream_upload` 134392260–134668495 — covers a wide upload span but **starts at
  `0x0802A9C4`**, above this function; does not include `0x8005a58`.
- #12/#13 (LCD/DMA blit), #5 (DMA ISR) — different addresses.
- The remaining contracts (#2,#3,#4,#6–#11,#14–#17) are address-less peripheral/topology
  claims, none asserting anything about `0x8005a58` or the GPIOA/GPIOB range mux.

**Therefore `contract_id=null`, `contract_verified=false`.** The GPIO mux relay state and the
Part-2 cal float are **not execution-verified**. Note also: contract #19's verified runtime
work is the *SPI3 sample read path*; it confirms PB6 (SPI3 CS) handshakes, **not** the
PA15/PA10/PB10/PB11 **gain-relay** pins this function drives — those are a different GPIOB/
GPIOA bit set and were never exercised by any oracle run. Do not borrow #19's `verified=1` for
this row.

---

## 3. CORRECT — ripcord fixes one R-side structural error

The clean-room equivalent is `fpga_set_scope_frontend_range(range_idx)` in
`firmware/src/drivers/fpga.c:728`, whose own comment names this function
("Approximate gpio_mux_portc_porte / gpio_mux_porta_portb"). Two corrections fall out of the
clean ripcord decode:

1. **`gpio_mux_porta_portb` writes GPIOA/GPIOB ONLY — never GPIOC or GPIOE.** ripcord's
   `peripheral_xrefs` for this function are exclusively `GPIOA {SCR,CLR}` and
   `GPIOB {SCR,CLR}` (plus one stray `TMR13 C1DT` from a tail/adjacency — see §5). The R-side
   `fpga_set_scope_frontend_range` writes `GPIOC->clr` and `GPIOE->scr/clr` in every case
   (lines 737–799). Those PC/PE writes belong to the **sibling** `gpio_mux_portc_porte`, a
   *separate* stock function — the R-side fused two stock functions into one and attributed
   PC/PE relay state to a routine that does not touch them. Faithful reimplementation must
   **split** these: `gpio_mux_porta_portb` = PA15/PA10 + PB10/PB11 only.

2. **The R-side per-range PA/PB truth table is a stated "approximation", and it does not match
   the stock switch.** Stock Part 1 keys 10 distinct cases (0..9) with the
   `0x8000/0x800/0x400` bit pattern above; the R-side collapses them into 6 grouped cases with
   a hand-built table flagged "intentionally simple … reconstructed truth table". ripcord
   gives the actual per-case GPIOA/GPIOB SCR-vs-CLR pattern to replace the placeholder (the
   per-case bit writes are enumerable from the disasm at `0x08005a62`–`0x08005bd2`).

Both corrections are **static-inferred** (ripcord decode vs R-side source); neither is
execution-verified — the relay pins were never driven in an oracle run.

---

## 4. GAP — R-side omits this function's "Part 2" cal computation entirely

`fpga_set_scope_frontend_range` ends after setting relay pins (line 806). The stock function
**always** continues into Part 2 (§1): read `state[0x2D]` range index, pick a state-struct
sample-buffer pair by the `(range<5)/(==4||state[0x14]==3)` predicates, compute
`(u16[a]-u16[b]) / 200.0` and the `state[5]+0x64` VFP offset, and **store the `(u32)` result
to TMR13_C1DT (`0x40001c34`)**. That per-range delta→float **calibration/measurement** step
has **no analogue** in the R-side range setter. Concrete reimplementation items:

- The Part-2 buffer-pair selector (offsets `+0x314/+0x2d8`, `+0x328/+0x2ec`,
  `+0x33c/+0x300` from `state + range*2`) — these index into the per-channel sample/cal arrays
  in the `0x200000F8` state struct (same struct family as the acq engine's `+0x5B0/+0x9B0`).
- The `/200.0` scale (`vldr s2, [pc,#0x40]` constant `0x43480000`) and the `state[5]+0x64`
  signed-byte offset feeding the VFP path — a vertical-position / zero-cal correction, whose
  output is written to **TMR13_C1DT** (the timer compare/data register), not discarded.

Until Part 2 is reimplemented, the clean-room front-end range path sets relays but skips the
per-range cal that stock performs on every range change. **This is the numerator-growth item**
for this row.

---

## 5. Note on the `TMR13 C1DT` xref  — CORRECTED (was wrongly dismissed as over-read)

ripcord shows one `TMR13 C1DT DATA` xref (`0x40001c34`) on this function. **An earlier draft
of this note dismissed it as adjacency/over-read at the function boundary. That was wrong.**
The disasm is unambiguous: at `0x08005c3a` Part 2 loads `r0 = 0x40001c34` (TMR13_C1DT),
finishes the float pipeline (`vmul/vadd/vcvt.u32.f32`), and at `0x08005c56` executes
`vstr s0, [r0]` immediately followed by `bx lr` at `0x08005c5a`. This is the **terminal,
in-body, control-flow-reachable store of Part 2's computed result** — i.e. TMR13_C1DT is the
*destination* of the whole per-range cal computation, not an over-read. The single DATA-typed
xref is genuine and load-bearing: this function's observable side effect besides the GPIO
relays is a write of `(u32)((u16[a]-u16[b])/200.0 * (state[5]+0x64) + u16[b])` into
TMR13_C1DT. Treat the TMR13 write as a real part of the function's behavior; it is plausibly
a PWM/timer-driven vertical-position or trigger-level output. Still **static-inferred** (no
oracle run drove it), but it is a finding, not noise.

---

## 6. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08001a58` | note | "FPGA_BLOCKED … Part 2 (always runs): s…" | "ripcord-confirmed clean decode (FUN_08005a58, NOT mis-cut, no corrupt context). Part1 = PA15/PA10 + PB10/PB11 range-mux switch(0..9), GPIOA/GPIOB ONLY. Part2 = state[0x2D]-keyed buffer-pair cal: (u16[a]-u16[b])/200.0 + state[5]+0x64 VFP offset, result stored to TMR13_C1DT (0x40001c34). R-side fpga_set_scope_frontend_range fuses-in the PC/PE sibling (wrong) and omits Part2 (gap). NO contract covers 0x8005a58 — V0 stands." | §1–§4 |
| `08001a58` | V | V0 | **V0** (unchanged — no `verified=1` contract covers this addr; static-on-static only) | §2 |
| `08001a58` | resolved | no | **partial** (decode clean + R-side corrections identified; relay/cal values still hardware-bound) | §3,§4 |
| `08001a58` | klass note | FPGA_BLOCKED | keep FPGA_BLOCKED for the *relay-state-vs-silicon* question; the *decode* is no longer blocked | §2 |

**Honest scope:** ripcord adds **no execution evidence** here — there is no oracle trace of
the gain-relay pins, so the firmware's actual per-range PA/PB relay states remain
silicon-bound and this row stays **V0**. What ripcord *does* add is a clean, mis-cut-free,
corrupt-context-free confirmation of osc's static decode, plus two R-side corrections (split
the PA/PB mux from the PC/PE sibling; replace the placeholder truth table) and one R-side gap
(Part-2 per-range cal). Useful, but `new_evidence=false` in the strict execution sense — and
that is the correct, honest verdict, not a manufactured promotion.

---

## 7. Verification (adversarial, 2026-06-13)

Re-ran every load-bearing check against the ripcord warehouse and the stock binary.
**Default-to-skepticism pass; 2 claims overturned, neither changes D/R/V.**

**Checks performed:**

1. **CROSSWALK — HOLDS.** `scripts/query` on `functions` (stock_v120) at addr 134240856
   returns exactly one row: `FUN_08005a58`, addr `0x8005a58`, size 506. `0x08001a58 + 0x4000
   = 0x08005a58` lands on the entry, not inside a larger body. `was_miscut=false` confirmed.
   `crosswalk_ok=true`.

2. **CONTRACT REALITY — HOLDS.** Draft cites `contract_id=null`. Dumped all 19 rows of
   `build/contracts.sqlite`. No contract has `[addr_start,addr_end)` covering 134240856:
   #1 `memset` 134238908–134238976 (below), #12 134244328+ (above), #18 starts 134392260
   (above), #19 134460500+ (above). No `gpio`/`mux`/`range` claim asserts this address.
   `contract_verified=false` is correct. No borrowed `verified=1`. No hallucinated contract id.

3. **V-PROMOTION — HOLDS at V0.** No V2 claims in the draft; nothing to demote. No execution
   coverage exists, so V0 is the ceiling and is correct.

4. **DISASM SPOT-CHECK — overturned 2 claims.** Disassembled `0x08005a58`+200 insns and read
   the literal pool directly from `targets/stock_v120/stock_v120.bin`:
   - Part 1 verified: every store targets only `0x40010810/814` (GPIOA SCR/CLR) and
     `0x40010c10/c14` (GPIOB SCR/CLR); the `0xffff0000`-fold (`str r3,[r2,r1]` with
     `r1=0xfffffc00/fc04`, `r2=0x40010c10`) reaches GPIOA. `peripheral_xrefs` corroborates:
     GPIOA SCR×7/CLR×7, GPIOB SCR×9/CLR×5, **plus one TMR13 C1DT**. No GPIOC/GPIOE/GPIOD.
     → CONFIRM and CORRECT #1 (GPIOA/GPIOB only; split PC/PE sibling) **hold**.
   - Part 2 verified: `ldrb [r1,#0x2d]` (state[0x2D]) at `0x08005bdc`; predicates `cmp #5`
     / `cmp #4` / `ldrb [r1,#0x14] cmp #3`; offset pairs `+0x314/+0x2d8`, `+0x33c/+0x300`,
     `+0x328/+0x2ec` all match; `ldrsb [r1,#5] + 0x64` matches. → **holds**.
   - **OVERTURNED #1 — the divisor is `200.0`, not `1023.0`.** `vldr s2,[pc,#0x40]` at
     `0x08005c1a` resolves (Align(PC,4)) to runtime `0x08005c5c`; the literal there is
     `0x43480000` = **200.0**. `0x447fc000` (1023.0) appears nowhere in the function body.
     The draft's `/1023.0` and its "10-bit full-scale normalize" rationale were a fabricated
     constant. Fixed throughout §1, §4, §6 and the resolved-symbols.
   - **OVERTURNED #2 — the TMR13_C1DT write is real, not over-read.** §5 had dismissed it.
     The disasm shows Part 2's float pipeline terminating in `vstr s0,[0x40001c34]; bx lr`
     (`0x08005c56`/`0x08005c5a`) — TMR13_C1DT is the *destination* of the per-range cal
     result, reachable in-body. Rewrote §5 to record this as a genuine side effect.

**Net effect on the verdict:** both overturns are *internal-accuracy* corrections within a
clean static decode — one is a wrong constant, one is an under-claim. Neither adds nor removes
execution evidence, and neither breaks the function-boundary or peripheral-set claims.

- **D = D3** (unchanged; decode is in fact slightly *more* complete now — Part 2's output
  destination is identified).
- **R = R1** (unchanged; R-side `fpga_set_scope_frontend_range` still exists as an
  approximation, still fuses the PC/PE sibling, still omits Part 2 — now including its TMR13
  output and the `/200.0` scale).
- **V = V0** (unchanged; no `verified=1` contract covers `0x8005a58`; static-on-static only).

`approved_V = V0`. `hallucination_found = true` (the `/1023.0` constant). `crosswalk_ok = true`.
