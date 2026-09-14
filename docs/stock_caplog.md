# stock-caplog — a logger that rides inside stock V1.2.0

`scripts/stock_caplog/` answers questions of the form *"what does the stock firmware
actually send / actually touch when I do X?"* without a logic analyzer and without opening
the case. It patches the user's own stock V1.2.0 APP image with two six-byte hooks and a
692-byte code cave, runs, and leaves its log in a corner of SRAM that survives a
MENU+Power round trip, where a firmware that leaves that SRAM alone reads it back over its
debug shell.

It is how the meter SoC's word map was measured (issue #15): one word per function,
stock's menu order, and the fact that stock's meter never moves a frontend pin.

**One constraint up front:** the block lives at `0x20037000`, and this firmware's own `.bss`
covers that address (`_ebss` = `0x200375D4` in the app flavour, higher in `guest`; the stack
runs from `0x20037FE0` down). Booting OpenScope zeroes the block — measured 2026-09-13 as an
all-zero read. The reader has to be something that does not touch that SRAM: today that is
the 2C23T port (`Stlkv/OpenScope-2C23T-2C53T-port`, RAM ends `0x20036078`), and — untested —
the OpenScope HID bootloader's memory read. See Limits.

## What it records

| hook | where in stock | what it logs |
|---|---|---|
| `caplog_tx()` | dvom TX task, `0x0803E44C`, right after a frame to the meter SoC is complete | tick, the 10 TX bytes (ring of 32) |
| `caplog_tick()` | FreeRTOS SysTick handler, `0x0802A994` | tick counter; every ODR change on ports A–E (which pins moved, ring of 64), every CFGLR/CFGHR change, and a per-pin toggle counter (80 × u16) that outlives the ring |

Block: `0x20037000..0x20037700`, magic `CAPL`. It sits above stock's stack top
(`0x20036F90`), above the 2C23T port's RAM end (`0x20036078`), and below the factory IAP
bootloader's RAM magic at `0x20037FE0` (the IAP's own RAM ends at `0x20001A08`). It is
**inside** this firmware's `.bss` — see above. The cave has no `.data` and
no `.bss` (the linker script asserts it), uses integer registers only, and calls nothing
but `vTaskDelay`, which the TX hook displaced and replays.

## Procedure (factory IAP bootloader)

1. Build: `python3 scripts/stock_caplog/build_caplog_image.py`
   (needs `arm-none-eabi-gcc` on PATH and the stock image at
   `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`; any other sha256 is refused).
   Output: `firmware/build/APP_2C53T_V1.2.0_caplog.bin`, 12 bytes changed in the original,
   cave appended at `0x080BE700`.
2. MENU+Power into the IAP volume, flash the patched image (`scripts/iap_flash.py`).
3. Run the scenario in stock — the meter menu, a function change, whatever the question is.
   The rings keep the *last* 32 frames / 64 events; the toggle counters keep everything.
4. MENU+Power again **without removing power**, flash a reader that leaves `0x20037000`
   alone. Measured path: the 2C23T port firmware (its shell answers `mem <addr> <len>`).
   Booting this firmware instead zeroes the block (its `.bss` covers it).
5. `python3 scripts/stock_caplog/caplog_dump.py --port /dev/ttyACM0 --save run.caplog`
   — the tool probes the shell and speaks either `mem read <addr> 64` (this firmware) or
   `mem <addr> 256` (the port). `--decode run.caplog` re-renders a saved block. A `BAD` magic
   means the block did not survive or stock never ticked with the hooks in; do not read
   numbers out of it. Units with the OpenScope HID bootloader: `--hid` reads through
   `hid_flash.py` without booting an application at all — untested, see Limits.

## What it has established so far (unit #2, 2026-09-07; samples in `scripts/stock_caplog/samples/`)

1. **One word per function, no pairs.** Walking stock's meter menu: `0C 17 0B 0A 12 11 10 14`
   in menu order — DC Voltage, Continuity, Resistance, **Capacitance = 0x0A**, Temperature,
   small DC current, large DC current, Auto (`run2`). The AC/DC toggle inside a function is its
   own word: `0C→0D`, `17→0E`, `11→16`, `10→15` (`run3`). HOLD sends nothing. `0x0F` is never sent.
2. **Stock's meter moves no frontend pin.** Across 16 + 24 function changes the only GPIO
   traffic was the key-matrix scan (PA7/PA8, PB0, PC5/PC10, PE2/PE3, 63 toggles each in `run3`)
   and a 1 ms pulse train on PB12 (820). PC12/PE4-6/PA15/PA10/PB10/PB11/PB9/PA6, PC11,
   PD12/PD13, PC1/PC2/PC4/PD2 held still. The function switching is inside the SoC.
3. The TX frame is `AA 55 05 xx 00 00 00 00 00 cs`, byte for byte what
   `meter_build_tx_frame()` now sends.

`scripts/test_stock_caplog.py` pins all of this: the encoder against the reference patch
bytes, the hook sites against the stock image, the cave's size and the 12-byte footprint,
and the three sample dumps against the word table in `fpga_meter_plan.c`.

## Limits

1. **V1.2.0 only.** Hook addresses, the displaced instructions and the cave address are facts
   about that image; the builder refuses any other sha256. A new stock version needs the two
   sites found again (dvom TX: the `bl vTaskDelay` after the frame is built; SysTick: the
   handler's prologue).
2. **This firmware cannot be the reader.** Its `.bss` reaches past `0x20037000` and startup
   zeroes it (measured: all-zero block after booting `guest-coldtrace-meter`); the stack sits
   above `.bss` up to `0x20037FE0`, so there is no free window to move the block into either.
   Measured readback path: the 2C23T port (RAM ends `0x20036078`; three round trips, magic
   intact). Candidate for units with the OpenScope HID bootloader: `caplog_dump.py --hid`
   (`hid_flash.read_memory`, arbitrary address) — untested; whether the bootloader's own
   startup leaves that SRAM alone is the first thing to check. Also on those units,
   `build_stock_hybrid_image.py` pins the clean stock sha256 and will not accept the patched
   image as-is; it would need a flag to take a caplog image knowingly.
3. The rings are small by design (the block must fit above the stack and below the IAP magic).
   Long scenarios keep only their tail; the toggle counters are the long-run instrument.
4. The SysTick hook costs five GPIO reads and compares per tick. Stock ran normally with it, but
   nothing here has measured what it does to stock's timing.
