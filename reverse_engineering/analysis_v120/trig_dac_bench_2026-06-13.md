# Trigger-DAC bench session + signal-generator correction (2026-06-13)

Bench session on Unit 2 to verify the faithful reimplementation of the stock scope
trigger-comparator DAC (`scope_trigger.c/.h`, mirroring stock `FUN_080018a4` /
`gpio_mux_portc_porte`: `dac = ((upper-base)/200.0)*(level+100)+base` -> `DAC1_DHR12R1`
@0x40007408 + software trigger @0x40007404).

## Result: FIRMWARE-LEVEL CONFIRMED (V1), analog (V2) BLOCKED

- Over the USB debug shell, `trig raw <code>` and `trig <range> <level>` produced the
  **correct computed 12-bit codes** (raw 0/1024/2048/3072/4095 pass through;
  `trig 0 -100 / 0 / +100` -> 0 / 2047 / 4095). The compute path + register writes are
  byte-faithful to stock. Scored **D3 / R2 / V1** (static + on-device code execution).
- **Analog output (V2) could NOT be observed.** DAC1 lands on **PA4 = MCU pin 29
  (LQFP100)**, which on this board is the *internal* scope-trigger comparator reference -
  it is **not broken out to any external jack**. No fine-tip probes on hand to reach pin 29.

## Why the signal-generator jack is the WRONG observable (key correction)

We attempted to read the trig-DAC voltage on the **signal-generator output jack**. It read a
**fixed ~3.245 VDC regardless of DAC code** (raw 2048 and raw 0 both -> 3.245 V), and
**~0.0 VAC even with our siggen actively set to 3.3 Vpp / 10 kHz sine, "output on."**

Root cause (user-identified, confirmed against Apicula sister-project analysis): **the signal
generator is an FPGA bitstream module, not an MCU DAC.** The FNIRSI 2C53T FPGA image is
**4 independent blocks: 2 scope acquisition channels (BSRAM_0/BSRAM_3) + 1 DMM + 1
signal-generator (left-edge ODDRC primitive).** The siggen output jack is driven by the FPGA,
NOT by MCU DAC1/PA4.

Consequences:
1. **Our `firmware/src/drivers/dac_output.c` is a clean-room MISATTRIBUTION** - it drives the
   signal generator from MCU DAC1/PA4 (TMR6+DMA). The real siggen lives in the FPGA. That is
   why setting 3.3 Vpp produced 0.0 VAC: our DAC isn't wired to that jack, and the FPGA siggen
   never loads (config-entry wall, same blocker as the scope).
2. **Our "signal generator" mode is non-functional and FPGA-gated**, exactly like scope
   acquisition - it joins the FPGA-blocked set, it is not a working feature. The 3.245 VDC is
   the FPGA pin's idle/pulled-up level.
3. The siggen jack is **not** a valid observable for the trigger DAC; PA4 must be probed
   directly.

## Next step (unblocks V2)

Microscope arriving ~week of 2026-06-15 enables fine-trace probing of **PA4 (pin 29)**. Batch
the trig-DAC analog V2 confirmation with other pin-level checks then. Until then the trig-DAC
module stays **LOCAL / uncommitted** (`scope_trigger.c/.h`, `usb_debug.c` `trig` cmd,
Makefile entry) per the "commit firmware only once bench-confirmed" rule.

Also TODO (decode campaign): reclassify `dac_output.c` / the siggen-config stock function as
DIVERGENT-misattributed -> the siggen is FPGA_BLOCKED, not an MCU-DAC feature.
