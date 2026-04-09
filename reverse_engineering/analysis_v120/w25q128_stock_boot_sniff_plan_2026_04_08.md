# W25Q128 Stock-Boot Sniff Plan 2026-04-08

Purpose:
- turn the missing-image/state hypothesis into one concrete capture procedure
- sniff the external SPI flash bus during a stock-app boot attempt
- distinguish:
  - early failure before real storage traversal
  - coherent filesystem traversal followed by later failure
  - likely small external-metadata mismatch

Primary references:
- [missing_image_state_forensic_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/missing_image_state_forensic_split_2026_04_08.md)
- [w25q128_dump_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/w25q128_dump_2026_04_08.md)
- [ARCHITECTURE.md](/Users/david/Desktop/osc/reverse_engineering/ARCHITECTURE.md)
- [stock_app_slot_test.py](/Users/david/Desktop/osc/scripts/stock_app_slot_test.py)

## 1. Signals to capture

The external flash is on SPI2:

- `PB12` = chip select, active low
- `PB13` = clock
- `PB14` = MISO
- `PB15` = MOSI

Ground:
- logic-analyzer ground to board ground

Confirmed pin mapping:
- [ARCHITECTURE.md](/Users/david/Desktop/osc/reverse_engineering/ARCHITECTURE.md#L113)
- [peripheral_map.md](/Users/david/Desktop/osc/docs/design/peripheral_map.md#L67)

## 2. Decoder settings

Known protocol settings:
- standard SPI NOR flash reads via opcode `0x03`
- Mode 3
- `CPOL = 1`
- `CPHA = 1`
- chip select active low

Grounding:
- [peripheral_map.md](/Users/david/Desktop/osc/docs/design/peripheral_map.md#L77)
- [HARDWARE_PINOUT.md](/Users/david/Desktop/osc/reverse_engineering/HARDWARE_PINOUT.md#L315)

## 3. Analyzer-speed caveat

The flash bus is believed to run around `~30 MHz`:
- [ARCHITECTURE.md](/Users/david/Desktop/osc/reverse_engineering/ARCHITECTURE.md#L70)

So:

- a cheap `fx2lafw` 24 MHz analyzer is **good enough for traffic-shape questions**
  - does `PB12` assert repeatedly?
  - are there sustained flash bursts after app jump?
  - does boot die before meaningful storage activity?
- it is **not ideal for reliable byte-level SPI decode** at full stock speed

Practical split:

- if all you have is the 24 MHz analyzer, use it for:
  - burst timing
  - CS activity count
  - rough “no traversal / some traversal / sustained traversal” classification
- if you can borrow a faster analyzer, prefer `>=100 MHz` for byte-level address
  decode

## 4. Safe test loop

We already have a reversible app-slot test path:
- [stock_app_slot_test.py](/Users/david/Desktop/osc/scripts/stock_app_slot_test.py)

Use this loop:

1. Keep the permanent bootloader intact.
2. Hook up the analyzer to `PB12/PB13/PB14/PB15/GND`.
3. Arm the capture **before** launching the stock app.
4. Flash the stock app into the app slot:

```bash
python3 /Users/david/Desktop/osc/scripts/stock_app_slot_test.py flash-stock
```

5. Let the device attempt boot for `2-5s`.
6. Stop capture.
7. Recover to our firmware:

```bash
python3 /Users/david/Desktop/osc/scripts/stock_app_slot_test.py restore
```

## 5. What to trigger on

Preferred trigger:
- falling edge on `PB12` (flash CS assert)

If your analyzer cannot trigger well:
- start capture first
- then run `flash-stock`
- trim the file later to the first large post-jump `PB12` activity

## 6. What “good” and “bad” look like

### Result A: almost no meaningful flash activity

Signs:
- one or two short `PB12` assertions only
- no sustained burst train
- black screen follows immediately

Interpretation:
- strongest support for missing MCU-side high-flash descriptor/data
- failure likely happens before real filesystem traversal

### Result B: clear sustained flash traversal

Signs:
- repeated `PB12` assertions over tens/hundreds of milliseconds
- many clock bursts after jump
- with a fast analyzer, likely repeated `0x03` reads at multiple addresses

Interpretation:
- weakens the “dies before storage traversal” theory
- pushes us toward:
  - small external metadata mismatch
  - later runtime state/hardware issue

### Result C: coherent early traversal, then repeated small-loop retries

Signs:
- structured burst trains early
- later short repeated reads to a narrow address range
- boot still ends dark

Interpretation:
- strongest support for small external metadata mismatch
- likely a missing or malformed expected file/sector rather than no storage path

## 7. What to look for if byte decode is possible

From the current dump:
- volume 0 starts at `0x000000`
- volume 1 starts at `0x200000`
- there are two FAT12 volumes

Grounding:
- [w25q128_dump_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/w25q128_dump_2026_04_08.md)

So useful address clues would be:
- reads near `0x000000`
- reads near `0x200000`
- deeper reads that repeat in a tight band after those

Useful opcode clue:
- `0x03` plus 3-byte address is the expected baseline read path

## 8. Capture recommendations

### If using the 24 MHz fx2lafw analyzer

Goal:
- classify activity shape, not exact bytes

Recommended channels:
- `D0 = PB12 CS`
- `D1 = PB13 CLK`
- `D2 = PB14 MISO`
- `D3 = PB15 MOSI`

Recommended capture:
- sample at max available rate
- capture `2-5s`
- do not depend on SPI decode correctness

### If using a faster analyzer

Goal:
- decode commands and addresses

Recommended settings:
- sample at `>=100 MHz`
- SPI Mode 3
- CS active low
- decode MOSI as command/address stream

## 9. Success criteria

This capture is successful if it lets us answer **one** of these clearly:

1. stock dies before meaningful flash traversal
2. stock traverses flash coherently, then dies later
3. stock appears to loop on a narrow metadata/resource read pattern

Any one of those is more valuable now than another broad command-guessing pass.

## 10. Immediate next decision after the capture

- If Result A:
  - prioritize missing MCU-side image/state
  - stop command-family expansion
- If Result B:
  - revisit external metadata mismatch and later runtime state
- If Result C:
  - compare the narrow failing address band against the current dump and extracted
    FAT contents first
