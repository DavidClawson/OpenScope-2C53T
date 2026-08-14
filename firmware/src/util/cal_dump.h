/*
 * cal_dump.h — read-only inspection of the MCU-internal saved-config sector.
 *
 * WHY THIS EXISTS (2026-08-14)
 * ---------------------------------------------------------------------------
 * Stock restores a calibration-like table into RAM 0x20000358..0x2000044A from
 * a saved config guarded by a sentinel at ms[0x34E], and falls back to defaults
 * compiled into the stock image (0x080261BE..0x08026506) when that sentinel is
 * erased or zero. The saved config itself was narrowed to MCU internal flash at
 * 0x08006000 (analysis_v120/w25q128_flash_map_2026-06-13.md).
 *
 * Nobody has ever read those 4 KB, on any unit. They could not: flash read
 * protection (FLASH_OBR bit1 RDPRT) is active, so an external debugger cannot
 * dump MCU flash — and on this part merely bringing up the debug port kills the
 * running CPU (analysis_v120/hardfault_idle_task_2026-08-11.md).
 *
 * But RDP only blocks EXTERNAL access. The CPU reads its own flash normally.
 * So the firmware can do what the debugger cannot.
 *
 * WHAT IT ANSWERS
 *   - Is there per-device calibration on this platform at all?
 *   - If our reflashing destroyed it, this sector reads blank, which is itself
 *     the answer (and means bench unit #1 can no longer settle the question).
 *   - The CRC32 gives every user a single number to report, so "do units differ"
 *     becomes answerable without anyone shipping a 4 KB dump around.
 *
 * SAFETY — the entire point of this module
 *   - It performs NO writes, of any kind, to anything. No erase, no program, no
 *     option bytes, not to MCU flash and not to the W25Q.
 *   - The region is read through a const volatile pointer. There is no code path
 *     here that could modify it.
 *   - It is only built into `make guest-caldump`, which links at 0x08007000 and
 *     therefore cannot overlap 0x08006000-0x08006FFF even accidentally.
 *   - It never returns, so no later init (FPGA, settings store, scheduler) runs.
 *     A diagnostic image should do one thing.
 *
 * CAVEAT ABOUT FLASHING TO MEASURE: flashing this image via the factory IAP
 * should not disturb 0x08006000 — stock's own app links at 0x08007000 and its
 * saved config lives at 0x08006000, so the factory IAP must preserve that
 * sector or stock would lose its settings on every official update. That is a
 * strong inference, not a proof. A blank result on a unit that was previously
 * reflashed by us therefore cannot distinguish "never had cal" from "we erased
 * it" from "the IAP erased it".
 */

#ifndef CAL_DUMP_H
#define CAL_DUMP_H

/* Base and length of the MCU-internal saved-config sector under inspection. */
#define CAL_DUMP_BASE   0x08006000u
#define CAL_DUMP_LEN    0x00001000u   /* 4 KB */

/* Render the report and never return. Read-only; see the header comment. */
void cal_dump_run(void) __attribute__((noreturn));

#endif /* CAL_DUMP_H */
