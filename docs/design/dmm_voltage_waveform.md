# DMM Voltage Waveform

The DMM voltage waveform is an overlay inside the existing multimeter AC/DC
voltage modes. It uses the multimeter voltage frontend routed to the COM and
V/Ohm/C jacks, not the oscilloscope CH1/CH2 inputs.

## Scope

- Enabled only for meter submodes `0` (DC voltage) and `1` (AC voltage).
- Other stock multimeter functions keep their existing UI and acquisition
  paths: resistance, continuity, diode, capacitance, temperature, and current.
- Capacitance remains a DMM mode, selected through the meter mode sequence, and
  is not part of this waveform overlay.

## Data Path

The decoded numeric DMM reading still comes from the USART2 meter frames. Those
frames arrive only a few times per second, so they cannot be used to reconstruct
a 50/60 Hz sine, inverter step waveform, or dimmer chop shape. They remain the
authoritative calibrated value shown in large digits.

DC voltage decimal/exponent selection follows the stock-analysis range hint bits
in the meter frame (`frame[8].7`, `frame[3].4`, `frame[4].4`, `frame[5].4`),
translated through the local stock-FSM display layer. AC voltage decimal format
follows the cleaner stock `frame[7]` formatter path and still requires separate
AC evidence. Bytes `[10..11]` may be exposed as a narrow empirical auxiliary
frequency hint, but they are not used to choose the voltage exponent.

The waveform shape is based on the stock-documented SPI3 acquisition case 5
(`METER_ADC_READ`), which is documented as a single-byte meter ADC read. The
firmware polls this candidate raw-sample path at the FreeRTOS tick rate while
the multimeter is in AC/DC voltage mode and stores the samples in a small ring
buffer.

Hardware validation must prove that repeated `METER_ADC_READ` polls while the
meter voltage frontend is active really return raw-enough DMM-path samples from
the COM + V/Ohm/C jacks. If they instead return a filtered/decimated DMM value,
then this feature cannot truthfully draw multi-hertz waveform shape from the
DMM path and must remain blocked rather than falling back to CH1/CH2 scope
inputs.

The USB debug shell commands used for validation are:

- `meter dump [delay_ms]`: parsed DMM/UI state, raw 12-byte frame, decoded BCD,
  decimal position, unit, frame[6] history, companion `aux_freq_i10`, and
  waveform sample count. It also reports expected/observed frame family and
  wrong-family reject reason so current/frontend failures are visible as mode
  activation evidence instead of only `---`.
- `meter stream [count] [delay_ms]`: compact decoded-frame stream for watching
  range/decimal instability without mixing concurrent serial readers; each
  row includes expected/observed frame family and wrong-family reject state.
- `meter mux-stream [count] [delay_ms]`: decoded-frame stream plus the tested
  DMM transition plan (`stock_mode`, frame family, Port C/E mux, Port A/B mux,
  settle time),
  observed frame family/reject reason, recent mode-sequence words, and actual
  frontend GPIO state.
- `meter frontend`: one-shot DMM selector, transition plan, GPIO, parsed
  reading, frame-family rejection state, recent USART command pairs, buzzer,
  discard-window state, and frames skipped while a mode transition was still
  busy.
- `meter adc-snapshot`: read-only DMM waveform sampler counters and summary.
- `meter wave`: waveform buffer stats, SPI3 meter-ADC diagnostics, and the
  decoded DMM reading.
- `meter wave path [direct|preacq]`, `meter wave selector [auto|0..255]`,
  and `meter wave preacq [auto|0..255]`: explicit diagnostics for the
  candidate DMM waveform SPI path.
- `mode meter [submode] [layout]`: switch the UI and FPGA frontend to a DMM
  submode from USB before capturing evidence.

The host helper `scripts/openscope_live_debug.py` can run those commands once
or poll them into a log without needing a terminal emulator. It has dedicated
subcommands for `meter-dump`, `meter-frontend`, `meter-stream`,
`meter-mux-stream`, `meter-adc-snapshot`, and `screen-capture`; use the generic
`command` or `poll` subcommands for firmware shell commands such as
`meter wave` and `mode meter 1 0`.

`screen-capture` defaults to the read-only firmware command `screen dumpbin`,
which returns a `SCREENBIN` header, exact-length packed indexed4 payload, and
CRC32. The host reads the payload by `len`, verifies the CRC, and writes a BMP
through the same palette path used by text dumps. The older `--rle-shadow` path
is now disabled because it cleared/paginated shadow state and switched the UI to
ACV while stitching pages.

The module has a narrow scaling abstraction for v1 calibration:
`meter_voltage_wave_scale_from_dmm_rms()` derives raw-count-to-volt scaling from
the decoded DMM RMS voltage, keeping the numeric DMM reading authoritative.
Later factory-table calibration can replace that scale source without changing
the UI renderer.

## Rendering

The full DMM voltage layout keeps the large numeric reading and bar graph. A
compact waveform panel is allowed only after the candidate sample source shows
real peak-to-peak movement; when the source is flat `0xFF`, the firmware must
show the waveform as unavailable rather than drawing a synthetic or scaled
trace.

- The trace is auto-scaled to the raw min/max in the sample buffer.
- A faint envelope shows the recent raw range so clipped or noisy shapes do not
  disappear when the synced trace window is short.
- Frequency is estimated from zero crossings when a stable AC shape is present.
  In the live AC mains frame shape captured on 2026-06-05, bytes `[10..11]`
  also alternate around `0x0031/0x0032`, which is exposed as a narrow empirical
  `aux_freq_hz` hint for sync. This is not yet stock-proven metadata.
- AC voltage mode shows an approximate `P-P~` value by scaling the raw peak span
  against the decoded DMM RMS reading.
- DC voltage mode labels the panel as ripple shape; the numeric DC reading
  remains the calibrated measurement.

This feature is a visual aid for ripple and waveform shape on the DMM voltage
jacks. It is not a calibrated oscilloscope replacement.

## Meter Render Timing Budget

Stock V1.2.0 initializes the ST7789 normal-mode frame-rate register
`FRCTRL2` (`0xC6`) to `0x0F`, annotated in the local reverse-engineering notes
as 60 Hz. That gives one visible LCD frame every 16.67 ms. The stock display
path also renders through RAM buffers and blits pixels to the EXMC LCD data
register with DMA, rather than doing every UI primitive as a visible direct LCD
write.

This firmware currently has no proven LCD TE/vsync path. The stock analysis has
not shown a `TEON` (`0x35`) setup or a LCD tearing-effect interrupt to synchronize
draws with panel blanking. Until a real TE pin/interrupt is proven, the meter
anti-flicker requirement is:

- normal steady-state meter redraws must avoid full-screen visible clears;
- normal steady-state redraw time must stay below one 60 Hz LCD frame
  (`16667 us`);
- the engineering target for retained/partial meter redraws is `<=12000 us`,
  leaving margin for FreeRTOS scheduling and interrupts;
- structural redraws after mode/layout/theme/REL/HOLD/continuity-flash changes
  may exceed that target, but should not occur continuously.

The firmware measures `draw_meter_screen()` with the Cortex-M4 DWT cycle counter
and exposes the result through USB debug:

- `meter dump [delay_ms]`
- `ui dump`
- `screen dumpbin [x y w h]` for fast read-only screenshot timing

Relevant fields are `draw_us`, `max_draw_us`, `over_budget`, `last_full`,
`full_clears`, and `partial_clears`. During a stable AC/DC voltage measurement,
`last_full` should settle to `0`, `partial_clears` should advance, and
`over_budget` should not increase after the initial structural redraws.

## 2026-06-05 Live Status

Official V1.2.0 firmware was downloaded from FNIRSI's published Shopify CDN
bundle as `APP_2C53T_V1.2.0_251015.bin`:

- size: `751232` bytes
- sha256: `a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760`

`analyzeHeadless` was not available on the bench host, so this pass used the
existing Ghidra decompile plus fresh `arm-none-eabi-objdump` disassembly of the
downloaded binary. The stock ACV case in `meter_mode_handler` branches from the
TBB table to `0x08037228` and reads `frame[7]` bit 0 to select the ACV decimal
state. The pass did not prove that ACV frames use `[10..11]` as a companion
frequency field; stock frequency-unit paths exist elsewhere in the DMM FSM, but
that is different evidence.

Current custom firmware now ports the stock DMM display-state machine into
`meter_data.c` and translates its display format/unit outputs into the local
renderer fields. The parser stock-family state and the frontend transition
sequence both derive their UI-submode mapping from `fpga_meter_plan`, so one
tested table controls selector/apply words, mux metadata, and parser debug
state. Autoscan only scores candidates when the parser reports a clean matching
frame family for the active transition plan; it no longer uses nonzero numeric
magnitude as evidence for any mode. AC voltage/current candidates also need AC
evidence (`is_ac` from the frame status or companion frequency metadata), and
the parser applies the same gate before rendering manual AC modes, so DC
payloads are not promoted into AC display states. For ACV, this
means the display formatter still uses the stock `frame[7]` format selector
instead of using `extra` as a range hint once AC evidence is present:

Out-of-range local submodes fail closed. The shared plan API marks them with an
invalid stock mode/frame family, emits no selector/apply word, and the parser
rejects any frame with `METER_REJECT_INVALID_SUBMODE` instead of silently
falling back to DCV. USB `mode meter` already bounds user input, but the parser
and plan layer keep this guard so future callers cannot arm the voltage
frontend by accident.

- `frame[7] bit0 set` maps to stock ACV format index `0`, rendered locally as
  `X.XXX V`.
- `frame[7] bit0 clear` maps to stock ACV format index `1`, rendered locally
  as `XXX.X V`.

After flashing the full-FSM port, the live unit produced stable
`227.5..227.8 V` readings with `aux_freq_i10=490`, matching Georgian mains at
about 50 Hz. The `aux_freq_i10` value is still empirical live metadata from
`[10..11]`; it is not used to select the voltage range.

The waveform source is still not solved. A post-flash sweep across
`direct/preacq` SPI paths and selector `0/1` showed the candidate sample stream
advancing at about 1 kHz but every sample remained `0xFF`, with `p2p=0` and no
usable DMM-path shape. A later diagnostic build expanded the sweep to
`preacq=0x80..0x8F` and `selector=0..7`; all 128 combinations still returned
`pre_rx=FF`, `last=FF`, `min=FF`, and `max=FF` while the USART2 DMM reading
remained live at about `4.995..4.997 V` on a 5 V DC lab supply. That blocks
claiming the voltage waveform overlay works on the real COM + V/Ohm/C jacks.

The stock DMM command-table evidence is tracked in
`reverse_engineering/analysis_v120/meter_mode_command_table_2026_06_05.md`.
That note now separates the recovered eight stock selector slots from the local
eleven-submode porting map and records the evidence boundary for `ms[0x02]`
(`gpio_mux_portc_porte`) versus `ms[0x03]` (`gpio_mux_porta_portb`). In
particular:

- stock current evidence proves two selector slots: DC current has display-side
  mA/A formatter evidence, AC current currently has ACA mA formatter evidence,
  and no separate recovered AC A or uA selector has been found;
- capacitance and temperature share the recovered extended selector slot
  `0x0512` in the local port; stock notes support capacitance-like formatting
  and a mode-5 Fahrenheit conversion path, not separate stock selector modes;
- the current two-frame discard and 20 ms settle window is a conservative local
  transition policy, while stock evidence so far proves command pacing,
  USART2 frame filtering, and transition-frame recognition rather than exact
  fixed settle/discard constants.
