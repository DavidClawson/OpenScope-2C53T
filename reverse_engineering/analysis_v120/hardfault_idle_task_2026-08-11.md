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

---

# Update — the fault handler is validated, and the ~55 s claim is now in doubt

## The self-test passed completely

`make guest-faulttest` fires `udf #0` from the shell task ~10 s after boot.
**With no debugger attached, the UI froze at ~10 s** — so the fault fires, reaches
the handler, and freezes the device on its own. Attaching afterwards read:

```
magic      = FA17ED00   valid record
kind       = 4          UsageFault — identified specifically, not as a HardFault
cfsr       = 00010000   UNDEFINSTR only: one clean event, because CFSR is now
                        cleared at boot instead of accumulating since reset
hfsr       = 00000000   did NOT escalate — the configurable handlers work
r3         = 000003E8   = 1000, our exact trigger (dbg_shell_loops == 1000)
exc_return = FFFFFFFD   PSP / task context, correctly resolved from EXC_RETURN
pc         = 08019F48 -> `udf #0` in vUsbDebugTask, usb_debug.c:2733
sp         = 20031100
cyccnt     = D881B234
```

Byte-exact on every field. The instrument is sound.

## Which undermines the headline of this document

The handler demonstrably records a task-context fault into `.noinit`. Yet the
"~55 s HardFault" produced **no record at all** — `magic` stayed uninitialised
garbage across two separate attempts, even though `SHCSR` showed `USGFAULTACT`
and `.noinit` is provably writable (`fault_init()` wrote `boot_count` four words
along).

A handler that captures a deliberate fault perfectly but never captures the
"real" one is evidence that the real one is not the same kind of event.

**And the user reports the UI comes up fine and only halts "when we ran some of
the tests".** Re-reading the timeline: the loop counter froze at 5479 iterations
= 54.79 s, which is about when the FIRST OpenOCD attach happened. Every freeze
observed so far has been noticed *after* attaching.

So the likely correct statement is **not** "the firmware HardFaults ~55 s into
every boot" but **"attaching the debugger puts it into a fault"** — plausibly a
DEMCR vector-catch halt configured by OpenOCD at `init`, which stops the core on
exception entry *before* the handler's first instruction. That would explain
`USGFAULTACT` set with nothing recorded.

**This is a confound I built a theory on top of without controlling for it**, and
it is the same mistake pattern as the `/2` status reads: an anomaly observed
only through an instrument, attributed to the target rather than the instrument.

## What still stands

* The fault handler, CFSR clearing, and `.noinit` record are real improvements
  and are validated.
* `pc = 0xFFFFFFFE` with `IPSR = 3` is the Cortex-M LOCKUP signature, not
  self-evidently "an RDP artifact". Whether it is provoked by attaching is now
  the open question — and if it is, that is *still* worth knowing, because it
  means SWD-derived readings need care.

## The experiment that settles it

Flash a normal `make guest`, boot it, and **leave it alone with no debugger for
5+ minutes, watching the UI**. Nothing else.

* **UI stays alive** -> there is no ~55 s bug; the freeze is debugger-induced,
  this document's headline is wrong, and the remaining question is what OpenOCD
  does at attach.
* **UI freezes on its own** -> the bug is real. Attach afterwards and read
  `g_fault`, which we now know works.

Cost: one flash and five minutes of not touching anything.

---

# RESOLVED — flash read protection kills the CPU the moment SWD attaches

## The measurement

Device left alone with no debugger: **UI still animating after 5 minutes.**
ST-Link merely plugged in: **still animating.** Bare
`openocd -c init -c targets -c shutdown`: **frozen.** Same with a minimal config
that omits `target/stm32f1x.cfg` entirely (no flash bank, no DBGMCU poke) —
`scripts/at32_minimal.cfg`. So it is not the config; it is enabling debug.

```
FLASH_OBR = 0x03FFFFFE   bit1 RDPRT = 1  -> READ PROTECTION ACTIVE
option bytes @0x1FFFF800 = FFFFFFFF      -> blocked
flash @0x08007000        = FFFFFFFF      -> blocked
SRAM  @0x20000000        = 20000004      -> readable, fine
```

## The mechanism

RDP is set. On STM32F1-class parts and their AT32 equivalents this is not merely
"the debugger cannot read flash" — **bringing up the debug port disables the
flash array**, as an anti-extraction measure. The CPU executes from flash, so at
the instant of attach it can no longer fetch instructions:

```
attach -> flash array disabled -> instruction fetch fails
       -> IACCVIOL / UNDEFINSTR -> HardFault
       -> the fault handler is ALSO in flash, unfetchable
       -> LOCKUP, pc = 0xFFFFFFFE, IPSR = 3
```

## What this explains, all at once

| Observation | Cause |
|---|---|
| freezes on every attach | flash disabled, CPU cannot fetch |
| `pc=0xFFFFFFFE`, `IPSR=3` on every halt | genuine LOCKUP |
| CFSR shows IACCVIOL + UNDEFINSTR | fetching from a dead flash array |
| the fault handler never recorded the "~55 s fault" | handler is in flash |
| **the RTT console never worked** | RTT needs a LIVE CPU while the host reads SRAM |
| the self-test DID record | `udf` fired at 10 s with nothing attached: flash alive, handler ran, record written to SRAM; we attached afterwards and read it |

The self-test is the clean confirmation: the handler works, and only ever when
nothing is attached.

## Retraction

**There is no "~55 s HardFault". The device runs indefinitely.** The earlier
sections of this document claim a spontaneous periodic fault; that is wrong. The
counter froze at 54.79 s because that is when the first OpenOCD attach happened.
Everything downstream of that reading — including "any bench observation later
than ~55 s is suspect" — is withdrawn.

The `pc = 0xFFFFFFFE` note in `CLAUDE.md` was closer to right than my correction
of it: it IS an artifact of read protection. It is just not benign, and not a
readout glitch — the core really is locked up, because attaching killed it.

## What survives, and what does not

**Survives.** Exp E (register diff), Exp K and Exp L (anchored FPGA status
reads). All of these attach, let the CPU die, and then have the HOST drive SPI3
and read peripherals. Peripherals keep the state the firmware configured before
the attach, and the FPGA does not care why the CPU stopped. The spin-park images
still did their job: they stopped execution at the config-enable instant so the
peripheral state we read was the state at that instant.

**Does not survive.** Anything needing the CPU to RUN with a debugger attached:
* **the RTT console** — dead on this unit while RDP is set, in any form
* **the "SWD GPIO sampler on a running target"** (bench_session_plan_2026-07-30
  Task 2b) — same reason; it was never going to work
* live `mdw` polling of firmware variables

## Options

1. **Accept it.** SWD stays a halt-and-poke instrument: attach, CPU dies, host
   drives the bus. That is exactly how Exps E/K/L worked and they were the most
   productive SWD sessions this project has had. The on-LCD overlays remain the
   way to read live firmware state.
2. **Clear RDP.** Unprotecting triggers a mass erase by design — that removes the
   FACTORY IAP bootloader this unit depends on, not just the app. Recovery needs
   ROM DFU via BOOT0, i.e. opening the case, and afterwards the flashing
   workflow changes (our bootloader uses a different app slot). Real risk, real
   payoff: it would unlock RTT and the host-driven sweep loop that motivated all
   of this. **Not to be done casually, and not without checking we can restore
   the factory bootloader.**
