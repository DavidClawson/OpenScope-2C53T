# DMM Evidence / Gap Ledger

Date: 2026-06-06

This ledger is the current stock-grounded boundary for the OpenScope DMM work.
It exists to keep the next fix pointed at reverse engineering and state-machine
evidence, not at harness expansion, observed-value multipliers, or OCR.

## Current Low-DCV Blocker

The live visual check still has an unresolved physical mismatch:

```text
visual source/load display: 0.200 V
CDC frame: 5A A5 44 8E EF E7 07 24 80 00 01 89
stock-visible decode: digits=4366, frame[2].3=0, class=4, value=0.4366 V
```

The decoder is doing the stock-visible math for that frame.  Do not promote
this visual mismatch into a decoder coefficient.  The next useful evidence is
one of:

- a DMM-owned runtime writer or trace for `DAT_200000fa`/`DAT_200000fb`
  (`ms[0x02]`/`ms[0x03]`) while stock switches DMM ranges or functions
- a stock H2/SPI3 acceptance/apply condition plus multi-point DMM effect
- a real W25Q/system-file/factory-calibration source, not an invented filename
- a repeatable safe live trace showing which stock selector/mux/range state
  changes before the low-DCV frame is produced

## Per-Path Evidence Status

| Path | Stock evidence recovered | Local policy in open firmware | Missing evidence |
| --- | --- | --- | --- |
| DCV | Selector slot 0, `0x0514`; formatter case 0 in `FUN_080028E0`; BCD `+10000` extension via `frame[2].3`; decimal class priority `frame[8].7`, `frame[3].4`, `frame[4].4`, `frame[5].4`; mux writer bodies `FUN_080018a4` and `FUN_08001a58` are stock GPIO writers. | Use slot 0 for selector and mux projection; parse only stock-visible class bits; preserve the `0.200 V -> 0.4366 V` mismatch as unresolved frontend/range/calibration. | Runtime DMM-owned `ms[0x02]`/`ms[0x03]` writer, H2/apply effect, or factory calibration source that explains low DCV. |
| ACV | Selector slot 1, `0x050C`; dynamic apply pair `0x050C/0x050D`; formatter case 1; stock status/decimal helper paths around `0x080371C8`, `0x08037228`, `0x080372BC`; no stock proof that `frame[7].2` alone is AC-present confidence. | Require companion line-frequency evidence for AC validity; DC input in ACV fails closed. | Stronger AC-present source if stock has one beyond current frequency evidence. |
| DC mA / DC A | Selector slot 2, `0x0517`; dynamic apply pair `0x0517/0x050E`; DCA formatter variant evidence at `0x08002AFE`/`0x08002B54`; stock `DAT_2000102e` is formatter/variant shadow only. | Local mA and A share stock slot 2 and mux projection; uA is unresolved/unexposed; current modes reject voltage-family payloads. | Safe current-jack series live traces and a physical current range writer/calibration source. |
| AC mA / AC A | Selector slot 3, `0x050B`; ACA formatter unit index 5; local AC A display override stays separate from stock unit-index evidence. | Local AC mA and AC A share stock slot 3 and mux projection; AC current requires frequency evidence. | Safe current-jack series live traces and stock evidence for any split between mA and A. |
| Resistance | Selector slot 4, `0x050A`; formatter case 4; kOhm band is unit-normalized. | Low-Ohm normal frames fail closed with `METER_REJECT_UNRESOLVED_CALIBRATION`; no one-unit bench coefficient. | Factory coefficient source for low-Ohm band. |
| Continuity | Selector slot 6, `0x0511`; dynamic apply pair `0x0511/0x0516`; distinctive continuity segment marker. | Continuity marker is marker-visible and rejected outside continuity mode. | Exact physical beep/current behavior across probes if needed for final live proof. |
| Diode | Selector slot 7, `0x0510`; dynamic apply pair `0x0510/0x0515`; formatter/debug family pinned. | Diode has its own active-plan family but no independent normal-frame marker; wrong marker-visible families clear stale diode payloads. | Physical diode validation and any stock frame marker stronger than active-plan classification. |
| Capacitance / temperature | Selector slot 5, `0x0512`; formatter cases 5/6/7 prove display-unit families and format offsets, not separate physical frontend slots. | Local capacitance and temperature share slot 5 and mux projection; suffix/display split is local policy over stock formatter evidence. | Separate stock selector/range writer or live traces proving a physical split. |
| DC uA / AC uA | No selector-table entry, formatter path, mux writer, or safe live current trace recovered. | `FPGA_METER_INVALID_LOCAL_SUBMODE`; autoscan cannot select uA. | Real stock/live evidence for a microamp frontend/range path. |

## Cross-Cutting Evidence

- Selector table: stock table at runtime `0x080BB3FC` / app image
  `0x080B43FC` contains low bytes `14 0c 17 0b 0a 12 11 10`.
- Selector consumers: stock xrefs at `0x080042E2` and `0x080048BA` index
  `DAT_20001025`, build `0x0500 | low`, and store through the raw-word path.
- Selector/shadow xref closure: `DAT_20001025` has 9 RAM-map refs and
  `DAT_2000102e` has 7 RAM-map refs. The current closure note keeps those
  bytes scoped as digital DMM selector/formatter-shadow state: they feed
  `0x05xx` raw-word commands and display-unit variants, but they are not the
  missing `ms[0x02]`/`ms[0x03]` analog mux/range writer and not a low-DCV
  correction.
- Mode-state `ms[0xF68]` boundary: `DAT_20001060` has 7 RAM-map refs and
  gates stock mode-init, command-bank, dynamic raw-word helper, and transport
  paths. It is command-bank/transport state, not physical DMM range state, not
  exact settle/discard proof, and not a low-DCV correction.
- Saved-mode `ms[0xF64]` boundary: `DAT_2000105c` has only two RAM-map refs in
  the UI renderer, but stock config load writes word 12's low halfword to
  `ms[0xF64]` at `0x08025E50`, and boot restore reads it at `0x08026F50` before
  copying the low byte to live `ms[0xF68]`. This is saved mode-init state, not
  display-only bitmap height, not physical DMM range state, and not a low-DCV
  correction.
- Dynamic apply helper: `0x08006120`, `0x08006194`, `0x0800626A`, and
  `0x08006288` recover only ACV, DCA, continuity, and diode apply pairs.
- Command dispatcher: `FUN_0800B908` queues boot/runtime mode-init command
  banks (`0x00/0x09/(0x07|0x0A)`, `0x1A..0x1E`,
  `0x00/0x08/0x09/(0x07|0x0A)`, `0x16..0x19`,
  `0x00/0x12/0x13/0x14/0x09/(0x07|0x0A)`). This is command-byte sequencing,
  not analog mux/range writing. Production comments now keep the `0x1A..0x1E`
  and `0x08` replay bytes out of the low-DCV range-param bucket: old notes such
  as `param=0 -> 10V range`, `Below ~1V`, or `Meter: configure range` are stale
  unless a stock writer/trace ties those bytes to DMM physical range state.
- Boot mode-init TBH state map: the `FUN_0800B908` table at `0x0800B926` maps
  `ms[0xF68]` states `0..9` to command-bank targets `0x0800B93E`,
  `0x0800B9D6`, `0x0800BA6C`, `0x0800BACE`, `0x0800BB64`, `0x0800BBBE`,
  `0x0800BC2A`, `0x0800BC2E`, `0x0800BCA6`, and `0x0800BC32`. This ties stock
  command banks to the selecting state byte, but `ms[0xF68]` is not DMM
  `ms[0x02]`/`ms[0x03]` analog mux state, not raw selector words, and not
  low-DCV calibration.
- Non-meter command-bank overlap boundary: `ms[0xF68]` state 4 queues
  `0x1F..0x21` and state 5 queues `0x25..0x28`, which overlap stock
  frequency/acquisition and timebase/packed-state command-family evidence.
  State 6 queues only `0x29`; state 8 queues `0x00,0x2C`. These are
  command-bank states, not DMM physical range proof, not capacitance/temperature
  physical range proof, not a DMM calibration/range source, and not a
  continuity/diode physical frontend proof.
- Meter probe branch guard: the three `FUN_0800B908` meter arms at `0x0800B9D6`,
  `0x0800BACE`, and `0x0800BC32` read GPIOC bit 7 from `0x40011008` and select
  the `0x07/0x0A` command tail (`PC7` high keeps `0x07`; `PC7` low selects
  `0x0A`). This is probe/tail sequencing only; it is not DMM runtime range state,
  not a physical range/calibration source, not a low-DCV correction, and not
  H2/SPI3 apply proof.
- Meter transport operation guard: stock restore/runtime slices now have named
  operation-order checks for USART2 enable/disable, DVOM task resume/suspend,
  PC11 set/clear, queue reset/drain (`0x20002D7C` and `0x20002D74`), selector
  reset, stale-state clear, and the runtime tail-call to `FUN_0800B908`. This is
  reset/resume/drain evidence only; exact settle/discard timing, analog
  `ms[0x02]`/`ms[0x03]` writers, H2/SPI3 acceptance, and low-DCV calibration
  remain unresolved.
- Meter basic raw-word queue guard: stock materializes `0x0508` at
  `0x080033CA`, `0x0509` at `0x08003BA4`, and `0x0514` at `0x08005B7A` onto
  the raw-word queue path. Those words ground wake/start/variant sequencing;
  they are not DMM runtime range state, low-DCV correction words, or factory
  calibration coefficients.
- Dvom TX command pacing guard: stock `FUN_080373F4` consumes raw halfwords
  from queue `0x20002D74`, writes the USART2 TX frame, sets CTRL1 bit `0x80`,
  then executes `0x0803744C: movs r0,#0x0A` and
  `0x0803744E: BL 0x0803A390`. This is 10-tick command-channel pacing after
  each raw-word frame. It is not a recovered DMM settle/discard rule, not an
  analog range writer, and not calibration.
- Mux writers: `FUN_080018a4` and `FUN_08001a58` are 10-way GPIO hardware
  writers. Current direct runtime mux-state writers are classified as
  scope/siggen paths, not DMM range proof.
- Mux writer scope tails: the two hardware writers also read adjacent scope
  state (`DAT_20000125`, `DAT_2000010c`, `DAT_200000fc/fd + 100`) and update
  scope threshold/DAC registers (`_DAT_40007408`, `_DAT_40007404`,
  `_DAT_40001c34`). These refs are now guarded as
  scope threshold/calibration context, not missing DMM runtime range state,
  low-DCV correction, or meter calibration coefficients.
- Mux-state xref closure: the current V1.2.0 RAM-map and text-decompile
  surface has been exhausted as negative DMM evidence. `DAT_200000fa` has
  25 RAM-map refs / 26 full-decompile refs; `DAT_200000fb` has 11 RAM-map refs
  / 10 full-decompile refs. The only decompile-visible indexed writes are
  `full_decompile.c:2566` in `FUN_08001c60` and `full_decompile.c:8745` in
  `FUN_08019e98`; both are classified and guarded as scope/siggen autorange
  paths, not DMM runtime mux/range writers.
- Mux writer literal-pointer negative guard: the whole APP image has no static
  32-bit literal/function-pointer refs to `FUN_080018a4` (`0x080018A4` or
  `0x080018A5`) or `FUN_08001a58` (`0x08001A58` or `0x08001A59`). This closes
  the obvious hidden-table escape hatch for these mux writers, but it still does
  not prove that no computed or state-mediated DMM runtime path exists.
- Scope Trigger Overlay 105B Boundary: `DAT_2000105B` and the
  `0x080BB40C`/`0x080BB40E` halfword lookup are now guarded as
  `FUN_08021B40` / `scope_draw_trigger_overlay` evidence. The inspected block
  reads scope/channel drawing state and `DAT_200000FC`/`DAT_200000FD` offsets,
  not DMM selector words, not `0x20002D74`, not the mux writers, and not an
  analog range/calibration source. The current downloaded APP image has a
  zero-filled app-slot shadow at `0x080B740C`, so this high-flash-looking
  address is not a recovered DMM command/range/calibration table.
- Saved-config default boundary: stock `FUN_080223BC` seeds `0x05050000`, so
  default persistent mux bytes are `ms[0x02]=5` and `ms[0x03]=5`. That is
  persistence/default evidence only; it is not a recovered normal runtime DMM
  mux writer, not a universal frontend setting, and not a low-DCV correction.
- Saved-config calibration default boundary: stock master init restores a
  persistent/default calibration-like table at `0x20000358..0x2000044A` and
  checks `ms[0x34E]` at `0x080261A8`; if that sentinel is `0xFFFF` or `0x0000`,
  it writes hardcoded defaults at `0x080261BE..0x08026506`. This narrows a real
  stock data-source lead, but it is not a recovered DMM physical coefficient,
  not a low-DCV correction, and not a runtime `ms[0x02]`/`ms[0x03]` range writer
  without a DMM-owned consumer xref or live trace.
- Button/key debounce false `ms[0x02]` guard: the tempting `[r4,#2]` byte write
  at `0x080393A4` belongs to the key/button task, not DMM meter state. The
  guarded owner sequence at `0x08039374` loads `0x20002D50` button state,
  `0x20002D58` debounce counters, and `0x08046528` button-map table; the
  guarded `0x0803947A` third-column sequence reads/writes `[r4,#2]` as the
  third key-column debounce counter. This is negative DMM evidence, not DMM
  `ms[0x02]`, not a mux/range writer, not a low-DCV correction, and not a
  factory calibration source. Plain-language guard: [r4,#2] is the third key-column debounce counter, not DMM state.
- H2/SPI3: stock proves a byte-exact `0x3B`/table/`0x3A` transfer of
  115,638 bytes from `0x08051D19`. Open firmware `h2_upload_done` and USB
  `H2T` diagnostics mean bytes transmitted only; no recovered FPGA ACK/apply
  status or DMM calibration effect exists yet.
- PC4 post-H2 trigger-run-mode boundary: stock `0x08026E2E..0x08026E8A`
  queues USART2 commands `1,2,6,7,8`, configures GPIOC.4, then reads
  DAT_2000010f / `ms[0x17]` and sets PC4 only when that byte equals `2`.
  `STATE_STRUCTURE.md` identifies that byte as `trigger_run_mode`, a scope
  state (`0=AUTO`, `1=NORMAL`, `2=SINGLE`). This is not DMM `ms[0x02]`/`ms[0x03]`,
  not an H2 ACK/apply signal, not a low-DCV correction,
  and not factory calibration. Open firmware therefore keeps PC4 unresolved
  until a stock `.data` default, trace, or measured multi-mode effect proves
  the required level.
- W25Q/system file: bench-unit `System file/9999.BIN` is cluster 0 and size 0,
  so it is not a recovered meter calibration source.

## Next RE Target

The highest-value next target is a stock-runtime path or trace tying DMM
function/range selection to `DAT_200000fa`/`DAT_200000fb` before the low-DCV
frame is emitted. Because the current static mux-state surface is already
classified as negative DMM evidence, repeating those `DAT_200000fa/fb` refs is
not progress unless a new writer, xref owner, or trace is recovered. If that
path cannot be recovered statically, the next live experiment should capture
stock or stock-equivalent command/mux state across multiple DCV points. Those
multiple DCV points, including low DCV, 5 V, and 32 V, are hard-case inputs, not
new multiplier fit points. The physical run should come after the software
state-machine guard remains green; it should not expand webcam/OCR tooling, and
it should not probe current modes without correct jack, series wiring, and load
limiting.
