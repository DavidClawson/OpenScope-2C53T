# Rigorous Coverage Ledger — stock firmware → understood + runnable + verified

**Created:** 2026-06-13
**Why:** `COVERAGE.md` reports "~99% understanding," but that counts functions *named*.
The 2026-06-13 stock-bringup-diff workflow proved all 22 critical-path units **diverge**
from our firmware — so "named" overstates "understood and runnable." This ledger measures
the real thing on a hard, non-gameable rubric, function-by-function, and produces one
headline % we drive toward 100%.

## The rubric (per function, 309 total)

Three dimensions, scored independently:

- **D — Decode** (do we understand exactly what it does?)
  - 0 none · 1 named/role only · 2 partial register-level · **3 full register-level
    reconstruction, nothing unexplained**
- **R — Reimplement** (is it in our runnable firmware?)
  - 0 absent · 1 partial · **2 complete in our firmware** · **NA = justified not-needed**
    (libc we replace, stock-only assets, dead code) with a one-line reason
- **V — Verify** (is our version provably equivalent to stock?)
  - 0 unverified · 1 static equivalence-reviewed · **2 bench- or emulation-confirmed
    equivalent** · NA (when R=NA)

**A function is RESOLVED when:** `D=3` AND ( (`R=2` AND `V≥1`) OR `R=NA` ).
i.e. we either faithfully reimplemented and verified it, or justified skipping it.

**Headline metric = RESOLVED / (meaningful functions).**

### Denominator (meaningful functions)

Of 309 real functions, some are legitimately **SKIP-justified** (`R=NA`) and count as
resolved once justified — we don't need to reimplement printf:

| Class | ~count | Disposition |
|---|---|---|
| C runtime (printf/math/memset/memcpy/strcmp) | 56 | NA — we use our own libc |
| JPEG/Huffman decoders (0x08030524, 0x08031f20) | 2 | NA — boot-logo assets |
| Font/display-render engine | ~9 | NA — our own font system |
| UI image/asset rendering | TBD | NA — our own UI |
| **Meaningful (hardware/protocol/logic)** | **~235** | must reach D=3 + R + V |

The ledger lists every function with its class so the denominator is explicit, not fudged.

## Baseline scoreboard (MEASURED 2026-06-13 — `coverage_ledger.csv`, 309 functions)

| Metric | Baseline | Current (2026-06-13) |
|---|---|---|
| Full register-level decode (D=3) | 23/309 = 7.4% | **107/309 = 34.6%** |
| Skip-justified (R=NA) | 119 | **191** |
| **Meaningful (R≠NA: real device behavior)** | 190 | **118** |
| RESOLVED, gross | 60/309 | 139/309 |
| **RESOLVED, meaningful — THE HEADLINE** | 1/190 = 0.5% | **11/118 = 9.3%** |
| Verified equivalent to stock (V≥1, meaningful) | ~0 | 11 (SPI2 ×3, lcd_read_data, +6 RCC/GPIO/SPI primitives) |

**Honest reading of 0.5%→9.3% (three rounds):** the move splits into **denominator correction**
(reclassifying the FreeRTOS kernel + our own UI out of "device behavior we reimplement" — they
were mis-scored R2) and **genuine numerator growth** (~10 real drivers verified faithful: SPI2
read path, `lcd_read_data`, and the RCC/GPIO/SPI bring-up primitives, all V1 *by code
inspection*, not bench). The **decode win is large and real: D3 23→107** (35% of all functions
fully register-decoded). **⚠️ ZERO scope/FPGA acquisition behavior is resolved** — the entire
acquisition chain (`scope_main_fsm`, decimator, measurement engine, trigger DAC) is
FPGA_BLOCKED or ABSENT; the scope still runs on synthetic data pending the config-entry wall.
**Kernel names fixed** (46 → correct upstream symbols, e.g. `freertos_timer_command` →
`prvCheckForValidListAndQueue`); ~12 more R1 names corrected from the decode (the `spi_flash_*`
cluster is really a FatFs/FTL layer absent from our tree). Reimpl roadmap (what grows the
numerator next, FPGA-independent first) in `analysis_v120/relabel_r1_2026-06-13.json`.

### Resolve-loop log

| Date | Functions resolved | Headline | Method |
|---|---|---|---|
| 2026-06-13 | `spi2_block_read`, `spi2_receive_byte`, `spi2_transceive_byte` (3) | 0.5% → **2.1%** | Static register-equivalence vs `flash_fs.c` (`flash_fs_raw_read` / `flash_fs_raw_spi_xfer`). Stock cmd-0x03 read protocol byte-identical to ours; Ghidra `i2c_transfer` is a mislabel of the SPI2 SR (TXE/RXNE) poll. V1 (static). |
| 2026-06-13 | `master_init` decode-diff (14-region workflow) — honest D3/R1→**D2/R0** correction | 2.1% (no change — still unresolved) | 13/14 regions decoded to D3; reimpl floor R0 (cal-table restore absent). **Output: a ranked, testable FPGA config-entry hypothesis** (pre-0x3B USART burst + floating frontend relays) — the campaign's stated win condition. See `analysis_v120/master_init_decode_diff_2026-06-13.md` + plan-doc Current State. |
| 2026-06-13 | `flash_cfg_cal` detour — live W25Q128 dump from Unit 2 | 2.1% (elimination result) | Drove the device's `flash dump` shell directly (no reflash) + offline FAT12 parse. **Factory cal is NOT on the W25Q:** the chip is UI JPEGs + screenshots + one *empty* `9999.BIN` config placeholder (cluster 0 / size 0), two volumes filling all 16 MB with no raw region. Narrows the R0 `flash_cfg_cal` blocker to MCU flash `0x08006000` (overwritten by our app) or the FPGA bitstream. Tooling: `scripts/flash_capture.py`. See `analysis_v120/w25q128_flash_map_2026-06-13.md`. |
| 2026-06-13 | verify-first harvest (8-agent workflow) — 62 of the 63 R2-unresolved classified | 2.1% → **3.9%** (denominator-shrink + 1 driver) | 46 `NA(same FreeRTOS source)` (kernel verified present in build) + 15 `NA(our own UI)` + **1 VERIFY_FAITHFUL** (`lcd_read_data` V1) + 1 held UNRESOLVED (`fpga_spi3_transfer`, FPGA wall). **D3 23→75.** Honest: mostly denominator correction; genuine proven-equivalent device behavior added = 1 trivial fn. Surfaced the `lcd_*` Ghidra mislabels + the kernel-name hazard. |
| 2026-06-13 | relabel + R1 decode (16-agent workflow) — 42 driver/protocol fns | 3.9% → **9.3%** | 46 kernel names corrected to upstream symbols; R1 cluster decoded to D3 (D3 75→107). 14 resolved: **6 VERIFY_FAITHFUL** (RCC/GPIO/SPI primitives — genuine numerator) + 8 NA_OUR_OWN. 12 FPGA_BLOCKED (the actual scope, decoded but bench-gated), 5 ABSENT_R0 (FatFs layer we lack), 4 DIVERGENT, 6 DECODE_ONLY. Honesty catch: `scope_autoset_trigger_track` (08001c60) corrected from NA→FPGA_BLOCKED. Roadmap: `analysis_v120/relabel_r1_2026-06-13.json`. |

**Track the meaningful figure (9.3%), not the gross (40%+)** — the gross number is
dominated by libc/FreeRTOS/USB-descriptor plumbing auto-credited as `R=NA`. Of the 118
functions with real device semantics, **11** are resolved (`usart2_isr`, SPI2 ×3,
`lcd_read_data`).

The gap between "~99% understood" (old soft metric) and **9.3% meaningful-resolved** *is
the project.* Every point of it is now a concrete, checkable line in `coverage_ledger.csv`.
The honest path up from here is **numerator** growth — faithfully reimplementing + verifying
the genuine drivers/protocol (the R1 frontend/scope/meter cluster), not more NA reclasss.

### The spine (23 D3 functions = the load-bearing nodes)

Fully decoded but mostly **not** reimplemented/verified — this is where the campaign
lives: `master_init` (15.4KB early-boot bring-up — also the FPGA NV-awaken hypothesis),
`gpio_mux_portc_porte` + `gpio_mux_porta_portb` (frontend relay + DAC trigger path),
`fpga_spi3_transfer` (V is genuinely bench-gated, not static-verifiable: the Ghidra
decode @0x08037800 is register-context-corrupt — `unaff_r7`/`unaff_r9`/`in_ZR`, it's the
misdecoded acquisition inner-loop; needs the issue-#18 0x04/0x05 readout litmus),
`usart_tx_config_writer`, `scope_main_fsm`, `scope_measurement_engine`,
`scope_mode_timebase`, `meter_data_process`, and the SPI2 flash primitives.
Top-22 worklist with concrete next-actions is in the workflow result / will seed each session.

## Relationship to the FPGA secret (honest)

The 2026-06-13 register-level proof (`CONFIG_ENABLE` can't engage `SYSTEM_EDIT_MODE`;
`FLASH_LOCK` set) strongly suggests the FPGA secret is **NOT a decodable MCU instruction
on the SSPI path** — we've exhausted that with proof. So driving this ledger to 100% is
primarily about a **complete, faithful, runnable custom firmware** (worth it on its own).

BUT one FPGA hypothesis remains firmware-reachable and is *directly targeted* by full
coverage: **stock brings its FPGA NV design to an "announce-frames-alive" state in the
first ~1.4s that ours never reaches.** If that's caused by an MCU action, a 100% faithful
decode of the early-boot path finds it. So the campaign serves both goals; we just don't
over-promise the secret falls out of it.

## Process (the steady push)

1. **Populate** — a workflow scores all 309 functions on D/R/V (batched), writing
   `coverage_ledger.csv` + per-function notes. Produces the real baseline headline %.
2. **Prioritize** — sort unresolved by (relevance × leverage). FPGA/clock/early-boot
   first (also chases the remaining hypothesis); assets/libc get fast NA justifications.
3. **Resolve in batches** — for each function: full register decode (D→3), reimplement or
   justify NA (R), then verify (V) via static equivalence or — best — an **emulation
   trace** of stock execution (the ground-truth layer; see plan doc). Update the ledger.
4. **Re-measure** — headline % ticks up each session. Stop at 100% OR when a decoded
   early-boot divergence yields a testable FPGA hypothesis.

## Files

- `coverage_ledger.csv` — the per-function ledger (created by the population workflow).
- Source inventory: `analysis_v120/function_map_complete.txt` (309 addr/size/callers),
  `analysis_v120/function_names.md` (names + confidence), `COVERAGE.md` (soft tracker).
- Verification engine (deferred build): Unicorn/Renode stock-execution trace, see
  `docs/fpga_stock_bringup_diff_plan.md`.
