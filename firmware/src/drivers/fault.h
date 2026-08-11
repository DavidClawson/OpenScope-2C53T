/*
 * OpenScope 2C53T — fault capture
 *
 * Background (analysis_v120/hardfault_idle_task_2026-08-11.md): this unit
 * HardFaults ~55 s into every boot and has done for weeks. Diagnosing it was
 * needlessly hard because:
 *
 *   * MemManage / BusFault / UsageFault handlers were all DISABLED (SHCSR = 0),
 *     so every fault escalated to HardFault and the specific cause was lost.
 *   * CFSR is write-1-to-clear and nothing ever cleared it, so its bits
 *     ACCUMULATE across every fault since reset — the value describes a smear of
 *     events, not one event.
 *   * the default handlers are a bare `b .`, so nothing was recorded at all and
 *     the evidence had to be reconstructed by hand over SWD.
 *
 * This fixes all three. The record lives in .noinit so it SURVIVES a reset,
 * which matters for an intermittent fault: the device can die, reset, and still
 * tell you what happened on the previous boot.
 */

#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>
#include <stdbool.h>

#define FAULT_MAGIC 0xFA17ED00u

/* Which handler ran. Recorded because with the configurable handlers enabled a
 * BusFault no longer masquerades as a HardFault. */
typedef enum {
    FAULT_KIND_NONE = 0,
    FAULT_KIND_HARD,
    FAULT_KIND_MEMMANAGE,
    FAULT_KIND_BUS,
    FAULT_KIND_USAGE,
} fault_kind_t;

typedef struct {
    uint32_t magic;        /* FAULT_MAGIC once fully written */
    uint32_t kind;         /* fault_kind_t */
    uint32_t boot_count;   /* increments each boot; tells old records from new */

    /* SCB fault status, sampled before anything else can disturb it. */
    uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar;

    /* The exception frame the core stacked. */
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;

    uint32_t exc_return;   /* bit2: 0 = frame on MSP, 1 = on PSP */
    uint32_t sp;           /* the stack the frame was found on */
    uint32_t cyccnt;       /* DWT cycle counter at fault. NOT a FreeRTOS tick:
                            * the handler must not call FreeRTOS (see fault.c). */
    char     task[16];     /* left empty by the handler on purpose — identify the
                            * task offline from `sp`, which lands in its stack. */
} fault_record_t;

/* Non-static so the address comes out of the ELF with nm and the record can be
 * read over SWD on a dead target — which is how this one has to be read. */
extern volatile fault_record_t g_fault;

/*
 * Enable the configurable fault handlers, clear stale CFSR/HFSR bits, and note
 * whether the PREVIOUS boot died. Call early in main(), before anything that
 * could fault. Safe before the scheduler starts.
 */
void fault_init(void);

/* True if a valid record from a previous boot is present. */
bool fault_have_previous(void);

/* Human-readable one-liner for the previous fault, e.g.
 * "BUS pc=0801eac8 cfsr=00008201 t=54780 idle". Empty if there is none. */
const char *fault_previous_summary(void);

#endif /* FAULT_H */
