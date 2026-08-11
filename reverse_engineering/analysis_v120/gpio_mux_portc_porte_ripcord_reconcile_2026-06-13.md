# Reconcile: `gpio_mux_portc_porte` 0x080018a4 ↔ ripcord `FUN_080058a4` — corrected port label, not FPGA-blocked

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`), its SVD-resolved
`peripheral_xrefs`, the loaded binary literal at `0x08001a54`, and the
execution-verified contract ledger (`build/contracts.sqlite`).
**Why this matters:** `0x080018a4` was booked in `coverage_ledger.csv` as
**D3/R1/V0, FPGA_BLOCKED** with the note *"writes GPIO BSRR/CRH-class registers in the
0x4001101x/0x4001181x (**GPIOD/E**) block … relay/range routing … needs live ADC stream
to verify."* ripcord **corrects the port label** (it is **GPIOC**+GPIOE, not GPIOD) and
**kills the FPGA_BLOCKED verdict**: this function touches no SPI3 / USART2 / DMA at all —
it is pure analog-frontend control (range relays + a per-range trigger-comparator DAC on
PA4) and is therefore **bench-verifiable on PA4 without a working FPGA/acquisition chain**.
No execution contract covers it yet, so the read-structure stays **V1** (static, two
independent methods agree), not V2.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `gpio_mux_portc_porte @0x080018a4` | `FUN_080058a4 @0x080058a4` |

`0x080018a4 + 0x4000 = 0x080058a4`, which is the **entry** of ripcord `FUN_080058a4`
(size 422 B). **Not a mis-cut** — osc and ripcord carved the same function body. Rule
holds: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

**No verified contract covers `0x080058a4`.** Nearest verified contracts in the ledger are
`memset` (`…end 0x8005300`) and `xmc_lcd_ili9341_ramrd` (`0x80067e8…`); neither overlaps.
`contract_id = null`, so nothing here is execution-verified. Everything below is
**static-inferred** — but the static decode is now SVD-resolved and unambiguous.

---

## 1. CORRECT — two ledger-note errors fixed by ripcord

**(a) Port label: GPIO*C*, not GPIOD.** ripcord's SVD-resolved `peripheral_xrefs` for this
function are unambiguous:

| stock write | ripcord SVD | meaning |
|---|---|---|
| `_DAT_40011010` / `_DAT_40011014` | **GPIOC** SCR (0x10) / CLR (0x14) | range-relay set/clear, port C |
| `_DAT_40011810` / `_DAT_40011814` | **GPIOE** SCR (0x10) / CLR (0x14) | range-relay set/clear, port E |

AT32F403A bases (from `AT32F403Axx_v2.svd`): GPIOC `0x40011000`, GPIO**D** `0x40011400`
(**never touched here**), GPIOE `0x40011800`. The ledger note's "GPIOD/E" is wrong on the
C port; the function *name* `gpio_mux_portc_porte` was already right. Also note these are
**SCR/CLR** (atomic set/clear) writes, not "BSRR/CRH-class" — no CRH/CRL mode-config write
happens in this function (the note's "CRH-class" is inaccurate).

**(b) Not FPGA-blocked.** This function has **zero** SPI3 / USART2 / DMA / FPGA-interface
accesses. Its only peripherals are GPIOC, GPIOE, and **DAC** (`0x40007400` block). The
"needs live ADC stream to verify" framing is misapplied — the verifiable output is the
DAC1 voltage on **PA4**, observable on a scope probe with no FPGA in the loop.

The DAC tail (after the relay switch), fully resolved:

```
_DAT_40007408 = (_DAT_40007408 & 0xfffff000) | (uVar4 & 0xfff);  // DAC D1DTH12R, write low 12b
_DAT_40007404 = _DAT_40007404 | 1;                                // DAC SWTRG, D1SWTRG pulse
```

`0x40007408` = DAC `D1DTH12R` (offset 0x08, CH1 12-bit right-aligned data);
`0x40007404` = DAC `SWTRG` (offset 0x04, `D1SWTRG`). Confirmed against the SVD register map
and against `peripheral_xrefs` (DAC D1DTH12R READ+WRITE, SWTRG WRITE).

The 12-bit code is computed by a VFP path from **per-range RAM cal tables**:

```
val   = ((float)(*puVar1 - *puVar2) / DAT_08005a54) * (float)(DAT_200000fc + 100)
        + (float)*puVar2;
uVar4 = (uint)val;            // VectorFloatToUnsigned, round-to-nearest
```

I read the divisor literal directly out of the ripcord-loaded binary:
`runtime 0x08005a54` (= osc `0x08001a54`) = bytes `00 00 48 43` = **`200.0f`**. This
independently confirms the constant the R-side already extracted (`scope_trigger.h`).

---

## 2. CONFIRM — osc static decode and ripcord agree (V1 — no contract, NOT V2)

Both your decode and ripcord's SVD-resolved decode assert the same structure. Two
independent **static** methods agreeing is solid, but neither is execution — so this is
**V1**, not V2. Do not promote to V2 without a Renode contract that runs this routine.

| claim | osc decode | ripcord (SVD + literal) | tier |
|---|---|---|---|
| 10-way per-range switch (cases 0–9) | `switch(param_1) 0..9` | `cmp r0,#9; tbb [pc,r0]`, 10 entries | static-inferred |
| relay routing via GPIOC SCR/CLR + GPIOE SCR/CLR | "GPIO 0x4001101x/0x4001181x" | GPIOC `0x10/0x14`, GPIOE `0x10/0x14` | static-inferred |
| per-range trigger DAC = D1DTH12R write + SWTRG pulse | `0x40007408` + `0x40007404\|=1` | DAC D1DTH12R + SWTRG, SVD-confirmed | static-inferred |
| divisor constant = `200.0f` @ osc `0x08001a54` | `/ DAT_08001a54` | binary literal `00004843` = 200.0f | static-inferred |
| compute is VFP float `(Δtable / 200) * gain + base` | `(fVar3/div)*fVar5 + fVar6` | same P-code (VectorSignedToFloat/…) | static-inferred |

**Ledger action:** keep D3. Read-structure V0 → **V1** (two static methods concur).
Reserve V2 for a future `execution-verify` run that drives PA4.

---

## 3. GAP — R-side corrections (`drivers/scope_trigger.c`, `drivers/fpga.c`)

The clean-room split this function across **two** R-side files. Both are close; three
concrete divergences:

1. **Gain term is a RAM state global, not the function argument.** `scope_trigger.c`
   computes `((upper-base)/200)*(level+100)+base` where `level` is the user trigger level
   `[-100..100]` passed in. Stock multiplies by `(DAT_200000fc + 100)` — `DAT_200000fc` is
   a **16-bit RAM state value** (read live each call), not the call's `param_1`. `param_1`
   in stock is the **range index** that selects the relay case *and* indexes the cal tables;
   the gain comes from state `0x200000fc`. The R-side conflates "range" and "level" plumbing
   relative to stock. Re-route: range → switch+table-index (param), level/gain → state read.

2. **Cal tables are per-range RAM pairs with a mode discriminator, not static linear.**
   Stock selects one of **three** table-pairs by runtime mode
   (`DAT_20000125 < 5`, and within that `DAT_20000125==4 || DAT_2000010c==3`):
   - tables at `0x200003a8`/`0x2000036c`, or `0x200003bc`/`0x20000380`, or
     `0x20000394`/`0x20000358` (each indexed `+ param_1*2`, i.e. `uint16` per range).
   The DAC numerator is `*puVar1 - *puVar2` (a **difference** of two RAM tables) and the
   additive base is `*puVar2`. `scope_trigger.c`'s `cal_base[]=0 / cal_upper[]=4095`
   single static table is a documented placeholder and a *structural* simplification: it
   drops both the difference-of-two-tables form and the 3-way mode selection. Faithful
   model: two RAM cal arrays per mode, `code = ((t1[r]-t2[r])/200)*(state_fc+100) + t2[r]`.

3. **Relay truth table: stock touches only GPIOC+GPIOE here; R-side adds GPIOA/GPIOB and an
   invented bit pattern.** `fpga_set_scope_frontend_range()` writes GPIOA PA15/PA10/PA6 and
   GPIOB PB10/PB9 in addition to GPIOC PC12 / GPIOE PE4-6, and its per-case bit values are
   explicitly "intentionally simple … reconstructed truth table," not extracted. Stock
   `FUN_080058a4` writes **only** GPIOC (one CLR/SCR of `0xf800`/`0xf804`-shaped masks +
   `0x40011014`) and GPIOE (`0x40011810/0x40011814` with values `0x10/0x20/0x40`). The
   exact per-range GPIOC/GPIOE bit pattern should be transcribed from the ripcord disasm
   (cases laid out at `0x080058b8…0x080059b6`), and the GPIOA/GPIOB writes that R-side adds
   are **not part of this function** — they belong to a different mux routine
   (`gpio_mux_porta_portb`) and should not be merged in here.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `080018a4` | note (port label) | "0x4001101x/0x4001181x (GPIOD/E) … BSRR/CRH-class" | "GPIO**C** SCR/CLR (0x40011010/14) + GPIOE SCR/CLR (0x40011810/14); SCR/CLR atomic writes, no CRH config" | §1a |
| `080018a4` | klass | FPGA_BLOCKED "needs live ADC stream" | **NOT FPGA-blocked** — no SPI3/USART2/DMA; output is DAC1 on PA4, scope-verifiable standalone | §1b |
| `080018a4` | note (DAC) | (implicit) | per-range trigger-comparator DAC: D1DTH12R(0x40007408)+SWTRG(0x40007404\|=1), divisor=200.0f@0x08001a54 (binary-confirmed), gain=state[0x200000fc]+100, 3-way RAM cal tables | §1 |
| `080018a4` | V (read structure) | V0 | **V1** (two static methods agree: osc decode + ripcord SVD/literal) — NOT V2, no contract | §2 |

R-side TODOs (numerator-growth, not blockers): (1) gain from state `0x200000fc`, not the
arg; (2) difference-of-two-RAM-tables cal with 3-way mode select; (3) transcribe exact
GPIOC/GPIOE per-range bits and drop the GPIOA/GPIOB writes that belong to the porta_portb mux.

---

## 5. Honest scope

`new_evidence = true`, but it is **correction**, not promotion. ripcord did not execute
this routine — there is no covering contract, so nothing moves to V2. What ripcord adds
over the existing osc ledger row: (a) fixes the port label (GPIOC, not GPIOD) via the
SVD; (b) overturns the FPGA_BLOCKED verdict — this is a self-contained analog-frontend +
PA4-DAC routine with no FPGA dependency, so it can be bench-verified now; (c)
binary-confirms the `200.0f` divisor the R-side guessed; (d) hands `scope_trigger.c` /
`fpga.c` three concrete faithfulness fixes (state-gain vs arg, RAM difference-table cal,
relay-bit transcription). The factory cal **values** remain unrecoverable (saved_config
overwritten — see `w25q128_flash_map_2026-06-13.md`); that is a data gap, independent of
this decode, and unchanged here.

---

## Verification (adversarial, 2026-06-13)

Independent adversarial re-check against the ripcord warehouse. Default posture:
overturn anything not solidly backed. Result: **nothing overturned** — the note was
already V1-disciplined and made no V2 / contract claims to puncture.

**Checks performed:**

1. **Crosswalk — PASS.** `functions WHERE source='stock_v120' AND addr<=134240420 AND
   addr+size>134240420` returns exactly `FUN_080058a4 @0x80058a4, size 422`. Runtime
   `0x080058a4` is the function *entry* (self), not a mis-cut interior. The
   `osc + 0x4000` rule holds. `crosswalk_ok = true`.

2. **Contract reality — N/A, no hallucination.** Draft cites `contract_id = null` /
   `contract_verified = false`. Ledger (`build/contracts.sqlite`, 19 rows) confirms **no
   contract** has `addr_start ≤ 134240420 < addr_end`; nearest are `memset` (ends
   `0x8005300`) and `xmc_lcd_ili9341_ramrd` (`0x80067e8…`). Nothing is falsely attributed
   to a contract. `hallucination_found = false`.

3. **V-promotion — none to demote.** The note scores the read-structure **V1** and
   explicitly forbids V2 absent a Renode run on PA4. No claim is dressed as
   execution-verified. Defensible final V = **V1**.

4. **CONFIRM spot-checks (disasm + binary + SVD), all PASS:**
   - Dispatch: `cmp r0,#9; bhi.w; tbb [pc,r0]` at `0x080058a4–aa` — genuine 10-way (0–9)
     table-branch. Confirmed.
   - Ports: case bodies `0x080058b8…0x080059b6` reach GPIOE via `movw r2,#0x1810;
     movt r2,#0x4001` (= `0x40011810`) and GPIOC via `movw r1,#0xf800/0xf804;
     movt r1,#0xffff; str r3,[r2,r1]` — i.e. base+(-0x800)= `0x40011010` (GPIOC SCR),
     base+(-0x7fc)= `0x40011014` (GPIOC CLR). SVD `peripheral_xrefs` for this function
     list **only** GPIOC SCR/CLR, GPIOE SCR/CLR, DAC D1DTH12R, DAC SWTRG — zero
     SPI3/USART2/DMA/FPGA. The GPIOD base `0x40011400` is **never** touched. Both CORRECTS
     (GPIOC-not-GPIOD; not-FPGA-blocked) hold.
   - DAC tail: `peripheral_xrefs` confirm DAC `D1DTH12R` (0x40007408, READ+WRITE) and
     `SWTRG` (0x40007404, WRITE). Holds.
   - Divisor literal: read 4 bytes at file offset `0x1a54` (vaddr `0x08005a54`, base
     `0x08004000`) directly from `targets/stock_v120/stock_v120.bin` → `00 00 48 43` →
     IEEE-754 LE = **200.0** exactly. Binary-confirmed, deterministic read (NOT execution),
     exactly as labeled.

**One phrasing nit (not overturned, noted for precision):** §3.3 calls the GPIOC accessor
`0xf800`/`0xf804`-shaped *masks*. They are not bit masks — they are the sign-extended
negative *address offsets* (`0xfffff800`/`0xfffff7fc`) used in `str r3,[r2,r1]` to reach
GPIOC from the GPIOE base register. The resolved addresses (`0x40011010`/`0x40011014`) and
the conclusion are correct; only the word "masks" is loose. Left as-is; does not change any
claim or score.

**Final approved ledger position:** **D3 / R1 / V1**, FPGA_BLOCKED verdict killed,
port-label correction (GPIOC, not GPIOD) upheld. No claim overstated.
