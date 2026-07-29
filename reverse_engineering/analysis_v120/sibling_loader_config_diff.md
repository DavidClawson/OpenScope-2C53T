# Config-entry diff: our firmware vs working sibling + openFPGALoader

**Date:** 2026-06-13
**Purpose:** Step-back diff of our FPGA SSPI config-upload path against two external references that *work*, to find anything we've never tried. Both were cloned locally for this analysis (not submodules):
- `rosenrot00/OpenScope-2C23T` → `/Users/david/Projects/RESEARCH/OpenScope-2C23T` — working custom firmware for the sibling device (same GW1N-2 family, IDCODE 0x0120681B).
- `trabucayre/openFPGALoader` → `/Users/david/Projects/RESEARCH/openFPGALoader` — canonical Gowin programmer; `src/gowin.cpp` `writeSRAM()` is the authoritative SSPI/JTAG SRAM-load recipe (TN653 p.9).

## TL;DR — two things we have NEVER done

1. **We never read the Gowin STATUS_REGISTER (opcode `0x41`).** We capture the `0x3A` close-return byte and a `0x03` *scope* status, but never the authoritative config status register. The sibling reads `0x41` and uses it as the success signal; openFPGALoader reads it and decodes named bits. **This register would tell us EXACTLY why config fails** instead of us guessing at `0xFF`. Highest-value, zero-risk diagnostic.
2. **We send ZERO trailing clocks after the last bitstream byte** — `spi3_pump()` finishes and we immediately deassert CS, then `0x3A`. The sibling's SPI variant clocks **200 trailing `0x00` bytes** after the bitstream (fpga.c:342-344); openFPGALoader sends a checksum frame + commands. Gowin needs trailing CCLK to run the CRC-check / wakeup / DONE sequence. Untested gap.

## Gowin STATUS_REGISTER (0x41) bit map (openFPGALoader gowin.cpp:42-57)

```
bit 0   CRC_ERROR            bit 12  GOWIN_VLD
bit 1   BAD_COMMAND          bit 13  DONE_FINAL      <- success = this set
bit 2   ID_VERIFY_FAILED     bit 14  SECURITY_FINAL
bit 3   TIMEOUT              bit 15  READY
bit 5   MEMORY_ERASE         bit 16  POR
bit 6   PREAMBLE             bit 17  FLASH_LOCK
bit 7   SYSTEM_EDIT_MODE
```

This is the discriminator we've been missing. Reading 0x41 after our upload would tell us which failure class we're in:
- `CRC_ERROR` / `ID_VERIFY_FAILED` set → bytes reached the config engine but content/transport is wrong (→ self-capture the wire).
- `BAD_COMMAND` / `READY` clear / status all-`0xFF` → the FPGA never entered config-receive (→ config-entry / RECONFIG_N problem, JTAG path).

i.e. it cleanly separates "wire problem" from "never-entered-config problem" — the exact fork we've been unable to resolve.

## Canonical openFPGALoader writeSRAM() sequence (gowin.cpp:778-828)

```
readStatusReg()              # optional pre-check
CONFIG_ENABLE  (0x15)
INIT_ADDR      (0x12)
XFER_WRITE     (0x17)        # JTAG transport; SSPI uses 0x3B for the same role
<bitstream>
send 0x0A ; <32-bit checksum> ; send 0x08   # JTAG-only framing
CONFIG_DISABLE (0x3A)
NOOP
status = readStatusReg() ; success = status & DONE_FINAL (bit 13)
```

## Working sibling fpga_init_once() — HW4.0 bitbang variant (fpga.c:166-198)

```
fpga_hw4_begin()                       # also calls fpga_start_clock_output()  <- feeds FPGA an MCO ref clock
SELECT(PA15) HIGH
RESET(PC8) LOW, 10ms, HIGH, 1ms        # <- RECONFIG_N pulse (re-enter config)
read_register32(0x11..)                # READ_IDCODE
read_register32(0x13..)                # READ_USERCODE
read_register32(0x41..)                # READ_STATUS  <- pre-config status read
write_command_word(0x1200)             # INIT_ADDR
write_command_word(0x1500)             # CONFIG_ENABLE
0x3B + <bitstream>                     # CS framed via PA15
status = read_register32(0x41..)       # <- success = status != 0xFFFFFFFF
write_command_word(0x3A00)             # CONFIG_DISABLE
delay 100ms
```

Note: the sibling does **NOT** send `0x05` (ERASE_SRAM); it does IDCODE/USERCODE/STATUS reads instead. Our 2C53T stock capture DOES show `05 00`, so this differs by board — not necessarily our bug, but worth noting.

Mode note: the sibling bit-bangs **Mode 0** (clock idles low, MOSI set while low, MISO sampled on rising edge — fpga.c:66-79). Our SPI3 is **Mode 3**. Our stock 2C53T capture indicated Mode 3, so this is a board/design difference, not confirmed-wrong for us — but unverified on our own wire.

## What's board-specific and NOT worth chasing for us

- **RECONFIG_N pin choice.** Sibling pulses PC8 (bitbang variant) / PC9 (SPI variant). On the 2C53T, **PC9 = power-hold (HIGH or device dies), PC8 = power button** — both off-limits. We already hunted MCU-routed reset pins (PC4/PD3/PD2, all negative — commit 3c53e53) and tested PD2 as a held strap (negative — 3e1b777). The mechanism is real (Gowin spec + sibling both confirm RECONFIG_N is how you re-enter config from a running design) but our board's RECONFIG_N is either unidentified or strapped to a passive → this is what the JTAG bench resolves.
- **MCO reference clock** (`fpga_start_clock_output`). Sibling feeds the FPGA a clock on PA8. On 2C53T, **PA8 is a button-matrix column**, and our FPGA is NV instant-on with its own oscillator (runs the meter design standalone). So MCO-via-MCU isn't our path. (Worth a one-line confirmation that our FPGA self-clocks during config — but low priority.)

## Recommended firmware experiments (testable NOW on Unit 2, no JTAG bench)

Ranked by information-per-effort:

1. **Add a Gowin `0x41` STATUS_REGISTER read** after the `0x3A` close (CS frame: `0x41 00 00 00` then read 4 bytes, like the sibling's read_register32), store into `fpga_state_t`, and decode the bits in the `fpga reinit` echo. **This is the diagnostic that ends the guessing** — it tells us CRC vs ID-verify vs not-ready.
2. **Add a trailing-clock knob** (`uint16_t trailing_clocks`) to `fpga_cfg_seq_opts_t`: after the pump, before `0x3A`, clock N×`0x00` bytes (sibling uses ~200). Sweep 0 / 64 / 200 / 512.
3. **Add pre-config IDCODE/STATUS reads** (`0x11`, `0x41`) before the `05/12/15` prelude — cheap, matches the sibling's "wake + verify" preamble.

If (1) returns a real status with `ID_VERIFY_FAILED` or `CRC_ERROR` → the bytes are reaching the engine and we have a content/transport bug → prioritize the self-capture. If (1) returns all-`0xFF` / `READY` clear → confirms config-entry is the wall → JTAG is the only path, with evidence.

## BENCH RESULT (2026-06-13, Unit 2) — config-entry wall PROVEN by direct measurement

Implemented both gaps as `fpga reinit` additions (0x41 STATUS read + decode; `tc<n>` trailing-clocks knob). `make guest` clean, stock-faithful defaults. Warm-reinit results:

1. **Trailing clocks 64/200/512 → ZERO effect.** `0x3A` close stays `01`, `0x41` stays `80 01 C8 10`. Hypothesis ruled out.
   > **UNSUPPORTED AS WRITTEN (2026-07-28, Exp I).** The `0x41` readout used here was invalid (see note under 2), and an invalid-but-deterministic read reports "no change" whether or not anything changed. The conclusion may well hold, but it needs re-running against a readout that has been anchored to a known answer.
2. **`0x41` STATUS read = stable `80 01 C8 10` ("READY POR") across repeats** — looked like a real register.
   > **CONFIRMED SPURIOUS (2026-07-28, Exp I).** Point 3 below was right and got un-learned by later sessions. `0x8001C810` is `0x00039020` sampled one bit early, and `0x00039020` is itself a **bit-rotation of the `0xC8100001` free-running pattern** from point 3 (verified: it is one of the 32 rotations; odds of a genuine value landing there are ~7.5e-9). `/2` and `/256` reads simply land on different phases of the same stream — that is the entire difference between the two numbers. **The project has never successfully read this register**, and `0x00039020` — treated from 2026-06 onward as "the refusal signature" and retro-validated in Exp F — is phase noise. See `expE_swd_state_diff_2026-07-28.md` § Experiment I.
3. **DECISIVE opcode-discrimination probe (`spi3 xfer`):** `0x11`(IDCODE), `0x41`(STATUS), `0x00`(no-op), `0x13`(USERCODE) **ALL return identical MISO `10 00 01 C8 10 00 01 C8`.** The FPGA does not decode the opcode — it free-runs a fixed 4-byte `C8 10 00 01` pattern on MISO (its running NV/user design) and ignores MOSI.

**Conclusion:** `0x11` did NOT return IDCODE `0x0120681B` ⇒ the FPGA is **not in SSPI config-receive mode**. The `0x41`="READY POR" decode was a spurious phase-slice of the free-running stream (proven by `0x00` giving the same bytes). No `CRC_ERROR` / `ID_VERIFY_FAILED` because the bitstream bytes never reach the config engine — they're *ignored*, not *rejected*. This is the cleanest direct evidence (vs prior inference) that the MCU's SPI sequence cannot knock the GW1N out of its running NV design into SSPI slave-config. **FT232H JTAG SRAM-load remains the route.** The `0x41`-read + `tc` knob are kept as permanent diagnostics.

## Sources
- `/Users/david/Projects/RESEARCH/OpenScope-2C23T/src/fpga.c` (sibling, working)
- `/Users/david/Projects/RESEARCH/openFPGALoader/src/gowin.cpp` lines 31-60 (opcodes/status bits), 778-828 (writeSRAM)
- Gowin TN653 / UG290 (referenced in gowin.cpp comments)
