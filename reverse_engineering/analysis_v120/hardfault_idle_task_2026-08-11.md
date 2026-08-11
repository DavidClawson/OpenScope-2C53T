# The bench unit HardFaults ~55 s after every boot (2026-08-11)

Found while validating the RTT console. It is not an RTT bug — it predates that
work — and it is almost certainly the intermittent "device seems halted" /
dark-screen / splash-hang behaviour this unit has shown for weeks.

## The measurement

Instrumented `guest-rtt` build, clean boot, nothing attached but the ST-Link:

```
dbg_shell_entered = 0xA5A5A5A5     the shell task did start
dbg_shell_loops   = 0x1567 = 5479  and then stopped
xTickCount        = 0xD5FC = 54780
```

5479 idle iterations x 10 ms = 54,790 ms against a tick of 54,780 ms — they agree
to within one iteration. **The whole scheduler stopped at ~54.8 s**, not just the
shell task. Confirmed frozen across three separate OpenOCD attaches spanning
~15 s, so it is not an artifact of attaching.

## Why the tick froze

```
ICSR  = 0x0442D803   VECTACTIVE = 3  -> HardFault active
                     PENDSTSET set   -> SysTick pending, permanently starved
HFSR  = 0x40000000   FORCED          -> escalated from a configurable fault
SHCSR = 0x00000000   mem/bus/usage handlers disabled, hence the escalation
SysTick CTRL = 0x00010007            still enabled and counting
```

SysTick is alive and *pending*; it simply cannot run because HardFault outranks
it. That is why the CPU reports `running` while every task is dead.

## Not RTT — the A/B

Reflashed `OPENSCOPE_expQ_sweep2.bin` (2026-07-28, pre-RTT), let it sit a few
minutes:

| | pre-RTT | RTT build |
|---|---|---|
| ICSR | `0x0442D803` (VECTACTIVE=3) | `0x0442D803` (VECTACTIVE=3) |
| HFSR | `0x40000000` FORCED | `0x40000000` FORCED |
| CFSR | `0x00010001` IACCVIOL + UNDEFINSTR | `0x00008201` IACCVIOL + PRECISERR |

`IACCVIOL` in both: the core tried to fetch an instruction from a
non-executable address. (CFSR is write-1-to-clear and this firmware never clears
it, so its bits ACCUMULATE across every fault since reset — the two columns are
not necessarily one event each.)

## The faulting frame — identical in both builds

`LR = 0xFFFFFFFD` on entry, so the fault was taken from Thread mode on the PSP,
i.e. inside a task.

```
              pre-RTT                RTT build
R0            0x00000000             0x00000000
R1  R2  R12   0xA5A5A5A5 (x3)        0xA5A5A5A5 (x3)   <- FreeRTOS stack fill
R3            0x00000001             0x00000001
xPSR          0x61000000             0x61000000
PC / LR       0x0801E024/0x0801E07D  0x0801EAC8/0x0801EB19
```

Symbolised against the RTT build's ELF:

```
PC 0x0801EAC8 -> prvCheckTasksWaitingTermination   FreeRTOS/tasks.c:6120
LR 0x0801EB18 -> prvIdleTask                       FreeRTOS/tasks.c:5861
```

and the instruction at that PC is the function's *first* load:

```
0801eac0 <prvCheckTasksWaitingTermination>:
 801eac2:  ldr r4,[pc,#56]    ; r4 = 0x2002e8ac  (uxDeletedTasksWaitingCleanUp)
 801eac8:  ldr r3,[r4,#0]     ; <-- stacked PC
 801eaca:  cbnz r3, ...       ; list is empty, would return immediately
```

## What is ruled out

* **Stack overflow** — the stack around the frame is untouched `0xA5A5A5A5` fill,
  so there was ample headroom, and `vApplicationStackOverflowHook` never ran (it
  paints "STACK OVERFLOW" on the LCD; the screen shows the normal, frozen UI).
* **Heap exhaustion** — `xFreeBytesRemaining` = 20,576 of 32,768, and
  `xMinimumEverFreeBytesRemaining` is the *same* number, so nothing ever came
  close. `vApplicationMallocFailedHook` never ran either.
* **A corrupted termination list** — read after the fault:
  `uxDeletedTasksWaitingCleanUp = 0`, and `xTasksWaitingTermination` is a valid
  EMPTY list (`uxNumberOfItems=0`, `pxIndex` -> `xListEnd`, sentinel
  `0xFFFFFFFF`). There was nothing to clean up and no `vTaskDelete` anywhere in
  `src/`.
* **An unbalanced FreeRTOS critical section** — `uxCriticalNesting = 0`.

So a plain load from a valid SRAM address, in a function that would have returned
immediately, appears to have faulted. That does not add up on its own, which is
why the accumulated-CFSR caveat above matters: the stacked frame may belong to a
*later* fault than the one that set `IACCVIOL`.

## Correction to a long-standing note

`CLAUDE.md` records that `pc = 0xfffffffe` with `IPSR = 3` on every halt is "an
RDP artifact" and that **"`pc` IS USELESS ON THIS TARGET"**. That is at best
incomplete: `0xFFFFFFFE` is the Cortex-M **LOCKUP** PC, and `IPSR = 3` is
HardFault. Both are exactly what a genuinely locked-up core reports. Some
previous SWD readings may have been taken from a dead device rather than a parked
one — the peripheral state would still read correctly (it was set before the
fault), which is precisely why this went unnoticed.

The Exp E/K parks remain trustworthy: they were taken seconds after boot, well
inside the ~55 s window, and their peripheral state matched what the code had
just configured.

## Why it matters beyond the console

**Any bench observation made more than ~55 s after boot is suspect.** Last
session's readings are fine — the sweep reported `DONE 12/12`, so the input task
was demonstrably alive — but the RTT-driven host loops this work was meant to
enable would run straight past the deadline. **The fault has to be fixed before
the RTT workflow is worth anything.**

## Next

1. Enable the configurable fault handlers (`SHCSR` UsageFault/BusFault/MemManage)
   and add a real HardFault handler that latches `CFSR`/`HFSR`/`BFAR`/`MMFAR` and
   the stacked frame into a known SRAM location. Right now every fault escalates
   and the evidence is accumulated rather than per-event.
2. Clear `CFSR` at boot so its bits describe one event instead of all of them.
3. Bisect: does a build from before the sweep/trace work also fault? The oldest
   staged image is 2026-07-28; anything older needs a rebuild.
4. Check whether it is time-based (~55 s) or count-based by instrumenting a
   second, independent counter.
