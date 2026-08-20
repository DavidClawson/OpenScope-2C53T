# Spec: Module loader (guided procedures on-device)

**Track:** modules
**Stage now:** S0 — 17 procedure files across four domains exist with a
provisional schema (`modules/README.md`), and **nothing in the firmware reads
them**. This is dev-plan item D1 wearing spec clothes.
**Champion:** —

## What it is

Pick "Automotive → CAN bus wiring check" from a menu; the device shows the
setup, the safety notes, then steps you through the procedure with
expected-vs-fail guidance at each step. The differentiating feature of the
whole project for non-electronics trades.

## Prior art

No instrument in this class has anything like it. The nearest analogues are
Fluke's application notes (paper) and the automotive scopes (Pico) whose
guided tests cost 10–100× this device. `docs/ideas/feature_catalog.md`
sketches the market by trade; `modules/README.md` holds the schema and — 
critically — the **instrument-reality table** that constrained the seed
content: no module reads a number off the scope, no module needs two
instrument functions at once.

## Our angle

Domain knowledge is the scarce ingredient and it doesn't require C:
CONTRIBUTING.md already green-lights module contributions. A loader turns 17
JSON files from documentation into product, and every future contribution
lands on-device with zero firmware changes.

## Hardware dependencies

Almost none — that's the point. The seed content was deliberately written
against the instrument as it is (qualitative scope steps, quantitative meter
steps only). Two real dependencies:

- **Storage:** W25Q residency needs the region layer's read path (dev-plan C1
  gate). But v1 can **embed the JSON in the image** — 17 files is tens of KB,
  and it sidesteps C1 entirely. Recommendation: embed first, W25Q later.
- **Coexistence:** several procedures are meter-mode; in `guest-coldtrace`
  the meter is dead (see `meter/meter-in-the-scope-build.md`). The schema's
  `firmware_build` field already handles this honestly — the loader must
  refuse or warn when the required build isn't the running one.

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| S0+ (guard first) | A host-side schema validator in the test suite: every file in `modules/` parses and conforms; a deliberately malformed fixture must fail. (This is pre-S1 on purpose — the content should be guarded before the loader exists, so the loader is written against verified input.) |
| S1 | One procedure (recommend `automotive/can_bus_wiring_ohms`) renders on-device from the embedded pack: setup screen, safety screen, steps navigable by button, in the build its `firmware_build` field names. |
| S2 | That procedure run end-to-end on the bench against real hardware (two 120 Ω resistors): the 60 Ω check passes, and the single-terminator fault reads ~120 Ω and shows the module's `fail` text. Writeup in `docs/experiments/`. |
| S3 | Schema validator extended to the trust rules: a `quantitative: false` module that declares a numeric measurement fails validation; interpreter host-tested over fixture procedures. |
| S4 | Module browser by category; knowledge-base entries reachable from steps; W25Q-resident packs loadable without reflashing (now the C1 dependency bites). |

## Open questions

1. JSON on a Cortex-M4: parse at build time into a generated C table (no
   runtime parser, schema errors become build errors — recommended), or embed
   a minimal JSON reader? The build-time route also *is* the S0+ validator.
2. Does the interpreter ever get to set the instrument (mode/range/timebase),
   or stay read-only with manual-setup instructions, as the schema assumes
   today? Read-only ships sooner; revisit at S4.
