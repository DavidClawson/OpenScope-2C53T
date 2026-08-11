/*
 * OpenScope 2C53T — fault capture. See fault.h for why this exists.
 */

#include "fault.h"
#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

/* .noinit is NOLOAD in the linker script, so startup neither zeroes nor loads
 * it: the record survives a reset and can be reported on the next boot. */
volatile fault_record_t g_fault __attribute__((section(".noinit")));

static bool  prev_valid;
static char  prev_summary[80];

/* Captured before g_fault is overwritten, so the CURRENT boot can still report
 * what the PREVIOUS one did. */
static fault_record_t prev;

static const char *kind_name(uint32_t k)
{
    switch (k) {
        case FAULT_KIND_HARD:       return "HARD";
        case FAULT_KIND_MEMMANAGE:  return "MEM";
        case FAULT_KIND_BUS:        return "BUS";
        case FAULT_KIND_USAGE:      return "USAGE";
        default:                    return "?";
    }
}

void fault_init(void)
{
    prev_valid = (g_fault.magic == FAULT_MAGIC);
    if (prev_valid) {
        memcpy(&prev, (const void *)&g_fault, sizeof(prev));
        snprintf(prev_summary, sizeof(prev_summary),
                 "%s pc=%08lx lr=%08lx cfsr=%08lx t=%lu %s",
                 kind_name(prev.kind),
                 (unsigned long)prev.pc, (unsigned long)prev.lr,
                 (unsigned long)prev.cfsr, (unsigned long)prev.tick,
                 prev.task);
    } else {
        prev_summary[0] = '\0';
    }

    /* Keep the record for inspection but bump the boot counter so a stale
     * record is distinguishable from one produced this boot. */
    uint32_t boots = prev_valid ? prev.boot_count : 0u;
    g_fault.boot_count = boots + 1u;

    /* Enable MemManage / BusFault / UsageFault. Without these every fault
     * escalates to HardFault and the specific cause is lost — which is exactly
     * what happened to the ~55 s fault. */
    SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk |
                   SCB_SHCSR_BUSFAULTENA_Msk |
                   SCB_SHCSR_USGFAULTENA_Msk);

    /* CFSR is write-1-to-clear and nothing ever cleared it, so its bits used to
     * accumulate across every fault since reset. Clear it so the next value
     * describes ONE event. */
    SCB->CFSR  = SCB->CFSR;
    SCB->HFSR  = SCB->HFSR;

    /* Precise bus faults are much easier to attribute than imprecise ones.
     * ACTLR.DISDEFWBUF forces the write buffer off so a faulting store reports
     * at the instruction that caused it rather than several instructions later.
     * Costs a little store throughput; worth it while this bug is open.
     *
     * ACTLR is at 0xE000E008 and is not a member of SCB_Type in this CMSIS
     * version, hence the direct access. */
    *(volatile uint32_t *)0xE000E008u |= (1u << 1);
}

bool fault_have_previous(void)      { return prev_valid; }
const char *fault_previous_summary(void) { return prev_summary; }

/*
 * Record and stop. Deliberately minimal and allocation-free: the record is
 * written FIRST, so if anything later in this function faults again (which is
 * how the core ends up in LOCKUP with pc=0xFFFFFFFE) the evidence is already
 * safe in .noinit.
 */
static void fault_capture(uint32_t *frame, uint32_t exc_return, uint32_t kind)
{
    g_fault.magic = 0;                    /* invalid while being written */

    g_fault.kind  = kind;
    g_fault.cfsr  = SCB->CFSR;
    g_fault.hfsr  = SCB->HFSR;
    g_fault.dfsr  = SCB->DFSR;
    g_fault.afsr  = SCB->AFSR;
    g_fault.mmfar = SCB->MMFAR;
    g_fault.bfar  = SCB->BFAR;

    g_fault.r0   = frame[0];
    g_fault.r1   = frame[1];
    g_fault.r2   = frame[2];
    g_fault.r3   = frame[3];
    g_fault.r12  = frame[4];
    g_fault.lr   = frame[5];
    g_fault.pc   = frame[6];
    g_fault.xpsr = frame[7];

    g_fault.exc_return = exc_return;
    g_fault.sp         = (uint32_t)frame;
    g_fault.tick       = (uint32_t)xTaskGetTickCountFromISR();

    /* Best effort — pcTaskGetName walks pxCurrentTCB, which may itself be the
     * thing that is corrupt, so copy defensively rather than trusting strncpy
     * on a wild pointer. */
    g_fault.task[0] = '\0';
    {
        TaskHandle_t h = xTaskGetCurrentTaskHandle();
        if (h != NULL) {
            const char *n = pcTaskGetName(h);
            if (n != NULL) {
                for (unsigned i = 0; i < sizeof(g_fault.task) - 1u; i++) {
                    char c = n[i];
                    g_fault.task[i] = c;
                    if (c == '\0') break;
                }
                g_fault.task[sizeof(g_fault.task) - 1u] = '\0';
            }
        }
    }

    __DMB();
    g_fault.magic = FAULT_MAGIC;
    __DMB();

    /* Stop here rather than resetting: the frozen state is what we have been
     * reading over SWD, and the record is in .noinit either way. If the IWDG is
     * running it will reset us and the record still survives. */
    for (;;) { }
}

/*
 * EXC_RETURN bit 2 selects which stack the core pushed the frame onto: 0 = MSP
 * (fault in a handler), 1 = PSP (fault in a task). Getting this wrong means
 * reading eight words of unrelated stack and inventing a plausible, wrong
 * answer — so it is resolved here rather than guessed at afterwards.
 */
#define FAULT_ENTRY(name, kind)                     \
    __attribute__((naked)) void name(void)          \
    {                                               \
        __asm volatile (                            \
            "tst   lr, #4              \n"          \
            "ite   eq                  \n"          \
            "mrseq r0, msp             \n"          \
            "mrsne r0, psp             \n"          \
            "mov   r1, lr              \n"          \
            "mov   r2, %0              \n"          \
            "b     fault_capture_c     \n"          \
            :: "i" (kind) : "r0", "r1", "r2"        \
        );                                          \
    }

void fault_capture_c(uint32_t *frame, uint32_t exc_return, uint32_t kind);
void fault_capture_c(uint32_t *frame, uint32_t exc_return, uint32_t kind)
{
    fault_capture(frame, exc_return, kind);
}

FAULT_ENTRY(HardFault_Handler,  FAULT_KIND_HARD)
FAULT_ENTRY(MemManage_Handler,  FAULT_KIND_MEMMANAGE)
FAULT_ENTRY(BusFault_Handler,   FAULT_KIND_BUS)
FAULT_ENTRY(UsageFault_Handler, FAULT_KIND_USAGE)
