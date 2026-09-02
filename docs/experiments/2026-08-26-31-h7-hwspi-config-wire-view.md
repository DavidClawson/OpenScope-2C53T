# EXP-31 (H7) — wire view of the failing hardware-SPI config on an anchored-open port

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-bringup` (bus-released boot, `FPGA_USART_SILENT_SCOPE`)
  plus the new `fpga_spi3_bus_reacquire()` / `fpga busreacquire` path.
- **Instrument:** HiLetgo fx2lafw 24MHz clone, 4 ch @ 4 MS/s, CS-falling trigger.
  D0=SCK(PB3) D1=MISO(PB4) D2=MOSI(PB5) D3=CS(PB6), GND from the SWD header.
- **Capture:** `reverse_engineering/captures/h7_hwspi_config_openport_2026-08-26.sr`
- **Status:** CLOSED — the SPI3 transport is byte-perfect on the wire; the wall is
  definitively **NOT** on the SPI3 bus. Confirms the "firmware search exhausted"
  conclusion (EXP-29/30) at the physical layer.

## 1. Why this build

Every prior wall reading was taken on a build that ran the config *at boot*, which
(a) is a one-shot event nearly impossible to align to a truncating analyzer and
(b) kills CDC (blocks `fpga_init` before USB enumerates). `guest-bringup` boots
into a **pristine, config-receptive FPGA** (it runs `fpga_bus_release()` and
returns — no config, no acq task flooding the bus), and because it runs no config
it **keeps CDC** (verified: `/dev/ttyACM0`, `2e3c:5740`, after one `-71` replug).
That gives a shell to fire the failing prelude on demand into an armed LA window —
repeatable, no boot-race.

New this session (fpga.c / usb_debug.c):
- `fpga_spi3_bus_reacquire()` — faithful mirror of the `fpga_init` SPI3 block:
  PB3/5 → AF, PB4(MISO) → input pull-up (stock idle), PB6 → CS out HIGH, CTRL1/2 +
  SPE, PC6 HIGH, clears `bus_released`.
- `fpga busreacquire` shell verb; `fpga reinit` auto-reacquires if the bus was
  released. "reinit" means "take the bus and configure", so this is the correct
  home for it.

## 2. Procedure

True FPGA power cycle (POWER → Goodbye → unplug USB → replug → POWER) so the part
is genuinely cold, then:

```
fpga busreacquire
spi3 gowin                       # ANCHOR
fpga reinit 7 100 600 pe k7      # prelude+upload+close all at /256, read wall
```

`k7` forces the command phase to /256 (~444 kHz on the wire) so the 24 MHz clone
can sample it; `pe` reads STATUS(0x41) at /256 right after 0x15 (the SYSTEM_EDIT_
MODE wall test). The LA was armed with a CS-falling trigger so the capture aligns
to the first config frame.

## 3. Anchor (mandatory, EXP-J)

```
[/256] IDCODE(0x11): 0x0120681B == GW1N-2 OK (read path ANCHORED)
       STATUS(0x41): 0x00039020  [MemErase][GowinVLD][Ready][POR][FLASHlock]
```

Read path valid, part cold/unconfigured/receptive — the state stock faces at boot.

## 4. Digital wall (shell)

```
0x3A close:        00                (stock F8)
0x41 STATUS:       00039020          GWVLD READY POR
post-0x15 STATUS:  00039020   EDIT_MODE(bit7)=no (0x15 did NOT engage)
acqread 04/05:     00 01 C8 10 …     (free-running status pattern = unconfigured)
```

The wall, reproduced on a confirmed-open port with valid reads throughout — the
strongest form we have. CONFIG_ENABLE does not engage config mode. No error bits
(no CRC/BAD_CMD/ID_FAIL): the bytes are not *rejected*, config entry never happens.

## 5. Wire view (the new data)

Aligned by transfer index (MOSI = MCU→FPGA, MISO = FPGA→MCU):

```
MOSI:  05  12  15  … 3B  FF FF FF … FF  A5 C3 06 …   (Gowin .fs preamble + payload)
MISO:  00  90  00  … 00 03 90 20  00 03 90 20  …     (status word, streaming)
```

CS framing / timing (measured from the raw capture):

| frame | t (ms) | dur | meaning |
|---|---|---|---|
| 0 | 19.1  | 34.8 µs | `05 00` ERASE_SRAM |
| 1 | 129.2 | 34.8 µs | `12 00` (+110 ms = 100 ms gap + frame) |
| 2 | 239.3 | 34.8 µs | `15 00` CONFIG_ENABLE (+110 ms) |
| 3 | 239.4 | 140 µs  | `pe` STATUS(0x41) probe |
| 4+ | 239.5→ | — | 0x3B bulk upload (capture truncates mid-upload) |

- **SCK = 444 kHz**, clean, mode-3, 205 k edges — exactly the intended /256.
- Each prelude command is **cleanly CS-split** with the intended 100 ms gaps.
- MOSI carries the **correct** bytes: 05/12/15, then 3B and the real Gowin
  bitstream (`FF…FF A5 C3` preamble). No corruption, no missing bytes, no bit-order
  error — the transport is byte-perfect on the physical wire.
- MISO is driven the whole time (FPGA is powered + listening), but is **entirely**
  the status register content: `00 90 00` during 05/12/15 are bytes of `00039020`;
  the steady stream is `00 03 90 20` (and its one-bit-early alias `80 01 C8 10`,
  EXP-I). The FPGA **never** emits a config ack, error code, or state change in
  response to any config command.

## 6. Conclusion

- **Established:** on the physical wire the hardware-SPI config is byte-perfect —
  correct bytes, correct mode-3 clocking, correct CS-split framing, correct 100 ms
  gaps — and the FPGA is powered, clocking, and actively driving MISO. It receives
  a flawless `05 → 12 → 15` on an anchored-open, config-receptive part and does not
  enter configuration.
- **Excluded (now at the wire level, not just by register snapshot):** any SPI3
  transport defect — bit order, timing, corruption, framing, dropped bytes, dead/
  Hi-Z MISO. The FPGA hears us perfectly and declines.
- **The differentiator is definitively OFF the SPI3 bus.** This matches apicula
  exactly (`docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md`): an auto-booted GW1N re-enters
  configuration only via RECONFIG_N (≥25 ns low pulse) or a power cycle — never from
  CONFIG_ENABLE on the SSPI wire alone. A power cycle was already refuted for us
  (Exp R). So the live lead is an **off-SPI3 co-signal stock asserts near
  CONFIG_ENABLE that our firmware does not** — the same thread Exp G/B2 left open.

## 7. Blind spots / next

- The LA watched only the 4 SPI3 lines. The co-signal, by definition, is on a pin
  we did **not** probe. Next capture must add candidate control lines (RECONFIG_N
  route) alongside SPI3.
- **Cheapest decisive next step (issue #18):** is RECONFIG_N even wired to the MCU,
  and to which pin? If known, assert it before/around CONFIG_ENABLE. Exp T (PC9)
  was negative but position-dependent (pulsed pre-prelude, not at stock's position).
- A **stock** boot LA capture would show the co-signal directly. Stock clocks SPI3
  at /2 (60 MHz, beyond this analyzer), but a RECONFIG_N-class co-signal is a slow
  GPIO level/pulse and is capturable at 24 MHz on the non-SPI channels.
- One unit. `k7` runs the prelude at /256; stock writes at /2. The wall is
  identical at both rates (EXP-26 and the /2 history), so rate is not implicated,
  but the wire capture itself is a /256 observation.
