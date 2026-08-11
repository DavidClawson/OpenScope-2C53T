# Bench Session Plan — FPGA Config-Entry Crack (2026-06-19)

**Goal:** get the GW1N-UV2 FPGA running the **scope** design under *our* firmware,
by (A) proving the bitstream is good via a direct JTAG SRAM load, and (B) finding
the SSPI config-entry recipe the MCU can replay.

**Where we are coming in** (already established, no need to re-prove):
- Our SPI3 **read path works** — the warm-handoff test (Jun 13, Round 3) read a
  clean sine ramp off a stock-configured FPGA via the `0x04`/`0x05` framing.
- The bitstream is **byte-perfect on the wire** vs a stock-boot Saleae capture
  (file offset `0x4AD19`, IDCODE `0x0120681B`), yet the FPGA **rejects** it from
  the MCU (`0x3A` close → `FF` not `F8`; status → all-`FF`). So the problem is
  **config-entry state**, not the bytes and not our read path.
- SSPI port is **clock-limited**: IDCODE garbles at 60 MHz, reads correct at
  ~470 kHz. A few-hundred-kHz external master is in the sweet spot.

This session attacks the one remaining unknown: **what makes the FPGA accept
config**, and whether a fresh JTAG SRAM config free-runs (continuous capture).

---

## ⚠️ SAFETY — read first

1. **JTAG = SRAM ONLY. NEVER flash.** Do not use `-f` / `--write-flash` /
   `--external-flash` / `--user-flash`. The FPGA's internal NV flash holds the
   **only copy** of the stock meter design — there is no undo. Safe flags only:
   `-m` (SRAM), `--detect`, `--read-register` (read-only).
   **Rule: if a flag contains "flash" or is `-f`, STOP.**
2. **Share GND only — do NOT wire VDD or VPP.** The board powers its own FPGA;
   the FT232H / ESP32 are signal-only. GW1N has no VPP pin; do not drive it.
3. SRAM loads are **non-destructive and reversible** — power-cycle and the stock
   NV image reloads. This is the safe experiment.
4. Everything is **3.3 V native** (GW1N-UV2 I/O + CJMCU-FT232H + ESP32). No level
   shifter. Never use a 5 V Arduino without shifting.

---

## Pad map — which tool goes where

> ⚠️ Pad identities below are documented but **NOT yet buzzed out on this board.**
> DMM-trace each pad to its FPGA pin BEFORE soldering (see Track A step 1).

| Tool | Pads | Signals | Role |
|------|------|---------|------|
| **FT232H (JTAG programmer)** | **5 gold diagonal test pads right next to the FPGA** | TMS / TCK / TDI / TDO + GND | **Oracle** — SRAM-load the bitstream directly |
| **ESP32 (SSPI analyzer)** | **back-side SPI3 test pads** (maksidze, issue #18) | SCK / MISO / MOSI / CS + GND | Sweep SSPI handshake for the config-entry recipe |

**Confirmed JTAG pinout (QN48 package, Gowin UG171):**
TMS=pin 8→AD3 · TCK=pin 9→AD0 · TDI=pin 10→AD1 · TDO=pin 11→AD2 · GND=pin 2.
(QN48, *not* QN48H — pins differ. MODE[2:0]=`00X` → JTAG config is HW-permitted.)

**FT232H (CJMCU silkscreen) → JTAG:** AD0=TCK, AD1=TDI, AD2=TDO(in), AD3=TMS, GND=GND.
Classic mistake: TDI/TDO swap (TDO is an FT232H *input* on AD2).

**ESP32 (default VSPI) → SPI3 pads:** GPIO18=SCK, GPIO23=MOSI, GPIO19=MISO,
GPIO5=CS(active LOW), GND=board GND.

---

## Pre-session checklist

- [ ] `openFPGALoader` installed — `openFPGALoader --version` (apt 0.12.0 on this box).
- [ ] `arduino-cli` installed (in `~/.local/bin`) — for the ESP32 sketch.
- [ ] Magnet wire (30–34 AWG), flux, fine-tip iron, hot glue / Kapton for strain relief.
- [ ] DMM (continuity + DC volts).
- [ ] CJMCU-FT232H + an ESP32 dev board. Powered USB hub (run scope USB-C + FT232H
      + ESP32 live at once).
- [ ] Bitstream staged: `fpga_bitstream/scope_bitstream_2c53t_v120.bin`
      (115,638 B, sha256 `5a0e7338…`, carries Gowin preamble + IDCODE `0x0120681B`).

---

## Track A — JTAG oracle (FT232H). DO THIS FIRST.

The decisive diagnostic: a fresh SRAM config with **no MCU reset afterward** —
exactly what the warm-handoff couldn't give. If the scope free-runs after this,
config-entry is provably the whole problem.

1. **Trace (device OFF, battery out).** DMM-buzz each of the 5 gold pads to the
   FPGA pins: pad→pin 9 = TCK, pin 10 = TDI, pin 11 = TDO, pin 8 = TMS, pin 2 =
   GND. Also confirm one pad/ via is true board GND.
2. **Solder** magnet wire to the 4 JTAG pads + GND. Strain-relieve every joint.
3. **Wire to FT232H:** AD0→TCK, AD1→TDI, AD2→TDO, AD3→TMS, GND→GND. **No VDD/VPP.**
4. **Power the scope** from its own battery/USB. FT232H is signal-only.
5. **Detect (the green light):**
   ```
   openFPGALoader -c ft232 --detect
   ```
   ✅ Success = IDCODE **`0x0120681B`**. Flaky? add `--freq 1000000`.
6. **SRAM-load the bitstream (SAFE):**
   ```
   openFPGALoader -c ft232 -m --file-type bin fpga_bitstream/scope_bitstream_2c53t_v120.bin
   ```
   Watch for DONE high / the scope trace coming alive. If `--file-type bin` is
   rejected, fall back to `fpga_bitstream/scope_bitstream_2c53t_v120.fs` (bit
   order unverified — regenerate with `make_fs.py` if it CRC-errors where the bin
   doesn't).
7. **If config is rejected**, read why:
   ```
   openFPGALoader -c ft232 --read-register STATUS
   ```
   Decode CRC-error vs IDCODE-mismatch bits (Gowin UG290).

**Interpreting Track A:**
- Scope comes alive + free-running capture ⇒ **bitstream good, config-entry was
  the whole problem.** Next: nail the SSPI recipe (Track B) and port to `fpga.c`.
- Loads but no capture ⇒ run-state / arm issue; `spi3 armtest` probes are ready.
- `--detect` fails ⇒ recheck TDI/TDO swap, shared GND, JTAGSEL_N, lower `--freq`.

## ⛔ JTAG commands to NEVER run

```
openFPGALoader -c ft232 -f ...              # FLASH — NO
openFPGALoader -c ft232 --write-flash ...   # FLASH — NO
openFPGALoader -c ft232 --external-flash ...# NO
openFPGALoader -c ft232 --user-flash ...    # NO
```

---

## Track B — ESP32 SSPI sweep (can run in parallel / maksidze can run his ESP32)

1. **Solder** magnet wire to the back-side SPI3 pads: SCK, MISO, MOSI, CS + a GND
   via. Strain-relieve. Scrape/tin enamel first.
2. **Wire to ESP32:** GPIO18→SCK, GPIO23→MOSI, GPIO19→MISO, GPIO5→CS, GND→GND.
3. **Flash the sketch:** `tools/esp32_sspi_bringup/esp32_sspi_bringup.ino`.
4. **Flash the scope** with this branch (`make guest` for the factory-bootloader
   unit) and in its debug shell run **`fpga busrelease`** — MCU tri-states
   PB3/PB5/PB6, holds PC6/PB11 HIGH and PC9 power. The bus is now the ESP32's.
   (No un-release; power-cycle/re-flash to reclaim.)
5. **Iterate** over serial (115200): `id` (does the config port answer? expect
   `01 20 68 1B`) → `enable` (prelude `05`/`12`/`15`) → `status`, sweeping
   `clk <kHz>` and `raw XX XX …` framings until STATUS flips toward the stock
   `00 01 42 2E 2E` and the `0x3A` close returns `F8`.

**ESP32 TODO still open** (from the rig README): bench-validate `fpga busrelease`
actually Hi-Z's the lines (scope the pads); confirm IDCODE read framing /
dummy-byte count vs the #18 capture; add the 115,638-byte `0x3B…0x3A` upload path
once the IDCODE/prelude round-trip is confirmed.

---

## Endgame

Both tracks feed one outcome: the **SSPI config-entry recipe** the MCU can run.
JTAG proves the bitstream + run-state; ESP32 finds the handshake. The winning
recipe goes back into `firmware/src/drivers/fpga.c`, and
`fpga_acquisition_task` gets rewritten from the old `0x80|range` to the real
`0x04`/`0x05` read protocol so the scope UI shows a live trace.

## References
- `docs/gowin_jtag_programmer_guide.md` — full FT232H/openFPGALoader playbook + safety.
- `tools/esp32_sspi_bringup/README.md` — ESP32 rig wiring + serial commands.
- `docs/fpga_warm_handoff_test.md` — read-path validation results (Jun 13).
- `reverse_engineering/captures/SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md` — bitstream
  offset `0x4AD19`, post-config SPI3 writes, `0x04`/`0x05` read protocol.
- GitHub issue **#18** — maksidze's pad exposure (SPI3 back-side + JTAG TAP).
