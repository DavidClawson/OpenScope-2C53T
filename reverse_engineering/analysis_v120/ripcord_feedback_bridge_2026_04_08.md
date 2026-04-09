# ripcord Cross-Check and Feedback (2026-04-08)

This note folds the useful `ripcord` findings into the main `osc` reverse-
engineering trail and records the main cautions.

Primary `ripcord` inputs:

- `/Users/david/Desktop/ripcord/notes/scope_acquisition_spec.md`
- `/Users/david/Desktop/ripcord/notes/fpga_interaction_analysis.md`
- `/Users/david/Desktop/ripcord/notes/fpga_version_evolution.md`

## What We Should Adopt

### 1. `scope_acquisition_spec.md` is directionally strong

The strongest carry-forward points are:

- boot/runtime prerequisites really are staged
- `PB11`, `PC6`, `PB6`, USART2, SPI3, H2 upload, scheduler, and timer-driven
  runtime all matter
- the grouped scope-entry families are a useful working model
- timer-driven re-arm remains a better stock match than display-driven polling

This lines up with the current `osc` view that scope bring-up is not one magic
byte; it is a staged choreography.

### 2. `fpga_version_evolution.md` is strong at the peripheral-surface level

The useful conclusion is that the FPGA-facing hardware surface looks stable from
V1.0.7 onward. That is a good prioritization result:

- later builds are unlikely to reveal a different SPI3/USART2 architecture
- version diffing is still useful, but mainly for narrowing which higher-level
  state paths changed

## What We Should Treat Cautiously

### 1. `fpga_interaction_analysis.md` over-assigns some roles

The current hardware-confirmed map in `osc` suggests:

- `PB6` is the software-controlled SPI3 chip select
- `PC6` is an enable/gate line, not the primary chip-select line
- `PB11` is an FPGA active-mode control, not a display SPI bit-bang line

So the following `ripcord` claims should not be imported as facts:

- `PC6` as the SPI chip-select line
- PB11 bit-bang/display-role claims
- strong “main loop” / “scope acquisition task” labels from zero-callers alone

### 2. The “exact wire-level scope entry” wording is too strong

The biggest cross-check result is that `ripcord`'s grouped-family model is
useful, but its wire-level wording around `0x0B..0x11` is too strong.

Newer `osc` queue-split work suggests at least part of that family is better
understood as upstream/internal selectors that are translated before final
16-bit UART TX words are enqueued.

So the right way to reuse the `ripcord` note is:

- keep the group ordering as a staging model
- do not assume every listed `cmd_lo` is a directly observed final wire value

## Where ripcord and Current osc RE Agree

These are the strongest common threads:

- scope mode depends on staged prerequisites, not just SPI3 electrical setup
- grouped configuration families matter more than any single guessed byte
- timer cadence and re-arm sequencing matter
- scope failure is more likely a missing orchestration/state problem than a
  broken SPI worker

## Where Current osc RE Has Moved Further

The current `osc` work has gone further on:

- separating internal selector families from final wire traffic
- tracing the right-panel/state-5 cluster and the `E1C = 2 -> 0x2A` handoff
- identifying packed scope-state presets as part of runtime re-entry into the
  shared bank emitter

That means `ripcord` is currently best used as:

- architectural support
- version-prioritization support
- a source of good timer/boot/runtime questions

It is not yet the best authority for:

- exact pin-role claims
- exact task-role names
- exact final wire-level scope-entry bytes

## Practical Takeaway

For the active scope-mode search, the best combined working model is:

1. Keep `ripcord`'s staged prerequisite and grouped-family model.
2. Keep `osc`'s newer queue-split and packed-state runtime model.
3. Treat the remaining blocker as “missing preset + queue + timer choreography”
   before assuming a missing single command byte.

## Related ripcord Feedback Note

The team-facing feedback memo is also saved at:

- `/Users/david/Desktop/ripcord/notes/osc_project_feedback_2026_04_08.md`
