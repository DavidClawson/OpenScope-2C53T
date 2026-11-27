/*
 * mcu_cal.h — read-only decode of the factory calibration page.
 *
 * WHAT THIS IS
 * ------------
 * The per-device factory calibration lives in the MCU saved-config page at
 * 0x08006000 (see factory_cal_truth_2026-08-14.md and
 * archive/factory_cal/README.md). The calibration region is page offsets
 * 0x038..0x127: 60 uint32 words, word i = (B[i] << 16) | A[i], where A and
 * B are each uint16[60]:
 *
 *   A[ 0..29]  CH1 trigger DAC baselines  (RAM mirror ms+0x260, 0x20000358)
 *   A[30..59]  CH1 trigger DAC uppers     (RAM mirror ms+0x29C)
 *   B[ 0..29]  CH2 trigger DAC baselines  (RAM mirror ms+0x2D8)
 *   B[30..59]  CH2 trigger DAC uppers     (RAM mirror ms+0x314)
 *
 * Stock's gpio_mux_portc_porte (0x080088A4) indexes A[range]/A[30+range] for
 * CH1 (ranges 0..9) and gpio_mux_portab (0x08008A58) indexes B likewise for
 * CH2, computing DAC = ((upper - base) / 200) * (level + 100) + base.
 *
 * SAFETY — same discipline as cal_dump.c: read-only. The page is accessed
 * through a const volatile pointer; nothing here can write anything.
 *
 * VALIDITY (stock behavior, 0x0802d19e): signature byte [0x00] must be
 * 0x55, and the sentinel B[59] must be nonzero and != 0xFFFF — otherwise
 * stock treats the table as absent and rewrites compiled-in defaults. We
 * mirror that: an invalid page reports "not available" and callers fall
 * back to their own placeholders.
 */

#ifndef MCU_CAL_H
#define MCU_CAL_H

#include <stdbool.h>
#include <stdint.h>

/* Saved-config page base, same constant as cal_dump.h. */
#define MCU_CAL_PAGE_BASE   0x08006000u

/* Calibration region within the page. */
#define MCU_CAL_OFF         0x038
#define MCU_CAL_WORDS       60

/* Signature byte at page offset 0. */
#define MCU_CAL_SIG         0x55

/* Sentinel: B[59] (word 59 high half). 0xFFFF or 0 means "no table". */
#define MCU_CAL_SENTINEL    0xFFFFu

/* Ranges the stock trigger path indexes (cases 0..9). */
#define MCU_CAL_RANGES      10

typedef enum {
    MCU_CAL_OK = 0,          /* page decoded, tables available        */
    MCU_CAL_NO_PAGE,        /* signature != 0x55                    */
    MCU_CAL_NO_TABLE,       /* sentinel says stock rewrote defaults */
} mcu_cal_status_t;

/* One-shot decode (cheap; call freely). Returns MCU_CAL_OK iff the
 * page carries a valid factory table. */
mcu_cal_status_t mcu_cal_status(void);

/* CH1 (A) / CH2 (B) trigger pair for range 0..9: *base = A[range] or
 * B[range], *upper = A[30+range] or B[30+range]. Returns false (and
 * leaves *base/*upper untouched) if the page is invalid or range is out
 * of domain. */
bool mcu_cal_trigger_pair(uint8_t ch, uint8_t range, uint16_t *base, uint16_t *upper);

#endif /* MCU_CAL_H */
