# Build A — hardware-SPI3 config-entry test (bench procedure)

Bench plan item 3 (`docs/bench_plan_2026-08-13.md`). Isolates the load-bearing
variable behind Stlkv's #18 cold-start success.

## Why this build exists

Stlkv (issue #18, 2026-08-12) cold-started the FPGA using **our pins** (PB3/4/5,
**PB6 CS** — not PA15) and **our standard 115,638-byte payload**, via maksidze's
2C23T-V0.4 **GPIO bit-bang** SSPI loop with a richer prelude
(`0x11 IDCODE → 0x13 USERCODE → 0x41 STATUS → 0x12 → 0x15 → 0x3B <payload> → 0x41 → 0x3A`).
STATUS went `0x00039020 → 0x0003F460` — **DONE_FINAL(13) set**, 4/4 boots. That
narrows the difference from our two months of hardware-SPI refusals to three
candidates, none of them bytes/payload/CS-pin/MCU-state (all already excluded):

1. slow/gapped **bit-bang clocking** of the whole sequence,
2. the **richer prelude reads** adjacent to CONFIG_ENABLE,
3. **GPIO-mode vs hardware-SPI-AF** pins.

**Build A tests (1)+(2) on our existing hardware-SPI3 path, without bit-banging:**
- `cmd_br = 7` **and** `upload_br = 7` → the *entire* sequence, including the 0x3B
  payload, clocks at **/256** (~2.0 s upload; the splash→trace gap is expected).
- `prelude_reads = 1` → `0x11 / 0x13 / 0x41` inserted in their own CS frames
  **between 0x05 ERASE_SRAM and 0x12 INIT_ADDR** — the position Stlkv's loader uses
  (`fpga.c` step `[1b]`).
- `probe_edit = 1` → reads STATUS(0x41) at /256 right after 0x15 (the `ED:` line).

Everything else stays our stock-faithful hardware-SPI3 sequence.

## Build + flash (no USB CDC needed — CDC is dead on unit #1)

```
cd firmware && make guest-configA          # -DFPGA_CONFIG_A=1, guest LD @ 0x08007000
python3 ../scripts/iap_flash.py            # MENU+Power → upgrade mode, detect, flash
```
Our images auto-boot into the app after IAP (Exp R correction) — no charge-mode step.

## Read the result — LCD `SCOPE_DEBUG_OVERLAY`, scope screen

The overlay is compiled in (guest builds define `SCOPE_DEBUG_OVERLAY`). Two lines
carry the verdict:

- **`CFG:xxxxxxxx Dn Ex Ay Lu Hu`** — `CFG` is STATUS(0x41) read at /256 on the
  anchored path after the full upload + 0x3A close. **`D` = DONE_FINAL (bit 13).**
- **`S1:037F ED:........ H2:Y`** — `S1` is SPI3 CTRL1 latched during the config
  frame; **`037F` confirms BR=7 (/256) on-device**, `0347` would mean /2.

**SUCCESS = `D1` in the CFG line (and CFG ≈ `0003F460`-class), with `S1:037F`.**
Expect a one-shot 04/05 baseline (engine starts UNARMED after config — the arm is
a separate step, see the netlist answer in #18 / `fpga-arm-register-netlist`).

**WALL HOLDS = `D0`, CFG `00039020`, `ED:....` bit7 clear** — same refusal as every
prior hardware-SPI run, now with /256 throughout + adjacent reads ruled out.

## ⚠ Must be a genuine FPGA power cycle

Per Exp R: pinhole reset and MENU+Power do NOT drop FPGA power. To cold-start the
FPGA: **hold POWER → "Goodbye" → UNPLUG USB (device dark) → replug**. Otherwise the
FPGA has not seen a fresh POR and the test is about MCU-driven config entry on an
already-running NV design (still a valid read, but note which one you took).

## Interpreting the outcome

- **D1 (success):** clock + prelude sufficient; **bit-bang is unnecessary.** Fold
  Build A into `fpga_init` as the default cold-boot path ⇒ cold boot straight into
  OpenScope becomes the shippable route (meets the no-case-crack constraint), and
  the stock-stub chainload idea is **dead**. Then chase the engine-arm (item 4)
  using the register map from the #18 netlist answer. Try /8 or /16 upload after
  as a boot-time optimization. Update CLAUDE.md + memory.
- **D0 (wall holds):** GPIO-mode-vs-AF is the last remaining variable ⇒ **Build B**
  (below), the true bit-bang transplant.

Firmware: `fpga.c` `FPGA_CONFIG_A` block + step `[1b]`; `fpga.h` `prelude_reads`;
`Makefile` `guest-configA`.

---

# Build B — bit-bang transplant (`make guest-configB`)

Ported byte-for-byte from Stlkv's working cold-start loader
(`Stlkv/OpenScope-2C23T-2C53T-port` `2c53t-port`, `src/fpga.c fpga53_v04_configure`).
Runs the whole SSPI handshake on **GPIO-mode PB3(SCK)/PB4(MISO)/PB5(MOSI)/PB6(CS)**
— not the SPI3 peripheral — mode-3 MSB-first, then restores SPI3 AF so acquisition
works if config takes. Implemented as `fpga_bitbang_config_sequence()`, called at
boot **instead of** the hardware sequence.

```
cd firmware && make guest-configB && python3 ../scripts/iap_flash.py
```
Same LCD verdict: **`CFG:...D1`** = DONE_FINAL = success. `S1:BBBB` is a marker
that this was the bit-bang build (there is no SPI3 CTRL1 to report).

### ⚠ Build B is NOT a single-variable delta from Build A — read before interpreting

Build B faithfully reproduces **Stlkv's exact V0.4 recipe**, which differs from our
hardware sequence in **two** ways, not one:
1. **GPIO bit-bang vs hardware-SPI AF** (the variable we care about), and
2. **no `0x05` ERASE_SRAM prelude** — V0.4 goes `IDCODE → USERCODE → STATUS → 0x12
   → 0x15 → 0x3B` with no `0x05`; our hardware sequence (and Build A) sends `0x05`.

So if **A fails and B succeeds**, the cause is bit-bang **and/or** the missing
`0x05` — a follow-up bisect names which (cheapest: add `0x05` back to Build B, or
flip Build B's pins to AF, and re-run). If **both fail**, the difference from
Stlkv's rig is off our PB3/4/5/6 pins entirely (his GD32-lineage board vs our AT32,
or something his firmware does elsewhere in boot). If **B succeeds**, we have
Stlkv's success reproduced on our own codebase — the platform for boot-to-trace.

Firmware: `fpga.c` `FPGA_CONFIG_B` block (`fpga_bitbang_config_sequence`, `bb_*`
helpers); `Makefile` `guest-configB`. No reset pulse (the V0.4 reset pin maps to
the 2C53T POWER button — must not be driven).

---

# Item 5 — CH2 trigger reference bring-up (`make guest-warmtest-ch2`)

Independent of the config-entry A/B; it runs on the **warm-handoff** path (stock
already configured the FPGA), so it can be A/B'd in the same bench session.

**Why CH2 was dead:** ripcord contract 38 — CH2's trigger comparator reference is
not a DAC channel but a **PWM-DAC on TMR13 channel 1** (compare reg `C1DT @
0x40001C34`), output on **PA6** (`TMR13_CH1`, `tmr13_mux=0` — stock never remaps,
verified: no `0x4001001C` literal in the image). Its per-range value uses the
**same cal formula as DAC1**. Our firmware never programmed TMR13, so CH2's
comparator had no reference — exactly the CH1 situation before the DAC1 fix.

**Stock TMR13 config**, decoded from master_init (`0x0802B0FE`..`0x0802B34E`):
`DIV(PSC)=0`, `PR(ARR)=4094` (0xFFE, a 4095-count period), `CM1` OCM=`0b111` (PWM),
`CCTRL` `C1EN|C1P` (enable + active-low), `CTRL1` CEN. `guest-warmtest-ch2` brings
that up and arms `C1DT` mid-scale (2048), mirroring the DAC1 arm.

```
cd firmware && make guest-warmtest-ch2 && python3 ../scripts/iap_flash.py
# then the warm-handoff recipe: stock boot → MENU+pinhole handoff (see
# docs/fpga_warm_handoff_test.md), scope mode, switch to CH2.
```
**SUCCESS = a live CH2 trace appears** (the way CH1 came up after the DAC1 fix),
instead of flat/dead. Verdict is visual on the LCD.

### ⚠ PA6 caveat — the one thing to confirm
PA6 was `Unknown` in `HARDWARE_PINOUT` and had been loosely called an
analog-frontend line. Identifying it as `TMR13_CH1` is a **decoded inference**,
now driven as AF PWM by this build. If CH2 stays dead **and** something in the
frontend misbehaves, PA6 may have a conflicting role — back it out
(`guest-warmtest` without the flag) and flag for the ripcord session. If CH2 comes
alive, PA6 = CH2 trigger reference is confirmed and the pinout entry graduates.

Firmware: `scope_trigger.c/.h` (`scope_trigger_ch2_*`, TMR13 regs); `fpga.c`
`FPGA_CH2_TRIGGER` block; `Makefile` `guest-warmtest-ch2`.

---

# Combined bench cycle — flash order & one-line verdicts

All three are independent guest images; flash via `python3 scripts/iap_flash.py`.
Cold-boot = hold POWER → "Goodbye" → unplug USB → replug (real FPGA power cycle).

| # | Build | Boot | Read | SUCCESS |
|---|-------|------|------|---------|
| 3A | `guest-configA` | cold power cycle | scope overlay `CFG`/`S1` | `CFG:…D1` + `S1:037F` |
| 3B | `guest-configB` (only if 3A = D0) | cold power cycle | scope overlay `CFG` | `CFG:…D1` (`S1:BBBB`) |
| 5 | `guest-warmtest-ch2` | stock boot → warm handoff | CH2 scope trace | live CH2 trace appears |

If 3A or 3B breaks the wall, config entry is solved in our firmware ⇒ fold the
winner into `fpga_init`, kill the stock-stub chainload, update CLAUDE.md + memory.
Item 5 is orthogonal and can be run regardless of the 3A/3B outcome.
