# Scope Acquisition: Bugs and Discrepancies Found by Ripcord

Automated analysis of the stock V1.2.0 firmware (`APP_2C53T_V1.2.0_251015.bin`)
cross-referenced with the osc project's current `fpga.c` implementation.
Produced by ripcord's xref analysis + decompiled code synthesis on 2026-04-08.

The full protocol spec is at `ripcord/notes/scope_acquisition_spec.md`.

---

## Bug 1: Dual-Channel Data Is Interleaved, Not Sequential (CONFIRMED BUG)

**Severity: HIGH — will produce garbled waveforms in dual-channel mode**

The FPGA sends interleaved data: CH1[0], CH2[0], CH1[1], CH2[1], ...
The osc firmware reads sequentially: 1024 bytes into CH1, then 1024 into CH2.

**Stock firmware (correct):**
```c
// From fpga_task_annotated.c, case 4 (2048-byte dual-channel)
CS_ASSERT();
spi3_xfer(0xFF);  // discard echo byte
for (int i = 0; i < 1024; i += 2) {
    buffer[i]     = spi3_xfer(0xFF);  // CH1 sample
    buffer[i + 1] = spi3_xfer(0xFF);  // CH2 sample
}
CS_DEASSERT();
// De-interleave into separate CH1/CH2 arrays afterward
```

**osc firmware (incorrect) — in `fpga_acquisition_task`, dual-channel case:**
```c
// Current: reads 512 CH1 samples, then 512 CH2 samples
for (int i = 0; i < 512; i++) ch1[i] = spi3_xfer(0xFF);
for (int i = 0; i < 512; i++) ch2[i] = spi3_xfer(0xFF);
```

**Fix:** Read interleaved, then de-interleave:
```c
CS_ASSERT();
spi3_xfer(0xFF);  // discard echo
uint8_t raw[1024];
for (int i = 0; i < 1024; i++) {
    raw[i] = spi3_xfer(0xFF);
}
CS_DEASSERT();
// De-interleave
for (int i = 0; i < 512; i++) {
    ch1[i] = raw[i * 2];
    ch2[i] = raw[i * 2 + 1];
}
```

---

## Bug 2: Scope Config Commands 0x0B-0x11 Use Guessed Parameters (LIKELY ISSUE)

**Severity: HIGH — may prevent FPGA from arming ADC for scope mode**

The osc firmware's `cmd_hi` values for commands 0x0B through 0x11 are
labeled in the code as "empirically least-bad bank-2 defaults." These
configure the FPGA's scope acquisition registers and wrong values could
prevent the ADC from starting or produce incorrect sample timing.

**Stock firmware:** The parameter values come from the state structure
at offsets that depend on the current scope configuration (timebase,
channel enable, etc.). The exact mapping requires decompiling the
`usart_tx_config_writer` at `0x08039734` which packs 2-byte queue items
where the high byte is the parameter and the low byte is the command.

**Current osc values (from fpga.c):**
```c
fpga_timed_send_cmd(0x01, 0x0B, 15);  // guessed
fpga_timed_send_cmd(0x00, 0x0C, 15);  // guessed
fpga_timed_send_cmd(0x03, 0x0D, 15);  // guessed
fpga_timed_send_cmd(0x80, 0x0E, 15);  // guessed
fpga_timed_send_cmd(0x00, 0x0F, 15);  // guessed
fpga_timed_send_cmd(0x01, 0x10, 15);  // guessed
fpga_timed_send_cmd(0x00, 0x11, 15);  // guessed
```

**To resolve:** Capture stock USART2 TX with a logic analyzer during
scope mode entry. The correct values will be visible as the `cmd_hi`
bytes in the 10-byte USART frames sent after the mode switch.

---

## Bug 3: Missing Watchdog Feed (POTENTIAL DEVICE RESET)

**Severity: MEDIUM — may cause the device to reset after 2-5 seconds**

Stock firmware boot step 49 enables the Independent Watchdog (IWDG).
Stock's `input_and_housekeeping` feeds it every ~50ms via:
```c
if (++iwdg_counter >= 11) {
    IWDG->kr = 0xAAAA;  // feed watchdog
    iwdg_counter = 0;
}
```

The osc firmware does not implement watchdog feeding. If the stock
bootloader or init code enables the IWDG before the app takes over,
the device will reset after the watchdog timeout (~2-5 seconds with
IWDG prescaler /64 and reload 1249).

**Fix:** Add to the TMR3 ISR or a periodic task:
```c
static uint8_t iwdg_counter = 0;
if (++iwdg_counter >= 11) {
    WDT->rld = 0xAAAA;  // AT32 watchdog feed
    iwdg_counter = 0;
}
```

**Alternative:** If the osc bootloader does NOT enable IWDG, this is
a non-issue. Verify by checking if `IWDG->pr` and `IWDG->rlr` are
written during boot (addresses `0x40003004` and `0x40003008`).

---

## Bug 4: Acquisition Trigger Is Display-Driven, Not Timer-Driven (ARCHITECTURAL)

**Severity: MEDIUM — may cause inconsistent acquisition timing**

Stock firmware drives acquisition from a hardware timer (TMR3 ISR at
~1ms), which provides a consistent, predictable trigger cadence. It
sends TWO trigger items to `spi3_data_queue` per timer event for
double-buffered display update.

The osc firmware triggers acquisition from the display rendering loop
via `fpga_trigger_scope_read()`. This means:
- Acquisition rate is limited by frame rendering time
- Acquisition timing is irregular (depends on UI complexity)
- The FPGA may stall if it expects periodic polling

**This may not cause acquisition to fail entirely**, but it could
cause dropped frames or inconsistent timebase behavior, especially
at fast timebases where the FPGA expects sub-millisecond trigger
cadence.

**Fix:** Add a dedicated timer ISR or periodic FreeRTOS task that
triggers acquisition at a rate matching the current timebase setting,
independent of the display loop.

---

## Bug 5: SPI3 DMA Not Used (PERFORMANCE)

**Severity: LOW — polled transfers work but are slower**

The stock firmware configures DMA2 Channel 1 for SPI3 transfers
(29 DMA register writes found in the master init by ripcord's xref
analysis). The osc firmware uses polled SPI3 only.

At 60MHz SPI with 1024-byte frames, polled transfers consume ~17μs
per frame, which is fast enough for scope operation. DMA would free
the CPU during transfers but is not required for correctness.

The DMA configuration in the stock firmware may be for the bulk
calibration table upload (115,638 bytes) where CPU-free transfer
is more beneficial. The acquisition task's decompiled code shows
polled transfers for all 9 acquisition modes.

**Recommendation:** Low priority. Implement DMA for the H2 bulk cal
upload first (where it matters most), then optionally for acquisition.

---

## Version History Insight

Ripcord's cross-version analysis of V1.0.3, V1.0.7, V1.1.2, V1.2.0
reveals:

- **V1.0.3 had USART2-only FPGA communication** (no SPI3 at all)
- **V1.0.7 introduced the full SPI3 + DMA2 infrastructure** — this was
  a complete rewrite of the FPGA interaction layer
- **The FPGA protocol code has not changed since V1.0.7** — register
  access counts are identical across V1.0.7/V1.1.2/V1.2.0 (206 SPI3,
  20 USART2, 29 DMA2, 14 GPIOB, 31 GPIOC)
- All changes after V1.0.7 were UI, internationalization, and minor patches

**Implication:** FNIRSI got the FPGA protocol right on the second try
(V1.0.7) and hasn't needed to change it since. The protocol is stable
and well-characterized. If the osc firmware's scope acquisition doesn't
work, the issue is in the osc implementation, not in the protocol
understanding.

---

## Priority Order for Debugging

1. **Fix dual-channel interleaving** (Bug 1) — immediate, clear bug
2. **Verify cmd_hi values for 0x0B-0x11** (Bug 2) — needs logic analyzer or deeper decompilation
3. **Check/implement watchdog feeding** (Bug 3) — quick check, easy fix
4. **Consider timer-driven acquisition** (Bug 4) — architectural, not urgent for first-light
5. **DMA for bulk transfers** (Bug 5) — performance optimization, not blocking

## Reference

Full protocol spec: `/Users/david/Desktop/ripcord/notes/scope_acquisition_spec.md`
FPGA version evolution: `/Users/david/Desktop/ripcord/notes/fpga_version_evolution.md`
FPGA interaction analysis: `/Users/david/Desktop/ripcord/notes/fpga_interaction_analysis.md`
