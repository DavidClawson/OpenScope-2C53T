# Bench Day — SPI3 / ESP32 / FT232H FPGA bring-up (single-page checklist)

One page to keep in front of you while soldering. Combines the two routes
(ESP32 over SSPI, FT232H over JTAG) into one run-order checklist.

- **Target:** FNIRSI 2C53T, FPGA = **Gowin GW1N-UV2 (QN48)**, IDCODE `0x0120681B`.
- **Question we're answering:** is our bitstream itself good (→ **JTAG oracle**), or is only the **SSPI config-entry** handshake broken (→ **ESP32 iterate**)?
- **Branch:** `experimental/esp32-bringup` (this doc, the ESP32 sketch, and `make guest-bringup` only exist here — not on `main`).
- Full detail lives in [`gowin_jtag_programmer_guide.md`](gowin_jtag_programmer_guide.md) (JTAG) and [`../tools/esp32_sspi_bringup/README.md`](../tools/esp32_sspi_bringup/README.md) (ESP32). This is the condensed run sheet.

---

## ⛔ Safety — read once before connecting anything

- **SRAM ONLY on JTAG. NEVER flash.** No `-f` / `--write-flash` / `--external-flash` / `--user-flash`. The FPGA's internal NV flash holds the **only copy of the stock meter design** — no undo. The only safe load flag is `-m` (SRAM, volatile); `--detect` / `--read-register` are read-only.
- **Share GND only — never feed power in.** The scope powers its own FPGA from battery/USB-C. The ESP32 and FT232H are signal-only; do **not** wire their 5V/3V3 into the board.
- **Continuity-trace the JTAG pads before trusting the table** (Step 2). A TDI/TDO swap is the classic time-waster.
- **SRAM load is reversible** — power-cycle and the stock NV image reloads. This is the safe experiment.

---

## 🧰 Gear

- ESP32 dev board (native **3.3 V** — *not* a 5 V Uno without level shifting)
- **CJMCU-FT232H** (single-channel FT232H, MPSSE, 3.3 V I/O)
- Magnet wire, **30–34 AWG**
- Small **breadboard** + jumpers (its real job: a **common ground rail** + tidy fan-out)
- **Powered USB hub** (so ESP32 + FT232H + the scope's USB-C are all live at once — this is the mars box's hub)
- Multimeter (continuity + DC volts)
- Fine-tip iron, magnifier/loupe, Kapton tape or hot glue (strain relief)
- Host: `brew install openfpgaloader`; Arduino IDE with the ESP32 board package

---

## 🔌 The two pad groups — DON'T conflate them

They're different nets, which is exactly why both programmers can be connected at once.

### Group A — back-side SPI3 pads → **ESP32** (SSPI route)
maksidze's reverse-side breakout. Has a usable GND.

| Board pad (MCU pin) | → ESP32 GPIO |
|---------------------|--------------|
| **SCK** (PB3)  | GPIO18 |
| **MOSI** (PB5) | GPIO23 |
| **MISO** (PB4) | GPIO19 |
| **CS** (PB6, active LOW) | GPIO5 |
| **GND** | ESP32 GND (+ breadboard rail) |

### Group B — 5 gold pads next to the FPGA → **FT232H** (JTAG route)
Diagonal row of gold pads. **Confirm with the continuity trace (Step 2) before wiring.**

| FPGA JTAG pad (QN48 pin) | → FT232H |
|--------------------------|----------|
| **TCK** (pin 9)  | AD0 |
| **TDI** (pin 10) | AD1 |
| **TDO** (pin 11) | AD2 |
| **TMS** (pin 8)  | AD3 |
| **GND** | from the breadboard rail |

> ⚠️ **The 5th gold pad is `3V3`, NOT GND.** Leave it unconnected — the FT232H gets its ground from the shared breadboard rail (tied to the SPI3 GND), not from this pad. Easy to get wrong at 11 pm.

**Tie all grounds together** on one breadboard rail: SPI3 GND + ESP32 GND + FT232H GND. That shared reference is mandatory for both buses.

---

## ✅ Run order

### Step 1 — Solder
Magnet wire to **Group A** (SCK/MISO/MOSI/CS + GND) and **Group B** (the 5 gold pads).
Scrape/tin the enamel first; strain-relieve every joint (pads lift if the wire flexes).

### Step 2 — Continuity-trace the gold pads (device OFF, battery out)
DMM in beep mode. For each gold pad, find which FPGA pin it lands on:
- Which pad beeps to **GND**? (orient the group)
- Map the other four to FPGA pins **8/9/10/11** → TMS/TCK/TDI/TDO per the table.
- Bonus: if a pad goes to **pin 48 (RECONFIG_N)** or **pin 3 (JTAGSEL_N)**, note it — that's config-mode control, even more interesting.

### Step 3 — Power up & sanity check
Power the scope normally (its own USB-C / battery). Plug the FT232H and ESP32 into the powered hub. Confirm the FT232H enumerates: `openFPGALoader --list-cables` (expect `ft232`).

### Step 4 — JTAG oracle FIRST (FT232H) — "is the bitstream good?"
```bash
openFPGALoader -c ft232 --detect                  # GREEN LIGHT = IDCODE 0x0120681B
openFPGALoader -c ft232 --freq 1000000 --detect   # if flaky, slow to 1 MHz
```
Then SRAM-load (volatile, safe):
```bash
openFPGALoader -c ft232 -m --file-type bin ../fpga_bitstream/scope_bitstream_2c53t_v120.bin
```
- **Scope comes alive** ⇒ bitstream is good, the problem is **purely SSPI entry**. Huge.
- Watch **DONE** go HIGH. Diagnose a reject with `openFPGALoader -c ft232 --read-register STATUS`.
- 🚫 never a flag with `flash` in it.

### Step 5 — ESP32 SSPI iterate — "can a clean external master enter config?"
**Hand it the bus first.** Flash this branch and build **`make guest-bringup`** (boots straight into bus-released + USART-silent; factory-bootloader unit #2 → use `guest`). Or plain `make guest` then type `fpga busrelease` in the USB debug shell. This tri-states MCU PB3/PB5/PB6 so the ESP32 owns the bus.

Flash `tools/esp32_sspi_bringup/esp32_sspi_bringup.ino`, open serial @ **115200**:
```
clk 400          # START SLOW — IDCODE is garbage at 60 MHz, correct at ~470 kHz
id               # read IDCODE — success = 01 20 68 1B (config port answers at all)
enable           # send config-enable prelude (05 / 12 / 15)
status           # read status — stock's healthy value is 00 01 42 2E 2E
raw 03 00 ...    # arbitrary CS frame, prints MISO echo — sweep framings
```
**What we're hunting:** `id` → `01 20 68 1B` means the external master gets a response our MCU never did. Then sweep `clk` + `enable`/`raw` framing to see if `status` moves off all-`FF` toward `00 01 42 2E 2E` / a clean `0x3A` close. Either outcome is decisive — success points the recipe back into `fpga.c`; same-wall failure rules SSPI out and crowns JTAG.

---

## Notes
- **No un-release command** — power-cycle or re-flash to give the bus back to the MCU.
- Keep JTAG wires short (TCK is a clock). On macOS the Apple FTDI driver can claim the FT232H — unload it if `--detect` can't see the device.
- This whole rig is **bench-validation-pending** — nothing here is confirmed-working yet. Record what you actually see (even just the `id` result); that's the data.
