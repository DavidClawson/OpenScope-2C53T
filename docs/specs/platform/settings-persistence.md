# Spec: Settings persistence that actually persists

**Track:** platform
**Stage now:** S2, with an **open defect** — the boot reconcile has never once
been observed to restore a persisted timebase (three consecutive failures,
2026-08-19, including one committed through a documented immediate-flush point)
**Champion:** DavidClawson

## What it is

Change a setting, power off however you like, power on: the device is the way
you left it. Today that is true only sometimes, and — worse — we cannot yet
say *why* the failing case fails.

## Prior art

Wishlist Tier 1 #2's sting is exactly this: stock "reverts to Automatic mode
and… beeps at you" every mode re-entry. Every instrument owners like remembers
its state. Stock persists via a saved config in MCU internal flash
(`0x08006000`); ours appends records to the W25Q through the region layer.

## Our angle

The append-log design is right (wear-friendly, stock regions untouched). What
was missing is the project's own standard applied to itself: the store has
carried five diagnostic fields (`storage_bound`, `load_result`, `writes`,
`write_failures`, `changes_seen`) since it was written, and **no caller ever
read one** — the same shape as the `/2` SSPI reads and the unread `vdiv_table`.
The `settings` shell command (commit 5e659f2) finally wires the instrument.

## Hardware dependencies

None beyond the W25Q region layer, which is live. The blocker is diagnostic,
not hardware: the failure is invisible from the front panel because "write
never reached flash" and "load never came back" present identically (boot
lands on the arm block's 0x08 either way).

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| S2 (repair) | Flash 5e659f2; read `settings` before and after a change + power cycle. The five fields must localise the failure to the write half or the load half. Then fix that half. Done when: change timebase → orderly power-off → boot prints `boot reconcile: pushed the persisted code` and the display shows the persisted code. Three consecutive passes (the failure was three consecutive, so the pass must be too). |
| S3 | Host test over the append-log encode/decode with corrupted-record and torn-write cases; on-device: a scripted `bench.py` change→cycle→verify loop as an acceptance test. Negative control: a build with the write path stubbed must fail the loop. |
| S4 | Kill the settle-window foot-gun: either write-through on every change (measure flash wear first) or make the 2 s window flush on *every* screen exit, so "a change carries only if a later button press follows" stops being a sentence users need to read. |

## Open questions

1. If the diagnosis says the *write* half fails only on some boundaries, is
   the MENU flush actually executing, or gated behind a condition that isn't
   met in scope mode? (`settings_store_flush` exists; who calls it, when?)
2. Should timebase persist at all when the persisted code is unmeasured?
   Current reconcile policy says no (pull down to 0x08). Revisit once codes
   0x09–0x0C get rates.
