# FPGA Future Possibilities (Gowin GW1N-UV2)

*Research compiled March 2026*

## FPGA Specifications

The 2C53T uses a **Gowin GW1N-UV2** (identified by EEVblog user jbtronics, markings sanded by FNIRSI).

| Resource | Count | Notes |
|----------|-------|-------|
| LUT4 | 2,304 | |
| Flip-Flops | 2,016 | |
| Block SRAM | 72 Kbit (9 KB) | 4 blocks |
| Shadow SRAM | 18 Kbit | |
| User Flash | 96 Kbit | Non-volatile |
| PLLs | 1 | |
| DSP/Multiplier Blocks | **0** | Only GW1N-4 and above have hardware multipliers |
| On-chip oscillator | Yes | 2.5–125 MHz |
| Configuration | Non-volatile (built-in flash) | No external config memory needed |
| Core voltage | Internal LDO (UV = Universal Voltage) | Accepts 1.8/2.5/3.3V, regulates to 1.2V internally |
| Speed grades | C6 (commercial), I5 (industrial) | Fabric speeds typically 150–200 MHz for simple logic |

### "UV" Designation

UV = Universal Voltage. The FPGA has an integrated LDO regulator, so the board only needs a single supply rail (1.8V, 2.5V, or 3.3V). This simplifies power design in a battery-powered handheld — likely why FNIRSI chose it.

## Comparison to Other Budget Scope FPGAs

| Feature | Gowin GW1N-UV2 (2C53T) | Anlogic EF2L45 (FNIRSI 1013D) | Lattice LCMXO2-1200HC (Hantek 2D72) |
|---------|------------------------|-------------------------------|--------------------------------------|
| LUTs | 2,304 | 4,480 | 1,280 |
| Flip-Flops | 2,016 | 4,480 | ~1,280 |
| Block RAM | 72 Kbit | 700 Kbit | ~64 Kbit |
| DSP Blocks | **0** | **15** (18x18) | **0** |
| PLLs | 1 | 1 | 1 |
| Non-volatile | Yes (built-in) | No (external flash) | Yes (built-in) |

The GW1N-UV2 is mid-tier for budget scopes — more capable than the Lattice in the Hantek, but much smaller than the Anlogic in the 1013D. FNIRSI clearly cost-optimized the 2C53T, pushing more processing to the GD32F307 MCU.

## Estimated Current Resource Usage

| Function | Est. LUTs | Est. FFs | Notes |
|----------|-----------|----------|-------|
| ADC interface + clock generation | 200–300 | 100–200 | Capture data from ADC at 250 MHz |
| Dual-channel sample buffers | 300–500 | 200–400 | Write to BRAM, manage pointers |
| Timebase / decimation | 200–300 | 100–200 | Divide sample rate for slower timebases |
| Trigger (edge/level) | 100–200 | 50–100 | Threshold compare, edge detect |
| UART interface to MCU | 100–200 | 50–100 | Parse string commands, send responses |
| Misc control/status | 100–200 | 50–100 | Channel coupling, gain relays, etc. |
| **Estimated total** | **~1,000–1,700** | **~550–1,100** | |
| **Estimated free** | **~600–1,300** | **~900–1,450** | |

Block RAM is likely mostly consumed by sample buffers (72 Kbit = 9 KB, enough for ~4,500 12-bit samples or ~9,000 8-bit samples across two channels).

## Feasible Enhancements (~500–1000 spare LUTs)

### Advanced Trigger Types (highest value)

| Trigger Type | Est. LUTs | Description |
|-------------|-----------|-------------|
| Pulse width | ~200 | Trigger when pulse is wider/narrower than threshold. Catches glitches |
| Runt | ~300 | Trigger on pulses crossing one threshold but not another |
| Dropout/timeout | ~150 | Trigger when signal goes quiet for N cycles |
| Serial pattern (I2C) | ~400–600 | Match a specific I2C address at hardware speed |
| Serial pattern (UART) | ~300–500 | Match a specific byte/character at wire rate |
| Serial pattern (SPI) | ~300–500 | Match a specific SPI byte on MOSI/MISO |

These are the highest-value additions because the MCU (120 MHz Cortex-M4) physically cannot do real-time trigger evaluation on a 250 MHz sample stream. Only the FPGA can.

### Measurement Acceleration

| Function | Est. LUTs | Description |
|----------|-----------|-------------|
| Hardware frequency counter | ~200–300 | Count zero crossings over gated interval. Far more accurate than software |
| PWM analyzer | ~200 | Continuous duty cycle + frequency measurement. Great for automotive/embedded |
| Min/max tracker | ~100–150 | Track min/max sample values in hardware between MCU reads |

### Other Possibilities

| Function | Est. LUTs | Description |
|----------|-----------|-------------|
| Logic analyzer mode | ~300–500 | Treat ADC data lines as 8-bit digital capture (speed-dependent on ADC wiring) |
| Equivalent-time sampling | ~200–300 | Build up high-resolution waveforms of repetitive signals beyond the real-time sample rate |

## Not Feasible (too resource-intensive)

| Function | Why not |
|----------|---------|
| FFT | Needs multipliers (0 DSP blocks) and deep RAM. A 256-point FFT would consume the entire FPGA |
| FIR/IIR digital filters | Each multiply-accumulate costs ~300–400 LUTs in fabric. A 4-tap FIR would use all free space |
| Deep protocol decode buffers | Only 9 KB BRAM total. Can match patterns but can't store/decode full transactions |
| Waveform generation / DDS | Signal generator already uses MCU's built-in DAC; no benefit to moving it |
| Compression / encoding | No multipliers, insufficient RAM for meaningful buffering |

## Optimal Architecture: FPGA Triggers + MCU Decodes

The most practical split for protocol-aware capture:

```
FPGA (250 MHz, real-time):
  → Watches raw sample stream continuously
  → Matches hardware trigger pattern (e.g., I2C start + address 0x48)
  → Fires trigger, captures sample window into buffer

MCU (120 MHz, post-capture):
  → Reads captured sample buffer from FPGA
  → Decodes full protocol transaction in software
  → Computes measurements, runs FFT
  → Renders everything on LCD
```

This is the same architecture used by Rigol, Siglent, and Keysight scopes — the difference is they have larger FPGAs with more trigger types. With ~600–1000 spare LUTs, the 2C53T could implement 3–4 advanced trigger types, which would already exceed anything in its price class.

## Prerequisites (Year 3+ Roadmap)

Before any FPGA modifications are possible:

1. **Physical access to FPGA JTAG pins** — need to locate test points or trace PCB connections
2. **Determine if security bit is set** — if FNIRSI enabled readback protection, can't dump the existing bitstream (can still write new ones)
3. **Reverse-engineer or reimplement base gateware** — either dump + decompile the stock bitstream, or rewrite from scratch based on the USART2 protocol RE
4. **Toolchain support** — Gowin's proprietary IDE (GowinEDA, free license) supports GW1N-UV2. The open-source Apicula/Yosys/nextpnr flow does NOT yet support GW1N-UV2 (closest supported: GW1N-UV4)
5. **Testing infrastructure** — need real hardware with JTAG connection to validate

### Possible shortcut: MCU as FPGA programmer

If the GD32 has GPIO connections to the FPGA's JTAG or SSPI programming pins (not just USART2), the MCU could potentially bitbang a programming sequence. This would allow FPGA updates via the existing USB firmware update path — no separate JTAG programmer needed. Requires PCB trace analysis to determine feasibility.

## References

- [Gowin GW1N Product Page](https://www.gowinsemi.com/en/product/detail/48/)
- [GW1N Datasheet DS100](https://cdn.gowinsemi.com.cn/DS100E.pdf)
- [Project Apicula (open-source Gowin bitstream tools)](https://github.com/YosysHQ/apicula)
- [Yosys + nextpnr-gowin synthesis flow](https://github.com/YosysHQ/nextpnr)
- [EEVblog 2C53T thread (FPGA identification)](https://www.eevblog.com/forum/testgear/new-handheld-scopedmm-fnirsi-2c53t-2ch-50mhz250msps-(aug-2024)/)
- [FNIRSI 1013D teardown (Anlogic FPGA comparison)](https://www.cnx-software.com/2022/11/16/fnirsi-1013d-teardown-and-mini-review-a-portable-oscilloscope-based-on-allwinner-cpu-anlogic-fgpa/)
