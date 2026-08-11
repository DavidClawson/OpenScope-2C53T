# Reconcile: `FUN_08022d40` 0x08022d40 ↔ ripcord `FUN_08026d40` — role CORRECTED (DAC-ch2/TMR7 siggen rate, not FPGA)

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`).
**Why this matters:** `08022d40` is booked in `coverage_ledger.csv` as **D3/R0/V0,
FPGA_BLOCKED**, with a (self-)corrected note claiming it is a *"float-classifier + per-cell
uint16 counter updater on the scope/meter state struct … index base 0x200000F8 + r7*120 +
(fp&15)*2 … tail-branches into master-init … Ties to scope acquisition path → FPGA_BLOCKED."*
The ripcord clean decode shows **both** the original Ghidra "LCD window" guess **and** osc's
own corrected note are about a different routine. This function is the **DAC-channel-2 /
TMR7 signal-generator rate-configuration** routine — an OUTPUT path with **zero FPGA
involvement**. The correction unblocks the row from FPGA_BLOCKED and hands `dac_output.c`
a concrete second-channel gap.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `FUN_08022d40 @0x08022d40` | `FUN_08026d40 @0x08026d40` (size 210 B) |

`0x08022d40 + 0x4000 = 0x08026d40`. ripcord's `functions` row for `0x8026d40` is a
**standalone 210-byte function** whose entry **equals** the runtime addr. **Not mis-cut**
— osc carved the boundary correctly. The prologue is present and self-contained
(`push {r4,r5,r7,lr}; vpush {d8}; sub sp,#0x18`), through a clean `pop {r4,r5,r7,pc}` at
`0x8026e10`; it does **not** tail-branch into master-init. Rule confirmed:
`ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — the role is wrong twice; ripcord resolves it to DAC-ch2/TMR7

The osc note's corrected description (float classifier, `base 0x200000F8 + r7*120`, u16
counters at `+0x288/+0x2C4`, tail-branch into `0x80237e0`) **does not match this address**
— it describes a different function. The actual `FUN_08026d40` body, with `peripheral_xrefs`
resolved by ripcord's register map, is unambiguous:

```
0x8026D48: movw r1,#0xf8 / movt r1,#0x2000     ; r1 = 0x200000F8  state-struct base
0x8026D50: ldr  r0,[r1,#0xe5c]                  ; r0 = state+0xE5C  (siggen sample count)
0x8026D54: movw r4,#0x1400 / movt r4,#0x4000    ; r4 = 0x40001400  TMR7 base
0x8026D5E: ldrb r1,[r1,#0xe58]                  ; r1 = state+0xE58  (siggen enable flag, u8)
0x8026D5C/D62: cbz ... -> 0x8026dd6             ; if count==0 OR enable==0 -> DISABLE branch
0x8026D64: movs r1,#0xc8 / mul r5,r0,r1         ; r5 = count * 200
0x8026D7C: vcvt.f32.u32 s16,s0                  ; period_f
0x8026D80: bl 0x802e430                         ; helper -> local_2c[0] (numerator)
0x8026D8E: str r0,[r4]                          ; TMR7_CTRL1 &= ~1   (stop timer)
0x8026D9C: cmp r5,#0xfa0 / it lo / vmovlo s16,s2; if (count*200 < 4000) period_f *= 10.0
0x8026DB4: movlo r0,#0x13 / str r0,[r4,#0x2c]   ; TMR7+0x2C = 1, or 0x13 in slow tier
0x8026DA6: vdiv s0,s0,s16                        ; ratio = numerator_f / period_f
0x8026DB8: vadd s0,s0,#-1.0 / vcvt.u32.f32       ; reload = ratio - 1.0
0x8026DC0: vstr s0,[r4,#0x28]                    ; TMR7+0x28 = reload value
0x8026DC4: orr [r4,#0x14],#1                     ; TMR7+0x14 |= 1  (SWEVT ovf re-load)
0x8026DD2: orr [r4],#1                            ; TMR7_CTRL1 |= 1 (start timer)
```

DISABLE branch (`0x8026dd6`): clears TMR7_CTRL1 bit0, then walks DAC base `0x40007400`:
clears `DAC+0x14` low 12 bits (`d2dth12r`, DAC ch2 data), sets `DAC+0x00` bits `0x380000`
(ch2 trigger-source field), sets `DAC+0x04` bit1 (`SWTRG` ch2), `bfi [r4],#0x13,#3`. This is
**tear-down of DAC channel 2 + TMR7**.

`peripheral_xrefs` for `function_addr=134376768` corroborate exactly this and **nothing else**:

| peripheral | registers touched | note |
|---|---|---|
| **TMR7** (0x40001400) | CTRL1, SWEVT, PR, DIV | timer rate/enable |
| **DAC** (0x40007400) | CTRL, SWTRG, D2DTH12R | **channel-2** data + trigger |

No SPI3, no USART2, no GPIOB/PB6, no DMA-in, no XMC. Resolution table:

| osc symbol / claim | actual value | meaning |
|---|---|---|
| `r4` (Ghidra: LCD window) | `0x40001400` | **TMR7 base** (APB1+0x1400, confirmed vs at32f403a_407.h) |
| "base 0x200000F8 + r7*120 …" | `state+0xE5C` / `state+0xE58` | siggen **sample count** + **enable flag** (scalars, not a per-cell counter array) |
| "tail-branch into master-init" | (none) | function returns cleanly at `0x8026e10` |
| `s16`/`s2` float body | period scaling | `period = count*200`, `×10` slow-tier, reload `= num/period − 1` |
| `0x40007400` block | DAC ch2 | `D2DTH12R`=`+0x14`, trigger-source field `+0x00[21:19]`, `SWTRG`=`+0x04` |

**Ledger action:** klass `FPGA_BLOCKED` → **wrong classification**; role corrected to
`siggen_dac2_tmr7_rate_config` (an OUTPUT path). No FPGA dependency exists, so the row is
not blocked on the bench at all.

---

## 2. CONFIRM — execution evidence

**No verified contract covers this address.** Scanning `contracts.sqlite` for an entry whose
`[addr_start, addr_end)` brackets `134376768`: the nearest are #5 `dma1ch2_lcd_blit`
(starts 134256240, LCD, unrelated) and #18 `fpga_bitstream_upload` (starts 134392260, above
this addr). **`contract_id = null`, `contract_verified = false`.**

The DAC-ch2 *output* family is touched by contracts but **none execution-verifies this
specific routine**:
- #7 `dma2ch4_dac_siggen` (conf 0.9, **verified blank**): "DMA2-Ch4 drives DAC ch2
  (C4PADDR=0x40007414=DHR12R2) from a circular SRAM LUT — the built-in signal/cal generator;
  an output." That is the **DMA half** of the same siggen; `FUN_08026d40` is the **TMR7-rate
  / enable-gate half**. Static agreement on the subsystem (DAC ch2 = siggen output), but #7
  is not `verified=1`, so this is **V1 static-corroborated**, not V2.
- #8 `exti3_isr_dac_awg` (conf 0.8, verified blank): the AWG point-update ISR. Same siggen
  family, also not execution-verified.

So every claim here is **static-inferred**. There is **no V0→V2 promotion** available: no
`verified=1` contract asserts any sub-claim of this function. Honest tier: the role
correction and register decode are **V1 (two-method static agreement: ripcord decode +
register map + osc, plus subsystem-consistent with unverified contracts #7/#8)**.

---

## 3. GAP — R-side corrections for `firmware/src/drivers/dac_output.c`

The clean-room `dac_output.c` implements **only DAC channel 1** (PA4, `d1dth12r`) driven by
**TMR6** + **DMA2_CH3**. Stock `FUN_08026d40` configures **DAC channel 2** (`d2dth12r` at
`0x40007414`) driven by **TMR7** + DMA2_CH4 (the #7 path). The second channel is entirely
absent from firmware/src. Concrete reimplementation items:

1. **Add a second DAC output channel (ch2 / TMR7 / DMA2_CH4).** `dac_output.c` hardcodes
   `DMA2_CH3->paddr = &DAC->d1dth12r` and `TMR6`. Stock runs a *parallel* ch2 generator on
   TMR7→DAC2. There is **no TMR7 reference anywhere in firmware/src** (`grep -rn TMR7
   firmware/src` is empty). Faithful behavior needs the ch2 timer+DMA path added.

2. **Rate math differs.** R-side `tmr6_set_rate` uses fixed prescaler tiers
   (`period = APB1/rate`, escalate prescaler 1→8→64 when `period>65536`). Stock uses a
   **float ratio with a single ×10 slow-tier switch at count·200 < 4000**:
   `reload = helper(count) / (count*200 [×10 if slow]) − 1`, and writes a tier flag (1 vs
   0x13) to `TMR7+0x2C`. The exact `count*200` mapping and the `FUN_0802e430` numerator
   (project `0x0802a430`) are stock-specific; reproduce the tier boundary and the `−1.0`
   reload convention.

3. **Enable gating is state-driven, not a `running` bool.** Stock gates the whole routine on
   `state+0xE5C` (count) **and** `state+0xE58` (enable u8); when either is zero it tears down
   TMR7 **and** DAC ch2 (`d2dth12r` cleared, trigger field set, `SWTRG` bit). R-side
   `dac_output_stop` is the ch1 analogue; mirror it for ch2 keyed on the siggen-enable state
   bytes. (`siggen_ui.c` already computes a `preview_rate = frequency_hz*280/2.5` — the UI
   side exists; the ch2 driver binding does not.)

These are numerator-growth items for the **siggen/AWG output subsystem**, not the FPGA
acquisition path.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `08022d40` | klass | `FPGA_BLOCKED` | `siggen_dac2_tmr7` (OUTPUT path; no FPGA dependency) | §1, §3 |
| `08022d40` | note | "float-classifier + per-cell uint16 counter … 0x200000F8+r7*120 … tail-branch master-init … FPGA_BLOCKED" | "ripcord FUN_08026d40 (210B, not mis-cut): DAC-ch2/TMR7 signal-generator RATE config. r4=TMR7 0x40001400; gates on state+0xE5C count & state+0xE58 enable; period=count*200 (×10 if <4000), reload=helper/period−1 → TMR7+0x28, tier flag TMR7+0x2C; else-branch tears down TMR7+DAC ch2 (d2dth12r/SWTRG). peripheral_xrefs: TMR7+DAC only, no SPI3/USART2/GPIO. Sibling of contracts #7 (DMA2-Ch4→DAC2) / #8 (EXTI3 AWG). See FUN_08022d40_ripcord_reconcile" | §1 |
| `08022d40` | R | R0 | **R1** (clean-room ch1 analogue exists in dac_output.c; ch2/TMR7 path is the named GAP) | §3 |
| `08022d40` | V | V0 | **V1** (two-method static agreement; NO verified contract covers this addr — stays V1, not V2) | §2 |
| `08022d40` | D | D3 | D3 (keep) | — |

**Honest scope (new_evidence = true):** ripcord adds real, non-trivial evidence here — it
**reclassifies** the row out of the FPGA bucket entirely. The corrected role (DAC-ch2/TMR7
siggen rate) is **static-inferred (V1)**: it rests on ripcord's clean decode + register-map
resolution + subsystem consistency with contracts #7/#8, none of which is `verified=1` for
this address. **No execution verification applies** — there is no Renode trace through
`0x8026d40`, and no contract brackets it (`contract_id=null`). Do **not** promote to V2.
The win is qualitative and correct: this was never FPGA code, so it was never bench-blocked;
it is an output-side siggen routine whose second-channel reimplementation is now a concrete
`dac_output.c` task.

---

## 5. Recipe (per-function template)

1. **crosswalk** osc addr `+0x4000` → ripcord runtime addr; `functions` where
   `addr ≤ X < addr+size`. Here entry == X → **not mis-cut** (osc boundary correct).
2. **clean decode**: `disasm.py --start 0x8026d40` + `decompiled_c` + `peripheral_xrefs`.
   peripheral_xrefs alone (TMR7+DAC, no FPGA periphs) refuted the FPGA_BLOCKED tag.
3. **execution evidence**: `contracts.sqlite` — no `[addr_start,addr_end)` covers
   `134376768` → contract_id null → claims cap at V1.
4. **diff vs R-side**: `dac_output.c` implements DAC ch1/TMR6 only; ch2/TMR7 is the GAP.
5. **score + write**: D3 keep, R0→R1, V0→V1, klass FPGA_BLOCKED→siggen_dac2_tmr7.

---

## Verification (adversarial, 2026-06-13)

Independent adversarial re-check against the ripcord warehouse and contract ledger.
Default-to-skepticism pass; goal was to overturn anything not solidly backed.

**Checked:**

1. **Crosswalk.** `functions WHERE addr ≤ 134376768 < addr+size` (stock_v120) →
   single hit `FUN_08026d40 @0x8026d40`, size 210. Runtime addr equals the function
   entry, so 0x8026d40 genuinely lands in the claimed containing fn (the function
   *is* itself). **crosswalk_ok = true. Not mis-cut — confirmed.**

2. **Contract reality.** `contracts.sqlite` has **no row whose
   `[addr_start, addr_end)` brackets 134376768** — confirmed by direct range query
   (zero rows). So `contract_id = null` / `contract_verified = false` is accurate, not
   a hallucination. Cited siblings checked individually:
   - #7 `dma2ch4_dac_siggen`: `verified` is **blank** (not 1); `addr_start` is
     1073873920 = `0x40007400` (a *peripheral* address, not code) — does not bracket
     this routine. Draft's "verified blank … the DMA half" is correct.
   - #8 `exti3_isr_dac_awg`: `verified` blank; `addr_start` 134257680 = `0x08009C10`
     (the EXTI3 ISR), a different address. Correct.
   - #18 (the only nearby `verified=1` row) starts at 134392260 = `0x802a9c4`,
     **above** this addr and is an SPI3 dispatch fragment — unrelated. Draft's "#18
     starts above this addr" is correct.
   **No hallucinated contract id, address, or evidence. hallucination_found = false.**

3. **V-promotion.** Nothing in the draft is scored V2; it self-caps at V1 with the
   explicit reason "no `verified=1` contract asserts any sub-claim of this function."
   That ceiling is correct: V2 would require a verified contract asserting *this
   function's* behavior, which does not exist. Static subsystem-agreement with
   unverified #7/#8 supports V1, not V2. **No demotions. approved_V = V1.**

4. **Disassembly spot-check** (`disasm.py --start 0x8026d40`, 70 insns) — the
   load-bearing decode was re-derived and matches byte-for-byte:
   - `r1 = 0x200000F8`; `ldr [r1,#0xe5c]` (count), `ldrb [r1,#0xe58]` (enable u8);
     `cbz`×2 → 0x8026dd6 disable branch. ✓
   - `r4 = 0x40001400` (TMR7 base). ✓
   - `mul r5,r0,#0xc8` (count·200); `×10` slow tier when `r5 < 0xfa0` (4000); tier
     flag `1`/`0x13` → `[r4,#0x2c]`; `vdiv`/`vadd -1.0` reload → `[r4,#0x28]`. ✓
   - `bl 0x802e430` numerator helper (osc `0x0802a430`). ✓
   - clean `pop {r4,r5,r7,pc}` at 0x8026e10, **no tail-branch** into master-init. ✓
   - disable branch walks DAC base `0x40007400`: `bics +0x14` low-12 (d2dth12r),
     `orr +0x00 #0x380000` (ch2 trigger field), `orr +0x04 #2` (SWTRG ch2 bit1),
     `bfi … #0x13,#3`. ✓ (Minor cosmetic: the resolution table writes `[r4]` for the
     final `bfi`/`orr` block, but the body correctly operates on `r0 = 0x40007400`;
     does not change any claim.)

5. **peripheral_xrefs** for `function_addr=134376768`: 19 rows, **TMR7
   (CTRL1/SWEVT/PR/DIV) + DAC (CTRL/SWTRG/D2DTH12R) only** — zero SPI3 / USART2 /
   GPIOB / DMA / XMC. The `FPGA_BLOCKED → OUTPUT-path` reclassification is solidly
   backed; this routine has no FPGA-facing peripheral access.

**Verdict:** All CONFIRM and CORRECT claims hold; CONFIRM #1 is honestly tagged
static-inferred (not execution-verified) and is not overstated. Nothing overturned.
**Final approved scores: D3 (keep), R1, V1.**
