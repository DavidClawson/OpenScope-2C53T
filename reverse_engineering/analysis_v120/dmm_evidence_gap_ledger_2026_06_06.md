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

- a no-OCR/image-view live still that shows the PSU/source terminals, both
  leads, and the 2C53T `COM` + `V/Ohm/C` jacks in one frame, followed by
  multiple safe DCV points on that same visible wiring path
- a DMM-owned runtime writer or trace for `DAT_200000fa`/`DAT_200000fb`
  (`ms[0x02]`/`ms[0x03]`) while stock switches DMM ranges or functions
- a stock H2/SPI3 acceptance/apply condition plus multi-point DMM effect
- a real W25Q/system-file/factory-calibration source, not an invented filename
- a repeatable safe live trace showing which stock selector/mux/range state
  changes before the low-DCV frame is produced

### Live Negative Probes, 2026-06-06

After flashing the OpenScope app build from this branch, the webcam/image-view
evidence still showed the source at `0.200 V` while CDC/LCD reported about
`0.4315..0.4325 V`.  The following bounded stock-surface probes did not move
the DCV value toward the physical source:

```text
stock saved-default mux projection 5/5:
  PC12=0 PE4=1 PE5=0 PE6=1; PA15=0 PA10=1 PB10=0 PB11=1
  result: display about 0.4321 V

auxiliary AFE pins high:
  PB9=1 PA6=1
  result: display about 0.4322 V

post-H2 PC4 low:
  PC4=0
  result: display about 0.4325 V
```

The current-head build `Jun  6 2026 15:33:48` removed the guessed boot-time
raw replay of `0x1A..0x1E` and was flashed through the guarded OpenScope HID
path.  Webcam/image-view evidence after that flash still showed the source at
`0.200 V` while the OpenScope LCD and CDC frontend reported `0.4330 V`
(`tmp/live_evidence/post_flash_webcam.jpg` in the local run).

The later OpenScope app build `Jun  6 2026 16:27:29`
(`firmware/build/firmware.bin` SHA-256
`96a741049be7856e8da0c9c01a656b826f0d88d9dd319c2b5382a86aebcf46db`) fixed the
stock H2 preamble and close CS framing, passed guarded `hid-app` preflight, and
was flashed through HID IAP.  This was a valid OpenScope app image, not the
stock/vendor APP image.  Post-flash live evidence still failed:

```text
version: OpenScope 2C53T, Build: Jun  6 2026 16:27:29
source/load display by image view: about 0.200 V, 0.0000 A, 0.000 W
OpenScope framebuffer: 0.4330 V; SPI3 probe flat FF
CDC meter dump:
  valid=1 reading_submode=0 result_class=1 updates=134 display=0.4330 unit=V
  bcd_value=4330 decimal_pos=1
  frame=5A A5 44 8E 8F EF 0B 24 80 00 01 2F
CDC meter frontend:
  selector=0514 apply=0000 probe=0507 start=0509
  display=0.4329 unit=V bcd_value=4329 dp=1 f6=0F f7=24 f8=80 f9=00 extra=012F
  PC6_spi=1 PB11_active=1 PC11_meter_mux=1 PC7_probe=1 PC0_ready=1
  PC12_route=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
CDC mux stream:
  stable raw=4330/4331, display=0.4330/0.4331 V under slot-0 DCV selector
```

Local captures from that run:

```text
tmp/post-h2-preamble-screen.bmp
  sha256=a04bff66555909c3e56188fc3613ce89a5bfdef5ea90821c0364d8b25c6c1efb
tmp/post-h2-preamble-screen.png
  sha256=e89f691a0ab277114c590df49160e481778c9c0ab7baaa05fcd14e6e0af33773
tmp/post-h2-preamble-webcam.jpg
  sha256=254fde81ab13fbe43a334961826a624b623d6ea465618024b148322ac511b5ae
tmp/post-h2-preamble-webcam-dark.jpg
  sha256=f64d466cbc0242e4c79ac79e44b941f23209daebe7c464bb624fa58c32228b67
```

This closes the tempting claim that byte-accurate H2 TX framing alone fixes the
low-DCV bug.  It was necessary stock evidence, but the live DMM frame is still
wrong before display formatting.  Do not convert this into a multiplier or OCR
problem.  The next useful target remains a stock H2/SPI3 apply condition, a
DMM-owned analog/range writer, a real calibration source, or a multi-point
stock-equivalent trace proving what state changes before the wrong frame is
emitted.

The diagnostic-only OpenScope app build `Jun  6 2026 16:44:07`
(`firmware/build/firmware.bin` SHA-256
`7d5beeef3d8e4aed510bd36b5e9a639c40da3eb667aaa6c3c8338d254c1810b1`) was also
flashed through guarded HID IAP after `hid-app` preflight classified it as an
OpenScope app image.  It added only the `spi3 stock-readback` shell diagnostic.
Live output:

```text
=== Stock SPI3 Case-8 Readback Diagnostic ===
PC0=1 PC6=1 PB11=1 PB6=1
rx first_ff_discard=FF seed_hi=FF cmd_0a=FF mid_ff_discard=FF low=FF
ms46_equiv=0xFFFF
Interpretation: diagnostic scope readback only; not a DMM multiplier, range writer,
H2 ACK, or calibration proof.

DMM after the readback diagnostic:
  valid=1 reading_submode=0 result_class=1 display=0.4334 V
  frame=5A A5 44 8E 8F 4F 0E 24 80 00 01 2F
  frontend selector=0514 apply=0000 probe=0507 start=0509
  PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
  PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
```

Framebuffer image-view evidence from that build:

```text
tmp/post-stock-readback-screen.bmp
  sha256=0c398e70912c21a488aed1149e28f978dc4be2b4f75107e4f7e627eae602847c
tmp/post-stock-readback-screen.png
  sha256=0900e09d2054e2e7ea1673cc2fe29f7f2ab2bf28595cda791a1594df46686f56
tmp/post-stock-readback-webcam.jpg
  sha256=3c55b408f8089ac8fbd36a11d8e98722b95cb3c18d6011fb4730326e487194e1
tmp/post-stock-readback-webcam-dark.jpg
  sha256=2337a466c124624d200eccc12b521715f2154bcd2c91d5734d1e2373e239e838
```

The framebuffer still shows `0.4334 V` and `SPI3 probe flat FF`.  Webcam
image-view still shows the 2C53T around `0.4333..0.4334 V` while the PSU/source
display is about `0.210 V` with zero current/power.  This reinforces the
classification: `spi3 stock-readback`, case 8, and `ms+0x46` are useful
observability for the next stock-equivalent trace, not a production low-DCV
correction.

After the mux-pair direct-store audit, the same diagnostic build was re-read
over CDC on `/dev/ttyACM0` at 2026-06-06 17:08 local time.  It still reported
stable DCV frames before display formatting:

```text
version: OpenScope 2C53T, Build: Jun  6 2026 16:44:07
display=0.4338 V, bcd_value=4338, dp=1
frame=5A A5 44 8E 8F EF 0F 24 80 00 01 30/33
frontend selector=0514 apply=0000 probe=0507 start=0509
PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
```

This readback does not add a new workaround.  It confirms that the latest
stock-evidence/test pass did not fix the live low-DCV physical mismatch and
that the wrong value is still present in the incoming DMM frame under slot-0
DCV state.

An all-arm GPIO mux sweep, keeping the DCV selector active, tested every
recovered `FUN_080018A4` / `FUN_08001A58` stock mux projection:

```text
baseline after settle: display about 0.4328 V
arm0: display=0.4330
arm1: display=0.4329
arm2: display=0.4330
arm3: display=0.4329
arm4: display=0.4329
arm5: display=0.4329
arm6: display=0.4329
arm7: display=0.4329
arm8: display=0.4329
arm9: display=0.4328
```

The stock formatter/update tail was also probed in zero-parameter form after
DCV reinit:

```text
fpga cmd 0x001B
fpga cmd 0x001C
fpga cmd 0x001E
result after settle: display about 0.4322 V
```

These are negative live probes, not production mappings.  They rule out the
tempting one-step fixes for the current bench setup: mapping DCV to the saved
`ms[0x02]/ms[0x03] = 5/5` default, selecting any other recovered mux arm,
restoring the older PB9/PA6-high local state, treating the post-H2 PC4 gap as
the low-DCV correction, or adding zero-parameter `0x1B/0x1C/0x1E` display-tail
frames.  The remaining blocker still points at an unrecovered DMM-owned
mux/range writer, H2/SPI3 acceptance/effect evidence, physical wiring, or
factory calibration.

The later OpenScope app build `Jun  6 2026 17:17:52`
(`firmware/build/firmware.bin` SHA-256
`43542b8999ac17aaed591bd07527ddce8f8d1ec2307e1e82288ad3ad1483faf9`) moved PC6
HIGH to the stock-visible point after SPI3 CTRL1/CTRL2/SPE setup and IRQ enable,
then was flashed through guarded HID IAP.  This corrected a real stock-order
mismatch, but live DCV was still wrong before display formatting:

```text
version: OpenScope 2C53T, Build: Jun  6 2026 17:17:52
framebuffer image-view: 0.4344 V, SPI3 probe flat FF
webcam image-view: 2C53T around 0.4344 V; PSU/source display about 0.20 V,
  0.0000 A, 0.000 W
CDC meter frontend:
  display=0.4345 V, bcd_value=4345, dp=1
  frame=5A A5 44 8E 4F CE 07 24 80 00 01 32/35
  selector=0514 apply=0000 probe=0507 start=0509
  PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
  PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
```

Local captures from that run:

```text
tmp/screen_after_pc6_order_20260606.bmp
  sha256=b6bf8e5745dbdf9072bbb24814d08b6d610095cb853805b4528b001c424ac784
tmp/screen_after_pc6_order_20260606.png
  sha256=ff1a14dba190d9a1e736f8de7c2ba20a3759d31f12f98b8d4454efe148660ef4
tmp/webcam_after_pc6_order_20260606.jpg
  sha256=b1e306e2ef3a1d4dd92cb707443c9aaf55c32a16cbdcea4e53d66608ed264b06
```

Additional stock-surface probes on the same build remained negative:

```text
manual probe-tail alternative:
  sent 0x050A then 0x0509
  result: display stayed about 0.4345 V

manual PC6 polarity check:
  PC6 LOW -> spi3 read returned all 00; DCV stayed about 0.4346 V
  PC6 HIGH -> spi3 read returned all 00; DCV stayed about 0.4346 V

manual runtime meter-basic bank prefix:
  sent 0x00/0x09, 0x00/0x07, 0x00/0x1A
  result: returned frames stayed 5A A5 44 8E 4F EE/CE 07 24 80...
          display stayed about 0.4346 V
```

These probes close the tempting claims that PC6/SPI3 init ordering, the PC7
`0x0507`/`0x050A` tail, or the stock runtime `0x00` command-bank prefix by
themselves correct the current low-DCV bench failure.  The framebuffer/webcam
evidence is no-OCR/image-view evidence, but the visible wiring path in that
frame is still not a full terminal-to-terminal proof; a final physical
validation still needs a still that clearly shows the PSU/source terminals, both
leads, and the 2C53T `COM` plus `V/Ohm/C` jacks in one frame.  Do not use that
visibility gap to invent a decoder multiplier.

Fresh read-only evidence at 2026-06-06 17:49 local time, after the W25Q
calibration-boundary guard was added, still shows the same failure:

```text
fresh webcam image-view:
  tmp/webcam_fresh_20260606.jpg
  2C53T display about 0.4349 V; source display 0.200 V, 0.000 A, 0.000 W
  lower jacks/source terminal-to-terminal wiring path remains partly occluded
CDC meter dump:
  valid=1 reading_submode=0 result_class=1 updates=10804 display=0.4349 unit=V
  bcd_value=4349 decimal_pos=1
  frame=5A A5 44 8E 4F CE 0F 24 80 00 01 38
CDC meter frontend:
  selector=0514 apply=0000 probe=0507 start=0509
  PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
  PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
```

This keeps the W25Q/doc/test work classified correctly: it prevents invented
factory-calibration coefficients, but it does not fix the live physical DCV
mismatch.

Open firmware transition correction at 2026-06-06 18:xx local time: stock
runtime mode switching disables USART2 itself during DMM drain
(`0x0800741A`: `CTRL1 &= ~0x2000`) and re-enables it in the enable/resume tail
(`0x08007360`: `CTRL1 |= 0x2000`).  The local `fpga_meter_reset_transport()`
previously masked only RX/TX interrupts while leaving UEN set during queue/task
reset.  The local transition path now clears UEN during the drain and restores
UEN before resuming DVOM tasks.

The flashed live check on the resulting OpenScope app build did not fix low
DCV:

```text
version: OpenScope 2C53T, Build: Jun  6 2026 18:05:23
image: firmware/build/firmware.bin
sha256: eae2936a7e5dd7aedf2bd6f0cbc699254f7dd3f6be97909bc329cea82285f0d6
preflight: hid-app kind=openscope-app, address=0x08004000
webcam/image-view: source display about 0.200 V; 2C53T LCD about 0.4314 V
CDC DCV after flash:
  display=0.4314..0.4315 V, bcd_value=4314/4315, dp=1
  frame=5A A5 44 8E 0F 4A 0E 24 80 00 01 27
  frontend selector=0514 apply=0000 probe=0507 start=0509
  PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
  PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
ACV on the same DC input:
  display=---, valid=0, reject=3
DCV after returning from ACV:
  display=0.4313 V, bcd_value=4313, frame raw digits 04,03,01,03
```

This closes the UEN transition mismatch as insufficient by itself.  It remains
good stock-state-machine fidelity, but it is not an analog mux writer, not H2
apply proof, not a calibration coefficient, and not a physical low-DCV
correction.

The same live run showed the experimental DMM voltage waveform sampler had
accumulated more than `100000` SPI3 meter-ADC probe samples while the normal DCV
reading was being judged. That sampler is not the stock USART2 DMM value path
and its live source remains unproven (`0xFF`/flat evidence in the waveform
notes). The open firmware now leaves the sampler disabled by default and exposes
it only through explicit `meter wave sampler on/off` diagnostics.

The flashed live check on the sampler-off build did not fix low DCV either:

```text
version: OpenScope 2C53T, Build: Jun  6 2026 18:17:24
image: firmware/build/firmware.bin
sha256: bbd662e5608ea75fbe574626e641e37e6b260e7643c7ccf524bf0dbdc6c4f499
preflight: hid-app kind=openscope-app, address=0x08004000
meter wave sampler: off
meter wave: samples_total=0, delta_250ms=0, approx_rate=0 Hz
CDC DCV with sampler off:
  display=0.4307..0.4308 V, bcd_value=4307/4308, dp=1
  frame=5A A5 44 8E EF EB 0F 24 80 00 01 21
  wave_samples=0
  frontend selector=0514 apply=0000 probe=0507 start=0509
  PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
  PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
ACV on the same DC input:
  display=---, valid=0, reject=3, wave_samples=0
DCV after returning from ACV:
  display=0.4307 V, bcd_value=4307, wave_samples=0
```

This removes a non-stock 1 kHz SPI3 loop from ordinary DMM voltage operation and
closes sampler interference as insufficient by itself. It is still a correctness
cleanup: future low-DCV work should continue at the DMM-owned runtime
frontend/range writer, H2/apply effect, factory calibration source, or visible
wiring proof boundary. It is not permission to restore decoder coefficients.

## Per-Path Evidence Status

| Path | Stock evidence recovered | Local policy in open firmware | Missing evidence |
| --- | --- | --- | --- |
| DCV | Selector slot 0, `0x0514`; formatter case 0 in `FUN_080028E0`; BCD `+10000` extension via `frame[2].3`; decimal class priority `frame[8].7`, `frame[3].4`, `frame[4].4`, `frame[5].4`; mux writer bodies `FUN_080018a4` and `FUN_08001a58` are stock GPIO writers. | Use slot 0 for selector and mux projection; parse only stock-visible class bits; preserve the `0.200 V -> 0.4366 V` mismatch as unresolved frontend/range/calibration. | Runtime DMM-owned `ms[0x02]`/`ms[0x03]` writer, H2/apply effect, or factory calibration source that explains low DCV. |
| ACV | Selector slot 1, `0x050C`; dynamic apply pair `0x050C/0x050D`; formatter case 1; stock status/decimal helper paths around `0x080371C8`, `0x08037228`, `0x080372BC`; no stock proof that `frame[7].2` alone is AC-present confidence. | Require companion line-frequency evidence for AC validity; DC input in ACV fails closed. | Stronger AC-present source if stock has one beyond current frequency evidence. |
| DC mA / DC A | Selector slot 2, `0x0517`; dynamic apply pair `0x0517/0x050E`; DCA formatter variant evidence at `0x08002AFE`/`0x08002B54`; stock `DAT_2000102e` is formatter/variant shadow only. | Local mA and A share stock slot 2 and mux projection; uA is unresolved/unexposed; current modes reject voltage-family payloads; AUTO/autoscan must not live-sweep current modes without explicit safe current-jack/series wiring proof. | Safe current-jack series live traces and a physical current range writer/calibration source. |
| AC mA / AC A | Selector slot 3, `0x050B`; ACA formatter unit index 5; local AC A display override stays separate from stock unit-index evidence. | Local AC mA and AC A share stock slot 3 and mux projection; AC current requires frequency evidence; AUTO/autoscan must not live-sweep AC current modes without explicit safe current-jack/series wiring proof. | Safe current-jack series live traces and stock evidence for any split between mA and A. |
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
- Command-dispatch queue boundary: stock `0x08036A50` consumes one-byte commands
  from `0x20002D6C`, but that queue is not the same channel as the raw USART/DVOM
  halfword queue `0x20002D74`. The downloaded APP image has a non-callable
  literal/shadow surface at `0x0804BE74`
  (`00000000 00400000 00000000 00000400 ...`), while the callable-looking
  normalized table prefix is at `0x08044E74`
  (`0800fd39 0800fe9d 080104ed ...`). This keeps `FUN_0800B908`
  command-bank bytes such as `0x1A..0x1E` scoped as byte-dispatch evidence until
  a recovered materializer bridges them to `0x20002D74`; they are not raw DMM
  range/calibration words by themselves.
- Normalized `0x1A..0x1E` handler negative: the table entries
  `0x1A->0x08010A1C`, `0x1B->0x08010A94`, `0x1C->0x08010B80`,
  `0x1D->0x08010C78`, and `0x1E->0x08010E70` are real, but guarded body snippets
  materialize RAM `0x20008350` and store halfwords at display-buffer offsets
  `0x13A`, `0x13C`, `0x140`, and `0x142`. These handlers do not touch
  `0x20002D74`, do not format USART2 TX frames, and do not write
  `ms[0x02]`/`ms[0x03]`; they are not the missing DMM wire/range bridge.
- Mux writers: `FUN_080018a4` and `FUN_08001a58` are 10-way GPIO hardware
  writers. Current direct runtime mux-state writers are classified as
  scope/siggen paths, not DMM range proof.
- Live low-DCV mux-bit negative check: on the current diagnostic build, manual
  PB10 high (stock arm 1's Port A/B difference from DCV arm 0) did not change
  the raw DCV frame/display (`0.4335 V` stayed `0.4335 V`) and PB10 was
  restored low. This rules out the simple PB10-only gain-bit hypothesis for the
  current `0.200 V` source versus `0.433x V` meter mismatch, but it is not a
  recovered DMM runtime mux writer.
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
- Mux-state absolute direct-store guard: raw stock slices at
  `0x08005B56..0x08005DA4`, `0x0800695A..0x08006BC6`,
  `0x08006F5C..0x080070E8`, and `0x08025D94..0x08025DC2` classify direct
  `ms[0x02]`/`ms[0x03]` pair stores as scope/active-mode mux-state pair seeds
  or saved-config restore state. Direct stores are therefore classified, not
  absent; none is yet a DMM-owned runtime range writer, low-DCV correction, or
  recovered DMM selector/raw-word owner.
- Legacy `fpga_task_annotated.c` formatter-state cleanup: `ms[0xF37]` is now
  documented as `DAT_2000102f` formatter/decimal state, not `meter_cal_coeff`.
  The old Type-4 `Meter range config` comment is now classified as a
  Type-4-shaped FPGA config arm with no normal runtime DMM caller tying it to
  physical range selection. This prevents stale legacy annotations from
  becoming a low-DCV/current/range coefficient.
  Machine-readable guard: no normal runtime DMM caller tying it to physical range selection.
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
  status or DMM calibration effect exists yet. A focused 2026-06-06 audit keeps
  the H2 proof TX-side only: stock asserts PB6 CS at `0x08026AE6`, writes
  `0x3B` at `0x08026B06..0x08026B08`, drains SPI3 RX/MISO at sites such as
  `0x08026B26`, `0x08026B76`, `0x08026BB6`, and `0x08026BF6` without a recovered
  compare/store/branch on those bytes, sends the close opcode `0x3A` at
  `0x08026C96..0x08026C98`, and deasserts PB6 CS at `0x08026CF6`. The
  post-H2 queue bytes `1,2,6,7,8` go to SPI3 queue `0x20002D78`, not the raw
  USART/DMM word queue. The next RE target is stock SPI3 acquisition case 7
  (`trigger_byte - 1 == 7`, case start `0x0803775C`) and readers of the
  status-like word around `ms+0x46`; do not treat `0x3A`, MISO drain, PC4, or
  byte count as DMM low-DCV correction proof before that path is recovered.
- SPI3/PC6 enable-order correction: the stock init choreography enables SPI3
  before raising PC6. `SPI3_INIT_SEQUENCE_DECODED.md` records SPI3 CTRL2/CTRL1
  enable before GPIOC config + `PC6 HIGH`, then the 100 ms pre-handshake delay.
  The open firmware previously configured and raised PC6 immediately after PB6
  CS setup, before SPI3 CTRL1/CTRL2/SPE were programmed. Current code now moves
  `GPIOC->scr = PC6_MASK` after `FPGA_SPI->ctrl1 |= (1 << 6) /* SPE */;` and
  `NVIC_EnableIRQ(SPI3_I2S3EXT_IRQn)`, before the same 100 ms delay. This is a
  stock-order fix and a plausible H2/apply candidate, not proof by itself that
  low DCV is corrected; live validation after flashing must decide its effect.
- SPI3 acquisition case-7 first-pass boundary: the decompile-visible case 7 in
  `FUN_08037800` / `spi3_acquisition_task` writes `ms[0x18]` to SPI3 and returns
  to the `0x20002D78` acquisition-trigger receive loop. The adjacent readback
  helper assembles `ms+0x46` from SPI3 bytes by seeding from `SPI3_DATA`, sending
  `0x0A`, then two `0xFF` bytes, and storing `(high << 8) | low`; this is
  documented in `FPGA_TASK_ANALYSIS.md` as calibration readback. Current evidence
  still does not tie `ms+0x46`, case 7, or `ms[0x18]` to DMM low-DCV/range
  correction. Treat it as a scope/SPI acquisition status/readback lead until a
  DMM-owned consumer xref or live stock trace proves otherwise.
- SPI3 case-8 readback diagnostic boundary: a 2026-06-06 read-only sidecar
  classified `FUN_08037800` as a Ghidra label inside the SPI3 acquisition task,
  not a standalone H2/DMM apply function.  The stock switch table at
  `fpga_task_decompile.txt:2126` maps trigger byte 9 (`trigger_byte - 1 == 8`) to
  `0x0803779C..0x080378F4`, which stores `ms+0x46`; raw stores are visible at
  `0x08037816`, `0x080378A4`, and `0x080378F4`.  The only RAM-map consumer is
  `FUN_08021DE4`, and `core_subsystems_annotated.c` names the word
  `trigger_position_sample`.  Open firmware therefore exposes
  `spi3 stock-readback` as a diagnostic-only shell command.  Its bytes are not a
  DMM multiplier, not a low-DCV correction, not H2 ACK/apply proof, and not a
  recovered factory-calibration source.
- PC4 post-H2 trigger-run-mode boundary: stock `0x08026E2E..0x08026E8A`
  follows the SPI3 queue block that sends trigger bytes `1,2,6,7,8` to
  `0x20002D78`; it then configures GPIOC.4 and reads
  DAT_2000010f / `ms[0x17]` and sets PC4 only when that byte equals `2`.
  `STATE_STRUCTURE.md` identifies that byte as `trigger_run_mode`, a scope
  state (`0=AUTO`, `1=NORMAL`, `2=SINGLE`). This is not DMM `ms[0x02]`/`ms[0x03]`,
  not an H2 ACK/apply signal, not a low-DCV correction,
  and not factory calibration. Open firmware therefore keeps PC4 unresolved
  until a stock `.data` default, trace, or measured multi-mode effect proves
  the required level.
- Auxiliary AFE PB9/PA6 live negative check: setting PB9 and PA6 high together
  during the same DCV bench state left the raw frame/display essentially
  unchanged (`0.4335..0.4336 V`) and both pins were restored low. This keeps
  the stock-xref boundary intact: stock proves output configuration only, not a
  mode-specific assertion, and the old "both high" bench posture is not an
  immediate low-DCV correction.
- W25Q/system file: bench-unit `System file/9999.BIN` is cluster 0 and size 0,
  so it is not a recovered meter calibration source.  A 2026-06-06 full
  read-only W25Q128 dump (`16 MiB`, SHA-256
  `5706dcd936bdb5d60bfdb5c972fb1db7b1554c6004b57ce36c7544ac8a377d14`)
  confirms the same empty `9999.BIN` entry, a populated JPG-asset Volume 0, an
  empty reachable-root manifest for Volume 1, and only unxrefed FAT/BMP-like
  orphan residue after `0x200000`.  This is still negative boundary evidence,
  not proof that all possible factory calibration is absent.
- Post-H2 SPI3 queue live negative check: open firmware build
  `Jun 6 2026 18:32:36` (`firmware/build/firmware.bin` SHA-256
  `d840b086a9b209548b2b95d5fe7d86fa43e402b13b2ec2b9b8541949d8cffa5e`)
  moved stock post-H2 bytes `1,2,6,7,8` from the wrong USART replay path to
  `spi3_acq_queue` and kept the diagnostic DMM waveform sampler off stock
  trigger byte `6`.  Live `status` showed `post-H2 SPI3 boot: enq=5 ok=5
  drop=0 mask=0x1F`, so the local queued path ran.  Live DCV still emitted
  wrong raw frames around `raw=4321..4323`, `class=1`, `display=0.4321..0.4323
  V`, with frames such as `5A A5 44 8E AF 0D 0A 24 80 00 01 33`.  ACV on the
  same DC setup still rejected as `display=---`, `reject=3`, preserving the
  AC-evidence gate.  Webcam capture was used only for visual bench context; the
  source-display digits were not clean enough to use as exact evidence.  This
  closes "wrong queue target alone" as insufficient; the remaining live wrong
  value is still upstream of display formatting and requires deeper stock
  command/materialization or AFE/H2-effect recovery.
- Boot frontend-before-activation live negative check: open firmware commit
  `9e1bd360eeadcc5fa7b092c8f12bbf886b81e608` (`firmware/build/firmware.bin`
  SHA-256 `b322e60ab8180bcc7cee3733967be6d851bbb1813a740be3345d5b85aa95a8d3`)
  was flashed through guarded HID IAP after `flash_preflight.py hid-app`
  accepted the image as an OpenScope app. This build keeps PB11 unconfigured
  during SPI3/H2 replay, then applies `fpga_set_meter_frontend_for_submode(0)`
  before the boot meter activation block. Live image-view evidence from
  `tmp/live_boot_frontend_webcam_video0_dcv_after_wait.jpg` shows the source at
  `0.200 V`, while the 2C53T LCD and CDC both show `0.4332 V`. CDC dump frames
  include `5A A5 44 8E 8F AF 0D 24 80 00 01 36`, `display=0.4332 V`,
  `class=1`, and the frontend dump confirms the planned slot-0 DCV projection:
  `PC12=1 PE4=1 PE5=0 PE6=1; PA15=1 PA10=1 PB10=0 PB9=0 PA6=0; PB11=1;
  PC11=1`. ACV on the same DC input still failed closed as `display=---`,
  `reject=3`. This closes "boot DCV projection before activation" as
  insufficient by itself; the remaining wrong value is still a DMM frame
  production / stock command-materialization / H2-acceptance-effect /
  factory-calibration gap, not a decimal decoder or one-point multiplier target.
- Runtime producer trace hook: open firmware build
  `Jun 6 2026 19:55` (`firmware/build/firmware.bin` SHA-256
  `02ba377f779277a925ede77116ff97fc92fe57770ebd909c8ddb615432425be8`)
  was accepted by `flash_preflight.py hid-app` as `kind: openscope-app`,
  flashed through guarded HID IAP, and booted back to CDC.  The new read-only
  `meter trace` command records the transport/selector counters at the exact
  point the USART2 ISR copies a `0x5A 0xA5` data frame into firmware-owned
  `fpga.rx_frame`, then prints the parsed `meter_data_snapshot()` beside that
  producer frame.  A live post-flash trace reported:
  `producer_last_rx data=33 tx=29 echo=0 seq=1 seq_sub=0 busy=0 discard=0`,
  `wire selector=0514 apply=0000 probe=0507 start=0509`,
  `gpio_frontend PC12=1 PE4=1 PE5=0 PE6=1 PA15=1 PA10=1 PB10=0 PB9=0 PA6=0`,
  `h2 bytes=115638 done=1 post_enq=5 post_ok=5 post_drop=0 post_mask=1F`,
  and matching producer/parser bytes
  `5A A5 A4 BD 8D CF 47 20 00 00 01 38`, decoded as `raw=2235`,
  `display=2.235 V`.  This validates the trace surface and proves, for that
  live frame, that parser/UI formatting did not alter the firmware-owned data
  frame.  It is not a low-DCV fix and it is not evidence that the visible
  `0.200 V` blocker is resolved; the next low-DCV run should capture this same
  trace while the image-viewed source/load display is at the intended hard-case
  voltage.
- Runtime producer history extension: the follow-on trace hook now keeps a
  small ISR-owned ring of complete `0x5A 0xA5` data frames with the producer
  counters visible at each arrival: `tx_count`, `echo_count`, DMM sequence
  count/submode, transition-busy flag, and discard budget.  `meter trace`
  prints those `rxh` records newest-first beside the parsed frame.  This is a
  root-cause tool for the next low-DCV run: it can show whether the wrong
  `0.436x V` bytes are present in consecutive producer frames, whether they
  only appear during discarded transition frames, or whether they are tied to a
  specific selector/apply sequence.  It is still diagnostic-only and must not be
  used as a feedback path for range decisions or observed-case correction.
- Runtime transition/apply history extension: `meter trace` now also records a
  small DMM transition ring (`mth` rows) that pairs the stock-like selector,
  optional apply word, probe/start words, and producer counters with the planned
  mux GPIO projection and the actual GPIO levels sampled immediately after
  `fpga_set_meter_frontend_for_submode()`.  The GPIO mask bits are
  `PC12, PE4, PE5, PE6, PA15, PA10, PB10, PB11, PB9, PA6`.  This is the
  runtime bridge needed for the next low-DCV capture: it can prove whether the
  `0.436x V` producer frames followed the intended DCV writer state or whether
  the wrong frame begins at/after a specific selector/apply transition.  It is
  not an inferred factory coefficient, not an image-derived proof path, and not
  a range feedback mechanism.
- Transition/apply trace live smoke: open firmware commit `6b1f8a4` was built
  into `firmware/build/firmware.bin` SHA-256
  `4162b553813143c87e239643e63d2211cf54210db491552fa579ae957042b90a`, accepted
  by `flash_preflight.py hid-app`, flashed through guarded HID IAP, and booted
  back to CDC.  Live `meter trace` printed the new `mth` row:
  `sub=0 seq=1 selector=0514 apply=0000 probe=0507 start=0509 tx=4..7
  data=0..0 planned_gpio=0BB actual_gpio=0BB`, followed by stable producer
  history frames such as `5A A5 A4 BD AD 8D 4A 20 00 00 01 31`, decoded as
  `display=2.227 V`.  The no-OCR webcam/image-view bench still showed the
  supply display in low-voltage CV state, about `0.100 V`, while the 2C53T LCD
  showed about `2.227 V`; the visible wiring path was partially obscured and is
  not by itself a wiring proof.  This moves the blocker forward: the wrong
  producer bytes follow the local intended DCV mux projection and `0x0514`
  selector sequence, so the next target is stock command materialization,
  missing apply/acceptance, H2 effect, or true factory calibration rather than
  parser/display math.
- Producer-frame trace race cleanup: commit `b84dcf0` changed `meter trace` to
  snapshot `fpga.rx_frame` under the same critical section used for RX history
  and parsed-frame debug state.  Guarded HID flash of image SHA-256
  `e6d39abdc6dc05e49e2ea8c22fbd59e0e91d4d0713edf1780cc36414a83914f1`
  confirmed the diagnostic boundary.  After `fpga diag clear`, `mode scope`,
  then `mode meter 0`, the immediate trace had matching
  `producer_frame=parsed_frame=5A A5 E4 2E 63 25 07 00 00 00 01 33` (`OL`) and
  `mth n=0 sub=0 seq=2 selector=0514 apply=0000 probe=0507 start=0509
  tx=33..36 data=1..1 planned_gpio=0BB actual_gpio=0BB`.  A later trace at the
  same low-voltage bench setup had matching stable wrong frames,
  `producer_frame=parsed_frame=5A A5 A4 BD AD CD 47 20 00 00 01 2D`, decoded as
  `display=2.225 V`.  This rules out a debug-print race as the source of the
  wrong low-DCV value and keeps the blocker upstream of parser/display math:
  the DMM producer itself emits OL during transition and then emits the wrong
  DCV payload under the local `0x0514`/`0x0BB` state.
- Post-H2 SPI3 RX trace: commit `0c06520` records raw MISO bytes for the five
  stock post-H2 SPI3 triggers (`1/2/6/7/8`) instead of treating `post_ok` as an
  acceptance signal.  Guarded HID flash of image SHA-256
  `b3b639a4bb21974d158d3f0e6e7b3d0e7ead988a9838f053fc9e3c0bb43fe0af` showed
  `h2 bytes=115638 done=1 post_enq=5 post_ok=5 post_drop=0 post_mask=1F`, but
  every recorded post-H2 RX byte was `FF`:
  trigger `01` -> `FF FF`, trigger `02` -> `FF FF FF FF FF FF`, trigger `06`
  -> `FF FF`, trigger `07` -> `FF FF`, trigger `08` -> `FF FF FF FF FF`.
  The same trace still decoded stable wrong low-DCV output as `display=2.228 V`
  from `producer_frame=5A A5 A4 BD AD ED 4F 20 00 00 01 34` under
  `selector=0514 apply=0000 probe=0507 start=0509 planned_gpio=actual_gpio=0BB`.
  This makes post-H2 byte count/`post_ok` a TX/replay diagnostic only, not H2
  apply proof.
- Controlled `0x0508` live negative: the stock basic configure word is a real
  materializer, but adding it to the live DCV raw-word sequence did not repair
  the low-voltage producer value.  With the same bench setup, `fpga wire words
  0x0508 0x0514 0x0507 0x0509` produced no change after settle:
  `display=2.228 V`, `producer_frame=5A A5 A4 BD AD ED 4F 20 00 00 01 2C`, and
  the same DCV frontend GPIO levels.  Therefore `0x0508` is not sufficient as
  the missing low-DCV fix; any future use must be justified by broader stock
  transition evidence, not by this point.
- Auxiliary AFE pin live negative: PB9 and PA6 are still unresolved stock
  outputs, so the current open firmware keeps them low.  A controlled DCV-only
  low-voltage run manually tested the four PB9/PA6 combinations after
  `mode meter 0` and a stock DCV raw-word tail (`0x0514 0x0507 0x0509`), without
  entering current or passive modes.  All four combinations stayed in the same
  wrong producer range: `PB9=0 PA6=0 -> display=2.232 V`,
  `PB9=0 PA6=1 -> display=2.233 V`, `PB9=1 PA6=0 -> display=2.233 V`,
  `PB9=1 PA6=1 -> display=2.233 V`, with unchanged DCV selector and frontend
  core levels.  Therefore PB9/PA6 alone are not the missing low-DCV frontend
  state.
- SPI3 meter-ADC sidecar live negative: the experimental stock case-5 sampler
  does not currently expose a usable raw DMM measurement source.  On the same
  DCV low-voltage bench state, direct mode produced `3200/3200` samples at
  `FF`, and pre-acq mode produced `3160/3160` samples at `FF` with
  `last_preacq_rx=FF`; the USART DMM producer simultaneously remained valid at
  `dmm_display=2.233 V`.  Treat this as "case-5 not armed/valid in local
  firmware" evidence, not as a hidden raw voltage reading.
- Machine-readable producer trace hook: commit `1abb222` adds host-side
  `openscope_live_debug.py meter-trace --json`, which parses the existing
  read-only firmware `meter trace` output into a structured record containing
  the selected DMM plan, wire words, producer frame, parsed frame, frontend GPIO
  levels, H2/post-H2 SPI3 state, and decoded stock-visible value.  This is a
  trace artifact, not a correction path.  A live run on `/dev/ttyACM0` with
  build `Jun  6 2026 20:25:17` first showed `PB9=1 PA6=1` from old manual
  runtime state; after `mode meter 0` the production transition reset them to
  `PB9=0 PA6=0` with `planned_gpio=actual_gpio=0BB`.  After the discard window
  drained, the structured trace still showed the wrong value before display
  formatting:

```json
{
  "wire": {"selector": "0514", "apply": "0000", "probe": "0507", "start": "0509"},
  "gpio_frontend": {"PC12": 1, "PE4": 1, "PE5": 0, "PE6": 1, "PA15": 1, "PA10": 1, "PB10": 0, "PB9": 0, "PA6": 0},
  "producer_frame": "5A A5 A4 BD 8D EF 4F 20 00 00 01 5E",
  "parsed_frame": "5A A5 A4 BD 8D EF 4F 20 00 00 01 5E",
  "decoded": {"display": "2.238", "unit": "V", "raw": 2238, "family": "0/0"},
  "calibration_state": {"bytes": 115638, "done": 1, "post_mask": "1F", "post_rx": "FF-only"}
}
```

  This keeps the current blocker upstream of parser/display math and closes the
  stale-manual-pin explanation for the post-reinit production path.  It also
  makes future live validation comparable without OCR or ad-hoc text scraping:
  the raw measurement source is the USART2 12-byte DMM producer frame, and the
  next useful trace must move earlier than that frame.
- USART TX-frame audit + next trace hook: stock `dvom_TX` at
  `0x0803743A..0x08037442` writes only `tx_buffer[3]=cmd_lo`,
  `tx_buffer[2]=cmd_hi`, and `tx_buffer[9]=cmd_hi+cmd_lo`; the rest of
  `0x20000005..0x2000000E` remains the zero-filled BSS/padding surface already
  documented in `remaining_unknowns.md`.  Current OpenScope TX frames such as
  `00 00 05 09 00 00 00 00 00 0E` therefore match stock byte construction, so
  changing TX headers/trailers is not justified evidence.  This slice instead
  adds diagnostic-only early USART2 RX-sync counters and the last 10-byte echo
  snapshot to `meter trace`: `rx_sync data_start=...
  echo_start=... data_hdr=... echo_hdr=... bad_second=... stray=...` and
  `last_echo_frame=...`.  These fields do not feed decoder/range decisions;
  they exist to prove, on the next guarded live firmware, whether the
  `echo_count=0` observation is "no `AA 55` echo stream seen" or a parser/sync
  rejection before the 12-byte DMM producer frame.
- RX-sync / SPI3 all-FF live boundary: guarded HID flash of OpenScope app image
  SHA-256 `fe5b1d3a87891c94aa1967c94ddd58e62f6c6a0b35031f3d0a546186ec84e89d`
  booted as build `Jun 6 2026 20:48:14`.  A live post-flash `meter trace` /
  `status` run showed USART2 data-frame starts and headers increasing while
  echo starts stayed zero: `rx_sync data_start=94 echo_start=0 data_hdr=93
  echo_hdr=0 bad2=1 stray=12` early, then `data_start=318 echo_start=0
  data_hdr=317 echo_hdr=0` after SPI3 diagnostics.  This proves the current
  `echo_count=0` observation is not an `AA 55` parser-rejection problem in the
  firmware RX state machine; no `0xAA` echo-frame starts are appearing while
  `0x5A 0xA5` data frames are flowing.  The same guarded run showed all local
  SPI3/H2 diagnostic MISO samples stuck at `FF`: raw CS-low reads, cmd `0x80`
  reads, USART arm `0x20/0x21` reads, stock case-8 readback, H2 TX replay
  preamble/body/close sampling, and all five post-H2 trigger readbacks.  This is
  a runtime producer/apply boundary, not a decoder-math issue and not H2 ACK
  proof.
- Reversible SPI3_GMUX live negative: with the same build still running, a
  controlled CDC session saved the live state, wrote `IOMUX->remap5` at
  `0x40010028` from `0x02000000` to `0x00000000`, ran `spi3 acqtest` and
  `spi3 stock-readback`, then restored `0x02000000`.  Clearing remap5 did not
  produce any non-`FF` SPI3 MISO bytes (`T1..T4` all `0/16` or `0/32` non-FF,
  `ms46_equiv=0xFFFF`).  Therefore the local `SPI3_GMUX_0010` write is not the
  current low-DCV/H2-acceptance fix, even though it remains an empirical AT32
  pin-routing choice.  Future work should move earlier than the USART2 DMM
  producer frame through stock command/materialization, H2 apply effect, or
  factory-calibration recovery rather than repeating this GMUX toggle.
- Normalized command-table width + TX-history trace hook: a Capstone-assisted
  binary pass over normalized table `0x08044E74` confirms it contains 45 Thumb
  handler entries (`0x00..0x2C`), not just the first eight entries previously
  guarded by the stock-literal test.  The DMM-adjacent handlers in that table
  reference the RAM state base `0x20008350`; the table handlers themselves do
  not call `xQueueGenericSend` into raw halfword queue `0x20002D74`.  Separate
  raw materializers still exist, including the stock DCV-like path around
  `0x08005B7A`, which writes `0x0514` to `0x20002D54`, sends it through
  `0x20002D74`, then queues byte-dispatch entries `0x1D` and `0x1B` through
  `0x20002D6C`.  That is command/state sequencing evidence, not permission to
  transmit `0x1D`/`0x1B` as raw USART words in OpenScope.  The next firmware
  trace increment therefore exposes the already-recorded TX frame ring in
  `meter trace` as `txh` rows, so the low-DCV run can compare the exact
  10-byte USART frames preceding the wrong `0x5A 0xA5` producer frame against
  stock `dvom_TX` construction before touching decoder math or inventing a
  calibration factor.
- TX-history live smoke: OpenScope app image SHA-256
  `ccc62439460f9b5434f985ce02e20d8c15ca9f65ae9bb6eca084b96a6203e8cf`
  (`Build: Jun 6 2026 21:09:30`) passed guarded `flash_preflight.py hid-app`,
  was flashed through HID IAP, and booted back to CDC.  A live `meter trace
  --json` run showed the new `tx_history` ring immediately before a stable
  DCV producer frame.  The producer history stayed identical across eight
  frames (`5A A5 A4 BD 4D 4E 4E 20 00 00 01 46`, decoded `display=2.244 V`)
  while the TX history showed repeated stock-shaped start frames
  `00 00 05 09 00 00 00 00 00 0E`, preceded by probe
  `00 00 05 07 00 00 00 00 00 0C` and selector
  `00 00 05 14 00 00 00 00 00 19`.  `last_echo_frame` remained all zero and
  `rx_sync` still showed data-frame starts/headers increasing with no
  `0xAA 0x55` echo starts.  This live run was not image-viewed as the hard
  `0.200 V` setup, so it is only a TX/producers trace-surface validation; it
  does not prove the low-DCV blocker fixed.
- TX-count correlated TX-history smoke: OpenScope app image SHA-256
  `e108446cb8ff930bd961abfa6b49a693de2a2feabf5c802dfb94d3991dc34884`
  (`Build: Jun 6 2026 21:13:01`) was flashed through guarded HID IAP and
  booted back to CDC.  A live `meter trace --json` run showed the `txh` rows
  now include the producer `tx` counter snapshot: newest `txh n=0 tx=308
  frame=00 00 05 09 00 00 00 00 00 0E`, with earlier rows stepping back
  one-for-one through `tx=307`, `tx=306`, and so on.  The paired producer
  history had matching `tx` snapshots on DMM frames (`rxh n=0 data=441 tx=308
  ... frame=5A A5 A4 BD 4D 8E 4F 20 00 00 01 43`, decoded `display=2.243 V`).
  This narrows the next live low-DCV run to a concrete command-to-producer
  boundary: the wrong producer frame can now be compared to the immediately
  preceding USART2 command frame by counter, instead of only by wall-clock
  ordering.  It still is not a low-DCV fix, not stock ACK proof, and not a
  calibration coefficient.
- Byte-dispatch bridge live negative + non-poll TX control ring: a sequential
  run on the same bench state first reset DCV with `mode meter 0`, captured a
  baseline producer frame around `2.236 V`, then sent `fpga stock bridge dynamic
  ch1`, waited for settle, and captured another trace.  The candidate emitted
  seven TX frames including dynamic raw words `0x0510` and `0x0511` plus
  display/update byte `0x1B`, but the settled producer value stayed at the same
  raw/class (`raw=2236`, class `1`).  This is a live negative for promoting that
  existing bench bridge into the production DCV fix.  Because the 4 Hz poll task
  quickly overwrites the ordinary TX history with `0x0509`, the next diagnostic
  build adds a separate non-poll `tx_control_history` ring to `meter trace`.
  OpenScope app image SHA-256
  `594da221a95bc02c52f51e928d54cc358d8aa2368a3dea5bfae403afb0883b0f`
  (`Build: Jun 6 2026 21:19:53`) was flashed through guarded HID IAP and booted
  back to CDC.  A live settled trace showed `tx_history` newest entries were all
  poll frames (`0x0509`), while `tx_control_history` still retained the setup
  frames by TX counter: `tx=6 0x0507`, `tx=5 0x0514`, `tx=4 0x0514`,
  `tx=3 0x0507`, and `tx=1 0x0508`.  This improves the low-DCV trace boundary
  without changing production meter commands or claiming a calibration/source
  fix.
- Explicit mux-arm live negative: OpenScope app image SHA-256
  `2583641ecb0d2b884bc8e5b0151e95468b39a6c6640ab8f559712647666c10b6`
  (`Build: Jun 6 2026 21:29:26`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`.  The new debug-only `meter mux-arms <ce> <ab> [ms]` command
  applies explicit stock `ms[0x02]`/`ms[0x03]` GPIO projections, sends one
  `0x0509` poll, then prints the normal machine-readable `meter trace`.  On the
  current bench state, `0/0`, `5/5`, `5/0`, and `0/5` all applied their planned
  GPIO masks exactly (`0BB`, `0AA`, `0BA`, `0AB` respectively), but the settled
  producer frame stayed materially unchanged at about `2.230..2.231 V`
  (`raw=2230..2231`, class `1`).  Trace artifacts were stored under
  `tmp/live_mux_arms_20260606/` in the local run.  This is not the hard
  image-viewed `0.200 V` validation setup and does not prove the DMM fixed; it
  is a runtime boundary showing that simply forcing the saved-config default
  mux arms (`5/5`) or one-sided variants is not an immediate explanation for
  the current producer-frame value.  The diagnostic hook must remain explicit
  debug tooling, not production range feedback.
- H2/SPI3 RX-summary live negative: OpenScope app image SHA-256
  `2747616247fb2057e94be8d77257c5d391adabc05556232fc52ce71dba501321`
  (`Build: Jun 6 2026 21:36:18`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`.  The diagnostic-only `meter trace` output now records RX byte
  classes while streaming the stock 115,638-byte H2 table and records the six
  stock-visible close/flush RX bytes without changing the TX sequence.  The
  live trace showed `rx00=0`, `rxff=115638`, `rxother=0`, `close_len=6`, and
  `h2_close_rx bytes=FF FF FF FF FF FF`, while the DMM producer frame decoded
  as `raw=2236`, class `1`, display `2.236 V`
  (`5A A5 A4 BD 8D EF 47 20 00 00 01 3A`) on the current bench state.  This is
  an H2 acceptance/apply boundary, not a DMM fix: MISO provides no observable
  ACK/status during the H2 body or close sequence, so future calibration claims
  still need stock xrefs, W25Q/system-file evidence, or a runtime effect trace
  instead of invented coefficients.
- Producer stream trace extension: OpenScope app image SHA-256
  `a97563febdcbdf1f62d579141f7b5d99f94121a0de74c39e0586d997f2418235`
  (`Build: Jun 6 2026 21:44:06`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`.  `meter mux-stream` now prints each changed reading with
  producer counters, the active stock-like selector/apply words, current
  frontend GPIO state, and the full 12-byte stock-visible frame.  This fixes a
  diagnostic truncation where `usb_debug_printf()` cut the long stream line at
  255 bytes.  A live stream on the current bench state showed stable DCV
  frames under `seq_word=0514`, `seq_apply=0000`, `PC12=1 PE4=1 PE5=0 PE6=1
  PA15=1 PA10=1 PB10=0 PB9=0 PA6=0`, with frames such as
  `5A A5 A4 BD 8D CF 47 20 00 00 01 3F` and
  `5A A5 A4 BD 8D CF 47 20 00 00 01 3C`, decoded as `raw=2235`,
  `display=2.235 V`.  This is a better capture tool for the next controlled
  low-DCV sweep: it can show exactly which producer frames appear while the
  physical input changes, without OCR, magnitude feedback, or decoder-side
  coefficients.  It is not itself a correction and was not the image-viewed
  `0.200 V` validation case.
- Boot-order replay live negative: OpenScope app image SHA-256
  `941e5061713e204fdd986d2a966c90e9560aff41b1a6ddb7931825f1bccbd9c2`
  (`Build: Jun 6 2026 21:49:33`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`.  The diagnostic-only `meter boot-sequence [ms]` command applies
  the current DMM submode frontend and replays the stock boot/wake command
  order `0x0508, 0x0509, 0x0507/0x050A, selector`, then prints the normal
  machine-readable `meter trace`.  On the current bench state, DCV submode 0
  applied `planned_gpio=0BB` and `actual_gpio=0BB`; TX history showed the
  expected replay frames `0x0508`, `0x0509`, `0x0507`, `0x0514`.  The stream
  then passed through `OL`/blank frames and settled back to the same scale as
  the prior current-bench DCV traces, with frames such as
  `5A A5 A4 BD 8D AF 4D 20 00 00 01 3F` decoded as `raw=2232`,
  `display=2.232 V`.  This is not the image-viewed `0.200 V` validation setup
  and does not prove a user-visible fix.  It does narrow the upstream search:
  simply replaying the exact stock boot word order against the current runtime
  frontend is not enough to correct the producer-frame value, so future work
  should move toward the runtime writer/apply/calibration path rather than
  repeating `0x0508` order experiments.
- Factory-cal trace boundary: OpenScope app image SHA-256
  `905d4d6125ac519a8bd781eb04eee3c9def239bb18703222b1d62e8b00543c4d`
  (`Build: Jun 6 2026 21:53:48`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`.  `meter trace --json` now carries a machine-readable
  `factory_cal` object next to the selected plan, GPIO, H2, and producer-frame
  fields.  The current firmware reports `factory_cal.loaded=0`,
  `ch_size=301`, `channels=2`; this is the fail-closed placeholder described
  in `flash_fs.c`, not a recovered physical correction.  A settled live DCV
  trace on the current bench state showed `producer_frame=
  5A A5 A4 BD 8D AF 4D 20 00 00 01 36`, decoded `raw=2232`,
  `display=2.232 V`, with DCV GPIO `planned_gpio=0BB`, `actual_gpio=0BB`, H2
  body RX `rxff=115638`, and H2 close/post-trigger RX bytes still all `FF`.
  The local W25Q extraction visible in `tmp/w25q-full-2026-06-06-extract`
  still has only JPG assets plus a zero-byte `System file/9999.BIN` outside
  system-volume metadata.  This trace does not fix low DCV; it makes each bad
  producer-frame capture explicitly show that no host-readable factory-cal
  mirror is loaded, so future work must recover a real stock/W25Q/H2/runtime
  source before using calibration state.
- RX ownership and voltage-marker fail-closed boundary: OpenScope app image
  SHA-256
  `0e3287ba4af9821a4ea3ad96264d42f30b585af0e06d31553661a6ec53804214`
  (`Build: Jun 6 2026 21:55:18`) was flashed through guarded HID IAP after
  `flash_preflight.py hid-app` classified it as `openscope-app` at
  `0x08004000`. The firmware now moves complete USART2 `5A A5` data frames
  from the ISR to `dvom_RX` as immutable queue events instead of signalling a
  binary semaphore over one shared `fpga.rx_frame`. This closes a producer/task
  ownership race: the parsed frame is now the exact frame event captured by the
  ISR. The decoder also now rejects DCV/ACV frames that lack the recovered
  stock-visible voltage metadata in `frame[8]/frame[9]`; the live bad frame
  `5A A5 CC 47 FE EB 47 20 00 00 01 3C` is covered by a unit test and must
  render invalid/cleared instead of a confident `154.06 V`.

  The post-flash live trace on the current bench state showed producer and
  parsed frames matching exactly:
  `5A A5 CC E7 EB CB 07 24 80 00 01 3F`, decoded as `display=1.5005`,
  `raw=15005`, `family=0/0`, `reject=0`, with DCV GPIO `planned_gpio=0BB`,
  `actual_gpio=0BB`, `factory_cal.loaded=0`, and H2 RX still all `FF`. This
  proves the old unmarked-`frame[8]=00` active-voltage fallback is no longer
  present in the observed runtime path. It does not prove physical DCV
  correctness for the user-visible `0.200 V` setup: the webcam still path was
  attempted through `/dev/video0` and produced a black frame, so no image-viewed
  source/readout confirmation was available in this run. If the source was not
  actually near `1.5005 V`, the remaining bug is still upstream of stock
  decimal decoding: frontend writer/apply state, H2/SPI3 acceptance, or missing
  factory/system-file calibration evidence.
- Low-DCV producer-family threshold boundary: OpenScope app image SHA-256
  `b00f944242259e14710507c06db96a2690a28c6e7fe44947c5d7566624a796e2`
  was flashed through guarded HID IAP after `flash_preflight.py hid-app`
  classified it as `openscope-app` at `0x08004000`. The build string still
  reported `Build: Jun 6 2026 22:38:57`, so the image SHA and live behavior are
  the provenance markers for this run. A live low-DCV stream near the user's
  reported threshold showed the same DCV selector/control state throughout:
  `seq_word=0514`, `seq_apply=0508`, `PC12=1 PE4=1 PE5=0 PE6=1 PA15=1 PA10=1
  PB10=0 PB9=0 PA6=0`. With no GPIO or command change, the producer alternated
  between voltage-marked frames such as
  `5A A5 E6 AF 4D 0E 0A 00 82 00 01 47` (`raw=8241`,
  `display=0.8241 V`) and unmarked frames such as
  `5A A5 E4 2E 63 25 07 00 00 00 01 47` (`reject=1`, display cleared to
  `---`). User-visible observations in the same sweep were `0.570 V -> ---`
  and `0.93 V -> 0.9244`. This is not evidence for ohms mode selection: it is
  evidence that under the current DCV setup the producer crosses between
  stock-visible voltage-family metadata (`frame[8]=0x82`) and an unmarked frame
  family (`frame[8]=0x00`) around the low-voltage boundary. The decoder must
  keep failing closed on `frame[8]=0x00` DCV frames, but it must not reject all
  `frame[8]=0x82, frame[7]=0x00` frames by magnitude or by `frame[6]` alone:
  the live `0.93 V` point and the stock 1.5 V rotating fixture use that marked
  shape. The unresolved fix remains upstream: recover/apply the missing
  frontend/range/state/calibration source that makes sub-1V DCV produce the
  correct stock-visible voltage frame, rather than tuning decoder coefficients.
- Stock-shaped poll cadence live check: OpenScope app image SHA-256
  `77885ba416a0db7c6ad2c8d07500670437b285ee65db2c5ecbcbb4d3ec5bb3cc`
  was flashed through guarded HID IAP after `flash_preflight.py hid-app`
  classified it as an `openscope-app` image at `0x08004000`. Before this
  change, live `status` showed the DMM TX history as a bare repeated
  `00 00 05 09 ...` start command. The firmware now reissues the recovered
  plan words on each DMM poll: basic config `0508`, DCV selector `0514`, probe
  `0507`, and start `0509` for DCV, plus the stock-plan apply word for modes
  that have one. The post-flash live `tx_frames_recent` history confirmed the
  repeated `0508 0514 0507 0509` sequence, so the recovered selector/config
  state is now kept alive on real hardware instead of only at transition time.

  This is not yet the low-DCV fix. On the current bench state the first and
  steady producer frame remained
  `5A A5 E4 2E 63 25 07 00 00 00 01 47`, decoded as invalid/cleared
  (`display=---`, `reject=1`) with digit codes `0A 0B 0C 0D`, DCV
  `planned_gpio=0BB`, `actual_gpio=0BB`, `factory_cal.loaded=0`, and H2 RX
  bytes still all `FF`. Therefore the old bare-`0509` loop was a real
  stock-shape gap, but it is not sufficient to make sub-1V DCV produce a
  marked voltage frame. The remaining blocker is still producer-side state
  before stock decimal decoding: an unrecovered dispatcher/materializer,
  range/frontend writer, ASIC/H2 acceptance path, or factory-calibration source.
- Low-DCV false-voltage parser hardening: OpenScope app image SHA-256
  `2b983d96e076064cd1ae0ded505e11e93ecfb786f9e8d7abc630b09daaac94f8`
  was flashed through guarded HID IAP after `flash_preflight.py hid-app`
  classified it as an `openscope-app` image at `0x08004000`. The parser no
  longer accepts bare `frame[8]=0x80` as confident voltage-family metadata:
  bit 7 remains the stock class-4 decimal exponent only after low bits `0x02`
  prove the voltage family (`0x82` remains valid for the known good low-DCV
  fixtures). The live `0.200 V -> 0.4366 V` frame
  `5A A5 44 8E EF E7 07 24 80 00 01 89` now fails closed in tests as
  `METER_REJECT_WRONG_FRAME_FAMILY` instead of being blessed as a normal DCV
  reading.

  The parser also now rejects mixed special/digit segment codes before normal
  BCD assembly. Only raw digit codes `0..9` enter the numeric path after the
  explicit OL/blank/continuity terminal cases; codes such as `0x0C..0x14` no
  longer get clamped into decimal digits. Post-flash live CDC on the current
  bench state repeatedly produced
  `5A A5 E4 2E 63 25 07 00 00 00 01 47` with raw digits `0A 0B 0C 0D`; the
  firmware reported `display=---`, `valid=0`, `bcd_value=0`, and `reject=1`
  across `meter-trace` and five `meter-dump` polls. This closes the immediate
  read-function class of false confident voltages from unmarked/special frames.
  It still does not prove the analog/frontend producer path correct at every
  low voltage: marked `0x82` frames continue to decode by stock decimal math,
  while missing marked frames remain a producer/range/calibration problem.

  A stock-in-Renode DMM trace oracle was added under `emulator/renode/` to log
  stock USART2 command frames and RAM state around `ms[0x02]`, `ms[0x03]`,
  `DAT_20001025`, and adjacent formatter bytes. It is trace-only, uses generic
  ACK/status transport responses, and must not be treated as a real analog
  frontend model or hardware-in-loop proxy. The current host has neither
  `renode` nor the Python `unicorn` module installed, so this trace path is
  prepared but not executed in this run.

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

AUTO/autoscan is now part of that safety boundary: `meter_auto_candidates()` is
voltage-only (`DCV`, `ACV`) because AUTO physically reconfigures the frontend on
an unknown live input. Current, resistance, continuity, diode, capacitance, and
temperature remain explicit manual submodes with parser/state-machine coverage,
but default autoscan must not sweep them until stock/live evidence proves safe
current-jack detection and de-energized passive-probe conditions.

## 2026-06-06 Shorted-Probes Live Sweep

Bench fixture: user shorted the DMM probes. During the host-driven
`shorted-probes-sweep` the user heard the relay click twice, like a fast
enable/disable transition. That sound is useful live context only; the firmware
evidence below remains the CDC state/trace output.

OpenScope app image SHA-256
`a146d420fe1a924011e3b357e2c9f21dfba4789f20ca233d27ec27ee011425d7`
was flashed through guarded HID IAP after `flash_preflight.py hid-app`
classified it as an `openscope-app` image at `0x08004000`. This image included
the recovered stock state-8 command-bank prefix for continuity/diode:
`0x00, 0x2C` before the local raw selector/apply path. A live sweep with shorted
probes still failed:

- DCV selected `0x0514` but reported `---` from the unmarked/special producer
  frame family instead of a near-zero voltage.
- ACV selected `0x050C`/apply `0x050D` but rejected as wrong-family, not the
  expected missing-AC-evidence case.
- Resistance selected `0x050A` but did not produce near-zero ohms or the
  unresolved-calibration reject.
- Continuity selected `0x0511`/apply `0x0516` with planned bank
  `1/00/2C`, but the dump remained `beep=0` and did not produce the recovered
  continuity marker.

Interpretation: the stock state-8 byte-bank prefix is a real missing command
surface, but applying it in the local transition path is not sufficient to fix
the shorted-probes case. The double relay click plus repeated `---`/OL-shaped
frames point at an upstream apply/hold/frontend state issue before stock
decimal decoding and before continuity marker parsing. The next live trace must
capture the exact transition history, including whether the byte-bank prefix was
actually sent for the switch that produced the first post-transition frame, and
pair that with GPIO/mux state and producer frames. Do not replace this with
OCR, prose validators, one-point coefficients, or magnitude-based mode guesses.
