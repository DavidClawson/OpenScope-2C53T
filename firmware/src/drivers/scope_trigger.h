/*
 * OpenScope 2C53T - Scope Trigger Comparator DAC
 *
 * Faithful reimplementation of the CH1 trigger-comparator DAC path from stock
 * gpio_mux_portc_porte (FUN_080018a4). Stock computes a 12-bit DAC code from a
 * per-range calibration table and the user trigger level, writes it to
 * DAC1_DHR12R1 (0x40007408) and pulses the software trigger (0x40007404 |= 1).
 * The DAC1 output appears on PA4 — directly scopeable, so this path is
 * bench-verifiable WITHOUT a working FPGA/scope acquisition chain.
 *
 * Stock formula (divisor constant = 200.0f, extracted from the V1.2.0 image at
 * 0x08001a54): dac = ((upper - base) / 200.0) * (level + 100) + base
 *   - upper[range], base[range] : per-range cal entries (uint16)
 *   - level : trigger level, [-100 .. +100]  (level+100 -> [0..200])
 *
 * NOTE on cal data: the factory per-range cal is unrecoverable on our units
 * (MCU saved_config @0x08006000 overwritten; W25Q holds no cal — see
 * analysis_v120/w25q128_flash_map_2026-06-13.md). The table below is a
 * documented PLACEHOLDER (full-scale linear). The CODE PATH (formula + register
 * writes) is faithful and verifiable; the cal DATA is a TODO pending a
 * regenerated calibration once the scope acquisition path works.
 *
 * Shares DAC1/PA4 with the signal generator (dac_output.c). Scope and siggen are
 * mutually exclusive modes — do not run both at once.
 */
#ifndef SCOPE_TRIGGER_H
#define SCOPE_TRIGGER_H

#include <stdint.h>

/* Configure DAC1 (PA4) for single-value software-triggered output. Idempotent. */
void scope_trigger_dac_init(void);

/* Pure formula (no side effects) — testable in isolation. Returns 12-bit code. */
uint16_t scope_trigger_dac_compute(int range, int level);

/* Faithful path: compute(range, level) then write DAC1 + software trigger. */
void scope_trigger_dac_set(int range, int level);

/* Direct 12-bit write to DAC1 + software trigger (hardware-path verification). */
void scope_trigger_dac_raw(uint16_t code);

/* Last value written to DAC1_DHR12R1 (for the debug shell echo). */
uint16_t scope_trigger_dac_last(void);

#endif /* SCOPE_TRIGGER_H */
