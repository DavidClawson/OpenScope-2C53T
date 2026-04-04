# Next Session Plan

Baseline: commit `756153f` on `main`. Created 2026-04-04 after a session that landed meter modes (DCV/Resistance/Continuity/Diode working on hardware), DAC output driver, bootloader POWER+PRM combo, and 3-strike boot safety fix.

## The 5 priorities, ordered for next time

1. **Meter auto-ranging** — decimal position + unit suffix track range bits from USART frames
2. **Frequency / capacitance meter modes** — plumb untested submodes end-to-end
3. **Scope acquisition** — get real waveforms on screen (biggest feature gap)
4. **Signal gen FPGA encoding** — unlock BNC output (high-risk RE, own session)
5. **Polish** — remove debug overlay, load factory cal from SPI flash

## Recommended single-session ordering

1. **Phase 0 (60 min)** — polish warm-ups, ships one commit
2. **Phase 1 (2.5 hr)** — auto-ranging, biggest visible win per hour
3. **Phase 2 (1.5 hr)** — freq/cap modes, piggybacks on Phase 1
4. **Phase 3 (1 hr)** — polish + cal application
5. **Phase 4 Levels A+B (2 hr)** — scope acquisition bringup, not full

**Do NOT bundle Phase 5 with Phase 4 — give signal gen its own session.**

---

## Phase 0 — Warm-up Polish (30–60 min)

Sub-hour, zero-RE, always-useful changes. Batch at session start so a slow RE dive later still produces a shippable commit.

- **P0.1 Remove scope debug overlay** (10 min) — gate behind `SCOPE_DEBUG_OVERLAY` macro. Files: `src/ui/scope_ui.c`.
- **P0.2 Dead-code sweep** (15 min) — move `btntest.c`, `fpgatalk.c`, `spi3test.c`, etc. to `src/attic/` or `tools/bringup/`. Check Makefile.
- **P0.3 Factory calibration load stub** (20 min) — load 301 bytes per channel from W25Q128JVSQ into RAM mirror at `0x20000358-0x20000434`. Don't apply yet, just load and log. Files: `src/drivers/flash_fs.c`.
- **P0.4 Demo waveform gate** (10 min) — only run synthetic demo when `scope_acquisition_ready == false`. Files: `src/ui/scope_ui.c`, `src/ui/scope_state.c`.

---

## Phase 1 — Meter Auto-Ranging (medium, 2–3 hours) ⭐ recommended first

Self-contained, tight bench feedback loop. `range_indicator` and `probe_type` are already parsed in `meter_data.c` but never feed `decimal_pos`.

### Success criteria
- DCV step sources (0.001, 0.01, 0.1, 1, 10 V) each read with correct decimal without manual submode switching
- Resistance series (100 Ω, 1 kΩ, 10 kΩ, 100 kΩ, 1 MΩ) all read with correct unit (Ω / kΩ / MΩ)
- Mid-reading autorange transitions don't flash wrong-magnitude values
- `default_decimal_pos[]` becomes fallback only

### Approach
1. Decompile stock `meter_mode_handler` FSM at `0x080371B0` from `decompiled_2C53T_v2.c` — ~6 states, keyed on `frame[6]` bits 4–5 and `frame[7]` bit 3
2. Build `decimal_pos_for_range[submode][range_bits]` and `unit_suffix_for_range[submode][range_bits]` tables
3. Wire the lookup into `meter_data_process_frame`
4. Add `unit_suffix` field to `meter_reading_t`
5. Add build-flag-gated UART trace for bench debugging

### Files
- `src/drivers/meter_data.c`, `.h`
- `src/ui/meter_ui.c` (unit rendering, bar-graph full-scale per range)

### Key decisions first
- Exact bit layout of range index — READ THE GHIDRA DECOMP, don't guess
- Is there an "autorange in progress" flag to suppress bad frames?

---

## Phase 2 — Freq / Capacitance Meter Modes (medium, 1.5–2 hours)

Piggybacks on Phase 1's range/unit infrastructure. FPGA plumbing already exists in `fpga_set_meter_mode()`.

### Success criteria
- Frequency: 1 kHz / 10 kHz / 100 kHz / 1 MHz each read correctly with Hz/kHz/MHz unit
- Capacitance: 100 pF / 1 nF / 100 nF / 1 µF / 10 µF each within reasonable tolerance
- No watchdog resets entering/exiting these modes
- system_mode mapping confirmed: frequency=4, capacitance=3

### Approach
1. Extend Phase 1 tables with frequency and capacitance rows
2. Cross-check `fpga_set_meter_mode()` command sequences against stock via `fpga_comms_deep_dive.c`
3. Hardware bench test
4. If frequency readings garbled, check whether it uses a different digit encoding (look for mode-keyed branch in stock `meter_data_processor`)

### Files
- `src/drivers/meter_data.c`, `src/drivers/fpga.c`, `src/ui/meter_ui.c`

### Depends on
Phase 1

---

## Phase 3 — Polish & Cal Application (easy–medium, 1–2 hours)

Do **before** the big RE dives so the session always produces a clean commit.

### Success criteria
- Demo waveform off by default
- Factory cal from Phase 0.3 actually applied to DCV path, accuracy preserved
- All debug printfs behind `FW_DEBUG` flag
- No dead static functions, no compiler warnings

### Files
- `src/ui/scope_ui.c`, `src/ui/scope_state.c` — demo gate
- `src/drivers/meter_data.c` — apply per-channel gain/offset
- `src/drivers/flash_fs.c` — the loader stubbed in P0.3

### Depends on
P0.3

---

## Phase 4 — Scope Acquisition (hard, 4–6 hours, own session)

**The feature the user cares most about as a tool.** SPI3 read path already exists. Problem is FPGA state when we poke it. Stacked success criteria — any level is shippable.

### Levels
- **Level A (must do first):** `spi3_xfer()` has a hard timeout. No path can watchdog-reset. Stuck FPGA shows error banner, not reboot loop.
- **Level B:** Scope mode entry sends full stock command sequence including voltage-range preamble (`command_code = 0x80 XOR voltage_range`). FPGA consistently reports ready.
- **Level C:** Single-shot acquisition returns 8-bit samples; fed through fixed scale, draws correct DC level for known 1 V input.
- **Level D:** Continuous triggered acquisition at one timebase, stable on 1 kHz square wave from probe comp pin.

### Approach
1. **First, fix the hang.** Rewrite `spi3_xfer()` at `src/drivers/fpga.c:85` with cycle-bounded timeout + sentinel return. Every caller propagates failure.
2. **Add ready gate.** Before any scope read burst, poll FPGA status N times; abort if never "ready". Re-enable debug overlay during this phase.
3. **Decompile scope-mode init.** Grep `decompiled_2C53T_v2.c` for `command_code = 0x80 ^ voltage_range` — that's the fingerprint. Trace upward to find scope equivalent of `siggen_configure`. **One bounded RE task.**
4. **Mirror into `fpga.c`** as `fpga_scope_mode_enter(voltage_range, timebase, trigger_src, trigger_level)`.
5. **Drive from `scope_ui.c`.** Demo as fallback when acquisition fails.
6. **Bench validate** Levels C → D. Watch for sign inversion, endianness, packing order.

### Key unknowns first
- Preamble on USART2, SPI3, or both? (stock RE implies USART2 config + SPI3 data)
- Sample packing: interleaved vs separate bursts (re-verify against stock, `fpga.c:357` assumes interleaved)
- Trigger level units (raw ADC vs mV) — may need Phase 3 cal

### Files
- `src/drivers/fpga.c`, `.h`
- `src/ui/scope_ui.c`, `src/ui/scope_state.c`

### Depends on
P0.1 (overlay), Phase 3 (accurate scaling)

### Trade-off
Level A → B is ~70% of the value. If time runs out there, stop. Levels C/D can resume next session with zero re-learning.

---

## Phase 5 — Signal Gen FPGA Encoding (hard, OWN SESSION)

Highest-risk item. **Don't bundle with Phase 4.** The MCU DAC on PA4 isn't routed to the BNC — stock firmware routes through FPGA via cmds 0x02–0x06, whose param encoding lives in dispatch handlers at `0x0804BE74` that haven't been decompiled.

### Recommended: capture-first, RE-second
Inverts usual order because dispatch-table RE is unbounded (each handler could hide arbitrary packing), while a logic-analyzer capture is time-bounded.

### Levels
- **A:** Dispatch table at `0x0804BE74` dumped, 5 handlers (cmds 0x02–0x06) encodings identified
- **B:** Cmd 0x03 + known frequency produces detectable signal on BNC
- **C:** All 7 stock waveforms selectable, each produces correct shape
- **D:** Frequency/amplitude sliders produce expected changes

### Capture-first approach
1. **Logic analyzer on stock firmware** — HiLetgo 24 MHz 8CH via sigrok-cli (per CLAUDE.md). Clip USART2 TX between MCU and FPGA. Flash stock firmware temporarily if needed.
2. **Drive the stock UI** to cycle through waveforms, sweep frequency, sweep amplitude, change offset
3. **Decode USART2 frames** — framing is known from `fpga_comms_deep_dive.c`, only payload semantics are unknown
4. **Correlate** waveform → cmd 0x03 byte, frequency → cmd 0x0? 4-byte word, amplitude/offset → byte
5. **Mirror encoding** in `src/ui/siggen_ui.c` and `src/dsp/signal_gen.*` as plain lookup + scale
6. **RE as tiebreaker only** — drop into Ghidra on specific handlers when capture leaves ambiguity

### Alternative (no stock board available)
Dump dispatch table bytes from decompiled binary, resolve function pointers, decompile each handler. Est. 1–2 hr × 5 handlers = 5–10 hr, high variance.

### Files
- `src/ui/siggen_ui.c`
- `src/dsp/signal_gen.h` (52 lines today, likely needs expansion)
- `src/drivers/fpga.c` (new `fpga_siggen_*` config writers)

### Depends on
**Ideally Phase 4 Level C first** — then you can verify siggen output using the unit's own scope, closing the loop without external hardware. Otherwise you need a second scope on the bench.

### Trade-off
Phase 5 before Phase 4 is legitimate only if external verification gear is available. Otherwise Phase 4 enables Phase 5 testing and the dependency direction is fixed.

---

## Cross-cutting concerns

- **Watchdog safety** — every new FPGA interaction in Phase 4/5 must have a timeout. The current `spi3_xfer` hang is the canonical failure mode.
- **Calibration struct ownership** — RAM block at `0x20000358-0x20000434` read by multiple subsystems in stock. Own it in `flash_fs.c`, expose via read-only pointer, don't let drivers re-load.
- **Submode numbering** — Phase 1 tables are keyed on submode. If later phases renumber, ALL tables update together. Define enum in one header (`meter_data.h`), grep-enforce.
- **Bootloader safety** — the 3-strike fix in `756153f` is the safety net. Don't disable during experiments. Don't add code that can boot-loop before it engages.

---

## Key RE resources

- `reverse_engineering/analysis_v120/fpga_comms_deep_dive.c` — mode dispatcher, USART protocol, state machine
- `reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md` — 9 SPI3 modes, config writer types
- `reverse_engineering/analysis_v120/STATE_STRUCTURE.md` — complete state offset map
- `reverse_engineering/decompiled_2C53T_v2.c` — 39K lines, contains `siggen_configure` at `0x08001C60`, `meter_mode_handler` FSM at `0x080371B0`
- `reverse_engineering/HARDWARE_PINOUT.md` — DAC registers, pin assignments
- Dispatch table at `0x0804BE74` — **NOT YET DUMPED** (Phase 5 blocker)
- sigrok-cli with HiLetgo 24MHz 8CH USB analyzer

## Critical source files

- `src/drivers/meter_data.c` — Phase 1, 2, 3
- `src/drivers/fpga.c` — Phase 2, 4, 5
- `src/ui/scope_ui.c` — Phase 0, 3, 4
- `src/ui/siggen_ui.c` — Phase 5
- `src/ui/meter_ui.c` — Phase 1, 2
- `reverse_engineering/decompiled_2C53T_v2.c` — Phase 1, 4, 5 research
