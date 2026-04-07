# SPI3 MISO Dead: Five Hypotheses (2026-04-06)

## Context

After a full-day session (compliance audit, Capstone binary trace, Unicorn emulation), we proved:
- SPI3 registers are EXACTLY correct (CTRL1=0x0347, CTRL2=0x0003, Unicorn-confirmed)
- Handshake byte count is correct (2 bytes per CS transaction)
- H2 cal table uploads (115,638 bytes)
- USART2 works (meter data flows — FPGA is alive)
- PB4 (MISO) is HIGH (floating, FPGA not driving it)

**Critical discovery: PC0 is HIGH.** Stock firmware monitors PC0 as a "data ready" signal from the FPGA (LOW = waveform data ready for SPI3 read). PC0 never goes LOW on our firmware. The FPGA's acquisition engine is not running. This means the problem is NOT SPI3 hardware — it's that we never properly told the FPGA to start acquiring.

## Hypothesis 1: "Dead channel" — param=0x00 means everything is off

**Likelihood: HIGH**

Commands 0x0B-0x11 configure the scope's channels, trigger, and timebase. Stock firmware computes real parameter values via a dispatch table at `0x0804BE74`. We send `param=0x00` for all of them.

If `0x00` means "channel disabled" for cmd 0x0B, "timebase stopped" for cmd 0x0F, and "no trigger" for cmd 0x0D — the FPGA has nothing to acquire. It sits idle, never asserts PC0, SPI3 returns 0xFF forever.

**Test:** Send known non-zero params for a basic scope config: CH1 DC coupled, 5V/div, auto trigger, 1ms/div timebase. Extract exact byte values from the dispatch table.

**Key file:** `analysis_v120/fpga_comms_deep_dive.c` — the `usart_tx_config_writer` function (7-type TBB switch) computes params from state fields.

## Hypothesis 2: Missing cmd 3 heartbeat — acquisition loop never starts

**Likelihood: HIGH**

Stock scope FSM sends command 3 to `usart_cmd_queue` on every exit (the "exit epilog"). Command 3 goes through the dispatch table, which re-arms the FPGA's acquisition trigger. Without continuous cmd 3 heartbeats, the FPGA may:
- Never enter continuous acquisition mode
- Or do one capture, assert PC0 once, then stop

The acquisition engine is designed as a one-shot that requires explicit re-triggering.

**Test:** After scope init commands, send cmd 3 via USART at ~20Hz. Watch if PC0 ever goes LOW.

**Key code:** `analysis_v120/scope_main_fsm_annotated.c` — exit epilog always sends `cmd=3` to usart_cmd_queue.

## Hypothesis 3: FPGA needs 500Hz TMR3 USART cadence

**Likelihood: MEDIUM**

Stock firmware drives USART2 exchanges at 500Hz via TMR3 ISR. The FPGA might use this cadence as a "heartbeat watchdog" — no periodic frames = stay idle. Our commands arrive at irregular intervals from FreeRTOS tasks.

Meter mode works because our poll task sends at a regular ~4Hz cadence (enough for the meter IC). Scope acquisition may need the faster 500Hz rate.

**Test:** Configure TMR3 to send USART frames every 2ms (500Hz) with current scope state.

**Key code:** `analysis_v120/fpga_task_annotated.c` — `input_and_housekeeping()` (function 8, 1342B) runs in TMR3 ISR.

## Hypothesis 4: FPGA stuck in meter mode — missing mode transition

**Likelihood: MEDIUM**

We send meter commands (0x0508, 0x0509, etc.) during `fpga_init()`, then scope commands in `fpga_enter_scope_mode()`. But we never send an explicit "exit meter mode" command. Stock's mode-switch handler has a COMMON_TAIL that suspends tasks, clears state, then sends new mode init.

If the FPGA's internal mode is still "meter," it ignores scope commands and the scope acquisition engine stays inactive.

**Test:** Either (a) don't send meter commands during init, or (b) send the mode-switch COMMON_TAIL sequence before scope commands. Also try: send `cmd_hi=0x05, cmd_lo=0x01` (possible "enter scope mode" command derived from mode_init_dispatcher case 0).

**Key code:** `analysis_v120/fpga_comms_deep_dive.c` — mode_init_dispatcher at `0x0800B908`.

## Hypothesis 5: PC0 is bidirectional — MCU must strobe it to trigger acquisition

**Likelihood: LOW**

We assumed PC0 is an FPGA output the MCU reads. But it could be bidirectional — the MCU pulses PC0 LOW to trigger acquisition, then the FPGA drives it LOW when data is ready.

**Test:** Configure PC0 as push-pull output, pulse LOW for 1us, switch back to input, check if FPGA responds. Or verify in the decompilation that `input_and_housekeeping` never writes to PC0.

## Priority for Next Session

1. **Extract real scope parameter values from the dispatch table** — figure out what non-zero bytes to send for commands 0x0B-0x11 for a basic CH1 DC 5V/div 1ms/div config
2. **Implement cmd 3 heartbeat** — send it at ~20Hz after scope init
3. **Monitor PC0** — if it goes LOW after the above, we're done; just wire up the SPI3 reads to the PC0 trigger
4. **If PC0 stays HIGH:** investigate TMR3 500Hz cadence and mode transition sequence

## What We Built Today

- `scripts/trace_peripheral_writes.py` — Capstone binary trace (static disassembly)
- `scripts/unicorn_init_trace.py` — Unicorn emulation of stock init (dynamic trace)
- `analysis_v120/compliance_audit_2026_04_06.md` — Full 4-agent register audit
- Debug overlay: HS bytes, PB4 state, PC0 data-ready, live SPI3 register values
- 8 compliance fixes in `fpga.c` (prescaler, CTRL2, IRQ, PC11, boot cmds, handshake, timing, PB11)

## Files Modified

- `firmware/src/drivers/fpga.c` — SPI3 prescaler /2, CTRL2=0x03, SPI3 IRQ, boot cmds 0x03-0x05, handshake 2-byte, PC11 gpio_init, PB11 moved later, SPI3 IRQ handler stub
- `firmware/src/ui/scope_ui.c` — Debug overlay: HS bytes, PB4, PC0, SPI3 registers
