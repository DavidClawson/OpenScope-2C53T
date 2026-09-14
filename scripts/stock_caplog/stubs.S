/* Entry stubs for the caplog code cave. Both sites are patched in the stock
 * image with `b.w stub` + `nop` over six bytes; the displaced instructions
 * are replayed here and control returns with an absolute branch.
 *
 * Site 1 — dvom TX task, 0x0803E44C:
 *     0803e44c: 200a        movs r0, #10
 *     0803e44e: f002 ff9f   bl   0x08041390     (vTaskDelay)
 *     0803e452: e7e5        b.n  0x0803e420     <- return here
 *   The frame at 0x20000005 is complete and USART2 TDBE is already armed.
 *   lr is dead (the task never returns), r12 is scratch, r4-r9 are preserved
 *   by the C callee.
 *
 * Site 2 — SysTick handler, 0x0802A994:
 *     0802a994: b580        push {r7, lr}
 *     0802a996: f04f 0010   mov.w r0, #16
 *     0802a99a: f380 8811   msr  BASEPRI, r0    <- return here
 *   The push is replayed first so EXC_RETURN is on the stack before the call
 *   clobbers lr; the handler ends with pop {r7, pc}.
 */
    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .text.stubs, "ax", %progbits

    .global caplog_stub_tx
    .thumb_func
caplog_stub_tx:
    bl      caplog_tx
    movs    r0, #10
    ldr     r12, =0x08041391        /* vTaskDelay, thumb */
    blx     r12
    ldr     r12, =0x0803E453        /* back to the task loop */
    bx      r12

    .global caplog_stub_tick
    .thumb_func
caplog_stub_tick:
    push    {r7, lr}
    bl      caplog_tick
    mov.w   r0, #16
    ldr     r12, =0x0802A99B        /* msr BASEPRI, r0 */
    bx      r12

    .pool
