---
name: bench-experiment
description: Run and record a hardware experiment on the 2C53T bench using a controlled five-step cycle. Use whenever a bench measurement is about to be taken, a hypothesis tested, or a prior result questioned.
---

# Bench experiment cycle

Every measurement on this project goes through the same five steps, plus two
fields that exist because of how this project has actually failed.

## Why the extra fields

This project's wrong answers have almost never come from bad hypotheses. They
came from **instruments that could not have detected what they claimed to
exclude**, and which returned a stable, plausible number anyway. A stable wrong
number is indistinguishable from a right one.

Documented cases: SSPI status reads at `/2` (garbage, believed for weeks);
PB4/MISO floating (every status read taken on a line with no defined idle
level); Exp F comparing output LEVELS, which cannot distinguish a pin driven LOW
from one left FLOATING, so it "excluded" a class it could not see; a fixed-bin
DFT that reported 1.44 where a peak search found 22.8; `fpga scope range <n> 0`
silently addressing BOTH channels because the argument is 1-based; `usart tx`
queueing into a task that a coldtrace build never creates, so the frames were
never transmitted at all.

Two rules follow, and they are not optional:

1. **A negative result is worthless until a positive control has been shown to
   work through the same path, in the same session.** "X produced no signal"
   means nothing until *something* has produced a signal through that exact
   instrument.
2. **State what the test cannot see.** If you cannot name a blind spot, you have
   not understood the instrument.

## The cycle

Write one file per experiment in `docs/experiments/`, named
`YYYY-MM-DD-NN-short-slug.md`, using `docs/experiments/TEMPLATE.md`.

1. **Problem** — the open question this serves. One sentence, and it must be a
   question the project actually has, not a restatement of the procedure.
2. **Hypothesis** — a prediction with a *falsifier*: "if H is true we see A; if
   false we see B." If both outcomes are consistent with the hypothesis, stop
   and rewrite it.
3. **Procedure** — exact commands, pin states, build flags, signal settings.
   Include the **preconditions you verified by readback** rather than assumed
   (register values, `S1:0347`, `CTRL1=0x0000202C`, IDCODE anchor `0x0120681B`).
4. **Control** — the known-good case, run in the same session through the same
   path. Record its result *first*. If the control fails, the experiment is
   VOID, not negative, and must be recorded as such.
5. **Results** — raw numbers, not impressions. Screen observations are weak
   evidence and must be labelled as such.
6. **Blind spots** — what this test could not have detected.
7. **Conclusion** — what is now established, what is excluded, and explicitly
   what is *not*. Prefer "void" over "negative" when the control did not hold.

## Recording conventions

- **Withdraw in place, never delete.** A withdrawn result stays in the file with
  a `WITHDRAWN <date>: reason` banner. Deleted mistakes get re-made.
- **Anchor FPGA measurements**: read IDCODE (`0x11`) at `/256` and confirm
  `0x0120681B` before trusting any config-port read.
- **Never read Gowin STATUS (`0x41`) during capture** — it desynchronises a
  running configured part and only a true power cycle recovers it.
- A **true power cycle** is POWER → "Goodbye" → **unplug USB** → replug. The
  pinhole reset and a POWER-button shutdown with USB attached both leave the
  FPGA powered.

## Git workflow

- One branch per bench session: `bench/YYYY-MM-DD`.
- One commit per completed cycle, message `exp(NN): <one-line conclusion>`,
  carrying the experiment file plus any firmware change it required.
- At session end, write/extend the narrative post in `docs/devlog/` and merge to
  `main`. The devlog is the story; `docs/experiments/` is the evidence.

## Scale to the question

A five-minute register poke does not need a full file — but it does still need
the control. If you are about to record a *negative*, or to exclude something,
write the file.
