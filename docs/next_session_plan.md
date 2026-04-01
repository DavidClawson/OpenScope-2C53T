# Next Session Plan: FPGA SPI3 Communication

## Status (as of 2026-04-01)

**The problem:** FPGA holds SPI3 MISO HIGH (all reads = 0xFF) despite correct SPI3 configuration, control pins, and USART communication.

**What works:**
- SPI3 Mode 3 (CPOL=1, CPHA=1), Master, /2 clock (60MHz) ✓
- PC6 HIGH (FPGA SPI enable) ✓
- PB11 HIGH (FPGA active mode) ✓
- USART bidirectional communication (10-byte TX, 12-byte RX frames) ✓
- FPGA sends meter measurement data via USART (0x5A A5 E4 2E 63...) ✓
- Hardware connection verified (PB4 driven by FPGA, GPIO pull-down test) ✓

**What doesn't work:**
- SPI3 reads always 0xFF — FPGA not driving data on MISO during SPI clock

## Root Cause Hypotheses (ranked by likelihood)

### 1. Missing TMR3 timer configuration (HIGH probability)
The stock firmware enables TMR3 (IRQ 29) which drives the USART exchange AND may generate a hardware signal the FPGA uses as a trigger. Without TMR3:
- The FPGA might not know when to prepare SPI3 data
- The acquisition timing chain is broken

**Action:** Disassemble TMR3 init from the init function decompilation (~0x08026E90-0x08026F00). Configure TMR3 in our test firmware to match stock settings.

### 2. Missing USART handshake completion (MEDIUM probability)
The USART ISR only sends to queue 0x20002D7C after:
- TX sends all 10 bytes (tx_index == 10)
- RX receives valid 12-byte frame (0x5A 0xA5)
- exchange_lock flag (meter_state+0xF3C) == 0

Our test firmware sends USART bytes by polling, not via the ISR. The FPGA may expect a specific bidirectional timing pattern that only TMR3-driven exchanges produce.

**Action:** Implement a proper TMR3-driven USART exchange in the test firmware, matching the stock firmware's ISR behavior.

### 3. Missing mode_state variable (MEDIUM probability)
The `usart_cmd_dispatcher` only sends to the SPI3 queue when `meter_state[0xF68] == 2`. This variable is initialized to 8 during boot and changed by mode switches. Without FreeRTOS running the dispatcher, this code path never executes.

**Action:** Set meter_state[0xF68] = 2 directly in RAM, or implement a minimal FreeRTOS-based test.

### 4. FPGA needs specific SPI3 clock activity first (LOW probability)  
The FPGA might need to see SPI3 clock (SCK) toggling in a specific pattern before enabling MISO output. The stock firmware's SPI3 handshake (send 0x05 + 0x00) might need specific timing relative to USART.

**Action:** Try different SPI3 handshake sequences with longer delays.

### 5. Additional GPIO pin needed (LOW probability)
There might be another undiscovered control pin. The init function has 47 GPIO configurations and we've only identified the critical ones.

**Action:** Cross-reference GPIO configs in init_function_decompile.txt with our test firmware.

## Concrete Steps for Next Session

### Step 1: TMR3 Analysis (30 min)
```
1. In init_function_decompile.txt, find TMR3 (0x40000400) configuration
2. Extract: prescaler, period, interrupt enable, output compare settings
3. Check if TMR3 has an output pin (which GPIO?) that could be FPGA trigger
4. Look for TMR3 IRQ handler — what does it do?
```

### Step 2: Minimal FreeRTOS Test (1 hour)
```
1. Build spi3test with FreeRTOS (using the main firmware Makefile, not hwtest)
2. Create two tasks:
   a. USART exchange task (sends 10-byte commands, processes 12-byte responses)
   b. SPI3 probe task (tries SPI3 reads after USART establishes)
3. Configure TMR3 to match stock firmware
4. Set meter_state[0xF68] = 2
5. Flash and test
```

### Step 3: Logic Analyzer Capture (alternative approach)
```
1. Flash stock firmware back onto device
2. Connect logic analyzer to PB3 (SCK), PB4 (MISO), PB5 (MOSI), PB6 (CS)
3. Capture the first few seconds of boot
4. Decode SPI3 frames to see exactly what the FPGA sends
5. Note timing relative to USART activity
```
This requires the HiLetgo 24MHz analyzer + sigrok, which we have.

### Step 4: Full Audit Document (ongoing)
Continue building the traceability matrix:
- Stock function → custom implementation status
- Every decompiled block → integrated / not needed / pending

## Key Files

| File | Purpose |
|------|---------|
| `reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md` | 53-step boot timeline |
| `reverse_engineering/analysis_v120/init_function_decompile.txt` | 6,391 lines, full init |
| `reverse_engineering/analysis_v120/fpga_task_decompile.txt` | 5,701 lines, 10 sub-functions |
| `reverse_engineering/analysis_v120/usart_protocol_decompile.txt` | 787 lines, protocol spec |
| `firmware/src/spi3test.c` | Current test firmware (last: USART frame capture) |
| `scripts/decompile_*.py` | Capstone-based decompiler scripts |

## Quick Reference: What the FPGA Expects

```
Boot sequence (from stock firmware):
1. Power on, FPGA loads bitstream (non-volatile)
2. FPGA starts USART heartbeat (0x5A bytes at 9600 baud)
3. MCU sets PC6 HIGH, PB11 HIGH
4. MCU configures SPI3 Mode 3, enables SPI
5. MCU sends USART boot commands: 0x01, 0x02, 0x06, 0x07, 0x08
6. MCU does SPI3 handshake: flush, CS + 0x05 + 0x00, flush
7. MCU starts TMR3 → drives periodic USART exchanges
8. After successful USART handshake: SPI3 data begins
9. SPI3 commands 1-9 trigger different acquisition modes
```
