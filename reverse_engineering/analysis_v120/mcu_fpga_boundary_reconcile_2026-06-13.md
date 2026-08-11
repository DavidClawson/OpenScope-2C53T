# Fourth reconciliation: the MCU↔FPGA boundary, closed across three independent estimates

**Date:** 2026-06-13
**Sources reconciled:**
- **ripcord** (MCU emulation) — execution-verified SPI3/USART command behavior, contract #19.
- **osc** (MCU clean-room + bench) — `HARDWARE_PINOUT.md`, `firmware/src/drivers/fpga.c`,
  `FIRMWARE_FINDINGS_2026_05_30.md` (silicon: MISO inert, PC0 stuck high, "one buffer then stop").
- **apicula** (FPGA netlist) — `gw1n2-apicula/tools/m5/scope_unpacked.v` unpacked stock design;
  `tools/m_arming.py` trace (re-run 2026-06-13); `docs/R3-CAPTURE-ARMING-FINDINGS.md`.

**Why this matters:** the previous three reconciliations were all MCU-side. The genuine
ceiling — *what turns the FPGA's SPI3 data output on* (the open ask in `FIRMWARE_FINDINGS`) —
is an **FPGA-internal** question, unreachable from MCU analysis. Apicula answers it from the
netlist; this note bridges that answer to the MCU pins, producing **one falsifiable,
bench-testable arm-the-capture procedure.**

---

## 1. The headline: MISO-inert is explained, and it is not a wiring fault

`FIRMWARE_FINDINGS` reported MISO (PB4) **never driven low**, across boot handshake, the
115638-byte blob, and every command byte. The apicula netlist trace explains it exactly:

- The sample readback (`0x04`/`0x05`) clocks out on **`SO` (`IOB5B`, pin 17) — the only IOBUF
  in the entire design.** Its `SO.OEN` (output-enable) is **gated by the read-window logic.**
- That read-window logic is produced by a **~383-DFF free-running address/state counter.** The
  `m_arming.py` trace confirms the counter's cone depends on three primary inputs:
  **`IOR1B` (pin 35, run/re-arm), `IOB7B` (pin 19), `SI` (`IOB18B`, pin 24).**
- **The counter only advances while the run/re-arm input is in the run state.** Halt it →
  the counter completes one window and stops → `SO.OEN` never re-asserts → **PB4 reads idle
  `0xFF` forever.** That is the "captured one buffer then stopped" symptom, precisely.

**So MISO is inert because the FPGA capture engine is not running — not because PB4 is
mis-wired or the FPGA ignores SPI.** osc's custom firmware never drives the run/re-arm line
because, until apicula traced the fabric, nobody knew it was a separate pin.

---

## 2. The pin map — MCU (ripcord/osc) ↔ FPGA pad (apicula), matched by function

The MCU's runtime "SPI3" bus **is the GW1N SSPI configuration port repurposed as user I/O.**
Both sides are an unambiguous 4-wire SPI; the match is one-to-one:

| MCU pin (osc `HARDWARE_PINOUT`) | role | FPGA pad (apicula) | special fn | confidence |
|---|---|---|---|---|
| PB3 | SPI3 SCK | `IOB5A` (pin 16) | SCLK | **high** (function-unique) |
| PB5 | SPI3 MOSI | `IOB18B` (pin 24) | **SI** | **high** — and one SI bit feeds the capture-enable |
| PB4 | SPI3 MISO | `IOB5B` (pin 17) | **SO** | **high** — sole IOBUF = the `0x04`/`0x05` readback osc's bench confirmed |
| PB6 | SPI3 CS (active-LOW) | `IOB18A` (pin 23) | CS_N (SSPI_CS_N) | **high** |
| PA2 | USART2 TX | (top-edge IOT, unbonded on proxy) | — | med — separate command bus, not the capture engine |
| PA3 | USART2 RX | (top-edge IOT) | — | med |
| **PB11** | **"FPGA active mode" (HIGH in active measurement; tied to `FUN_08037800`)** | **`IOR1B` (pin 35)** | — | **hypothesis (ranked #1)** — see §3 |
| PC6 | "FPGA SPI enable" (must be HIGH for SPI3 to talk) | `IOB7B` (pin 19) | GCLKC_4 | hypothesis (ranked #1 for the secondary enable) |

The SPI bus rows are function-near-certain. The two dedicated control rows (PB11/PC6) are
**inference, not a board trace** — see the resolution and the test below.

---

## 3. The one open bridge: which MCU pin is the FPGA run/re-arm (`IOR1B`)?

Apicula's master run/re-arm is a **dedicated, non-SPI pad** (`IOR1B`, pin 35: 70× DFF.CE +
8× DFF.SET + 156× DFF.D). The MCU has exactly **two** dedicated FPGA-control GPIOs that are
not SPI3 or USART2: **PB11** and **PC6**. So `{PB11, PC6}` maps to `{IOR1B, IOB7B}`.

**Ranked resolution (by semantics — to be confirmed on the bench, not proven here):**
- **`IOR1B` (run/re-arm) ← PB11.** osc documents PB11 as *"FPGA active mode, HIGH during
  active measurement,"* tied to the acquisition function `FUN_08037800` — the cleanest match
  to "master run-enable."
- **`IOB7B` (secondary enable, GCLKC_4) ← PC6.** osc documents PC6 as *"must be HIGH for SPI3
  to communicate"* — an enable that gates the readback path.

Why this stayed hidden: osc **does** set PB11 HIGH, but only as a static boot-time level. The
apicula trace shows re-arm runs through the **8× DFF.SET (async preset)** fan-out — a
*pulse/re-issue* path, not a static level. A held-HIGH PB11 satisfies "active mode" but may
never re-pulse the SET nets that restart the counter after the first window.

---

## 4. Two gates in sequence — prior bench tests touched the wrong one

| gate | what it is | evidence | status |
|---|---|---|---|
| **(1) Config entry** | get the GW1N to (re)enter config so the user design boots from the bitstream | `CONFIG_ENTRY_REPLY_FROM_APICULA`: blocker is the **reconfiguration trigger** (RECONFIG_N low ≥25 ns or power cycle), **not** FLASH_LOCK; JTAG SRAM load bypasses it | open — config close status varied F8/FC/00 |
| **(2) Runtime arm** | once the design runs, start/sustain capture so MISO returns data | this note + R3: drive `IOR1B` (PB11?) to run **and** re-issue the SPI control-register sequence; `SO.OEN` gates MISO on the read window | open — never attempted (run line unknown until now) |

`FIRMWARE_FINDINGS`' "PB11 already HIGH still yields 0/115638 non-FF" tested gate **1** (during
blob upload). The IOR1B finding is about gate **2** (post-config). They are independent; gate 2
can't even be observed until gate 1 lets the user design boot. **The warm-handoff build already
in `fpga.c` (`FPGA_WARM_HANDOFF_TEST`) is the correct isolator** — it boots stock (gate 1 passes,
design alive in SRAM), reflashes without power-cycle, and tests the runtime read path alone.

---

## 5. Falsifiable bench recipe (the deliverable)

Run on the warm-handoff build (stock-configured FPGA still alive), so gate 1 is satisfied:

1. Drive **PB11 → run state** (the `IOR1B` candidate). If a single static level doesn't arm,
   **pulse** it (exercise the 8× DFF.SET re-arm path), not just hold it HIGH.
2. **Re-issue the stock post-config SPI control-register write** over SCLK/SI/CS_N exactly as
   stock does: `01 08 / 02 03 / 06 00 / 07 00 / 08 AD` (one bit of this register feeds the
   capture-enable).
3. Read back with `spi3 acqread` (opcode `0x04` CH1 / `0x05` CH2, ~1025 bytes).

**Predicted outcomes:**
- `span > 0` on CH1/CH2 → **IOR1B ← PB11 confirmed**, the run/re-arm model holds, gate 2 cracked.
- Still all-`0xFF` with PB11 pulsed + register re-issued → IOR1B is **not** PB11; swap to PC6
  and repeat. If neither arms it, the run line is one of the **unbonded top-edge IOT pads**
  (the "MCU bus" group apicula can't pin-number on the proxy) and needs a board-trace.

### BENCH RESULT (2026-06-13, Unit 2) — both MCU candidates falsified

Ran `spi3 armtest` / `spi3 armtest pc6` (new `usb_debug.c` command) on a **clean**
warm handoff (stock → live scope trace → straight-through soft reset into
`guest-warmtest`). The handoff was confirmed good: the PB11-run baseline acqread
returned the stale one-shot buffer (CH1 `AE`=174 / CH2 `53`=83, matching the
warm-handoff doc's Round-1 DC levels), re-validating the `0x04`/`0x05` read path.

- **PB11 pulse + control-reg re-issue → no re-arm** (after-pulse span 0, both ch).
- **PC6 pulse + control-reg re-issue → no re-arm** (after-pulse span 0, both ch).
- Read *window* still fires (CH1 status `80 00 00`, PC0 1→0) — readback alive, no
  fresh capture behind it.

So `IOR1B ← {PB11, PC6}` is **falsified** for an edge-pulse + control-reg re-arm.
Resolution lands on the fallback branch: the run/re-arm line is an **unbonded
top-edge IOT pad** (needs a board trace) and/or run-state is non-re-establishable
from the MCU post-soft-reset. **JTAG** (fresh config, no MCU reset) is the unlock;
`spi3 armtest` is retained as a probe for that session. Full log: warm-handoff doc
RESULTS Round 4.

---

## 6. Honest caveats

- **Function-match, not continuity.** The SPI3↔SSPI rows are near-certain by role; the
  PB11/PC6 → IOR1B/IOB7B rows are **ranked hypotheses**. A board trace (or the §5 differential
  test) is what promotes them.
- **Apicula's roles are static-structural** — fan-out *kind* (CE/SET/D) + LUT INIT decode, not
  a simulation. Treat the "drive HIGH = run" **polarity** as a bench hypothesis.
- **Proxy pin numbers (QFN48XF) may not equal the scope's package.** The reliable, package-
  independent handles are the **apicula IOB names** (`IOR1B`, `IOB5A/B`, `IOB18A/B`, `IOB7B`)
  and their **special functions** (SI/SCLK/SO/CS_N) — cross-check the board against those.
- This does **not** crack the FPGA's internal acquisition logic — it defines the **boundary
  contract** (which pins arm/clock/read it), which is the actual goal: a custom firmware that
  makes the stock FPGA acquire.

---

## 7. Provenance of each claim

| claim | provenance |
|---|---|
| MISO = sole IOBUF `SO`, OEN gated by read-window counter | apicula netlist trace (`m_arming.py`, `scope_unpacked.v`) — static-structural |
| capture counter cone depends on IOR1B + IOB7B + SI | apicula `m_arming.py` 2026-06-13 (re-run) — static-structural |
| `0x04`/`0x05` clock CH1/CH2 buffers out on SO | apicula + **osc bench confirmed** (warm-handoff read test) |
| PB3/5/4/6 = SPI3 SCK/MOSI/MISO/CS | osc `HARDWARE_PINOUT` + ripcord exec (contract #19, PB6 CS execution-verified) |
| PB11 = active mode, PC6 = SPI enable | osc `HARDWARE_PINOUT` (static decode) |
| PB11 ↔ IOR1B, PC6 ↔ IOB7B | **hypothesis** (semantic ranking) — §5 test pending |
| config blocker = reconfig trigger, not FLASH_LOCK | apicula `CONFIG_ENTRY_REPLY` (UG290-2.9E) |
