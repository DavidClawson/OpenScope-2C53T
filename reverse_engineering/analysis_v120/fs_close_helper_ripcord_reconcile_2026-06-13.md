# Reconcile: `fs_close_helper` 0x08034070 ↔ ripcord `FUN_08038070`/`FUN_08038078` — confirm mislabel, correct the reset thunk

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`).
**Why this matters:** `0x08034070` is booked in `coverage_ledger.csv` as
**D3/R0/V0, FPGA_BLOCKED, score=no**, with a (correct) note that it is **mislabeled** —
"not 'fs_close_helper' and not 8B … mid-body of a larger soft-float scope DISPLAY-SCALING
recompute." This reconcile **confirms that mislabel from the warehouse side**, resolves the
8-byte/626-byte overlap, and **corrects one sub-guess** osc made: the "first op
`FUN_08028314`" is **not VFP** — it is a `SCB_AIRCR` `SYSRESETREQ` system-reset routine.
There is **no FPGA content here** and **no execution contract covers this address**; this is
a static-only reconcile and a small correction, not a V-promotion.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `fs_close_helper @0x08034070` | `FUN_08038070 @0x08038070` (8 B) |
| real body | (osc: "mid-body of a larger recompute") | `FUN_08038078 @0x08038078` (626 B) |

`0x08034070 + 0x4000 = 0x08038070`. Ripcord **does** cut a standalone function at exactly
`0x08038070`, but it is **8 bytes**, not the body osc's `fs_close_helper` label implied.
`was_miscut` is therefore **partial/yes**: the 8 B `FUN_08038070` is a thunk; the real
soft-float work is the adjacent 626 B `FUN_08038078 @0x08038078` (= osc `0x08034078`, which
osc already books separately as `scope_display_refresh`). General rule holds:
`ripcord_runtime_addr = osc_project_addr + 0x4000`.

What ripcord actually shows at `0x08038070` (disasm):

```
0x08038070:  bl   #0x802c314        ; tail-call into a no-return routine
0x08038074:  movs r0, r0            ; 0x0000 padding (nop-like)
0x08038076:  movs r0, r0            ; 0x0000 padding
0x08038078:  push.w {r4..fp, lr}    ; <-- real 626 B function starts here
0x0803807C:  sub  sp, #4
0x0803807E:  vpush {d8, d9}
0x08038082:  movw r5, #0xf8 ; movt r5,#0x2000  ; r5 = 0x200000F8 state base
0x0803808A:  ldrb r0, [r5, #0x2d]              ; r0 = device_mode (state+0x2D)
```

`0x08038070` is an 8-byte block: one `bl` plus two halfwords of zero padding. Ghidra's
**decompiler hallucinated the 626 B body into `FUN_08038070`** because the `bl` targets a
no-return routine (see §1) and there is no `bx`/return after it, so the decompiler flowed
straight into the adjacent `FUN_08038078`. That is why `decompiled_c` for `0x08038070` and
`0x08038078` are nearly **byte-identical** in the warehouse — the only difference is the
leading `FUN_0802c314();` call. They are not two routines; they are one body plus a thunk
that the decompiler merged.

---

## 1. CORRECT — the "first op" is a system-reset thunk, not VFP

osc's note says: *"first op (FUN_08028314, also VFP not 'GPIOD_BOP') is mislabeled too."*
osc was right that the original `GPIOD_BOP` label is wrong, but its **replacement guess
("VFP") is also wrong.** Ripcord disassembly of the `bl` target
(`0x0802c314` = osc `0x08028314`) shows a **Cortex-M system-reset routine**:

```
0x0802C314:  movw r0,#0xed0c ; movt r0,#0xe000   ; r0 = 0xE000ED0C = SCB->AIRCR
0x0802C318:  dsb  sy
0x0802C320:  ldr  r1,[r0]
0x0802C324:  and  r1,r1,#0x700                   ; preserve PRIGROUP
0x0802C326:  movs r2,#4 ; movt r2,#0x5fa         ; r2 = 0x05FA0004 (VECTKEY | SYSRESETREQ)
0x0802C32C:  orrs r1,r2
0x0802C32E:  str  r1,[r0]                         ; AIRCR = 0x05FA…04  -> SYSRESETREQ
0x0802C330:  dsb  sy
0x0802C334:  nop;nop;nop;nop
0x0802C33C:  b    #0x802c334                       ; spin forever until reset lands
```

This is the canonical `NVIC_SystemReset()` body (write `VECTKEY 0x05FA` + `SYSRESETREQ`
bit 2 to `SCB_AIRCR @0xE000ED0C`, then dead-loop). Consequences:

| osc symbol | osc guess | ripcord-corrected meaning |
|---|---|---|
| `FUN_08028314` (= ripcord `FUN_0802c314`) | "VFP, not GPIOD_BOP" | **`NVIC_SystemReset` — `SCB_AIRCR SYSRESETREQ` + dead-loop. No-return.** |
| `FUN_08038070` (8 B) | (part of "fs_close_helper") | **a `bl NVIC_SystemReset` thunk** — likely an assert/fault-shim or fatal-error trampoline, not the display body |

Because `FUN_0802c314` **never returns**, the `bl` at `0x08038070` is a control-flow dead
end; the 626 B display-scaling body at `0x08038078` is reached by its own callers, not by
falling out of `0x08038070`. The merged decompile is an artifact, exactly as osc suspected.

This also lets osc retire one open thread in its note: the truncated *"…0x40022004/44=
0xcdef89ab FMC KEYR/OPTKEYR + option-byte writers belong to the C…"* — those FMC unlock
writes are **not in this function**. `peripheral_xrefs` for both `0x08038070` and
`0x08038078` return **zero MMIO accesses** (query below). The FMC option-byte writers are a
different routine; do not attribute them here.

---

## 2. CONFIRM — osc's mislabel call is correct; ripcord agrees (static only)

These claims are asserted **both** by osc's static decode **and** by ripcord's static
warehouse. **No execution contract covers `0x08038070`** (see §3), so every row here is
**static-inferred** — agreement of two static decodes, **not** an emulation promotion.

| claim | osc decode | ripcord static evidence | tier |
|---|---|---|---|
| not `fs_close_helper`, not an 8 B logic body | ledger note | 8 B = one `bl` + 0x0000 padding; real body is the adjacent 626 B fn | static-inferred · **CONFIRMED** |
| body is a soft-float DISPLAY-SCALING recompute | "matches function_names.md L43 'Scope display refresh'" | `FUN_08038078`: device_mode `state+0x2D`, 64-bit VFP helpers `FUN_0804…`, no MMIO | static-inferred · **CONFIRMED** |
| reads flash Y-transform LUT `&DAT_080465c8` indexed `% 3` | ledger note | decompile: `(&DAT_080465c8)[DAT_20000125 + (DAT_20000125/3)*-3 & 0xff]` | static-inferred · **CONFIRMED** |
| writes scale/measurement state block `0x20000eb8..0x20000ef0` | ledger note | decompile writes `_DAT_20000eb8/eb9/eba/ebc/ebe/ec8/ecc/ed0/ed4/ed8` | static-inferred · **CONFIRMED** |
| no FPGA / no peripheral I/O | (FPGA_BLOCKED tag is conservative) | `peripheral_xrefs` = **0 rows** for both addrs | static-inferred · **CONFIRMED** |

**The `FPGA_BLOCKED` tag is wrong on the merits.** There is no FPGA, SPI3, USART2, DMA, or
any MMIO in this region — it is pure soft-float + SRAM-state arithmetic against a flash LUT.
This row was never bench-blocked; it was mislabeled. It needs no microscope/JTAG, only a
relabel.

---

## 3. Execution evidence — none covers this address

`addr = 0x08038070 = 134447216`. Scanning `contracts.sqlite`:

- **#18** `fpga_bitstream_upload` has `addr_start=134392260 … addr_end=134668495`, which
  **numerically spans** this address — but #18 is the SPI3 **bitstream upload pump**
  (`FUN_0802a9c4`), and its `addr_end` is an explicitly **over-wide "mis-cut fragment
  spanning ~0x0802A8xx-0x0802B2xx" estimate**, not a semantic body covering
  `0x08038078`. The display-scaling routine is unrelated to the upload pump. **Do not** treat
  #18 as covering this function. (Its own evidence text is entirely about the 115638-byte
  SPI3 stream; it asserts nothing about `0x08038078`.)
- **#19** `acq_engine_runtime` covers `134460500..134467592` — **does not** include
  `134447216`.
- No other `verified=1` contract's `[addr_start,addr_end)` covers this address.

**Verdict: `contract_id = null`, `contract_verified = false`.** Nothing here is
execution-verified. **No V-promotion is available.** Keep `V0`. The honest move is the
relabel + a `V`-to-`VNA` reclass (this is pure UI the clean-room intentionally doesn't
mirror — see §4), not a V0→V2.

---

## 4. GAP — R-side: clean-room renders its own scope view (NA_OURS, no fix owed)

The R-side equivalent is `firmware/src/ui/scope_ui.c`, which computes display geometry from
its **own** `vdiv_table[…].label` / `volts_per_pixel` model (lines ~257–597) rather than the
stock soft-float recompute that writes the `0x20000eb8..0x20000ef0` state block via the
`&DAT_080465c8` `% 3` LUT. This is a **deliberate clean-room reimplementation**, consistent
with osc's sibling ledger row `08034078,scope_display_refresh` already booked **RNA/VNA,
NA_OURS** ("we render our own scope view"). There is **no faithful-reimplementation GAP to
fix here** — stock's per-mode Y-transform recompute is intentionally not mirrored.

The one R-side item worth a cross-check (not a gap in *this* function): the stock
`NVIC_SystemReset` thunk (§1) corresponds to R-side `firmware/src/drivers/dfu_boot.c` /
`input_handler.c` (both reference `SYSRESETREQ`/`NVIC_SystemReset`). Stock reaches reset via
the 8 B `bl`-thunk at `0x08038070`; the clean-room calls the CMSIS helper directly. Behaviorally
equivalent; no action.

---

## 5. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08034070` | name | `fs_close_helper` | `reset_thunk_then_display_scale_entry` (8 B: `bl NVIC_SystemReset` + pad; real body is `08034078`) | §0,§1 |
| `08034070` | note (correction) | "first op FUN_08028314 … VFP" | "FUN_08028314 = NVIC_SystemReset: writes SCB_AIRCR(0xE000ED0C)=0x05FA…04 SYSRESETREQ then dead-loops (no-return); NOT VFP. Decompile merge of 0x08034070 thunk + 0x08034078 body is a Ghidra artifact (bl to no-return). FMC OPTKEYR writers are NOT in this fn — peripheral_xrefs=0." | §1 |
| `08034070` | klass | `FPGA_BLOCKED` | **reclass: not FPGA, no MMIO; pure soft-float UI scaling. RNA/VNA (NA_OURS) like sibling 08034078** | §2,§4 |
| `08034070` | R / V | `R0 / V0` | **`RNA / VNA`** (clean-room renders its own scope view; nothing to reimplement, nothing to execution-verify) | §3,§4 |

(Keep `D3`. The decode is not novel beyond what osc already had; see scope note.)

---

## 6. Honest scope

**new_evidence = true, but small.** osc had already cracked the substance: this is not
`fs_close_helper`, not 8 B, and is the soft-float display-scaling region. Ripcord adds three
things on top of that: (a) it **disambiguates the 8 B thunk from the 626 B body** and
explains the duplicated decompile as a Ghidra `bl`-to-no-return merge artifact; (b) it
**corrects osc's residual guess** that `FUN_08028314` is "VFP" — it is `NVIC_SystemReset`
(`SCB_AIRCR SYSRESETREQ`); (c) it **disproves the `FPGA_BLOCKED` tag** via a zero-row
`peripheral_xrefs` census, so the row can be reclassed `RNA/VNA` (NA_OURS) and **removed
from the FPGA-blocked bucket** — a cleanup, not a hardware crack. **No execution contract
covers this address; nothing here is execution-verified; no V-promotion.** This does not
touch the FPGA secret — there is no FPGA here to touch.

---

## Verification (adversarial, 2026-06-13)

Independent adversarial re-check against the live ripcord `stock_v120` warehouse and
`build/contracts.sqlite`. Every load-bearing claim was re-run; nothing was rubber-stamped.

**Checks performed:**

1. **Crosswalk.** `functions WHERE addr<=134447216 AND addr+size>134447216` returns exactly
   `FUN_08038070, size=8`. The neighbor query returns `FUN_08038070 (8 B)` + `FUN_08038078
   (626 B)`. `0x08038070` genuinely lands in the claimed 8 B containing fn, with the 626 B body
   adjacent. **crosswalk_ok = true.**
2. **Thunk disasm.** `disasm.py --start 0x8038070`: `bl #0x802c314` + `movs r0,r0` ×2 = 8 B
   exactly. **Confirmed.**
3. **Reset-routine disasm.** `disasm.py --start 0x802c314`: `movw/movt r0=0xE000ED0C`,
   read-modify-write masking `0x700`, OR `0x05FA0004`, `str`, `dsb`, `b #0x802c334` self-loop.
   Textbook `NVIC_SystemReset`, no-return. The "VFP" and "GPIOD_BOP" labels are both refuted.
   **CORRECT claim upheld.**
4. **Body prologue.** `disasm.py --start 0x8038078`: `push {r4..fp,lr}; vpush {d8,d9};
   r5=0x200000F8; ldrb r0,[r5,#0x2d]`. Mechanically confirms device_mode read at state+0x2D and
   VFP use. The `movw/movt r7=0xAAAAAAAB` divide-by-3 magic at `0x08038092` independently
   corroborates the `&DAT_080465c8 % 3` LUT claim. **CONFIRMED.**
5. **peripheral_xrefs census.** `function_addr IN (134447216,134447224)` → **0 rows**. The
   `FPGA_BLOCKED` tag is unwarranted. **CONFIRMED.**
6. **Decompile spot-check (0x08038078).** Contains literal `080465c8` (LUT) **and** writes to
   `0x20000eb8..0x20000ed0` (state block). The §2 CONFIRM rows hold.
7. **Contract reality.** `contract_id = null` in the draft, so nothing is attributed for
   execution. The two contracts the note reasons *about*:
   - **#18** `[134392260, 134668495]` (`verified=1`) numerically spans `134447216`, but its
     evidence text is wholly about the 115638-byte SPI3 bitstream pump (`FUN_0802a9c4`,
     execution-verified 2026-05-30) and the over-wide `addr_end` is a self-described mis-cut
     estimate. It asserts **nothing** about `0x08038078`. The note's handling is correct.
   - **#19** `[134460500, 134467592]`. `134447216 < 134460500`, so the address is **below #19's
     range** and #19 does not cover it. The note's §3 phrasing ("covers 134460500..134467592 —
     does not include 134447216") is accurate. (The structured *draft's* proposed_V field had a
     muddled "ends below this addr" rationale; the prose note does not, so no body fix was
     required.)

**Overturned:** none. No CONFIRM was demoted; no V2 was claimed to demote. No hallucinated
contract id, address, or evidence string was found.

**Final approved D / R / V:** **D3 / RNA / VNA.** V is Not-Applicable (not a promotion): the
region is pure soft-float UI scaling with zero MMIO, no execution contract covers it, and the
clean-room intentionally does not mirror it (NA_OURS). The relabel
(`fs_close_helper` → reset-thunk + display-scale entry), the `NVIC_SystemReset` correction,
and the removal from the FPGA-blocked bucket all stand.
