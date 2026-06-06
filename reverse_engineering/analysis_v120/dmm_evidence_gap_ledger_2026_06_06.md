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
