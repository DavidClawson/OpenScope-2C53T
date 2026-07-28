# Reconcile: `scope_main_fsm` 0x08019e98 ↔ ripcord `FUN_0801de98` — structure clarified, NO new execution evidence

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`).
**Why this matters / bottom line up front:** `0x08019e98` was booked **D3/R1/V0,
FPGA_BLOCKED**, note *"oscilloscope acquisition spine, gated on DAT_20001060==2 …
dispatches to four sub-handlers …"*. Cross-walk confirms it is a real 13.3 KB standalone
function (no mis-cut) and **resolves the osc symbol `DAT_20001060` to `state_struct+0xf68`**,
tying this FSM into the same 0x200000F8 state struct the acquisition engine uses. But the
honest headline is: **no execution-verified contract covers this address, and ripcord adds
no new wire-level evidence for it.** The one substantive correction is taxonomic — this is
the **UI/render/state FSM (the display-loop spine), not the SPI3 wire-level acquisition
engine** (that is a *different* function, `FUN_0803B454`, contract #19). Calling it "the
acquisition spine" conflates two distinct subsystems. `new_evidence = false`.

---

## 0. Address crosswalk (`+0x4000`)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `scope_main_fsm @0x08019e98` | `FUN_0801de98 @0x0801de98` (size **13274 B**) |

`0x08019e98 + 0x4000 = 0x0801de98`, which is the **exact entry** of warehouse function
`FUN_0801de98` (134340248, 13274 B). **NOT a mis-cut** — osc carved this correctly as a
standalone function; it is not an inner region of a larger task. `was_miscut = false`.

Distinguish from the SPI3 engine: the wire-level acquisition task is `FUN_0803B454`
(runtime `0x0803B454`, contract #19, the 9-mode SPI3 burst/roll engine). `scope_main_fsm`
@`0x0801de98` does **not** touch SPI3/USART2 (see §2 peripheral census) — it is the
display/state machine that *consumes* what the acquisition task produced.

---

## 1. CORRECT — resolve the `DAT_20001060` gate to a state-struct offset; characterize the FSM

ripcord's disassembly of the prologue resolves the gate the osc note named as a bare
absolute (`DAT_20001060`) into the **shared 0x200000F8 state struct** (`sl = r10`):

```
0x0801DE98:  push.w {r4,r5,r6,r7,r8,sb,sl,fp,lr}
0x0801DE9E:  vpush  {d8..d15}                 ; this fn has a VFP body (trigger-DAC float math)
0x0801DEA4:  movw   sl, #0xf8
0x0801DEA8:  movt   sl, #0x2000               ; sl = 0x200000F8  → global state-struct base
0x0801DEAC:  ldrb.w r0, [sl, #0xf68]          ; state+0xf68 = 0x20001060
0x0801DEB0:  cmp    r0, #2
0x0801DEB2:  bne.w  #0x801f4d4                 ; gate: run body ONLY in scope mode (==2)
0x0801DEB6:  ldrb.w r0, [sl, #0x2e]           ; state+0x2e
0x0801DEBA:  ldrb.w r1, [sl, #0x17]           ; state+0x17
0x0801DEC6:  ldrb.w r0, [sl, #0x2d]           ; state+0x2d = range_index (same byte mode-1 uses)
0x0801DED0:  ldrb.w r1, [sl, #0xdb0]          ; state+0xdb0 = FSM sub-state byte (1→2→3 walk)
```

Resolution of the symbols the osc note carried as raw absolutes:

| osc symbol (note) | ripcord-resolved | meaning |
|---|---|---|
| `DAT_20001060` (scope-mode gate `==2`) | `state_struct + 0xf68` | scope-mode flag inside the 0x200000F8 struct |
| (FSM sub-state, note's "four sub-handlers" walk) | `state_struct + 0xdb0` | sub-state byte; code walks `1→2→3` (`strb #3` at 0x801DEE6, `strb #2` at 0x801DEFA — both write `[sl,#0xdb0]`) |
| `state+0x2d` | `0x20000125` | `range_index` — the **same** byte the acq-engine mode-1 range-gate reads (contract #19, Run 7) |
| `state+0x17`, `state+0x2e` | `0x2000010f`, `0x20000126` | scope sub-mode / enable selectors |

This is a real correction to the osc decode: `DAT_20001060` was an isolated label; it is in
fact field `+0xf68` of the same struct the acquisition engine, USART2 ISR, and bitstream
pump all share — so the scope-mode gate, the acq buffers (`+0x5B0/+0x9B0`), and the
range_index (`+0x2d`) are all one coherent state object. **All static-inferred** (disasm +
struct-base arithmetic), not execution-verified.

**Taxonomy correction (the load-bearing one):** the ledger note calls `0x08019e98` "the
oscilloscope acquisition spine." That conflates it with the SPI3 acquisition engine. The
peripheral census (§2) shows this function touches **DAC + GPIO + TMR, never SPI3/USART2**.
It is the **display/render + trigger-comparator FSM** that runs once per frame in scope
mode; the bulk-sample wire engine is the separate `FUN_0803B454`. Re-word the note: "scope
display/state FSM (frame spine)", reserving "acquisition engine" for `0x0803B454`.

---

## 2. CONFIRM — static-only agreement (NO V-promotion; no contract covers this addr)

Peripheral census for `FUN_0801de98` (`peripheral_xrefs`, address-keyed):

| peripheral | register | refs | reading |
|---|---|---|---|
| DAC | `D1DTH12R` (DHR12R1) W/R | 9/9 | **trigger-comparator DAC** level write (PA4, software-triggered) |
| DAC | `SWTRG` W/R | 9/9 | software trigger after each level update |
| TMR13 | `C1DT` W | 9 | timer compare (timebase / PWM-ish) |
| GPIOC | `SCR`/`CLR`/`IDT` | 9/7/2 | discrete output strobes + 2 input polls |
| GPIOD | `SCR` | 3 | discrete output strobe |

These agree with osc's static decode that the FSM drives the **trigger-level comparator
DAC** and GPIO. They corroborate the clean-room `drivers/scope_trigger.c`, which writes the
**exact same DAC registers** (`DAC_DHR12R1 0x40007408`, `DAC_SWTRG 0x40007404`) and cites
"Stock writes these exact addresses in FUN_080018a4." So the DAC trigger path is doubly
attested by static decode on both sides.

**But there is NO execution evidence for this address.** Query of `build/contracts.sqlite`
for any contract with `addr_start ≤ 134340248 < addr_end` returns **empty**. The verified
contracts (#1 memset, #12 RAMRD, #13 FB-blit, #18 bitstream, #19 acq-engine) all cover
*other* address ranges; #19 covers `[134460500, 134467592)` = `0x0803B454…`, which is a
different function entirely. Therefore:

- **`contract_id = null`, `contract_verified = false`.**
- DAC/GPIO/TMR access here stays **V1 at most** (two independent *static* decodes agree —
  osc + clean-room `scope_trigger.c` — but neither is execution).
- **No V0→V2 promotion is warranted for this address.** The osc note's V0 is correct as-is.

(Note on peripheral-census reliability: this function's accesses are address-literal DAC/
GPIO writes, so unlike the SPI3 engine — whose `movw/movt`-built `0x40003C00` never appeared
in the address census, contract #16 — the DAC/GPIO/TMR census here is trustworthy. The
*absence* of SPI3/USART2 is real for this function, not an extraction artifact.)

---

## 3. GAP — R-side divergences (`firmware/src`)

The stock 13.3 KB monolithic FSM is decomposed on the R side across several files; the
divergences are reimplementation-completeness items, not blockers:

1. **The stock single 13.3 KB scope-mode FSM is split into ~4 R-side units with no single
   spine.** Stock `scope_main_fsm` is one gated (`state+0xf68==2`) function that walks a
   `state+0xdb0` sub-state and inlines render + trigger-DAC + GPIO strobes. The R side
   spreads this across `ui/scope_ui.c:draw_scope_screen()` (render), `drivers/fpga.c:
   fpga_acquisition_task()` (SPI3 reads), `ui/scope_state.c` (state), and the `main.c`
   display loop (`scope_ui.c:389` comment: "SPI3 acquisition triggers are now fired from
   main.c display loop"). There is **no single function that reproduces the stock gate +
   `+0xdb0` sub-state walk**. If byte/behavior parity with stock's frame cadence is wanted,
   the `state+0xdb0` `1→2→3` sub-state progression (stock 0x801DED0–0x801DF0C) is currently
   unmodeled.

2. **`state+0xdb0` sub-state FSM is not represented in `scope_state.c`.** The R-side
   `scope_state_t` (ui/scope_state.c) models user settings (vdiv, timebase, trigger mode/
   edge/source, coupling, probe) but not this per-frame acquisition-arming sub-state byte.
   The closest analog is the `fpga.stock_shadow` FSM in `drivers/fpga.c`
   (`visible_state/substate/e1c/phase`), which models a *different* (diag/handshake) state
   group, explicitly hedged as "We do not claim this is exact stock control flow." The
   `+0xdb0` arming byte (`subs r2, r1, #1; cmp r2, #1` two-state debounce at 0x801DED8) is a
   concrete item to port.

3. **Trigger-comparator DAC: range/level cal is placeholder, not stock.**
   `drivers/scope_trigger.c` faithfully reproduces the DAC register sequence (DHR12R1 +
   SWTRG, the exact registers this FSM hits 9× each) and the stock divisor `200.0f`, but its
   `cal_base[]/cal_upper[]` tables are full-scale placeholders ("factory cal unrecoverable").
   The stock FSM's DAC writes here are driven by the real per-range cal — amplitude/trigger-
   level will be wrong vs stock until real cal is regenerated. (This is a cal-data gap, not a
   code-path gap; the code path matches.)

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08019e98` | name | `scope_main_fsm` | keep, but note clarifies "scope **display/state** FSM (frame spine), NOT the SPI3 acquisition engine (that = 0x0803b454)" | §1 |
| `08019e98` | note | "acquisition spine, gated on DAT_20001060==2 …" | append: "ripcord crosswalk = FUN_0801de98 (13274 B, NOT a mis-cut). DAT_20001060 resolves to state_struct+0xf68 (0x200000F8 base); FSM sub-state = state+0xdb0 (1→2→3 walk); range_index = state+0x2d (shared w/ acq-engine). Peripheral census: DAC(trigger DAC)+GPIOC/D+TMR13, NO SPI3/USART2 — confirms this is display/trigger FSM, not the wire engine. NO contract covers this addr." | §1,§2 |
| `08019e98` | V | V0 | **V0 (unchanged)** — no execution-verified contract covers this address; DAC/GPIO agreement is static-only (osc + clean-room scope_trigger.c), V1 ceiling | §2 |
| `08019e98` | D | D3 | **D3 (unchanged)** | — |
| `08019e98` | R | R1 | **R1 (unchanged)** — R-side exists but split across 4 files w/ no single-spine equivalent + cal placeholder (§3) | §3 |

---

## 5. Honest scope

`new_evidence = false`. ripcord does **not** add execution evidence for `0x08019e98` — no
contract covers it, and the Renode oracle has not run this function. What the crosswalk
*does* deliver, all static-inferred: (a) confirms it is a real standalone 13.3 KB function,
not a mis-cut; (b) resolves the osc symbol `DAT_20001060` → `state_struct+0xf68` and the
FSM sub-state → `state+0xdb0`, unifying it with the shared 0x200000F8 struct; (c) one real
taxonomic correction — this is the **display/trigger-DAC frame FSM**, not the "acquisition
spine" (that is the separate `0x0803B454`, contract #19). The peripheral census (DAC/GPIO/
TMR, no SPI3/USART2) is trustworthy here because the accesses are address-literal, so the
absence of SPI3 is a real fact about this function, not the movw/movt census blind spot that
hid the SPI3 engine elsewhere. The V0 booking stands; do not promote. The genuine
deliverable is the GAP list: the `state+0xdb0` sub-state walk is unmodeled on the R side, and
the trigger-DAC cal tables are placeholders.

---

## Verification (adversarial, 2026-06-13)

Independent skeptical re-check of this note, on the same default-to-overturn discipline the
osc campaign applied (19/52 claims overturned there). Result: **no claim overturned; V0
upheld; one cosmetic address/value pairing corrected.**

**Checks run (cwd = ripcord):**

1. **CROSSWALK — PASS.** `scripts/query "...functions WHERE source='stock_v120' AND addr<=134340248
   AND addr+size>134340248"` returns exactly one row: `FUN_0801de98 @0x801de98, size 13274`.
   `0x08019e98+0x4000 = 0x0801de98` lands on the entry, not an interior. `was_miscut=false`
   upheld. `crosswalk_ok=true`.

2. **CONTRACT REALITY — PASS (nothing to hallucinate).** Draft cites `contract_id=null`.
   Verified directly: `SELECT ... FROM contracts WHERE addr_start<=134340248 AND
   addr_end>134340248` returns **zero rows** — no contract covers this address.
   `contract_verified=false` is correct. The note's cross-reference to **contract #19** is
   accurate, not misattributed: #19 is `verified=1`, covers `[0x803b454, 0x803d008)` (a
   *different* function), and its evidence text genuinely asserts `range_index=state+0x2D`
   (Run 7 execution-verified), exactly as cited here. No hallucinated id/address/evidence.

3. **V-PROMOTION — PASS (nothing promoted).** The note promotes no claim to V2; it holds V0
   with an explicit V1 static ceiling for the DAC/GPIO agreement (osc decode + clean-room
   `scope_trigger.c`, both static). Since no execution-verified contract covers `0x0801de98`,
   V1 is the correct ceiling and **V0 (unchanged) is the defensible final verdict.** No
   demotions needed because no over-promotion was attempted.

4. **DISASM SPOT-CHECK — PASS** (`scripts/analysis/disasm.py --target stock_v120 --start
   0x0801de98 --end 0x0801df10`). Every load-bearing instruction claim verified at the cited
   address:
   - `movw sl,#0xf8 / movt sl,#0x2000` → `sl = 0x200000F8` (state-struct base). ✓
   - `ldrb.w r0,[sl,#0xf68]; cmp r0,#2; bne.w` → gate at `state+0xf68` (=0x20001060) `==2`.
     Confirms `DAT_20001060 → state+0xf68`. ✓
   - `ldrb.w r0,[sl,#0x2d]; cmp r0,#0x13` → `range_index` read at `state+0x2d`; the `#0x13`
     bound is the same clamp domain as contract #19's `idx<=0x12`. ✓
   - sub-state walk on `state+0xdb0`: `strb.w #3 @0x801DEE6` and `strb.w #2 @0x801DEFA`. ✓

**Single correction (cosmetic, not an overturn):** the draft body originally paired the
sub-state stores as "`strb #2`/`#3` at 0x801DEE6/0x801DEFA", which inverts the value↔address
mapping. Disasm shows `#3` is stored at 0x801DEE6 and `#2` at 0x801DEFA. Both stores exist,
both target `[sl,#0xdb0]`, and the `1→2→3` progression is real — corrected inline in §1 so the
note no longer overstates the precise ordering.

**Final approved booking: D3 / R1 / V0** — unchanged from the draft. All three are defensible:
crosswalk solid, no contract covers the address, static-only agreement caps the DAC/GPIO claim
at V1 and leaves the address at V0.
