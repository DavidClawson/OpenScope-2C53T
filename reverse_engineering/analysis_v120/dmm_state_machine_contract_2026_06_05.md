# DMM State-Machine Contract and Low-DCV Guardrail

Date: 2026-06-05

This note records the software validation direction after the live low-DCV
failure. The decoder must stay stock-disassembly-grounded; it must not grow
observed-value coefficient hacks or a webcam/OCR-driven multiplier loop.

## Stock-Bound Contract

Stock-visible DCV math remains:

- raw digits are decoded as `d0*1000 + d1*100 + d2*10 + d3`
- `frame[2].3` extends raw by `+10000`
- voltage class priority is `frame[8].7`, then `frame[3].4`, then
  `frame[4].4`, then `frame[5].4`, then default class 0
- display value is `extended_raw / 10^class`

The current firmware tests intentionally preserve that contract, including the
live `0.200 V` failure frame:

```text
frame=5A A5 44 8E EF E7 07 24 80 00 01 89
stock decode: raw=4366, class=4, display=0.4366 V
visual source/load display: 0.200 V
```

That mismatch is unresolved frontend/range/calibration behavior. It is not
evidence for a one-point low-voltage multiplier.

The range-class guard is now exhaustive over the stock-visible bits: `all 16 combinations`
of `frame[8].7`, `frame[3].4`, `frame[4].4`, and `frame[5].4` are tested both
with and without the `frame[2].3` `+10000` extension. The expected class is
always the stock priority order above; no decoded magnitude may select or
override the class.

## Exhaustive Local Mode Contract

`firmware/tests/test_fpga_meter_plan.c` now asserts that the local DMM
state-machine covers every local UI submode and every recovered stock selector
slot:

| local submode | local meaning | stock slot | frame family |
| --- | --- | --- | --- |
| 0 | DCV | 0 | voltage |
| 1 | ACV | 1 | voltage |
| 2 | DC mA | 2 | current |
| 3 | DC A | 2 | current |
| 4 | AC mA | 3 | current |
| 5 | local AC A | 3 | current |
| 6 | resistance | 4 | resistance |
| 7 | continuity | 6 | continuity |
| 8 | diode | 7 | diode |
| 9 | capacitance | 5 | extended |
| 10 | local temperature | 5 | extended |

The test fails if a local submode has no valid stock slot, no valid frame
family, or no valid selector word, and it also fails if any recovered stock
slot stops being represented by the local model.

The same software contract now explicitly guards the shared local splits. DC
small-current and DC A must keep the same stock slot/selector/mux projection;
AC small-current and local AC A must keep the same stock slot/selector/mux
projection; capacitance and temperature must keep the same stock slot/selector/
mux projection. These are deliberate evidence boundaries over the
eight-entry stock selector table. A future hard input must not cause the port to invent a
new 0x05xx selector for one side of a local split unless a recovered stock
writer or repeatable stock-runtime trace proves it.

The current software model does not expose a separate uA local submode. uA is
unresolved and unexposed until stock range state or live current traces prove a
real microamp frontend/range path. Current tests therefore assert that the
implemented current submodes render only the recovered/local mA and A units,
not an invented uA unit.

## Current Evidence Boundary

The software contract proves parser/state safety only:

- wrong-family voltage frames clear stale current/passive readings
- low-DCV `frame[8]=0x80` and `frame[8]=0x82` class-4 voltage frames clear
  stale current/passive readings outside voltage modes
- a dedicated low-DCV stale-current guard now walks every implemented current
  submode `{2,3,4,5}` so DC mA, DC A, AC mA, and AC A all fail closed instead
  of retaining a prior current payload when a voltage-family class-4 frame
  appears
- the 32-case range-class matrix covers all stock bit combinations and the
  optional `+10000` extension
- marker-visible wrong-family frames are now covered as an explicit matrix:
  stock voltage metadata and the continuity segment marker must clear stale
  payloads in every local submode whose expected frame family differs
- the transition-plan model also has a pure frame-family policy matrix over
  every recovered family enum (`voltage`, `current`, `resistance`,
  `continuity`, `diode`, `extended`): once a frame family is known, only an
  exact expected/observed family match is acceptable, and invalid or mismatched
  families fail closed
- `frame[6] upper nibble 4` is guarded as a negative classifier boundary, not
  promoted to a global resistance-family marker: kOhm resistance frames use it,
  but current-family fixtures can also carry `0x4x` as status/hold metadata
- special/terminal voltage-family frames such as stock `OL` are also rejected
  across every non-voltage submode `{2..10}` instead of being tested only
  against one current mode and resistance
- diode and the shared capacitance/temperature extended slot have explicit
  formatter-debug guards: diode must stay on local diode family / stock mode 7
  and capacitance/temperature must stay on local extended family / stock mode 5;
  these tests pin stock formatter indices and local suffix overrides without
  claiming physical diode/capacitance/temperature calibration is solved
- AC modes fail closed without line-frequency evidence
- mode invalidation clears stale payloads before transition
- the first post-transition frames are discarded before parsing
- local current and extended splits remain local policy over shared stock slots
- the transition settle/discard policy guard is explicit: every valid local
  submode uses the same uniform local settle/discard policy, invalid submodes emit no settle/discard,
  and exact stock settle/discard counts remain open
- the transition phase matrix is exercised for every local submode: a
  `busy transition frame` is rejected without consuming the discard budget,
  then each planned discard frame drains in order, and only the following
  stable frame is accepted
- the transport-gate source-frame matrix walks every ordered source submode to
  every destination submode with the planned destination discard budget: while
  the transition is busy and while the destination discards drain, a valid
  prior-source frame is not handed to the parser and the invalidated destination
  payload remains blank
- the ordered mode-transition stale matrix now covers every source submode to
  every destination submode, so a valid prior-mode payload cannot survive a
  transition just because it remains numerically plausible

The goal gate now has explicit state-machine property anchors in
`scripts/validate_dmm_goal.py`, so passing the final command is tied to these
C invariants and not just to a thin live harness. The static anchors require
all local submodes 0..10, the 16 range-class bit combinations with both
`frame[2].3` extension states, AC-evidence rejection, wrong-family rejection,
the marker-visible low-DCV/continuity cases, the pure all-family mismatch
policy matrix, and the all-source/all-destination transition stale matrix to
remain present and registered in `main()`. They also require the
transport-gate source-frame matrix so transition safety stays a software
state-machine property, not a camera/OCR or multiplier-tuning exercise.

Live validation only switches DCV/ACV on the energized bench input. Passive and
current modes stay unprobed in that live path: passive voltage-payload rejection
and current voltage-payload rejection are software/property-test obligations
until a current jack, series circuit, and load-limited source are explicitly set
up for a safe current-mode run.

Physical correctness for arbitrary DMM inputs still requires deeper stock xrefs
or repeatable live traces of the analog frontend/range path. The parser still
does not invent frame-family markers for unmarked current, resistance, diode,
or extended BCD frames, because doing so would require value-shape or
mode-guessing logic rather than recovered stock metadata. In particular,
`frame[6] & 0xF0 == 0x40` is not a standalone resistance-family marker: it is
valid resistance kOhm-band evidence only inside resistance mode, while existing
current-family fixtures show the same high nibble can carry status/hold state.
Upper nibble 0 remains unresolved low-Ohm/factory calibration territory. The 2026-06-06 mux
xref audit in `meter_mode_command_table_2026_06_05.md` classifies the recovered
`FUN_080018a4`/`FUN_08001a58` runtime callers as scope/siggen paths, not DMM
runtime selector proof. The same note now guards the auxiliary AFE PB9/PA6
boundary: stock configures those pins as outputs at `0x080241D4..0x080241F0`,
but no recovered direct BOP/BCR stock write sets their level. The open firmware
therefore keeps PB9/PA6 in the stock-reset/output-low state for DMM setup
instead of asserting an invented high level. The known open items are still the
DMM-specific writers for stock `ms[0x02]` and `ms[0x03]`, any later trace that
proves mode-specific PB9/PA6 levels, the exact effect and commit semantics of
the H2 SPI3 bulk replay, and any real factory calibration source in W25Q/system
files/SPI bulk tables.

The W25Q/System-file boundary is now narrowed in
`meter_w25q_calibration_boundary_2026_06_06.md`: the bench-unit
`System file/9999.BIN` directory entry is archive attribute `0x20`, cluster 0,
size 0, so it is not a recovered meter calibration source. This is negative
evidence for the obvious `9999.BIN` lead only; it does not prove that all
factory calibration is absent.

The H2 boundary is deliberately strict: stock proves a 115,638-byte SPI3 table
from `0x08051D19` bracketed by `0x3B`/`0x3A`, and the open firmware can report
that those bytes were transmitted. That byte count is not acceptance proof. No
recovered stock path shows an FPGA ACK/apply status, and no current live trace
proves that the table fixes low-DCV or current-mode correctness.

## Validation Direction

Use unit/state-machine/property tests for the broad mode matrix first. Use the
physical device only after the software contract is stable, and then validate
hard scenarios: low DCV, range boundaries, AC evidence on DC input, stale-frame
transitions, and safe current-mode circuits with the correct jack and
load-limited series wiring.
