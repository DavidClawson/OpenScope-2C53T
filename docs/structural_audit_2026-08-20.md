# RTOS structural audit — 2026-08-20

A full pass over `firmware/src` (~77k lines, 130 files) looking for structural
reliability problems: LCD writer discipline, shared-state races, bus ownership,
ISR hygiene, task/stack/queue budgets, error-path honesty, init-order
fragility, and dead weight. Run against the working tree at `86c6f11`.

**Verification status matters in this project** (see the devlog: three
instrument bugs each survived for weeks because a stable wrong number looks
like a right one). Items marked **[V]** were verified by reading the cited
code directly during the audit review; unmarked items were reported by the
audit sweep and spot-checked for plausibility but should be re-read at the
cited line before work starts. None of the P0 claims are unverified.

The audit also produced a list of ~36 **deliberate, documented choices** that
look like bugs and are not (§ Deliberate choices, at the end). Read that
before "fixing" anything in its list.

---

## P0 — produces wrong data or wrong behavior today

> **STATUS 2026-08-20 (same day): ALL FIVE P0 ITEMS ARE FIXED IN CODE.**
> P0.3 + P0.4 in `9d22750`; P0.1 + P0.2 in `ec8da20`; P0.5 in the commit
> after it. Host suites green throughout. **Bench validation pending** —
> the next flash should confirm: coldtrace boot → live trace → badges → one
> raw `spi3` shell command while the scope runs (parks cleanly, no desync),
> and a settings save/power-cycle round trip (the write path now verifies).
> The text below is kept as written, as the record of what was found.

### P0.1 [V] SPI3 has no real ownership — the park is a no-op in default builds
`fpga.c` `fpga_acq_pause()`: in every non-warmtest build the body is
`return true;` with a comment arguing the queue-driven task only acts on
explicit triggers. But `main.c`'s display loop calls `fpga_scope_heartbeat()`
every 7 frames, which posts to `spi3_acq_queue` — so the acq task IS
periodically active, and every shell command that "correctly parks" is
unprotected in `make guest`. Worse, 7+ shell commands (`spi3 xfer`,
`spi3 acqread`, `spi3 seq`, `spi3 armtest`, …) bypass the park entirely and
drive CS/SPI3 raw from the usb_dbg task at priority 2 — *below* the acq task
at 3, so the acq task can preempt them mid-CS-frame. `cmd_spi3_acqread_one`
carries a comment claiming it "does not touch the acquisition task", which is
false — it touches the shared bus.

**Consequence: a live source of corrupted experimental data.** Interleaved CS
frames garble both transfers and look like FPGA misbehaviour.
**Fix (M):** one `spi3_claim()/spi3_release()` that actually parks (works in
all builds), used by the raw helpers; delete the false comment. Related:
`fpga_scanner.c` "pauses" the acq task by setting `fpga.spi3_active = false`,
which **nothing reads** (the file admits it at :915).

### P0.2 [V] Torn waveform buffers — the renderer has no snapshot
`fpga.ch1_buf[]`/`ch2_buf[]` are written in place, byte-by-byte, by the acq
task (prio 3) and read by the display task (prio 1) with no double-buffer,
seqlock, or generation counter. A capture that lands mid-render splices two
frames. Every measurement badge, `scope_freq_estimate`, and the renderer read
through this. Plausibly explains the "capture glitch in the first ~26 µs,
124/168 frames" that `fpga.c:2965` attributes to FPGA re-arm — that
attribution should be re-tested after the fix (hypothesis, not established).
**Fix (M):** ping-pong buffers + published index, or a generation seqlock
mirroring the one `meter_data.c` already got right.

### P0.3 [V] SPI3 timeout reads count as success
`spi3_xfer()` returns `0xFF` on timeout — indistinguishable from real
all-ones data. The acq task's main read path then does
`spi3_ok_count++; spi3_timeout_count = 0; data_ready = true;`
**unconditionally** ("Always count as OK for now"). So the
`SPI3_BACKOFF_THRESHOLD` machinery is dead code on the normal path, and 1024
timeout bytes render as a confident flat trace reported as a good
acquisition. The shell's own `spi3_raw_xfer` returns `0xEE` *specifically to
distinguish timeouts* — the production path doesn't.
**Fix (M):** status out-param (or sentinel + counter); stop claiming OK.

### P0.4 [V] W25Q write failure is reported as success
`flash_fs_raw_sector_erase()` / `flash_fs_raw_program()` call `void` nolock
cores and `return FLASH_FS_OK` unconditionally; nothing reads the W25Q
status-register fail bits, and `raw_wait_busy()`'s bounded-guard expiry is
ignored. `settings_store` then sets `g_saved = g_live` and won't retry — the
store believes settings persisted when they may not have, and its
`g_write_blocked` diagnostic can never fire for a genuine flash fault. One
day after commissioning the first runtime writer, this is the next honesty
gap in that path.
**Fix (M):** propagate status from the nolock cores; read SR1 after
erase/program.

### P0.5 [V] Pre-scheduler FPGA mode entry can silently overflow the queue
`main.c` calls `fpga_set_meter_mode()` / `fpga_enter_scope_mode()` after
`fpga_create_tasks()` but **before** `vTaskStartScheduler()`.
`fpga_send_cmd()` sees a non-NULL queue and enqueues (zero-timeout) into a
depth-10 queue with **no consumer running** — long mode sequences truncate
silently. `fpga_timed_send_cmd()` already guards this exact hazard with an
`xTaskGetSchedulerState()` check; `fpga_send_cmd()` doesn't.
**Fix (S):** same guard in `fpga_send_cmd`, or move the calls into the
display task's first pass.

*(P0.0, for the record: the default `make`/`make guest`/`make emu` builds
carried a "`fpga_scope_write_reg` used but never defined" warning — the
definition sat inside the warm-handoff `#if` while its caller was
unconditional, and only linked because a constant-false stub dead-coded the
call. Found independently by the warning ratchet the same day and fixed in
`6d0c946` before this doc was written.)*

## P1 — correctness the user can see

### P1.1 The remaining decorative controls (EXP-17/EXP-19's siblings)
The pattern fixed twice already — UI mutates state, hardware never hears —
still holds for:

- **Coupling** (BTN_CH1/BTN_CH2, settings): prints "CH1 AC" while PC12 stays
  wherever boot left it. This one prints a wrong *unit*, not just a wrong
  number. `fpga_scope_coupling_param()` exists but only `fpga_scope_reinit()`
  (which no button calls) consumes it.
- **Trigger mode / edge / source**: popup says "Trig: Normal", FPGA keeps the
  arm-block default. The byte-builders exist (`fpga_scope_trigger_mode_byte`,
  `_trigger_lsb`); nothing pushes them on button press. (Trigger *level* is
  real — reg 0x08 — but the mode/edge UI around it is not.)
- **Channel enable**: `fpga_set_channel_mask()` — the PC1/PC2 mask that the
  whole CH2 investigation hinged on — is never called from the input handler.
  Toggling a channel in the UI does not reroute the converters.
- **bw_limit**: reaches nothing. (**probe 1x/10x** is legitimately UI-only —
  it declares a physical probe setting.)

**Fix (S each):** the EXP-17 template exists and has been applied twice:
`fpga_apply_X()` single entry point + boot reconcile + "NOT SET" on failure.
Do coupling first (wrong-unit class), then channel mask (CH2 relevance).

### P1.2 [V] Two independent TX engines on USART2
`usart2_send_cmd()` (polled, writes `USART2->dt` directly) coexists with the
ISR-pumped `fpga_usart_tx_task` frame engine, no arbitration. Polled callers
include the meter-poll activation burst and shell commands. A polled byte
mid-ISR-frame corrupts the 10-byte command on the wire; the RX gate that
drops data frames during TX (`rx_data_tx_busy_drop_count`) is evidence this
happens. Also `fpga_usart_scope_enable(false)` (display task, mode switch)
can kill CTRL1 mid-frame. **Fix (M):** route everything through the queue, or
critical-section + `tx_index` check in the polled path.

### P1.3 Silent drops on control paths
- `send_cmd()` (input handler): zero-timeout, result discarded. A dropped
  `DCMD_REDRAW_ALL` strands the power-off overlay — the exact failure the
  code's own comment warns about. Retry that one with a timeout; count drops.
- `fpga_send_cmd()`: ~40 of ~45 call sites `(void)` the result; a full queue
  truncates a mode-entry sequence mid-way, leaving the FPGA half-configured.
  Add a `fpga_send_seq()` that reports partial failure + a drop counter.
- `meter_rx_queue` ISR send result discarded — dropped meter frames are
  invisible. Count them.
- Acq task discards triggers silently when `!initialized || bus_released`.

### P1.4 8 of 10 tasks are unmonitored
Health monitor covers `display` and `key` only. If `dvom_RX`, `fpga`,
`meter_poll`, or `usb_dbg` wedges, the watchdog stays fed and the device runs
forever with a dead meter or dead acquisition. Three FPGA tasks run on
**64-word (256 B)** stacks that have *never been measured* (high-water-mark
sampling only covers registered tasks). **Fix (M):** raise `HEALTH_MAX_TASKS`,
register everything, per-task deadlines (meter_auto legitimately sleeps
~700 ms; usb_dbg blocks in USB sends), then right-size the tiny stacks from
real HWM data. Note `GUEST_BUILD` skips `watchdog_init()` entirely
(deliberate for the bench) — so this machinery is currently never exercised
on hardware; schedule one non-guest soak once monitoring is broadened.

## P2 — robustness and discipline

- **The FPGA scanner** (`fpga_scanner.c`, 971 lines, Settings item 9): runs
  up to ~67 min *on the input task*, drawing the LCD outside the display task,
  "pausing" the acq task via a flag nothing reads, feeding the watchdog
  directly while the input task is by definition stalled. It also early-outs
  (does nothing) in every warmtest/coldtrace image actually flashed today.
  Either gate it behind a real park + `ui_modal_active`, give it its own
  task/mode, or retire it to the attic.
- **`ui_modal_active` is advisory** (today's countdown fix): the 120 ms settle
  is a hope, not a handshake — a display frame already inside
  `lcd_set_window→RAMWR` when the flag flips can still interleave once. The
  per-second overlay repaint heals it, so this is cosmetic-rare; the clean fix
  is a display-task ack (flag + counter) or routing the overlay through a
  `DCMD_MODAL_*` command. Do it when a second modal appears.
- **`scope_state` / mode triple**: input task, usb_dbg task, and
  meter_autoselect all write `current_mode`/`meter_submode`/scope fields;
  hardware pushes are paired with the writes non-atomically. A snapshot
  accessor for readers + one serializing owner for mode transitions.
- **`shared_mem` 88 KB pool**: input task can re-acquire (and memset) the
  pool while the display task walks FFT/persistence structures backed by it.
  Torn frame at best, HardFault at worst.
- **Settings writes spin at priority 4**: the whole erase+program (45–400 ms,
  busy-wait polls, FS mutex held) runs on the *input* task. Move to a
  low-prio writer task fed by a queue, or yield in the poll loops.
- **500 Hz button ISR calls HAL `gpio_init()` 7–11× per scan** at the syscall
  ceiling — substantial jitter source for every other ISR. Precompute the four
  CRL/CRH masks and write them directly (the codebase already does this
  elsewhere).
- **`status_bar.c:74`** [V-class]: battery-critical path busy-spins ~1–4 s in
  the display task with a `for(volatile…)` loop — health monitor times out at
  2 s, watchdog fires, and the clean shutdown becomes a reset. One-line fix:
  `vTaskDelay`.
- **USART2 TX frame build** has no critical section vs its own ISR
  (`tx_index = 0; memset(...)` while TDBEIEN may still be pending); a 10 ms
  delay is the only guard.
- **dvom_TX/meter_poll/mtr_wave at 256 B stacks**, `key` ties the timer
  service at prio 4 (the comment claiming the timer task is "highest" is now
  wrong), `data_ready` set before CS deasserts on one path.

## P3 — cleanup

- **~1.4k lines compiled but unreachable**: `decode/` (5 decoders, zero
  consumers), `esp_comm.c`, `screenshot.c` (and BTN_SAVE shows "SAVED #n"
  without saving anything — either wire it to `flash_fs` or stop the popup
  lying), `mask_test.c` (dead LCD writer), 2 of the module .c files. Drop
  from `C_SOURCES` or wire up.
- **File splits**, mechanical, high-value for review bandwidth:
  `usb_debug.c` (6921 lines, ~110 commands, if/strcmp dispatch → per-prefix
  files behind one command table), `fpga.c` (5684 lines → transport / config /
  meter / scope / diag), `scope_ui.c` (2256 lines → split the live compositor,
  which is the piece coupled to P0.2).
- **`attic/`** (18 files, ~6.9k lines) out of `src/`; 4× duplicated
  mutate→apply→popup button blocks → one helper (natural home for the P1.1
  pushes); shell commands that read `meter_reading` raw instead of through the
  seqlock that exists for exactly that; missing init-order comments for the
  five undocumented load-bearing orderings in `main()` (meter_data before
  fpga tasks; input queue before button_scan; theme→splash; buzzer init→task;
  flash_fs's PB12–PB15 touch predating fpga_init).

---

## What is deliberately NOT on the list

The audit checked ~36 documented deliberate choices against the code and all
hold. Highlights (full inventory in the audit transcript): the meter seqlock
and immutable-event RX queue (both replaced known-broken designs — do not
"simplify"); `DCMD_REDRAW_ALL` never gated (the modal teardown depends on
it); `fault_display` writing the LCD raw with interrupts off; `watchdog_init`
skipped in GUEST_BUILD; the warm-handoff early-returns ("the stock-armed
state IS the experiment"); `fpga_apply_vdiv` not parking (GPIO relays, no SPI
frame); settings-store refusal semantics; `rtt.c` (looks dead under RDP, is
actually the primary bench console via `DEBUG_SHELL_RTT_ONLY`).

## Suggested sequencing

1. **P0.3 + P0.4 first** — they are the project's signature defect class
   (stable plausible wrong numbers) sitting on the two paths we just
   commissioned (acquisition badges, settings persistence). Small, honest,
   testable.
2. **P0.1 + P0.2 as one arc** — bus ownership + buffer snapshot; both feed
   the "why does the bench data sometimes glitch" question and want a bench
   session to validate.
3. **P1.1 controls audit** — third and final application of the EXP-17
   template; closes the decorative-controls memory item.
4. P1.2–P1.4 as a "plumbing week"; P2/P3 opportunistically, file splits
   before any large new feature lands in those files.
