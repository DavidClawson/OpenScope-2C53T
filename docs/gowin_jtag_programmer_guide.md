# Gowin FPGA JTAG Programming — Bench Guide (FT232H + openFPGALoader, macOS)

Target: **FNIRSI 2C53T** FPGA = **Gowin GW1N-UV2** (GW1N-2 family, IDCODE `0x0120681B` confirmed).
Programmer: **CJMCU-FT232H** (single-channel FT232H, MPSSE, 3.3V I/O).
Goal: load the known-good scope bitstream into the FPGA's **volatile SRAM only** (bypassing the broken MCU SSPI upload) and read its config status — **without ever touching the FPGA's internal NV flash** (it holds the only copy of the meter design).

---

## Bench-day quick start — read this first

Two questions that come up every time you set this up:

**Q: "The FT232H programmer has several headers — do I need pins from all of them?"**
**No. You need exactly 5 pins, all on the MPSSE / `AD` bank:** `AD0`, `AD1`, `AD2`, `AD3`, and `GND`. Leave everything else unconnected — the other I/O bank (`AC0–AC9`), `AD4–AD7`, and **especially the power pins (`5V` / `3V3` / `VBUS`)**. Do **not** wire FT232H power into the scope; the device powers its own FPGA and you only share **GND** as a signal reference. (This guide targets the **CJMCU-FT232H**; if your programmer is something else — a real Gowin dongle, an ESP32, etc. — tell me, the wiring differs.)

**Q: "There are copper pads I have to solder onto — which ones?"**
The **diagonal row of 5 gold/copper test pads right next to the FPGA.** Those are TMS/TCK/TDI/TDO + GND broken out from the leadless QFN (you can't solder the chip's own pads). Solder thin **magnet wire, 30–34 AWG**, scrape/tin the enamel first, and strain-relieve each joint (Kapton/hot-glue) — these pads lift if the wire flexes.
**Ignore the 7-pin `M0..M3 / GND / VDD / VPP` header** for now — its labels are non-standard and unverified (§2). The 5 gold pads are the route.

**The 5 wires** (confirm pad identity with the continuity trace in §4 before trusting this — it's 5 minutes and saves a TDI/TDO swap):

| FPGA JTAG pad (QN48 pin) | → FT232H pin |
|--------------------------|--------------|
| **TCK** (pin 9)  | **AD0** |
| **TDI** (pin 10) | **AD1** |
| **TDO** (pin 11) | **AD2** |
| **TMS** (pin 8)  | **AD3** |
| **GND** (pin 2, or any board GND) | **GND** |

**Then — device powered ON, FT232H plugged into USB:**
```bash
brew install openfpgaloader                       # once
openFPGALoader -c ft232 --detect                  # GREEN LIGHT = IDCODE 0x0120681B
openFPGALoader -c ft232 --freq 1000000 --detect   # if flaky, slow the clock to 1 MHz
```
Only after `--detect` succeeds, load the bitstream to **SRAM** (volatile, safe, reversible by power-cycle):
```bash
openFPGALoader -c ft232 -m --file-type bin fpga_bitstream/scope_bitstream_2c53t_v120.bin
```
🚫 **Never use `-f` or any flag containing `flash`** — it overwrites the FPGA's only copy of the meter design. The only safe load is `-m` (SRAM); `--detect` and `--read-register` are read-only.

The rest of this document is the full reference: safety detail (§0), package/pinout proof (§1), the M0-M3 header analysis (§2), exact MPSSE wiring (§3), the continuity-trace procedure (§4), and every openFPGALoader command incl. status readback (§5).

---

## 0. SAFETY CHECKLIST — read before connecting anything

- [ ] **SRAM ONLY. NEVER flash.** Do not use `-f` / `--write-flash` / `--external-flash` / `--user-flash`. Those write the internal NV flash = the only copy of the stock meter design. There is no undo.
- [ ] **Verify the 7-pin header with a multimeter FIRST** (Sections 3–4). The `M0..M3 GND VDD VPP` silkscreen is **non-standard and unverified** — we do not yet know if those pins are JTAG or Gowin MODE straps.
- [ ] **Leave VDD and VPP header pins DISCONNECTED from the FT232H.** Do not feed power into them. The device powers the FPGA from its own battery/USB. The FT232H provides signal reference (GND) only; share GND, not power.
- [ ] **Power the device from its own supply** (battery / USB-C), FPGA running. The FT232H is signal-only. Never back-power the board from the FT232H's VBUS/3V3 pin.
- [ ] **Confirm voltages match.** GW1N-UV2 I/O is 3.3V; CJMCU-FT232H is 3.3V I/O. Good. But confirm the header's VDD reads ~3.3V (Section 4) before trusting that assumption.
- [ ] **SRAM load is non-destructive and reversible** — power-cycle the device and the stock NV bitstream loads again. This is the safe experiment.

---

## 1. GW1N-2 / GW1N-UV2 configuration & JTAG pins

The GW1N-2 has **three** mode-select pins: **MODE[2:0] (MODE0, MODE1, MODE2)** — **there is no MODE3.** (Source: Gowin UG103 *Package & Pinout User Guide*, Table 1-3.) That alone tells you the header's "M3" label is suspicious (see Section 2).

### JTAG pins (the four you actually wire to the FT232H)
| Signal | Dir | Function |
|--------|-----|----------|
| **TCK** | in  | Test clock |
| **TMS** | in  | Test mode select |
| **TDI** | in  | Test data in |
| **TDO** | out | Test data out |

Plus relevant dedicated config pins on the FPGA:
| Pin | Function | Notes |
|-----|----------|-------|
| **JTAGSEL_N** | JTAG enable | Active-low. If used as GPIO, JTAG can disappear — pull LOW before power-up to recover JTAG (openFPGALoader Gowin note). |
| **RECONFIG_N** | reconfig trigger | Input, internal weak pull-up. Low pulse re-triggers configuration. |
| **DONE** | config done | Goes HIGH when configuration completes. Useful litmus that an SRAM load took. |
| **READY** | config ready | Status output during configuration. |

### Pin numbers — package CONFIRMED: QN48 ✅
Physically confirmed 2026-06-13 from the chip marking: **GW1N-UV2, QN48** (QFN-48, 6×6mm, 12 pads/side). The marking reads `QN48` + a grade letter (no "H"), so it is the plain **QN48**, not QN48H. **Pin 1 is the dot** at one corner of the package; QFN numbering runs counterclockwise from pin 1 (top view), down the left edge first.

**Confirmed JTAG pinout** (authoritative — Gowin **UG171** *GW1N-2 Pinout*, QN48 column):

| JTAG | QN48 pin | wire to FT232H |
|------|----------|----------------|
| **TMS** | **8**  | AD3 |
| **TCK** | **9**  | AD0 |
| **TDI** | **10** | AD1 |
| **TDO** | **11** | AD2 |
| **GND (VSS)** | **2** (also 26) | GND |

These four are **consecutive, all on the left edge** (pins 1–12 run down the left side). The board breaks them out as a **diagonal row of 5 gold test pads right next to the FPGA** (found 2026-06-13) — almost certainly TMS/TCK/TDI/TDO + GND. Solder magnet wire to those gold pads — the QFN's own pads are inaccessible (leadless).

⚠️ **QN48 vs QN48H matters — the JTAG pins are at DIFFERENT numbers:**
| JTAG | QN48 (ours) | QN48H |
|------|-------------|-------|
| TMS  | 8  | 10 |
| TCK  | 9  | 11 |
| TDI  | 10 | 13 |
| TDO  | 11 | 15 |

We have the plain **QN48** → use **8/9/10/11**. (Power pins also differ — QN48 core VCC = pin 12, QN48H = pin 37 — an electrical tiebreaker if ever needed.)

**Bench trace → wire:** with the DMM, buzz each of the 5 gold pads to find which FPGA pin it lands on, then wire per the table above — pad→pin 9 = TCK → AD0; pin 10 = TDI → AD1; pin 11 = TDO → AD2; pin 8 = TMS → AD3; pin 2 (or any board GND) = GND. No datasheet-squinting needed at the bench.

### Full QN48 config-pin map — the 5-pad target list (authoritative, UG171 QN48 column)
Every pin a gold pad could plausibly land on, so a traced "pad → pin N" is a one-line lookup:

| Signal | QN48 pin | What it means if a pad lands here |
|--------|---------:|-----------------------------------|
| **TMS** | **8**  | JTAG → wire to AD3 |
| **TCK** | **9**  | JTAG → wire to AD0 |
| **TDI** | **10** | JTAG → wire to AD1 |
| **TDO** | **11** | JTAG → wire to AD2 |
| **JTAGSEL_N** | **3** | JTAG-reconfig select; if exposed, pulling LOW guarantees JTAG is live |
| **RECONFIG_N** | **48** | **config-entry trigger candidate** — a LOW pulse re-triggers configuration. If stock pulses this at boot, it's the mechanism we've been hunting. |
| **MODE0** | **13** | the only externally-strappable config-mode bit (see below) |
| MODE1 | GND | internally grounded in QN48 → fixed 0 |
| MODE2 | GND | internally grounded in QN48 → fixed 0 |
| GND (VSS) | **2** (also 26) | shared reference |

**MODE strapping (confirmed 2026-06-13, UG171 QN48 column):** MODE1 and MODE2 are **internally tied to GND** in the QN48 package — only MODE0 (pin 13) is externally settable. So `MODE[2:0] = 0,0,MODE0` = `00X`, which per UG103 Table 1-3 is the **JTAG / Autoboot / SSPI** group. **The part is hardware-permitted to accept JTAG configuration** — a green light for the FT232H path before we even wire it. (If MODE0 also reads 0, it's `000` = JTAG + Autoboot.)

> ⚠️ Doc note: the GW1N-2 QN48 numbers above come from **UG171** (*GW1N-2 Pinout*). Do **not** read pin numbers from **UG114** — that is the **GW1N-9** pinout (different die); its JTAG numbers happen to coincide but its non-JTAG config pins do not.

### Core voltage (for sanity-checking VDD/VPP during the trace)
- GW1N-**UV** version: **VCC core = 1.8 / 2.5 / 3.3V** (UV does not use the 1.2V LV core). VCCX = 2.5/3.3V. I/O (VCCIO) typically 3.3V here. (Source: UG103 §1.2, "UV version devices support 1.8V, 2.5V, and 3.3V VCC".)

---

## 2. What do "M0–M3" and "VPP" most likely mean?

**Honest prior — this header is ambiguous and the labels are non-standard.** Two competing interpretations; the continuity trace (Section 4) decides.

**Interpretation A — they really are MODE straps (Gowin's actual scheme):**
Gowin genuinely uses **MODE[2:0]** strap pins to pick config mode. For GW1N-2/QN48 the relevant strap table is (UG103 Table 1-3):
| MODE[2:0] | Modes enabled |
|-----------|---------------|
| `00X` (QN48/QN48H) | JTAG / Autoboot / SSPI |
| `000` | JTAG + Autoboot |
| `XXX` (QN88) | JTAG / I2C / Autoboot / SSPI / MSPI / DUAL BOOT / SERIAL / CPU |

Note: there are only **three** mode bits. **If the header has a real "M3", it is NOT a Gowin mode pin** — it is the board designer's own net name (e.g. could be a 4th unrelated net, a duplicate, or a JTAG line relabeled). This is a strong tell.

**Interpretation B — the silkscreen is FNIRSI's own net names and these are actually the JTAG signals.** Very plausible: a 7-pin "FPGA programming header" with power that a factory uses is far more likely to be **TCK/TDI/TDO/TMS + GND + VDD + (VPP)** than four mode straps (you don't normally break out all of MODE on a programming header — straps are usually just resistors). "M0..M3" could be the designer's shorthand for the four JTAG nets.

**On "VPP": GW1N FPGAs have no VPP pin.** Gowin's GW1N config scheme uses VCC / VCCX / VCCIO — there is **no dedicated flash programming-voltage (VPP) pin** in the datasheet. So "VPP" is almost certainly either (a) the board designer's own label for something, or (b) a programming-enable / VCCX rail. **Do not assume it needs an external voltage; do not drive it.** Measure it (Section 4).

### Decision rule for the trace
- If a header **M-pin buzzes to an FPGA JTAG ball (TCK/TDI/TDO/TMS)** → it's a **JTAG line** → wire it to the matching FT232H pin. (Interpretation B confirmed.)
- If a header **M-pin buzzes to an FPGA MODE0/1/2 ball** → it's a **strap** → do NOT wire it to JTAG data; it just selects mode. You'd need to find JTAG elsewhere (test points / FPGA balls directly).
- If "M3" buzzes to nothing recognizable / a JTAG ball → confirms the label is loose.

---

## 3. FT232H → JTAG wiring (standard MPSSE assignment)

openFPGALoader cable name for a bare FT232H board: **`ft232`** (one MPSSE interface). Standard FTDI MPSSE JTAG pinout:

| FT232H (CJMCU silkscreen) | MPSSE | JTAG signal | → FPGA / header pin |
|---------------------------|-------|-------------|---------------------|
| **AD0 / D0**  | TCK out | **TCK** | header pin that buzzed to FPGA TCK |
| **AD1 / D1**  | TDI out | **TDI** | → FPGA TDI |
| **AD2 / D2**  | TDO in  | **TDO** | ← FPGA TDO |
| **AD3 / D3**  | TMS out | **TMS** | → FPGA TMS |
| **GND**       | —       | **GND** | header GND (shared reference) |

Wiring notes:
- **Share GND only.** Connect FT232H GND ↔ board GND. This is mandatory for a valid signal reference.
- **Do NOT connect VDD or VPP** from the header to the FT232H. The board powers its own FPGA; the FT232H is signal-only. (The CJMCU-FT232H also has a 3V3/VBUS pin — leave it out of this circuit; do not back-feed the board.)
- **I/O level:** CJMCU-FT232H drives 3.3V logic; GW1N-UV2 I/O is 3.3V → direct connect is fine. No level shifter needed.
- Keep JTAG wires short (TCK is a clock). If detection is flaky, drop the clock with `--freq 1000000` (1 MHz).

---

## 4. Continuity-trace procedure (no soldering)

Device **powered OFF, battery disconnected** for the continuity steps.

**Step A — find GND, VDD, VPP (power pins first):**
1. DMM in continuity mode. Find a known chassis/board GND (USB-C shield, a large ground pour, MCU GND pin).
2. Buzz each of the 7 header pins to that GND. The pin that beeps = **GND**. (Confirms the "GND" silkscreen, and orients the header.)
3. The "VDD" pin should read continuity to the FPGA's VCC/VCCIO rail and to the board 3.3V net. Mark it; **do not wire it**.
4. "VPP": buzz to FPGA VCCX and to 3.3V/other rails. Record what it connects to. **Do not wire it.**

**Step B — powered voltage check (device ON):**
5. Power the device normally. DMM in DC volts, black on GND.
6. Measure "VDD" → expect ~**3.3V**. Measure "VPP" → record (likely 3.3V or a config rail; if it floats near 0 it may be a strap/enable).
7. Measure each "M" pin idle voltage → a clean HIGH (~3.3V) or LOW (~0V) that doesn't move suggests a **strap**; a pin that's mid-rail or toggles suggests a **signal line** (possible JTAG). Power off again before the next step.

**Step C — identify the M-pins (device OFF):**
8. From the datasheet pin map for your confirmed package, locate the FPGA's **TCK, TDI, TDO, TMS** balls and the **MODE0/1/2** balls.
9. Buzz each header "M" pin to each of those FPGA balls. Apply the **decision rule** in Section 2.
10. Record the full mapping using the template in `docs/design/fpga_programming_investigation.md` §Step 5.

**Open questions the trace must answer (write these down):**
- Are the 4 "M" pins JTAG (TCK/TDI/TDO/TMS) or MODE straps?
- Since GW1N-2 has only 3 MODE pins, what is "M3" actually? (4 JTAG nets would explain a 4th "M".)
- What rail is "VPP" — VCCX, a program-enable, or unused?
- Is JTAGSEL_N tied so JTAG is live, or does it need a pulldown before power-up?
- What is MODE[2:0] strapped to (does it permit JTAG config — `00X` does on QN48)?

---

## 5. openFPGALoader commands (macOS)

Install: `brew install openfpgaloader`. List cables to confirm the FT232H string: `openFPGALoader --list-cables` (expect `ft232`).

### 5a. Detect / scan the JTAG chain (the green light)
```bash
openFPGALoader -c ft232 --detect
```
**Success = IDCODE `0x0120681B`** printed (GW1N-2 family). That single result confirms: header pins are JTAG, wiring is right, JTAGSEL_N is enabled, and the FPGA is talking. If detection fails, see §5e.

Slow the clock if flaky:
```bash
openFPGALoader -c ft232 --freq 1000000 --detect
```

### 5b. Load bitstream into SRAM (volatile, SAFE — this is the experiment)
SRAM is the **default** destination in openFPGALoader (no flag = SRAM). `-m` forces memory/SRAM explicitly:
```bash
openFPGALoader -c ft232 -m scope.fs
```
- `-m` = load to **SRAM** (RAM). Volatile: gone on power-cycle → the stock NV image reloads. **This is the only load you should run.**
- Lower the clock if the load doesn't take: add `--freq 1000000`.
- Watch for **DONE** going HIGH (and your bench behavior) to confirm the design ran.

**Bitstream file format / our raw blob:**
- openFPGALoader expects a Gowin **`.fs`** by default (deduced from extension). Our bitstream is a **raw 115,639-byte blob** extracted from firmware (`reverse_engineering/.../fpga_cal_table.h`, or raw at **file offset `0x4AD19`** of `APP_2C53T_V1.2.0_251015.bin`). The raw blob already carries the Gowin `.fs` preamble + IDCODE `0x0120681B`.
- A raw binary can be loaded by forcing the type:
  ```bash
  openFPGALoader -c ft232 -m --file-type bin scope.bin
  ```
  `--file-type` overrides extension-based detection. **Caveat to verify on the bench:** a `.fs` is an ASCII bitfile, while this is the *binary* config stream. If `--file-type bin` is rejected or loads garbage, the cleaner path is to obtain/rebuild a proper `.fs` from GowinEDA, or convert. Start with `--detect` (no file) to prove the chain before worrying about file format.
- **Pre-staged files** (verified: 115,638 B, sha256 `5a0e7338…`, preamble + IDCODE `0x0120681B`):
  - `fpga_bitstream/scope_bitstream_2c53t_v120.bin` — the raw on-wire config stream (**primary**; use with `-m --file-type bin`).
  - `fpga_bitstream/scope_bitstream_2c53t_v120.fs` — a hand-built ASCII `.fs` fallback (round-trips to the same sha256). **Bit order (MSB-first/byte) is UNVERIFIED until the bench** — if loading the `.fs` gives a CRC/IDCODE error that the raw `.bin` does not, the bit order is wrong; regenerate with `fpga_bitstream/make_fs.py`. Order of preference at the bench: raw `.bin` first, this `.fs` second, GowinEDA `.fs` if both fail.

### 5c. Read config status / readback (diagnose the config-reject)
After (or instead of) a load, query the FPGA status register to see why config is rejected:
```bash
openFPGALoader -c ft232 --read-register STATUS
```
(GW1N supports the JTAG status register read; the status word's error bits — CRC error, IDCODE mismatch, etc. — are decoded in the Gowin programming guide UG290. Exact subcommand may be `--read-register` / `--read-statusreg` depending on your openFPGALoader version — check `openFPGALoader --help`.) This is the direct way to find out *why the MCU's SSPI upload was rejected* (e.g. per-frame CRC vs IDCODE mismatch).

### 5d. Commands to AVOID — DANGER (writes internal NV flash)
**Never run any of these — they overwrite the only copy of the stock meter design:**
```
# DO NOT RUN — these write the FPGA's internal NV flash:
openFPGALoader -c ft232 -f  scope.fs          #  -f / --write-flash  = FLASH. NO.
openFPGALoader -c ft232 --write-flash ...      #  FLASH. NO.
openFPGALoader -c ft232 --external-flash ...   #  external SPI flash. NO.
openFPGALoader -c ft232 --user-flash ...       #  user flash. NO.
```
**Rule of thumb: if the flag has the word `flash` in it, or it's `-f`, STOP.** The only safe destination flag is `-m` (SRAM), and `--detect` / `--read-register` (read-only).

### 5e. If `--detect` fails
- Re-check wiring (TDO is an FT232H *input* on AD2; a TDI/TDO swap is the classic mistake).
- Confirm GND is shared and the device is powered (FPGA must be up).
- JTAGSEL_N: if used as GPIO, JTAG is hidden — may need it pulled LOW before power-up.
- Lower `--freq`. Try `--list-cables` to confirm the FT232H enumerated. On macOS, the Apple FTDI driver can claim the device — if so, unload it or use the libusb path openFPGALoader ships with.
- If the M-pins turned out to be **MODE straps not JTAG** (Section 4), there's nothing to detect on this header — JTAG must be picked up elsewhere (FPGA balls / test points).

---

## Sources
- Gowin **DS100** GW1N Series Data Sheet — JTAG/config pins, GW1N-2 core/POR voltages.
- Gowin **UG103** GW1N Series Package & Pinout User Guide — Table 1-3 (GW1N-2 MODE[2:0] config modes; confirms 3 mode pins), UV-version VCC = 1.8/2.5/3.3V, per-package QN48/QN88 pin maps.
- Gowin **UG290** FPGA Products Programming and Configuration Guide — status register bits, RECONFIG_N, SSPI/MSPI modes.
- openFPGALoader docs — `-c ft232` (FT232H), `-m` SRAM (default) vs `-f` flash, `--detect`, `--file-type`, JTAGSEL_N pulldown note: <https://trabucayre.github.io/openFPGALoader/vendors/gowin.html>, <https://trabucayre.github.io/openFPGALoader/compatibility/cable.html>
- Repo priors: `docs/design/fpga_programming_investigation.md` (candidate JTAG pin numbers, trace template), `reverse_engineering/HARDWARE_PINOUT.md` §"FPGA Programming Header", `reverse_engineering/captures/SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md` (bitstream offset `0x4AD19`, IDCODE).
