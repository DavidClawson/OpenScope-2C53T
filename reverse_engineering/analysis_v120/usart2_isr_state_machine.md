# USART2 ISR State Machine — Resolved

**Status:** Supersedes `meter_fsm_deep_dive.md` Q1 and the "12-byte RX echo"
claim in `CLAUDE.md`. Derived from `decompiled_2C53T_v2.c` function
`usart2_isr_real` @ 0x080277b4 (lines 13647–13710) and `dvom_tx_task`
@ 0x080373f4 (lines 30816–30839). Cross-checked against the bench
capture in `meter_frame_capture_log.md` (2026-04-04).

## The contradiction that prompted this

Prior decomp analysis claimed `rx_integrity_marker` = data frame byte[7],
validated against `0xAA`. Today's bench data shows data frame[7] is
*never* `0xAA` — it's `0x00`, `0x20`, or `0x28` depending on mode.
Both observations turn out to be correct; the reconciliation is below.

## What the ISR actually does

The USART2 RX path maintains **one shared 12-byte buffer** at
`0x20004E11` but handles **two distinct frame types** with **different
lengths and different trigger conditions**:

### Data frame (12 bytes)
- Header: `0x5A 0xA5`
- Carries meter digits, status flags, BCD payload
- Completes when `usart2_rx_byte_index == 0xC` (12)
- On completion, if `usart2_tx_byte_index == 10 && DAT_20001034 == 0`:
  release `dvom_rx_semaphore` → wakes meter parser
- **No integrity check on byte[7]** — the FSM downstream reads byte[7]
  as a status-flags field (bits 0/2/4/5 meaningful)

### Echo frame (10 bytes — NOT 12)
- Header: `0xAA 0x55`
- FPGA's acknowledgement of a TX command we just sent
- Completes when `usart2_rx_byte_index == 10 && buffer[1] == 0x55`
- On completion, validates two fields:
  - `rx_echo_verify_byte` (byte[3]) == `DAT_20000008` — the command
    byte we sent, stored by `dvom_tx_task` at line 30833
  - `rx_integrity_marker` (byte[7]) == `0xAA` — fixed integrity byte
    the FPGA always places here in echo frames
- If either check fails → `goto LAB_080277d0` (does not reset index,
  so the frame is effectively dropped on the next byte)

### Why Ghidra aliased the two
`rx_integrity_marker` is just the RAM label for address `0x20004E18`.
In a 12-byte data frame that's index 7; in a 10-byte echo frame that's
also index 7. The buffer is reused, so Ghidra can't distinguish
"data byte[7]" from "echo byte[7]" statically — same address, same
symbol. Prior analysis treated them as one variable; they are
physically the same byte of RAM but semantically two different
fields depending on which frame type just arrived.

## The ISR state machine (reconstructed)

```
byte 0 arrives:
  if buffer[0] not in {0x5A, 0xAA}: discard (goto end without reset)
  else: keep

byte 1 arrives (index == 2):
  valid combos: (0x5A, 0xA5) or (0xAA, 0x55)
  else: discard

byte N arrives (N >= 3):
  if NOT (buffer[0]==0xAA && index==10 && buffer[1]==0x55):
    # still in data-frame mode
    if index == 12:
      reset index
      if tx_done && mode_ok: release dvom_rx_semaphore   # DATA OK
    goto end
  else:
    # echo-frame complete at index 10
    if byte[3] != sent_cmd_byte OR byte[7] != 0xAA:
      goto end (drop)
    else:
      reset index                                         # ECHO OK
```

## What this fixes in our firmware / docs

### CLAUDE.md line 29 and 150 are wrong
Both say "10-byte TX / 12-byte RX" and "12-byte RX (0x5A 0xA5 data,
0xAA 0x55 echo)". **The echo is 10 bytes, not 12.** Only the data
frame is 12 bytes. Suggested corrections:

- Line 29: `9600 baud, 10-byte TX / 12-byte data RX / 10-byte echo RX`
- Line 150: `10-byte TX frames (header + cmd + params + checksum),
  12-byte data RX (0x5A 0xA5 header), 10-byte echo RX
  (0xAA 0x55 header, FPGA acks every TX command). Timer-driven via TMR3.`

### meter_fsm_deep_dive.md Q1 conclusion needs a footnote
Q1 wrote: "rx_integrity_marker is frame[7] (status byte), populated
from FPGA." That's correct for data frames but misleading because the
`!= 0xAA` check at line 13691 applies only to the echo-frame path.
The DCV FSM bit extractions (lines 30433+) run on data frames and
correctly read byte[7] as status bits, matching bench capture
values `0x00`/`0x20`/`0x28`.

### Our firmware already handles this correctly
`firmware/src/drivers/fpga.c:176-237` — the `USART2_IRQHandler`
already discriminates the two frame types by header byte and uses
different completion lengths (12 for data, 10 for echo). Only data
frames release `meter_sem`; echo frames bump a separate `echo_count`
and reset the index. No code change needed here — this was a
*documentation* gap, not a behavioral one. The driver's behavior is
now also matched by a specification.

## What this unblocks

1. **Removes one class of trial-and-error** on the USART2 protocol:
   we now know the two frame types are distinct lengths with distinct
   triggers, and the "sometimes the data looks weird" failure mode
   can be attributed to echo-frame leakage rather than FPGA glitches.
2. **Clarifies the TX → echo → data sequence**: every TX command
   from `dvom_tx_task` produces exactly one 10-byte echo frame from
   the FPGA, then (in meter mode) a stream of 12-byte data frames.
   The `DAT_20001034 == 0` gate on data-frame completion may be
   what controls whether we're expecting echoes or data next.
3. **Gives us a cross-check for future captures**: if we bench-capture
   a 10-byte frame starting with `0xAA 0x55`, byte[3] should equal
   the last command byte we sent, and byte[7] should always be `0xAA`.

## What remains unresolved

- ~~`DAT_20001034` — the mode flag that gates data-frame semaphore
  release.~~ **Resolved 2026-04-04**: this is the `usart_exchange_lock`
  uint16 already documented in `STATE_STRUCTURE.md` (+0xF3C) and
  `fpga_comms_deep_dive.c:169` as "nonzero = suppress notify". The
  read site at line 13680 gates `xSemaphoreGiveFromISR(meter_sem)`.
  The write site lives in `draw_measurement_display` @ 0x0800ec70
  (per `ram_map.txt`) and implements a display-render critical
  section: the UI raises the lock before rendering meter state, so
  the parser can't repopulate the state mid-frame. Initialized to 0
  at boot (`init_function_decompile.txt:5346`, `strh.w r0, [sl, #0xf3c]`).
  **Our firmware doesn't implement this lock** — `meter_ui.c` reads
  `meter_reading` unprotected. Low risk in practice because the
  update cadence is ~3 Hz and the reads are single-word, but worth
  adding if we ever see UI flicker or torn digits.
- `cRam20000007` / `cRam2000000e` — TX buffer's second byte and
  checksum. Their exact write sites in the TX fill path need to be
  traced to confirm the 10-byte TX frame layout we documented in
  `remaining_unknowns.md` section 5.
- Echo frame bytes [4..6], [8..9] — payload unknown. Could carry
  additional FPGA state (errors? ready flags?). Next bench session:
  dump full 10 bytes of an echo frame with a known TX command.
