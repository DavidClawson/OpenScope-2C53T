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

The USB debug shell command `meter wave` reports the current waveform buffer
stats and the decoded DMM reading for this validation pass.

The module has a narrow scaling abstraction for v1 calibration:
`meter_voltage_wave_scale_from_dmm_rms()` derives raw-count-to-volt scaling from
the decoded DMM RMS voltage, keeping the numeric DMM reading authoritative.
Later factory-table calibration can replace that scale source without changing
the UI renderer.

## Rendering

The full DMM voltage layout keeps the large numeric reading and bar graph, then
draws a compact waveform panel below them:

- The trace is auto-scaled to the raw min/max in the sample buffer.
- A faint envelope shows the recent raw range so clipped or noisy shapes do not
  disappear when the synced trace window is short.
- Frequency is estimated from zero crossings when a stable AC shape is present.
  The stock voltage-mode parser does not currently expose a separate decoded Hz
  companion reading, so this is the active sync path for v1.
- AC voltage mode shows an approximate `P-P~` value by scaling the raw peak span
  against the decoded DMM RMS reading.
- DC voltage mode labels the panel as ripple shape; the numeric DC reading
  remains the calibrated measurement.

This feature is a visual aid for ripple and waveform shape on the DMM voltage
jacks. It is not a calibrated oscilloscope replacement.
